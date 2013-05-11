#include<stdio.h>
#include<iconv.h>
#include<string.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<stdlib.h>
#include<strings.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/socket.h>
#include<netinet/in.h>
#define RECV_SIZE 4000000
#define SEND_SIZE 10000000
#define PAGE_MAX_LEN 1000000
int log_fd;
char* code_200="HTTP/1.1 200 OK\r\nServer: Larusx Http Server\r\nConnection:close\r\n\r\n";
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
//子线程函数
void* accepted_func(void* arg)
{
	int accepted_sockfd=c_sockfd;
	char buf[128000];
	char web_buf[128000]={0};
	char filename[100]={0};
	char boundary[100];
	int read_len,head_len=0,file_len;
	char* recv_pos;
	char* end_pos;
	char* tmp_pos;
	int savefile_fd;
	char* send_buf=(char*)malloc(SEND_SIZE);
	char* page_buf=(char*)malloc(PAGE_MAX_LEN);

	head_len=recv(accepted_sockfd,buf,128000,0);
		
	if(recv_pos=strstr(buf,"Sec-WebSocket-Key:"))
	{
		write(log_fd,buf,head_len);
		recv_pos+=strlen("Sec-WebSocket-Key:");
		sscanf(recv_pos,"%s",filename);
		unsigned char* return_key=sha1_base64_key(filename,strlen(filename));
		//printf("%s\n",return_key);
		//printf("%s\n%d\n",filename,strlen(filename));
		sprintf(web_buf,"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\nSec-WebSocket-Location: ws://127.0.0.1:8888\r\n\r\n",return_key);
		send(accepted_sockfd,web_buf,strlen(web_buf),0);
		write(log_fd,web_buf,strlen(web_buf));
		free(return_key);
		head_len=recv(accepted_sockfd,buf,128000,0);
		write(log_fd,buf,head_len);
		send(accepted_sockfd,"hello",5,0);
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
					send(accepted_sockfd,code_200,strlen(code_200),0);
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
					send(accepted_sockfd,code_200,strlen(code_200),0);
			}
		}
		else
		{
			int content_len,file_extra_len,filename_len;
			write(log_fd,buf,head_len);
			recv_pos=strstr(buf,"boundary=");
			sscanf(recv_pos+strlen("boundary="),"%s",boundary);
			recv_pos=strstr(buf,"Content-Length:");
			sscanf(recv_pos+strlen("Content-Length:"),"%d",&content_len);
			end_pos=strstr(recv_pos,"\r\n\r\n");
			end_pos=strstr(end_pos,"--");
			tmp_pos=strstr(end_pos,"\r\n\r\n");
			file_extra_len=(int)(tmp_pos-end_pos)+4;
			recv_pos=strstr(recv_pos,"filename=\"");
			recv_pos=recv_pos+strlen("filename=\"");
			end_pos=recv_pos;
			filename_len=0;
			while(*end_pos++!='\"')
				filename_len++;
			strncpy(filename,recv_pos,filename_len);
			recv_pos=strstr(end_pos,"\r\n\r\n")+4;
			filename[filename_len]=0;
			savefile_fd=open(filename,O_WRONLY|O_CREAT|O_EXCL,0644);
			if(savefile_fd==-1)
			{
				send(accepted_sockfd,code_200,strlen(code_200),0);
				send(accepted_sockfd,"File Exists!",strlen("File Exists!"),0);
				close(accepted_sockfd);
				free(send_buf);
				free(page_buf);
				return;
			}
			if((head_len-(int)(recv_pos-buf))>=(content_len-file_extra_len-strlen(boundary)-8))
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
	log_fd=open("test",O_WRONLY|O_APPEND|O_CREAT|O_SYNC,0644);//日志文件
	c_sockfd=socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in s_sock; 
	struct sockaddr_in c_sock; 
	bzero(&s_sock,sizeof(struct sockaddr_in));
	s_sock.sin_family=AF_INET;
	s_sock.sin_addr.s_addr=htonl(INADDR_ANY);
	s_sock.sin_port=htons(8888);
	int s_sockfd=socket(AF_INET,SOCK_STREAM,0);
	setsockopt(s_sockfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
	bind(s_sockfd,(struct sockaddr *)(&s_sock),sizeof(struct sockaddr));
	int sin_size=sizeof(struct sockaddr_in);
	listen(s_sockfd,20);
	pthread_attr_t p_attr;
	pthread_attr_init(&p_attr);
	pthread_attr_setdetachstate(&p_attr,PTHREAD_CREATE_DETACHED);
	while(1)
	{
		c_sockfd=accept(s_sockfd,(struct sockaddr*)(&c_sock),&sin_size);
		pthread_create(&pthread_id,&p_attr,&accepted_func,NULL);
	}
	close(log_fd);
	close(s_sockfd);
	return 0;
}
