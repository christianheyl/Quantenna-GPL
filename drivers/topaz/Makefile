EXTRA_CFLAGS +=	-Wall -Werror -Wno-unknown-pragmas \
		-I../include \
		-I../ \
		-I../drivers/include/shared \
		-I../drivers/include/kernel

ifeq ($(board_config),topaz_config)
	EXTRA_CFLAGS += -DTOPAZ_VB_CONFIG
else
	ifeq ($(board_config),topaz_vb_config)
		EXTRA_CFLAGS += -DTOPAZ_VB_CONFIG
	else
		ifeq ($(board_config),topaz_rfic6_config)
			EXTRA_CFLAGS += -DTOPAZ_VB_CONFIG
		endif
	endif
endif

ifeq ($(board_config),topaz_msft_config)
	EXTRA_CFLAGS += -DERICSSON_CONFIG
else
	ifeq ($(board_config),topaz_msmr_config)
		EXTRA_CFLAGS += -DERICSSON_CONFIG
	endif
endif

obj-y += tqe.o hbm.o busmon.o temp_sens.o topaz_congest_queue.o

ifeq ($(QTN_EXTERNAL_MODULES),y)
obj-$(CONFIG_ARCH_TOPAZ_FWT) += fwt_if.o
obj-$(CONFIG_ARCH_TOPAZ_TQE) += switch_tqe.o
obj-$(CONFIG_ARCH_TOPAZ_TQE) += switch_vlan.o
obj-$(CONFIG_ARCH_TOPAZ_EMAC) += dpi.o
obj-$(CONFIG_ARCH_TOPAZ_EMAC) += switch_emac.o
endif

ifneq ($(CONFIG_ARCH_TOPAZ_SWITCH_TEST),)
	TOPAZ_TEST ?= $(CONFIG_ARCH_TOPAZ_SWITCH_TEST)
endif

ifneq ($(TOPAZ_TEST),)
	obj-m += busmon_test.o
	FWT_TEST ?= $(TOPAZ_TEST)
	DPI_TEST ?= $(TOPAZ_TEST)
	VLAN_TEST ?= $(TOPAZ_TEST)
	IPPRT_TEST ?= $(TOPAZ_TEST)
	TQE_PKTGEN ?= $(TOPAZ_TEST)
endif

ifneq ($(TOPAZ_FWT_MAINTENANCE_ENABLE),)
	EXTRA_CFLAGS += -DTOPAZ_FWT_MAINTENANCE_ENABLE=$(TOPAZ_FWT_MAINTENANCE_ENABLE)
endif

obj-$(FWT_TEST) += fwt_test.o
obj-$(DPI_TEST) += dpi_test.o
obj-$(VLAN_TEST) += vlan_test.o
obj-$(IPPRT_TEST) += ipprt_test.o
obj-$(TQE_PKTGEN) += switch_pktgen.o

obj-m += $(obj-1)

test_gen_pcap: test_gen_pcap.c
	gcc -Wall -Werror -Wextra -O -g $< -lpcap -o $@ -MD -MF $@.d
-include test_gen_pcap.d

switch_test.pcap: test_gen_pcap
	./$< $@

fwttest:
	gcc -DCONSOLE_TEST -Wall -Werror -Wextra -O2 -g fwt_test.c -o fwt.test.o -lz	\
		-I../../ -I../../include/
	./fwt.test.o

