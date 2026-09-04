# CS:APP Malloc Lab build for the reorganized self-study tree.

CC = cc
CPPFLAGS = -I. -Itools
CFLAGS = -Wall -O2

OBJS = mdriver.o mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o

.PHONY: all test test-all clean

all: mdriver

mdriver: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

mdriver.o: tools/mdriver.c tools/fsecs.h tools/fcyc.h tools/clock.h tools/memlib.h tools/config.h mm.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

mm.o: mm.c mm.h tools/memlib.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

memlib.o: tools/memlib.c tools/memlib.h tools/config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

fsecs.o: tools/fsecs.c tools/fsecs.h tools/config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

fcyc.o: tools/fcyc.c tools/fcyc.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clock.o: tools/clock.c tools/clock.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

ftimer.o: tools/ftimer.c tools/ftimer.h tools/config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

test: mdriver
	./mdriver -V -f traces/short1-bal.rep
	./mdriver -V -f traces/short2-bal.rep

test-all: mdriver
	./mdriver -v

clean:
	rm -f $(OBJS) mdriver
