#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <libgen.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "include/types.h"
#include "include/protocol.h"
#include "include/tracking_system.h"
#include "include/helpers.h"
#include "include/controller.h"

void check_usage(int argc, char *argv[]);
void create_log_file();
void init();
int create_monitor_thread();
int create_sighandler_thread();
void listen_server();
void set_socket();
tracking_system_t get_server_tracking_system();
void init_sync();
void sync_difference();
void *dir_monitor(void *arg);
void *signal_handler_thread(void *arg);
void my_log(const char *format, ...);
void clean_up();

char *dir_name;
int port_number, client_socket, log_fd;
char *server_address, *log_file_path;
tracking_system_t client_tracking_system, server_tracking_system;
pthread_t monitor_thread, signal_thread;
sigset_t signal_set;
pthread_mutex_t comm_lock;

int main(int argc, char *argv[])
{
    check_usage(argc, argv);
    create_sighandler_thread();
    create_log_file();
    init();
    create_monitor_thread();
    listen_server();
    clean_up();
    return 0;
}

void check_usage(int argc, char *argv[])
{
    // Check the number of arguments
    if (argc < 3 || argc > 4)
    {
        fprintf(stderr, "Usage: %s [dirName] [port_number] [server_address (optional)]\n", argv[0]);
        exit(1);
    }

    // Assign command-line arguments to global variables
    dir_name = argv[1];
    port_number = atoi(argv[2]);

    // Check for errors in command-line arguments
    if (port_number <= 0)
    {
        fprintf(stderr, "Error: Invalid port number\n");
        exit(1);
    }

    if (argc == 4)
    {
        // Connect over the network
        server_address = argv[3];
    }
    else
    {
        // Connect locally
        server_address = "127.0.0.1";
    }

    check_directory(dir_name);
}

void create_log_file()
{
    // Get the current time
    time_t current_time;
    time(&current_time);
    struct tm *time_info = localtime(&current_time);

    // Extract the last directory name from the full directory path
    char *base_name = basename(strdup(dir_name));

    // Create the log file path
    char log_file_name[FILENAME_MAX];
    snprintf(log_file_name, sizeof(log_file_name), "log_%s.txt", base_name);
    log_file_path = malloc(sizeof(char) * (MAX_PATH_LEN + FILENAME_MAX));
    snprintf(log_file_path, (MAX_PATH_LEN + FILENAME_MAX), "%s/%s", dir_name, log_file_name);

    // Remove the newline character from the time string
    char *time_str = asctime(time_info);
    time_str[strcspn(time_str, "\n")] = '\0';

    // Check if the log file exists
    log_fd = open(log_file_path, O_WRONLY);
    if (log_fd == -1)
    {
        // Log file doesn't exist, create it and write "Connected"
        log_fd = open(log_file_path, O_WRONLY | O_CREAT, 0777);
        if (log_fd == -1)
        {
            perror("Error opening log file");
            return;
        }

        write(log_fd, "Connected on ", strlen("Connected on "));
        write(log_fd, time_str, strlen(time_str));
        write(log_fd, "\n", 1);
    }
    else
    {
        // Log file exists, append "Reconnected"
        close(log_fd);
        log_fd = open(log_file_path, O_WRONLY | O_APPEND);
        if (log_fd == -1)
        {
            perror("Error opening log file");
            return;
        }

        write(log_fd, "Reconnected on ", strlen("Reconnected on "));
        write(log_fd, time_str, strlen(time_str));
        write(log_fd, "\n", 1);
    }
}

void init()
{
    int connection_value = 0;
    pthread_mutex_init(&comm_lock, NULL);
    init_tracking_system(&client_tracking_system, dir_name, log_file_path);
    set_socket();
    ssize_t received = recv(client_socket, &connection_value, sizeof(int), 0);
    if (received == -1)
    {
        perror("recv");
        pthread_mutex_unlock(&comm_lock);
        exit(1);
    }
    if (connection_value != 1)
    {
        my_log("Que full... Waiting...\n");
    }
    if (send_init_req(client_socket, dir_name) == -1)
    {
        pthread_mutex_unlock(&comm_lock);
        exit(1);
    }

    my_log("Connection established. Send initalize sync request to the server...\n");
    server_tracking_system = get_server_tracking_system();
    init_sync();
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
    if (pthread_create(&monitor_thread, NULL, dir_monitor, (void *)dir_name) != 0)
    {
        fprintf(stderr, "Error creating thread\n");
        return -1;
    }
    return 0;
}

void listen_server()
{
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
        // Process the received request
        switch (req.status)
        {
        case QUIT:
        {
            my_log("Received quit request from server...Bye\n");
            pthread_mutex_unlock(&comm_lock);
            return;
        }
        case SHUT_DOWN:
        {
            my_log("Received shutdown request from server...Bye\n");
            send_shut_down_req(client_socket);
            tracking_system_set_shutdown(&client_tracking_system);
            pthread_kill(signal_thread, SIGUSR1);
            pthread_mutex_unlock(&comm_lock);
            return;
        }
        case UPDATE:
        {
            my_log("Received update request from server for: %s\n", req.payload.create_or_update_req.tracked_file.path);
            on_create_or_update_req(req, client_socket, client_tracking_system.dir_path, &client_tracking_system, UPDATE);
            break;
        }
        case DELETE:
        {
            my_log("Received delete request from server for: %s\n", req.payload.delete_req.tracked_file.path);
            on_delete_req(req, client_tracking_system.dir_path, &client_tracking_system);
            break;
        }
        case CREATE:
        {
            my_log("Received create request from server for: %s\n", req.payload.create_or_update_req.tracked_file.path);
            on_create_or_update_req(req, client_socket, client_tracking_system.dir_path, &client_tracking_system, CREATE);
            break;
        }
        default:
            break;
        }
        pthread_mutex_unlock(&comm_lock);
    }
}

void set_socket()
{
    // Set up the client socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        perror("Error opening socket");
        exit(1);
    }

    // Set up the server address
    struct sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port_number);
    if (inet_pton(AF_INET, server_address, &(sock_addr.sin_addr)) <= 0)
    {
        perror("Invalid server address");
        exit(1);
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0)
    {
        perror("Error connecting to server");
        destroy_tracking_system(&client_tracking_system);
        pthread_kill(signal_thread, SIGUSR1);
        pthread_join(signal_thread, NULL);
        exit(1);
    }
}

tracking_system_t get_server_tracking_system()
{
    // Receive the tracking_system struct
    tracking_system_t tracking_system;
    size_t bytesReceived = 0;
    my_log("Getting metadata of server files...\n");
    while (bytesReceived < sizeof(tracking_system_t))
    {
        // Calculate the number of bytes remaining to receive
        size_t remainingBytes = sizeof(tracking_system_t) - bytesReceived;
        size_t chunkSize = (remainingBytes < CHUNK_SIZE) ? remainingBytes : CHUNK_SIZE;

        // Receive the chunk of data
        ssize_t received = recv(client_socket, ((char *)&tracking_system) + bytesReceived, chunkSize, 0);
        if (received == -1)
        {
            perror("recv");
            exit(1);
        }

        // Update the number of bytes received
        bytesReceived += received;
    }

    // Allocate memory for tracked_files
    tracking_system.tracked_files = malloc(sizeof(tracked_file_t) * tracking_system.num_tracked_files);
    if (tracking_system.tracked_files == NULL)
    {
        perror("Memory allocation failed");
        exit(1);
    }

    // Receive tracked_files data
    bytesReceived = 0;
    while (bytesReceived < sizeof(tracked_file_t) * tracking_system.num_tracked_files)
    {
        // Calculate the number of bytes remaining to receive
        size_t remainingBytes = sizeof(tracked_file_t) * tracking_system.num_tracked_files - bytesReceived;
        size_t chunkSize = (remainingBytes < CHUNK_SIZE) ? remainingBytes : CHUNK_SIZE;

        // Receive the chunk of data
        ssize_t received = recv(client_socket, ((char *)tracking_system.tracked_files) + bytesReceived, chunkSize, 0);
        if (received == -1)
        {
            perror("recv");
            exit(1);
        }

        // Update the number of bytes received
        bytesReceived += received;
    }

    return tracking_system;
}

void init_sync()
{
    my_log("Getting content of server files...\n");
    for (int i = 0; i < server_tracking_system.num_tracked_files; i++)
    {
        tracked_file_t file = server_tracking_system.tracked_files[i];
        fflush(stdout);
        char filepath[MAX_PATH_LEN];
        construct_file_path(file.path, server_tracking_system.dir_path, filepath, dir_name);
        // Create the file or directory if it does not exist
        if (file.is_dir == 1)
        {
            struct stat st = {0};
            if (stat(filepath, &st) == -1)
            {
                if (mkdir(filepath, 0777) == -1)
                {
                    perror("mkdir");
                    continue;
                }
            }
        }
        else
        {
            int file_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if (file_fd == -1)
            {
                perror("open");
                continue;
            }
            close(file_fd);
            my_log("Send get request to server for: %s\n", filepath);
            send_get_req(file, filepath, client_socket);
        }
    }

    my_log("Sync from server to client is finished\n");
    my_log("Starting sync from client to server...\n");
    sync_difference();
    my_log("Sync from client to server is finished\n\n");
}

void sync_difference()
{
    int i;
    for (i = 0; i < client_tracking_system.num_tracked_files; i++)
    {
        tracked_file_t new_file = client_tracking_system.tracked_files[i];
        char file_path[MAX_PATH_LEN];
        construct_file_path(new_file.path, client_tracking_system.dir_path, file_path, server_tracking_system.dir_path);

        tracked_file_t *tracked_file = find_tracked_file(&server_tracking_system, file_path);
        if (tracked_file == NULL)
        {
            my_log("Send create request to server for: %s\n", new_file.path);
            send_create_or_update_req(new_file, client_tracking_system.dir_path, client_socket, CREATE);
        }
    }
}

void *dir_monitor(void *arg)
{
    const char *dir_path = (const char *)arg;
    destroy_tracking_system(&client_tracking_system);
    init_tracking_system(&client_tracking_system, dir_path, log_file_path);
    while (1)
    {
        pthread_mutex_lock(&comm_lock);
        if (tracking_system_check_signal(&client_tracking_system, 0) == 1)
        {
            send_quit_req(client_socket);
            pthread_mutex_unlock(&comm_lock);
            break;
        }
        else if (tracking_system_check_shutdown(&client_tracking_system, 0) == 1)
        {
            pthread_mutex_unlock(&comm_lock);
            break;
        }
        check_statuses(&client_tracking_system);
        check_deletion(&client_tracking_system);

        int i;
        tracked_file_t *tracked_file = NULL;
        for (i = 0; i < client_tracking_system.num_tracked_files; ++i)
        {
            tracked_file = &client_tracking_system.tracked_files[i];
            if (tracked_file == NULL)
            {
                continue;
            }
            else if (tracked_file->status == CREATED)
            {
                my_log("File creation detected. Sending create request to the server for : %s\n", tracked_file->path);
                send_create_or_update_req(*tracked_file, dir_name, client_socket, CREATE);
                tracked_file->status = STABLE;
            }
            else if (tracked_file->status == UPDATED)
            {
                my_log("File modification detected. Sending update request to the server for : %s\n", tracked_file->path);
                send_create_or_update_req(*tracked_file, dir_name, client_socket, UPDATE);
                tracked_file->status = STABLE;
            }
            else if (tracked_file->status == DELETED)
            {
                my_log("File deletion detected. Sending delete request to the server for : %s\n", tracked_file->path);
                send_delete_req(*tracked_file, dir_name, client_socket);
                remove_tracked_file(&client_tracking_system, tracked_file->path);
                i--; // NO NEED I GUESS
            }
        }
        pthread_mutex_unlock(&comm_lock);
        usleep(50000);
    }

    return NULL;
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
        my_log("\n\nReceived %s signal. Closing the program...\n\n", signal_str);
        tracking_system_set_signal(&client_tracking_system, signal_str);
    }
    return NULL;
}

void my_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    size_t len = strlen(message);
    write(log_fd, message, len);
    write(STDOUT_FILENO, message, len);
}

void clean_up()
{
    free(server_tracking_system.tracked_files);
    close(client_socket);
    close(log_fd);
    pthread_join(monitor_thread, NULL);
    pthread_join(signal_thread, NULL);
    pthread_mutex_destroy(&comm_lock);
    destroy_tracking_system(&client_tracking_system);
}