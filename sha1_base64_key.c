#include<stdint.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include"sha1.h"
unsigned char* sha1_base64_key(unsigned char *str,int str_len)
{
	int i,j;
//	unsigned char str[100]="BwLnAfgkPHY+R/zxLR8E2A==";
	unsigned char* key="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	strcat(str,key);
	SHA1Context sha;
	uint8_t result[20];
	SHA1Reset(&sha);
	SHA1Input(&sha,str,36+str_len);
	SHA1Result(&sha,result);
//	unsigned char sha1_str[100];
//	for(i=0,j=0;i<20;i++)
//	{
//		sprintf(&sha1_str[j],"%02x",result[i]);
//		j+=2;
//	}
//	for(i=0;i<40;i++)
//		printf("%c",sha1_str[i]);
//	printf("\n");
	unsigned char* base64_str;
	base64_str=base64_encode(result,20);
//	printf("%s\n",base64_str);
//	free(base64_str);
	return base64_str;
}
