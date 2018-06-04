//
// Created by root on 5/23/18.
//

#include "epoll.h"
#include "dbg.h"

struct epoll_event *events;

//epoll_create1 flags参数只能为EPOLL_CLOEXEC, 为0时与epoll_create相同
int my_epoll_create(int flags) {
//    int fd = epoll_create(flags);
    int fd = epoll_create1(flags);
    check(fd > 0, "my_epoll_create: epoll_create1");

    events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * MAXEVENTS);
    check(events != NULL, "my_epoll_create: malloc");
    return fd;
}

void my_epoll_add(int epfd, int fd, struct epoll_event *event){
    int rc = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, event);
    check(rc == 0, "my_epoll_add: epoll_ctl");
    return;
}

void my_epoll_mod(int epfd, int fd, struct epoll_event *event){
    int rc = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, event);
    check(rc == 0, "my_epoll_mod: epoll_ctl");
    return;
}

void my_epoll_del(int epfd, int fd, struct epoll_event *event){
    int rc = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, event);
    check(rc == 0, "my_epoll_del: epoll_ctl");
    return;
}

int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout){
    int rc = epoll_wait(epfd, events, maxevents, timeout);
    check(rc >= 0, "my_epoll_wait: epoll_wait");
    return rc;
}
