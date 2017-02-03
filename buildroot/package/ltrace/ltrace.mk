#############################################################
#
# ltrace
#
#############################################################
LTRACE_VERSION:=0.7.3
LTRACE_SOURCE:=ltrace_$(LTRACE_VERSION).orig.tar.bz2
LTRACE_SITE:=http://ftp.acc.umu.se/mirror/cdimage/snapshot/Debian/pool/main/l/ltrace
LTRACE_DIR:=$(BUILD_DIR)/ltrace-$(LTRACE_VERSION)
LTRACE_CAT:=$(BZCAT)
LTRACE_BINARY:=ltrace
LTRACE_TARGET_BINARY:=usr/bin/ltrace

#LTRACE_ARCH:=$(ARCH)
#ifeq ("$(strip $(ARCH))","armeb")
#LTRACE_ARCH:=arm
#endif

$(DL_DIR)/$(LTRACE_SOURCE):
	$(WGET) -P $(DL_DIR) $(LTRACE_SITE)/$(LTRACE_SOURCE)


$(LTRACE_DIR)/.unpacked: $(DL_DIR)/$(LTRACE_SOURCE) $(DL_DIR)/$(LTRACE_SOURCE2)
	$(LTRACE_CAT) $(DL_DIR)/$(LTRACE_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(LTRACE_DIR) package/ltrace/ ltrace\*.patch
	touch $(LTRACE_DIR)/.unpacked

$(LTRACE_DIR)/.configured: $(LTRACE_DIR)/.unpacked
	(cd $(LTRACE_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
		--includedir=/include \
		--sysconfdir=/etc \
		--disable-static \
	);
	touch $(LTRACE_DIR)/.configured;

$(LTRACE_DIR)/$(LTRACE_BINARY): $(LTRACE_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(LTRACE_DIR)

$(TARGET_DIR)/$(LTRACE_TARGET_BINARY): $(LTRACE_DIR)/$(LTRACE_BINARY)
	$(MAKE) DESTDIR=$(TARGET_DIR) -C $(LTRACE_DIR) install
	rm -Rf $(TARGET_DIR)/usr/man

ltrace: uclibc libelf $(TARGET_DIR)/$(LTRACE_TARGET_BINARY)

ltrace-source: $(DL_DIR)/$(LTRACE_SOURCE)

ltrace-clean:
	$(MAKE) prefix=$(TARGET_DIR)/usr -C $(LTRACE_DIR) uninstall
	-$(MAKE) -C $(LTRACE_DIR) clean

ltrace-dirclean:
	rm -rf $(LTRACE_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LTRACE)),y)
TARGETS+=ltrace
endif
