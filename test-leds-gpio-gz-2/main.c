/*
* @Author: dazhi
* @Date:   2022-10-26 17:13:57
* @Last Modified by:   dazhi
* @Last Modified time: 2023-07-17 09:59:41
*/


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "gpio_export.h"
#include <time.h>

#define PINS_TOTAL_NUM  20
static int leds_pin[PINS_TOTAL_NUM];


static void init_leds_pins_array(void)
{

	leds_pin[0] = get_pin(RK_GPIO1,RK_PC2);
	leds_pin[1] = get_pin(RK_GPIO4,RK_PD6);
	leds_pin[2] = get_pin(RK_GPIO4,RK_PD1);
	leds_pin[3] = get_pin(RK_GPIO4,RK_PD2);
	leds_pin[4] = get_pin(RK_GPIO2,RK_PB3);

	leds_pin[5] = get_pin(RK_GPIO1,RK_PA3);
	leds_pin[6] = get_pin(RK_GPIO1,RK_PD0);
	leds_pin[7] = get_pin(RK_GPIO4,RK_PC6);
	leds_pin[8] = get_pin(RK_GPIO2,RK_PD2);

	leds_pin[9] = get_pin(RK_GPIO4,RK_PC7);
	leds_pin[10] = get_pin(RK_GPIO2,RK_PB2);
	leds_pin[11] = get_pin(RK_GPIO2,RK_PA5);
	leds_pin[12] = get_pin(RK_GPIO2,RK_PA0);
	leds_pin[13] = get_pin(RK_GPIO2,RK_PA2);

	leds_pin[14] = get_pin(RK_GPIO2,RK_PB0);
	leds_pin[15] = get_pin(RK_GPIO2,RK_PA4);
	leds_pin[16] = get_pin(RK_GPIO2,RK_PA6);
	leds_pin[17] = get_pin(RK_GPIO2,RK_PA1);
	leds_pin[18] = get_pin(RK_GPIO2,RK_PB4);
	leds_pin[19] = get_pin(RK_GPIO2,RK_PA7);
}



int main(int argc,char* argv[])
{
	int i;
	time_t t,t0 = 0;
	struct tm * mytm;
	//引脚数组初始化
	init_leds_pins_array();

	//导出引脚
	for(i=0;i<PINS_TOTAL_NUM;i++)
		printf("pins_num = %d\n",leds_pin[i]);

	//设置输出
	for(i=0;i<PINS_TOTAL_NUM;i++)
	{
		if(false == gpio_direction_set(leds_pin[i], GPIO_DIR_OUT))
			leds_pin[i] = -1;
	//	gpio_pull_enable(leds_pin[i], true);  //设置上拉
	}	
	//t0 = time(NULL);  //初始时间
	while(1)
	{
		
		for(i=0;i<PINS_TOTAL_NUM;i++)
		{
			// t = time(NULL);
			// if(t - t0 > 1)
			// {
			// 	mytm = localtime(&t);
			// 	printf("i = %d,time: %d:%d:%d\n",i,mytm->tm_hour,mytm->tm_min,mytm->tm_sec);
			// }
			// t0 = t;
			if(leds_pin[i] > 0)
			{				
				gpio_level_set(leds_pin[i], GPIO_LEVEL_HIGH);
				usleep(10000);
			}
		}

		for(i=0;i<PINS_TOTAL_NUM;i++)
		{
			// t = time(NULL);
			// if(t - t0 > 1)
			// {
			// 	mytm = localtime(&t);
			// 	printf("i = %d,time: %d:%d:%d\n",i,mytm->tm_hour,mytm->tm_min,mytm->tm_sec);
			// }
			// t0 = t;
			if(leds_pin[i] > 0)
			{	
				gpio_level_set(leds_pin[i], GPIO_LEVEL_LOW);
				usleep(10000);
			}
		//	printf("--i = %d\n",i);
		}			
	}


	// for(i=1;i<PINS_TOTAL_NUM;i++)
	// 	gpio_direction_unset(leds_pin[i]);

	return 0;
}

