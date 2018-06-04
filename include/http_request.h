//
// Created by root on 5/24/18.
//

#ifndef MYHTTP_HTTP_REQUEST_H
#define MYHTTP_HTTP_REQUEST_H

#include <time.h>
#include "list.h"
#include "util.h"

#define MY_AGAIN EAGAIN

#define MY_HTTP_PARSE_INVALID_METHOD     10
#define MY_HTTP_PARSE_INVALID_REQUEST    11
#define MY_HTTP_PARSE_INVALID_HEADER     12

#define MY_HTTP_UNKNOWN                  0x0001
#define MY_HTTP_GET                      0x0002
#define MY_HTTP_HEAD                     0x0004
#define MY_HTTP_POST                     0x0008

#define MY_HTTP_OK                       200

#define MY_HTTP_NOT_MODIFIED             304

#define MY_HTTP_NOT_FOUND                404

#define MAX_BUF   8124

typedef struct my_http_request_s{
    void *root;
    int fd;
    int epfd;
    char buf[MAX_BUF];
    size_t pos;
    size_t last;
    int state;
    void *request_start;
    void *method_end;
    int method;
    void *uri_start;
    void *uri_end;
    void *path_start;
    void *path_end;
    void *query_start;
    void *query_end;
    int http_major;
    int http_minor;
    void *request_end;

    struct list_head list;
    void *cur_header_key_start;
    void *cur_header_key_end;
    void *cur_header_value_start;
    void *cur_header_value_end;

    void *timer;
} my_http_request_t;

typedef struct {
    int fd;
    int keep_alive;
    time_t mtime;  // modified time
    int modified;

    int status;
} my_http_out_t;

typedef struct my_http_header_s{
    void *key_start;
    void *key_end;
    void *value_start;
    void *value_end;
    list_head list;
} my_http_header_t;

typedef int (*my_http_header_handler_pt)(my_http_request_t *request, my_http_out_t *out,
                                         char *data, int len);

typedef struct {
    char *name;
    my_http_header_handler_pt handler;
} my_http_header_handle_t;

void my_http_handle_header(my_http_request_t *request, my_http_out_t *out);
int my_http_close_conn(my_http_request_t *request);

int my_init_request_t(my_http_request_t *request, int fd, int epfd, my_conf_t *cf);
int my_free_request_t(my_http_request_t *request);

int my_init_out_t(my_http_out_t *out, int fd);
int my_free_out_t(my_http_out_t *out);

const char *get_shortmsg_from_status_code(int status_code);

extern my_http_header_handle_t my_http_header_in[];

#endif //MYHTTP_HTTP_REQUEST_H
