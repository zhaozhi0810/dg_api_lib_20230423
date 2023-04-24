# 可以不添加下面4个变量


uart_test_4qt:main.o ComFunc.o
	aarch64-linux-gnu-gcc $^ -o $@
	

%.o:%.c
	aarch64-linux-gnu-gcc $< -o $@ -c


clean:
	rm *.o uart_test_4qt


