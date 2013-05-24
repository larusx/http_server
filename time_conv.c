#include "time_conv.h"
static char* day[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static char* month[]={"Jan","Feb","Mar","Apr","May","June","July","Aug","Sep","Oct","Nov","Dec"};
static int find_number(char* str,int day_or_month)
{
	int i;
	if(day_or_month == 0)
	{
		for(i=0;i<7;i++)
		{
			if(!strcmp(str,day[i]))
				return i;
		}
	}
	else
	{
		for(i=0;i<12;i++)
		{
			if(!strcmp(str,month[i]))
				return i;
		}
	}
	return -1;
}

time_t GMT_time(char* GMT)
{
	struct tm time;
	char str[4];
	time_t return_time;
	strncpy(str,GMT,3);
	str[3]=0;
	time.tm_wday=find_number(str,0);
	strncpy(str,&GMT[5],2);
	str[2]=0;
	time.tm_mday=atoi(str);
	strncpy(str,&GMT[7],3);
	str[3]=0;
	time.tm_mon=find_number(str,1);
	strncpy(str,&GMT[11],4);
	str[4]=0;
	time.tm_year=1900+atoi(str);
	strncpy(str,&GMT[16],2);
	str[2]=0;
	time.tm_hour=atoi(str);
	strncpy(str,&GMT[19],2);
	str[2]=0;
	time.tm_min=atoi(str);
	strncpy(str,&GMT[22],2);
	str[2]=0;
	time.tm_sec=atoi(str);
	return	return_time=mktime(&time);
}

void time_GMT(time_t time,char* GMT)
{
	struct tm* t;
	t=gmtime(&time);
	sprintf(GMT,"%s, %02d %s %04d %02d:%02d:%02d GMT",day[t->tm_wday],t->tm_mday,month[t->tm_mon],1900+t->tm_year,t->tm_hour,t->tm_min,t->tm_sec);
	return;
}
