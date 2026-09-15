# Proxy Lab 构建脚本
#
# 除 handin 规则外，可以按需修改本文件。
# 教师会使用本 Makefile 执行 make，
# 从源码构建你的代理程序。

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy

.PHONY: all test clean

# 验证转发、并发和缓存行为，不运行官方评分。
test: proxy
	python3 tools/test-part1.py

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

cache.o: cache.c cache.h
	$(CC) $(CFLAGS) -c cache.c

proxy.o: proxy.c csapp.h cache.h
	$(CC) $(CFLAGS) -c proxy.c

proxy: proxy.o csapp.o cache.o
	$(CC) $(CFLAGS) proxy.o csapp.o cache.o -o proxy $(LDFLAGS)

# 在上一级目录生成用于提交的 proxylab-handin.tar 压缩包。
# 不要修改此提交规则！
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz
