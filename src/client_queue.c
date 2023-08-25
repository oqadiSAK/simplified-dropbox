#include "../include/client_queue.h"

void client_queue_init(client_queue_t *queue, int capacity)
{
    int i;
    pthread_cond_init(&(queue->enqueue_cv), NULL);
    pthread_cond_init(&(queue->dequeue_cv), NULL);
    pthread_mutex_init(&(queue->lock), NULL);
    queue->data = (client_info_t **)malloc(sizeof(client_info_t *) * capacity);
    queue->running_clients = (client_info_t **)malloc(sizeof(client_info_t *) * capacity);
    for (i = 0; i < capacity; ++i)
    {
        queue->data[i] = NULL;
        queue->running_clients[i] = NULL;
    }
    queue->front = 0;
    queue->rear = 0;
    queue->running_count = 0;
    queue->capacity = capacity;
    queue->size = 0;
    queue->signal_received = 0;
    queue->signal_str = NULL;
}

void client_queue_destroy(client_queue_t *queue)
{
    pthread_cond_destroy(&(queue->enqueue_cv));
    pthread_cond_destroy(&(queue->dequeue_cv));
    pthread_mutex_destroy(&(queue->lock));
    if (queue->signal_received)
    {
        int i;
        for (i = 0; i < queue->capacity; i++)
        {
            if (queue->data[i] != NULL)
                free(queue->data[i]);
        }
    }

    for (int i = 0; i < queue->running_count; ++i)
    {
        free(queue->running_clients[i]);
    }

    free(queue->running_clients);
    free(queue->data);
    free(queue);
}

void client_queue_enqueue(client_queue_t *queue, client_info_t *item)
{
    pthread_mutex_lock(&(queue->lock));
    while ((queue->size == queue->capacity) && (!queue->signal_received))
        pthread_cond_wait(&(queue->enqueue_cv), &(queue->lock));

    if (!queue->signal_received)
    {
        queue->data[queue->rear] = item;
        queue->rear = (queue->rear + 1) % queue->capacity;
        queue->size += 1;
    }
    else
    {
        free(item);
    }
    pthread_cond_signal(&(queue->dequeue_cv));
    pthread_mutex_unlock(&(queue->lock));
}

client_info_t *client_queue_dequeue(client_queue_t *queue)
{
    pthread_mutex_lock(&(queue->lock));
    while (queue->size == 0 && !queue->signal_received)
        pthread_cond_wait(&(queue->dequeue_cv), &(queue->lock));

    if (queue->size == 0)
    {
        pthread_mutex_unlock(&(queue->lock));
        return NULL;
    }

    client_info_t *retval = queue->data[queue->front];
    queue->data[queue->front] = NULL;
    if (queue->signal_received)
    {
        pthread_cond_signal(&(queue->enqueue_cv));
        pthread_mutex_unlock(&(queue->lock));
        return NULL;
    }
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size -= 1;

    // Store the dequeued client in the running_client array
    queue->running_clients[queue->running_count] = retval;
    queue->running_count += 1;

    pthread_cond_signal(&(queue->enqueue_cv));
    pthread_mutex_unlock(&(queue->lock));
    return retval;
}

void remove_running_client(client_queue_t *queue, client_info_t *client_info)
{
    pthread_mutex_lock(&(queue->lock));
    // Find the index of the element you want to delete
    int index_to_delete = -1;
    for (int j = 0; j < queue->running_count; ++j)
    {
        if (client_info == queue->running_clients[j])
        {
            index_to_delete = j;
            break;
        }
    }
    if (index_to_delete == -1)
        return;
    // Shift the elements after the index to delete
    for (int i = index_to_delete; i < queue->running_count - 1; i++)
    {
        queue->running_clients[i] = queue->running_clients[i + 1];
    }
    // Decrement the count to reflect the removal
    queue->running_count -= 1;
    pthread_mutex_unlock(&(queue->lock));
}

void queue_set_signal(client_queue_t *queue, char *signal_str)
{
    pthread_mutex_lock(&(queue->lock));
    queue->signal_received = 1;
    queue->signal_str = signal_str;
    pthread_cond_broadcast(&(queue->enqueue_cv));
    pthread_cond_broadcast(&(queue->dequeue_cv));
    pthread_mutex_unlock(&(queue->lock));
}

int queue_check_signal(client_queue_t *queue)
{
    int retval;
    pthread_mutex_lock(&(queue->lock));
    retval = queue->signal_received;
    pthread_mutex_unlock(&(queue->lock));
    return retval;
}
