# 编译器设定和编译选项
CC = gcc

# 编译目标：src目录下的所有.c和.S文件
tcp: 
	$(CC) -c -g *.c
	$(CC) -g -o tcp *.o -lnids

# 定义的一些伪目标
.PHONY: clean

# make clean可以清除已生成的文件
clean:
	rm -f *.o tcp
