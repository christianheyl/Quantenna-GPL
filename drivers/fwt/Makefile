#
# Copyright (c) 2013 Quantenna Communications, Inc.
#

EXTRA_CFLAGS += -Wall -Werror -Wno-unknown-pragmas -mlong-calls \
		-I../drivers -I../include -I../drivers/include/shared -I../drivers/include/kernel

fwt_ctrl-objs += fwt.o fwt_sw.o fwt_db.o
obj-m += fwt_ctrl.o

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions Module.symvers

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
