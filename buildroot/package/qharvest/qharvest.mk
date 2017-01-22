QHARVEST_VERSION=1.27
QHARVEST_SOURCE=qharvestd-$(QHARVEST_VERSION).tar.gz
QHARVEST_SITE=ftp://ftp.quantenna.com
TOPDIR:=$(shell readlink -f $(TOPDIR))
QHARVEST_MK_DIR=$(TOPDIR)/package/qharvest
QHARVEST_DIR=$(QHARVEST_MK_DIR)/qharvestd

.PHONY: qharvest qharvest-clean
EXTRA_WARNINGS= -Wall -Wshadow
QTN_DISTRIBUTION:="Distributed as binary only user executable"

#Do not use dependencies for this target, because they are executed even if
#this target exists and appears as order-only prerequisite of another target.
#Target's handler isn't executed in this case.
$(QHARVEST_DIR):
	test -f $(DL_DIR)/$(QHARVEST_SOURCE) || \
		$(WGET) -P $(DL_DIR) $(QHARVEST_SITE)/$(QHARVEST_SOURCE)
	$(ZCAT) $(DL_DIR)/$(QHARVEST_SOURCE) | tar -C $(QHARVEST_MK_DIR) $(TAR_OPTIONS) -
	test -d $(QHARVEST_DIR) || \
		mv $(QHARVEST_MK_DIR)/qharvestd-$(QHARVEST_VERSION) $(QHARVEST_DIR)
	toolchain/patch-kernel.sh $(QHARVEST_DIR) $(QHARVEST_MK_DIR) \*.patch

qharvest: libjson libcurl qcsapi zlib | $(QHARVEST_DIR)
	CC="$(TARGET_CC)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS) -lz -Wl,-rpath-link=$(BUILD_DIR)/staging_dir/lib" \
	$(MAKE) -C $(QHARVEST_DIR) \
		PREFIX="$(TARGET_DIR)" \
		SDK_DIR="$(TOPDIR)/.." \
		LIBCURL_STATIC=$(BR2_LIBCURL_STAGING_ONLY) \
		LIBJSON_STATIC=$(BR2_JSON_C_STAGING_ONLY) \
		install

#Do not touch $(QHARVEST_DIR) if it contains only binaries (SDK case).
#Else remove $(QHARVEST_DIR) if it is not a link to development repository,
#so newly checkouted patches will be applied on 'make fromscratch'.
qharvest-clean:
	-if test -n "$(shell find -L $(QHARVEST_DIR) -name '*.c')"; then \
		$(MAKE) -C $(QHARVEST_DIR) clean; \
		test -d $(QHARVEST_DIR) -a ! -L $(QHARVEST_DIR) && rm -rf $(QHARVEST_DIR); \
	fi

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_QHARVEST)),y)
TARGETS+=qharvest
endif
