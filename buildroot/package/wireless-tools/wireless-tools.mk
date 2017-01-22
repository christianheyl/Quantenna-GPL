#############################################################
#
# wireless-tools - Wireless Tools
#
#############################################################

WT_DIR=$(TOPDIR)/package/wireless-tools

.PHONY: wireless-tools

WIRELESS_TOOLS_VER:=29
WIRELESS_TOOLS_SUBVER:=

WIRELESS_TOOLS_BUILD_DIR=$(WT_DIR)/wireless_tools.$(WIRELESS_TOOLS_VER)

wireless-tools:
	$(MAKE) -C $(WIRELESS_TOOLS_BUILD_DIR) PREFIX="$(TARGET_DIR)" \
		CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS)" \
		all install-dynamic install-bin

wireless-tools-clean:
	-$(MAKE) -C $(WIRELESS_TOOLS_BUILD_DIR) clean

wireless-tools-dirclean: wireless-tools-clean

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_WIRELESS_TOOLS)),y)
TARGETS+=wireless-tools
endif
