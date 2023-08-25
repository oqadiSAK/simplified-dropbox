#include "../include/client_handler.h"

void *client_handler(void *arg)
{
    // Cast the argument to the appropriate type
    worker_thread_argument_t *worker_thread_argument = (worker_thread_argument_t *)arg;
    client_queue_t *client_queue = worker_thread_argument->client_queue;
    tracking_system_t *tracking_system = worker_thread_argument->tracking_system;

    while (1)
    {
        if (queue_check_signal(client_queue))
            return NULL;
        // Dequeue a client_info_t structure from the queue
        // Mutexes are used in the queue implementation
        client_info_t *client_info = client_queue_dequeue(client_queue);
        if (queue_check_signal(client_queue))
            return NULL;
        printf("Accepted client %s:%d\n", client_info->ip, client_info->port);
        pthread_mutex_lock(&comm_lock);
        int client_socket = client_info->socket;
        req_t init_req;
        memset(&init_req, 0, sizeof(req_t));
        ssize_t init_recieved = recv(client_socket, &init_req, sizeof(req_t), 0);
        if (init_recieved == -1)
        {
            perror("recv");
            exit(1);
        }
        else if (init_recieved == 0)
        {
            // Client disconnected
            break;
        }

        if (init_req.status != INIT)
        {
            exit(1);
        }
        else
        {
            on_init_req(init_req, client_info, client_socket);
        }
        pthread_mutex_lock(&tracking_system->tracking_mutex);
        send_initial_tracking_system(tracking_system, client_socket);
        pthread_mutex_unlock(&tracking_system->tracking_mutex);
        pthread_mutex_unlock(&comm_lock);
        while (1)
        {
            req_t req;
            memset(&req, 0, sizeof(req_t));
            ssize_t received = recv(client_socket, &req, sizeof(req_t), 0);
            pthread_mutex_lock(&comm_lock);
            if (received == -1)
            {
                perror("recv");
                pthread_mutex_unlock(&comm_lock);
                exit(1);
            }
            else if (received == 0)
            {
                pthread_mutex_unlock(&comm_lock);
                break;
            }

            // Process the received req
            switch (req.status)
            {
            case SHUT_DOWN:
                pthread_mutex_unlock(&comm_lock);
                return NULL;
            case QUIT:
            {
                send_quit_req(client_socket);
                remove_running_client(client_queue, client_info);
                printf("Client %s:%d disconnected\n", client_info->ip, client_info->port);
                pthread_mutex_unlock(&comm_lock);
                break;
            }
            case GET:
            {
                on_get_req(req, client_socket);
                break;
            }
            case UPDATE:
            {
                handle_create_or_update(UPDATE, req, tracking_system, client_socket, client_queue, client_info);
                break;
            }
            case DELETE:
            {
                handle_delete(req, tracking_system, client_socket, client_queue, client_info);
                break;
            }
            case CREATE:
            {
                handle_create_or_update(CREATE, req, tracking_system, client_socket, client_queue, client_info);
                break;
            }
            default:
                break;
            }
            pthread_mutex_unlock(&comm_lock);
        }
        // Close the client socket
        /* close(client_socket); */
        free(client_info);
    }

    // Exit the thread
    return NULL;
}

void send_initial_tracking_system(tracking_system_t *tracking_system, int client_socket)
{
    // Send the tracking_system struct
    send_chunk_by_chunk(client_socket, tracking_system, sizeof(tracking_system_t));

    // Send tracked_files data
    for (int i = 0; i < tracking_system->num_tracked_files; i++)
    {
        send_chunk_by_chunk(client_socket, &(tracking_system->tracked_files[i]), sizeof(tracked_file_t));
    }
}

void send_chunk_by_chunk(int client_socket, const void *data, size_t dataSize)
{
    size_t bytesSent = 0;
    while (bytesSent < dataSize)
    {
        // Calculate the number of bytes remaining to send
        size_t remainingBytes = dataSize - bytesSent;
        size_t chunkSize = (remainingBytes < CHUNK_SIZE) ? remainingBytes : CHUNK_SIZE;

        // Send the chunk of data
        ssize_t sent = send(client_socket, ((const char *)data) + bytesSent, chunkSize, 0);
        if (sent == -1)
        {
            perror("send");
            exit(1);
        }

        // Update the number of bytes sent
        bytesSent += sent;
    }
}

void handle_create_or_update(request_status_t status, req_t req, tracking_system_t *tracking_system,
                             int client_socket, client_queue_t *client_queue, client_info_t *curr_client_info)
{
    on_create_or_update_req(req, client_socket, tracking_system->dir_path, tracking_system, status);
    for (int i = 0; i < client_queue->running_count; i++)
    {
        client_info_t *client = client_queue->running_clients[i];
        if (client->socket != client_socket)
        {
            send_create_or_update_req(req.payload.create_or_update_req.tracked_file, curr_client_info->dir_path, client->socket, status);
        }
    }
}

void handle_delete(req_t req, tracking_system_t *tracking_system,
                   int client_socket, client_queue_t *client_queue, client_info_t *curr_client_info)
{
    on_delete_req(req, tracking_system->dir_path, tracking_system);
    for (int i = 0; i < client_queue->running_count; i++)
    {
        client_info_t *client = client_queue->running_clients[i];
        if (client->socket != client_socket)
        {
            send_delete_req(req.payload.delete_req.tracked_file, curr_client_info->dir_path, client->socket);
        }
    }
}
