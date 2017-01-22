#
# Quantenna Communications Inc. Driver Makefile
#

NETDEBUG_STRUCT_HEADERS := ../include/qtn/muc_phy_stats.h \
				../include/qtn/muc_txrx_stats.h \
				../include/qtn/auc_debug_stats.h \
				../include/qtn/skb_recycle.h \
				$(src)/qdrv_wlan.h \
				$(src)/qdrv_slab_def.h \
				../linux/include/linux/netdevice.h

NETDEBUG_CHECKSUM_HEADER := $(obj)/qdrv_netdebug_checksum.h

EXTRA_CFLAGS += -Wall -Werror -Wno-unknown-pragmas -DQDRV \
		-I../drivers -I../include -I../drivers/include/shared -I../drivers/include/kernel
EXTRA_CFLAGS += -mlong-calls
EXTRA_CFLAGS += -DQTN_ENABLE_TRACE_BUFFER=0 -DQSCS_ENABLED -DQTN_BG_SCAN -DQBMPS_ENABLE
EXTRA_CFLAGS += -DCONFIG_QTN_80211K_SUPPORT

ifeq ($(CONFIG_QVSP), y)
EXTRA_CFLAGS += -DCONFIG_QVSP
endif
EXTRA_CFLAGS += -DQTN_DEBUG

ifeq ($(board_config), qtm710_rgmii_config)
EXTRA_CFLAGS += -DQTM710_RGMII_CONFIG
endif

ifeq ($(board_config),topaz_msft_config)
	EXTRA_CFLAGS += -DERICSSON_CONFIG
else
	ifeq ($(board_config),topaz_msmr_config)
		EXTRA_CFLAGS += -DERICSSON_CONFIG
	endif
endif

EXTRA_CFLAGS += -g      # needed for pktlogger

SOURCE_MODULES := qdrv_module.o qdrv_control.o qdrv_soc.o \
			qdrv_hal.o qdrv_muc.o qdrv_mac.o qdrv_mac_reserve.o qdrv_comm.o \
			qdrv_wlan.o qdrv_tx.o qdrv_rx.o qdrv_scan.o \
			qdrv_hostlink.o qdrv_vap.o \
			qdrv_dsp.o qdrv_txbf.o qdrv_muc_stats.o \
			qdrv_radar.o qdrv_math.o qdrv_bridge.o \
			qdrv_uc_print.o qdrv_sch.o qdrv_sch_pm.o \
			qdrv_pktlogger.o qdrv_auc.o qdrv_pcap.o	\
			qdrv_vlan.o qdrv_show.o \
			qdrv_mu.o qdrv_fw.o

ifneq ($(KERNELRELEASE),)

obj-m		+=	qdrv.o
qdrv-objs	+=	$(SOURCE_MODULES)

$(src)/qdrv_slab_def.h: $(src)/qdrv_slab_def.h.in $(src)/qdrv_slab_watch.h
	cpp $(src)/qdrv_slab_def.h.in > $(src)/qdrv_slab_def.h

$(src)/qdrv_control.c $(src)/qdrv_pktlogger.c $(src)/qdrv_wlan.c $(src)/qdrv_tx.c: $(NETDEBUG_CHECKSUM_HEADER)

$(src)/qdrv_module.c: $(src)/qdrv_slab_def.h

$(obj)/headers.txt: $(NETDEBUG_STRUCT_HEADERS)
	cat $(NETDEBUG_STRUCT_HEADERS) | \
		../host/scripts/struct_get.sh |\
		grep -v '^[ 	]*$$' > $(obj)/headers.txt

.PHONY: $(NETDEBUG_CHECKSUM_HEADER) qdrv_slab_def.h

$(NETDEBUG_CHECKSUM_HEADER): $(obj)/headers.txt
	echo '/* Automatically generated file.  Do not edit. */' > $@
	echo -n '#define QDRV_NETDEBUG_CHECKSUM ' >> $@
	echo '0x'`md5sum $(obj)/headers.txt | cut -b 1-8` >> $@
	echo -n '#define QDRV_BUILDDATE ' >> $@
	echo `date '+%s'`'UL' >> $@

else # $(KERNELRELEASE)

include ../Makefile_defines

DWARF_FILE      ?= ./qdrv_dwarf.txt
HEADERS_FILE    ?= ./headers.txt

default:
	$(MAKE) -C $(KERNELDIR) \
		ARCH=$(ARCHITECTURE) \
		CROSS_COMPILE=$(TOOLS_PREFIX) \
		M=$(PWD) \
		modules

install:
	$(MAKE) -C $(KERNELDIR) \
		ARCH=$(ARCHITECTURE) \
		CROSS_COMPILE=$(TOOLS_PREFIX) \
		M=$(PWD) \
		INSTALL_MOD_PATH=$(INSTALL_PATH) \
		modules_install

dwarf:
	$(TOOLS_PREFIX)readelf --debug-dump=info qdrv.o > $(DWARF_FILE)
	cp headers.txt $(HEADERS_FILE) || :

endif # $(KERNELRELEASE)

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions \
		headers.txt qdrv_dwarf.txt qdrv_slab_def.h \
		$(NETDEBUG_CHECKSUM_HEADER) $(QDRV_BLD_HEADER)

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
