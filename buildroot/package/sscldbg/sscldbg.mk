#############################################################
#
# sscldbg
#
#############################################################
SSCLDBG_VERSION=sscldbg-0.1
SSCLDBG_BASE_DIR=$(TOPDIR)/package/sscldbg
SSCLDBG_DIR=$(SSCLDBG_BASE_DIR)/$(SSCLDBG_VERSION)

$(SSCLDBG_DIR)/sscldbg:
	cp -f $(SSCLDBG_BASE_DIR)/sscldbg.config $(SSCLDBG_DIR)/.config
	$(MAKE) -C $(SSCLDBG_DIR) sscldbg CC=$(TARGET_CC)

$(TARGET_DIR)/usr/sbin/sscldbg: $(SSCLDBG_DIR)/sscldbg
	cp -af $(SSCLDBG_DIR)/sscldbg $(TARGET_DIR)/usr/sbin/

sscldbg: $(TARGET_DIR)/usr/sbin/sscldbg

sscldbg-clean:
	-$(MAKE) -C $(SSCLDBG_DIR) clean

sscldbg-dirclean: sscldbg-clean
	rm -f $(SSCLDBG_DIR)/.config
	rm -f $(TARGET_DIR)/usr/sbin/sscldbg

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_SSCLDBG)),y)
TARGETS+=sscldbg
endif
