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
void page_select(const char* page_name,int* page_len,char* buf)
{
	int fd=open(page_name,O_RDONLY);
	int len=read(fd,buf,PAGE_MAX_LEN);
	*page_len=len;
	close(fd);
}
void parse_GET_filename(const char* head,char* filename)
{
	char* slash=strchr(head,'/');
	if(*(slash+1)==' ')
		filename[0]=0;
	else
		sscanf(slash+1,"%s",filename);
}
void parse_http_head(const char* head,char* filename)
{
	if(strstr(head,"GET"))
		parse_GET_filename(head,filename);
}
int main()
{
	int fd=open("test",O_WRONLY|O_APPEND|O_CREAT|O_SYNC,0644);
	int savefile_fd;
	char filename[100];
	char* page;
	char* recv_pos;
	char* end_pos;
	char* tmp_pos;
	char boundary[100];
	int read_len,on=1,content_len,filename_len,file_extra_len,head_len,file_len;
	char buf[4097]={0};
	char* recv_buf=(char*)malloc(RECV_SIZE); 
	char* send_buf=(char*)malloc(SEND_SIZE);
	char* page_buf=(char*)malloc(PAGE_MAX_LEN);
	struct stat file_stat;
	struct sockaddr_in s_sock; struct sockaddr_in c_sock; 
	bzero(&s_sock,sizeof(struct sockaddr_in));
	s_sock.sin_family=AF_INET;
	s_sock.sin_addr.s_addr=htonl(INADDR_ANY);
	s_sock.sin_port=htons(80);
	int s_sockfd=socket(AF_INET,SOCK_STREAM,0);
	int c_sockfd=socket(AF_INET,SOCK_STREAM,0);
	setsockopt(s_sockfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
	bind(s_sockfd,(struct sockaddr *)(&s_sock),sizeof(struct sockaddr));
	int sin_size=sizeof(struct sockaddr_in);
	listen(s_sockfd,10);
	char* re="HTTP/1.1 200 OK\r\nServer: Test http server\r\nConnection:close\r\n\r\n";
	while(1)
	{
		c_sockfd=accept(s_sockfd,(struct sockaddr*)(&c_sock),&sin_size);
		filename[0]=0;
		read_len=recv(c_sockfd,buf,4096,0);
		head_len=read_len;
		buf[read_len]=0;
		if(!(recv_pos=strstr(buf,"Content-Length:")))
		{
			write(fd,buf,read_len);
			parse_http_head(buf,filename);	
		}
		else
		{
			write(fd,buf,read_len);
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
				send(c_sockfd,re,strlen(re),0);
				send(c_sockfd,"File Exists!",strlen("File Exists!"),0);
				close(c_sockfd);
				continue;
			}
			
			if((head_len-(int)(recv_pos-buf))>=(content_len-file_extra_len-strlen(boundary)-8))
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
					read_len=recv(c_sockfd,recv_buf,RECV_SIZE,0);
					content_len-=read_len;
					write(savefile_fd,recv_buf,read_len);		
//					printf("%d\n",read_len);
//					printf("left %d\n",content_len);
				}
				if(content_len<0)
					ftruncate(savefile_fd,file_len);
			}
			close(savefile_fd);
			filename[0]=0;
		}
		if(filename[0]==0)
		{
			int page_len;
	//		send(c_sockfd,index,index_len,0);
			page_select("a.html",&page_len,page_buf);
			send(c_sockfd,re,strlen(re),0);
			send(c_sockfd,page_buf,page_len,0);
			recv_pos=strstr(buf,"Sec-WebSocket-Key:");
			if(recv_pos!=NULL)
			{
				recv_pos+=strlen("Sec-WebSocket-Key:");
				sscanf(recv_pos,"%s",filename);
				unsigned char* return_key=sha1_base64_key(filename,strlen(filename));
				//printf("%s\n",return_key);
				sprintf(buf,"HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\nUpgrade: websocket\r\n\r\n",return_key);
				send(c_sockfd,buf,4096,0);
				free(return_key);
			}
		}
		else
		{
			int send_fd=open(filename,O_RDONLY);
			if(send_fd!=-1)
			{
				fstat(send_fd,&file_stat);
				file_len=file_stat.st_size;
				send(c_sockfd,re,strlen(re),0);
				while(file_len>0)
				{
					read_len=read(send_fd,send_buf,SEND_SIZE);
					send(c_sockfd,send_buf,read_len,0);
					file_len-=read_len;
				}
				printf("%s\n",filename);
				close(send_fd);
			}
			else
				send(c_sockfd,re,strlen(re),0);
		}
		close(c_sockfd);
	}
	free(recv_buf);
	free(send_buf);
	free(page_buf);
	close(fd);
	close(s_sockfd);
	return 0;
}
