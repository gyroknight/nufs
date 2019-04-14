#include <assert.h>
#include <errno.h>

#include "bitmap.h"
#include "directory.h"
#include "pages.h"
#include "slist.h"
#include "util.h"

#include "storage.h"

void storage_init(const char* path) {
  pages_init(path);
  inodes_init();
  directory_init();
}

int storage_stat(const char* path, struct stat* st) {
  int inodeIdx = tree_lookup(path);

  if (inodeIdx != -ENOENT) {
    inode* found = get_inode(inodeIdx);

    st->st_mode = found->mode;
    st->st_size = found->size;
    st->st_uid = getuid();
    st->st_atime = found->atime;
    st->st_mtime = found->mtime;
    st->st_ctime = found->ctime;
    st->st_nlink = found->refs + 1;

    return 0;
  } else {
    return -ENOENT;
  }
}

int storage_mknod(const char* path, int mode) {
    filepath node = to_filepath(path);
    int dirIdx = tree_lookup(node.crumbs);

    if (dirIdx < 0 || streq(node.file, "")) {
      return -ENOENT;
    }

    int newNodeIdx = alloc_inode();
    int newPageIdx = alloc_page();

    if (newNodeIdx < 0 || newPageIdx < 0) {
        if (newNodeIdx >= 0) {
            free_inode(newNodeIdx);
        }

        if (newPageIdx >= 0) {
            free_page(newPageIdx);
        }

        return -ENOSPC;
    }

    inode* newNode = get_inode(newNodeIdx);

    newNode->ptrs[0] = newPageIdx;
    newNode->mode = mode;

    int rv = directory_put(get_inode(dirIdx), node.file, newNodeIdx);
    assert(rv == 0);

    if (is_folder(mode) || is_link(mode)) {
      newNode->size = PAGE_SIZE;
    }

    return 0;
}

int storage_unlink(const char* path) {
  filepath fp = to_filepath(path);

  int dirIdx = tree_lookup(fp.crumbs);
  inode* dir = get_inode(dirIdx);
  dirent* fileEnt = directory_lookup(dir, fp.file);

  if (dirIdx < 0 || fileEnt == 0) {
    return -ENOENT;
  }

  inode* file = get_inode(fileEnt->inum);

  if (file->refs == 0) {
    if (is_folder(file->mode)) {
      char nestedPath[4096];
      strncpy(nestedPath, path, 4096);
      join_to_path(nestedPath, fp.file);
      int rv = storage_rmdir(nestedPath);
      if (rv != 0) {
        return rv;
      }
    }

    free_inode(fileEnt->inum);
  } else {
    file->refs--;
  }

  directory_delete(dir, fp.file);

  return 0;
}

int storage_link(const char* from, const char* to) {
  filepath fp = to_filepath(to);
  int fromIdx = tree_lookup(from);
  int toIdx = tree_lookup(to);
  int toParentIdx = tree_lookup(fp.crumbs);
  if (fromIdx < 0 || toParentIdx < 0) {
    return -ENOENT;
  }
  if (toIdx >= 0) {
    return -EEXIST;
  }

  inode* fromFile = get_inode(fromIdx);

  if (is_folder(fromFile->mode)) {
    return -EISDIR;
  }

  inode* toParent = get_inode(toParentIdx);

  int rv = directory_put(toParent, fp.file, fromIdx);

  if (rv == 0) {
    fromFile->refs++;
    return 0;
  } else {
    return rv;
  }
}

int storage_symlink(const char* to, const char* from) {
  int fromIdx = tree_lookup(from);
  if (fromIdx >= 0) {
    return -EEXIST;
  }

  int rv = storage_mknod(from, __S_IFLNK | 0777);

  if (rv < 0) {
    return rv;
  }

  fromIdx = tree_lookup(from);

  inode* link = get_inode(fromIdx);

  strncpy(pages_get_page(link->ptrs[0]), to, PAGE_SIZE);
  return 0;
}

int storage_readlink(const char* path, char* buf, size_t size) {
  int linkIdx = tree_lookup(path);
  if (linkIdx < 0) {
    return -ENOENT;
  }
  inode* link = get_inode(linkIdx);

  if (is_link(link->mode)) {
    size = min(size, PAGE_SIZE);
    strncpy(buf, pages_get_page(link->ptrs[0]), size);
    return 0;
  } else {
    return -EPERM;
  }
}

int storage_rename(const char *from, const char *to) {
  filepath fpOld = to_filepath(from);
  filepath fpNew = to_filepath(to);

  if (!streq(fpOld.crumbs, fpNew.crumbs)) {
    return -1;
  } else {
    int dirIdx = tree_lookup(fpOld.crumbs);

    dirent* entry = directory_lookup(get_inode(dirIdx), fpOld.file);

    if (entry > 0) {
      strncpy(entry->name, fpNew.file, 48);
      return 0;
  } else {
      return -ENOENT;
    }
  }
}

int storage_read(const char* path, char* buf, size_t size, off_t offset) {
  int fileIdx = tree_lookup(path);

  if (fileIdx < 0) {
    return -ENOENT;
  }

  inode* file = get_inode(fileIdx);

  if (is_folder(file->mode)) {
    return -EISDIR;
  }

  if ((file->mode & __S_IFREG) == __S_IFREG) {
    if (offset >= file->size) {
      return 0;
    }

    // Set maximum readable bytes, either the size or to the EOF, whichever is smaller
    size = size < file->size - offset ? size : file->size - offset;

    // Adjust for offsets across inodes
    while (offset >= 5 * PAGE_SIZE) {
      file = get_inode(file->iptr);
      offset -= 5 * PAGE_SIZE;
    }

    size_t bytesLeft = size;
    size_t bufOffset = 0;
    while (bytesLeft > 0) {
      // Read from direct pointers
      for (int ii = 0; ii < 5; ii++) {
        void* currentPage = pages_get_page(file->ptrs[ii]);
        if (offset >= PAGE_SIZE) {
          offset -= PAGE_SIZE;
        } else {
          int maxBytes = PAGE_SIZE - offset;
          int bytesRead = min(bytesLeft, maxBytes);
          memcpy(buf + bufOffset, currentPage + offset, bytesRead);
          offset = 0;
          bufOffset += bytesRead;
          bytesLeft -= bytesRead;
        }
      }

      // Continue reading from indirect nodes
      file = get_inode(file->iptr);
    }

    return size;
  }

  return -1;
}

int storage_write(const char* path, const char* buf, size_t size, off_t offset) {
  int fileIdx = tree_lookup(path);

  if (fileIdx < 0) {
    return -ENOENT;
  }

  inode* file = get_inode(fileIdx);

  if (is_folder(file->mode)) {
    return -EISDIR;
  }

  if ((file->mode & __S_IFREG) == __S_IFREG) {
    // Grow inode if it is too small to store data to write
    if (offset + size > file->size) {
      grow_inode(file, offset + size);
    }

    // Adjust for offsets across inodes
    while (offset >= 5 * PAGE_SIZE) {
      file = get_inode(file->iptr);
      offset -= 5 * PAGE_SIZE;
    }

    size_t bytesLeft = size;
    int bufOffset = 0;
    while (bytesLeft > 0) {
      // Write to direct pointers
      for (int ii = 0; ii < 5; ii++) {
        void* currentPage = pages_get_page(file->ptrs[ii]);
        if (offset >= PAGE_SIZE) {
          offset -= PAGE_SIZE;
        } else {
          int maxBytes = PAGE_SIZE - offset;
          int bytesRead = min(bytesLeft, maxBytes);
          memcpy(currentPage + offset, buf + bufOffset, bytesRead);
          offset = 0;
          bufOffset += bytesRead;
          bytesLeft -= bytesRead;
        }
      }

      // Continue writing to indirect nodes
      file = get_inode(file->iptr);
    }

    return size;
  }

  return -1;
}

int storage_truncate(const char *path, off_t size) {
  int fileIdx = tree_lookup(path);
  if (fileIdx < 0) {
    return -ENOENT;
  }
  
  inode* file = get_inode(fileIdx);

  if (size > file->size) {
    return grow_inode(file, size);
  } else {
    return shrink_inode(file, size);
  }
}

int storage_access(const char* path, int mask) {
  // TODO: Add support for other masks
  int rv = 0;

  if (mask = F_OK) {
    // If the file doesn't exist, return ENOENT
    rv = tree_lookup(path) < 0 ? -ENOENT : 0;
  }

  return rv;
}

int storage_set_time(const char* path, const struct timespec ts[2]) {
  int rv = 0;
  int fileIdx = tree_lookup(path);

  if (fileIdx < 0) {
    return -ENOENT;
  }

  inode* file = get_inode(fileIdx);
  file->atime = ts[0];
  file->mtime = ts[1];
  return rv;  
}

slist* storage_list(const char* path) {
  return directory_list(path);
}

int storage_mkdir(const char* path, mode_t mode) {
  int rv = storage_mknod(path, __S_IFDIR | mode);
  if (rv != 0) {
    return -ENOSPC;
  }

  filepath fp = to_filepath(path);

  int dirIdx = tree_lookup(path);
  int parentIdx = tree_lookup(fp.crumbs);

  inode* dir = get_inode(dirIdx);

  directory_put(dir, ".", dirIdx);
  directory_put(dir, "..", parentIdx);

  return rv;
}

int storage_rmdir(const char* path) {
  int dirIdx = tree_lookup(path);
  if (dirIdx < 0) {
    return -ENOENT;
  }

  filepath fp = to_filepath(path);
  int parentIdx = tree_lookup(fp.crumbs);

  inode* dir = get_inode(dirIdx);
  inode* parent = get_inode(parentIdx);

  if (is_folder(dir->mode)) {
    int rv;
    slist* contents = directory_list(path);

    while (contents != 0) {
      if (!streq(contents->data, ".") && !streq(contents->data, "..")) {
        char filePath[4096];
        strncpy(filePath, path, 4096);
        join_to_path(filePath, contents->data);
        rv = storage_unlink(filePath);
        if (rv != 0) {
          return rv;
        }
      }

      contents = contents->next;
    }

    free_inode(dirIdx);
    return directory_delete(parent, fp.file);
  } else {
    return -ENOTDIR;
  }
}

int storage_chmod(const char* path, mode_t mode) {
  int fileIdx = tree_lookup(path);

  if (fileIdx < 0) {
    return -ENOENT;
  }

  inode* file = get_inode(fileIdx);
  file->mode = file->mode | mode;
  file->ctime = time(NULL); 

  return 0;
}

filepath to_filepath(const char* path) {
    filepath ret;
    strcpy(ret.crumbs, "/");

    assert(*path == '/');

    slist* tokens = s_split(path, '/');

    while (tokens != 0) {
        if (tokens->next == 0) {
            strncpy(ret.file, tokens->data, 255);
        } else {
            join_to_path(ret.crumbs, tokens->data);
        }

        tokens = tokens->next;
    }

    s_free(tokens);

    return ret;
}
