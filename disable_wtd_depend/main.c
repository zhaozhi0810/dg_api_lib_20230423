

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/input.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
//#include "../include/drv722.h"
#include <termios.h>
#include "debug.h"


//aarch64-linux-gnu-gcc ./main.c -o wdDisable 
int mcu_watchdog_disable(void);

#define MCU_UART_DEVICE			"/dev/ttyS0"
#define MCU_COMM_CMD_WATCHDOG_ENABLE		(0x8a)
#define MCU_UART_FRAME_DATA_LENTH	(4)
#define MCU_UART_FRAME_HEADER	(0xa5)
#define MCU_UART_FRAME_TAIL		(0x5a)



enum {
	MCU_WATCHDOG_DISABLE,
	MCU_WATCHDOG_ENABLE,
};


#define UART_FRAME_VERIFY(arg0, arg1, arg2, arg3, arg4)	\
	((arg0 + arg1 + arg2 + arg3 + arg4) & 0xFF)

typedef struct {
	unsigned char header;
	unsigned char cmd;
	unsigned char data0;
	unsigned char data1;
	unsigned char data2;
	unsigned char data3;
	unsigned char verify;
	unsigned char tail;
} UART_FRAME_FORMAT_S;




static int s_uart_device_param_init(int uart_fd) {
	CHECK(uart_fd > 0, -1, "Error uart_fd is %d", uart_fd);

	struct termios termios;

	CHECK(!tcgetattr(uart_fd, &termios), -1, "Error tcgetattr with %d: %s", errno, strerror(errno));
	cfmakeraw(&termios);
	CHECK(!cfsetspeed(&termios, B115200), -1, "Error cfsetspeed with %d: %s", errno, strerror(errno));
	//termios.c_lflag &= (~(ICANON|ECHO|ECHOE|ISIG)); /*lsr add 20220406*/	
	termios.c_cflag &= (~CSIZE);  //控制模式，屏蔽字符大小位
	termios.c_cflag |= CS8;       //data bits 8位数据
	termios.c_cflag &= (~PARENB);   //no parity check
	termios.c_cflag &= (~CSTOPB);   //1 stop bits
	tcflush(uart_fd, TCIOFLUSH);				//溢出的数据可以接收，但不读
	CHECK(!tcsetattr(uart_fd, TCSANOW, &termios), -1, "Error tcsetattr with %d: %s", errno, strerror(errno));
	return 0;
}

int uart_device_init(char *file) {
	CHECK(file, -1, "Error file is null!");
	int uart_fd = 0;
	CHECK((uart_fd = open(file, O_RDWR)) > 0, -1, "Error open with %d: %s", errno, strerror(errno));
	if(s_uart_device_param_init(uart_fd)) {
		ERR("Error s_uart_device_param_init!");
		CHECK(!close(uart_fd), -1, "Error close with %d: %s", errno, strerror(errno));
		return -1;
	}
	return uart_fd;
}


int uart_send(int uart_fd, void *buf, long unsigned int buf_lenth) {
	CHECK(uart_fd > 0, -1, "Error uart has not been init!");
	CHECK(buf, -1, "Error buf is null!");
	CHECK(buf_lenth, -1, "Error buf_lenth is %lu", buf_lenth);

	CHECK(write(uart_fd, buf, buf_lenth) == buf_lenth, -1, "Error write with %d: %s", errno, strerror(errno));
	return 0;
}



static int s_mcu_uart_send(int mcu_uart_fd, int mcu_uart_cmd, char *send_buf) {
	CHECK(mcu_uart_fd > 0, -1, "Error mcu_uart_fd is %d", mcu_uart_fd);
	UART_FRAME_FORMAT_S cmd_send_format;

	bzero(&cmd_send_format, sizeof(UART_FRAME_FORMAT_S));
	cmd_send_format.header = MCU_UART_FRAME_HEADER;
	cmd_send_format.cmd = mcu_uart_cmd;
	if(send_buf) {
		cmd_send_format.data0 = send_buf[0];
		cmd_send_format.data1 = send_buf[1];
		cmd_send_format.data2 = send_buf[2];
		cmd_send_format.data3 = send_buf[3];
	}
	cmd_send_format.verify = UART_FRAME_VERIFY(
			cmd_send_format.cmd,
			cmd_send_format.data0,
			cmd_send_format.data1,
			cmd_send_format.data2,
			cmd_send_format.data3);
	cmd_send_format.tail = MCU_UART_FRAME_TAIL;
	CHECK(!uart_send(mcu_uart_fd, &cmd_send_format, sizeof(UART_FRAME_FORMAT_S)), -1, "Error uart_send!");
	return 0;
}



int uart_device_exit(int uart_fd) {
	CHECK(uart_fd > 0, -1, "Error s_uart_fd is %d", uart_fd);
	CHECK(!close(uart_fd), -1, "Error close with %d: %s", errno, strerror(errno));
	return 0;
}



int mcu_watchdog_disable(void) {
	int uart_fd = 0;
	char send_buf[MCU_UART_FRAME_DATA_LENTH] = {0};
	CHECK((uart_fd = uart_device_init(MCU_UART_DEVICE)) > 0, -1, "Error uart_device_init!");
	send_buf[0] = MCU_WATCHDOG_DISABLE;
	if(s_mcu_uart_send(uart_fd, MCU_COMM_CMD_WATCHDOG_ENABLE, send_buf)) {
		ERR("Error s_mcu_uart_send!");
		uart_device_exit(uart_fd);
		return -1;
	}
	uart_device_exit(uart_fd);
	return 0;
}



int main(int argc,char * argv[])
{
    if(mcu_watchdog_disable()) {
		printf("Error mcu_watchdog_disable!");
		return -1;
	}

    printf("关闭看门狗\n");	

    return 0;
}


