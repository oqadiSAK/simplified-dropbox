#ifndef req_H
#define req_H

#include "types.h"

typedef enum
{
    INIT,
    GET,
    UPDATE,
    DELETE,
    CREATE,
    QUIT,
    SHUT_DOWN
} request_status_t;

typedef enum
{
    OK,
    PENDING,
} response_status_t;

typedef struct
{
    char client_dir_path[MAX_PATH_LEN];
} init_req_t;

typedef struct
{
    tracked_file_t tracked_file;
} get_req_t;

typedef struct
{
    tracked_file_t tracked_file;
    char client_dir_path[MAX_PATH_LEN];
} delete_req_t;

typedef struct
{
    tracked_file_t tracked_file;
    char client_dir_path[MAX_PATH_LEN];
} create_or_update_req_t;

typedef struct
{
    int quit;
} quit_req_t;

typedef struct
{
    int shut_down;
} shut_down_req_t;

typedef struct
{
    request_status_t status;
    union
    {
        init_req_t init_req;
        get_req_t get_req;
        delete_req_t delete_req;
        create_or_update_req_t create_or_update_req;
        quit_req_t quit_req;
        shut_down_req_t shut_down_req;
    } payload;
} req_t;

typedef struct
{
    response_status_t status;
    ssize_t data_length;
    char data[CHUNK_SIZE];
} res_t;

#endif