#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "types.h"
#include "client_queue.h"
#include "protocol.h"
#include "tracking_system.h"
#include "helpers.h"
#include "controller.h"

extern pthread_mutex_t comm_lock;

void *client_handler(void *arg);
void send_initial_tracking_system(tracking_system_t *tracking_system, int client_socket);
void send_chunk_by_chunk(int client_socket, const void *data, size_t dataSize);
void handle_create_or_update(request_status_t status, req_t req, tracking_system_t *tracking_system,
                             int client_socket, client_queue_t *client_queue, client_info_t *curr_client_info);
void handle_delete(req_t req, tracking_system_t *tracking_system,
                   int client_socket, client_queue_t *client_queue, client_info_t *curr_client_info);

#endif