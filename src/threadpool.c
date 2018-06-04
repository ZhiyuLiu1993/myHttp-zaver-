//
// Created by root on 5/21/18.
//

#include <threadpool.h>
#include "threadpool.h"

typedef enum {
    immediate_shutdown = 1,
    graceful_shutdown = 2
} my_threadpool_sd_t;

static int threadpool_free(my_threadpool_t *pool);
static void *threadpool_worker(void *arg);

my_threadpool_t *threadpool_init(int thread_num){
    if(thread_num <= 0){
        log_err("the arg of threadpool_init must greater than 0");
        return NULL;
    }

    my_threadpool_t *pool;
    ///这个pool在free中没有释放，需要看在哪进行free
    if((pool = (my_threadpool_t *)malloc(sizeof(my_threadpool_t))) == NULL){
        goto err;
    }

    pool->thread_count = 0;
    pool->queue_size = 0;
    pool->shutdown = 0;
    pool->started = 0;
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_num);
    pool->head = (my_task_t *)malloc(sizeof(my_task_t));

    if((pool->threads == NULL) || (pool->head == NULL)){
        goto err;
    }

    pool->head->func = NULL;
    pool->head->arg = NULL;
    pool->head->next = NULL;

    if(pthread_mutex_init(&(pool->lock), NULL) != 0){
        goto err;
    }

    if(pthread_cond_init(&(pool->cond), NULL) != 0){
        pthread_mutex_destroy(&(pool->lock));
        goto err;
    }

    int i;
    for( i = 0; i < thread_num; ++i){
        if(pthread_create(&(pool->threads[i]), NULL,
        threadpool_worker, (void *)pool) != 0){
            threadpool_destroy(pool, 0);
            return NULL;
        }
        log_info("thread: %08x started", (uint32_t)pool->threads[i]);

        ++pool->thread_count;
        ++pool->started;
    }

    return pool;

err:
    if(pool) {
        threadpool_free(pool);
    }

    return NULL;
}

int threadpool_add(my_threadpool_t *pool, void(*func)(void *), void *arg){
    int rc;
    int err = 0;
    if(pool == NULL || func == NULL){
        log_err("pool == NULL or func == NULL");
        return -1;
    }

    if(pthread_mutex_lock(&(pool->lock)) != 0){
        log_err("pthread_mutex_lock");
        return -1;
    }

    if(pool->shutdown){
        err = my_tp_already_shutdown;
        goto out;
    }

    my_task_t *task = (my_task_t *)malloc(sizeof(my_task_t));
    if(task == NULL){
        log_err("malloc task fail");
        goto out;
    }

    task->func = func;
    task->arg = arg;
    task->next = pool->head->next;
    pool->head->next = task;

    ++pool->queue_size;

    rc = pthread_cond_signal(&(pool->cond));
    check(rc == 0, "pthread_cond_sidnal");

out:
    if(pthread_mutex_unlock(&(pool->lock)) != 0){
        log_err("pthread_mutex_unlock");
        return -1;
    }

    return err;
}

int threadpool_free(my_threadpool_t *pool){
    if(pool == NULL || pool->started > 0){
        return -1;
    }

    if(pool->threads) {
        free(pool->threads);
        pool->threads = NULL;
    }

    my_task_t *old;
    while(pool->head->next){
        old = pool->head->next;
        pool->head->next = pool->head->next->next;
        free(old);
        old = NULL;
    }
    //内存释放后置为NULL，有部分内存没有释放
    free(pool->head);
    pool->head = NULL;
    free(pool);
    pool = NULL;

    return 0;
}

int threadpool_destroy(my_threadpool_t *pool, int graceful){
    int err = 0;

    if(pool == NULL){
        log_err("pool == NULL");
        return my_tp_invalid;
    }

    if(pthread_mutex_lock(&(pool->lock)) != 0){
        return my_tp_lock_fail;
    }

    do {
        if(pool->shutdown){
            err = my_tp_already_shutdown;
            break;
        }

        pool->shutdown = (graceful) ? graceful_shutdown : immediate_shutdown;

        if(pthread_cond_broadcast(&(pool->cond)) != 0){
            err = my_tp_cond_broadcast;
            break;
        }

        if(pthread_mutex_unlock(&(pool->lock)) != 0){
            err = my_tp_lock_fail;
            break;
        }

        int i;
        for (i = 0; i < pool->thread_count; ++i) {
            if(pthread_join(pool->threads[i], NULL) != 0){
                err = my_tp_thread_fail;
            }
            log_info("thread %08x exit", (uint32_t)pool->threads[i]);
        }

    } while(0);

    if(!err){
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->cond));
        threadpool_free(pool);
    }

    return err;
}

static void *threadpool_worker(void *arg){
    if(arg == NULL){
        log_err("arg should be type my_threadpool_t*");
        return NULL;
    }

    my_threadpool_t *pool = (my_threadpool_t *)arg;
    my_task_t *task;

    while(1){
        pthread_mutex_lock(&(pool->lock));

        //TODO:是否会唤醒多个线程，产生惊群效应
        while((pool->queue_size == 0) && !(pool->shutdown)){
            pthread_cond_wait(&(pool->cond), &(pool->lock));
        }

        if(pool->shutdown == immediate_shutdown){
            break;
        } else if((pool->shutdown == graceful_shutdown) && pool->queue_size == 0){
            break;
        }

        task = pool->head->next;
        if(task == NULL){
            pthread_mutex_unlock(&(pool->lock));
            continue;
        }

        pool->head->next = task->next;
        --pool->queue_size;

        pthread_mutex_unlock(&(pool->lock));

        (*(task->func))(task->arg);

        free(task);
        task = NULL;
    }

    --pool->started;
    pthread_mutex_unlock((&(pool->lock)));
    pthread_exit(NULL);

    return NULL;
}
