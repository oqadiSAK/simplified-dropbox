#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "include/types.h"
#include "include/client_queue.h"
#include "include/client_handler.h"
#include "include/tracking_system.h"

void check_usage(int argc, char *argv[]);
void set_socket();
void init();
int create_sighandler_thread();
int create_monitor_thread();
void wait_threads();
void process_connection_req();
void *dir_monitor(void *arg);
void send_req_to_all_clients(request_status_t status, tracked_file_t *tracked_file);
void *signal_handler_thread(void *arg);
void clean_up();

char *directory;
int thread_pool_size, port_number, server_socket, counter_handler_thread;
client_queue_t *client_queue;
pthread_t *handler_threads, monitor_thread, signal_thread;
tracking_system_t *tracking_system;
sigset_t signal_set;
worker_thread_argument_t *worker_thread_argument;
pthread_mutex_t comm_lock;

int main(int argc, char *argv[])
{
    check_usage(argc, argv);
    create_sighandler_thread();
    set_socket();
    init();
    process_connection_req();
    clean_up();
    return 0;
}

void check_usage(int argc, char *argv[])
{
    // Check the number of arguments
    if (argc != 4)
    {
        printf("Usage: %s [directory] [thread_pool_size] [port_number]\n", argv[0]);
        exit(1);
    }

    directory = argv[1];
    check_directory(directory);
    thread_pool_size = atoi(argv[2]);
    port_number = atoi(argv[3]);

    // Check if thread_pool_size is valid
    if (thread_pool_size <= 0)
    {
        printf("Invalid thread pool size argument. Please provide a positive integer.\n");
        exit(1);
    }

    // Check if port_number is valid
    if (port_number <= 0 || port_number > MAX_PORT_NUMBER)
    {
        printf("Invalid port number argument. Please provide a valid port number in the range [1-65535].\n");
        exit(1);
    }
}

void set_socket()
{
    // Set up the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Error opening socket");
        exit(1);
    }

    // Set up the server address
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_number);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the server address
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Error binding socket");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(server_socket, BACKLOG_LIMIT) < 0)
    {
        perror("Error listening for connections");
        exit(1);
    }

    printf("Server started. Listening for connections...\n");
}

void init()
{
    int i;
    worker_thread_argument = NULL;
    pthread_mutex_init(&comm_lock, NULL);
    // Init client queue
    client_queue = malloc(sizeof(client_queue_t));
    client_queue_init(client_queue, thread_pool_size);
    counter_handler_thread = 0;

    // Init tracking system
    tracking_system = malloc(sizeof(tracking_system_t));
    memset(tracking_system, 0, sizeof(tracking_system_t));
    init_tracking_system(tracking_system, directory, NULL);

    // Init threads
    handler_threads = (pthread_t *)malloc(thread_pool_size * sizeof(pthread_t));
    if (handler_threads == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for handler threads\n");
        exit(EXIT_FAILURE);
    }

    worker_thread_argument = malloc(sizeof(worker_thread_argument_t));
    worker_thread_argument->client_queue = client_queue;
    worker_thread_argument->tracking_system = tracking_system;

    for (i = 0; i < thread_pool_size; i++)
    {
        if (queue_check_signal(client_queue))
            break;
        if (pthread_create(&handler_threads[i], NULL, client_handler, worker_thread_argument) != 0)
        {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        counter_handler_thread++;
    }
    create_monitor_thread();
}

int create_sighandler_thread()
{
    block_thread_signals(&signal_set);
    if (pthread_create(&signal_thread, NULL, signal_handler_thread, &signal_set) != 0)
    {
        fprintf(stderr, "Error creating thread\n");
        return -1;
    }
    return 0;
}

int create_monitor_thread()
{
    if (pthread_create(&monitor_thread, NULL, dir_monitor, (void *)directory) != 0)
    {
        fprintf(stderr, "Error creating thread\n");
        return -1;
    }
    return 0;
}

void wait_threads()
{
    int i;
    for (i = 0; i < counter_handler_thread; i++)
    {
        pthread_join(handler_threads[i], NULL);
    }
}

void process_connection_req()
{
    // Accept and handle client connections
    while (1)
    {
        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);

        // Accept a client connection
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        if (queue_check_signal(client_queue) == 1)
        {
            return;
        }
        if (client_socket < 0)
        {
            perror("Error accepting connection");
            continue;
        }

        // Retrieve client information
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_address.sin_addr), client_ip, INET_ADDRSTRLEN);
        int clientPort = ntohs(client_address.sin_port);
        printf("Connection request from %s:%d\n", client_ip, clientPort);
        fflush(stdout);

        // Build client info struct
        client_info_t *client_info = malloc(sizeof(client_info_t));
        strncpy(client_info->ip, client_ip, INET_ADDRSTRLEN);
        client_info->port = clientPort;
        client_info->socket = client_socket;

        int connection_value = 0;
        if (client_queue->running_count >= thread_pool_size)
        {
            printf("Que full, connection is suspended\n");
        }
        else
        {
            connection_value = 1;
        }
        send(client_socket, &connection_value, sizeof(int), 0);
        // Push the client_info_t structure to the queue buffer
        client_queue_enqueue(client_queue, client_info);
    }
}

void *dir_monitor(void *arg)
{
    const char *dir_path = (const char *)arg;
    destroy_tracking_system(tracking_system);
    init_tracking_system(tracking_system, dir_path, NULL);

    while (1)
    {
        pthread_mutex_lock(&comm_lock);
        if (queue_check_signal(client_queue) == 1)
        {
            break;
        }
        check_statuses(tracking_system);
        check_deletion(tracking_system);

        if (queue_check_signal(client_queue) == 1)
        {
            pthread_mutex_unlock(&comm_lock);
            break;
        }
        int i;
        tracked_file_t *tracked_file = NULL;
        for (i = 0; i < tracking_system->num_tracked_files; ++i)
        {
            if (queue_check_signal(client_queue) == 1)
            {
                pthread_mutex_unlock(&comm_lock);
                return NULL;
            }
            tracked_file = &tracking_system->tracked_files[i];
            if (tracked_file == NULL)
            {
                pthread_mutex_unlock(&comm_lock);
                continue;
            }
            else if (tracked_file->status == CREATED)
            {
                send_req_to_all_clients(CREATE, tracked_file);
                tracked_file->status = STABLE;
            }
            else if (tracked_file->status == UPDATED)
            {
                send_req_to_all_clients(UPDATE, tracked_file);
                tracked_file->status = STABLE;
            }
            else if (tracked_file->status == DELETED)
            {
                send_req_to_all_clients(DELETE, tracked_file);
                remove_tracked_file(tracking_system, tracked_file->path);
                i--;
            }
        }
        pthread_mutex_unlock(&comm_lock);
        usleep(50000);
    }

    return NULL;
}

void send_req_to_all_clients(request_status_t status, tracked_file_t *tracked_file)
{
    for (int i = 0; i < client_queue->running_count; i++)
    {
        client_info_t *client = client_queue->running_clients[i];
        if (status == CREATE)
        {
            send_create_or_update_req(*tracked_file, directory, client->socket, CREATE);
        }
        else if (status == UPDATE)
        {
            send_create_or_update_req(*tracked_file, directory, client->socket, UPDATE);
        }
        else if (status == DELETE)
        {
            send_delete_req(*tracked_file, directory, client->socket);
        }
    }
}

void *signal_handler_thread(void *arg)
{
    sigset_t *signal_set = (sigset_t *)arg;
    int signo;
    siginfo_t info;
    char *signal_str = NULL;
    if (sigwaitinfo(signal_set, &info) == -1)
    {
        perror("sigwaitinfo");
        return NULL;
    }
    signo = info.si_signo;

    if (signo == SIGUSR1)
    {
        return NULL;
    }
    else if (signo == SIGINT)
        signal_str = "SIGINT";
    else if (signo == SIGSTOP)
        signal_str = "SIGSTOP";
    else if (signo == SIGTSTP)
        signal_str = "SIGTSTP";
    else if (signo == SIGTERM)
        signal_str = "SIGTERM";
    else if (signo == SIGQUIT)
        signal_str = "SIGQUIT";

    if (signal_str != NULL)
    {
        printf("\n\nReceived %s signal. Closing the server and sending shut down request to the clients...\n\n", signal_str);
        for (int i = 0; i < client_queue->running_count; i++)
        {
            client_info_t *client = client_queue->running_clients[i];
            send_shut_down_req(client->socket);
        }
        queue_set_signal(client_queue, signal_str);
        shutdown(server_socket, SHUT_RDWR);
    }
    return NULL;
}

void clean_up()
{
    close(server_socket);
    wait_threads();
    destroy_tracking_system(tracking_system);
    client_queue_destroy(client_queue);
    free(worker_thread_argument);
    free(handler_threads);
    free(tracking_system);
    pthread_mutex_destroy(&comm_lock);
    pthread_join(monitor_thread, NULL);
    pthread_join(signal_thread, NULL);
}