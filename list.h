#ifndef _LIST_H
#define _LIST_H
//链表结构
typedef struct socket_list{
	int sock_fd;
	struct socket_list* pnext;
}socket_list;
#endif
