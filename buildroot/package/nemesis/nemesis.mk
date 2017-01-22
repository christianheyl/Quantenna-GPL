#############################################################
#
# nemesis
#
#############################################################

NEMESIS_VERSION:=1.4
NEMESIS_SOURCE:=nemesis-$(NEMESIS_VERSION).tar.gz
NEMESIS_SITE:=http://sourceforge.net/projects/nemesis/files/nemesis/$(NEMESIS_VERSION)/nemesis-$(NEMESIS_VERSION).tar.gz/download
NEMESIS_DIR:=$(BUILD_DIR)/nemesis-$(NEMESIS_VERSION)
NEMESIS_CAT:=$(ZCAT)

$(DL_DIR)/$(NEMESIS_SOURCE):
	$(WGET) -P $(DL_DIR) $(NEMESIS_SITE)/

$(NEMESIS_DIR)/.unpacked: $(DL_DIR)/$(NEMESIS_SOURCE)
	$(NEMESIS_CAT) $(DL_DIR)/$(NEMESIS_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(NEMESIS_DIR) package/nemesis/ nemesis\*.patch
	touch $(NEMESIS_DIR)/.unpacked

$(NEMESIS_DIR)/.configured: $(NEMESIS_DIR)/.unpacked
	(	cd $(NEMESIS_DIR); rm -rf config.cache;  \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		ac_cv_func_malloc_0_nonnull=yes \
		./configure \
		--with-gnu-ld \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--disable-ipv6 \
		--disable-dependency-tracking \
		--disable-web100 \
		--prefix=/usr \
		--exec-prefix=/usr \
		--bindir=/usr/bin \
		--sbindir=/usr/sbin \
		--libexecdir=/usr/sbin \
		--sysconfdir=/etc \
		--datadir=/usr/share \
		--localstatedir=/var \
		--mandir=/usr/man \
		--infodir=/usr/info \
		--with-libnet-includes=$(STAGING_DIR)/include \
		--with-libnet-libraries=$(STAGING_DIR)/lib \
		--includedir=$(STAGING_DIR)/include \
	);
	touch $(NEMESIS_DIR)/.configured

$(NEMESIS_DIR)/src/nemesis: $(NEMESIS_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(NEMESIS_DIR)
	-$(STRIP) --strip-unneeded $(NEMESIS_DIR)/src/nemesis

$(TARGET_DIR)/usr/bin/nemesis: $(NEMESIS_DIR)/src/nemesis
	cp $(NEMESIS_DIR)/src/nemesis $(TARGET_DIR)/usr/bin/nemesis

nemesis: $(TARGET_DIR)/usr/bin/nemesis

nemesis-source: $(DL_DIR)/$(NEMESIS_SOURCE)

nemesis-clean:
	@if [ -d $(NEMESIS_KDIR)/Makefile ] ; then \
		$(MAKE) -C $(NEMESIS_DIR) clean ; \
	fi;

nemesis-dirclean:
	rm -rf $(NEMESIS_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_NEMESIS)),y)
TARGETS+=nemesis
endif
