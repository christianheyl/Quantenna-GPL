#############################################################
#
# igmpproxy
#
#############################################################

IGMPPROXY_VERSION:=0.1
IGMPPROXY_SOURCE:=igmpproxy-$(IGMPPROXY_VERSION).tar.gz
IGMPPROXY_SITE:=http://sourceforge.net/projects/igmpproxy/files/igmpproxy/0.1/igmpproxy-0.1.tar.gz/download
IGMPPROXY_DIR:=$(BUILD_DIR)/igmpproxy-$(IGMPPROXY_VERSION)
IGMPPROXY_CAT:=$(ZCAT)

$(DL_DIR)/$(IGMPPROXY_SOURCE):
	$(WGET) -P $(DL_DIR) $(IGMPPROXY_SITE)/

$(IGMPPROXY_DIR)/.unpacked: $(DL_DIR)/$(IGMPPROXY_SOURCE)
	$(IGMPPROXY_CAT) $(DL_DIR)/$(IGMPPROXY_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(IGMPPROXY_DIR) package/igmpproxy/ igmpproxy\*.patch
	touch $(IGMPPROXY_DIR)/.unpacked

$(IGMPPROXY_DIR)/.configured: $(IGMPPROXY_DIR)/.unpacked
	(	cd $(IGMPPROXY_DIR); rm -rf config.cache;  \
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
		--includedir=$(STAGING_DIR)/include \
	);
	touch $(IGMPPROXY_DIR)/.configured

$(IGMPPROXY_DIR)/src/igmpproxy: $(IGMPPROXY_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(IGMPPROXY_DIR)
	-$(STRIP) --strip-unneeded $(IGMPPROXY_DIR)/src/igmpproxy

$(TARGET_DIR)/usr/bin/igmpproxy: $(IGMPPROXY_DIR)/src/igmpproxy
	cp $(IGMPPROXY_DIR)/src/igmpproxy $(TARGET_DIR)/usr/bin/igmpproxy

igmpproxy: $(TARGET_DIR)/usr/bin/igmpproxy

igmpproxy-source: $(DL_DIR)/$(IGMPPROXY_SOURCE)

igmpproxy-clean:
	@if [ -d $(IGMPPROXY_KDIR)/Makefile ] ; then \
		$(MAKE) -C $(IGMPPROXY_DIR) clean ; \
	fi;

igmpproxy-dirclean:
	rm -rf $(IGMPPROXY_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_IGMPPROXY)),y)
TARGETS+=igmpproxy
endif
