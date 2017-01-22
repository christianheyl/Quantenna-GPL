#############################################################
#
# tar to archive target filesystem
#
#############################################################

RUBY_FS_PATCH_DIR = $(TOPDIR)/target/device/Quantenna/Ruby
RUBY_SCRIPTS_DIR = $(TOPDIR)/target/device/Quantenna/Ruby/scripts
RUBY_POWER_TABLES_DIR = $(TOPDIR)/../power_tables/board_config

CHKIMAGE = $(TOPDIR)/../u-boot/tools/chkimage_target
GCC_LIBDIR = /usr/local/ARC/gcc/arc-linux-uclibc/lib/
GDBSERVER_SRC = /usr/local/ARC/gcc/bin/gdbserver
GDBSERVER_DEST = $(TARGET_DIR)/usr/bin/gdbserver

RUBY_SCRIPT_DIRS = \
		scripts \
		scripts/int_testing \
		scripts/int_testing/script_bf \
		scripts/int_testing/script_vco

RUBY_ALL_SCRIPTS = $(shell cd $(RUBY_SCRIPTS_DIR) ; ls -1 .)

RUBY_EXCLUDED_SCRIPTS = get_radar_status ntpclient ntpclient.sh

ifneq ($(POST_RF_LOOP),1)
RUBY_EXCLUDED_SCRIPTS += post
endif

RUBY_STD_SCRIPTS = $(filter-out $(RUBY_EXCLUDED_SCRIPTS),$(RUBY_ALL_SCRIPTS))

RUBY_TARGETS = \
		ruby_hotplug \
		ruby_scripts \
		ruby_releasefile \
		ruby_inetd \
		ruby_sysfs \
		ruby_profile \
		ruby_interfaces_simple \
		chkimage \
		ruby_gdbserver \
		ruby_initd \
		ruby_regulatory_db \
		ruby_power_tables

RUBY_OTHERS = \
		ruby_socipaddress \
		ruby_syslogconf \
		ruby_inittab \
		ruby_hostname \

RUBY_FW_FILES = \
		qtn_driver.0 \
		cal_driver.0 \
		qtn_driver.0.RFIC4 \
		cal_driver.0.RFIC4 \
		suc_driver.0 \
		stub_driver.0 \
		rdsp_driver.0 \
		qtn_driver.mac_only.0 \
		qtn_driver.cal_ruby.0 \
		qtn_driver.qtn_ruby.0 \
		qtn_driver.bb_only.0

ruby_fs: $(RUBY_TARGETS)
	@echo "#################################################"
	@echo "# Patched generic filesystem with Ruby files.   #"
	@echo "#################################################"

ruby_sysfs:
	@mkdir -p $(TARGET_DIR)/sys

ruby_hotplug:
	@cp -f $(RUBY_FS_PATCH_DIR)/hotplug $(TARGET_DIR)/sbin/hotplug
	@chmod 0755 $(TARGET_DIR)/sbin/hotplug

ruby_releasefile:
	@echo "$(shell whoami)@$(shell uname -n) on $(shell date --rfc-2822)" \
		>> $(TARGET_DIR)/release.txt

ruby_inetd:
	@cp -f $(RUBY_FS_PATCH_DIR)/S42inetd $(TARGET_DIR)/etc/init.d/S42inetd
	@cp -f $(RUBY_FS_PATCH_DIR)/inetd.conf $(TARGET_DIR)/etc/inetd.conf

ruby_interfaces_simple:
	@cp -f $(RUBY_FS_PATCH_DIR)/interfaces $(TARGET_DIR)/etc/network/interfaces

ruby_socipaddress:
	@echo -n "$(BR2_TARGET_RUBY_IPC_SUBNET).1" > $(TARGET_DIR)/etc/soc1ipcaddr
	@echo -n "$(BR2_TARGET_RUBY_IPC_SUBNET).2" > $(TARGET_DIR)/etc/soc2ipcaddr

ruby_interfaces:
	# Writing /etc/network/interfaces_soc1
	@echo "auto lo eth1_0 ipc0" > $(TARGET_DIR)/etc/network/interfaces_soc1
	@echo "iface lo inet loopback" >> $(TARGET_DIR)/etc/network/interfaces_soc1
	#@echo "iface eth1_0 inet dhcp" >> $(TARGET_DIR)/etc/network/interfaces_soc1
	@echo "iface eth1_0 inet static" >> $(TARGET_DIR)/etc/network/interfaces_soc1
	@echo "    address 192.168.11.2" >> $(TARGET_DIR)/etc/network/interfaces_soc1
	@echo "    netmask 255.255.255.0" >> $(TARGET_DIR)/etc/network/interfaces_soc1
	@echo "iface ipc0 inet static" >> $(TARGET_DIR)/etc/network/interfaces_soc1
	@echo "    address $(BR2_TARGET_RUBY_IPC_SUBNET).1" >> $(TARGET_DIR)/etc/network/interfaces_soc1
	@echo "    netmask 255.255.255.0" >> $(TARGET_DIR)/etc/network/interfaces_soc1
	@echo "    pointopoint $(BR2_TARGET_RUBY_IPC_SUBNET).2" >> $(TARGET_DIR)/etc/network/interfaces_soc1
	# Writing /etc/network/interfaces_soc2
	@echo "auto lo eth2_0 ipc0" > $(TARGET_DIR)/etc/network/interfaces_soc2
	@echo "iface lo inet loopback" >> $(TARGET_DIR)/etc/network/interfaces_soc2
	@echo "iface eth2_0 inet dhcp" >> $(TARGET_DIR)/etc/network/interfaces_soc2
	@echo "iface ipc0 inet static" >> $(TARGET_DIR)/etc/network/interfaces_soc2
	@echo "    address $(BR2_TARGET_RUBY_IPC_SUBNET).2" >> $(TARGET_DIR)/etc/network/interfaces_soc2
	@echo "    netmask 255.255.255.0" >> $(TARGET_DIR)/etc/network/interfaces_soc2
	@echo "    pointopoint $(BR2_TARGET_RUBY_IPC_SUBNET).1" >> $(TARGET_DIR)/etc/network/interfaces_soc2

ruby-initd-files-y =	\
	S30procsys	\
	S40jffs		\
	S40network	\
	S50PCIe		\
	S50Wireless	\
	S55rpc		\
	S80switch_ctrl	\
	S90maui		\
	S60debug	\
	S70monitor_temperature \
	S91dhclient	\
	S92httpd	\
	S41qevt \
	S10coredump	\
	S09qproc_mon

ruby-initd-files-$(BR2_PACKAGE_BOA)	+= S42boa

.PHONY: FORCE
$(TARGET_DIR)/etc/init.d/%: $(RUBY_FS_PATCH_DIR)/% FORCE
	# Copying ${<F}
	cp -f $< $@

ruby_initd: $(ruby-initd-files-y:%=$(TARGET_DIR)/etc/init.d/%)

ruby_power_tables:
	mkdir -p $(TARGET_DIR)/etc/power_tables
ifneq (${wildcard $(RUBY_POWER_TABLES_DIR)/power_table.conf},)
	@cp -f $(RUBY_POWER_TABLES_DIR)/power_table.conf $(TARGET_DIR)/etc/power_tables/
endif
ifneq (${wildcard $(RUBY_POWER_TABLES_DIR)/tx_power_*.txt},)
	@cp -f $(RUBY_POWER_TABLES_DIR)/tx_power_*.txt $(TARGET_DIR)/etc/power_tables/
endif


ruby_regulatory_db:
	@cp -f $(TOPDIR)/../host/region_utils/qtn_regulatory_db.bin $(TARGET_DIR)/etc/

ruby_syslogconf:
	# Writing /etc/syslog.conf
	@cp -f $(RUBY_FS_PATCH_DIR)/syslog.conf $(TARGET_DIR)/etc/syslog.conf

ruby_inittab:
	# Writing /etc/inittab
	@cp -f $(RUBY_FS_PATCH_DIR)/inittab $(TARGET_DIR)/etc/inittab

ruby_profile:
	# Writing /etc/profile
	@cp -f $(RUBY_FS_PATCH_DIR)/profile $(TARGET_DIR)/etc/profile

ruby_hostname:
	# Writing /etc/init.d/S10hostname
	@cp -f $(RUBY_FS_PATCH_DIR)/S10hostname $(TARGET_DIR)/etc/init.d/S10hostname

ruby_scripts:
	# Writing /scripts
	for subfolder in $(RUBY_SCRIPT_DIRS) ; do \
		if [ ! -d $(TARGET_DIR)/$$subfolder ]; then \
			mkdir -p $(TARGET_DIR)/$$subfolder; fi; \
	done
	for script in $(RUBY_STD_SCRIPTS) ; do \
		if [ -f $(RUBY_FS_PATCH_DIR)/scripts/$$script -o -d $(RUBY_FS_PATCH_DIR)/scripts/$$script ]; then \
			cp -rf $(RUBY_FS_PATCH_DIR)/scripts/$$script $(TARGET_DIR)/scripts ; \
		else \
			echo No such script $$script; fi; \
	done
	chmod a+x $(TARGET_DIR)/scripts -R
	if [ "$(board_platform)" = "topaz" -o "$(board_platform)" = "topaz_rfic6" ]; then \
		(cd $(TARGET_DIR)/scripts && ln -nsf topaz-cal start-cal && ln -nsf topaz-qvlan qvlan); \
	else \
		(cd $(TARGET_DIR)/scripts && ln -nsf ruby-cal start-cal && ln -nsf ruby-qvlan qvlan); \
	fi
	@cp -f $(RUBY_FS_PATCH_DIR)/hosts $(TARGET_DIR)/etc/hosts
	@echo "# Build time configuration variables" > $(TARGET_DIR)/scripts/build_config
	echo "STATELESS="$(BR2_TARGET_RUBY_STATELESS) >> $(TARGET_DIR)/scripts/build_config

libgcc:
	# copy libgcc shared library
	cp -f $(GCC_LIBDIR)/libgcc* $(TARGET_DIR)/lib/

chkimage:
	# copying host chkimage
	cp -f $(CHKIMAGE) $(TARGET_DIR)/bin/chkimage

ifeq (${GDBSERVER},1)
ruby_gdbserver:
	# Target gdbserver requested
	cp -f $(GDBSERVER_SRC) $(GDBSERVER_DEST)
else
ruby_gdbserver:
	# Target gdbserver not requested, deleting if present
	@if [ -f $(GDBSERVER_DEST) ] ; then	\
		rm -fv $(GDBSERVER_DEST) ;	\
	fi
endif

ruby_fs-source:

ruby_fs-clean:

ruby_fs-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_RUBY)),y)
TARGETS+=ruby_fs
endif
