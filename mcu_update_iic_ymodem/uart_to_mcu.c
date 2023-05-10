// main.c 
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// #include <linux/input.h>
// #include <linux/uinput.h>
#include <linux/kd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>             // exit
#include <sys/ioctl.h>          // ioctl
#include <string.h>             // bzero
// #include <pthread.h>
// #include <semaphore.h>

#include <stdarg.h>

#include "iicFunc.h"


//#include "Common.h"

/*global defines*/
//static QUEUE keyCmdQueue;
//static QUEUE mouseCmdQueue;


static 	int p_opt = 0;   //打印调试信息，默认不打印
static 	int uart_fd;



static void show_version(char* name)
{
    printf( "%s Buildtime :"__DATE__" "__TIME__,name);
}
 


int uart_init(int argc, char *argv[])
{
	if(i2c_init() < 0)
	{
		printf("i2c_init() < 0 \n");
		exit(-1);
	}
}



void uart_exit()
{
	i2c_close();
}




/*
*********************************************************************************************************
*	函 数 名: UART_ReceiveByte
*	功能说明: 接收发送端发来的字符         
*	形    参：c  字符
*             timeout  溢出时间
*	返 回 值: 0 接收成功， -1 接收失败
*********************************************************************************************************
*/
int UART_ReceiveByte (uint8_t *c, uint32_t timeout)
{
	return IIC3_0x40_ReceiveByte (c, timeout);
}

/*
*********************************************************************************************************
*	函 数 名: UART_ReceivePacket
*	功能说明: 接收发送端发来的字符         
*	形    参：data  数据
*             timeout  溢出时间
*	返 回 值: 0 接收成功， -1 接收失败
*********************************************************************************************************
*/
int UART_ReceivePacket (uint8_t *data, uint16_t length, uint32_t timeout)
{
	return IIC3_0x40_ReceivePacket (data, length, timeout);
}






/*
*********************************************************************************************************
*	函 数 名: Uart_SendByte
*	功能说明: 发送一个字节数据         
*	形    参：c  字符
*	返 回 值: 0
*********************************************************************************************************
*/
void UART_SendByte (uint8_t c)
{
	IIC3_0x40_SendByte (c);
}




/*
*********************************************************************************************************
*	函 数 名: UART_SendPacket
*	功能说明: 发送一串数据
*	形    参: data  数据
*             length  数据长度
*	返 回 值: 无
*********************************************************************************************************
*/
void UART_SendPacket(uint8_t *data, uint16_t length)
{
	IIC3_0x40_SendPacket(data, length);
}



