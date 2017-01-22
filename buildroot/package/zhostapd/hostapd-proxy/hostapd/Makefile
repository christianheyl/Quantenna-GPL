ifndef CC
CC=gcc
endif

ifndef CFLAGS
CFLAGS = -MMD -O2 -Wall -g
endif

-include .config

OBJS += main.o
OBJS += qlink_server.o
OBJS += hapr_log.o
OBJS += hapr_eloop.o
OBJS += hapr_server.o
OBJS += hapr_client.o
OBJS += hapr_bss.o
OBJS += hapr_l2_packet.o
OBJS += hapr_utils.o
OBJS += hapr_netlink.o

LIBS += -lpthread

ALL=hostapd

all: verify_config $(ALL)

Q=@
E=echo
ifeq ($(V), 1)
Q=
E=true
endif

%.o: %.c
	$(Q)$(CC) -c -o $@ $(CFLAGS) $<
	@$(E) "  CC " $<

verify_config:
	@if [ ! -r .config ]; then \
		echo 'Building hostapd requires a configuration file'; \
		echo '(.config). See README for more instructions. You can'; \
		echo 'run "cp defconfig .config" to create an example'; \
		echo 'configuration.'; \
		exit 1; \
	fi

install: all
	mkdir -p $(DESTDIR)/usr/local/bin
	for i in $(ALL); do cp -f $$i $(DESTDIR)/usr/local/bin/$$i; done

hostapd: $(OBJS)
	$(Q)$(CC) $(LDFLAGS) -o hostapd $(OBJS) $(LIBS)
	@$(E) "  LD " $@

clean:
	rm -f core *~ *.o hostapd
	rm -f *.d

-include $(OBJS:%.o=%.d)
