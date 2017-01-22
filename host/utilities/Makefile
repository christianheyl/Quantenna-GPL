#
# (C) Copyright 2007 Quantenna Communications Inc.
#

CFLAGS += -Wall -Werror

.PHONY: bin2bin bin2hex ruby_mem_helper

all: bin2bin bin2hex ruby_mem_helper

bin2bin:
	gcc $(CFLAGS) bin2bin.c -Wall -o bin2bin

bin2hex:
	gcc $(CFLAGS) bin2hex.c -Wall -o bin2hex

ruby_mem_helper: ruby_mem_helper.c
	gcc $(CFLAGS) -I../../common $< -o $@

clean:
	rm -f *.o bin2bin bin2hex ruby_mem_helper

distclean: clean
