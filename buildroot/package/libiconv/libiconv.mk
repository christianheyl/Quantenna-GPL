#############################################################
#
# libiconv
#
#############################################################

LIBICONV_VER:=1.13.1
LIBICONV_DIR:=$(BUILD_DIR)/libiconv-$(LIBICONV_VER)
LIBICONV_SITE:=http://ftp.gnu.org/pub/gnu/libiconv/
LIBICONV_SOURCE:=libiconv-$(LIBICONV_VER).tar.gz
LIBICONV_CAT:=zcat


$(DL_DIR)/$(LIBICONV_SOURCE):
	 $(WGET) -P $(DL_DIR) $(LIBICONV_SITE)/$(LIBICONV_SOURCE)

$(LIBICONV_DIR)/.unpacked: $(DL_DIR)/$(LIBICONV_SOURCE)
	$(LIBICONV_CAT) $(DL_DIR)/$(LIBICONV_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(LIBICONV_DIR) package/libiconv/ libiconv-*.patch
	touch $@

$(LIBICONV_DIR)/.configured: $(LIBICONV_DIR)/.unpacked
	(cd $(LIBICONV_DIR); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(HOST_NAME) \
		--prefix= \
		--enable-shared \
		$(LIBICONV_CONFIG_FLAGS) \
	);
	touch $(LIBICONV_DIR)/.configured

$(LIBICONV_DIR)/src/libiconv.so.$(LIBICONV_VER): $(LIBICONV_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(LIBICONV_DIR)

$(TARGET_DIR)/lib/libiconv.so.$(LIBICONV_VER): $(LIBICONV_DIR)/src/libiconv.so.$(LIBICONV_VER)
	$(MAKE) CC=$(TARGET_CC) -C $(LIBICONV_DIR) install-lib includedir=$(TARGET_DIR)/include libdir=$(TARGET_DIR)/lib
	@rm -rf $(TARGET_DIR)/include $(TARGET_DIR)/lib/libiconv.la

$(STAGING_DIR)/lib/libiconv.so.$(LIBICONV_VER): $(LIBICONV_DIR)/src/libiconv.so.$(LIBICONV_VER)
	$(MAKE) CC=$(TARGET_CC) -C $(LIBICONV_DIR) install-lib includedir=$(STAGING_DIR)/include libdir=$(STAGING_DIR)/lib

libiconv: $(STAGING_DIR)/lib/libiconv.so.$(LIBICONV_VER) $(TARGET_DIR)/lib/libiconv.so.$(LIBICONV_VER)

libiconv-source: $(LIBICONV_DIR)/.unpacked

libiconv-clean:
	rm -f $(STAGING_DIR)/include/iconv.h \
	      $(STAGING_DIR)/lib/libiconv.so*
	rm -f $(TARGET_DIR)/include/iconv.h \
	      $(TARGET_DIR)/lib/libiconv.so*

libiconv-dirclean:
	rm -rf $(LIBICONV_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIBICONV)),y)
TARGETS+=libiconv
endif

