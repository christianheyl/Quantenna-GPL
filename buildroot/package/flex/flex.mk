#############################################################
#
# flex
#
#############################################################
FLEX_VERSION:=2.5.33
FLEX_PATCH_VERSION:=11
FLEX_SOURCE:=flex_$(FLEX_VERSION).orig.tar.gz
FLEX_PATCH:=flex_$(FLEX_VERSION)-$(FLEX_PATCH_VERSION).diff.gz
FLEX_SITE:=http://ftp.gnome.org/mirror/cdimage/snapshot/20050323/Debian/pool/main/f/flex
FLEX_SOURCE_DIR:=$(BUILD_DIR)/flex-$(FLEX_VERSION)
FLEX_DIR1:=$(TOOL_BUILD_DIR)/flex-$(FLEX_VERSION)-host
FLEX_DIR2:=$(BUILD_DIR)/flex-$(FLEX_VERSION)-target
FLEX_CAT:=$(ZCAT)
FLEX_BINARY:=flex
FLEX_TARGET_BINARY:=usr/bin/flex

$(DL_DIR)/$(FLEX_SOURCE):
	 $(WGET) -P $(DL_DIR) $(FLEX_SITE)/$(FLEX_SOURCE)

$(DL_DIR)/$(FLEX_PATCH):
	 $(WGET) -P $(DL_DIR) $(FLEX_SITE)/$(FLEX_PATCH)

flex-source: $(DL_DIR)/$(FLEX_SOURCE) $(DL_DIR)/$(FLEX_PATCH)

$(FLEX_SOURCE_DIR)/.unpacked: $(DL_DIR)/$(FLEX_SOURCE) $(DL_DIR)/$(FLEX_PATCH)
	$(FLEX_CAT) $(DL_DIR)/$(FLEX_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
ifneq ($(FLEX_PATCH),)
	toolchain/patch-kernel.sh $(FLEX_SOURCE_DIR) $(DL_DIR) $(FLEX_PATCH)
	if [ -d $(FLEX_SOURCE_DIR)/debian/patches ]; then \
		toolchain/patch-kernel.sh $(FLEX_SOURCE_DIR) $(FLEX_SOURCE_DIR)/debian/patches \*.patch ; \
	fi
endif
	touch $@

#############################################################
#
# build flex for use on the host system
#
#############################################################

$(FLEX_DIR1)/.configured: $(FLEX_SOURCE_DIR)/.unpacked
	mkdir -p $(FLEX_DIR1)
	(cd $(FLEX_DIR1); rm -rf config.cache; \
		CC="$(HOSTCC)" \
		$(FLEX_SOURCE_DIR)/configure \
		--prefix=/usr \
		$(DISABLE_NLS) \
	);
	touch $@

$(FLEX_DIR1)/$(FLEX_BINARY): $(FLEX_DIR1)/.configured
	$(MAKE) -C $(FLEX_DIR1)
	touch -c $@

$(TOOL_BUILD_DIR)/$(FLEX_BINARY): $(FLEX_DIR1)/$(FLEX_BINARY)
	$(MAKE) DESTDIR=$(TOOL_BUILD_DIR) -C $(FLEX_DIR1) install
	touch -c $@

flex-host: uclibc $(TOOL_BUILD_DIR)/$(FLEX_BINARY)

flex-host-clean:
	-$(MAKE) -C $(FLEX_DIR1) clean

flex-host-dirclean:
	rm -rf $(FLEX_DIR1)


#############################################################
#
# build flex for use on the target system
#
#############################################################

$(FLEX_DIR2)/.configured: $(FLEX_SOURCE_DIR)/.unpacked
	mkdir -p $(FLEX_DIR2)
	(cd $(FLEX_DIR2); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		$(FLEX_SOURCE_DIR)/configure \
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
		--mandir=/usr/man \
		--infodir=/usr/info \
		--includedir=$(TARGET_DIR)/usr/include \
		$(DISABLE_NLS) \
		$(DISABLE_LARGEFILE) \
	);
	touch $@

$(FLEX_DIR2)/$(FLEX_BINARY): $(FLEX_DIR2)/.configured
	$(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(FLEX_DIR2)

$(TARGET_DIR)/$(FLEX_TARGET_BINARY): $(FLEX_DIR2)/$(FLEX_BINARY)
	$(MAKE1) \
	    prefix=$(TARGET_DIR)/usr \
	    exec_prefix=$(TARGET_DIR)/usr \
	    bindir=$(TARGET_DIR)/usr/bin \
	    sbindir=$(TARGET_DIR)/usr/sbin \
	    libexecdir=$(TARGET_DIR)/usr/lib \
	    datadir=$(TARGET_DIR)/usr/share \
	    sysconfdir=$(TARGET_DIR)/etc \
	    sharedstatedir=$(TARGET_DIR)/usr/com \
	    localstatedir=$(TARGET_DIR)/var \
	    libdir=$(TARGET_DIR)/usr/lib \
	    infodir=$(TARGET_DIR)/usr/info \
	    mandir=$(TARGET_DIR)/usr/man \
	    includedir=$(TARGET_DIR)/usr/include \
	    -C $(FLEX_DIR2) install
ifeq ($(strip $(BR2_PACKAGE_FLEX_LIBFL)),y)
	install -D $(FLEX_DIR2)/libfl.a $(STAGING_DIR)/lib/libfl.a
endif
	rm -rf $(TARGET_DIR)/share/locale $(TARGET_DIR)/usr/info \
		$(TARGET_DIR)/usr/man $(TARGET_DIR)/usr/share/doc
	(cd $(TARGET_DIR)/usr/bin; ln -snf flex lex)

flex: uclibc $(TARGET_DIR)/$(FLEX_TARGET_BINARY)

flex-clean:
	$(MAKE) \
	    prefix=$(TARGET_DIR)/usr \
	    exec_prefix=$(TARGET_DIR)/usr \
	    bindir=$(TARGET_DIR)/usr/bin \
	    sbindir=$(TARGET_DIR)/usr/sbin \
	    libexecdir=$(TARGET_DIR)/usr/lib \
	    datadir=$(TARGET_DIR)/usr/share \
	    sysconfdir=$(TARGET_DIR)/etc \
	    sharedstatedir=$(TARGET_DIR)/usr/com \
	    localstatedir=$(TARGET_DIR)/var \
	    libdir=$(TARGET_DIR)/usr/lib \
	    infodir=$(TARGET_DIR)/usr/info \
	    mandir=$(TARGET_DIR)/usr/man \
	    includedir=$(TARGET_DIR)/usr/include \
		-C $(FLEX_DIR2) uninstall
	rm -f $(TARGET_DIR)/usr/bin/lex
ifeq ($(strip $(BR2_PACKAGE_FLEX_LIBFL)),y)
	-rm $(STAGING_DIR)/lib/libfl.a
endif
	-$(MAKE) -C $(FLEX_DIR2) clean

flex-dirclean:
	rm -rf $(FLEX_DIR2)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_FLEX)),y)
TARGETS+=flex
endif
