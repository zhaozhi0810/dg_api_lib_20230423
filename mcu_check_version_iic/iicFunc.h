

#ifndef __IICFUNC_H__
#define __IICFUNC_H__

int i2c_init(void);
void i2c_close(void);


int IIC3_0x40_ReceiveByte (uint8_t *c, uint32_t timeout);
int IIC3_0x40_ReceivePacket (uint8_t *data, uint16_t length, uint32_t timeout);
void IIC3_0x40_SendByte (uint8_t c);
void IIC3_0x40_SendPacket(uint8_t *data, uint16_t length);



#endif


