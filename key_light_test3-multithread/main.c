/*
* @Author: dazhi
* @Date:   2023-02-28 15:49:15
* @Last Modified by:   dazhi
* @Last Modified time: 2023-05-29 14:54:40
*/


#include "drv722.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#if 0
//左侧（中括号中的数）是722自动自定义的键值 比如KC_L1 --> 1
//右侧是s_user_key_value中对应值得索引号，比如数字0，在数组中排13，就是0xd，
//与驱动中正好是反的，目的就是在drv722test中的点亮灯的序号正好是文档表格中的数字
//2023-02-22，by zhaozhi
static const unsigned char s_user_map_led_value[] = {
	[0x01] = 1	, //图示1左1 	 //KC_L1   1
	[0x02] = 3	, //图示2左2 	 //KC_L2   3
	[0x03] = 5	, //图示3左3 	 //KC_L3   5
	[0x04] = 7	, //图示4左4 	 //KC_L4   7
	[0x05] = 9	, //图示5左5 	 //KC_L5   9
	[0x06] = 44 , //图示6左6 	 //KC_L6   44
	[0x07] = 2	, //图示7右1 	 //KC_R1   2
	[0x08] = 4	, //图示8右2 	 //KC_R2   4
	[0x09] = 6	, //图示9右3 	 //KC_R3   6
	[0x0A] = 8	, //图示10右4	 //KC_R4	8  
	[0x0B] = 10 , //图示11右5	 //KC_R5	10
	[0x0C] = 45 , //图示12右6	//KC_R6    45
	[0x0D] = 27 , //0		//KC_NUM0	27
	[0x0E] = 18 , //1		//KC_NUM1	18
	[0x0F] = 19 , //2		//KC_NUM2	19	
	[0x10] = 20 , //3		//KC_NUM3	20	
	[0x11] = 21 , //4		//KC_NUM4	21	
	[0x12] = 22 , //5		//KC_NUM5	22	
	[0x13] = 23 , //6		//KC_NUM6	23	
	[0x14] = 24 , //7		//KC_NUM7	24	
	[0x15] = 25 , //8		//KC_NUM8	25	
	[0x16] = 26 , //9		//KC_NUM9	26	
	[0x17] = 28 , //*		//KC_DOT   28	
	[0x18] = 29 , //#		//KC_CLEAR	 29 
	[0x19] = 13 , //电话（拨号）			//KC_TEL   13
	[0x1A] = 35 , //音量+ 	//KC_VOLUME_UP	   35
	[0x1B] = 36 , //音量- 	//KC_VOLUME_DOWN	36	
	[0x27] = 14 , //TEST		//KC_TESKKEY   14	//原0x1c改0x27
	[0x1D] = 11 , //内通			//KC_INC	  11
	[0x1E] = 12 , //外通			//KC_EXC	  12 
	[0x1F] = 30 , //上 		//KC_UP 	  30
	[0x20] = 31 , //下 		//KC_DOWN	  31
	[0x21] = 17 , //PTT 	//KC_PTT		17	
	[0x22] = 32 , //左 		//KC_LEFT	32
	[0x23] = 33 , //右 		//KC_FIGHT	33
	[0x24] = 34 , //OK			//KC_OK 	34
	[0x25] = 15 , //保持			//KC_HOLD	15
	[0x26] = 16 , //设置/摘挂机			//KC_RECOV	16
	[0x27] = 42 , //测试/复位		//KC_xxx	
	[0x28] = 37 , //指示灯-红		//LED_RED  37	
	[0x29] = 38 , //指示灯-绿		//LED_GREEN 38	
	[0x2A] = 39 , //指示灯-蓝		//KC_BLUE	39	
	[0x2B] = 40  //全部键灯 		//ALL_KEY_LED  40
};
#endif

static int delay_time_ms1,delay_time_ms2;





static void *flash_keylight_thread1(void *param) {

//	unsigned int KeyIndex = 0;
//	unsigned int delaytime = 100;   //100表示100ms

	// if(param)
	// 	KeyIndex = *(int*)param;

	if(delay_time_ms1 < 10)
		delay_time_ms1 = 10;

	printf("thread1 light = 13 delaytime = %d ms\n",delay_time_ms1);

	while(1) {
		drvDimLED(13);
		usleep(delay_time_ms1*1000);
		drvLightLED(13);
		usleep(delay_time_ms1*1000);
		printf("thread1111111111\n");
	}

	return NULL;
}


static void *flash_keylight_thread2(void *param) {

//	unsigned int KeyIndex = 0;
//	unsigned int delaytime = 100;   //100表示100ms

	// if(param)
	// 	KeyIndex = *(int*)param;

	if(delay_time_ms2 < 10)
		delay_time_ms2 = 10;

	printf("thread light = 14 delaytime = %d ms\n",delay_time_ms2);

	while(1) {
		drvDimLED(14);
		usleep(delay_time_ms2*1000);
		drvLightLED(14);
		usleep(delay_time_ms2*1000);
		printf("thread2222222222\n");
	}

	return NULL;
}


static void s_gpio_notify_func(int gpio, int val) {
	printf("gpio Key=%#x, value=%#x", gpio, val);
}

static void s_panel_key_notify_func(int gpio, int val) {
	printf("panel Key %#x, value %#x", gpio, val);
}




int main(int argc,char* argv[])
{
	int delay_time_ms = 0;

//	int KeyIndex;
	pthread_t pthread_id1,pthread_id2;


	printf("Usage : %s [time_ms] [time1_ms] [time2_ms] \n",argv[0]);
	drvCoreBoardInit();
	drvSetLedBrt(100);

	drvSetGpioCbk(s_gpio_notify_func);
	drvSetGpioKeyCbk(s_panel_key_notify_func);

	if(argc == 1)  //如果没有参数的话
	{
		delay_time_ms = 100;   //100ms	
		delay_time_ms1 = 150;   //100ms
		delay_time_ms2 = 200;   //100ms
	}
	else 
	{
		if(argc >= 2)
		{
			delay_time_ms = atoi(argv[1]);

			if(delay_time_ms < 100)   //最小为100ms
				delay_time_ms = 100;
		}
		if(argc >= 3)
		{
			delay_time_ms1 = atoi(argv[2]);

			if(delay_time_ms1 < 10)   //最小为100ms
				delay_time_ms1 = 10;
		}
		if(argc >= 4)
		{
			delay_time_ms2 = atoi(argv[3]);

			if(delay_time_ms2 < 10)   //最小为100ms
				delay_time_ms2 = 10;
		}	
	}


	printf("main延时时间 =  %d ms\n",delay_time_ms/1000);
	printf("thread1延时时间 =  %d ms\n",delay_time_ms1/1000);
	printf("thread2延时时间 =  %d ms\n",delay_time_ms2/1000);

	pthread_create(&pthread_id1, NULL, flash_keylight_thread1, NULL);
	pthread_create(&pthread_id2, NULL, flash_keylight_thread2, NULL);
	pthread_detach(pthread_id1);
	pthread_detach(pthread_id2);
//	pthread_create(&watchdog_feed_thread_id, NULL, s_watchdog_feed_thread, NULL)
	while(1)
	{	
		drvDimLED(38);
		usleep(delay_time_ms*1000);
		drvLightLED(38);
		usleep(delay_time_ms*1000);
		printf("main thread\n");
	}
}




