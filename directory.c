#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

#include "bitmap.h"
#include "inode.h"
#include "pages.h"
#include "slist.h"
#include "util.h"

#include "directory.h"

const int DIRENT_COUNT = 4096 / sizeof(dirent); // max dirents per page

void directory_init() {
  // First inode is always reserved for root
  if (!bitmap_get(get_inode_bitmap(), 0)) {
    int rootPage = alloc_page();
    assert(rootPage == 5); // Should probably move the data start page to const

    int rootIdx = alloc_inode();
    assert(rootIdx == 0);

    inode* root = get_inode(rootIdx);
    root->mode = __S_IFDIR | 0755;
    root->ptrs[0] = 5;
    root->size = 4096;
    directory_put(root, ".", rootIdx);
    directory_put(root, "..", rootIdx); // .. points to itself, should point to folder above mnt?
  }
}

dirent* directory_lookup(inode* dd, const char* name) {
  while (dd > 0) {
    // Verify inode is a directory
    if ((dd->mode & __S_IFDIR) == __S_IFDIR) {
      // Check direct pointers for file
      for (int ii = 0; ii < 5; ii++) {
        int pageIdx = dd->ptrs[ii];
        if (pageIdx > 0) {
          dirent* dir = pages_get_page(pageIdx);

          for (int jj = 0; jj < DIRENT_COUNT; jj++) {
            if (streq(name, dir[jj].name)) {
              return &(dir[jj]);
            }
          }
        }
      }

      // Switch to indirect pointer if not found
      if (dd->iptr > 0) {
        dd = get_inode(dd->iptr);
      } else {
        break;
      }
    } else {
      // inode is not a directory
      break;
    }
  }

  return 0;
}

int tree_lookup(const char* path) {
  if (strcmp(path, "/") == 0) {
    return 0;
  }

  int currentInode = 0;
  slist* crumbs = s_split(path, '/');

  while (crumbs != 0) {
    dirent* entry = directory_lookup(get_inode(currentInode), crumbs->data);
    if (entry == 0) {
      s_free(crumbs);
      return -ENOENT;
    }
    currentInode = entry->inum;
    crumbs = crumbs->next;
  }

  s_free(crumbs);
  return currentInode;
}

int directory_put(inode* dd, const char* name, int inum) {
  while (dd != 0) {
    // Check direct pointers
    for (int ii = 0; ii < 5; ii++) {
      // Directory out of space, add more pages
      if (dd->ptrs[ii] == 0) {
        int newPageIdx = alloc_page();

        if (newPageIdx < 0) {
          return -ENOSPC;
        }

        dd->ptrs[ii] = newPageIdx;
        dd->size += PAGE_SIZE;
      }

      dirent* entries = pages_get_page(dd->ptrs[ii]);
      for (int jj = 0; jj < DIRENT_COUNT; jj++) {
        // Find entry with empty name
        if (entries[jj].name[0] == 0) {
          strncpy(entries[jj].name, name, 48);
          entries[jj].inum = inum;
          return 0;
        }
      }
    }

    // Out of space in direct pointers, add indirect node
    // TODO: Propagate size changes to top inode
    if (dd->iptr == 0) {
      int rv = grow_inode(dd, dd->size + PAGE_SIZE);
    }

    dd = get_inode(dd->iptr);
  }

  return -ENOSPC;
}

int directory_delete(inode* dd, const char* name) {
  dirent* entry = directory_lookup(dd, name);
  if (entry > 0) {
    entry->inum = 0;
    memset(entry->name, 0, 48); // I could probably just set the first char to 0, but
                                // that could have security implications
    return 0;
  } else {
    return -ENOENT;
  }
}

slist* directory_list(const char* path) {
  int dirIdx = tree_lookup(path);
  if (dirIdx < 0) {
    return 0;
  }

  inode* dir = get_inode(dirIdx);

  // Verify inode is directory
  if ((dir->mode & __S_IFDIR) != __S_IFDIR) {
    return 0;
  }

  slist* contents = 0;

  while (dir != 0) {
    // Check direct pointers for entries
    for (int ii = 0; ii < 5; ii++) {
      if (dir->ptrs[ii] != 0) {
        dirent* currentPage = pages_get_page(dir->ptrs[ii]);
        for (int jj = 0; jj < DIRENT_COUNT; jj++) {
          if (currentPage[jj].name[0] != 0) {
            contents = s_cons(currentPage[jj].name, contents);
          }
        }
      }
    }

    if (dir->iptr != 0) {
      // Check indirect pointer
      dir = get_inode(dir->iptr);
    } else {
      dir = 0;
    }
  }

  return contents;
}