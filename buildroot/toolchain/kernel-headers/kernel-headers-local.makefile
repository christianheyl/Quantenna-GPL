#############################################################
#
# local kernel
#
#############################################################

ifeq ("$(DEFAULT_KERNEL_HEADERS)","local")
VERSION:=2
PATCHLEVEL:=6
SUBLEVEL:=20
EXTRAVERSION:=
LOCALVERSION:=
LINUX_HEADERS_VERSION:=$(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)
LINUX_HEADERS_SITE:=http://www.kernel.org/pub/linux/kernel/v2.6/
LINUX_HEADERS_SOURCE:=linux-$(LINUX_HEADERS_VERSION).tar.bz2
LINUX_HEADERS_CAT:=$(BZCAT)
LINUX_HEADERS_UNPACK_DIR:=$(BUILD_DIR)/linux-$(LINUX_HEADERS_VERSION)
LINUX_HEADERS_DIR:=$(TOOL_BUILD_DIR)/linux
LINUX_LOCAL_DIR=$(BASE_DIR)/../linux
LINUX_HEADERS_IS_KERNEL=y

ifeq ($(LINUX_HEADERS_IS_KERNEL),y)

$(LINUX_HEADERS_DIR)/.configured:
	(cd $(LINUX_LOCAL_DIR) ; \
	 $(MAKE) ARCH=$(KERNEL_ARCH) CC="$(HOSTCC)" \
		INSTALL_HDR_PATH=$(LINUX_HEADERS_DIR) headers_install ; \
	)
	mv $(LINUX_HEADERS_DIR)/include/* $(LINUX_HEADERS_DIR)
	touch $@

endif

kernel-headers: $(LINUX_HEADERS_DIR)/.configured

kernel-headers-source:

endif

TARGETS+=kernel-headers
