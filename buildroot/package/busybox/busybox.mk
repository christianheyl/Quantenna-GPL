#############################################################
#
# busybox
#
#############################################################

BUSYBOX_VER:=1.10.3
BUSYBOX_DIR:=$(TOPDIR)/package/busybox/busybox-$(BUSYBOX_VER)
BUSYBOX_CONFIG_FILE:=$(BUSYBOX_DIR)/.config.unpatched.current
BUSYBOX_PATCH_LIST:=busybox.mixed-rules.patch busybox.ping_01-option-W.patch busybox.ping_02-option-i.patch

busybox-source: $(DL_DIR)/$(BUSYBOX_SOURCE) $(BUSYBOX_CONFIG_FILE) dependencies

# Patch vanilla busybox files. This is not consistent across all busybox package, several modified
# files are kept in VCS in full, and simply overwrite vanilla busybox files. That should be fixed in the feature.
$(BUSYBOX_DIR)/.source:
	toolchain/patch-kernel.sh $(BUSYBOX_DIR) package/busybox $(BUSYBOX_PATCH_LIST)
	touch $@

$(BUSYBOX_DIR)/.configured: $(BUSYBOX_CONFIG_FILE) | $(BUSYBOX_DIR)/.source
	cp -f $(BUSYBOX_CONFIG_FILE) $(BUSYBOX_DIR)/.config
	$(SED) s,^CONFIG_PREFIX=.*,CONFIG_PREFIX=\"$(TARGET_DIR)\", \
		$(BUSYBOX_DIR)/.config ;
ifeq ($(BR2_LARGEFILE),y)
	$(SED) "s/^.*CONFIG_LFS.*/CONFIG_LFS=y/;" $(BUSYBOX_DIR)/.config
else
	$(SED) "s/^.*CONFIG_LFS.*/CONFIG_LFS=n/;" $(BUSYBOX_DIR)/.config
	$(SED) "s/^.*FDISK_SUPPORT_LARGE_DISKS.*/FDISK_SUPPORT_LARGE_DISKS=n/;" $(BUSYBOX_DIR)/.config
endif
	$(SED) "s/^.*CONFIG_HTTPD.*/CONFIG_HTTPD=n/;" $(BUSYBOX_DIR)/.config
ifeq ($(BR2_PACKAGE_BUSYBOX_SKELETON),y)
	# force mdev on
	$(SED) "s/^.*CONFIG_MDEV.*/CONFIG_MDEV=y/" $(BUSYBOX_DIR)/.config
endif
ifeq ($(BR2_INET_IPV6),y)
	$(SED) "s/^.*CONFIG_PING6.*/CONFIG_PING6=y/" $(BUSYBOX_DIR)/.config
endif
	yes "" | $(MAKE1) CC=$(TARGET_CC) CROSS_COMPILE="$(TARGET_CROSS)" \
		CROSS="$(TARGET_CROSS)" -C $(BUSYBOX_DIR) oldconfig
	touch $@


$(BUSYBOX_DIR)/busybox: $(BUSYBOX_DIR)/.configured
	$(MAKE1) CC=$(TARGET_CC) CROSS_COMPILE="$(TARGET_CROSS)" \
		CROSS="$(TARGET_CROSS)" PREFIX="$(TARGET_DIR)" \
		ARCH=$(KERNEL_ARCH) \
		EXTRA_CFLAGS="$(TARGET_CFLAGS)" -C $(BUSYBOX_DIR)
ifeq ($(BR2_PREFER_IMA)$(BR2_PACKAGE_BUSYBOX_SNAPSHOT),yy)
	rm -f $@
	$(MAKE1) CC=$(TARGET_CC) CROSS_COMPILE="$(TARGET_CROSS)" \
		CROSS="$(TARGET_CROSS)" PREFIX="$(TARGET_DIR)" \
		ARCH=$(KERNEL_ARCH) STRIP="$(STRIP)" \
		EXTRA_CFLAGS="$(TARGET_CFLAGS)" -C $(BUSYBOX_DIR) \
		-f scripts/Makefile.IMA
endif

$(TARGET_DIR)/bin/busybox: $(BUSYBOX_DIR)/busybox
ifeq ($(BR2_PACKAGE_BUSYBOX_INSTALL_SYMLINKS),y)
	$(MAKE1) CC=$(TARGET_CC) CROSS_COMPILE="$(TARGET_CROSS)" \
		CROSS="$(TARGET_CROSS)" PREFIX="$(TARGET_DIR)" \
		ARCH=$(KERNEL_ARCH) CONFIG_PREFIX="$(TARGET_DIR)" \
		EXTRA_CFLAGS="$(TARGET_CFLAGS)" -C $(BUSYBOX_DIR) install
else
	install -D -m 0755 $(BUSYBOX_DIR)/busybox $(TARGET_DIR)/bin/busybox
endif
	# Just in case
	-chmod a+x $(TARGET_DIR)/usr/share/udhcpc/default.script

busybox: uclibc $(TARGET_DIR)/bin/busybox

busybox-menuconfig: busybox-source $(BUSYBOX_DIR)/.configured
	$(MAKE1) __TARGET_ARCH=$(ARCH) -C $(BUSYBOX_DIR) menuconfig
	cp -f $(BUSYBOX_DIR)/.config $(BUSYBOX_CONFIG_FILE)

busybox-clean:
	rm -f $(TARGET_DIR)/bin/busybox
	-$(MAKE1) -C $(BUSYBOX_DIR) clean

busybox-dirclean:
	rm -rf $(BUSYBOX_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_BUSYBOX)),y)
TARGETS+=busybox
endif
