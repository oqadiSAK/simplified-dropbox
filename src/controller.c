#include "../include/controller.h"

int send_init_req(int socket, const char *dir_path)
{
    req_t req;
    memset(&req, 0, sizeof(req_t));
    req.status = INIT;
    strncpy(req.payload.init_req.client_dir_path, dir_path, MAX_PATH_LEN);

    // Send the request to the server
    ssize_t sent = send(socket, &req, sizeof(req_t), 0);
    if (sent == -1)
    {
        perror("send");
        return -1;
    }

    res_t res;

    while (1)
    {
        memset(&res, 0, sizeof(res_t));
        res.status = PENDING;
        ssize_t received = recv(socket, &res, sizeof(res_t), 0);
        if (received == -1)
        {
            perror("recv");
            return -1;
        }

        if (res.status != PENDING)
            break;
    }
    return 0;
}

int send_quit_req(int socket)
{
    req_t req;
    memset(&req, 0, sizeof(req_t));
    req.status = QUIT;
    req.payload.quit_req.quit = 1;
    // Send the request to the server
    ssize_t sent = send(socket, &req, sizeof(req_t), 0);
    if (sent == -1)
    {
        perror("send");
        return -1;
    }
    return 0;
}

int send_shut_down_req(int socket)
{
    req_t req;
    memset(&req, 0, sizeof(req_t));
    req.status = SHUT_DOWN;
    req.payload.shut_down_req.shut_down = 1;
    // Send the request to the server
    ssize_t sent = send(socket, &req, sizeof(req_t), 0);
    if (sent == -1)
    {
        perror("send");
        return -1;
    }
    return 0;
}

int send_get_req(tracked_file_t file, const char *filepath, int socket)
{
    req_t req;
    memset(&req, 0, sizeof(req_t));
    req.status = GET;
    req.payload.get_req.tracked_file = file;

    // Send the request to the server
    ssize_t sent = send(socket, &req, sizeof(req_t), 0);
    if (sent == -1)
    {
        return -1;
    }

    // Receive and append chunks of data to the file
    res_t res;
    // Append the received data chunk to the file
    int file_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0777);

    if (file_fd == -1)
    {
        return -1;
    }
    while (1)
    {
        memset(&res, 0, sizeof(res_t));
        res.status = PENDING;
        ssize_t received = recv(socket, &res, sizeof(res_t), 0);
        if (received == -1)
        {
            break;
        }

        if (res.status != PENDING)
        {
            break;
        }

        ssize_t bytes_written = write(file_fd, res.data, res.data_length);
        if (bytes_written == -1)
        {
            perror("write");
            break;
        }
    }
    close(file_fd);
    return 0;
}

int send_create_or_update_req(tracked_file_t new_file, char *cllient_dir_path, int socket, request_status_t status)
{
    // Send create request to the server
    req_t req;
    memset(&req, 0, sizeof(req_t));
    req.status = status;
    req.payload.create_or_update_req.tracked_file = new_file;
    strcpy(req.payload.create_or_update_req.client_dir_path, cllient_dir_path);

    ssize_t sent = send(socket, &req, sizeof(req_t), 0);
    if (sent == -1)
    {
        return -1;
    }

    // Open the file for reading
    int file_fd = open(new_file.path, O_RDONLY);
    if (file_fd == -1)
    {
        return -1;
    }
    // Read and send the file data in chunks
    char buffer[CHUNK_SIZE];
    ssize_t bytes_read;
    res_t res;
    while ((bytes_read = read(file_fd, buffer, CHUNK_SIZE)) > 0)
    {
        memset(&res, 0, sizeof(res_t));
        res.status = PENDING;
        res.data_length = bytes_read;
        memcpy(res.data, buffer, bytes_read);
        res.data[bytes_read] = '\0'; // Add null terminator
        send(socket, &res, sizeof(res_t), 0);
        memset(buffer, 0, CHUNK_SIZE);
    }
    memset(&res, 0, sizeof(res_t));
    res.status = OK;
    send(socket, &res, sizeof(res_t), 0);
    close(file_fd);
    return 0;
}

int send_delete_req(tracked_file_t file, char *cllient_dir_path, int socket)
{
    req_t req;
    memset(&req, 0, sizeof(req_t));
    req.status = DELETE;
    req.payload.delete_req.tracked_file = file;
    strcpy(req.payload.delete_req.client_dir_path, cllient_dir_path);

    // Send the request to the server
    ssize_t sent = send(socket, &req, sizeof(req_t), 0);
    if (sent == -1)
    {
        perror("send");
        return -1;
    }
    return 0;
}

void on_init_req(req_t req, client_info_t *client_info, int socket)
{
    init_req_t *init_req = &(req.payload.init_req);
    strncpy(client_info->dir_path, init_req->client_dir_path, MAX_PATH_LEN);
    res_t res;
    memset(&res, 0, sizeof(res_t));
    res.status = OK;
    send(socket, &res, sizeof(res_t), 0);
}

void on_get_req(req_t req, int socket)
{
    // Handle GET req
    get_req_t *get_req = &(req.payload.get_req);

    // Open the file for reading
    int file_fd = open(get_req->tracked_file.path, O_RDONLY, 0777);
    if (file_fd == -1)
    {
        return;
    }

    // Read and send the file data in chunks
    char buffer[CHUNK_SIZE];
    ssize_t bytes_read;
    res_t res;
    while ((bytes_read = read(file_fd, buffer, CHUNK_SIZE)) > 0)
    {
        memset(&res, 0, sizeof(res_t));
        res.status = PENDING;
        res.data_length = bytes_read;
        memcpy(res.data, buffer, bytes_read);
        send(socket, &res, sizeof(res_t), 0);
    }
    memset(&res, 0, sizeof(res_t));
    res.status = OK;
    send(socket, &res, sizeof(res_t), 0);
    close(file_fd);
}

void on_create_or_update_req(req_t req, int socket, char *dir_name, tracking_system_t *tracking_system, request_status_t status)
{
    create_or_update_req_t *create_or_update_req = &(req.payload.create_or_update_req);
    tracked_file_t new_file = create_or_update_req->tracked_file;
    char *client_dir_path = create_or_update_req->client_dir_path;
    char filepath[MAX_PATH_LEN];
    construct_file_path(new_file.path, client_dir_path, filepath, dir_name);

    pthread_mutex_lock(&tracking_system->tracking_mutex);
    if (new_file.is_dir == 1)
    {
        struct stat st = {0};
        if (stat(filepath, &st) == -1)
        {
            if (mkdir(filepath, 0777) == -1)
            {
                pthread_mutex_unlock(&tracking_system->tracking_mutex);
                return;
            }
        }
        res_t res;
        memset(&res, 0, sizeof(res_t));
        res.status = PENDING;
        ssize_t received = recv(socket, &res, sizeof(res_t), 0);
        if (received == -1 || res.status != OK)
        {
            perror("recv");
            exit(1);
        }
        update_tracking_system(tracking_system, filepath, status);
    }
    else
    {
        // Create or overwrite the file
        int file_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (file_fd == -1)
        {
            if (errno == ENOENT)
            {
                char *dir_path = strdup(filepath);
                char *dir_name = dirname(dir_path);
                int success = create_nested_directory(dir_name);
                free(dir_path);
                if (!success)
                {
                    pthread_mutex_unlock(&tracking_system->tracking_mutex);
                    return;
                }
                else
                {
                    file_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0777);
                }
            }
            else
            {
                pthread_mutex_unlock(&tracking_system->tracking_mutex);
                return;
            }
        }

        // Read and receive chunks of data from the client
        res_t res;
        while (1)
        {
            memset(&res, 0, sizeof(res_t));
            res.status = PENDING;
            ssize_t received = recv(socket, &res, sizeof(res_t), 0);
            if (received == -1)
            {
                perror("recv");
                exit(1);
            }

            if (res.status != PENDING)
                break;

            // Write the received data chunk to the file
            ssize_t bytes_written = write(file_fd, res.data, res.data_length);
            if (bytes_written == -1)
            {
                continue;
            }
        }

        update_tracking_system(tracking_system, filepath, status);
        close(file_fd);
    }
    pthread_mutex_unlock(&tracking_system->tracking_mutex);
}

void on_delete_req(req_t req, char *dir_name, tracking_system_t *tracking_system)
{
    // Handle DELETE req
    delete_req_t *delete_req = &(req.payload.delete_req);
    tracked_file_t deleted_file = delete_req->tracked_file;
    char *client_dir_path = delete_req->client_dir_path;

    char filepath[MAX_PATH_LEN];
    construct_file_path(deleted_file.path, client_dir_path, filepath, dir_name);
    pthread_mutex_lock(&tracking_system->tracking_mutex);
    if (deleted_file.is_dir)
    {
        remove_directory(tracking_system, filepath);
    }
    else
    {
        remove_tracked_file(tracking_system, filepath);
        unlink(filepath);
    }

    pthread_mutex_unlock(&tracking_system->tracking_mutex);
}

void remove_directory(tracking_system_t *tracking_system, const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char sub_path[MAX_PATH_LEN];
        snprintf(sub_path, sizeof(sub_path), "%s/%s", dir_path, entry->d_name);

        if (entry->d_type == DT_DIR)
        {
            remove_directory(tracking_system, sub_path); // Recursively remove subdirectory
        }
        else
        {
            remove_tracked_file(tracking_system, sub_path);
            unlink(sub_path);
        }
    }

    closedir(dir);
    remove_tracked_file(tracking_system, dir_path);
    rmdir(dir_path);
}