#include <assert.h>
#include <errno.h>

#include "bitmap.h"
#include "pages.h"
#include "util.h"

#include "inode.h"

const int INODE_COUNT = 256; // assumed to be 4 pages, root currently needs to be page 5

void inodes_init() {
  void* pbm = get_pages_bitmap();
  if (!bitmap_get(pbm, 1)) {
    int pagesNeeded = bytes_to_pages(INODE_COUNT * sizeof(inode));
    for (int ii = 1; ii <= pagesNeeded; ii++) {
      // inode pages should be adjacent for one big contiguous block
      assert(alloc_page() == ii);
    }
  }
}

inode* get_inode(int inum) {
  if (inum >= 0 && inum < INODE_COUNT && bitmap_get(get_inode_bitmap(), inum)) {
    return ((inode*)pages_get_page(1)) + inum;
  } else {
    return 0;
  }
}

int alloc_inode() {
  void* ibm = get_inode_bitmap();

  for (int ii = 0; ii < INODE_COUNT; ii++) {
    if (!bitmap_get(ibm, ii)) {
      bitmap_put(ibm, ii, 1);
      printf("+ alloc_inode() -> %d\n", ii);
      return ii;
    }
  }

  return -ENOSPC;
}

void free_inode(int inum) {
  inode* node = get_inode(inum);

  if (inum == 0 || node == 0) {
    return;
  }

  for (int ii = 0; ii < 5; ii++) {
    if (node->ptrs[ii] != 0) {
      free_page(node->ptrs[ii]);
      node->ptrs[ii] = 0;
    }
  }

  free_inode(node->iptr);

  node->size = 0;
  node->mode = 0;
  node->iptr = 0;

  bitmap_put(get_inode_bitmap(), inum, 0);
}

int grow_inode(inode* node, int size) {
  assert(node->size <= size);

  // Target size is larger than direct pages
  if (size > 5 * PAGE_SIZE) {
    // Add indirect node if nonexistent
    if (node->iptr == 0) {
      int newNodeIdx = alloc_inode();

      if (newNodeIdx < 0) {
        return -ENOENT;
      }

      inode* newNode = get_inode(newNodeIdx);
      newNode->mode = node->mode;
      node->iptr = newNodeIdx;
    }

    node->size = size;

    return grow_inode(get_inode(node->iptr), size - 5 * PAGE_SIZE);
  }

  int pagesNeeded = bytes_to_pages(size);

  for (int ii = 0; ii < pagesNeeded; ii++) {
    // Add page to node if nonexistent
    if (node->ptrs[ii] == 0) {
      int newPageIdx = alloc_page();
      if (newPageIdx < 0) {
        return -ENOSPC;
      }
      node->ptrs[ii] = newPageIdx;
    }
  }

  node->size = size;

  return 0;
}

int shrink_inode(inode* node, int size) {
  assert(node->size >= size);

  node->size = size;

  // Target size is larger than direct pages, adjust indirect node
  if (size > 5 * PAGE_SIZE) {
    return shrink_inode(get_inode(node->iptr), size - 5 * PAGE_SIZE);
  }

  int pagesNeeded = bytes_to_pages(size);

  // Free unused direct pages
  for (int ii = 4; ii >= pagesNeeded; ii--) {
    if (node->ptrs[ii] != 0) {
      free_page(node->ptrs[ii]);
      node->ptrs[ii] = 0;
    }
  }

  // Free unused indirect nodes
  if (node->iptr > 0) {
    free_inode(node->iptr);
  }

  return 0;
}

// Currently unused, calculates the total references for an inode and all connected indirect nodes
// since each inode only stores the number of references for its direct pointers
int total_refs(inode* node) {
  int refs = 0;

  while (node != 0) {
    refs += node->refs;
    if (node->iptr != 0) {
      node = get_inode(node->iptr);
    } else {
      node = 0;
    }
  }

  return refs;
}