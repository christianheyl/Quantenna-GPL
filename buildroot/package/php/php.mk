#############################################################
#
# php
#
#############################################################
PHP_VER:=5.0.5
PHP_SOURCE:=php-$(PHP_VER).tar.gz
PHP_SITE:=http://museum.php.net/php5
PHP_DIR:=$(BUILD_DIR)/php-$(PHP_VER)
PHP_CAT:=$(ZCAT)
PHP_BINARY:=isapi/cgi/php-cgi
PHP_TARGET_BINARY:=usr/sbin/php-cgi

$(DL_DIR)/$(PHP_SOURCE):
	 $(WGET) -P $(DL_DIR) $(PHP_SITE)/$(PHP_SOURCE)

php-source: $(DL_DIR)/$(PHP_SOURCE)

$(PHP_DIR)/.unpacked: $(DL_DIR)/$(PHP_SOURCE)
	$(PHP_CAT) $(DL_DIR)/$(PHP_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch  $(PHP_DIR)/.unpacked
	cp package/php/configure $(PHP_DIR)

$(PHP_DIR)/Makefile: $(PHP_DIR)/.unpacked
	(cd $(PHP_DIR); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
                --disable-all \
                --enable-session \
                --disable-cli \
                --enable-cgi \
		--prefix=/usr \
		--libdir=/lib \
		--libexecdir=/usr/lib \
		--libdir=/lib \
		--includedir=/include \
		--sysconfdir=/etc \
		--localstatedir=/var \
		--disable-ipv6 \
	);

$(TARGET_DIR)/$(PHP_TARGET_BINARY): $(PHP_DIR)/Makefile
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(PHP_DIR)
	$(STRIP) -s $(PHP_DIR)/sapi/cgi/php
	mkdir -p $(TARGET_DIR)/usr/lib/cgi-bin
	cp $(PHP_DIR)/sapi/cgi/php $(TARGET_DIR)/usr/lib/cgi-bin/php-cgi
	cp package/php/php.ini $(TARGET_DIR)/lib; chmod u+w $(TARGET_DIR)/lib

php: $(TARGET_DIR)/$(PHP_TARGET_BINARY)

php-clean:
	-$(MAKE) -C $(PHP_DIR) clean

php-dirclean:
	rm -rf $(PHP_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_PHP)),y)
TARGETS+=php
endif

