
.PHONY: q_utils q_event

Q_UTIL_DIR=$(TOPDIR)/package/q_utils

Q_UTILS_VER:=1.0.1

Q_UTILS_BUILD_DIR=$(Q_UTIL_DIR)/q_utils-$(Q_UTILS_VER)
EXTRA_WARNINGS= -Wall -Wshadow -Werror

q_utils:
	$(MAKE) -C $(Q_UTILS_BUILD_DIR) PREFIX="$(TARGET_DIR)" \
		CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS) $(EXTRA_WARNINGS)" \
		install

q_event: wireless-tools
	$(MAKE) -C $(Q_UTILS_BUILD_DIR) PREFIX="$(TARGET_DIR)" \
		LDFLAGS="-L$(TARGET_DIR)/lib" \
		CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS) $(EXTRA_WARNINGS) \
		-I../../wireless-tools/wireless_tools.29  \
		-I../../../../drivers/include/shared -I." \
		install_qevent

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_Q_UTILS)),y)
TARGETS+=q_utils q_event
endif


q_utils_clean:
	$(MAKE) -C $(Q_UTILS_BUILD_DIR) clean

q_utils_distclean: q_utils_clean

