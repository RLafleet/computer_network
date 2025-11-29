#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <winsock2.h>
#include <process.h>

typedef struct ThreadPoolTask
{
    void (*function)(void *arg);
    void *arg;
    struct ThreadPoolTask *next;
} ThreadPoolTask;

typedef struct ThreadPool
{
    int thread_count;
    HANDLE *threads;
    ThreadPoolTask *task_queue;
    ThreadPoolTask *task_queue_tail;
    HANDLE queue_mutex;
    HANDLE queue_not_empty;
    HANDLE queue_empty;
    int shutdown;
} ThreadPool;

typedef struct ClientData
{
    SOCKET client_sock;
    char root_dir[256];
} ClientData;

ThreadPool *thread_pool_create(int thread_count);
int thread_pool_add_task(ThreadPool *pool, void (*function)(void *), void *arg);
int thread_pool_destroy(ThreadPool *pool);
void handle_client(void *arg);

#endif // THREAD_POOL_H