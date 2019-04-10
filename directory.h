// based on cs3650 starter code

#ifndef DIRECTORY_H
#define DIRECTORY_H

#define DIR_NAME 48

#include "slist.h"
#include "pages.h"
#include "inode.h"

typedef struct dirent {
    char name[DIR_NAME];
    int  inum;
    char _reserved[12];
} dirent;

/**
 * @brief Creates /, the root directory.
 */
void directory_init();

/**
 * @brief Finds the dirent that matches the filename in the given folder's inode.
 *        Files cannot have the same name.
 * 
 * @param dd the inode corresponding to the parent directory
 * @param name the filename to find
 * @return dirent* the directory entry, returns 0 if file not found
 */
dirent* directory_lookup(inode* dd, const char* name);

/**
 * @brief Finds the index of the inode that corresponds to the path.
 * 
 * @param path the file to find
 * @return int the index of the file's inode. Returns ENOENT if not found
 */
int tree_lookup(const char* path);

/**
 * @brief Adds an entry to the directory.
 * 
 * @param dd the inode of the parent directory
 * @param name the name of the file to add
 * @param inum the inode of the file
 * @return int 0 if successful, ENOSPC if no pages or inodes are left
 */
int directory_put(inode* dd, const char* name, int inum);

/**
 * @brief Deletes an entry from a directory.
 * 
 * @param dd the inode of the parent directory
 * @param name The name of the file to remove
 * @return int 0 if successful, ENOENT if file not found
 */
int directory_delete(inode* dd, const char* name);

/**
 * @brief Creates a list of items in a directory.
 * 
 * @param path the path of the directory
 * @return slist* the list of items in the directory
 */
slist* directory_list(const char* path);

#endif

