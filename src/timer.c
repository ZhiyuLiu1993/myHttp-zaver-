//
// Created by root on 5/26/18.
//

#include <sys/time.h>
#include "timer.h"

static int timer_comp(void *ti, void *tj) {
    my_timer_node *timeri = (my_timer_node *)ti;
    my_timer_node *timerj = (my_timer_node *)tj;

    return (timeri->key < timerj->key)? 1: 0;
}

my_pq_t my_timer;
size_t my_current_msec;

static void my_time_update() {
    // there is only one thread calling my_time_update, no need to lock?
    struct timeval tv;
    int rc;

    rc = gettimeofday(&tv, NULL);
    check(rc == 0, "my_time_update: gettimeofday error");

    my_current_msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    debug("in my_time_update, time = %zu", my_current_msec);
}


int my_timer_init() {
    int rc;
    rc = my_pq_init(&my_timer, timer_comp, MY_PQ_DEFAULT_SIZE);
    check(rc == MY_OK, "my_pq_init error");

    my_time_update();
    return MY_OK;
}

int my_find_timer() {
    my_timer_node *timer_node;
    int time = MY_TIMER_INFINITE;
    int rc;

    while (!my_pq_is_empty(&my_timer)) {
        debug("my_find_timer");
        my_time_update();
        timer_node = (my_timer_node *)my_pq_min(&my_timer);
        check(timer_node != NULL, "my_pq_min error");

        if (timer_node->deleted) {
            rc = my_pq_delmin(&my_timer);
            check(rc == 0, "my_pq_delmin");
            free(timer_node);
            continue;
        }

        time = (int) (timer_node->key - my_current_msec);
        debug("in my_find_timer, key = %zu, cur = %zu",
              timer_node->key,
              my_current_msec);
        time = (time > 0? time: 0);
        break;
    }

    return time;
}

void my_handle_expire_timers() {
    debug("in my_handle_expire_timers");
    my_timer_node *timer_node;
    int rc;

    while (!my_pq_is_empty(&my_timer)) {
        debug("my_handle_expire_timers, size = %zu", my_pq_size(&my_timer));
        my_time_update();
        timer_node = (my_timer_node *)my_pq_min(&my_timer);
        check(timer_node != NULL, "my_pq_min error");

        if (timer_node->deleted) {
            rc = my_pq_delmin(&my_timer);
            check(rc == 0, "my_handle_expire_timers: my_pq_delmin error");
            free(timer_node);
            continue;
        }

        if (timer_node->key > my_current_msec) {
            return;
        }

        if (timer_node->handler) {
            timer_node->handler(timer_node->rq);
        }
        rc = my_pq_delmin(&my_timer);
        check(rc == 0, "my_handle_expire_timers: my_pq_delmin error");
        free(timer_node);
    }
}

void my_add_timer(my_http_request_t *rq, size_t timeout, timer_handler_pt handler) {
    int rc;
    my_timer_node *timer_node = (my_timer_node *)malloc(sizeof(my_timer_node));
    check(timer_node != NULL, "my_add_timer: malloc error");

    my_time_update();
    rq->timer = timer_node;
    timer_node->key = my_current_msec + timeout;
    debug("in my_add_timer, key = %zu", timer_node->key);
    timer_node->deleted = 0;
    timer_node->handler = handler;
    timer_node->rq = rq;

    rc = my_pq_insert(&my_timer, timer_node);
    check(rc == 0, "my_add_timer: my_pq_insert error");
}

void my_del_timer(my_http_request_t *rq) {
    debug("in my_del_timer");
    my_time_update();
    my_timer_node *timer_node = rq->timer;
    check(timer_node != NULL, "my_del_timer: rq->timer is NULL");

    timer_node->deleted = 1;
}
