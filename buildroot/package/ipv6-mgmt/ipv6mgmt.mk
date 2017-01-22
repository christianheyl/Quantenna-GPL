#############################################################
#
# IPv6 stateless and stateful address auto configuration management
#
#############################################################


IPV6_DIR=$(TOPDIR)/package/ipv6-mgmt

.PHONY: ipv6-mgmt

IPV6MGMT_VER:=1.0.1

IPV6MGMT_BUILD_DIR=$(IPV6_DIR)/ipv6-mgmt-$(IPV6MGMT_VER)
EXTRA_WARNINGS= -Wall -Wshadow -Werror

EXTRA_DEFINES= -D_GNU_SOURCE
TARGET_CFLAGS+=-fPIC

ipv6-mgmt:
	$(MAKE) -C $(IPV6MGMT_BUILD_DIR) PREFIX="$(TARGET_DIR)" \
		CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS) $(EXTRA_WARNINGS) $(EXTRA_DEFINES)"

ipv6-mgmt-clean:
	-$(MAKE) -C $(IPV6MGMT_BUILD_DIR) clean

ipv6-mgmt-dirclean: ipv6-mgmt-clean

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(BR2_INET_IPV6),y)
TARGETS+=ipv6-mgmt
endif
