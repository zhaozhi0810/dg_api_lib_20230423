/*
 * jc_keyboard.h
 *
 *  Created on: Jan 4, 2022
 *      Author: zlf
 */

#ifndef JC_KEYBOARD_H_
#define JC_KEYBOARD_H_

#define FRAME_HEADER_0	(0x55)
#define FRAME_HEADER_1	(0xAA)

#define FRAME_CMD_TYPE_GET_KEY_VALUE	(0x00)
#define FRAME_CMD_TYPE_GET_BRIGHTNESS	(0x10)
#define FRAME_CMD_TYPE_SET_BRIGHTNESS	(0x20)
#define FRAME_CMD_TYPE_GET_PANEL_MODEL	(0x30)
#define FRAME_CMD_TYPE_GET_PANEL_VER	(0x40)
#define FRAME_CMD_TYPE_RESET			(0x50)
#define FRAME_CMD_TYPE_KEY_LED_ON		(0x60)
#define FRAME_CMD_TYPE_KEY_LED_OFF		(0x70)

#define FRAME_CMD_TYPE_GET_KEY_VALUE_REPLY_PRESS	(0x01)
#define FRAME_CMD_TYPE_GET_KEY_VALUE_REPLY_RELEASE	(0x02)

#define FRAME_CMD_REPLY_SUCCESS	(0x5A)
#define FRAME_CMD_REPLY_FAILED	(0xA5)

#define FRAME_CMD_TYPE_SET_BRIGHTNESS_MIN	(0)
#define FRAME_CMD_TYPE_SET_BRIGHTNESS_MAX	(100)

/* Keyboard key code */
#define KEY_VALUE_FUNCTION_1	(0x01)
#define KEY_VALUE_FUNCTION_2	(0x02)
#define KEY_VALUE_FUNCTION_3	(0x03)
#define KEY_VALUE_FUNCTION_4	(0x04)
#define KEY_VALUE_FUNCTION_5	(0x05)
#define KEY_VALUE_FUNCTION_6	(0x06)
#define KEY_VALUE_FUNCTION_7	(0x07)
#define KEY_VALUE_FUNCTION_8	(0x08)
#define KEY_VALUE_FUNCTION_9	(0x09)
#define KEY_VALUE_FUNCTION_10	(0x0A)
#define KEY_VALUE_FUNCTION_11	(0x0B)
#define KEY_VALUE_FUNCTION_12	(0x0C)

#define KEY_VALUE_0			(0x0D)
#define KEY_VALUE_1			(0x0E)
#define KEY_VALUE_2			(0x0F)
#define KEY_VALUE_3			(0x10)
#define KEY_VALUE_4			(0x11)
#define KEY_VALUE_5			(0x12)
#define KEY_VALUE_6			(0x13)
#define KEY_VALUE_7			(0x14)
#define KEY_VALUE_8			(0x15)
#define KEY_VALUE_9			(0x16)
#define KEY_VALUE_ASTERISK	(0x17)
#define KEY_VALUE_POUND		(0x18)

#define KEY_VALUE_TELL				(0x19)
#define KEY_VALUE_VOLUME_INCREASE	(0x1A)
#define KEY_VALUE_VOLUME_DECREASE	(0x1B)

#define KEY_VALUE_SWITCH	(0x1C)
#define KEY_VALUE_INSIDE	(0x1D)
#define KEY_VALUE_OUTSIDE	(0x1E)

#define KEY_VALUE_UP	(0x1F)
#define KEY_VALUE_DOWN	(0x20)
#define KEY_VALUE_PTT	(0x21)
#define KEY_VALUE_LEFT	(0x22)
#define KEY_VALUE_RIGHT	(0x23)
#define KEY_VALUE_OK	(0x24)

/* Multi-keyboard only */
#define KEY_VALUE_KEEP	(0x25)
#define KEY_VALUE_SET	(0x26)
#define KEY_VALUE_TEST	(0x27)
#define ALL_KEY_VALUE	(0x2B)  /*lsr add 20220512*/
/* Multi-keyboard only end */

/* Keyboard LED key code */
#define KEY_RED_LED		(0x28)
#define KEY_GREEN_LED	(0x29)
#define KEY_BLUE_LED	(0x2A)
/* Keyboard LED key code end */
/* Keyboard key code end */

#define FRAME_VERIFY(arg0, arg1, arg2, arg3, arg4, arg5)	\
	((arg0 + arg1 + arg2 + arg3 + arg4 + arg5) & 0xFF)

typedef struct {
	unsigned char cmd_header0;
	unsigned char cmd_header1;
	unsigned char cmd_type;
	unsigned char cmd;
	unsigned char cmd_verify;
} KEYBOARD_I2C_SEND_MSG_S;

typedef struct {
	unsigned char cmd_header0;
	unsigned char cmd_header1;
	unsigned char cmd_type;
	unsigned char cmd_key0;
	unsigned char cmd_key1;
	unsigned char cmd_key2;
	unsigned char cmd_verify;
} KEYBOARD_I2C_RECV_MSG_S;

#endif /* JC_KEYBOARD_H_ */
