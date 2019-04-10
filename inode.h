// based on cs3650 starter code

#ifndef INODE_H
#define INODE_H

#include <sys/types.h>

#include "pages.h"

typedef struct inode {
    int refs; // reference count
    mode_t mode; // permission & type
    int size; // bytes
    int ptrs[5]; // direct pointers
    int iptr; // single indirect pointer
    time_t atime; // last access time
    time_t mtime; // last modification time
    time_t ctime; // last change time
} inode;

/**
 * @brief Reserves the pages needed for inodes
 */
void inodes_init();

/**
 * @brief Get the inode of the specified index
 * 
 * @param inum the index of the inode
 * @return inode* the inode at the index, returns 0 if index is invalid
 */
inode* get_inode(int inum);

/**
 * @brief Allocates a free inode.
 * 
 * @return int the index of the allocated inode, ENOSPC if out of available inodes
 */
int alloc_inode();

/**
 * @brief Frees the inode at the index, along with any associated pages and indirect nodes.
 * 
 * @param inum the index of the inode
 */
void free_inode(int inum);

/**
 * @brief Increases inode's size.
 * 
 * @param node the inode to grow
 * @param size the target size (must be >= to inode's size)
 * @return int 0 if successful, ENOSPC if out of space
 */
int grow_inode(inode* node, int size);

/**
 * @brief Decreases inode's size.
 * 
 * @param node the inode to shrink
 * @param size the target size (must be <= to inode's size)
 * @return int 0 if successful
 */
int shrink_inode(inode* node, int size);

#endif
