#ifndef __TIME_CONV_H
#define __TIME_CONV_H
#include<time.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
/* GMT时间格式转换成秒数*/
time_t GMT_time(char* GMT);

/* 秒数转GMT时间格式*/
void time_GMT(time_t time,char* GMT);

#endif
