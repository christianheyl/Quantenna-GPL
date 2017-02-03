#############################################################
#
# lighttpd
#
#############################################################
LIGHTTPD_VER:=1.4.13
LIGHTTPD_SOURCE:=lighttpd-$(LIGHTTPD_VER).tar.gz
LIGHTTPD_SITE:=http://www.lighttpd.net/download
LIGHTTPD_DIR:=$(BUILD_DIR)/lighttpd-$(LIGHTTPD_VER)
LIGHTTPD_CAT:=$(ZCAT)
LIGHTTPD_BINARY:=src/lighttpd
LIGHTTPD_TARGET_BINARY:=usr/sbin/lighttpd

$(DL_DIR)/$(LIGHTTPD_SOURCE):
	 $(WGET) -P $(DL_DIR) $(LIGHTTPD_SITE)/$(LIGHTTPD_SOURCE)

lighttpd-source: $(DL_DIR)/$(LIGHTTPD_SOURCE)

#############################################################
#
# build lighttpd for use on the target system
#
#############################################################
$(LIGHTTPD_DIR)/.unpacked: $(DL_DIR)/$(LIGHTTPD_SOURCE)
	$(LIGHTTPD_CAT) $(DL_DIR)/$(LIGHTTPD_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(LIGHTTPD_DIR) package/lighttpd/ lighttpd\*.patch
	touch  $(LIGHTTPD_DIR)/.unpacked

$(LIGHTTPD_DIR)/.configured: $(LIGHTTPD_DIR)/.unpacked
	(cd $(LIGHTTPD_DIR); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
		--libdir=/usr/lib \
		--libexecdir=/usr/lib \
		--sysconfdir=/etc \
		--localstatedir=/var \
		--with-openssl \
		--without-pcre \
		--program-prefix="" \
		--disable-static \
	);
	touch  $(LIGHTTPD_DIR)/.configured

$(LIGHTTPD_DIR)/$(LIGHTTPD_BINARY): $(LIGHTTPD_DIR)/.configured
	$(MAKE) -C $(LIGHTTPD_DIR)
    
$(TARGET_DIR)/$(LIGHTTPD_TARGET_BINARY): $(LIGHTTPD_DIR)/$(LIGHTTPD_BINARY)
	$(MAKE) DESTDIR=$(TARGET_DIR) -C $(LIGHTTPD_DIR) install
	# remove mod_*.la after file installation
	find $(TARGET_DIR)/lib $(TARGET_DIR)/usr/lib \( -name 'mod_*.a' -o -name 'mod_*.la' \) -print0 | xargs -0 rm -f
	$(INSTALL) -m 0755 -D $(LIGHTTPD_DIR)/openwrt/S51lighttpd $(TARGET_DIR)/etc/init.d/
	$(INSTALL) -m 0644 -D $(LIGHTTPD_DIR)/openwrt/lighttpd.conf $(TARGET_DIR)/etc/

lighttpd: uclibc openssl $(TARGET_DIR)/$(LIGHTTPD_TARGET_BINARY)

lighttpd-clean:
	$(MAKE) DESTDIR=$(TARGET_DIR) CC=$(TARGET_CC) -C $(LIGHTTPD_DIR) uninstall
	-$(MAKE) -C $(LIGHTTPD_DIR) clean

lighttpd-dirclean:
	rm -rf $(LIGHTTPD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIGHTTPD)),y)
TARGETS+=lighttpd
endif
