#ifndef _LIST_H
#define _LIST_H
#include<sys/socket.h>

//链表结构
typedef struct socket_list{
	int sock_fd;
	struct socket_list* pnext;
}socket_list;

void* list_head;

socket_list* list_tail;

socket_list* list_create(void);

void list_insert(socket_list* head,int fd);

void list_remove(socket_list* head,int fd);

int list_get_task(socket_list* head);

void list_clear(socket_list* head);

void list_send(socket_list* head,char* buf,int length);

#endif
