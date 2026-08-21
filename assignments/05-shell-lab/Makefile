# Makefile for the CS:APP Shell Lab
#
# 相对官方 handout 的改动：文件按用途分到了 docs/ traces/ tools/ 三个目录，
# 这里的路径跟着改；test01..16 / rtest01..16 用模式规则代替 32 条重复目标，
# 另加 test-all / rtest-all 一次跑完 16 个 trace。
# 测试程序仍然编译到当前目录，因为 trace 里是按 ./myspin 这样调用的。

TRACEDIR = traces
TOOLDIR  = tools
DRIVER   = ./$(TOOLDIR)/sdriver.pl
TSH      = ./tsh
TSHREF   = ./$(TOOLDIR)/tshref
TSHARGS  = "-p"
CC       = gcc
CFLAGS   = -Wall -O2

HELPERS  = ./myspin ./mysplit ./mystop ./myint
FILES    = $(TSH) $(HELPERS)
NUMS     = 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16

all: $(FILES)

myspin: $(TOOLDIR)/myspin.c
	$(CC) $(CFLAGS) -o $@ $<
mysplit: $(TOOLDIR)/mysplit.c
	$(CC) $(CFLAGS) -o $@ $<
mystop: $(TOOLDIR)/mystop.c
	$(CC) $(CFLAGS) -o $@ $<
myint: $(TOOLDIR)/myint.c
	$(CC) $(CFLAGS) -o $@ $<


##################
# Regression tests
##################

# 跑自己的 shell：make test01 … make test16
test%: $(FILES)
	$(DRIVER) -t $(TRACEDIR)/trace$*.txt -s $(TSH) -a $(TSHARGS)

# 跑参考 shell：make rtest01 … make rtest16（tshref 是 Linux x86-64 二进制）
rtest%: $(HELPERS)
	$(DRIVER) -t $(TRACEDIR)/trace$*.txt -s $(TSHREF) -a $(TSHARGS)

# 一次跑完 16 个 trace
test-all: $(addprefix test,$(NUMS))
rtest-all: $(addprefix rtest,$(NUMS))

.NOTPARALLEL:
.PHONY: all clean test-all rtest-all


# clean up
clean:
	rm -f $(FILES) *.o *~
