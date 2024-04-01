
/*

 * */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stdarg.h>

#define LOG_PATH "/root/log/"



void myjc_log_printf(const char *format,...)
{
		va_list ap; 				 /*初始化一个列表指针*/
		unsigned int str_length;
		//char print_buff[1024];
		char variable_buff[512];
		char buf_time[32];
		char file_name_buff[128] = {'\0'};
		char file_name_buff1[128] = {'\0'};
		time_t t;
		struct tm tim;
		FILE* fp_write=NULL;
		struct stat statbuf;
	
		va_start(ap,format);		/*将可变参数列表压入栈中*/
		vsnprintf(variable_buff,sizeof(variable_buff),format,ap);  /*将可变参数存入variable_buff*/
		va_end(ap); 				/*释放ap指针*/
	
	
		//目录是否存在？
		if(stat(LOG_PATH, &statbuf))  //出错
		{
			perror("stat");
			if(2 == errno)	//文件不存在
			{
				if(-1 == mkdir(LOG_PATH,0775))
				{
					perror("mkdir");
					return ;
				}
				goto go_on;
			}
			else
			{
				return ;
			}
		}
	
	go_on:
	
		t = time(NULL);  //获得系统时间
		localtime_r(&t, &tim);
		
		//打开文件
		strcpy(file_name_buff,LOG_PATH);
		strcat(file_name_buff,"log.txt");
		fp_write = fopen(file_name_buff, "a");	//转换文件描述符
		if(NULL == fp_write)
		{
			perror("ERROR:open log file ");
			return ;
		}
	
		//printf("fopen %s success!!\n",file_name_buff);
		//
		snprintf(buf_time,sizeof(buf_time),"[%4d_%02d_%02d %02d:%02d:%02d]",tim.tm_year+1900,tim.tm_mon+1,tim.tm_mday
																				,tim.tm_hour,tim.tm_min,tim.tm_sec);
		
		fwrite(buf_time,1,strlen(buf_time),fp_write);	//先写入时间
		fwrite(variable_buff,1,strlen(variable_buff),fp_write); 	//再写入内容
		fwrite("\n",1,2,fp_write);	   //再写入内容
	
		//fwrite(buf_time,1,strlen(buf_time),stdout);   //先写入时间
		//fwrite(variable_buff,1,strlen(variable_buff),stdout);	  //再写入内容
	
	
		str_length = ftell(fp_write);	//文件长度？
		fclose(fp_write);
		//printf("str_length = %d !!\n",str_length);
		if(str_length > 1024*1024)	//大于1m
		{	
			int loop;
			for(loop = 1; loop < 21; ++loop) {
				sprintf(file_name_buff1,"%slog_%d.txt",LOG_PATH,loop);
				if (access(file_name_buff1, F_OK) != -1) {
					printf("%s 存在\n", file_name_buff1);
				} 
				else {
					 printf("%s 不存在\n", file_name_buff1);
					 rename(file_name_buff, file_name_buff1);
				}
			}
	
			if(loop == 21)	 //已经到了20个文件
			{
				for(loop = 2; loop < 21; ++loop) {
					sprintf(file_name_buff,"%slog_%d.txt",LOG_PATH,loop);
					sprintf(file_name_buff1,"%slog_%d.txt",LOG_PATH,loop-1);
					if (access(file_name_buff, F_OK) != -1) {
						printf("%s 存在\n", file_name_buff);
						unlink(file_name_buff1);  //把原来的删除
						rename(file_name_buff, file_name_buff1);  //名字换一下
					} 
					else {
						 printf("%s 不存在\n", file_name_buff);
						 break;
					}
				}
	
			}
		}
		//printf("%s",print_buff);			 /*将数据打印到串口*/
		

}


#if 0
extern int is_wdg_feeding;   //喂狗了吗？

void *g_wdg_feeding_watch_thread(void *param) {

	while(1) {
		sleep(2);
		if(is_wdg_feeding == 0)  //没有喂狗
		{
			myjc_log_printf("is_wdg_feeding == 0, no feeding !!!!!\n");
		}
		else
		{
			is_wdg_feeding = 0;
		}
		
	}

	return NULL;
}
#endif


