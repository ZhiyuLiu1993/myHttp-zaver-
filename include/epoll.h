//
// Created by root on 5/23/18.
//

#ifndef MYHTTP_EPOLL_H
#define MYHTTP_EPOLL_H

#include <sys/epoll.h>

#define MAXEVENTS 1024

int my_epoll_create(int flags);
void my_epoll_add(int epfd, int fd, struct epoll_event *event);
void my_epoll_mod(int epfd, int fd, struct epoll_event *event);
void my_epoll_del(int epfd, int fd, struct epoll_event *event);
int my_epoll_wait(int epfd, struct epoll_event *event, int maxevents, int timeout);

#endif //MYHTTP_EPOLL_H
