#############################################################
#
# libnet
#
#############################################################

LIBNET_SOURCE:=libnet-1.0.2a.tar.gz
LIBNET_SITE:=http://packetfactory.openwall.net/libnet/dist/deprecated/libnet-1.0.2a.tar.gz
LIBNET_DIR:=$(BUILD_DIR)/Libnet-1.0.2a/
LIBNET_CAT:=$(ZCAT)

$(DL_DIR)/$(LIBNET_SOURCE):
	$(WGET) -P $(DL_DIR) $(LIBNET_SITE)

$(LIBNET_DIR)/.unpacked: $(DL_DIR)/$(LIBNET_SOURCE)
	$(LIBNET_CAT) $(DL_DIR)/$(LIBNET_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(LIBNET_DIR) package/libnet/ libnet\*.patch
	touch $(LIBNET_DIR)/.unpacked

$(LIBNET_DIR)/.configured: $(LIBNET_DIR)/.unpacked
	(	cd $(LIBNET_DIR); rm -rf config.cache;  \
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
	touch $(LIBNET_DIR)/.configured

$(LIBNET_DIR)/src/libnet.a: $(LIBNET_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(LIBNET_DIR)
	-$(STRIP) --strip-unneeded $(LIBNET_DIR)/src/libnet.a

$(STAGING_DIR)/lib/libnet.a: $(LIBNET_DIR)/src/libnet.a
	$(MAKE) DESTDIR=$(STAGING_DIR) -C $(LIBNET_DIR) install

$(STAGING_DIR)/bin/libnet-config: $(LIBNET_DIR)/libnet-config
	cp $(LIBNET_DIR)/libnet-config $(STAGING_DIR)/bin/

$(STAGING_DIR)/include/libnet.h: $(LIBNET_DIR)/include/libnet.h
	cp $(LIBNET_DIR)/include/libnet.h  $(STAGING_DIR)/include/
	cp -R $(LIBNET_DIR)/include/libnet/ $(STAGING_DIR)/include/

libnet: $(STAGING_DIR)/lib/libnet.a $(STAGING_DIR)/bin/libnet-config $(STAGING_DIR)/include/libnet.h

libnet-source: $(DL_DIR)/$(LIBNET_SOURCE)

libnet-clean:
	@if [ -d $(LIBNET_KDIR)/Makefile ] ; then \
		$(MAKE) -C $(LIBNET_DIR) clean ; \
	fi;

libnet-dirclean:
	rm -rf $(LIBNET_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIBNET)),y)
TARGETS+=libnet
endif
