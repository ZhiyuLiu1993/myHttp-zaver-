//
// Created by root on 5/21/18.
//

#ifndef MYHTTP_THREADPOOL_H
#define MYHTTP_THREADPOOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include "dbg.h"

#define  THREAD_NUM 8

typedef struct my_task_s{
    void (*func)(void *);
    void *arg;
    struct my_task_s *next;
} my_task_t;

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    pthread_t *threads;
    my_task_t *head;
    int thread_count;
    int queue_size;
    int shutdown;
    int started;
} my_threadpool_t;

typedef enum{
    my_tp_invalid = -1,
    my_tp_lock_fail = -2,
    my_tp_already_shutdown = -3,
    my_tp_cond_broadcast = -4,
    my_tp_thread_fail = -5
} my_threadpoll_error_t;

my_threadpool_t *threadpool_init(int thread_num);

int threadpool_add(my_threadpool_t *ppol, void (*func)(void*), void *arg);

int threadpool_destroy(my_threadpool_t *pool, int graceful);

#ifdef __cplusplus
};
#endif

#endif //MYHTTP_THREADPOOL_H
