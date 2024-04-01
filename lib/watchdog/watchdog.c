/*
 * watchdog.c
 *
 *  Created on: Nov 26, 2021
 *      Author: zlf
 */

#include <unistd.h>
#include <stdlib.h>

#include "debug.h"
#include "gpio.h"
#include "mcu.h"

#define WATCHDOG_FEED_PIN	(32*1 + 8*3 + 0)	// GPIO1_D0, L26, 56

static int isWatchDogInited = 0;   //初始化了吗？

int* is_wdg_feeding_p = NULL;   //喂狗了吗？


extern void myjc_log_printf(const char *format,...);



int drvWatchDogEnable(void) {
	if(isWatchDogInited == 1)
		return EXIT_SUCCESS;

	if(mcu_watchdog_enable()) {
		ERR("Error mcu_watchdog_enable!");
		return EXIT_FAILURE;
	}
	CHECK(!gpio_init(WATCHDOG_FEED_PIN, true, GPIO_INPUT_EDGE_NONE),EXIT_FAILURE , "Error gpio_init!");
	isWatchDogInited = 1;

	myjc_log_printf("drvWatchDogEnable\n");
	//printf("drvWatchDogEnable\n");
	return EXIT_SUCCESS;
}

int drvWatchDogDisable(void) {
	if(isWatchDogInited == 0)
		return EXIT_SUCCESS;

	if(mcu_watchdog_disable()) {
		ERR("Error mcu_watchdog_disable!");
		myjc_log_printf("Error : drvWatchDogDisable failed\n");
		return EXIT_FAILURE;
	}
	gpio_exit(WATCHDOG_FEED_PIN);

	isWatchDogInited = 0;

	myjc_log_printf("SUCCESS : drvWatchDogDisable\n");
	//printf("SUCCESS : drvWatchDogDisable\n");
	return EXIT_SUCCESS;
}

void drvWatchDogFeeding(void) {
	if(isWatchDogInited == 0)
		return ;

	if(gpio_set_value(WATCHDOG_FEED_PIN, 1)) {
		ERR("Error gpio_set_value!");
		gpio_exit(WATCHDOG_FEED_PIN);
		myjc_log_printf("Error : drvWatchDogFeeding set hight failed\n");
		return;
	}
	//is_wdg_feeding = 1;

	if(is_wdg_feeding_p != NULL)
		(*is_wdg_feeding_p) ++;
	else{
		myjc_log_printf("drvWatchDogFeeding is_wdg_feeding_p == NULL \n");
		printf("drvWatchDogFeeding is_wdg_feeding_p == NULL \n");
	}

	sleep(1);
	if(gpio_set_value(WATCHDOG_FEED_PIN, 0)) {
		ERR("Error gpio_set_value!");
	}
	//printf("SUCCESS : drvWatchDogFeeding\n");
}

int drvWatchdogSetTimeout(int timeout) {
	if(isWatchDogInited == 0)
		return EXIT_FAILURE;
	
	CHECK(timeout > 0, EXIT_FAILURE, "Error timeout is %d", timeout);
	ERR("Error non-supported!");

	//myjc_log_printf("SUCCESS : drvWatchdogSetTimeout\n");
	return EXIT_FAILURE;
}

int drvWatchdogGetTimeout(void) {
	ERR("Error non-supported!");
	return EXIT_FAILURE;
}


