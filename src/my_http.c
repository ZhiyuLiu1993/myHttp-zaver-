//
// Created by root on 5/26/18.
//
#include <stdint.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "util.h"
#include "timer.h"
#include "http.h"
#include "epoll.h"
#include "threadpool.h"

#define CONF "../myhttp.conf"
#define PROGRAM_VERSION "0.1"
//#define EPFDMAX   1024

extern struct epoll_event *events;

static const struct option long_options[]=
        {
                {"help",no_argument,NULL,'?'},
                {"version",no_argument,NULL,'V'},
                {"conf",required_argument,NULL,'c'},
                {NULL,0,NULL,0}
        };

static void usage() {
    fprintf(stderr,
            "myHttp [option]... \n"
            "  -c|--conf <config file>  Specify config file. Default ./myhttp.conf.\n"
            "  -?|-h|--help             This information.\n"
            "  -V|--version             Display program version.\n"
    );
}

int main(int argc, char* argv[]) {
    int rc;
    int opt = 0;
    int options_index = 0;
    char *conf_file = CONF;

    /*
    * parse argv
    * more detail visit: http://www.gnu.org/software/libc/manual/html_node/Getopt.html
    */

    if (argc == 1) {
        usage();
        return 0;
    }

    while ((opt=getopt_long(argc, argv,"Vc:h",long_options,&options_index)) != EOF) {
        switch (opt) {
            case  0 : break;
            case 'c':
                conf_file = optarg;
                break;
            case 'V':
                printf(PROGRAM_VERSION"\n");
                return 0;
            case ':':
            case 'h':
            default:
                usage();
                return 0;
        }
    }

    debug("conffile = %s", conf_file);

    if (optind < argc) {
        log_err("non-option ARGV-elements: ");
        while (optind < argc)
            log_err("%s ", argv[optind++]);
        return 0;
    }

    /*
    * read confile file
    */
    char conf_buf[BUFLEN];
    my_conf_t cf;
    rc = read_conf(conf_file, &cf, conf_buf, BUFLEN);
    check(rc == MY_CONF_OK, "read conf err");


    /*
    *   install signal handle for SIGPIPE
    *   when a fd is closed by remote, writing to this fd will cause system send
    *   SIGPIPE to this process, which exit the program
    */
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if (sigaction(SIGPIPE, &sa, NULL)) {
        log_err("install sigal handler for SIGPIPE failed");
        return 0;
    }

    /*
    * initialize listening socket
    */
    int listenfd;
    struct sockaddr_in clientaddr;
    // initialize clientaddr and inlen to solve "accept Invalid argument" bug
    socklen_t inlen = 1;
    memset(&clientaddr, 0, sizeof(struct sockaddr_in));

    //加入对listenfd的check，防止port不可用
    listenfd = open_listenfd(cf.port);
    check(listenfd > 0, "open listen");
    rc = make_socket_non_blocking(listenfd);
    check(rc == 0, "make_socket_non_blocking");

    /*
    * create epoll and add listenfd to ep
    */
    //此处不能设置EPFDMAX为0 这个参数在2.6.8内核被废弃，但依然大于0
    ///DONE:将epoll_create替换为epoll_create1  参数flags  除0外，只能为EPOLL_CLOEXEC
    int epfd = my_epoll_create(0);
//    int epfd = my_epoll_create(EPFDMAX);
    struct epoll_event event;

    my_http_request_t *request = (my_http_request_t *)malloc(sizeof(my_http_request_t));
    my_init_request_t(request, listenfd, epfd, &cf);

    event.data.ptr = (void *)request;
    event.events = EPOLLIN | EPOLLET;
    my_epoll_add(epfd, listenfd, &event);

    /*
    * create thread pool
    */
    /*
    my_threadpool_t *tp = threadpool_init(cf.thread_num);
    check(tp != NULL, "threadpool_init error");
    */

    /*
     * initialize timer
     */
    my_timer_init();

    log_info("my_http started.");
    int n;
    int i, fd;
    int time;

    int exit_status = 0;   //这个变量用于设置退出，暂时没有状态的改变

    /* epoll_wait loop */
    while (!exit_status) {
        //找到最小超时时间,由优先级队列实现
        time = my_find_timer();
        debug("wait time = %d", time);
        n = my_epoll_wait(epfd, events, MAXEVENTS, time);
        //处理已超时事件
        my_handle_expire_timers();

        for (i = 0; i < n; i++) {
            my_http_request_t *r = (my_http_request_t *)events[i].data.ptr;
            fd = r->fd;

            if (listenfd == fd) {
                /* we hava one or more incoming connections */

                int infd;
                while(1) {
                    infd = accept(listenfd, (struct sockaddr *)&clientaddr, &inlen);
                    if (infd < 0) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            /* we have processed all incoming connections */
                            break;
                        } else {
                            log_err("accept");
                            break;
                        }
                    }

                    rc = make_socket_non_blocking(infd);
                    check(rc == 0, "make_socket_non_blocking");
                    log_info("new connection fd %d", infd);

                    my_http_request_t *local_r = (my_http_request_t *)malloc(sizeof(my_http_request_t));
                    if (local_r == NULL) {
                        log_err("malloc(sizeof(my_http_request_t))");
                        break;
                    }

                    my_init_request_t(local_r , infd, epfd, &cf);
                    event.data.ptr = (void *)local_r;
                    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

                    my_epoll_add(epfd, infd, &event);
                    //添加默认超时时间及超时事件，关闭连接
                    my_add_timer(local_r, TIMEOUT_DEFAULT, my_http_close_conn);
                }   // end of while of accept

            } else {
                if ((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN))) {
                    ///FIXME:此处错误是否不应该退出
                    log_err("epoll error fd: %d", r->fd);
                    close(fd);
                    continue;
                }

                log_info("new data from fd %d", fd);
                //rc = threadpool_add(tp, do_request, events[i].data.ptr);
                //check(rc == 0, "threadpool_add");

                do_request(events[i].data.ptr);
            }
        }   //end of for
    }   // end of while(1)


    /*
    if (threadpool_destroy(tp, 1) < 0) {
        log_err("destroy threadpool failed");
    }
    */

    return 0;
}
