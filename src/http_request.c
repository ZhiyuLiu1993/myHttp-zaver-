//
// Created by root on 5/25/18.
//

#include <math.h>
//#include <time.h>
#include <unistd.h>
#include "http.h"
#include "http_request.h"
#include "error.h"

static int my_http_process_ignore(my_http_request_t *request, my_http_out_t *out, char *data, int len);
static int my_http_process_connection(my_http_request_t *request, my_http_out_t *out, char *data, int len);
static int my_http_process_if_modified_since(my_http_request_t *r, my_http_out_t *out, char *data, int len);

my_http_header_handle_t my_http_headers_in[] = {
        {"Host", my_http_process_ignore},
        {"Connection", my_http_process_connection},
        {"If-Modified-Since", my_http_process_if_modified_since},
        {"", my_http_process_ignore}
};

int my_init_request_t(my_http_request_t *request, int fd, int epfd, my_conf_t *cf) {
    request->fd = fd;
    request->epfd = epfd;
    request->pos = request->last = 0;
    request->state = 0;
    request->root = cf->root;
    INIT_LIST_HEAD(&(request->list));

    return MY_OK;
}

int my_free_request_t(my_http_request_t *request) {
    // TODO
    (void) request;
    return MY_OK;
}

int my_init_out_t(my_http_out_t *out, int fd) {
    out->fd = fd;
    out->keep_alive = 0;
    out->modified = 1;
    out->status = 0;

    return MY_OK;
}

int my_free_out_t(my_http_out_t *out) {
    // TODO
    (void) out;
    return MY_OK;
}

void my_http_handle_header(my_http_request_t *request, my_http_out_t *out) {
    list_head *pos;
    my_http_header_t *hd;
    my_http_header_handle_t *header_in;
    int len;

    list_for_each(pos, &(request->list)) {
        hd = list_entry(pos, my_http_header_t, list);
        /* handle */

        for (header_in = my_http_headers_in;
             strlen(header_in->name) > 0;
             header_in++) {
            //匹配请求name
            if (strncmp(hd->key_start, header_in->name, hd->key_end - hd->key_start) == 0) {
                //debug("key = %.*s, value = %.*s", hd->key_end-hd->key_start, hd->key_start, hd->value_end-hd->value_start, hd->value_start);
                len = (int)(hd->value_end - hd->value_start);
                (*(header_in->handler))(request, out, hd->value_start, len);
                break;
            }
        }
        /* delete it from the original list */
        list_del(pos);
        free(hd);
    }
}

int my_http_close_conn(my_http_request_t *request) {
    // NOTICE: closing a file descriptor will cause it to be removed from all epoll sets automatically
    // 但是如果这个描述符被dup或dup2的不会，可以用来过滤不同事件
    close(request->fd);
    free(request);

    return MY_OK;
}

static int my_http_process_ignore(my_http_request_t *request, my_http_out_t *out,
                                  char *data, int len) {
    (void) request;
    (void) out;
    (void) data;
    (void) len;

    return MY_OK;
}

static int my_http_process_connection(my_http_request_t *request, my_http_out_t *out,
                                      char *data, int len) {
    (void) request;
    if (strncasecmp("keep-alive", data, len) == 0) {
        out->keep_alive = 1;
    }

    return MY_OK;
}

static int my_http_process_if_modified_since(my_http_request_t *request, my_http_out_t *out,
                                             char *data, int len) {
    (void) request;
    (void) len;

    struct tm tm;
    //转换失败返回NULL，然后返回OK？
    if (strptime(data, "%a, %d %b %Y %H:%M:%S GMT", &tm) == (char *)NULL) {
        return MY_OK;
    }
    time_t client_time = mktime(&tm);

    double time_diff = difftime(out->mtime, client_time);
    if (fabs(time_diff) < 1e-6) {
        // log_info("content not modified clienttime = %d, mtime = %d\n", client_time, out->mtime);
        /* Not modified */
        out->modified = 0;
        out->status = MY_HTTP_NOT_MODIFIED;
    }

    return MY_OK;
}

const char *get_shortmsg_from_status_code(int status_code) {
    /*  for code to msg mapping, please check:
    * http://users.polytech.unice.fr/~buffa/cours/internet/POLYS/servlets/Servlet-Tutorial-Response-Status-Line.html
    */
    if (status_code == MY_HTTP_OK) {
        return "OK";
    }

    if (status_code == MY_HTTP_NOT_MODIFIED) {
        return "Not Modified";
    }

    if (status_code == MY_HTTP_NOT_FOUND) {
        return "Not Found";
    }


    return "Unknown";
}

