#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "types.h"
#include "protocol.h"
#include "helpers.h"
#include "tracking_system.h"

int send_init_req(int socket, const char *dir_path);
int send_quit_req(int socket);
int send_shut_down_req(int socket);
int send_get_req(tracked_file_t file, const char *filepath, int socket);
int send_create_or_update_req(tracked_file_t new_file, char *client_dir_path, int socket, request_status_t status);
int send_delete_req(tracked_file_t file, char *client_dir_path, int socket);
void on_init_req(req_t req, client_info_t *client_info, int socket);
void on_get_req(req_t req, int client_socket);
void on_create_or_update_req(req_t req, int client_socket, char *dir_name, tracking_system_t *tracking_system, request_status_t status);
void on_delete_req(req_t req, char *dir_name, tracking_system_t *tracking_system);
void remove_directory(tracking_system_t *tracking_system, const char *dir_path);

#endif
