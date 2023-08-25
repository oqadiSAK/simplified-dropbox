#ifndef HELPERS_H
#define HELPERS_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
#include "types.h"

void construct_file_path(const char *base_path, const char *relative_path, char *filepath, char *dir_name);
int lock_file(int fd);
int unlock_file(int fd);
int check_file_lock(const char *filename);
void block_thread_signals(sigset_t *signal_set);
void check_directory(const char *directory);
int create_nested_directory(const char *path);

#endif