#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "types.h"

void client_queue_init(client_queue_t *queue, int capacity);
void client_queue_destroy(client_queue_t *queue);
void client_queue_enqueue(client_queue_t *queue, client_info_t *item);
client_info_t *client_queue_dequeue(client_queue_t *queue);
void remove_running_client(client_queue_t *queue, client_info_t *client_info);
void queue_set_signal(client_queue_t *queue, char *signal_str);
int queue_check_signal(client_queue_t *queue);

#endif
