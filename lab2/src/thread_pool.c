#include "thread_pool.h"
#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static DWORD WINAPI worker_thread(LPVOID arg);

ThreadPool *thread_pool_create(int thread_count)
{
    if (thread_count <= 0)
        return NULL;

    ThreadPool *pool = (ThreadPool *)malloc(sizeof(ThreadPool));
    if (!pool)
        return NULL;

    memset(pool, 0, sizeof(ThreadPool));
    pool->thread_count = thread_count;
    pool->shutdown = 0;

    pool->queue_mutex = CreateMutex(NULL, FALSE, NULL);
    pool->queue_not_empty = CreateSemaphore(NULL, 0, INT_MAX, NULL);
    pool->queue_empty = CreateSemaphore(NULL, 1, 1, NULL);

    if (!pool->queue_mutex || !pool->queue_not_empty || !pool->queue_empty)
    {
        thread_pool_destroy(pool);
        return NULL;
    }

    pool->threads = (HANDLE *)malloc(thread_count * sizeof(HANDLE));
    if (!pool->threads)
    {
        thread_pool_destroy(pool);
        return NULL;
    }

    for (int i = 0; i < thread_count; i++)
    {
        pool->threads[i] = CreateThread(NULL, 0, worker_thread, pool, 0, NULL);
        if (!pool->threads[i])
        {
            thread_pool_destroy(pool);
            return NULL;
        }
    }

    return pool;
}

int thread_pool_add_task(ThreadPool *pool, void (*function)(void *), void *arg)
{
    if (!pool || !function)
        return -1;

    ThreadPoolTask *task = (ThreadPoolTask *)malloc(sizeof(ThreadPoolTask));
    if (!task)
        return -1;

    task->function = function;
    task->arg = arg;
    task->next = NULL;

    WaitForSingleObject(pool->queue_mutex, INFINITE);

    if (pool->task_queue == NULL)
    {
        pool->task_queue = task;
        pool->task_queue_tail = task;
    }
    else
    {
        pool->task_queue_tail->next = task;
        pool->task_queue_tail = task;
    }

    ReleaseSemaphore(pool->queue_not_empty, 1, NULL);
    ReleaseMutex(pool->queue_mutex);

    return 0;
}

int thread_pool_destroy(ThreadPool *pool)
{
    if (!pool)
        return -1;

    pool->shutdown = 1;

    for (int i = 0; i < pool->thread_count; i++)
    {
        ReleaseSemaphore(pool->queue_not_empty, 1, NULL);
    }

    WaitForMultipleObjects(pool->thread_count, pool->threads, TRUE, INFINITE);

    for (int i = 0; i < pool->thread_count; i++)
    {
        CloseHandle(pool->threads[i]);
    }
    free(pool->threads);

    ThreadPoolTask *task = pool->task_queue;
    while (task)
    {
        ThreadPoolTask *next = task->next;
        free(task);
        task = next;
    }

    if (pool->queue_mutex)
        CloseHandle(pool->queue_mutex);
    if (pool->queue_not_empty)
        CloseHandle(pool->queue_not_empty);
    if (pool->queue_empty)
        CloseHandle(pool->queue_empty);

    free(pool);
    return 0;
}

static DWORD WINAPI worker_thread(LPVOID arg)
{
    ThreadPool *pool = (ThreadPool *)arg;

    while (1)
    {
        WaitForSingleObject(pool->queue_not_empty, INFINITE);

        if (pool->shutdown)
        {
            break;
        }

        WaitForSingleObject(pool->queue_mutex, INFINITE);

        ThreadPoolTask *task = pool->task_queue;
        if (task)
        {
            pool->task_queue = task->next;
            if (pool->task_queue == NULL)
            {
                pool->task_queue_tail = NULL;
            }
        }

        ReleaseMutex(pool->queue_mutex);

        if (task)
        {
            task->function(task->arg);
            free(task);
        }
    }

    return 0;
}

void handle_client(void *arg)
{
    ClientData *client_data = (ClientData *)arg;

    if (client_data)
    {
        http_handle_request(client_data->client_sock, client_data->root_dir);

        closesocket(client_data->client_sock);

        free(client_data);
    }
}