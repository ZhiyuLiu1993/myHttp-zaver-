#ifndef UTIL_H
#define UTIL_H

#define LISTENQ 1024    //监听队列的最大数量

#define BUFLEN  8192 

#define DELIM   "="

#define MY_CONF_OK     0
#define MY_CONF_ERROR  101

#define MIN(a,b) ((a) < (b) ? (a) : (b))

typedef struct ht_conf_s {
	void *root;
	int port;
	int thread_num;
}my_conf_t;

int open_listenfd(int port);
int make_socket_non_blocking(int fd);

int read_conf(char *filename, my_conf_t *cfg, char *buf, int len);

#endif
