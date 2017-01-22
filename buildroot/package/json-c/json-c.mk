#############################################################
#
# JSON_C
#
#############################################################

JSON_C_VERSION = 0.10
JSON_C_SOURCE = json-c-$(JSON_C_VERSION).tar.gz
JSON_C_SITE = https://github.com/downloads/json-c/json-c
JSON_C_LICENSE = ICS
JSON_C_LICENSE_FILES = COPYING
JSON_C_INSTALL_STAGING = YES

JSON_C_DIR:=$(BUILD_DIR)/json-c-$(JSON_C_VERSION)
JSON_C_LIBRARY:=json-c
JSON_C_TARGET_LIBRARY:=usr/bin/json-c

ifeq ($(BR2_JSON_C_STAGING_ONLY),y)
JSON_C_TARGET = $(STAGING_DIR)/lib/libjson.so
else
JSON_C_TARGET = $(TARGET_DIR)/$(JSON_C_TARGET_LIBRARY)
endif

$(DL_DIR)/$(JSON_C_SOURCE):
	$(WGET) -P $(DL_DIR) $(JSON_C_SITE)/$(JSON_C_SOURCE)

$(JSON_C_DIR)/.unpacked: $(DL_DIR)/$(JSON_C_SOURCE)
	$(ZCAT) $(DL_DIR)/$(JSON_C_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(JSON_C_DIR) package/json-c/ \*.patch
	touch $(JSON_C_DIR)/.unpacked

$(JSON_C_DIR)/.configured: $(JSON_C_DIR)/.unpacked
	(cd $(JSON_C_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		ac_cv_func_malloc_0_nonnull=yes \
		ac_cv_func_realloc_0_nonnull=yes \
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
	);
	touch $(JSON_C_DIR)/.configured;

$(JSON_C_DIR)/$(JSON_C_LIBRARY): $(JSON_C_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(JSON_C_DIR)

$(STAGING_DIR)/lib/libjson.so: $(JSON_C_DIR)/$(JSON_C_LIBRARY)
	$(MAKE) DESTDIR=$(STAGING_DIR) -C $(JSON_C_DIR) install
	touch -c $(STAGING_DIR)/lib/libjson.so

$(TARGET_DIR)/$(JSON_C_TARGET_LIBRARY): $(STAGING_DIR)/lib/libjson.so
	cp -dpf $(STAGING_DIR)/lib/libjson.so* $(TARGET_DIR)/usr/lib/

libjson: uclibc $(JSON_C_TARGET)

libjson-source: $(DL_DIR)/$(JSON_C_SOURCE)

libjson-clean:
	-rm $(TARGET_DIR)/usr/lib/libjson.so*
	-$(MAKE) -C $(JSON_C_DIR) clean

libjson-dirclean:
	-rm -rf $(JSON_C_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_JSON_C)),y)
TARGETS+=libjson
endif


