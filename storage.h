// based on cs3650 starter code

#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "slist.h"

typedef struct filepath {
    char file[255]; // max length of a filename is 255 chars
    char crumbs[4096]; // max length of a pathname is 4096 chars
} filepath;

void   storage_init(const char* path);
int    storage_stat(const char* path, struct stat* st);
int    storage_read(const char* path, char* buf, size_t size, off_t offset);
int    storage_write(const char* path, const char* buf, size_t size, off_t offset);
int    storage_truncate(const char *path, off_t size);
int    storage_mknod(const char* path, int mode); 
int    storage_unlink(const char* path);
int    storage_link(const char *from, const char *to);
int    storage_symlink(const char* to, const char* from);
int    storage_readlink(const char* path, char* buf, size_t size);
int    storage_rename(const char *from, const char *to);
int    storage_set_time(const char* path, const struct timespec ts[2]);
int    storage_access(const char*path, int mask);
slist* storage_list(const char* path);
int    storage_mkdir(const char* path, mode_t mode);
int    storage_rmdir(const char* path);
int    storage_chmod(const char* path, mode_t mode);

/**
 * @brief Converts a full path into a filepath struct for easier access to the file's
 *        name and the breadcrumbs leading up to it.
 * 
 * @param path the full path of the file
 * @return filepath the filepath struct representation of the file
 */
filepath to_filepath(const char* path);

#endif
