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
//创建链表，生成第一个节点。
socket_list* list_create()
{
	socket_list* head=(socket_list*)malloc(sizeof(socket_list));
	head->pnext=NULL;
	head->sock_fd=-2;
	return head;
}
//添加链接的websocket描述字
void list_insert(socket_list* head,int fd)
{
	socket_list* node=(socket_list*)malloc(sizeof(socket_list));
	while(head->pnext!=NULL)
		head=head->pnext;
	head->pnext=node;
	node->pnext=NULL;
	node->sock_fd=fd;
	#ifdef PRINT
	printf("insert %d\n",node->sock_fd);
	#endif
}
//删除断开链接的socket
void list_remove(socket_list* head,int fd)
{
	socket_list* cur=head;
	socket_list* tmp;
	while(cur->pnext!=NULL)
	{
		if(cur->pnext->sock_fd==fd)
		{
			tmp=cur->pnext;
			#ifdef PRINT
			printf("remove %d\n",tmp->sock_fd);
			#endif
			cur->pnext=tmp->pnext;
			free(tmp);
			return;
		}
		cur=cur->pnext;
	}
}
//清除链表
void list_clear(socket_list* head)
{
	socket_list* cur=head;
	socket_list* tmp;
	while(cur!=NULL)
	{
		tmp=cur;
		cur=cur->pnext;
		free(tmp);
	}
}
//发送信息到链表上的websocket
void list_send(socket_list* head,char* buf,int length)
{
	#ifdef PRINT
	printf("sendto ");
	#endif
	while(head->pnext!=NULL)
	{
		head=head->pnext;
		send(head->sock_fd,buf,length,0);
		#ifdef PRINT
		printf("%d ",head->sock_fd);
		#endif
	}
#ifdef PRINT
	printf("\n");
#endif
}
