#############################################################
#
# iptables
#
#############################################################
IPTABLES_VER:=1.4.9
IPTABLES_SOURCE_URL:=ftp.netfilter.org/pub/iptables
IPTABLES_SOURCE:=iptables-$(IPTABLES_VER).tar.bz2
IPTABLES_BUILD_DIR:=$(BUILD_DIR)/iptables-$(IPTABLES_VER)
IPTABLES_CAT:=$(BZCAT)
IPTABLES_BIN:=/usr/sbin/iptables
IP6TABLES_BIN:=/usr/sbin/ip6tables

IPTABLES_CONFIG_FLAGS:=

$(DL_DIR)/$(IPTABLES_SOURCE):
	 $(WGET) -P $(DL_DIR) $(IPTABLES_SOURCE_URL)/$(IPTABLES_SOURCE) 

$(IPTABLES_BUILD_DIR)/.unpacked: $(DL_DIR)/$(IPTABLES_SOURCE)
	$(IPTABLES_CAT) $(DL_DIR)/$(IPTABLES_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch $(IPTABLES_BUILD_DIR)/.unpacked

$(IPTABLES_BUILD_DIR)/.patched: $(IPTABLES_BUILD_DIR)/.unpacked
	toolchain/patch-kernel.sh $(IPTABLES_BUILD_DIR) package/iptables/ iptables\*.patch
	touch $(IPTABLES_BUILD_DIR)/.patched

$(IPTABLES_BUILD_DIR)/.configured: $(IPTABLES_BUILD_DIR)/.patched
	( cd $(IPTABLES_BUILD_DIR); \
	  	#PATH=$(STAGING_DIR)/bin:$$PATH \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
		--exec-prefix=/usr \
		--bindir=/usr/bin \
		--sbindir=/usr/sbin \
		--sysconfdir=/etc \
		--libdir=/usr/lib \
		--libexecdir=/usr/lib \
		--datadir=/usr/share \
		--localstatedir=/var \
		--includedir=/include \
		--mandir=/usr/man \
		--infodir=/usr/info \
		--disable-static \
		--with-kernel=$(LINUX_DIR) \
		$(IPTABLES_CONFIG_FLAGS) \
	);
	touch $(IPTABLES_BUILD_DIR)/.configured

$(IPTABLES_BUILD_DIR)/iptables: $(IPTABLES_BUILD_DIR)/.configured
	#PATH=$(STAGING_DIR)/bin:$$PATH \
	#$(MAKE) -C $(IPTABLES_BUILD_DIR) \
	$(MAKE) CC=$(TARGET_CC) -C $(IPTABLES_BUILD_DIR) \
		DO_IPV6=1 NO_SHARED_LIBS=1 \
		KERNEL_DIR=$(LINUX_DIR) PREFIX=/usr \
		CC=$(TARGET_CC) COPT_FLAGS="$(TARGET_CFLAGS)"

$(TARGET_DIR)$(IPTABLES_BIN): $(IPTABLES_BUILD_DIR)/iptables
	#PATH=$(STAGING_DIR)/bin:$$PATH \
	#$(MAKE) -C $(IPTABLES_BUILD_DIR) \
	$(MAKE) CC=$(TARGET_CC) -C $(IPTABLES_BUILD_DIR) \
		DO_IPV6=1 NO_SHARED_LIBS=1 \
		KERNEL_DIR=$(LINUX_DIR) PREFIX=/usr \
		CC=$(TARGET_CC) COPT_FLAGS="$(TARGET_CFLAGS)" \
		DESTDIR=$(TARGET_DIR) install
	# remove mod_*.la after file installation
	find $(TARGET_DIR)/lib $(TARGET_DIR)/usr/lib \( -name 'libip*.la' -o -name 'libxt*.la' \) -print0 | xargs -0 rm -f
	#$(INSTALL) -D -m 0755 package/iptables/iptables.init $(TARGET_DIR)/etc/init.d/iptables

iptables: $(TARGET_DIR)$(IPTABLES_BIN)

iptables-source: $(IPTABLES_BUILD_DIR)/.patched

iptables-clean:
	-$(MAKE) -C $(IPTABLES_BUILD_DIR) KERNEL_DIR=$(LINUX_DIR) clean
	rm -f $(TARGET_DIR)$(IPTABLES_BIN) $(TARGET_DIR)$(IPTABLES_BIN) \
		$(TARGET_DIR)/etc/init.d/iptables
	rm -rf $(TARGET_DIR)/lib/xtables

iptables-dirclean:
	rm -rf $(IPTABLES_BUILD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_IPTABLES)),y)
TARGETS+=iptables
endif
