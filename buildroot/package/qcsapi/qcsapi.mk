#############################################################
#
# QCSAPI - Quantenna Control and Status APIs
#
#############################################################

Q_DIR=$(TOPDIR)/package/qcsapi

.PHONY: qcsapi

QCSAPI_VER:=1.0.1
QCSAPI_SUBVER:=
QCSAPI_BUILD_DIR=$(Q_DIR)/qcsapi-$(QCSAPI_VER)
QTN_LICENSE_BRIEF:="Dual BSD and GNU GENERAL PUBLIC LICENSE Version 2 or later"
QTN_LICENSE_FULL_PATH:="qcsapi-1.0.1/qcsapi.h:0:44"
QTN_SOURCE_DOWNLOAD:="Provided as part of the SDK."
QTN_VERSION:=1.0.1
QTN_DESCRIPTION:="Quantenna Security and Configuration API for configuration of the Quantenna SoC."
QTN_MODIFICATIONS:="Quantenna written/provided."
QTN_INTERACTION:="Provides a unified control and configuration interface from the command line (call_qcsapi userspace binary) and a userspace library for third party applications to link against.""

#############################################################
#
# Buildroot package selection
#
#############################################################
QCSAPI_EXTRA_PROGS =
ifeq ($(strip $(BR2_PACKAGE_QCSAPI)),y)
TARGETS += qcsapi

ifeq ($(strip $(BR2_PACKAGE_CALL_QCSAPI_RPC_SOCKET_CLIENT)),y)
QCSAPI_EXTRA_PROGS += call_qcsapi_sockrpc
endif
ifeq ($(strip $(BR2_PACKAGE_CALL_QCSAPI_RPC_PCIE_CLIENT)),y)
QCSAPI_EXTRA_PROGS += call_qcsapi_pcie
endif
ifeq ($(strip $(BR2_PACKAGE_CALL_QCSAPI_RPC_RAW_SOCKET_CLIENT)),y)
QCSAPI_EXTRA_PROGS += call_qcsapi_sockraw qfts
endif
ifeq ($(strip $(BR2_PACKAGE_CALL_QCSAPI_RPC_SERVER)),y)
QCSAPI_EXTRA_PROGS += call_qcsapi_rpcd
endif

ifeq ($(strip $(BR2_PACKAGE_QCSAPI_RPC_SOCKET_CLIENT)),y)
QCSAPI_EXTRA_PROGS += qcsapi_sockrpc
endif
ifeq ($(strip $(BR2_PACKAGE_QCSAPI_RPC_PCIE_CLIENT)),y)
QCSAPI_EXTRA_PROGS += qcsapi_pcie
endif
ifeq ($(strip $(BR2_PACKAGE_QCSAPI_RPC_RAW_SOCKET_CLIENT)),y)
QCSAPI_EXTRA_PROGS += qcsapi_sockraw
endif
ifeq ($(strip $(BR2_PACKAGE_QCSAPI_RPC_SERVER)),y)
QCSAPI_EXTRA_PROGS += qcsapi_rpcd
endif

ifeq ($(strip $(BR2_PACKAGE_QCSAPI_MONITOR_RFENABLE)),y)
QCSAPI_EXTRA_PROGS += monitor_rfenable
endif

endif


#############################################################
#
# Build recipes
#
#############################################################

EXTRA_WARNINGS = -Wall -Wshadow -Werror

# define _GNU_SOURCE to get prototypes for strcasecmp and strncasecmp
# Programs are present in the C library; this #define just insures their prototype is defined.
EXTRA_DEFINES = -D_GNU_SOURCE
TARGET_CFLAGS += -fPIC
STRIP = $(TARGET_CROSS)strip

ifeq ($(QCSAPI_DEBUG),1)
STRIP = echo
TARGET_CFLAGS += -g
endif

qcsapi: zlib
	$(MAKE) MAKEFLAGS="-j1" -C $(QCSAPI_BUILD_DIR) EXTRA_PROGS="$(QCSAPI_EXTRA_PROGS)" PREFIX="$(TARGET_DIR)" \
		CC=$(TARGET_CC) STRIP=$(STRIP) XCFLAGS="$(TARGET_CFLAGS) $(EXTRA_WARNINGS) $(EXTRA_DEFINES)	\
			-I../../../../drivers/include/shared	\
			-I../../../../				\
			-I../../../../include" build_all install

qcsapi-clean:
	-$(MAKE) -C $(QCSAPI_BUILD_DIR) clean

qcsapi-dirclean: qcsapi-clean

