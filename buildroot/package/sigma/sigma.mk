#############################################################
#
# Sigma
#
#############################################################
SIGMA_VERSION:=8.1.1
SIGMA_DIR:=$(TOPDIR)/package/sigma/wfa/Sigma_Sample_DUT_Code-Linux_v$(SIGMA_VERSION)
QNTN_SIGMA_DIR:=$(TOPDIR)/package/sigma/quantenna
SIGMA_SOURCE:=Sigma_Sample_DUT_Code-Linux_v$(SIGMA_VERSION).tar.gz

$(DL_DIR)/$(SIGMA_SOURCE):
	@echo "download is done, DL_DIR is $(DL_DIR)"


env_setup += STAGING_DIR="$(STAGING_DIR)"
env_setup += TARGET_DIR="$(TARGET_DIR)"
env_setup += TARGET_CFLAGS="$(TARGET_CFLAGS)"
env_setup += CC="$(TARGET_CC)"

$(SIGMA_DIR)/.unpacked: $(DL_DIR)/$(SIGMA_SOURCE)
	@echo "BUILD_DIR is $(BUILD_DIR)"
	$(ZCAT) $(DL_DIR)/$(SIGMA_SOURCE) | tar -C $(TOPDIR)/package/sigma/wfa $(TAR_OPTIONS) -
	patch -d $(SIGMA_DIR) -p1 < $(TOPDIR)/package/sigma/wfa/wft_to_qtn.patch
	touch $(SIGMA_DIR)/.unpacked

sigma: $(SIGMA_DIR)/.unpacked
	CC=$(TARGET_CC) $(MAKE) -C $(SIGMA_DIR)
	$(MAKE) -C $(QNTN_SIGMA_DIR) $(env_setup)
	mkdir -p $(TARGET_DIR)/usr/local/sbin
	cp -f $(SIGMA_DIR)/scripts/* $(TARGET_DIR)/usr/local/sbin/
	cp -f $(QNTN_SIGMA_DIR)/qtn_dut $(TARGET_DIR)/usr/bin/
	cp -f $(QNTN_SIGMA_DIR)/qtn_ca $(TARGET_DIR)/usr/bin/
	$(STRIP) --strip-unneeded $(TARGET_DIR)/usr/bin/qtn_dut
	$(STRIP) --strip-unneeded $(TARGET_DIR)/usr/bin/qtn_ca
	mkdir -p  $(TARGET_DIR)/scripts
	cat $(SIGMA_DIR)/../../sigma_ca | sed s/WFA_CA_PORT_PLACEHOLDER/$(BR2_PACKAGE_SIGMA_CA_PORT)/ | sed s/WFA_DUT_PORT_PLACEHOLDER/$(BR2_PACKAGE_SIGMA_DUT_PORT)/  > $(TARGET_DIR)/scripts/sigma_ca
	chmod +x $(TARGET_DIR)/scripts/sigma_ca


sigma-clean:
	@rm -rf $(SIGMA_DIR)
	$(MAKE) -C $(QNTN_SIGMA_DIR) $(env_setup) clean

sigma-dirclean: sigma-clean

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_SIGMA)),y)
TARGETS+=sigma
endif

