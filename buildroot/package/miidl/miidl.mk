#############################################################
#
# miidl
#
#############################################################
MIIDL_DIR=$(TOPDIR)/package/miidl/miidl-0.1
MIIDL_BUILD=$(MIIDL_DIR)/build/$(shell basename $(TARGET_CC))/miidl
MIIDL_TARGET=$(TARGET_DIR)/usr/sbin/miidl

.PHONY: FORCE

$(MIIDL_BUILD): FORCE
	$(MAKE) -C $(MIIDL_DIR) all CC=$(TARGET_CC)

$(MIIDL_TARGET): $(MIIDL_BUILD)
	cp -af $< $@

miidl: $(MIIDL_TARGET)

miidl-clean:
	$(MAKE) -C $(MIIDL_DIR) clean

miidl-dirclean:	miidl-clean

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_MIIDL)),y)
TARGETS+=miidl
endif

