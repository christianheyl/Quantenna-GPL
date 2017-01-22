#############################################################
#
# wpa_supplicant
#
WPASUPPLICANT_VERSION_NUM=1.1
WPASUPPLICANT_VERSION=wpa_supplicant-$(WPASUPPLICANT_VERSION_NUM)
WPASUPPLICANT_BASE_DIR=$(TOPDIR)/package/wpa_supplicant
WPASUPPLICANT_DIR=$(WPASUPPLICANT_BASE_DIR)/$(WPASUPPLICANT_VERSION)/wpa_supplicant

.PHONY: FORCE

$(WPASUPPLICANT_DIR)/wpa_supplicant: FORCE
	cp -f $(WPASUPPLICANT_BASE_DIR)/$(WPASUPPLICANT_VERSION).config $(WPASUPPLICANT_DIR)/.config
	$(MAKE) -C $(WPASUPPLICANT_DIR) all CC=$(TARGET_CC)

$(TARGET_DIR)/usr/sbin/wpa_supplicant: $(WPASUPPLICANT_DIR)/wpa_supplicant
	# Copy wpa_supplicant
	cp -af $< $@

wpa_supplicant: $(TARGET_DIR)/usr/sbin/wpa_supplicant

wpa_supplicant-clean:
	-$(MAKE) -C $(WPASUPPLICANT_DIR) clean

wpa_supplicant-dirclean: wpa_supplicant-clean
	#rm -f $(WPASUPPLICANT_DIR)/.config
	rm -f $(TARGET_DIR)/usr/sbin/wpa_supplicant

#############################################################
#
# wpa_cli
#
#############################################################

$(WPASUPPLICANT_DIR)/wpa_cli: $(WPASUPPLICANT_SRC)
	$(MAKE) -C $(WPASUPPLICANT_DIR) all CC=$(TARGET_CC)

$(TARGET_DIR)/usr/sbin/wpa_cli: $(WPASUPPLICANT_DIR)/wpa_cli
	# Copy wpa_cli
	cp -af $(WPASUPPLICANT_DIR)/wpa_cli $(TARGET_DIR)/usr/sbin/

wpa_cli: $(TARGET_DIR)/usr/sbin/wpa_cli

#############################################################
#
# wdskey
#
#############################################################

$(WPASUPPLICANT_DIR)/wdskey: $(WPASUPPLICANT_SRC)
	$(MAKE) -C $(WPASUPPLICANT_DIR) all CC=$(TARGET_CC)

$(TARGET_DIR)/usr/sbin/wdskey: $(WPASUPPLICANT_DIR)/wdskey
	# Copy wdskey
	cp -af $(WPASUPPLICANT_DIR)/wdskey $(TARGET_DIR)/usr/sbin/

wdskey: $(TARGET_DIR)/usr/sbin/wdskey

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_WPASUPPLICANT)),y)
TARGETS+=wpa_supplicant
# Targets used for debugging
TARGETS+=wpa_cli
TARGETS+=wdskey
endif

