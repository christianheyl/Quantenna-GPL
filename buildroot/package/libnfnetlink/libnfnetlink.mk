################################################################################
#
# libnfnetlink
#
################################################################################

LIBNFNETLINK_VERSION:=1.0.0
LIBNFNETLINK_SOURCE:=libnfnetlink-$(LIBNFNETLINK_VERSION).tar.bz2
LIBNFNETLINK_SITE:=http://www.netfilter.org/projects/libnfnetlink/files/
LIBNFNETLINK_DIR:=$(BUILD_DIR)/libnfnetlink-$(LIBNFNETLINK_VERSION)
LIBNFNETLINK_CAT:=$(BZCAT)

$(DL_DIR)/$(LIBNFNETLINK_SOURCE):
	$(WGET) -P $(DL_DIR) $(LIBNFNETLINK_SITE)/$(LIBNFNETLINK_SOURCE)

$(LIBNFNETLINK_DIR)/.unpacked: $(DL_DIR)/$(LIBNFNETLINK_SOURCE)
	$(LIBNFNETLINK_CAT) $(DL_DIR)/$(LIBNFNETLINK_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(LIBNFNETLINK_DIR) package/libnfnetlink/ libnfnetlink-*.patch
	touch $(LIBNFNETLINK_DIR)/.unpacked

$(LIBNFNETLINK_DIR)/.configured: $(LIBNFNETLINK_DIR)/.unpacked
	#(cd $(LIBNFNETLINK_DIR); \
	(cd $(LIBNFNETLINK_DIR); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		./configure \
		--build=$(GNU_HOST_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--target=$(GNU_TARGET_NAME) \
		--prefix=/usr \
		--exec-prefix=/usr \
		--bindir=/usr/bin \
		--sbindir=/usr/sbin \
		--libdir=/lib \
		--libexecdir=/usr/lib \
		--sysconfdir=/etc \
		--datadir=/usr/share \
		--localstatedir=/var \
		--includedir=/include \
		--mandir=/usr/man \
		--infodir=/usr/info \
		--disable-nls \
		--enable-static \
		--enable-shared \
	);
	touch $(LIBNFNETLINK_DIR)/.configured

$(LIBNFNETLINK_DIR)/src/libnfnetlink.a: $(LIBNFNETLINK_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(LIBNFNETLINK_DIR)
	-$(STRIP) --strip-unneeded $(LIBNFNETLINK_DIR)/src/libnfnetlink.a

$(STAGING_DIR)/lib/libnfnetlink.a: $(LIBNFNETLINK_DIR)/src/libnfnetlink.a
	$(MAKE) DESTDIR=$(STAGING_DIR) -C $(LIBNFNETLINK_DIR) install

libnfnetlink: $(STAGING_DIR)/lib/libnfnetlink.a

libnfnetlink-source: $(LIBNFNETLINK_SOURCE)/.unpacked

libnfnetlink-clean:
	#rm -f $(LIBNFNETLINK_BUILD_DIR)/.built
	-$(MAKE) -C $(LIBNFNETLINK_BUILD_DIR) clean

libnfnetlink-dirclean:
	rm -rf $(LIBNFNETLINK_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIBNFNETLINK)),y)
TARGETS+=libnfnetlink
endif
