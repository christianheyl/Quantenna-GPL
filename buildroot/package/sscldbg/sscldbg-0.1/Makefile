CC=gcc

ifndef CFLAGS
CFLAGS = -MMD -O2 -Wall -g
endif


# Uncomment following line and set the path to your kernel tree include
# directory if your C library does not include all header files.
# CFLAGS += -DUSE_KERNEL_HEADERS -I/usr/src/linux/include

-include .config

OBJS =	sscl_statedump.o \

sscldbg: $(OBJS)
	$(CC) -o sscldbg $(OBJS) $(LIBS)

all: sscldbg

.PHONY: clean

clean:
	rm -f *.o *.d sscldbg

#install:
#	scp sscldbg root@10.10.0.209:/var/ftp
#	scp sscldbg root@10.10.0.210:/var/ftp


