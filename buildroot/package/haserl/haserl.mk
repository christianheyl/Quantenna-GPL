#############################################################
#
# haserl
#
#############################################################

HASERL_VERSION=0.9.29
HASERL_SOURCE=haserl-$(HASERL_VERSION).tar.gz
HASERL_SITE=http://downloads.sourceforge.net/project/haserl/haserl-devel
HASERL_DIR=$(BUILD_DIR)/${shell basename $(HASERL_SOURCE) .tar.gz}
HASERL_WORKDIR=$(BUILD_DIR)/haserl-$(HASERL_VERSION)
HASERL_CAT:=$(ZCAT)
HASERL_INSTALL=$(TARGET_DIR)/usr/bin/haserl

$(DL_DIR)/$(HASERL_SOURCE):
	$(WGET) -P $(DL_DIR) $(HASERL_SITE)/$(HASERL_SOURCE)

$(HASERL_DIR)/.unpacked: $(DL_DIR)/$(HASERL_SOURCE)
	$(HASERL_CAT) $(DL_DIR)/$(HASERL_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch $(HASERL_DIR)/.unpacked

$(HASERL_DIR)/.configured: $(HASERL_DIR)/.unpacked
	(cd $(HASERL_DIR); rm -rf config.cache; \
		CFLAGS="$(TARGET_CFLAGS)" $(TARGET_CONFIGURE_OPTS) \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
		--sysconfdir=/etc \
	);
	touch $(HASERL_DIR)/.configured

.PHONY: FORCE

$(HASERL_WORKDIR)/src/haserl: $(HASERL_DIR)/.configured FORCE
	$(MAKE) CC=$(TARGET_CC) -C $(HASERL_WORKDIR)

$(HASERL_INSTALL): $(HASERL_WORKDIR)/src/haserl
	cp $< $@
	$(STRIP) $@

haserl:	uclibc $(HASERL_INSTALL)

haserl-source: $(DL_DIR)/$(HASERL_SOURCE)

haserl-clean:
	@if [ -d $(HASERL_WORKDIR)/Makefile ] ; then \
		$(MAKE) -C $(HASERL_WORKDIR) clean ; \
	fi;

haserl-dirclean:
	rm -rf $(HASERL_DIR) $(HASERL_WORKDIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_HASERL)),y)
TARGETS+=haserl
endif
