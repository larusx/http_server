//
//
// list.c
//
//
//
//websocket 链表
#define PRINT


#include<malloc.h>
#include"list.h"
#define LIST_LEN 1000
//创建链表，生成第一个节点。

socket_list* list_create()
{
	int i;
	list_head = malloc(sizeof(socket_list)*LIST_LEN);
  list_tail = list_head;
	socket_list* head=list_head;
	for(i=0;i<LIST_LEN;i++)
	{
		list_tail=(char*)list_tail+sizeof(socket_list);
		head->sock_fd=-2;
		head->pnext=list_tail;
		head=list_tail;
	}
	list_tail=(char*)list_tail-sizeof(socket_list);
	return (socket_list*)list_head;
}
//添加链接的websocket描述字
void list_insert(socket_list* head,int fd)
{
	while(head->pnext->sock_fd != -2)
		head=head->pnext;
	head->pnext->sock_fd=fd;
	#ifdef PRINT
	printf("insert %d\n",head->sock_fd);
	#endif
}
//删除断开链接的socket
void list_remove(socket_list* head,int fd)
{
	while(head->pnext->sock_fd != fd)
		head=head->pnext;
	list_tail->pnext=head->pnext;
	head->pnext=head->pnext->pnext;
	list_tail=list_tail->pnext;
	list_tail->sock_fd=-2;

}
//清除链表
void list_clear(socket_list* head)
{
	free(list_head);
}
//发送信息到链表上的websocket
void list_send(socket_list* head,char* buf,int length)
{
	#ifdef PRINT
	printf("sendto ");
	#endif
	while(head->pnext->sock_fd != -2)
	{
		send(head->pnext->sock_fd,buf,length,0);
		#ifdef PRINT
		printf("%d ",head->pnext->sock_fd);
		#endif
		head=head->pnext;
	}
#ifdef PRINT
	printf("\n");
#endif
}
