#############################################################
#
# QWE
#
#############################################################
QWE_DIR=$(BASE_DIR)/package/qwe/files

qwe:
	$(MAKE) -C $(BASE_DIR)/package/qwe/src \
		CROSS_COMPILE=$(TARGET_CROSS) TARGET_DIR=$(TARGET_DIR) install
	install -d $(TARGET_DIR)/bin
	install -d $(TARGET_DIR)/etc/init.d
	install -m0755 $(QWE_DIR)/qweconfig $(TARGET_DIR)/bin/
	install -m0755 $(QWE_DIR)/qweaction $(TARGET_DIR)/bin/
	install -m0755 $(QWE_DIR)/qweeventd $(TARGET_DIR)/bin/
	install -m0755 $(QWE_DIR)/sync_wps_band $(TARGET_DIR)/bin/
	install -m0755 $(QWE_DIR)/update_pm_state $(TARGET_DIR)/bin/
	install -m0644 $(QWE_DIR)/qwe.conf $(TARGET_DIR)/etc/
	install -m0755 $(QWE_DIR)/S55qwe $(TARGET_DIR)/etc/init.d/

qwe-clean:
	$(MAKE) -C $(BASE_DIR)/package/qwe/src TARGET_DIR=$(TARGET_DIR) clean
	rm -f $(TARGET_DIR)/bin/qweconfig
	rm -f $(TARGET_DIR)/bin/qweaction
	rm -f $(TARGET_DIR)/bin/qweeventd
	rm -f $(TARGET_DIR)/bin/sync_wps_band
	rm -f $(TARGET_DIR)/bin/update_pm_state
	rm -f $(TARGET_DIR)/etc/qwe.conf
	rm -f $(TARGET_DIR)/etc/init.d/S55qwe

qwe-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_QWE_PAL)),y)
TARGETS+=qwe
endif
