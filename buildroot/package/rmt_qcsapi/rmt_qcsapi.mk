
.PHONY: rmt_qcsapi

RMT_QCSAPI_DIR=$(TOPDIR)/package/rmt_qcsapi

RMT_QCSAPI_VER:=1.0.0

RMT_QCSAPI_BUILD_DIR=$(RMT_QCSAPI_DIR)/rmt_qcsapi-$(RMT_QCSAPI_VER)
EXTRA_WARNINGS= -Wall -Wshadow -Werror

rmt_qcsapi:
	$(MAKE) -C $(RMT_QCSAPI_BUILD_DIR) PREFIX="$(TARGET_DIR)" \
		CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS) $(EXTRA_WARNINGS)" \
		install

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_RMT_QCSAPI)),y)
TARGETS+=rmt_qcsapi
endif
