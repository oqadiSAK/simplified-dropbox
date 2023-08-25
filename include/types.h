#ifndef TYPES_H
#define TYPES_H

#include <pthread.h>
#include <netinet/in.h>
#include <signal.h>

#define BACKLOG_LIMIT 128
#define MAX_PORT_NUMBER 65535
#define MAX_PATH_LEN 4096
#define MAX_FILENAME_LEN 256
#define CHUNK_SIZE 4096

typedef struct
{
    char ip[INET_ADDRSTRLEN];
    int port;
    int socket;
    char dir_path[MAX_PATH_LEN];
} client_info_t;

typedef struct
{
    pthread_cond_t enqueue_cv;
    pthread_cond_t dequeue_cv;
    pthread_mutex_t lock;

    int front;
    int rear;
    int running_count;
    int capacity;
    int size;
    volatile sig_atomic_t signal_received;
    char *signal_str;

    client_info_t **data;
    client_info_t **running_clients;
} client_queue_t;

typedef enum
{
    STABLE,
    CREATED,
    DELETED,
    UPDATED,
} file_status_t;

typedef struct
{
    char path[MAX_PATH_LEN];
    time_t modified_time;
    int is_dir;
    file_status_t status;
} tracked_file_t;

typedef struct
{
    char dir_path[MAX_PATH_LEN];
    int num_tracked_files;
    tracked_file_t *tracked_files;
    pthread_mutex_t tracking_mutex;
    volatile sig_atomic_t signal_received;
    volatile sig_atomic_t shut_down;
    char *signal_str;
    char log_file_path[MAX_PATH_LEN];
} tracking_system_t;

typedef struct
{
    tracking_system_t *tracking_system;
    client_queue_t *client_queue;
} worker_thread_argument_t;

#endif