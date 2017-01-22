#############################################################
#
# dhcp
#
#############################################################
ifndef board_config
-include board_config.mk
endif

DHCP_VER:=4.1-ESV-R3
DHCP_SOURCE:=dhcp-$(DHCP_VER).tar.gz
DHCP_SITE:=ftp://ftp.isc.org/isc/dhcp

DHCP_CAT:=$(ZCAT)
DHCP_DIR:=$(BUILD_DIR)/dhcp-$(DHCP_VER)
DHCP_SERVER_BINARY:=server/dhcpd
DHCP_RELAY_BINARY:=relay/dhcrelay
DHCP_CLIENT_BINARY:=client/dhclient
DHCP_SERVER_TARGET_BINARY:=usr/sbin/dhcpd
DHCP_RELAY_TARGET_BINARY:=usr/sbin/dhcrelay
DHCP_CLIENT_TARGET_BINARY:=usr/sbin/dhclient
BVARS=PREDEFINES='-D_PATH_DHCPD_DB=\"/var/lib/dhcp/dhcpd.leases\" \
	-D_PATH_DHCLIENT_DB=\"/var/lib/dhcp/dhclient.leases\" \
	-D_PATH_DHCPD6_DB=\"/var/lib/dhcp/dhcpd6.leases\" \
	-D_PATH_DHCPD6_PID=\"/var/run/dhcp/dhcpd6.pid\" \
	-D_PATH_DHCLIENT6_DB=\"/var/lib/dhcp/dhclient6.leases\" \
	-D_PATH_DHCLIENT6_PID=\"/var/run/dhcp/dhclient6.pid\"' \
	VARDB=/var/lib/dhcp
DHCP_CONF_ENV = ac_cv_file__dev_random=yes

DHCP_CONF_OPT = \
	--localstatedir=/var/lib/dhcp \
	--enable-tracing=no \
	--enable-failover=no \
	--enable-FEATURE=no \
	--enable-debug=no \
	--enable-failover=no \
	--enable-execute=no \
	--enable-delayed-ack=no \
	--enable-paranoia=no \
	--enable-early-chroot=no \
	--enable-ipv4-pktinfo=no \
	--enable-use-sockets=no \
	--with-srv-lease-file=/var/lib/dhcp/dhcpd.leases \
	--with-cli-lease-file=/var/lib/dhcp/dhclient.leases \
	--with-srv-pid-file=/var/run/dhcpd.pid \
	--with-cli-pid-file=/var/run/dhclient.pid \
	--with-relay-pid-file=/var/run/dhcrelay.pid
ifeq ($(BR2_INET_IPV6),y)
	DHCP_CONF_OPT += --enable-dhcpv6
endif


$(DL_DIR)/$(DHCP_SOURCE):
	 $(WGET) -P $(DL_DIR) $(DHCP_SITE)/$(DHCP_SOURCE)

dhcp-source: $(DL_DIR)/$(DHCP_SOURCE)

dhcp_server-source: dhcp-source
dhcp_relay-source: dhcp-source
dhcp_client-source: dhcp-source

$(DHCP_DIR)/.unpacked: $(DL_DIR)/$(DHCP_SOURCE)
	$(DHCP_CAT) $(DL_DIR)/$(DHCP_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
ifneq ($(filter topaz_msmr_config topaz_msft_config,$(board_config)),)
	toolchain/patch-kernel.sh $(DHCP_DIR) package/dhcp/ dhclient_query_only.patch
endif
	touch $(DHCP_DIR)/.unpacked

$(DHCP_DIR)/.configured: $(DHCP_DIR)/.unpacked
	(cd $(DHCP_DIR); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS) -D__linux" \
		CPPFLAGS="$(SED_CFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
		--exec-prefix=/usr \
		--sysconfdir=/etc \
		$(DISABLE_NLS) \
		$(DHCP_CONF_OPT) \
		$(DHCP_CONF_ENV) \
		);
	touch $(DHCP_DIR)/.configured

lib_dependancy:$(DHCP_DIR)/.configured
	$(MAKE) $(TARGET_CONFIGURE_OPTS) $(BVARS) -C $(DHCP_DIR)/common
	$(MAKE) $(TARGET_CONFIGURE_OPTS) $(BVARS) -C $(DHCP_DIR)/minires
	$(MAKE) $(TARGET_CONFIGURE_OPTS) $(BVARS) -C $(DHCP_DIR)/dst
	$(MAKE) $(TARGET_CONFIGURE_OPTS) $(BVARS) -C $(DHCP_DIR)/omapip

$(TARGET_DIR)/$(DHCP_SERVER_TARGET_BINARY): lib_dependancy
	$(MAKE) $(TARGET_CONFIGURE_OPTS) $(BVARS) -C $(DHCP_DIR)/server
	$(STRIP) $(DHCP_DIR)/$(DHCP_SERVER_TARGET_BINARY)
	(cd $(TARGET_DIR)/var/lib; ln -snf /tmp dhcp)
	$(INSTALL) -m 0755 -D $(DHCP_DIR)/$(DHCP_SERVER_BINARY) $(TARGET_DIR)/$(DHCP_SERVER_TARGET_BINARY)
	$(INSTALL) -m 0755 -D package/dhcp/init-server $(TARGET_DIR)/etc/init.d/S80dhcp-server
	$(INSTALL) -m 0644 -D package/dhcp/dhcpd.conf $(TARGET_DIR)/etc/dhcp/dhcpd.conf
ifeq ($(BR2_INET_IPV6),y)
	$(INSTALL) -m 0644 -D package/dhcp/dhcpd-dhcpv6.conf $(TARGET_DIR)/etc/dhcp/dhcpd-dhcpv6.conf
endif
	rm -rf $(TARGET_DIR)/share/locale $(TARGET_DIR)/usr/info \
		$(TARGET_DIR)/usr/man $(TARGET_DIR)/usr/share/doc

$(TARGET_DIR)/$(DHCP_RELAY_TARGET_BINARY): lib_dependancy
	$(MAKE) $(TARGET_CONFIGURE_OPTS) $(BVARS) -C $(DHCP_DIR)/relay
	$(STRIP) $(DHCP_DIR)/$(DHCP_RELAY_BINARY)
	(cd $(TARGET_DIR)/var/lib; ln -snf /tmp dhcp)
	$(INSTALL) -m 0755 -D $(DHCP_DIR)/$(DHCP_RELAY_BINARY) $(TARGET_DIR)/$(DHCP_RELAY_TARGET_BINARY)
	$(INSTALL) -m 0755 -D package/dhcp/init-relay $(TARGET_DIR)/etc/init.d/S80dhcp-relay
	rm -rf $(TARGET_DIR)/share/locale $(TARGET_DIR)/usr/info \
		$(TARGET_DIR)/usr/man $(TARGET_DIR)/usr/share/doc

$(TARGET_DIR)/$(DHCP_CLIENT_TARGET_BINARY): lib_dependancy
	$(MAKE) $(TARGET_CONFIGURE_OPTS) $(BVARS) -C $(DHCP_DIR)/client
	$(STRIP) $(DHCP_DIR)/$(DHCP_CLIENT_BINARY)
	(cd $(TARGET_DIR)/var/lib; ln -snf /tmp dhcp)
	$(INSTALL) -m 0755 -D $(DHCP_DIR)/$(DHCP_CLIENT_BINARY) $(TARGET_DIR)/$(DHCP_CLIENT_TARGET_BINARY)
	$(INSTALL) -m 0644 -D package/dhcp/dhclient.conf $(TARGET_DIR)/etc/dhcp/dhclient.conf
ifeq ($(BR2_INET_IPV6),y)
	$(INSTALL) -m 0644 -D package/dhcp/dhclient-dhcpv6.conf $(TARGET_DIR)/etc/dhcp/dhclient-dhcpv6.conf
endif
	$(INSTALL) -m 0755 -D package/dhcp/dhclient-script $(TARGET_DIR)/sbin/dhclient-script
	rm -rf $(TARGET_DIR)/share/locale $(TARGET_DIR)/usr/info \
		$(TARGET_DIR)/usr/man $(TARGET_DIR)/usr/share/doc

dhcp_server: uclibc $(TARGET_DIR)/$(DHCP_SERVER_TARGET_BINARY)

dhcp_relay: uclibc $(TARGET_DIR)/$(DHCP_RELAY_TARGET_BINARY)

dhcp_client: uclibc $(TARGET_DIR)/$(DHCP_CLIENT_TARGET_BINARY)

dhcp-clean:
	$(MAKE) DESTDIR=$(TARGET_DIR) CC=$(TARGET_CC) -C $(DHCP_DIR) uninstall
	-$(MAKE) -C $(DHCP_DIR) clean

dhcp-dirclean:
	rm -rf $(DHCP_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_DHCP_SERVER)),y)
TARGETS+=dhcp_server
endif
ifeq ($(strip $(BR2_PACKAGE_DHCP_RELAY)),y)
TARGETS+=dhcp_relay
endif
ifeq ($(strip $(BR2_PACKAGE_DHCP_CLIENT)),y)
TARGETS+=dhcp_client
endif
