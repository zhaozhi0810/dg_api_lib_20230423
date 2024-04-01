/*
* @Author: dazhi
* @Date:   2023-06-09 10:29:03
* @Last Modified by:   dazhi
* @Last Modified time: 2023-07-06 10:39:56
*/


//检查软件的版本，一般就是程序启动（上电）的时候检查一次
//生成一个版本文件到tmp目录下的.mysoft_version(隐藏文件)
//里面对应的是文件名:md5值，每个软件占据一行
//单片机的版本 可以查询md5值
//内核和uboot好像 没有md5值呀，可能每次都只能升级了，没法进行对比操作。
//jc_keyboad.ko 算出来
//lib.so 可以算出来
//qt 可以算出来

/*
	单片机的版本查询，有的是串口（串口还不同），有的是iic。

	使用对应的进程算了，这样有利于维护，也可以自己手动调用

	路径： /home/deepin/mcu_update/read_mcu_version_

	
	read_mcu_version_hj: 话音接口板的单片机的版本
	read_mcu_version_dg_keyboard： 导光键盘单片机的版本
	read_mcu_version_dg_lcd 导光屏幕单片机的版本
 */




#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>             // exit
#include <string.h>             // bzero


#define UBOOT_NAME     "uboot"
#define KERNEL_NAME     "kernel"




static const char* sofetwarenames[] = {"/root/jc_keyboard.ko",
							"/usr/lib/libdrv722.so",
							"/root/drv722_22134_server",
							"/usr/lib/libdrv722_22134.so",							
							"/home/deepin/rk3399_qt5_test"
							//"/sys/mysysinfo/mysysinfo"    //这个单独处理
							};

//写入文件的名字
static const char* softfirmname[] ={
					"jc_keyboard.ko",
					"libdrv722.so",
					"drv722_22134_server",
					"libdrv722_22134.so",
					"rk3399_qt5_test",
					"uboot.img",
					"boot.img"
					,"hj22134-gd32f103-freertos.bin"
					,"jc_dg_keyboard_gd32.bin"
					,"hj22388_freertos-lcd-new5.bin"
};


extern int server_in_debug_mode;   //服务端进入调试模式



//存在返回1，否则不存在。
int is_file_exist(const char* filename)
{
	struct stat buf;
	if(!filename)
		return -1;


	if (stat(filename, &buf) == 0) {
		if(server_in_debug_mode)
        	printf("File %s exists\n",filename);
        return 1;
    } else {
    	if(server_in_debug_mode)
        	printf("File %s  does not exist\n",filename);
    }

    return 0;   //文件不存在
}


//返回值等于1 表示正常，否则是出错。
int get_file_md5sum(const char * filename,char md5buf[40])
{
	FILE * filep;
	char cmd[128] = {"md5sum "};
	
	int ret;

	strcat(cmd,filename);

	filep = popen(cmd,"r");
	if(!filep)
		return -1;
    ret = fread(md5buf,32,1,filep);

    md5buf[32] = '\0';
// 	printf("ret = %d\n",ret);
//    printf("get_file_md5sum = %s\n",md5_readBuf);

    pclose(filep);

    return ret;
}

static int write_index = 0;

static int write_md5sum_tofile(int fd,const char*firmname,char md5buf[40])
{
	char buf[128] = {0};

	sprintf(buf,"**%d.",++write_index);

	strcat(buf,firmname);
	strcat(buf,":\n");
	strncat(buf,md5buf,32);
//	if(buf[strlen(buf)-1] != '\n')
//		strcat(buf,"\n");   //占一行
	strcat(buf,"\n\n");
	return write(fd,buf,strlen(buf));
}

//返回0表示成功，其他表示失败
//mcu_id ： 0 表示话音接口板，1表示导光键盘，2表示导光屏幕，其他表示错误
static int get_mcu_version(int mcu_id,char md5buf[40])
{
	FILE * filep;
	char *mcucmd[3] = {"hj","dg_keyboard","dg_lcd"};
	char cmd[128] = {"/home/deepin/mcu_update/read_mcu_version_"};	
//	int ret = -1;
	char readbuf[128];   //读取缓存
//	char* pret;

	if(mcu_id <0 || mcu_id > 2)
		return -1;

	if(!md5buf)
		return -2;

	strcat(cmd,mcucmd[mcu_id]);  //组合命令
	if(server_in_debug_mode)
		printf("cmd = %s\n",cmd);
	filep = popen(cmd,"r");
	if(!filep)
		return -3;
    //ret = fread(readbuf,1024,1,filep);
 	if(NULL == fgets(readbuf,sizeof readbuf,filep))  //第一行不要
 	{
 		pclose(filep);
 		return -4;
 	}	
 	if(server_in_debug_mode)
 		printf("readbuf 1 mcu name = %s\n",readbuf);

 	if(NULL == fgets(readbuf,sizeof readbuf,filep))
 	{	
 		pclose(filep);
 		return -5;
 	}
 	if(server_in_debug_mode)
 		printf("readbuf 2 md5= %s\n",readbuf);
 	if((strlen(readbuf) > 32) && (strncmp("md5:",readbuf,4) == 0))
 		strcpy(md5buf,readbuf+4);   //拷贝
 	else
 	{
 		pclose(filep);
 		return -6;
 	}

 	if(server_in_debug_mode)
    	printf("get_mcu_version MD5 = %s\n",md5buf);
    pclose(filep);

    return 0;	

}



//firmname,只能是uboot和boot; trust不做对比，以uboot为准
//bootimg_size 表示bootimg的长度，字节表示，查询uboot的时候可以任意值
//返回值0表示正常，其他表示异常

static int get_boot_uboot_version(char*firmname,char md5buf[40],int bootimg_size)
{
	int blkn = 0;
	char cmd[128] = {0};
	int file_size;
	int ret;
	if(0 == access("/dev/mmcblk2p4", F_OK)) //文件存在
	{
		blkn = 2;
	}

	if((bootimg_size < 15*1024*1024) || (bootimg_size > 32*1024*1024))  //不能小于15M，也不能大于32M
	{
		bootimg_size = 20336640;
	}

	if(strncmp("boot",firmname,4) ==0)
	{
		//1. 先把boot分区中的boot读出来，计算md5，一致则不烧写
		if(0 != access("/tmp/boot_read.img", F_OK))   //文件不存在
		{
		//	int bootimg_size = 20336640;    //设置一个假设值。
			//printf(" boot.img size = %ld\n", st.st_size); 
			file_size = bootimg_size / 512;   //计算块的个数
			sprintf(cmd,"dd if=/dev/mmcblk%dp4 of=/tmp/boot_read.img count=%d  > /dev/null 2>&1",blkn,file_size); //count 是boot的文件大小除以512
			if(server_in_debug_mode)
				printf("cmd = %s\n",cmd);				
			ret = system(cmd);  //count 是boot的文件大小除以512
			if(ret == -1)
				printf("boot_read system error!\n");
		}

		if(0 != access("/tmp/boot_read.img", F_OK | R_OK))   //还是不存在
			return -1;

		get_file_md5sum("/tmp/boot_read.img",md5buf);  //获取md5值

		return 0;
	}		

	else if(strncmp("uboot",firmname,5) ==0)
	{
		//1. 先把uboot分区中的uboot读出来，计算md5，一致则不烧写
		if(0 != access("/tmp/uboot_read.img", F_OK))   //文件不存在
		{
			sprintf(cmd,"dd if=/dev/mmcblk%dp1 of=/tmp/uboot_read.img count=8192 > /dev/null 2>&1",blkn); //4MB 除512
			if(server_in_debug_mode)
				printf("cmd = %s\n",cmd);				
			ret = system(cmd);  //
			if(ret == -1)
				printf("uboot_read system error!\n");
			//turst以uboot为准，不再单独进行比对
			// sprintf(cmd,"dd if=/dev/mmcblk%dp2 of=/tmp/trust_read.img count=8192",blkn); //4MB 除512
			// printf("cmd = %s\n",cmd);				
			// system(cmd);  //
		}

		if(0 != access("/tmp/uboot_read.img", F_OK | R_OK))   //还是不存在
			return -1;

		get_file_md5sum("/tmp/uboot_read.img",md5buf);  //获取md5值
		return 0;
	}

	return 1;  //无法识别的
}






//获取软件版本
int get_software_version_in_gz(void)
{
	char md5buf[40];
	//char buf[128];
	int fd;
	//FILE* fp;
	unsigned int i;

	// if(is_file_exist("/tmp/.mysoft_version"))
	// 	return 0;
	write_index = 0;
	fd = open("/tmp/.mysoft_version",O_WRONLY | O_CREAT , 0666);
	if(fd < 0)
	{
		perror("open");
		printf("open(create) mysoft_version failed!\n");
		return -1;
	}
	if(server_in_debug_mode)
		printf("open mysoft_version success!!fd = %d\n",fd);

	for(i=0;i<sizeof sofetwarenames / sizeof(sofetwarenames[0])-1;i++)
	{
		if(is_file_exist(sofetwarenames[i]))
		{
			if(get_file_md5sum(sofetwarenames[i],md5buf)>0)
			{
				write_md5sum_tofile(fd,softfirmname[i],md5buf);
			}
		}
	}


	if(0 == get_boot_uboot_version("boot",md5buf,0))   //如果不知道bootimg的字节大小，填0就是
	{
		write_md5sum_tofile(fd,softfirmname[6],md5buf);	
	}
	if(0 == get_boot_uboot_version("uboot",md5buf,0))   //uboot不用第三个参数指定大小
	{
		write_md5sum_tofile(fd,softfirmname[5],md5buf);	
	}
#if 0
	if(is_file_exist("/sys/mysysinfo/mysysinfo"))
	{
		fp = fopen("/sys/mysysinfo/mysysinfo","r");
		if(fp)
		{
			fgets(buf,sizeof buf,fp);  //第一行不要
			fgets(buf,sizeof buf,fp);  //第二行是内核
			if(!strncmp("Kernel",buf,6))
			{

				write_md5sum_tofile(fd,softfirmname[6],buf+18);		
			}
			fgets(buf,sizeof buf,fp);  //第三行不要
			fgets(buf,sizeof buf,fp);  //第四行不要
			fgets(buf,sizeof buf,fp);  //第五行是uboot
			if(!strncmp("UbootVersion",buf,6))
			{
				write_md5sum_tofile(fd,softfirmname[5],buf+29);		
			}
			
			fclose(fp);
		}
	}
#endif


	for(i=0;i<3;i++)
	{
		if(get_mcu_version(i,md5buf) == 0)  //成功
		{
			write_md5sum_tofile(fd,softfirmname[7+i],md5buf);
		}
	}
	


	//关闭文件
	close(fd);

	return 0;
}




// //对比版本，这个可能要多个函数
// void check_software_version_in_gz(void)
// {

// }


#if 0
int main(int argc ,char * argv[])
{
	get_software_version_in_gz();

	return 0;
}


#endif
