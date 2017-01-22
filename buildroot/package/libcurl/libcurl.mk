#############################################################
#
# libcurl
#
#############################################################

LIBCURL_VERSION = 7.25.0
LIBCURL_SOURCE = curl-$(LIBCURL_VERSION).tar.gz
LIBCURL_SITE = http://curl.haxx.se/download
LIBCURL_LICENSE = ICS
LIBCURL_LICENSE_FILES = COPYING
LIBCURL_INSTALL_STAGING = YES
LIBCURL_CONF_OPT = --disable-verbose --disable-manual --enable-hidden-symbols

LIBCURL_DIR:=$(BUILD_DIR)/curl-$(LIBCURL_VERSION)
LIBCURL_LIBRARY:=libcurl
LIBCURL_TARGET_LIBRARY:=usr/bin/libcurl

ifeq ($(BR2_PACKAGE_OPENSSL),y)
LIBCURL_DEPENDENCIES += openssl
LIBCURL_CONF_ENV += ac_cv_lib_crypto_CRYPTO_lock=yes
# configure adds the cross openssl dir to LD_LIBRARY_PATH which screws up
# native stuff during the rest of configure when target == host.
# Fix it by setting LD_LIBRARY_PATH to something sensible so those libs
# are found first.
LIBCURL_CONF_ENV += LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:/lib:/usr/lib
LIBCURL_CONF_OPT += --with-ssl=$(STAGING_DIR) --with-random=/dev/urandom
else
LIBCURL_CONF_OPT += --without-ssl
endif

ifeq ($(BR2_LIBCURL_STAGING_ONLY),y)
LIBCURL_TARGET = $(STAGING_DIR)/lib/libcurl.so
else
LIBCURL_TARGET = $(TARGET_DIR)/$(LIBCURL_TARGET_LIBRARY)
endif


define LIBCURL_TARGET_CLEANUP
	-rm -rf $(TARGET_DIR)/usr/bin/curl-config \
	       $(if $(BR2_PACKAGE_CURL),,$(TARGET_DIR)/usr/bin/curl)
endef

LIBCURL_POST_INSTALL_TARGET_HOOKS += LIBCURL_TARGET_CLEANUP

define LIBCURL_STAGING_FIXUP_CURL_CONFIG
	$(SED) "s,prefix=/usr,prefix=$(STAGING_DIR)/usr," $(STAGING_DIR)/usr/bin/curl-config
endef

LIBCURL_POST_INSTALL_STAGING_HOOKS += LIBCURL_STAGING_FIXUP_CURL_CONFIG

$(eval $(autotools-package))

$(DL_DIR)/$(LIBCURL_SOURCE):
	$(WGET) -P $(DL_DIR) $(LIBCURL_SITE)/$(LIBCURL_SOURCE)

$(LIBCURL_DIR)/.unpacked: $(DL_DIR)/$(LIBCURL_SOURCE)
	$(ZCAT) $(DL_DIR)/$(LIBCURL_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch $(LIBCURL_DIR)/.unpacked

$(LIBCURL_DIR)/.configured: $(LIBCURL_DIR)/.unpacked
	(cd $(LIBCURL_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS=-Os \
		LDFLAGS=-Wl,-rpath-link=$(STAGING_DIR)/lib \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
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
		--disable-tftp \
		--disable-ftp \
		--disable-dict \
		--disable-file \
		--disable-imap \
		--disable-rtsp \
		--disable-pop3 \
		--disable-smtp \
		--disable-gopher \
		--disable-telnet \
		$(LIBCURL_CONF_OPT) \
	);
	touch $(LIBCURL_DIR)/.configured;

$(LIBCURL_DIR)/$(LIBCURL_LIBRARY): $(LIBCURL_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(LIBCURL_DIR)

$(STAGING_DIR)/lib/libcurl.so: $(LIBCURL_DIR)/$(LIBCURL_LIBRARY)
	$(MAKE) DESTDIR=$(STAGING_DIR) -C $(LIBCURL_DIR) install
	touch -c $(STAGING_DIR)/lib/libcurl.so

$(TARGET_DIR)/$(LIBCURL_TARGET_LIBRARY): $(STAGING_DIR)/lib/libcurl.so
	cp -dpf $(STAGING_DIR)/lib/libcurl.so* $(TARGET_DIR)/usr/lib/
#	cp -dpf $(STAGING_DIR)/usr/bin/curl $(TARGET_DIR)/usr/bin/curl
#	cp -dpf $(STAGING_DIR)/usr/bin/curl-config $(TARGET_DIR)/usr/bin/curl-config

libcurl: uclibc $(LIBCURL_DEPENDENCIES) $(LIBCURL_TARGET)

libcurl-source: $(DL_DIR)/$(LIBCURL_SOURCE)

libcurl-clean:
	-rm $(TARGET_DIR)/usr/lib/libcurl.so*
	-$(MAKE) -C $(LIBCURL_DIR) clean

libcurl-dirclean:
	-rm -rf $(LIBCURL_DIR)

curl: libcurl openssl
curl-clean: libcurl-clean
curl-dirclean: libcurl-dirclean
curl-source: libcurl-source

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIBCURL)),y)
TARGETS+=libcurl
endif
ifeq ($(strip $(BR2_PACKAGE_CURL)),y)
TARGETS+=curl
endif

