#include<stdio.h>
#include<string.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<error.h>
#include<signal.h>
#include<stdlib.h>
#include<strings.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include"list.h"
#include"time_conv.h"
#define RECV_SIZE 4000000
#define SEND_SIZE 10000000
#define PAGE_MAX_LEN 1000000
int log_fd;
int web_fd;
//连接的websocket
typedef struct websocket{
	socket_list* p_socket_list;
	pthread_mutex_t mutex;
}websocket;

websocket websocket_fds={NULL,PTHREAD_MUTEX_INITIALIZER};
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond=PTHREAD_COND_INITIALIZER;
extern char* sha1_base64_key(char *str,int str_len);

char* code_200="HTTP/1.1 200 OK\r\nServer: LarusX\r\nConnection:keep-alive\r\n\r\n";
int c_sockfd;
void page_select(const char* page_name,int* page_len,char* buf)
{
	int fd=open(page_name,O_RDONLY);
	int len=read(fd,buf,PAGE_MAX_LEN);
	*page_len=len;
	close(fd);
}
void parse_http_head(const char* head,char* filename)
{
	if(strstr(head,"GET"))
	{
		char* slash=strchr(head,'/');
		if(*(slash+1)==' ')
			filename[0]=0;
		else
			sscanf(slash+1,"%s",filename);
	}
}
void print_binary(unsigned char from)
{
	char to[9];
	to[8]=0;
	int move=1,i=7;
	for(i=7;i>=0;i--)
	{
		if(from&(move<<i))
			to[7-i]='1';
		else
			to[7-i]='0';
	}
#ifdef NDEBUG
	printf("%s\n",to);
#endif
}


//子线程函数
void* accepted_func(void* arg)
{
	pthread_mutex_lock(&mutex);
	int accepted_sockfd=c_sockfd;
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);
	char buf[128000];
	char web_buf[128000]={0};
	char code_cache[1024];
	char code_304[1024];
	char filename[100]={0};
	char boundary[100];
	char head_time[50];
	int read_len,head_len=0,file_len;
	char* file_ctime;  //文件修改时间
	char GMT_head_time[50]={0}; //http头Modified时间
	time_t head_time_t;
	char* recv_pos;
	char* end_pos;
	char* tmp_pos;
	int savefile_fd;
	char* send_buf=(char*)malloc(SEND_SIZE);
	char* page_buf=(char*)malloc(PAGE_MAX_LEN);

	head_len=recv(accepted_sockfd,buf,128000,0);
	
	//ajax请求
	if(recv_pos=strstr(buf,"X-Requested-With: XMLHttpRequest"))
	{
		write(log_fd,buf,strlen(buf));
		return;	
	}
	//websocket链接
	if(recv_pos=strstr(buf,"Sec-WebSocket-Key:"))
	{
		write(log_fd,buf,strlen(buf));
		//添加连接的websocket
		pthread_mutex_lock(&websocket_fds.mutex);
		list_insert(websocket_fds.p_socket_list,accepted_sockfd);
		pthread_mutex_unlock(&websocket_fds.mutex);
		//end	
		recv_pos+=strlen("Sec-WebSocket-Key:");
		sscanf(recv_pos,"%s",filename);
		char* return_key=sha1_base64_key(filename,strlen(filename));
		//printf("%s\n",return_key);
		//printf("%s\n",return_key);
		//printf("%s\n%d\n",filename,strlen(filename));
		sprintf(web_buf,"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\nSec-WebSocket-Location: ws://106.3.46.59:80\r\n\r\n",return_key);
		send(accepted_sockfd,web_buf,strlen(web_buf),0);
		write(log_fd,web_buf,strlen(web_buf));
		free(return_key);
		int i;
		unsigned char mask[4];
		unsigned char nchars;
		unsigned char ubuf[125000];
		while(head_len=recv(accepted_sockfd,ubuf,125000,0))
		{
			//for(i=0;i<head_len;i++)
		    //	print_binary(ubuf[i]);
			for(i=0;i<4;i++)
				mask[i]=ubuf[i+2];
			nchars=ubuf[1]&127;
			for(i=0;i<nchars;i++)
			{
				ubuf[i+6]=ubuf[i+6]^mask[i%4];
#ifdef NDEBUG
				printf("%c",ubuf[i+6]);
#endif
			}
#ifdef NDEBUG
			printf("\n");
#endif
			ubuf[0]=0x81;
			ubuf[1]=nchars;
			memcpy(&ubuf[2],&ubuf[6],nchars);
			write(web_fd,ubuf+2,nchars);
			write(web_fd,"\n",1);
			pthread_mutex_lock(&websocket_fds.mutex);
			//发送到链表上的websocket
			list_send(websocket_fds.p_socket_list,ubuf,2+nchars);
			pthread_mutex_unlock(&websocket_fds.mutex);
		}
#ifdef NDEBUG
		perror("socket:");
#endif
		//关闭连接移除websocket
		pthread_mutex_lock(&websocket_fds.mutex);
		list_remove(websocket_fds.p_socket_list,accepted_sockfd);
		pthread_mutex_unlock(&websocket_fds.mutex);
	}
	else 
	{
		if(!(recv_pos=strstr(buf,"Content-Length:")))
		{
			write(log_fd,buf,head_len);
			parse_http_head(buf,filename);	
			if(filename[0]==0)
			{
				int page_len;
				//		send(c_sockfd,index,index_len,0);
				page_select("a.html",&page_len,page_buf);
				send(accepted_sockfd,code_200,strlen(code_200),0);
				send(accepted_sockfd,page_buf,page_len,0);
			}
			else
			{
				int send_fd=open(filename,O_RDONLY);
				struct stat file_stat;
				if(send_fd!=-1)
				{
					fstat(send_fd,&file_stat);
					file_len=file_stat.st_size;
					time_GMT(file_stat.st_ctime,GMT_head_time);	
#ifdef NDEBUG
					printf("%s %d\n",GMT_head_time,file_stat.st_ctime-8*3600);
#endif
					if(!(recv_pos=strstr(buf,"If-Modified-Since: ")))
					{
label:			sprintf(code_cache,"HTTP/1.1 200 OK\r\nServer: LarusX\r\nConnection:keep-alive\r\nLast-Modified: %s\r\n\r\n",GMT_head_time);
						send(accepted_sockfd,code_cache,strlen(code_cache),0);
#ifdef NDEBUG
						perror("200:");
#endif
						while(file_len>0)
						{
							read_len=read(send_fd,send_buf,SEND_SIZE);
							send(accepted_sockfd,send_buf,read_len,0);
							file_len-=read_len;
						}
						//printf("%s\n",filename);
						close(send_fd);
					}
					else
					{
						sscanf(recv_pos+strlen("If-Modified-Since: "),"%[^\r]",head_time);
						head_time_t=GMT_time(head_time);
#ifdef NDEBUG
						printf("%s %d\n",head_time,head_time_t);
#endif
						if(file_stat.st_ctime - head_time_t > 8*3600)
							goto label;
						else
						{
							sprintf(code_304,"HTTP/1.1 304 Not Modified\r\nServer: LarusX\r\nConnection:keep-alive\r\nLast-Modified: %s\r\n\r\n",GMT_head_time);
							send(accepted_sockfd,code_304,strlen(code_304),0);
#ifdef NDEBUG
							perror("304:");
#endif
						}
					}
				}
				else
					send(accepted_sockfd,code_200,strlen(code_200),0);
			}
		}
		else
		{
			int content_len,file_extra_len,filename_len;
			write(log_fd,buf,head_len);
			recv_pos=strstr(buf,"Content-Length:");
			sscanf(recv_pos+strlen("Content-Length:"),"%d",&content_len);
			recv_pos=strstr(buf,"boundary=");
			sscanf(recv_pos+strlen("boundary="),"%s",boundary);
			recv_pos=strstr(recv_pos,"\r\n\r\n");
			if((end_pos=strstr(recv_pos,"----")) == NULL)
			{	
				send(accepted_sockfd,code_200,strlen(code_200),0);
				head_len=recv(accepted_sockfd,buf,128000,0);
				end_pos=strstr(buf,"----");
			}
		//	write(log_fd,buf,head_len);
#ifdef NDEBUG
			printf("%p\n",end_pos);
#endif
			tmp_pos=strstr(end_pos,"\r\n\r\n");
			file_extra_len=(int)(tmp_pos-end_pos)+4;
			recv_pos=strstr(end_pos,"filename=\"");
			recv_pos=recv_pos+strlen("filename=\"");
			end_pos=recv_pos;
			filename_len=0;
			while(*end_pos++!='\"')
				filename_len++;
			char* dir="/usr/upload/";
			strcpy(filename,dir);
			strncpy(filename+strlen(dir),recv_pos,filename_len);
			recv_pos=strstr(end_pos,"\r\n\r\n")+4;
			filename[strlen(dir)+filename_len]=0;
#ifdef NDEBUG
			printf("%s\n",filename);
#endif
			savefile_fd=open(filename,O_WRONLY|O_CREAT|O_EXCL,0644);
			if(savefile_fd==-1)
			{
				send(accepted_sockfd,"File Exists!",strlen("File Exists!"),0);
				close(accepted_sockfd);
				free(send_buf);
				free(page_buf);
				return;
			}
			if((head_len)>=(content_len-file_extra_len-strlen(boundary)-8))
			{
				read_len=write(savefile_fd,recv_pos,content_len-file_extra_len-strlen(boundary)-8);
			}
			else
			{
				read_len=write(savefile_fd,recv_pos,head_len-(int)(recv_pos-buf));
				//				printf("read %d\n",read_len);
				char* recv_buf=(char*)malloc(RECV_SIZE); 
				content_len=content_len-strlen(boundary)-8-file_extra_len;
				file_len=content_len;
				content_len=content_len-read_len;
				while(content_len>0)
				{
					read_len=recv(accepted_sockfd,recv_buf,RECV_SIZE,0);
					content_len-=read_len;
					write(savefile_fd,recv_buf,read_len);		
					//					printf("%d\n",read_len);
					//					printf("left %d\n",content_len);
				}
				if(content_len<0)
					ftruncate(savefile_fd,file_len);
				free(recv_buf);
			}
			send(accepted_sockfd,"Success!",strlen("Success!"),0);
			close(savefile_fd);
		}
	}
	free(send_buf);
	free(page_buf);
	close(accepted_sockfd);
}
int main()
{
	int on=1;
	pthread_t pthread_id;
	log_fd=open("log",O_WRONLY|O_APPEND|O_CREAT|O_SYNC,0644);//日志文件
	web_fd=open("record",O_WRONLY|O_APPEND|O_CREAT|O_SYNC,0644);//聊天记录
	c_sockfd=socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in s_sock; 
	struct sockaddr_in c_sock; 
	bzero(&s_sock,sizeof(struct sockaddr_in));
	s_sock.sin_family=AF_INET;
	s_sock.sin_addr.s_addr=htonl(INADDR_ANY);
	s_sock.sin_port=htons(80);
	int s_sockfd=socket(AF_INET,SOCK_STREAM,0);
	setsockopt(s_sockfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
	bind(s_sockfd,(struct sockaddr *)(&s_sock),sizeof(struct sockaddr));
	socklen_t sin_size=sizeof(struct sockaddr);
	listen(s_sockfd,20);
	pthread_attr_t p_attr;
	pthread_attr_init(&p_attr);
	pthread_attr_setdetachstate(&p_attr,PTHREAD_CREATE_DETACHED);
	//生成websocket链表
	websocket_fds.p_socket_list=list_create();
	signal(SIGPIPE,SIG_IGN);
	while(1)
	{
		c_sockfd=accept(s_sockfd,(struct sockaddr*)(&c_sock),&sin_size);
#ifdef NDEBUG
		perror("accept:");
#endif
		pthread_mutex_lock(&mutex);
		pthread_create(&pthread_id,&p_attr,&accepted_func,NULL);
		pthread_cond_wait(&cond,&mutex);
		pthread_mutex_unlock(&mutex);

	}
	//销毁websocket链表
	list_clear(websocket_fds.p_socket_list);
	close(log_fd);
	close(s_sockfd);
	return 0;
}
