#############################################################
#
# sysstat
#
#############################################################
SYSSTAT_VERSION = 10.0.0
SYSSTAT_SOURCE_URL=http://pagesperso-orange.fr/sebastien.godard/
SYSSTAT_BUILD_DIR=$(BASE_DIR)/package/sysstat/sysstat-$(SYSSTAT_VERSION)

sysstat: _configure
	$(MAKE) CC=$(TARGET_CC) -C $(SYSSTAT_BUILD_DIR)
	cp -af ${SYSSTAT_BUILD_DIR}/mpstat ${TARGET_DIR}/usr/sbin/
	cp -af ${SYSSTAT_BUILD_DIR}/pidstat ${TARGET_DIR}/usr/sbin/
	
_configure:
	if  [ ! -f $(SYSSTAT_BUILD_DIR)/Makefile ] || [ $(BASE_DIR)/package/sysstat/sysstat.mk -nt $(SYSSTAT_BUILD_DIR)/Makefile ; then \
		cd $(SYSSTAT_BUILD_DIR) && \
		$(SYSSTAT_BUILD_DIR)/configure \
		--target=$(REAL_GNU_TARGET_NAME) \
		--host=$(REAL_GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=$(TARGET_DIR) \
		--exec-prefix=$(TARGET_DIR) \
		--disable-nls \
		--disable-documentation \
			; \
	fi
sysstat-clean sysstat-distclean:
	@if [ -f $(SYSSTAT_BUILD_DIR)/Makefile ] ; then \
		$(MAKE) -C $(SYSSTAT_BUILD_DIR) $@; \
	fi
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_SYSSTAT)),y)
TARGETS+=sysstat
endif
