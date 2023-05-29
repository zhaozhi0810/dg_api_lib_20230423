/*
 * jc_keyboard.c
 *
 *  Created on: Dec 25, 2021
 *      Author: zlf
 */

#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include <asm/uaccess.h>
#include <linux/moduleparam.h>


#include "jc_keyboard.h"
#include "jc_keyboard_cmd.h"
#include "jc_keyuser.h"

#define JC_KEYBOARD_DRIVER_NAME		"jc_keyboard"
#define JC_KEYBOARD_IRQ_GPIO_NAME	"jc_keyboard_irq_gpio"
#define JC_KEYBOARD_IRQ_NAME		"jc_keyboard_irq"
#define JC_KEYBOARD_MISCDEV_MINOR	(20)
#define JC_KEYBOARD_I2C_TIMEOUT		(4000)	//4000 --> 1s  2022-09-06 dazhi修改，客户处调试暂时已通过。

#define NO_POLL_MODE   //不主动查询，需要单片机给出中断才行。
#define USE_SELF_WORK_QUEUE 

//static int jc_keyboard_inited = -1;  //是否加载成功，2023-01-09

#ifdef USE_SELF_WORK_QUEUE
//分配工作队列的指针
static struct workqueue_struct *jc_keyboard_wq;
#endif

static int debug_print = 0;   //默认不打印调试语句

static struct mutex iic_Mutex;      //互斥锁
static struct mutex iic_idel_Mutex;      //iic空闲互斥锁
static volatile unsigned long wait_respon_bits = 0; //需要等待什么应答，每一个位对应一种需求


module_param(debug_print,int, 0644);   //加载时传递参数

typedef struct {
	int irq_gpio;
	struct input_dev *input_dev;
	struct i2c_client *i2c_client;
	struct semaphore ioctl_sem;
} JC_KEYBOARD_INFO;

static char s_i2c_reply_ret = 0;
static struct miscdevice s_jc_keyboard_miscdevice;
static struct work_struct jc_keyboard_work;
static JC_KEYBOARD_INFO s_jc_keyboard_info;


//按键转换值的数组，定义在jc_keyuser.h
static const unsigned char s_user_key_value[] = {
		0,
		USER_KEY_VALUE_FUNCTION_1,   //0x1
		USER_KEY_VALUE_FUNCTION_2,   //0x2
		USER_KEY_VALUE_FUNCTION_3,   //0x3
		USER_KEY_VALUE_FUNCTION_4,   //0x4
		USER_KEY_VALUE_FUNCTION_5,   //0x5
		USER_KEY_VALUE_FUNCTION_6,   //0x6
		USER_KEY_VALUE_FUNCTION_7,   //0x7
		USER_KEY_VALUE_FUNCTION_8,   //0x8
		USER_KEY_VALUE_FUNCTION_9,   //0x9
		USER_KEY_VALUE_FUNCTION_10,   //0xa
		USER_KEY_VALUE_FUNCTION_11,   //0xb
		USER_KEY_VALUE_FUNCTION_12,   //0xc

		USER_KEY_VALUE_0,    //索引号13  -- 数字0
		USER_KEY_VALUE_1,    //索引号14  -- 数字1
		USER_KEY_VALUE_2,    //索引号15  -- 数字2
		USER_KEY_VALUE_3,    //索引号16  -- 数字3
		USER_KEY_VALUE_4,    //索引号17  -- 数字4
		USER_KEY_VALUE_5,    //索引号18  -- 数字5
		USER_KEY_VALUE_6,    //索引号19  -- 数字6
		USER_KEY_VALUE_7,    //索引号20  -- 数字7
		USER_KEY_VALUE_8,    //索引号21  -- 数字8
		USER_KEY_VALUE_9,    //索引号22  -- 数字9
		USER_KEY_VALUE_ASTERISK,    //索引号23  -- 
		USER_KEY_VALUE_POUND,    //索引号24  -- 

		USER_KEY_VALUE_TELL,    //索引号25
		USER_KEY_VALUE_VOLUME_INCREASE,    //索引号26
		USER_KEY_VALUE_VOLUME_DECREASE,    //索引号27

		USER_KEY_VALUE_SWITCH,
		USER_KEY_VALUE_INSIDE,
		USER_KEY_VALUE_OUTSIDE,

		USER_KEY_VALUE_UP,    //索引号31
		USER_KEY_VALUE_DOWN,    //索引号32
		USER_KEY_VALUE_PTT,    //索引号33
		USER_KEY_VALUE_LEFT,    //索引号34
		USER_KEY_VALUE_RIGHT,    //索引号35
		USER_KEY_VALUE_OK,    //索引号36

		USER_KEY_VALUE_KEEP,    //索引号37
		USER_KEY_VALUE_SET,    //索引号38
		USER_KEY_VALUE_TEST    //索引号39---> 0x27
};


//左侧（中括号中的数）是722自动自定义的键值 比如KC_L1 --> 1
//右侧是s_user_key_value中对应值得索引号，比如数字0，在数组中排13，就是0xd，
static const unsigned char s_user_map_led_value[] = {
		[1] = 0x01	, //图示1左1	     //KC_L1   1
		[3] = 0x02	, //图示2左2	     //KC_L2   3
		[5] = 0x03	, //图示3左3	     //KC_L3   5
		[7] = 0x04	, //图示4左4	     //KC_L4   7
		[9] = 0x05	, //图示5左5	     //KC_L5   9
		[44] = 0x06	, //图示6左6	     //KC_L6   44
		[2] = 0x07	, //图示7右1	     //KC_R1   2
		[4] = 0x08	, //图示8右2	     //KC_R2   4
		[6] = 0x09	, //图示9右3	     //KC_R3   6
		[8] = 0x0A	, //图示10右4     //KC_R4	8  
		[10] = 0x0B	, //图示11右5     //KC_R5	10
		[45] = 0x0C	, //图示12右6	//KC_R6    45
		[27] = 0x0D	, //0		//KC_NUM0   27
		[18] = 0x0E	, //1		//KC_NUM1   18
		[19] = 0x0F	, //2		//KC_NUM2   19	
		[20] = 0x10	, //3		//KC_NUM3   20	
		[21] = 0x11	, //4		//KC_NUM4   21	
		[22] = 0x12	, //5		//KC_NUM5   22	
		[23] = 0x13	, //6		//KC_NUM6   23	
		[24] = 0x14	, //7		//KC_NUM7   24	
		[25] = 0x15	, //8		//KC_NUM8   25	
		[26] = 0x16	, //9		//KC_NUM9   26	
		[28] = 0x17	, //*		//KC_DOT   28	
		[29] = 0x18	, //#		//KC_CLEAR   29	
		[13] = 0x19	, //电话（拨号）			//KC_TEL   13
		[35] = 0x1A	, //音量+		//KC_VOLUME_UP	   35
		[36] = 0x1B	, //音量-		//KC_VOLUME_DOWN	36	
		[14] = 0x27	, //TEST		//KC_TESKKEY   14   //原0x1c改0x27
		[11] = 0x1D	, //内通			//KC_INC      11
		[12] = 0x1E	, //外通			//KC_EXC      12 
		[30] = 0x1F	, //上			//KC_UP       30
		[31] = 0x20	, //下			//KC_DOWN     31
		[17] = 0x21	, //PTT		//KC_PTT        17	
		[32] = 0x22	, //左			//KC_LEFT   32
		[33] = 0x23	, //右			//KC_FIGHT  33
		[34] = 0x24	, //OK			//KC_OK     34
		[15] = 0x25	, //保持			//KC_HOLD   15
		[43] = 0x26	, //设置/摘挂机		//KC_RECOV  16改成43 原来[16] = 0x26	,
		[42] = 0x27	, //测试/复位		//KC_xxx	
		[37] = 0x28	, //指示灯-红		//LED_RED  37	
		[38] = 0x29	, //指示灯-绿		//LED_GREEN 38	
		[39] = 0x2A	, //指示灯-蓝		//KC_BLUE   39	
		[40] = 0x2B	 //全部键灯			//ALL_KEY_LED  40
};

static const struct of_device_id s_jc_keyboard_match_table[] = {
		{ .compatible = "jc,keyboard", },
		{},
};

static const struct i2c_device_id s_jc_keyboard_i2c_id[] = {
		{ JC_KEYBOARD_DRIVER_NAME, 0},
		{}
};

static void s_jc_keyboard_work_func_t(struct work_struct *work) {
	KEYBOARD_I2C_RECV_MSG_S keyboard_recv_msg;
	unsigned char i = 0;
	unsigned char cmd_verify_tmp = 0;
//	pr_err("2023debug s_jc_keyboard_work_func_t\n");
	memset(&keyboard_recv_msg, 0, sizeof(KEYBOARD_I2C_RECV_MSG_S));
	mutex_lock(&iic_Mutex);  //加锁
	if(i2c_master_recv(s_jc_keyboard_info.i2c_client, (char *)&keyboard_recv_msg, sizeof(KEYBOARD_I2C_RECV_MSG_S)) != sizeof(KEYBOARD_I2C_RECV_MSG_S)) {
		pr_err("Error i2c_master_recv, addr = %#x\n", s_jc_keyboard_info.i2c_client->addr);
		mutex_unlock(&iic_Mutex);  //开锁
		return;
	}
	mutex_unlock(&iic_Mutex);  //开锁

	if(debug_print)
		pr_err("i2c_master_recv: h0 = %#x, h1 = %#x type = %#x, key0=%#x, key1=%#x，key2=%#x,verify=%#x\n",
			keyboard_recv_msg.cmd_header0,
			keyboard_recv_msg.cmd_header1,
			keyboard_recv_msg.cmd_type,
			keyboard_recv_msg.cmd_key0,
			keyboard_recv_msg.cmd_key1,
			keyboard_recv_msg.cmd_key2,
			keyboard_recv_msg.cmd_verify);

	cmd_verify_tmp = FRAME_VERIFY(
			keyboard_recv_msg.cmd_header0,
			keyboard_recv_msg.cmd_header1,
			keyboard_recv_msg.cmd_type,
			keyboard_recv_msg.cmd_key0,
			keyboard_recv_msg.cmd_key1,
			keyboard_recv_msg.cmd_key2);
	if(keyboard_recv_msg.cmd_header0 != FRAME_HEADER_0 || keyboard_recv_msg.cmd_header1 != FRAME_HEADER_1) {
		pr_err("Error recv cmd header: %#x %#x\n", keyboard_recv_msg.cmd_header0, keyboard_recv_msg.cmd_header1);
		return;
	}
	if(keyboard_recv_msg.cmd_verify != cmd_verify_tmp) {
		if(debug_print)
			pr_err("Error verify %#x\n", keyboard_recv_msg.cmd_verify);
		return;
	}

	if((keyboard_recv_msg.cmd_type & 0xfc) == FRAME_CMD_TYPE_KEY_LED_FLASH)  //闪烁的指令
	{
		s_i2c_reply_ret = keyboard_recv_msg.cmd_key2;
		clear_bit(KEY_LED_FLASH_RESPONSE_BIT, &wait_respon_bits);   //置高某一位
		up(&s_jc_keyboard_info.ioctl_sem);
		return ;		
	}

//	pr_err("2023debug s_jc_keyboard_work_func_t cmd_type = %d,cmd_key2 = %d\n",keyboard_recv_msg.cmd_type,keyboard_recv_msg.cmd_key2);
	switch(keyboard_recv_msg.cmd_type) {
	case FRAME_CMD_TYPE_GET_KEY_VALUE_REPLY_PRESS:
	case FRAME_CMD_TYPE_GET_KEY_VALUE_REPLY_RELEASE:
		if(s_jc_keyboard_info.input_dev) {
		//	unsigned char key;
			unsigned char key_val[3] = {keyboard_recv_msg.cmd_key0,keyboard_recv_msg.cmd_key1,keyboard_recv_msg.cmd_key2};
			if(debug_print)
				pr_info("key1 = %d key2 = %d key3 = %d\n",key_val[0],key_val[1],key_val[2]);
			for(i=0;(i<3);i++)
			//for(i=0;(i<3) && key_val[i];i++)
			{
				//key = keyboard_recv_msg.cmd_key0? keyboard_recv_msg.cmd_key0:keyboard_recv_msg.cmd_key2;
				if(key_val[i] < KEY_VALUE_FUNCTION_1 || key_val[i] > KEY_VALUE_TEST) {
					continue;
					// pr_err("Error key %#x out of range!\n", key_val[i]);
					// break;
				}
				input_report_key(s_jc_keyboard_info.input_dev, s_user_key_value[key_val[i]], (keyboard_recv_msg.cmd_type == FRAME_CMD_TYPE_GET_KEY_VALUE_REPLY_PRESS)? 1:0);
			}
			input_sync(s_jc_keyboard_info.input_dev);
		}
		break;
	case FRAME_CMD_TYPE_GET_BRIGHTNESS:
		s_i2c_reply_ret = keyboard_recv_msg.cmd_key2;
		clear_bit(BRIGHTNESS_RESPONSE_BIT, &wait_respon_bits);   //清零某一位
		up(&s_jc_keyboard_info.ioctl_sem);
		break;
	case FRAME_CMD_TYPE_GET_PANEL_MODEL:
		s_i2c_reply_ret = keyboard_recv_msg.cmd_key2;
		clear_bit(PANEL_MODEL_RESPONSE_BIT, &wait_respon_bits);   //清零某一位
		up(&s_jc_keyboard_info.ioctl_sem);
		break;
	case FRAME_CMD_TYPE_GET_PANEL_VER:
		s_i2c_reply_ret = keyboard_recv_msg.cmd_key2;
		clear_bit(PANEL_MODEL_RESPONSE_BIT, &wait_respon_bits);   //清零某一位
		up(&s_jc_keyboard_info.ioctl_sem);
		break;
	case FRAME_CMD_TYPE_SET_BRIGHTNESS:
		s_i2c_reply_ret = keyboard_recv_msg.cmd_key2;
		clear_bit(PANEL_MODEL_RESPONSE_BIT, &wait_respon_bits);   //清零某一位
		up(&s_jc_keyboard_info.ioctl_sem);
		break;
	case FRAME_CMD_TYPE_RESET:
		s_i2c_reply_ret = keyboard_recv_msg.cmd_key2;
		clear_bit(RESET_RESPONSE_BIT, &wait_respon_bits);   //清零某一位
		up(&s_jc_keyboard_info.ioctl_sem);
		break;
	case FRAME_CMD_TYPE_KEY_LED_ON:
		s_i2c_reply_ret = keyboard_recv_msg.cmd_key2;
		clear_bit(KEY_LED_ON_RESPONSE_BIT, &wait_respon_bits);   //清零某一位
		up(&s_jc_keyboard_info.ioctl_sem);
		break;
	case FRAME_CMD_TYPE_KEY_LED_OFF:
		s_i2c_reply_ret = keyboard_recv_msg.cmd_key2;
		clear_bit(KEY_LED_OFF_RESPONSE_BIT, &wait_respon_bits);   //清零某一位
		up(&s_jc_keyboard_info.ioctl_sem);
		break;
	default:
		pr_err("Error non-supported cmd %#x\n", keyboard_recv_msg.cmd_type);
		break;
	}
}


#ifndef NO_POLL_MODE
//读取消息超时的处理
static int s_jc_keyboard_inttimeout_func_t(void) {
	KEYBOARD_I2C_RECV_MSG_S keyboard_recv_msg;
	unsigned char cmd_verify_tmp = 0;

	// if(jc_keyboard_inited != 1)  //驱动没有加载成功！！
	// 	return -1;
//	pr_err("2023debug s_jc_keyboard_inttimeout_func_t\n");
	memset(&keyboard_recv_msg, 0, sizeof(KEYBOARD_I2C_RECV_MSG_S));
	mutex_lock(&iic_Mutex);  //加锁
	if(i2c_master_recv(s_jc_keyboard_info.i2c_client, (char *)&keyboard_recv_msg, sizeof(KEYBOARD_I2C_RECV_MSG_S)) != sizeof(KEYBOARD_I2C_RECV_MSG_S)) {
		pr_err("Error i2c_master_recv, addr = %#x\n", s_jc_keyboard_info.i2c_client->addr);
		mutex_unlock(&iic_Mutex);  //开锁
		return -1;
	}
	mutex_unlock(&iic_Mutex);  //开锁

	cmd_verify_tmp = FRAME_VERIFY(
			keyboard_recv_msg.cmd_header0,
			keyboard_recv_msg.cmd_header1,
			keyboard_recv_msg.cmd_type,
			keyboard_recv_msg.cmd_key0,
			keyboard_recv_msg.cmd_key1,
			keyboard_recv_msg.cmd_key2);
	if(keyboard_recv_msg.cmd_header0 != FRAME_HEADER_0 || keyboard_recv_msg.cmd_header1 != FRAME_HEADER_1) {
		pr_err("Error recv cmd header: %#x %#x\n", keyboard_recv_msg.cmd_header0, keyboard_recv_msg.cmd_header1);
		return -1;
	}
	if(keyboard_recv_msg.cmd_verify != cmd_verify_tmp) {
		pr_err("Error verify %#x\n", keyboard_recv_msg.cmd_verify);
		return -1;
	}

	switch(keyboard_recv_msg.cmd_type) {
	case FRAME_CMD_TYPE_GET_KEY_VALUE_REPLY_PRESS:
	case FRAME_CMD_TYPE_GET_KEY_VALUE_REPLY_RELEASE:
		if(s_jc_keyboard_info.input_dev) {
			unsigned char key = keyboard_recv_msg.cmd_key0? keyboard_recv_msg.cmd_key0:keyboard_recv_msg.cmd_key2;
			if(key < KEY_VALUE_FUNCTION_1 || key > KEY_VALUE_TEST) {
				pr_err("Error key %#x out of range!\n", key);
				break;
			}
			input_report_key(s_jc_keyboard_info.input_dev, s_user_key_value[key], (keyboard_recv_msg.cmd_type == FRAME_CMD_TYPE_GET_KEY_VALUE_REPLY_PRESS)? 1:0);
			input_sync(s_jc_keyboard_info.input_dev);
		}
		break;
	case FRAME_CMD_TYPE_GET_BRIGHTNESS:
	case FRAME_CMD_TYPE_GET_PANEL_MODEL:
	case FRAME_CMD_TYPE_GET_PANEL_VER:
	case FRAME_CMD_TYPE_SET_BRIGHTNESS:
	case FRAME_CMD_TYPE_RESET:
	case FRAME_CMD_TYPE_KEY_LED_ON:
	case FRAME_CMD_TYPE_KEY_LED_OFF:
		s_i2c_reply_ret = keyboard_recv_msg.cmd_key2;
	//	up(&s_jc_keyboard_info.ioctl_sem);
		break;
	default:
		pr_err("Error non-supported cmd %#x\n", keyboard_recv_msg.cmd_type);
		return -1;
	}
	return 0;
}
#endif



static irqreturn_t s_jc_keyboard_i2c_isr(int irq, void *dev_id)
{
	if(irq != s_jc_keyboard_info.i2c_client->irq) {
		return IRQ_NONE;
	}
	if(debug_print)
		pr_err("enter jc_keyboard_i2c_isr\n");

#ifdef USE_SELF_WORK_QUEUE
	//将自己的工作和自己的工作队列进行管理，然后再登记
    if(!queue_work(jc_keyboard_wq, &jc_keyboard_work)) {
		pr_err("Error queue_work 2023!\n");
	}
#else
	if(!schedule_work(&jc_keyboard_work)) {
		pr_err("Error schedule_work!\n");
	}
#endif	
	return IRQ_HANDLED;
}

static int s_jc_keyboard_irq_init(void) {
	if(s_jc_keyboard_info.irq_gpio <= 0) {
		pr_err("Error irq_gpio is %d\n", s_jc_keyboard_info.irq_gpio);
		return -1;
	}
	if(s_jc_keyboard_info.i2c_client->irq <= 0) {
		pr_err("Error irq is %d\n", s_jc_keyboard_info.i2c_client->irq);
		return -1;
	}
	if(gpio_request(s_jc_keyboard_info.irq_gpio, JC_KEYBOARD_IRQ_GPIO_NAME)) {
		pr_err("Error gpio_request %s\n", JC_KEYBOARD_IRQ_GPIO_NAME);
		return -1;
	}
	if(gpio_direction_input(s_jc_keyboard_info.irq_gpio)) {
		pr_err("Error gpio_direction_input %d\n", s_jc_keyboard_info.irq_gpio);
		gpio_free(s_jc_keyboard_info.irq_gpio);
		return -1;
	}
	INIT_WORK(&jc_keyboard_work, s_jc_keyboard_work_func_t);

#ifdef USE_SELF_WORK_QUEUE
	//创建自己的工作队列和自己的内核线程
    jc_keyboard_wq = create_workqueue("myjc_keyboard_wq");
#endif
	if(request_irq(s_jc_keyboard_info.i2c_client->irq, s_jc_keyboard_i2c_isr, IRQF_TRIGGER_FALLING, JC_KEYBOARD_IRQ_NAME, NULL)) {
		pr_err("Error request_irq!\n");
		gpio_free(s_jc_keyboard_info.irq_gpio);
		return -1;
	}
	return 0;
}

static int s_jc_keyboard_irq_exit(void) {
	if(s_jc_keyboard_info.i2c_client->irq > 0) {
		free_irq(s_jc_keyboard_info.i2c_client->irq, NULL);
	}
	if(s_jc_keyboard_info.irq_gpio > 0) {
		gpio_free(s_jc_keyboard_info.irq_gpio);
	}

#ifdef USE_SELF_WORK_QUEUE
	//销毁自己的工作队列和内核线程
    destroy_workqueue(jc_keyboard_wq);
#endif
	return 0;
}

static int s_jc_keyboard_input_init(void) {
	int i = 0;
	if(s_jc_keyboard_info.input_dev) {
		pr_err("Error input_dev is not null!\n");
		return -1;
	}
	if(!(s_jc_keyboard_info.input_dev = input_allocate_device())) {
		pr_err("Error input_allocate_device!\n");
		return -1;
	}

	s_jc_keyboard_info.input_dev->name = JC_KEYBOARD_DRIVER_NAME;
	for(i = KEY_VALUE_FUNCTION_1; i <= KEY_VALUE_TEST; i ++) {
		input_set_capability(s_jc_keyboard_info.input_dev, EV_KEY, s_user_key_value[i]);
	}
	if(input_register_device(s_jc_keyboard_info.input_dev)) {
		pr_err("Error input_register_device!\n");
		input_free_device(s_jc_keyboard_info.input_dev);
		s_jc_keyboard_info.input_dev = NULL;
		return -1;
	}
	return 0;
}

static int s_jc_keyboard_input_exit(void) {
	if(!s_jc_keyboard_info.input_dev) {
		pr_err("Error input_dev is null!\n");
		return -1;
	}
	input_unregister_device(s_jc_keyboard_info.input_dev);
	input_free_device(s_jc_keyboard_info.input_dev);
	s_jc_keyboard_info.input_dev = NULL;
	return 0;
}

static long s_jc_keyboard_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long argv) {
	KEYBOARD_I2C_SEND_MSG_S keyboard_send_msg;
	if(debug_print)
		pr_err("enter jc_keyboard_unlocked_ioctl\n");
	// if(jc_keyboard_inited != 1)  //驱动没有加载成功！！
	// 	return -1;

	if(_IOC_TYPE(cmd) != KEYBOARD_IOC_MAGIC) {
		pr_err("Error cmd %d\n", cmd);
		return -1;
	}
	memset(&keyboard_send_msg, 0, sizeof(KEYBOARD_I2C_SEND_MSG_S));
	keyboard_send_msg.cmd_header0 = FRAME_HEADER_0;
	keyboard_send_msg.cmd_header1 = FRAME_HEADER_1;

	switch(cmd) {
	case KEYBOARD_IOC_GET_BRIGHTNESS:
		keyboard_send_msg.cmd_type = FRAME_CMD_TYPE_GET_BRIGHTNESS;
		set_bit(BRIGHTNESS_RESPONSE_BIT, &wait_respon_bits);   //置高某一位
		break;
	case KEYBOARD_IOC_SET_BRIGHTNESS:
		if(!argv || copy_from_user(&keyboard_send_msg.cmd, (void *)argv, 1)) {
			pr_err("Error copy_from_user!\n");
			return -2;
		}
		if(keyboard_send_msg.cmd < FRAME_CMD_TYPE_SET_BRIGHTNESS_MIN || keyboard_send_msg.cmd > FRAME_CMD_TYPE_SET_BRIGHTNESS_MAX) {
			pr_err("Error brightness out of range!\n");
			return -3;
		}
		keyboard_send_msg.cmd_type = FRAME_CMD_TYPE_SET_BRIGHTNESS;
		set_bit(SET_BRIGHTNESS_RESPONSE_BIT, &wait_respon_bits);   //置高某一位
		break;
	case KEYBOARD_IOC_GET_PANEL_MODEL:
		keyboard_send_msg.cmd_type = FRAME_CMD_TYPE_GET_PANEL_MODEL;
		set_bit(PANEL_MODEL_RESPONSE_BIT, &wait_respon_bits);   //置高某一位
		break;
	case KEYBOARD_IOC_GET_PANEL_VER:
		keyboard_send_msg.cmd_type = FRAME_CMD_TYPE_GET_PANEL_VER;
		set_bit(PANEL_VER_RESPONSE_BIT, &wait_respon_bits);   //置高某一位
		break;
	case KEYBOARD_IOC_RESET:
		keyboard_send_msg.cmd_type = FRAME_CMD_TYPE_RESET;
		set_bit(RESET_RESPONSE_BIT, &wait_respon_bits);   //置高某一位
		break;
	case KEYBOARD_IOC_KEY_LED_FLASH:   //2023-04-28
		{
			unsigned char user_val = 0;
			unsigned char flashtype = 0;

			if(!argv || copy_from_user(&user_val, (void *)argv, 1)) {
				pr_err("Error KEYBOARD_IOC_KEY_LED_FLASH copy_from_user!\n");
				return -4;
			}
			keyboard_send_msg.cmd = s_user_map_led_value[user_val & 0x3f];//(user_val & 0x3f);	//闪烁哪个灯
			flashtype = ((user_val >> 6) & 0x3);   //闪烁类型
			keyboard_send_msg.cmd_type = FRAME_CMD_TYPE_KEY_LED_FLASH | flashtype;  //发过去的命令是0x80，81，82，83
		}
		set_bit(KEY_LED_FLASH_RESPONSE_BIT, &wait_respon_bits);   //置高某一位
		break;
	case KEYBOARD_IOC_KEY_LED_ON:
	case KEYBOARD_IOC_KEY_LED_OFF: {
	//	int i = KEY_VALUE_FUNCTION_1;
		unsigned char user_key_code = 0;
		if(!argv || copy_from_user(&user_key_code, (void *)argv, 1)) {
			pr_err("Error copy_from_user!\n");
			return -5;
		}
		// for(; i < KEY_VALUE_TEST; i ++) {
		// 	if(s_user_key_value[i] == user_key_code) {
		// 		keyboard_send_msg.cmd = i;
		// 		break;
		// 	}
		// }
		if(user_key_code < sizeof(s_user_map_led_value)/sizeof(s_user_map_led_value[0]))
			keyboard_send_msg.cmd = s_user_map_led_value[user_key_code];
		else
			keyboard_send_msg.cmd = 0;  //20220905

		//2023-01-05  不能识别的按键
		if(keyboard_send_msg.cmd == 0){
			pr_err("Error keyboard_send_msg.cmd == 0! user_key_code = %d\n",user_key_code);
			return -6;
		}

		// if(i >= KEY_VALUE_TEST) {
		// 	if(user_key_code >= KEY_RED_LED && user_key_code <= ALL_KEY_VALUE) {  /*lsr modify 20220512 (KEY_BLUE_LED)*/
		// 		keyboard_send_msg.cmd = user_key_code;
		// 	}
		// 	else {
		// 		pr_err("Error key code %#x out of range!\n", keyboard_send_msg.cmd);
		// 		return -1;
		// 	}
		// }
		if(cmd == KEYBOARD_IOC_KEY_LED_ON) {
			keyboard_send_msg.cmd_type = FRAME_CMD_TYPE_KEY_LED_ON;
			set_bit(KEY_LED_ON_RESPONSE_BIT, &wait_respon_bits);   //置高某一位
		}
		else {
			keyboard_send_msg.cmd_type = FRAME_CMD_TYPE_KEY_LED_OFF;
			set_bit(KEY_LED_OFF_RESPONSE_BIT, &wait_respon_bits);   //置高某一位
		}
		break;
	}
	default:
		pr_err("Error non-supported cmd %d\n", cmd);
		return -7;
	}
	keyboard_send_msg.cmd_verify = FRAME_VERIFY(
			keyboard_send_msg.cmd_header0,
			keyboard_send_msg.cmd_header1,
			keyboard_send_msg.cmd_type,
			keyboard_send_msg.cmd,
			0,
			0);
#if 1
	if(debug_print)
		pr_err("i2c_master_send: h0 = %#x, h1 = %#x type = %#x, %#x, %#x\n",
			keyboard_send_msg.cmd_header0,
			keyboard_send_msg.cmd_header1,
			keyboard_send_msg.cmd_type,
			keyboard_send_msg.cmd,
			keyboard_send_msg.cmd_verify);
#endif
	mutex_lock(&iic_idel_Mutex);   //不让发
	mutex_lock(&iic_Mutex);  //加锁
	if(i2c_master_send(s_jc_keyboard_info.i2c_client, (char *)&keyboard_send_msg, sizeof(KEYBOARD_I2C_SEND_MSG_S)) != sizeof(KEYBOARD_I2C_SEND_MSG_S)) {
		pr_err("Error i2c_master_send!\n");
		mutex_unlock(&iic_Mutex);  //开锁
		mutex_unlock(&iic_idel_Mutex);  //开锁
		return -8;
	}
	mutex_unlock(&iic_Mutex);//开锁
	if(down_timeout(&s_jc_keyboard_info.ioctl_sem, JC_KEYBOARD_I2C_TIMEOUT/4)) {
		//没有中断的情况会超时
#ifndef NO_POLL_MODE
		if(s_jc_keyboard_inttimeout_func_t())	
#endif
		{
			pr_err("Error down_timeout!\n");
			mutex_unlock(&iic_idel_Mutex);  //开锁
			return -9;
		}
	}
	mutex_unlock(&iic_idel_Mutex);  //开锁



	switch(cmd) {
	case KEYBOARD_IOC_GET_BRIGHTNESS:
		if(test_bit(SET_BRIGHTNESS_RESPONSE_BIT, &wait_respon_bits))
		{
			pr_err("Error KEYBOARD_IOC_GET_BRIGHTNESS down_timeout! ----test_bit\n");
			clear_bit(SET_BRIGHTNESS_RESPONSE_BIT, &wait_respon_bits);
			return -20;
		}
		if(!argv || copy_to_user((void *)argv, &s_i2c_reply_ret, 1)) {
			pr_err("Error copy_to_user!\n");
			return -10;
		}
		break;
	case KEYBOARD_IOC_GET_PANEL_MODEL:
		if(test_bit(PANEL_MODEL_RESPONSE_BIT, &wait_respon_bits))
		{
			pr_err("Error KEYBOARD_IOC_GET_PANEL_MODEL down_timeout! ----test_bit\n");
			clear_bit(PANEL_MODEL_RESPONSE_BIT, &wait_respon_bits);
			return -21;
		}
		if(!argv || copy_to_user((void *)argv, &s_i2c_reply_ret, 1)) {
			pr_err("Error copy_to_user!\n");
			return -10;
		}
		break;
	case KEYBOARD_IOC_GET_PANEL_VER:
		if(test_bit(PANEL_VER_RESPONSE_BIT, &wait_respon_bits))
		{
			pr_err("Error KEYBOARD_IOC_GET_PANEL_VER down_timeout! ----test_bit\n");
			clear_bit(PANEL_VER_RESPONSE_BIT, &wait_respon_bits);
			return -22;
		}
		if(!argv || copy_to_user((void *)argv, &s_i2c_reply_ret, 1)) {
			pr_err("Error copy_to_user!\n");
			return -10;
		}
		break;
	case KEYBOARD_IOC_SET_BRIGHTNESS:
		if(test_bit(SET_BRIGHTNESS_RESPONSE_BIT, &wait_respon_bits))
		{
			pr_err("Error KEYBOARD_IOC_SET_BRIGHTNESS down_timeout! ----test_bit\n");
			clear_bit(SET_BRIGHTNESS_RESPONSE_BIT, &wait_respon_bits);
			return -23;
		}
		if(s_i2c_reply_ret == FRAME_CMD_REPLY_FAILED) {
			pr_err("Error execute failed!\n");
			return -11;
		}
		break;
	case KEYBOARD_IOC_RESET:
		if(test_bit(RESET_RESPONSE_BIT, &wait_respon_bits))
		{
			pr_err("Error KEYBOARD_IOC_RESET down_timeout! ----test_bit\n");
			clear_bit(RESET_RESPONSE_BIT, &wait_respon_bits);
			return -24;
		}
		if(s_i2c_reply_ret == FRAME_CMD_REPLY_FAILED) {
			pr_err("Error execute failed!\n");
			return -11;
		}
		break;	
	case KEYBOARD_IOC_KEY_LED_ON:
		if(test_bit(KEY_LED_ON_RESPONSE_BIT, &wait_respon_bits))
		{
			pr_err("Error KEYBOARD_IOC_KEY_LED_ON down_timeout! ----test_bit\n");
			clear_bit(KEY_LED_ON_RESPONSE_BIT, &wait_respon_bits);
			return -25;
		}
		if(s_i2c_reply_ret == FRAME_CMD_REPLY_FAILED) {
			pr_err("Error execute failed!\n");
			return -11;
		}
		break;
	case KEYBOARD_IOC_KEY_LED_OFF:   //灯的控制部分，不再等待应答，2022-09-05		
		if(test_bit(KEY_LED_OFF_RESPONSE_BIT, &wait_respon_bits))
		{
			pr_err("Error KEYBOARD_IOC_KEY_LED_OFF down_timeout! ----test_bit\n");
			clear_bit(KEY_LED_OFF_RESPONSE_BIT, &wait_respon_bits);
			return -26;
		}//return 0;
		if(s_i2c_reply_ret == FRAME_CMD_REPLY_FAILED) {
			pr_err("Error execute failed!\n");
			return -11;
		}
		break;
	default:
		if((cmd & 0xfc) == KEYBOARD_IOC_KEY_LED_FLASH)  //闪烁的指令
		{
			if(test_bit(KEY_LED_FLASH_RESPONSE_BIT, &wait_respon_bits))
			{
				pr_err("Error KEYBOARD_IOC_KEY_LED_FLASH down_timeout! ----test_bit\n");
				clear_bit(KEY_LED_FLASH_RESPONSE_BIT, &wait_respon_bits);
				return -27;
			}
			return 0;		
		}
		break;
	}
	return 0;
}

static struct file_operations s_jc_keyboard_fops = {
		.unlocked_ioctl = s_jc_keyboard_unlocked_ioctl,
};

static int s_jc_keyboard_misc_init(void) {
	memset(&s_jc_keyboard_miscdevice, 0, sizeof(struct miscdevice));
	s_jc_keyboard_miscdevice.minor = JC_KEYBOARD_MISCDEV_MINOR;
	s_jc_keyboard_miscdevice.name = JC_KEYBOARD_DRIVER_NAME;
	s_jc_keyboard_miscdevice.fops = &s_jc_keyboard_fops;
	if(misc_register(&s_jc_keyboard_miscdevice)) {
		pr_err("Error misc_register!\n");
		return -1;
	}
	sema_init(&s_jc_keyboard_info.ioctl_sem, 0);
	return 0;
}

static int s_jc_keyboard_misc_exit(void) {

	misc_deregister(&s_jc_keyboard_miscdevice);
	return 0;
}


static int jc_kayboard_command_for_read(struct i2c_client *client)
{
	int ret = -1;
	uint8_t tmpbuf[10] = {0};
	struct i2c_msg msg[1];

	msg[0].addr = client->addr;
	msg[0].flags = I2C_M_RD;/*Read*/
	msg[0].len = 7;
	msg[0].buf = tmpbuf;
	ret = i2c_transfer(client->adapter, msg, 1);
	return ret;
}


//static void s_jc_keyboard_exit(void);

static int s_jc_keyboard_probe(struct i2c_client *i2c_client, const struct i2c_device_id *i2c_device_id) {
	
	int ret;
//	jc_keyboard_inited = -1;
	if(debug_print)
		pr_err("addr = %#x\n", i2c_client->addr);

	ret = jc_kayboard_command_for_read(i2c_client);  //尝试着读一次
	if(ret < 0)  //读不了直接返回
	{
		pr_err("error : jc_keyboard_command_for_read,no probe\n");
//		s_jc_keyboard_exit();
		return ret;
	}


	memset(&s_jc_keyboard_info, 0, sizeof(JC_KEYBOARD_INFO));
	if((s_jc_keyboard_info.irq_gpio = of_get_named_gpio(i2c_client->dev.of_node, "irq-gpio", 0)) < 0) {
		pr_err("Error of_get_named_gpio %s\n", "irq-gpio");
		return -1;
	}
	if((i2c_client->irq = gpio_to_irq(s_jc_keyboard_info.irq_gpio)) < 0) {
		pr_err("Error gpio_to_irq %d\n", s_jc_keyboard_info.irq_gpio);
		return -1;
	}
	s_jc_keyboard_info.i2c_client = i2c_client;

	if(s_jc_keyboard_input_init()) {
		pr_err("Error s_jc_keyboard_input_init!\n");
		return -1;
	}
	if(s_jc_keyboard_misc_init()) {
		pr_err("Error s_jc_keyboard_misc_init!\n");
		s_jc_keyboard_input_exit();
		return -1;
	}
	if(s_jc_keyboard_irq_init()) {
		pr_err("Error s_jc_keyboard_irq_init!\n");
		s_jc_keyboard_misc_exit();
		s_jc_keyboard_input_exit();
		return -1;
	}
	
	 mutex_init(&iic_Mutex);  //初始化互斥锁
	 mutex_init(&iic_idel_Mutex);
//	jc_keyboard_inited = 1;  //加载成功！！
	return 0;
}

static int s_jc_keyboard_remove(struct i2c_client *i2c_client) {

	// if(jc_keyboard_inited != 1)  //驱动没有加载成功！！
	// 	return -1;

	if(s_jc_keyboard_irq_exit()) {
		pr_err("Error s_jc_keyboard_irq_exit!\n");
	}
	if(s_jc_keyboard_misc_exit()) {
		pr_err("Error s_jc_keyboard_misc_exit!\n");
	}
	if(s_jc_keyboard_input_exit()) {
		pr_err("Error s_jc_keyboard_input_exit!\n");
	}
	return 0;
}

static struct i2c_driver s_i2c_driver_jc_keyboard = {
		.probe = s_jc_keyboard_probe,
		.remove = s_jc_keyboard_remove,
		.driver = {
				.name = JC_KEYBOARD_DRIVER_NAME,
				.owner = THIS_MODULE,
				.of_match_table = s_jc_keyboard_match_table,
		},
		.id_table = s_jc_keyboard_i2c_id,
};

// static int s_jc_keyboard_init(void) {
// 	if(i2c_add_driver(&s_i2c_driver_jc_keyboard)) {
// 		pr_err("Error s_jc_keyboard_init i2c_add_driver!\n");
// 		return -1;
// 	}
// 	return 0;
// }

// static void s_jc_keyboard_exit(void) {
// 	return;
// //	i2c_del_driver(&s_i2c_driver_jc_keyboard);
// }


module_i2c_driver(s_i2c_driver_jc_keyboard);     //2023-03-30  增加 

// module_init(s_jc_keyboard_init);
// module_exit(s_jc_keyboard_exit);
// date 需要修改内核的makefile ，注释 886 KBUILD_CFLAGS   += $(call cc-option,-Werror=date-time)
MODULE_DESCRIPTION("Buildtime :"__DATE__" "__TIME__);
MODULE_AUTHOR("dazhi@jc,keyboard,2023-05");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.1.0");    //2023-05-10 版本1.1.0
