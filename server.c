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
#include<netinet/tcp.h>
//New feature!
#include<sys/sendfile.h>
#include<sys/epoll.h>

#include"list.h"
#include"time_conv.h"
#define RECV_SIZE 1000000
#define SEND_SIZE 1000000
#define PAGE_MAX_LEN 1000000
#define NTHREAD 20
int log_fd;
int web_fd;
//连接的websocket
typedef struct websocket{
	socket_list* p_socket_list;
	pthread_mutex_t mutex;
}websocket;

websocket websocket_fds={NULL,PTHREAD_MUTEX_INITIALIZER};
socket_list* task_queue_head;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t task_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t task_cond=PTHREAD_COND_INITIALIZER;
pthread_cond_t cond=PTHREAD_COND_INITIALIZER;
extern char* sha1_base64_key(char *str,int str_len);

char* code_200="HTTP/1.1 200 OK\r\nSet-cookie: id=3322\r\nServer: LarusX\r\nConnection:keep-alive\r\n\r\n";
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
	char send_buf[SEND_SIZE];
	char page_buf[PAGE_MAX_LEN];
	int on=1;
	char* return_key;
	int accepted_sockfd;
	int i;
	unsigned char mask[4];
	unsigned char nchars;
	char ubuf[125000];
	unsigned char nickname[1000];
	int nickname_len=0;
	char* tell="说：";
	int tell_len=strlen(tell);
	char* login="登陆了！";
	int login_len=strlen(login);
	int send_fd;
	struct stat file_stat;
	int content_len,file_extra_len,filename_len;
	char* dir="/home/apple/upload/";
	char* recv_buf=(char*)malloc(RECV_SIZE); 

//	setsockopt(accepted_sockfd,SOL_TCP,TCP_NODELAY,&on,sizeof(int));
//	SOL_SOCKET
while(1)
{
begin:pthread_mutex_lock(&mutex);
	while(!(accepted_sockfd=list_get_task(task_queue_head)))
		pthread_cond_wait(&cond,&mutex);
	pthread_mutex_unlock(&mutex);
	
	head_len=recv(accepted_sockfd,buf,128000,0);
	if(recv_pos=strstr(buf,"OPTIONS"))
	{
		sprintf(web_buf,"HTTP/1.1 200 OK\r\nAllow: GET, POST, OPTIONS\r\nContent-Length: 0\r\n\r\n");
		send(accepted_sockfd,web_buf,strlen(web_buf),0);
		write(log_fd,buf,strlen(buf));
		goto file;
	}
	//ajax请求
	else if(recv_pos=strstr(buf,"X-Requested-With: XMLHttpRequest"))
	{
		write(log_fd,buf,strlen(buf));
		goto file;
	}
	//websocket链接
	else if(recv_pos=strstr(buf,"Sec-WebSocket-Key:"))
	{
		write(log_fd,buf,strlen(buf));
		//添加连接的websocket
		pthread_mutex_lock(&websocket_fds.mutex);
		list_insert(websocket_fds.p_socket_list,accepted_sockfd);
		pthread_mutex_unlock(&websocket_fds.mutex);
		//end	
		recv_pos+=strlen("Sec-WebSocket-Key:");
		sscanf(recv_pos,"%s",filename);
		return_key=sha1_base64_key(filename,strlen(filename));
		//printf("%s\n",return_key);
		//printf("%s\n",return_key);
		//printf("%s\n%d\n",filename,strlen(filename));
		sprintf(web_buf,"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\nSec-WebSocket-Location: ws://106.3.46.59:80\r\n\r\n",return_key);
		send(accepted_sockfd,web_buf,strlen(web_buf),0);
		write(log_fd,web_buf,strlen(web_buf));
		free(return_key);
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
			if(strncmp(&ubuf[2],"/nick",5) ==0)
			{
				nickname[0]=0x81;
				memcpy(&nickname[2],&ubuf[7],nchars-5);
				strcpy(&nickname[nchars-3],login);
				nickname_len=nchars-5;
				nickname[1]=nickname_len+login_len;
				pthread_mutex_lock(&websocket_fds.mutex);
				//发送到链表上的websocket
				list_send(websocket_fds.p_socket_list,nickname,nickname_len+login_len+2);
				pthread_mutex_unlock(&websocket_fds.mutex);
				continue;
			}
			write(web_fd,ubuf+2,nchars);
			write(web_fd,"\n",1);
			if(nickname_len == 0)
			{
				pthread_mutex_lock(&websocket_fds.mutex);
				//发送到链表上的websocket
				list_send(websocket_fds.p_socket_list,ubuf,2+nchars);
				pthread_mutex_unlock(&websocket_fds.mutex);
			}
			else
			{
				ubuf[1]=nickname_len+tell_len+nchars;
				strcpy(&nickname[nickname_len+2],tell);
				strncpy(&nickname[nickname_len+tell_len+2],&ubuf[2],nchars);
				strncpy(&ubuf[2],&nickname[2],nchars+nickname_len+tell_len);
				pthread_mutex_lock(&websocket_fds.mutex);
				//发送到链表上的websocket
				list_send(websocket_fds.p_socket_list,ubuf,2+nchars+nickname_len+tell_len);
				pthread_mutex_unlock(&websocket_fds.mutex);

			}
		}
//#ifdef NDEBUG
		perror("socket:");
//#endif
		//关闭连接移除websocket
		pthread_mutex_lock(&websocket_fds.mutex);
		list_remove(websocket_fds.p_socket_list,accepted_sockfd);
		pthread_mutex_unlock(&websocket_fds.mutex);
	}
	else 
	{
file:		if(!(recv_pos=strstr(buf,"Content-Length:")))
		{
			write(log_fd,buf,head_len);
			parse_http_head(buf,filename);	
			if(filename[0]==0)
			{
				strcpy(filename,"a.html");
			}
			send_fd=open(filename,O_RDONLY);
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
					//while(file_len>0)
					//{
					//	read_len=read(send_fd,send_buf,SEND_SIZE);
					//	send(accepted_sockfd,send_buf,read_len,0);
					//	file_len-=read_len;
					//}
					//sendfile减少拷贝次数
					sendfile(accepted_sockfd,send_fd,0,file_len);
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
						sprintf(code_304,"HTTP/1.1 304 Not Modified\r\nServer: LarusX\r\nConnection:keep-alive\r\nSet-cookie: id=\"33\", domain=\"baidu.com\"\r\nLast-Modified: %s\r\n\r\n",GMT_head_time);
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
		else
		{
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
				goto begin;
			}
			if((head_len)>=(content_len-file_extra_len-strlen(boundary)-8))
			{
				read_len=write(savefile_fd,recv_pos,content_len-file_extra_len-strlen(boundary)-8);
			}
			else
			{
				read_len=write(savefile_fd,recv_pos,head_len-(int)(recv_pos-buf));
				//				printf("read %d\n",read_len);
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
			}
			send(accepted_sockfd,"Success!",strlen("Success!"),0);
			close(savefile_fd);
		}
	}
	close(accepted_sockfd);
}
}
void daemonize()
{
	int fd=open("/dev/null",O_RDWR);
	if(fork()==0)
	{
		dup2(fd,STDIN_FILENO);
		dup2(fd,STDOUT_FILENO);
		dup2(fd,STDERR_FILENO);
		close(fd);
		setsid();
		umask(0);
	}
	else
	{
		exit(0);
	}
}

int setnonblocking( int fd )
{
	int old_option = fcntl( fd, F_GETFL );
	int new_option = old_option | O_NONBLOCK;
	fcntl( fd, F_SETFL, new_option );
	return old_option;
}

void addfd( int epoll_fd, int fd )
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLOUT | EPOLLET | EPOLLERR;
	epoll_ctl( epoll_fd, EPOLL_CTL_ADD, fd, &event );
	setnonblocking( fd );
}

int main()
{
	daemonize();
	int on=1;
	int i;
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
	task_queue_head=list_create();
	int nthread=NTHREAD;
	for(i=0;i<NTHREAD;i++)
		pthread_create(&pthread_id,&p_attr,&accepted_func,NULL);

	signal(SIGPIPE,SIG_IGN);
	while(1)
	{
		c_sockfd=accept(s_sockfd,(struct sockaddr*)(&c_sock),&sin_size);
#ifdef NDEBUG
		perror("accept:");
#endif
		pthread_mutex_lock(&mutex);
		list_insert(task_queue_head,c_sockfd);
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);

	}
	//销毁websocket链表
	list_clear(websocket_fds.p_socket_list);
	close(log_fd);
	close(s_sockfd);
	return 0;
}
