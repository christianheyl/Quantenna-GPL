
.PHONY: minihttpd

MINIHTTPDDIR=$(TOPDIR)/package/zminihttpd

MINIHTTPD_VER:=1.19

MINIHTTPD_BUILD_DIR=$(MINIHTTPDDIR)/mini_httpd-$(MINIHTTPD_VER)
EXTRA_WARNINGS= -Wall -Wshadow -Werror

SSL_DEFS := -DDEFAULT_CERTFILE='\"/etc/minihttpd.pem\"'

ifeq ($(strip $(BR2_PACKAGE_OPENSSL)),y)
SSL_DEFS += -DUSE_SSL -DSUPPORT_DATA_MODEL
SSL_LIBS := -lssl -lcrypto
endif

minihttpd:
	$(MAKE) -C $(MINIHTTPD_BUILD_DIR) PREFIX="$(TARGET_DIR)" \
		BUILD_DIR="$(BUILD_DIR)" TOOLCHAIN_DIR="$(TOOLCHAIN_EXTERNAL_PATH)/$(TOOLCHAIN_EXTERNAL_PREFIX)"\
		SSL_DEFS="$(SSL_DEFS)" SSL_LIBS="$(SSL_LIBS)" \
		install

minihttpd-clean:
	-$(MAKE) -C $(MINIHTTPD_BUILD_DIR) clean

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_MINIHTTPD)),y)
TARGETS+=minihttpd
endif
