#ifndef DIR_WATCHER_H
#define DIR_WATCHER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "protocol.h"
#include "helpers.h"

void init_tracking_system(tracking_system_t *tracking_system, const char *dir_path, char *log_file_path);
void fill_tracking_system(tracking_system_t *tracking_system);
void fill_tracking_system_helper(tracking_system_t *tracking_system, const char *dir_path);
void check_statuses(tracking_system_t *tracking_system);
void check_statuses_helper(tracking_system_t *tracking_system, const char *dir_path);
void check_deletion(tracking_system_t *tracking_system);
void remove_tracked_file(tracking_system_t *tracking_system, const char *file_path);
tracked_file_t *find_tracked_file(tracking_system_t *tracking_system, const char *file_path);
void add_tracked_file(tracking_system_t *tracking_system, const char *file_path, time_t mtime, int is_dir);
void update_tracking_system(tracking_system_t *tracking_system, char *filepath, request_status_t status);
void check_modification(tracked_file_t *tracked_file, time_t mtime);
void tracking_system_set_signal(tracking_system_t *tracking_system, char *signal_str);
int tracking_system_check_signal(tracking_system_t *tracking_system, int lock);
void tracking_system_set_shutdown(tracking_system_t *tracking_system);
int tracking_system_check_shutdown(tracking_system_t *tracking_system, int lock);
void destroy_tracking_system(tracking_system_t *tracking_system);

#endif
