//
// Created by root on 5/25/18.
//

#ifndef MYHTTP_HTTP_PARSE_H
#define MYHTTP_HTTP_PARSE_H

#include "http_request.h"

#define CR '\r'
#define LF '\n'
#define CRLFCRLF "\r\n\r\n"

int my_http_parse_request_line(my_http_request_t *r);
int my_http_parse_request_body(my_http_request_t *r);

#endif //MYHTTP_HTTP_PARSE_H
