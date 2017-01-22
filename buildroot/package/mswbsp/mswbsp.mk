

.PHONY: mswbsp mswbsp-clean

RPCO802ADIR=$(TOPDIR)/package/mswbsp

RPCO802A_VER:=1.0

RPCO802A_BUILD_DIR=$(RPCO802ADIR)/mswbsp-$(RPCO802A_VER)
EXTRA_WARNINGS= -Wall -Wshadow -Werror

mswbsp: zlib qcsapi
	$(MAKE) -C $(RPCO802A_BUILD_DIR) PREFIX="$(TARGET_DIR)"  \
		BUILD_DIR="$(BUILD_DIR)" TOOLCHAIN_DIR="$(TOOLCHAIN_EXTERNAL_PATH)/$(TOOLCHAIN_EXTERNAL_PREFIX)"\
		install

mswbsp-clean:
	-$(MAKE) -C $(RPCO802A_BUILD_DIR) clean

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_MSWBSP)),y)
TARGETS+=mswbsp
endif
