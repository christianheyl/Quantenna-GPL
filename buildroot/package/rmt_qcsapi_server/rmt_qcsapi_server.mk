
.PHONY: rmt_qcsapi_server

RMT_QCSAPI_SERVER_DIR=$(TOPDIR)/package/rmt_qcsapi_server
RMT_QCSAPI_SERVER_VER:=1.0.0


RMT_QCSAPI_SERVER_BUILD_DIR=$(RMT_QCSAPI_SERVER_DIR)/rmt_qcsapi_server-$(RMT_QCSAPI_SERVER_VER)
EXTRA_WARNINGS= -Wall -Wshadow -Werror

rmt_qcsapi_server:
	$(MAKE) -C $(RMT_QCSAPI_SERVER_BUILD_DIR) PREFIX="$(TARGET_DIR)" \
		CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS) $(EXTRA_WARNINGS)" \
		install

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_RMT_QCSAPI_SERVER)),y)
TARGETS+=rmt_qcsapi_server
endif
