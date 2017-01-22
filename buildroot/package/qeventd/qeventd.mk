#############################################################
#
# Quantenna private connection managment daemon
#
#############################################################

QM_DIR=$(TOPDIR)/package/qeventd

.PHONY: qeventd

QEVENTD_VER:=1.0
QEVENTD_SUBVER:=

QEVENTD_BUILD_DIR=$(QM_DIR)/qeventd-$(QEVENTD_VER)

qeventd:
	$(MAKE) -C $(QEVENTD_BUILD_DIR) PREFIX="$(TARGET_DIR)" \
		CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS)" \
		all install

qeventd-clean:
	-$(MAKE) -C $(QEVENTD_BUILD_DIR) clean

qeventd-dirclean: qeventd-clean

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_QEVENTD)),y)
TARGETS+=qeventd
endif
