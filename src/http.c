//
// Created by root on 5/26/18.
//
#include <strings.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "http.h"
#include "http_parse.h"
#include "http_request.h"
#include "epoll.h"
#include "error.h"
#include "timer.h"

static const char* get_file_type(const char *type);
static void parse_uri(char *uri, int length, char *filename, char *querystring);
static void do_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
static void serve_static(int fd, char *filename, size_t filesize, my_http_out_t *out);
static char *ROOT = NULL;

mime_type_t myhttp_mime[] =
        {
                {".html", "text/html"},
                {".xml", "text/xml"},
                {".xhtml", "application/xhtml+xml"},
                {".txt", "text/plain"},
                {".rtf", "application/rtf"},
                {".pdf", "application/pdf"},
                {".word", "application/msword"},
                {".png", "image/png"},
                {".gif", "image/gif"},
                {".jpg", "image/jpeg"},
                {".jpeg", "image/jpeg"},
                {".au", "audio/basic"},
                {".mpeg", "video/mpeg"},
                {".mpg", "video/mpeg"},
                {".avi", "video/x-msvideo"},
                {".gz", "application/x-gzip"},
                {".tar", "application/x-tar"},
                {".css", "text/css"},
                {NULL ,"text/plain"}
        };

//解析请求
void do_request(void *ptr) {
    my_http_request_t *request = (my_http_request_t *)ptr;
    int fd = request->fd;
    int rc, n;
    char filename[SHORTLINE];
    struct stat sbuf;
    ROOT = request->root;
    char *plast = NULL;
    size_t remain_size;

    my_del_timer(request);
    for(;;) {

        plast = &request->buf[request->last % MAX_BUF];
        //什么情况会导致第一个比第二个大？
        ///不应该存在第一种情况，对缓冲区的访问是连续的
        remain_size = MIN(MAX_BUF - (request->last - request->pos) - 1,
                MAX_BUF - request->last % MAX_BUF);

        n = read(fd, plast, remain_size);
        check(request->last - request->pos < MAX_BUF, "request buffer overflow!");

        if (n == 0) {
            // EOF
            log_info("read return 0, ready to close fd %d, remain_size = %zu", fd, remain_size);
            goto err;
        }

        if (n < 0) {
            if (errno != EAGAIN) {
                log_err("read err, and errno = %d", errno);
                goto err;
            }
            break;
        }

        request->last += n;
        check(request->last - request->pos < MAX_BUF, "request buffer overflow!");

        log_info("ready to parse request line");
        //解析请求行
        rc = my_http_parse_request_line(request);
        if (rc == MY_AGAIN) {
            continue;
        } else if (rc != MY_OK) {
            log_err("rc != MY_OK");
            goto err;
        }

        //%.*s *用来指定宽度，对应一个整数，.与后面的数合起来置顶必须输出小于等于这个宽度
        log_info("method == %.*s", (int)(request->method_end - request->request_start),
                 (char *)request->request_start);
        log_info("uri == %.*s", (int)(request->uri_end - request->uri_start), (char *)request->uri_start);

        debug("ready to parse request body");
        //解析请求体
        rc = my_http_parse_request_body(request);
        if (rc == MY_AGAIN) {
            continue;
        } else if (rc != MY_OK) {
            log_err("rc != MY_OK");
            goto err;
        }

        /*
        *   handle http header
        */
        my_http_out_t *out = (my_http_out_t *)malloc(sizeof(my_http_out_t));
        if (out == NULL) {
            log_err("no enough space for my_http_out_t");
            exit(1);
        }

        rc = my_init_out_t(out, fd);
        check(rc == MY_OK, "my_init_out_t");

        //解析URI
        parse_uri(request->uri_start, request->uri_end - request->uri_start, filename, NULL);

        //struct stat sbuf
        if(stat(filename, &sbuf) < 0) {
            do_error(fd, filename, "404", "Not Found", " can't find the file");
            continue;
        }


        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        {
            do_error(fd, filename, "403", "Forbidden",
                     " can't read the file");
            continue;
        }

        out->mtime = sbuf.st_mtime;

        my_http_handle_header(request, out);
        check(list_empty(&(request->list)) == 1, "header list should be empty");

        if (out->status == 0) {
            out->status = MY_HTTP_OK;
        }

        //发送静态文件
        serve_static(fd, filename, sbuf.st_size, out);

        if (!out->keep_alive) {
            log_info("no keep_alive! ready to close");
            free(out);
            goto close;
        }
        free(out);

    }

    struct epoll_event event;
    event.data.ptr = ptr;
    //oneshot 只触发其上注册的一个事件，且只触发一次，下次再触发需要rpoll_ctl重置
    //主要用于线程池处理,epoll  ET模式同一事件也可能被触发多次，比如缓冲区很小，发送数据很大
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    my_epoll_mod(request->epfd, request->fd, &event);
    my_add_timer(request, TIMEOUT_DEFAULT, my_http_close_conn);
    return;

    err:
    close:
    rc = my_http_close_conn(request);
    check(rc == 0, "do_request: my_http_close_conn");
}

static void parse_uri(char *uri, int uri_length, char *filename, char *querystring) {
    check(uri != NULL, "parse_uri: uri is NULL");
    uri[uri_length] = '\0';

    //查找首次出现 ？ 的位置
    char *question_mark = strchr(uri, '?');
    int file_length;
    if (question_mark) {
        file_length = (int)(question_mark - uri);
        debug("file_length = (question_mark - uri) = %d", file_length);
    } else {
        file_length = uri_length;
        debug("file_length = uri_length = %d", file_length);
    }

    if (querystring) {
        //TODO
    }

    //复制
    strcpy(filename, ROOT);

    // uri_length can not be too long
    if (uri_length > (SHORTLINE >> 1)) {
        log_err("uri too long: %.*s", uri_length, uri);
        return;
    }

    debug("before strncat, filename = %s, uri = %.*s, file_len = %d", filename, file_length, uri, file_length);
    //连接
    strncat(filename, uri, file_length);

    //查找最后一次出现字符位置
    //filename最后一个/之后没有.，且最后一个字符不是 /  在之后加 /
    char *last_comp = strrchr(filename, '/');
    char *last_dot = strrchr(last_comp, '.');
    if (last_dot == NULL && filename[strlen(filename)-1] != '/') {
        strcat(filename, "/");
    }

    //添加默认的页面
    if(filename[strlen(filename)-1] == '/') {
        strcat(filename, "index.html");
    }

    log_info("filename = %s", filename);
    return;
}

static void do_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char header[MAXLINE], body[MAXLINE];

    sprintf(body, "<html><title>Zaver Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\n", body);
    sprintf(body, "%s%s: %s\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\n</p>", body, longmsg, cause);
    sprintf(body, "%s<hr><em>Zaver web server</em>\n</body></html>", body);

    sprintf(header, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    sprintf(header, "%sServer: Zaver\r\n", header);
    sprintf(header, "%sContent-type: text/html\r\n", header);
    sprintf(header, "%sConnection: close\r\n", header);
    sprintf(header, "%sContent-length: %d\r\n\r\n", header, (int)strlen(body));
    //log_info("header  = \n %s\n", header);
    rio_writen(fd, header, strlen(header));
    rio_writen(fd, body, strlen(body));
    //log_info("leave clienterror\n");
    return;
}

static void serve_static(int fd, char *filename, size_t filesize, my_http_out_t *out) {
    char header[MAXLINE];
    char buf[SHORTLINE];
    size_t n;
    struct tm tm;

    const char *file_type;
    const char *dot_pos = strrchr(filename, '.');
    file_type = get_file_type(dot_pos);

    sprintf(header, "HTTP/1.1 %d %s\r\n", out->status, get_shortmsg_from_status_code(out->status));

    if (out->keep_alive) {
        //将各个部分连接起来 通过header  %s   header的方式
        sprintf(header, "%sConnection: keep-alive\r\n", header);
        sprintf(header, "%sKeep-Alive: timeout=%d\r\n", header, TIMEOUT_DEFAULT);
    }

    if (out->modified) {
        sprintf(header, "%sContent-type: %s\r\n", header, file_type);
        sprintf(header, "%sContent-length: %zu\r\n", header, filesize);
        localtime_r(&(out->mtime), &tm);
        strftime(buf, SHORTLINE,  "%a, %d %b %Y %H:%M:%S GMT", &tm);
        sprintf(header, "%sLast-Modified: %s\r\n", header, buf);
    }

    sprintf(header, "%sServer: Zaver\r\n", header);
    sprintf(header, "%s\r\n", header);

    n = (size_t)rio_writen(fd, header, strlen(header));
    check(n == strlen(header), "rio_writen error, errno = %d", errno);
    if (n != strlen(header)) {
        log_err("n != strlen(header)");
        goto out;
    }

    if (!out->modified) {
        goto out;
    }

    int srcfd = open(filename, O_RDONLY, 0);
    check(srcfd > 2, "open error");
    // can use sendfile
    //通过mmap将文件映射到内存
    char *srcaddr = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    check(srcaddr != (void *) -1, "mmap error");
    close(srcfd);

    //利用CSAPP封装一层写入filesize长度
    n = rio_writen(fd, srcaddr, filesize);
    check(n == filesize, "rio_writen error");

    munmap(srcaddr, filesize);

    out:
    return;
}

static const char* get_file_type(const char *type)
{
    if (type == NULL) {
        return "text/plain";
    }

    int i;
    for (i = 0; myhttp_mime[i].type != NULL; ++i) {
        if (strcmp(type, myhttp_mime[i].type) == 0)
            return myhttp_mime[i].value;
    }
    return myhttp_mime[i].value;
}
