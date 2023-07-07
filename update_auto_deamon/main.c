

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>             // exit
#include <sys/ioctl.h>          // ioctl
#include <string.h>             // bzero
#include <pthread.h>
#include <semaphore.h>
#include <libgen.h>
#include <stdarg.h>
#include <signal.h>

#include <dirent.h>



#include "my_log.h"
#include "debug.h"


static const char* g_build_time_str = "Buildtime :"__DATE__" "__TIME__;   //获得编译时间
	
int server_in_debug_mode = 0;   //服务端进入调试模式		




#if 0
// filename 下载的文件名
// host_ip ftp服务器的ip，端口默认为21
// 返回0表示正常，其他表示异常
int ftp_client_get_file(char* filename,char* host_ip);
#else


 static int mySystem(const char *command)
 {
     int status;
     status = system(command);  
   
     if (-1 == status)  
     {  
         printf("mySystem: system error!");  
         return -1;
     }  
     else  
     {  
         if (WIFEXITED(status))  
         {  
             if (0 == WEXITSTATUS(status))  
             {  
                 return 0; 
             }               
             printf("mySystem: run shell script fail, script exit code: %d\n", WEXITSTATUS(status));  
             return -1;   
         }    
         printf("mySystem: exit status = [%d]\n", WEXITSTATUS(status));   
         return -1;
     }  
 }
     


// filename 下载的文件名
// host_ip ftp服务器的ip，端口默认为21
// 返回0表示正常，其他表示异常
//使用wget命令实现
int ftp_client_get_file(char* filename,char* host_ip)
{
	char cmd[300] = {0};
	int ret;

	if(!filename || !host_ip)   //指针不能为空
		return -4;

	if(strlen(host_ip) > 15)  //超过ip限定的长度
		return -2;

	if(strlen(filename) > 32)  //超过文件名的长度
		return -3;

	sprintf(cmd,"wget -nH -m -q --ftp-user=ftp_hnhtjc --ftp-password=123456 ftp://%s/%s ",host_ip,filename);
	
	if(server_in_debug_mode)  //调试模式下打印
		printf("cmd = %s\n",cmd);

	return  mySystem(cmd);
}


#endif




int get_file_md5sum(const char * filename,char md5buf[40]);

//ps -ef | grep drv_22134_server | grep -v grep | wc -l
//尝试启动server进程
//返回0表示没有该进程，非0表示存在进程了
static int is_server_process_start(char * cmd_name)
{
	FILE *ptr = NULL;
	char cmd[256] = "ps -ef | grep %s | grep -v grep | wc -l";
//	int status = 0;
	char buf[64];
	int count;

//	printf("server DEBUG:cmd_name = %s\n",cmd_name);
	snprintf(cmd,sizeof cmd,"ps -ef | grep %s | grep -v grep | wc -l",cmd_name);
	if(server_in_debug_mode)
		printf("DEBUG: check Process is running, cmd = %s\n",cmd);

	if((ptr = popen(cmd, "r")) == NULL)
	{
		printf("popen err\n");
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	if((fgets(buf, sizeof(buf),ptr))!= NULL)//获取进程和子进程的总数
	{
//		printf("server: buf = %s\n",buf);
		count = atoi(buf);
//		printf("count = %d\n",count);
		if(count < 2)//当进程数小于等于2时，说明进程不存在, 1表示有一个，是grep 进程的
		{
			pclose(ptr);
			if(server_in_debug_mode)
				printf("DEBUG: check Process: is not running!!\n");
			return 0;  //系统中没有该进程	
		}
		if(server_in_debug_mode)
				printf("DEBUG: check Process: process is running!!\n");
	}
	pclose(ptr);
	return 1;
}






static void s_sighandler(int signum) {
	printf("Receive signal %d, program will be exit!", signum);
	exit(0);  //进程退出
}

static int s_signal_init(void) {
	struct sigaction act;
	bzero(&act, sizeof(struct sigaction));
	CHECK(!sigemptyset(&act.sa_mask), -1, "Error sigemptyset with %d: %s", errno, strerror(errno));
	CHECK(!sigaddset(&act.sa_mask, SIGINT), -1, "Error sigaddset with %d: %s", errno, strerror(errno));
	CHECK(!sigaddset(&act.sa_mask, SIGQUIT), -1, "Error sigaddset with %d: %s", errno, strerror(errno));
	CHECK(!sigaddset(&act.sa_mask, SIGTERM), -1, "Error sigaddset with %d: %s", errno, strerror(errno));
//	CHECK(!sigaddset(&act.sa_mask, SIGKILL), -1, "Error sigaddset with %d: %s", errno, strerror(errno));
	act.sa_handler = s_sighandler;
	CHECK(!sigaction(SIGINT, &act, NULL), -1, "Error sigaction with %d: %s", errno, strerror(errno));
	CHECK(!sigaction(SIGQUIT, &act, NULL), -1, "Error sigaction with %d: %s", errno, strerror(errno));
	CHECK(!sigaction(SIGTERM, &act, NULL), -1, "Error sigaction with %d: %s", errno, strerror(errno));
//	CHECK(!sigaction(SIGKILL, &act, NULL), -1, "Error sigaction with %d: %s", errno, strerror(errno));
	return 0;
}



//返回0表示正常，
//1表示已经下载过（md5文件与本地update文件的值相同），不再下载
int download_update_tar(char* host_ip)
{
	FILE *fin;
	int bw = 0; 
	char md5_value[64] = {0};  //保存从md5文件中读取的数据	
	char md5buf[64];   //用于计算得出，
	int checksum_ok = 0;   //校验和正常，1表示正常，0表示不正常

	//2. 下载update.tar.gz
	if(ftp_client_get_file("update.tar.gz.md5",host_ip) == 0)   //先下载md5，这个成功，才下载压缩包
	{
		if(server_in_debug_mode)
			printf("download update.tar.gz.md5 success\n");
		fin = fopen("update.tar.gz.md5", "rb");
	    if (fin != NULL)
	    {
	        /* 文件打开成功*/
	        if(server_in_debug_mode)
	        	printf("open update.tar.gz.md5 success\n");
	    }
	    else
	    {
	        printf("open update.tar.gz.md5 error\n");
	        return -1;
	    }

	    //md5文件的第一行必须是md5值，一次性读出32字节，否则失败
	    bw = fread(md5_value, 1, 32, fin);
	    if(bw != 32)
	    {
	    	fclose(fin);
	    	printf("ERROR: read md5_file failed ! md5_value = %s ret = %d\n",md5_value,bw);
	    	return -1;
	    }
	    fclose(fin);
		

	    //1.判断本地是否存在update.tar.gz，如果存在则计算其md5，如果相同则不再下载否则进行一次下载
	    if(0 == access("update.tar.gz", F_OK)) //文件存在
	    {
	    	if(0 < get_file_md5sum("update.tar.gz",md5buf))  //计算文件的md5
			{
				if(0 == strncmp(md5buf,md5_value,32))  //md5正常
				{
				//	checksum_ok = 1;  //校验和正常
					printf("file is exist and checksum is the same return 1!\n");
					return 1;   //直接返回		
				}
				else  //checksum不同，则继续下载
				{
					md5buf[32] = 0;
					md5_value[32] = 0;
					printf("md5buf = %s\nmd5_value=%s\ngo on downlad \n",md5buf,md5_value);
				}
			}
	    }

	    //2.不存在，或者md5不同，则进行下载操作
		if(ftp_client_get_file("update.tar.gz",host_ip) == 0)  //压缩包正常之后，进行md5校验
		{
			if(server_in_debug_mode)
				printf("download update.tar.gz success\n");
			if(0 < get_file_md5sum("update.tar.gz",md5buf))
			{
				if(0 == strncmp(md5buf,md5_value,32))  //md5正常
				{
					checksum_ok = 1;  //校验和正常
					printf("update.tar.gz checksum is ok!\n");	
					return 0;	 //成功在这返回
				}
				else
				{
					md5buf[32] = 0;
					md5_value[32] = 0;
					printf("md5buf = %s\nmd5_value=%s\n",md5buf,md5_value);
				}
			}
		}
		else
		{
			printf("download update.tar.gz failed\n");
		}
	}
	else
	{
		printf("download update.tar.gz.md5 failed\n");
	}

	return -1;    //到这就是不成功了

}




//用于升级的文件名，根据文件名进行判断
static const char* sofetwarenames[] = {"/root/jc_keyboard.ko",
							"/usr/lib/libdrv722.so",
							"/root/drv722_22134_server",
							"/usr/lib/libdrv722_22134.so",							
							"/home/deepin/rk3399_qt5_test",
							"uboot.img",
							"trust.img",
							"boot.img",
							"hj22134-gd32f103-freertos.bin",   //话音接口板单片机
							"jc_dg_keyboard_gd32.bin",
							"hj22388_freertos-lcd"    //"hj22388_freertos-lcd-old5.bin",导光的屏幕单片机，升级文件是同一个，名字判断的时候old5.bin可以不同判断了
							};









//遍历update目录，对所有的文件进行一次升级操作
int update_software_now(void)
{
	DIR *dirp;
	struct dirent *direntp;
	int i;
	char checked[10] = {0};   //减少字符串对比的次数
	int blkn = 0;
	char cmd[300] = {0};
	char md5_value[64] = {0};  //保存从md5文件中读取的数据	
	char md5buf[64];   //用于计算得出，
	int ret;


	if(chdir("update") != 0)
	{
		printf("change Directory update Error: %s\n", strerror(errno));
		return -1;
	}
	/* 打开目录 */	
	if((dirp=opendir("./"))==NULL)
	{
		printf("Open Directory ./ Error: %s\n", strerror(errno));
		return -1;
	}
 
	if(0 == access("/dev/mmcblk2p4", F_OK)) //文件存在
	{
		blkn = 2;
	}


	/* 返回目录中文件大小和修改时间 */
	while((direntp=readdir(dirp))!=NULL) 
	{
		if(direntp->d_name[0] == '.')   //以.开头的都不处理
			continue;
		//printf("d_name = %s\n",direntp->d_name);
		if(!checked[0] && strncmp("jc_keyboard.ko",direntp->d_name,strlen("jc_keyboard.ko")) == 0)
		{
			if(server_in_debug_mode)
				printf("jc_keyboard.ko\n");
			checked[0] = 1;

			if(0 != access("/root/jc_keyboard.ko", F_OK))   //文件不存在
			{
				ret = mySystem("cp jc_keyboard.ko /root/jc_keyboard.ko");   //升级该文件
				if(ret < 0)
					perror("mySystem 1");
			}
			else
			{			
				//比较md5
				get_file_md5sum("/root/jc_keyboard.ko",md5buf);  //获取md5值
				get_file_md5sum("jc_keyboard.ko",md5_value);

				if(strcmp(md5_value,md5buf) == 0)
				{
					printf("jc_keyboard.ko md5 is the same,no need update\n");
				}
				else
				{
					ret = mySystem("cp jc_keyboard.ko /root/jc_keyboard.ko");   //升级该文件
					if(ret < 0)
						perror("mySystem 2");
				}
			}
		}	
		else if(!checked[1] && strncmp("libdrv722.so",direntp->d_name,strlen("libdrv722.so")) == 0)
		{
			if(server_in_debug_mode)
				printf("libdrv722.so\n");
			checked[1] = 1;

			if(0 != access("/usr/lib/libdrv722.so", F_OK))   //文件不存在，hj喂狗程序需要关闭一下。这个替换的时候会影响喂狗
			{
				ret = mySystem("cp libdrv722.so /usr/lib/libdrv722.so");   //升级该文件
				if(ret < 0)
					perror("mySystem3");
			}
			else
			{	
				//比较md5
				get_file_md5sum("/usr/lib/libdrv722.so",md5buf);  //获取md5值
				get_file_md5sum("libdrv722.so",md5_value);

				if(strcmp(md5_value,md5buf) == 0)
				{
					printf("libdrv722.so md5 is the same,no need update\n");
				}
				else
				{
					if(0 == access("/root/drv722api_disable_wtd", F_OK))  //文件存在
					{
						ret = mySystem("/root/drv722api_disable_wtd");    //这里先关闭一下看门狗，免得造成一些升级过程中的问题
						if(ret < 0)
							perror("mySystem4");
					}	
					ret = mySystem("cp libdrv722.so /usr/lib/libdrv722.so");   //升级该文件
					if(ret < 0)
						perror("mySystem5");
				}
			}
		}

		else if(!checked[2] && strncmp("drv722_22134_server",direntp->d_name,strlen("drv722_22134_server")) == 0)
		{
			if(server_in_debug_mode)
				printf("drv722_22134_server\n");	    //这个可能需要修改执行权限，或者能修改rc.local????
			checked[2] = 1;

			if(0 != access("/root/drv722_22134_server", F_OK))   //文件不存在
			{
				ret = mySystem("cp drv722_22134_server /root/drv722_22134_server");   //升级该文件
				if(ret < 0)
					perror("mySystem6");
			}
			else
			{			
				//比较md5
				get_file_md5sum("/root/drv722_22134_server",md5buf);  //获取md5值
				get_file_md5sum("drv722_22134_server",md5_value);

				if(strcmp(md5_value,md5buf) == 0)
				{
					printf("drv722_22134_server md5 is the same,no need update\n");
				}
				else
				{
					if(0 == access("/root/drv722api_disable_wtd", F_OK))  //文件存在
					{
						ret = mySystem("/root/drv722api_disable_wtd");    //这里先关闭一下看门狗，免得造成一些升级过程中的问题
						if(ret < 0)
							perror("mySystem7");
					}	
					ret = mySystem("cp drv722_22134_server /root/drv722_22134_server");   //升级该文件
					if(ret < 0)
						perror("mySystem8");
				}
			}

		}
		else if(!checked[3] && strncmp("rk3399_qt5_test",direntp->d_name,strlen("rk3399_qt5_test")) == 0)
		{
			if(server_in_debug_mode)
				printf("rk3399_qt5_test\n");    //这个可能需要修改执行权限，或者能修改rc.local????
			checked[3] = 1;	

			if(0 != access("/root/rk3399_qt5_test", F_OK))   //文件不存在
			{
				ret = mySystem("cp rk3399_qt5_test /root/rk3399_qt5_test");   //升级该文件
				if(ret < 0)
					perror("mySystem9");
			}
			else
			{			
				//比较md5
				get_file_md5sum("/root/rk3399_qt5_test",md5buf);  //获取md5值
				get_file_md5sum("rk3399_qt5_test",md5_value);

				if(strncmp(md5_value,md5buf,32) == 0)
				{
					printf("rk3399_qt5_test md5 is the same,no need update\n");
				}
				else
				{
					ret = mySystem("pkill -9 -f rk3399_qt5_test");   //杀掉该进程
					if(ret < 0)
						perror("mySystem10-0");
					ret = mySystem("cp rk3399_qt5_test /root/rk3399_qt5_test");   //升级该文件
					if(ret < 0)
						perror("mySystem10");
				}
			}


		}
		else if(!checked[4] && strncmp("uboot.img",direntp->d_name,strlen("uboot.img")) == 0)   //需要同时升级trust
		{
			if(server_in_debug_mode)
				printf("uboot.img\n");
			if(0 != access("/tmp/uboot_read.img", F_OK))   //文件不存在
			{
				sprintf(cmd,"dd if=/dev/mmcblk%dp1 of=/tmp/uboot_read.img count=8192",blkn); //4MB 除512
				printf("cmd = %s\n",cmd);				
				ret = mySystem(cmd);  //
				if(ret < 0)
					perror("mySystem11");

				sprintf(cmd,"dd if=/dev/mmcblk%dp2 of=/tmp/trust_read.img count=8192",blkn); //4MB 除512
				printf("cmd = %s\n",cmd);				
				ret = mySystem(cmd);  //
				if(ret < 0)
					perror("mySystem12");
			}

			checked[4] = 1;
			if(0 == access("trust.img", F_OK)) //文件存在
			{
				printf("trust.img\n");

				//比较md5
				get_file_md5sum("/tmp/uboot_read.img",md5buf);  //获取md5值
				get_file_md5sum("uboot.img",md5_value);

				//printf("md5buf=%s,md5_value=%s,\n",md5buf,md5_value);

				if(strncmp(md5_value,md5buf,32) == 0)
				{
					printf("uboot.img md5 is the same,no need update\n");
				}
				else
				{
					sprintf(cmd,"dd conv=fsync,notrunc of=/dev/mmcblk%dp1 if=uboot.img ",blkn);
					if(server_in_debug_mode)
						printf("update cmd = %s\n",cmd);
					ret = mySystem(cmd);  //count 是boot的文件大小除以512
					if(ret < 0)
						perror("mySystem13");
					ret = mySystem("cp uboot.img /tmp/uboot_read.img");   //升级的文件已经存在
					if(ret < 0)
						perror("mySystem14");

					sprintf(cmd,"dd conv=fsync,notrunc of=/dev/mmcblk%dp2 if=trust.img ",blkn);
					if(server_in_debug_mode)
						printf("update cmd = %s\n",cmd);
					ret = mySystem(cmd);  //count 是boot的文件大小除以512
					if(ret < 0)
						perror("mySystem15");
					ret = mySystem("cp trust.img /tmp/trust_read.img");   //升级的文件已经存在
					if(ret < 0)
						perror("mySystem16");
				}
			}
		}
		else if(!checked[5] && strncmp("boot.img",direntp->d_name,strlen("boot.img")) == 0)
		{
			struct stat st ; 
			int file_size = 0;			
			checked[5] = 1;
			if(server_in_debug_mode)
				printf("boot.img\n");


			//1. 先把boot分区中的boot读出来，计算md5，一致则不烧写
			if(0 != access("/tmp/boot_read.img", F_OK))   //文件不存在
			{
				stat( "boot.img", &st ); 
				if(server_in_debug_mode)
					printf(" boot.img size = %ld\n", st.st_size); 
				file_size = st.st_size / 512;   //计算块的个数
				sprintf(cmd,"dd if=/dev/mmcblk%dp4 of=/tmp/boot_read.img count=%d",blkn,file_size); //count 是boot的文件大小除以512
				if(server_in_debug_mode)
					printf("cmd = %s\n",cmd);				
				ret = mySystem(cmd);  //count 是boot的文件大小除以512
				if(ret < 0)
					perror("mySystem17");
			}
			get_file_md5sum("/tmp/boot_read.img",md5buf);  //获取md5值
			get_file_md5sum("boot.img",md5_value);

			//printf("md5buf=%s,md5_value=%s,\n",md5buf,md5_value);

			if(strncmp(md5_value,md5buf,32) == 0)
			{
				printf("boot.img md5 is the same,no need update\n");
			}
			else
			{
				sprintf(cmd,"dd conv=fsync,notrunc of=/dev/mmcblk%dp4 if=boot.img ",blkn);
				if(server_in_debug_mode)
					printf("update cmd = %s\n",cmd);
				ret = mySystem(cmd);  //count 是boot的文件大小除以512
				if(ret < 0)
					perror("mySystem18");
				ret = mySystem("cp boot.img /tmp/boot_read.img");   //升级的文件已经存在
				if(ret < 0)
					perror("mySystem19");
			}
		}
		else if(!checked[6] && strncmp("hj22134-gd32f103-freertos.bin",direntp->d_name,strlen("hj22134-gd32f103-freertos.bin")) == 0)
		{
			if(server_in_debug_mode)
				printf("hj22134-gd32f103-freertos.bin\n");
			checked[6] = 1;

			//
			ret = mySystem("../xyzmodem_send_hj -y -f hj22134-gd32f103-freertos.bin");   //这个正常
			if(ret < 0)
				perror("mySystem20");
			
		}
		else if(!checked[7] && strncmp("jc_dg_keyboard_gd32.bin",direntp->d_name,strlen("jc_dg_keyboard_gd32.bin")) == 0)
		{
			if(server_in_debug_mode)
				printf("jc_dg_keyboard_gd32.bin\n");
			checked[7] = 1;
			//
			ret = mySystem("../xyzmodem_send_dg_keyboard  -f jc_dg_keyboard_gd32.bin");   //这个正常
			if(ret < 0)
				perror("mySystem21");
		}
		else if(!checked[8] && strncmp("hj22388_freertos-lcd",direntp->d_name,strlen("hj22388_freertos-lcd")) == 0)
		{
			
			checked[8] = 1;
			snprintf(cmd,sizeof cmd,"../xyzmodem_send_dg_lcd -f %s",direntp->d_name);
			cmd[sizeof cmd - 1] = '\0';
			if(server_in_debug_mode)
				printf("hj22388_freertos-lcd cmd = %s\n",cmd);
			ret = mySystem(cmd);   //这个正常
			if(ret < 0)
				perror("mySystem22");
		}
		else if(!checked[9] && strncmp("libdrv722_22134.so",direntp->d_name,strlen("libdrv722_22134.so")) == 0)
		{
			if(server_in_debug_mode)
				printf("libdrv722_22134.so\n");
			checked[9] = 1;

			if(0 != access("/usr/lib/libdrv722_22134.so", F_OK))   //文件不存在
			{
				ret = mySystem("cp libdrv722_22134.so /usr/lib/libdrv722_22134.so");   //升级该文件
				if(ret < 0)
					perror("mySystem23");
			}
			else
			{	
				//比较md5
				get_file_md5sum("/usr/lib/libdrv722_22134.so",md5buf);  //获取md5值
				get_file_md5sum("libdrv722_22134.so",md5_value);

				if(strcmp(md5_value,md5buf) == 0)
				{
					printf("libdrv722_22134.so md5 is the same,no need update\n");
				}
				else
				{
					ret = mySystem("cp libdrv722_22134.so /usr/lib/libdrv722_22134.so");   //升级该文件
					if(ret < 0)
						perror("mySystem24");
				}
			}

		}
	}


	chdir("../");

	return 0;
}




static const char* my_opt = "DLs:";

int get_software_version_in_gz(void);

//是不是只是开机的时候运行一下？
//其他时候也可以手动运行？
// 必须要用-s指定服务器的ip，否则将使用本地升级。
int main(int argc, char *argv[]) 
{
//	int t;
	int log_enable = 0;   //不开启日志
	int ret;
	int c;
	char* server_ip = NULL;
	printf("%s running,%s\n",argv[0],g_build_time_str);

	if(argc != 1)
	{	
	    while(1)
	    {
	        c = getopt(argc, argv, my_opt);
	        //printf("optind: %d\n", optind);
	        if (c < 0)
	        {
	            break;
	        }
	        //printf("option char: %x %c\n", c, c);
	        switch(c)
	        {
	        	case 's':
	        		server_ip = optarg;

	                //debug_level = atoi(optarg);
	                printf("server_ip = %s\n",server_ip);
	                break;
	           	case 'D':
	        		server_in_debug_mode = 1;    //server开启调试模式		
					log_enable = 0;   //调试模式不开启日志
	                //debug_level = atoi(optarg);
	                printf("ServerDEBUG: update_serverProcess enter Debug Mode!!\n");
	                break;
	            case 'L':
	        		server_in_debug_mode = 0;    //server不开启调试模式		
					log_enable = 1;   //开启日志
	                break;	       
	       	 	default:	       
	                break;
	                //return 0;
	        }
	    }
	}

#if 0
	if(argc >= 2)  //只判断两个参数中的第二个，其他参数不判断
	{
		if(strcmp(argv[1],"-D") == 0)  //加了-D选项，则进入调试模式
		{
			server_in_debug_mode = 1;    //server开启调试模式		
			log_enable = 0;   //调试模式不开启日志
			printf("ServerDEBUG: update_serverProcess enter Debug Mode!!\n");
		}
		else if(strcmp(argv[1],"-L") == 0)  //加了-L选项，则开启日志
		{
			server_in_debug_mode = 0;    //server不开启调试模式		
			log_enable = 1;   //开启日志
		}
	}
#endif
	if(is_server_process_start(basename(argv[0])))   //防止服务程序被多次运行
	{
		printf("ERROR : update_Process is Running, Do not run it again!!\n");
		return 0;
	}
	if(server_in_debug_mode)	
		printf("ServerDEBUG: serverProcess is begin to Running\n");

#if 0
	//转为守护进程
	if(!server_in_debug_mode){
		if(daemon(0,0))   //daemon,调试模式不进入守护进程模式，
		{
			perror("daemon");
			return -1;
		}

	
		//日志记录
		if(log_enable)  //非零可以开启日志
		{
	 		if(0 !=log_init())  //调试模式不记录日志，
	 			printf("ERROR: log thread init!!");
		}

	}
#endif
	// if(server_in_debug_mode)	
	// 	printf("ServerDEBUG: serverProcess uart init ok!!!\n");


	//s_signal_init();

	//pthread_create(&mcu_uart_rcv_thread, NULL, mcu_recvSerial_thread, NULL);

	//1. 启动后建立版本信息
	get_software_version_in_gz();
	printf("get_software_version_in_gz done\n");
	

	if(server_ip != NULL)
	{
		ret = download_update_tar(server_ip);   //正常返回0或1
	}
	else
	{
		printf("update use local dir!\n");
		ret = 10;    //这种情况不需要解压
	}
	
	if(ret >= 0) //需要升级
	{
		printf("ready to update!\n");
		
		if(ret != 10)  //没有下载不用解压
			mySystem("tar xf update.tar.gz");   //解压
		update_software_now();    //正式升级的函数
	}
	else
		printf("ret = %d\n",ret);


	//while(1)
	{				
	//	sleep(1);
	}
	return 0;
}
