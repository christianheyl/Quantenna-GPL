#############################################################
#
# QWEADAPTER
#
#############################################################
ADAPTER_DIR=${shell ls $(BASE_DIR)/package/qweadapter/* -dF | grep '/$$'}

qweadapter:
ifneq ($(strip $(ADAPTER_DIR)),)
	$(MAKE) -C $(ADAPTER_DIR) ARCH=arc CROSS_COMPILE=$(TARGET_CROSS) TARGET_DIR=$(TARGET_DIR) all install
endif

qweadapter-clean:
ifneq ($(strip $(ADAPTER_DIR)),)
	$(MAKE) -C $(ADAPTER_DIR) ARCH=arc CROSS_COMPILE=$(TARGET_CROSS) TARGET_DIR=$(TARGET_DIR) clean
endif

qweadapter-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_QWE_ADAPTER)),y)
TARGETS+=qweadapter
endif
