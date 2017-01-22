#############################################################
#
# testipc
#
#############################################################

TESTIPC_SOURCE=testipc.tar.gz
TESTIPC_BUILD_DIR=$(BUILD_DIR)/testipc

$(TESTIPC_BUILD_DIR)/.unpacked:
		$(ZCAT) $(DL_DIR)/$(TESTIPC_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
		sed	-i -e s:'strip':'$(STRIP)':g $(TESTIPC_BUILD_DIR)/Makefile
		touch $(TESTIPC_BUILD_DIR)/.unpacked

$(TESTIPC_BUILD_DIR)/.configured: $(TESTIPC_BUILD_DIR)/.unpacked
		touch $(TESTIPC_BUILD_DIR)/.configured

$(TESTIPC_BUILD_DIR)/testipc: $(TESTIPC_BUILD_DIR)/.configured
		$(MAKE) -C $(TESTIPC_BUILD_DIR) \
		CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS)" \
		testipc

$(TARGET_DIR)/sbin/testipc: $(TESTIPC_BUILD_DIR)/testipc
		$(MAKE) -C $(TESTIPC_BUILD_DIR) \
		CROSS_COMPILE=$(TARGET_CROSS) CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS)" DESTDIR=$(TARGET_DIR) BINDIR="/sbin" \
		install

testipc: $(TARGET_DIR)/sbin/testipc

testipc-source:

testipc-clean:
	$(MAKE) DESTDIR=$(TARGET_DIR) CC=$(TARGET_CC) -C $(TESTIPC_BUILD_DIR) uninstall
	-$(MAKE) -C $(TESTIPC_BUILD_DIR) clean

testipc-dirclean:
	rm -rf $(TESTIPC_BUILD_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_TESTIPC)),y)
TARGETS+=testipc
endif
