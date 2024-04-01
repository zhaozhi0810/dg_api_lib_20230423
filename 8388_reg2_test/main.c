#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/input.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char* argv[])
{
	int t = 10000;   //次数
	int d = 2000;   // 间隔时间
	int i = 0;
	printf("Usage : %s [-t 10000] [-d 2000]\n",argv[0]);
	

	if(argc == 1)
	{
		//nothing todo
	}
	else if(argc == 3)
	{
		if(strcmp(argv[1],"-t") == 0)
			t = atoi(argv[2]);
		else if(strcmp(argv[1],"-d") == 0)
			d = atoi(argv[2]);
		else{
			printf("Usage : %s [-t 10000] [-d 2000]\n",argv[0]);
			return -1;
		}

	}
	else if(argc == 5)
	{
		if(strcmp(argv[1],"-t") == 0)
			t = atoi(argv[2]);
		else if(strcmp(argv[1],"-d") == 0)
			d = atoi(argv[2]);
		else{
			printf("Usage : %s [-t 10000] [-d 2000]\n",argv[0]);
			return -1;
		}	

		if(strcmp(argv[3],"-t") == 0)
			t = atoi(argv[4]);
		else if(strcmp(argv[3],"-d") == 0)
			d = atoi(argv[4]);
		else{
			printf("Usage : %s [-t 10000] [-d 2000]\n",argv[0]);
			return -1;
		}

	}
	else
	{
		printf("Usage : %s [-t 10000] [-d 2000]\n",argv[0]);
		return -1;
	}


	printf("t = %d times d = %d micro second\n",t,d);


	while(t--)
	{
		printf("i = %d\n",++i);
		printf("set reg2 bit7 ,mic reset\n");

		system("i2cset -f -y 4 0x10 0x2 0x80");

		usleep(d*1000);  //调整到ms

		printf("clear reg2 bit7 ,mic normal work\n");

		system("i2cset -f -y 4 0x10 0x2 0x00");
		usleep(d*1000);  //调整到ms

	}

	printf("process end\n");


	return 0;
		
}



