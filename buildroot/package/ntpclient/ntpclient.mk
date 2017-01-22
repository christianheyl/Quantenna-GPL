#############################################################
#
# NTP client
#
#############################################################


NTPCLIENT_VER=2010
NTPCLIENT_SOURCE:=ntpclient_$(NTPCLIENT_VER).tar.gz
NTPCLIENT_BUILD_DIR=$(BUILD_DIR)/ntpclient-$(NTPCLIENT_VER)

$(NTPCLIENT_BUILD_DIR)/.unpacked:
	$(ZCAT) $(DL_DIR)/$(NTPCLIENT_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch $(NTPCLIENT_BUILD_DIR)/.unpacked

$(NTPCLIENT_BUILD_DIR)/ntpclient: $(NTPCLIENT_BUILD_DIR)/.unpacked
	$(MAKE) -C $(NTPCLIENT_BUILD_DIR) CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
	BUILD_DIR="$(BUILD_DIR)" TOOLCHAIN_DIR="$(TOOLCHAIN_EXTERNAL_PATH)/$(TOOLCHAIN_EXTERNAL_PREFIX)"

ntpclient: $(NTPCLIENT_BUILD_DIR)/ntpclient
	cp -fa $(NTPCLIENT_BUILD_DIR)/ntpclient $(TARGET_DIR)/usr/sbin/
	cp -fa $(TOPDIR)/package/ntpclient/ntpclient.conf $(TARGET_DIR)/etc/

ntpclient-clean:
	-$(MAKE) -C $(NTPCLIENT_BUILD_DIR) clean

ntpclient-dirclean:
	rm -rf $(NTPCLIENT_BUILD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_NTPCLIENT)),y)
TARGETS+=ntpclient
endif

