/*
* @Author: dazhi
* @Date:   2022-10-26 17:13:57
* @Last Modified by:   dazhi
* @Last Modified time: 2023-05-24 09:19:33
*/


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "gpio_export.h"
#include <time.h>

#define PINS_TOTAL_NUM  160    //总共160个引脚，全部遍历一次



int main(int argc,char* argv[])
{
	int i;
	time_t t,t0 = 0;
	struct tm * mytm;
	int pin;   //临时存储pin的号码
	char group[] = {'A','B','C','D'};

	printf("enter main\n");

	//设置输出
	for(i=0;i<PINS_TOTAL_NUM;i++)
	{
		printf("(gpio%d-%c%d) ready to set out \n",i/32,group[i%32/8],i%32%8);
		if(i == 37 || i == 38 || i==147 || i== 148)
		{
			printf("ignore %d\n",i);  //GPIO4-c3,c4 是调试串口  GPIO1-A5（高电平控制rk808sleep）,A6(高电平导致复位)会重启
			continue;
		}	

		if(false == gpio_direction_set(i, GPIO_DIR_OUT))
		{
			printf("error:pin %d(gpio%d-%c%d) set out failed\n",i,i/32,group[i%32/8],i%32%8);
			continue;
		}	//leds_pin[i] = -1;

		//输出高
		gpio_level_set(i, GPIO_LEVEL_HIGH);

		//
		//sleep(1);
		//
		//
		

	}	

	system("cat /sys/kernel/debug/gpio");


	return 0;
}

