/*
* @Author: dazhi
* @Date:   2023-05-05 16:22:34
* @Last Modified by:   dazhi
* @Last Modified time: 2023-05-08 15:50:58
*/

 
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <linux/i2c-dev.h>
// Terrible portability hack between arm-linux-gnueabihf-gcc on Mac OS X and native gcc on raspbian.
#ifndef I2C_M_RD
#include <linux/i2c.h>
#endif
 
typedef unsigned char   u8;
typedef unsigned char   uint8_t;
typedef unsigned short   uint16_t;
typedef unsigned int   uint32_t;
static int i2c_fd = -1;
const char *i2c_fname = "/dev/i2c-3";   //按键在iic3上
#define MCU_IIC_SLAVER_ADDR 0x40
 




// Returns a new file descriptor for communicating with the I2C bus:
int i2c_init(void) {
    if ((i2c_fd = open(i2c_fname, O_RDWR)) < 0) {
        char err[64];
        sprintf(err, "open('%s') in i2c_init", i2c_fname);
        perror(err);
        return -1;
    }
 
    return i2c_fd;
}
 
void i2c_close(void) {
    if(i2c_fd >= 0)
    {
        close(i2c_fd);
        i2c_fd = -1;
    }
    
}
 
// Write to an I2C slave device's register:
static int i2c_write(u8 slave_addr, uint8_t data[],uint16_t len) {
    int retval;

    struct i2c_msg msgs[1];
    struct i2c_rdwr_ioctl_data msgset[1];

    if(i2c_fd < 0)
    {
        printf("error: i2c_fd < 0 \n");
        return -1;
    }

    msgs[0].addr = slave_addr;
    msgs[0].flags = 0;
    msgs[0].len = len;
    msgs[0].buf = data;
 
    msgset[0].msgs = msgs;
    msgset[0].nmsgs = 1;
 
    if (ioctl(i2c_fd, I2C_RDWR, &msgset) < 0) {
        perror("ioctl(I2C_RDWR) in i2c_write");
        return -1;
    }
 
    return 0;
}
 
// Read the given I2C slave device's register and return the read value in `*result`:
// 每次只能读一个字节上来
static int i2c_read(u8 slave_addr, uint8_t data[],uint8_t len) {
    int retval;
//    u8 outbuf[1], inbuf[1];
    struct i2c_msg msgs[1];//这个地方要封两个包。因为要先写入需要读的地址，然后才是读操作
    struct i2c_rdwr_ioctl_data msgset[1];
 

    if(i2c_fd < 0)
    {
        printf("error: i2c_fd < 0 \n");
        return -1;
    }


    msgs[0].addr = slave_addr;
    msgs[0].flags = I2C_M_RD;//表示读
    msgs[0].len = len;
    msgs[0].buf = data;
 
    // msgs[1].addr = slave_addr;
    // msgs[1].flags = I2C_M_RD;  //读
    // msgs[1].len = 1;
    // msgs[1].buf = inbuf;
 
    msgset[0].msgs = msgs;
    msgset[0].nmsgs = 1;
 
    // outbuf[0] = reg;
 
    // inbuf[0] = 0;
 
    //*result = 0;
    if (ioctl(i2c_fd, I2C_RDWR, &msgset) < 0) {
        perror("ioctl(I2C_RDWR) in i2c_read");
        return -1;
    }
 
    //*result = inbuf[0];
    return 0;
}



int IIC3_0x40_ReceiveByte (uint8_t *c, uint32_t timeout)
{
    int ret;
    usleep(100000);   //延时一下，2023-0508
    do
    {
        ret = i2c_read(MCU_IIC_SLAVER_ADDR, c, 1); 
        if(ret == 0)
        {
        //    printf("IIC3_0x40_ReceiveByte c = %#x\n",*c);
            break;
        }    
        else
        {
            usleep(1000);  //1ms
            timeout --;
        }   
    }
    while(timeout);

    return ret;
}



int IIC3_0x40_ReceivePacket (uint8_t *data, uint16_t length, uint32_t timeout)
{
    int ret;
    int i = 0;
    usleep(50000);  //延时一下100ms，2023-0508
    do
    {
        ret = i2c_read(MCU_IIC_SLAVER_ADDR, data,length); 
    //    ret = i2c_read(MCU_IIC_SLAVER_ADDR, data+i); 
        if(ret == 0)
        {
         //   printf("IIC3_0x40_ReceivePacket data = %#x,i=%d,len = %d\n",data[i],i,length);
            // i++;
            // if(i >= length)
                return 0;
        }
        else
        {
            usleep(1000);  //1ms
            timeout --;
        }   
    }
    while(timeout);

    printf("ERROR: IIC3_0x40_ReceivePacket i = %d,length = %d\n",i,length);

    return -1;

}


void IIC3_0x40_SendByte (uint8_t c)
{
    i2c_write(MCU_IIC_SLAVER_ADDR, &c,1) ;
}



void IIC3_0x40_SendPacket(uint8_t *data, uint16_t length)
{
    i2c_write(MCU_IIC_SLAVER_ADDR, data,length) ;
}

// int main(void)
// {
// 	if(i2c_init() < 0)
// 	{
// 		printf("error: i2c_init\n");
// 		return -1;
// 	}		

// 	i2c_close();

// 	return 0;
// }


