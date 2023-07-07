


update_auto_rk3399:main.o check_version.o my_log.o  
	aarch64-linux-gnu-gcc $^ -o $@ -lpthread 


%.o:%.c
	aarch64-linux-gnu-gcc $< -c 	


clean:
	rm update_auto_rk3399 *.o

