#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "util.h"
#include "dbg.h"

int open_listenfd(int port){
	if(port <= 0){
		port = 3000;
	}
	
	int listenfd;
	int optval = 1;

	struct sockaddr_in serveraddr;

	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	//消除bind时重用错误
	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
			(const void*)&optval, sizeof(int)) < 0)
		return -1;

	bzero((char *)&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)port);
	if(bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
		return -1;

	if(listen(listenfd, LISTENQ) < 0)
		return -1;

	return listenfd;
}

//使一个socket变为非阻塞的，如果监听套接字是阻塞的，下一个accept将会阻塞
int make_socket_non_blocking(int fd) {
	int flags;
	int s;

	flags = fcntl(fd, F_GETFL, 0);
	if(flags == -1){
		log_err("fcntl");
	}

	flags |= O_NONBLOCK;
	s = fcntl(fd, F_SETFL, flags);
	if(s == -1){
		log_err("fcntl");
		return -1;
	}
	//此处应返回s，而不是-1
	return s;
}

int read_conf(char *filename, my_conf_t *cfg, char *buf, int len){
	FILE *fp = fopen(filename, "r");
	if(!fp){
		log_err("cannot open config file: %s", filename);
		return MY_CONF_ERROR;
	}

	int pos = 0;
	char *delim_pos;
	int line_len;
	char *cur_pos = buf + pos;

	while(fgets(cur_pos, len-pos, fp)){
	    //配置文件中以'='为分隔符
		delim_pos = strstr(cur_pos, DELIM);
		line_len = strlen(cur_pos);

		if(!delim_pos)
			return MY_CONF_ERROR;

		if(cur_pos[strlen(cur_pos) - 1] == '\n'){
		    //此处需要修改为\0 否则端口转换会多出一个0
			cur_pos[strlen(cur_pos) - 1] = '\0';
		}

		if(strncmp("root", cur_pos, 4) == 0){
			cfg->root = delim_pos + 1;
		}

		if(strncmp("port", cur_pos, 4) == 0){
			cfg->port = atoi(delim_pos + 1);
		}
        //threadnum 不多出0 的原因是不以\n结尾
		if(strncmp("threadnum", cur_pos, 9) == 0){
			cfg->thread_num = atoi(delim_pos + 1);
		}

		cur_pos += line_len;
	}
//    log_info("port:%d\n", cfg->thread_num);
//    log_info("port:%d\n", cfg->port);

	fclose(fp);

	return MY_CONF_OK;
}

