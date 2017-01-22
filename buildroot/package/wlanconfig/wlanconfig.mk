#############################################################
#
# wlanconfig
#
#############################################################

WLANCONFIG_SOURCE:=wlanconfig.tar.bz2
WLANCONFIG_BUILD_DIR=$(BUILD_DIR)/wlanconfig

$(WLANCONFIG_BUILD_DIR)/.unpacked:
		$(BZCAT) $(DL_DIR)/$(WLANCONFIG_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
		sed	-i -e s:'strip':'$(STRIP)':g $(WIRELESS_TOOLS_BUILD_DIR)/Makefile
		touch $(WLANCONFIG_BUILD_DIR)/.unpacked

$(WLANCONFIG_BUILD_DIR)/.configured: $(WLANCONFIG_BUILD_DIR)/.unpacked
		touch $(WLANCONFIG_BUILD_DIR)/.configured

$(WLANCONFIG_BUILD_DIR)/wlanconfig: $(WLANCONFIG_BUILD_DIR)/.configured
		$(MAKE) -C $(WLANCONFIG_BUILD_DIR) \
		CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS)" \
		wlanconfig

$(TARGET_DIR)/sbin/wlanconfig: $(WLANCONFIG_BUILD_DIR)/wlanconfig
		$(MAKE) -C $(WLANCONFIG_BUILD_DIR) \
		CROSS_COMPILE=$(TARGET_CROSS) CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS)" DESTDIR=$(TARGET_DIR) BINDIR="/sbin" \
		install

wlanconfig: $(TARGET_DIR)/sbin/wlanconfig

wlanconfig-source:

wlanconfig-clean:
	$(MAKE) DESTDIR=$(TARGET_DIR) CC=$(TARGET_CC) -C $(WLANCONFIG_BUILD_DIR) uninstall
	-$(MAKE) -C $(WLANCONFIG_BUILD_DIR) clean

wlanconfig-dirclean:
	rm -rf $(WLANCONFIG_BUILD_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_WLANCONFIG)),y)
TARGETS+=wlanconfig
endif
