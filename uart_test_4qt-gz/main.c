/*
* @Author: dazhi
* @Date:   2022-11-14 16:08:49
* @Last Modified by:   dazhi
* @Last Modified time: 2023-04-21 16:41:26
*/
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/kd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>             // exit
#include <sys/ioctl.h>          // ioctl
#include <string.h>             // bzero
#include <pthread.h>
#include <semaphore.h>

#include <stdarg.h>

#include "ComFunc.h"

static const char* my_opt = "Dvhpwb:d:";
static int uart_fd;

void show_version(char* name)
{
    printf( "%s Buildtime :"__DATE__" "__TIME__,name);
    printf( "\n");
}
 
void usage(char* name)
{
    show_version(name);
 
    printf("    -h,    short help\n");
    printf("    -v,    show version\n");
    printf("    -d /dev/ttyS0, select com device\n");
    printf("    -p , printf recv data\n");
    printf("    -b , set baudrate\n");
    printf("    -n , set com nonblock mode\n");
    exit(0);
}


/* This function will open the uInput device. Please make 
sure that you have inserted the uinput.ko into kernel. */ 
int uart_init(int argc, char *argv[]) 
{
	int nonblock=1;
//	int i=0;
	char* com_port = "/dev/ttyS0";
	int p_opt = 0;
	int c;
	int baudrate = 115200;


//	create_queue(&keyCmdQueue);//´´½¨¼üÅÌÏûÏ¢»·ÐÎ¶ÓÁÐ
//	create_queue(&mouseCmdQueue);//´´½¨Êó±êÏûÏ¢»·ÐÎ¶ÓÁÐ
	printf("Program %s is running\n", argv[0]);
    if(argc != 1)
	{
	//	printf("usage: ./kmUtil keyboardComName mouseComName\n");		
	    while(1)
	    {
	        c = getopt(argc, argv, my_opt);
	        //printf("optind: %d\n", optind);
	        if (c < 0)
	        {
	            break;
	        }
	        //printf("option char: %x %c\n", c, c);
	        switch(c)
	        {
	        case 'p':
	        		p_opt = 1;
	                //debug_level = atoi(optarg);
	                printf("p_opt = 1\n");
	                break;
	        case 'd':
	        	//	com_port = 	
	                if(strncmp(optarg,"/dev/tty",8) == 0)
	             		com_port = optarg;
	             	else
	             		printf("select device error ,start with /dev/tty please!\n");
	                printf("com_port = %s.\n\n",com_port);
	                break;
	        case 'b':
	        		baudrate = atoi(optarg);
	        		if(baudrate < 1200)
	        			baudrate = 115200;
	                printf("set baudrate is: %d\n\n", baudrate);
	             //   p1 = optarg;
	                break;
	        case 'n':
	                printf("set com nonblock mode\n\n");
	                nonblock = 1;
	                break;
	        case ':':
	                fprintf(stderr, "miss option char in optstring.\n");
	                break;
	        case 'D':
	        	break;
	        case '?':
	        case 'h':
	        default:
	                usage(argv[0]);
	                break;
	                //return 0;
	        }
	    }
	    if (optind == 1)
	    {
	        usage(argv[0]);
	    }
	}
	// Return an error if device not found. 
	// if (setup_uinput_device())   //正确返回0
	// { 
	// 	printf("Unable to find uinput device \n"); 
	// 	return -1; 
	// } 
	
	uart_fd = PortOpen(com_port,nonblock);   //²ÎÊý2Îª0±íÊ¾Îª×èÈûÄ£Ê½£¬·Ç0Îª·Ç×èÈûÄ£Ê½
	if( uart_fd < 0 )
	{
	//	uinput_device_close();
		printf("Error: open serial port(%s) error.\n", com_port);	
		exit(1);
	}

	return PortSet(uart_fd,baudrate,1,'N');    //设置波特率等	
}


//程序退出时，串口部分的处理
void uart_exit(void) 
{
//	uinput_device_close();
	close(uart_fd);
}











//const char* my_opt = "vhpwb:d:";

/* This function will open the uInput device. Please make 
sure that you have inserted the uinput.ko into kernel. */ 
int main(int argc, char *argv[]) 
{
	char buf[16] = "1234567890\n";

	uart_init(argc, argv) ;
	show_version(argv[0]);
	//接收 打印 输出
	while(1)
	{
		if(PortRecv(uart_fd, buf, 15)>0)
		{
			printf("%s\n",buf);
			fflush(stdout);
		}	
		PortSend(uart_fd, buf, 11);

		usleep(500000);
	}

	uart_exit() ;

	return 0;
}



