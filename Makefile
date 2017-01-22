#
#  Copyright (c) 2007-2014 Quantenna Communications, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# Top-level makefile for building the entire system.
#

default: help

ifndef board_config
-include board_config.mk
endif

include Make.toolchain

# Provide a default config
board_config ?= topaz_config
board_platform ?= $(shell perl -e 'if ( "$(board_config)" =~ /topaz/ ) { print "topaz"; } else { print "ruby"; }')
board_type ?= asic
spi ?= yes
wmac_mode ?= ap
arc770 ?= no
arc_rev_ge_4_10 ?= no
qtn_hal_pm_corrupt_debug ?= no
pre ?= no # include prepopulated files
bb_only ?= 1
br2_jlevel ?= $(shell echo -n $$((`nproc || echo 0` + 1)))

br2_jlevel_max := 5
br2_jlevel := $(shell \
	if [ $(br2_jlevel) -gt $(br2_jlevel_max) ] ; \
	then echo $(br2_jlevel_max) ; \
	else echo $(br2_jlevel) ; \
	fi)

ifneq ($(filter topaz_pcie_config topaz_rgmii_config topaz_vzn_config,$(board_config)),)
mini_uboot_enabled ?= 1
endif

ifneq ($(filter topaz_pcie_config,$(board_config)),)
tiny_uboot_enabled ?= 0
endif

sigma_testbed_support_enabled ?= 0
tkip_support_enabled ?= $(sigma_testbed_support_enabled)
flash_env_size ?= 24

# export firmware staging area to child makefiles
QTN_FW_STAGING_REL = buildroot/target/device/Quantenna/Ruby/fw/$(board_platform)-bin
QTN_FW_PREBUILD_REL = buildroot/target/device/Quantenna/Ruby/fw_bin/$(board_platform)-bin
QTN_FW_STAGING = $(shell pwd)/$(QTN_FW_STAGING_REL)

QTN_FW_PREBUILD = $(shell pwd)/$(QTN_FW_PREBUILD_REL)
export QTN_FW_STAGING QTN_FW_STAGING_REL QTN_FW_PREBUILD

AWK := awk
CONFIG_AWK := ./post-process-configs.awk
export NEW_CONFIG = .config.new
first_run = .first_run

QDRV_DIR = drivers/qdrv
QDRV_CONFIG_FILE = $(QDRV_DIR)/qdrv_config.h
PLATFORM_ID_FILE = buildroot/target/device/Quantenna/Ruby/scripts/platform_id
POWER_TABLES_DIR = power_tables

# Directory sub-tree names for the major modules in the RUBY software tree
kdir      := linux
udir      := u-boot
fsdir     := buildroot
hostdir   := host
commondir := common
macfwdir  := macfw
aucfwdir  := aucfw
rdspfwdir := rdspfw
tftpdir   := tftp

ifeq ($(ccache_enable),yes)
compile_prefix := $(target_prefix)
TARGET_CC_FOR_BR := TARGET_CC=$(target_prefix)gcc TARGET_CROSS=$(target_prefix)
export PATH := $(PATH):$(tools_path)
else
compile_prefix := $(tools_path)$(target_prefix)
TARGET_CC_FOR_BR :=
endif

# Where the kernel modules live
INSTALL_MOD_PATH := $(PWD)/$(kdir)/modules

# Configuration file names
busybox := busybox-1.10.3
qcsapi :=  $(shell cd buildroot/package/qcsapi; ls -d qcsapi-*.*)

# The final image used by U-Boot to boot
image_suffix :=
.PHONY: image_suffix

PROFILE_MUC_EP :=
PROFILE_MUC_PG :=
PROFILE_MUC_P :=
PROFILE_LINUX_EP :=
PROFILE_MUC_SAMPLE_DCACHE :=
PROFILE_MUC_SAMPLE_IPTR :=
PROFILE_MUC_SAMPLE_IPTR_AUC :=
PROFILE_MUC_DROP_PACKETS :=
PROFILE_LINUX_SAMPLE_IPTR :=
PROFILE_LINUX_SAMPLE_DCACHE :=
PROFILE_MUC_SAMPLE_DCACHE :=
POST_RF_LOOP :=
.PHONY: PROFILE_MUC_EP PROFILE_MUC_PG PROFILE_MUC_P PROFILE_LINUX_EP PROFILE_MUC_SAMPLE_DCACHE PROFILE_MUC_SAMPLE_IPTR PROFILE_MUC_SAMPLE_IPTR_AUC PROFILE_LINUX_SAMPLE_DCACHE PROFILE_MUC_DROP_PACKETS PROFILE_LINUX_SAMPLE_IPTR POST_RF_LOOP
export PROFILE_MUC_EP
export PROFILE_MUC_P
export PROFILE_MUC_PG
export PROFILE_LINUX_EP
export PROFILE_MUC_SAMPLE_DCACHE
export PROFILE_MUC_SAMPLE_IPTR
export PROFILE_MUC_SAMPLE_IPTR_AUC
export PROFILE_MUC_DROP_PACKETS
export PROFILE_LINUX_SAMPLE_IPTR
export PROFILE_LINUX_SAMPLE_DCACHE
export POST_RF_LOOP

uncompressed-boot-image := $(tftpdir)/$(board_platform)-linux.img$(image_suffix)
gz-boot-image := $(tftpdir)/$(board_platform)-linux.gz.img$(image_suffix)
lzma-boot-image := $(tftpdir)/$(board_platform)-linux.lzma.img$(image_suffix)

# Where the kernel compiled image is...
kimage := $(kdir)/arch/arc/boot/Image
gz_compressed_kimage := $(kimage).gz
lzma_compressed_kimage := $(kimage).lzma

# lzma executable
LZMA := host/utilities/lzma
CHECK_LZMA_SIZE := host/utilities/check_lzma_image_size

STORE_OBJS := linux/vmlinux macfw/qtn_ruby macfw/bb_only $(shell find drivers/ -name \*.ko)

SPARSE = host/utilities/sparse/install/bin/sparse

# What revision are we building?
rev_num_file:=buildroot/target/device/Quantenna/Ruby/scripts/rev_num
rev_num:=$(shell cat ${rev_num_file} 2>/dev/null)
doc_rev_ext:=$(rev_num)

$(fsdir)/.config.% $(fsdir)/package/busybox/$(busybox)/.config.%: ./configs/%
	rm -f $(@F)
	cat $< > $(@F)
	$(AWK) -f $(CONFIG_AWK) $(@F)
	cd $(fsdir); \
		if ! cmp -s $(@F) $(NEW_CONFIG); then \
			cp $(NEW_CONFIG) $(@F); fi
	cd $(fsdir)/package/busybox/$(busybox); \
		if ! cmp -s .config.$(@F) $(NEW_CONFIG); then \
			cp $(NEW_CONFIG) $(@F); fi

$(kdir)/.config.%: $(kdir)/arch/arc/configs/%
	cat $< > $@

BUILD_CONFIGS = $(fsdir)/.config.$(board_config)	\
		$(fsdir)/package/busybox/$(busybox)/.config.$(board_config)	\
		$(kdir)/.config.$(board_config)

define setconfigs
	# setting configurations to $(1)
	cd $(kdir); \
		ln -fs .config.$(1) .config.current; \
		cp .config.current .config
	cd $(fsdir); \
		ln -fs .config.$(1) .config.current; \
		cp .config.current .config
	cd $(fsdir)/package/busybox/$(busybox);	\
		ln -fs .config.$(1) .config.unpatched.current; \
		touch .config.unpatched.current
endef

# copy possibly modified configs back over the board specific config
define syncconfigs
	# Syncing configs
	@if [ -f $(kdir)/.config ] ; then \
		cat $(kdir)/.config > $(kdir)/.config.current; \
	fi

	@if [ -f $(fsdir)/.config ] ; then \
		cat $(fsdir)/.config > $(fsdir)/.config.current; \
	fi
endef

test_config: buildroot/.config buildroot/package/busybox/busybox-1.10.3/.config
	perl -e 'foreach $$file qw($^) { foreach $$line (`cat $$file`) { $$p = `dirname $$file`; chop($$p); print "#prefix0#$$p/#\n$$line"; } }' > configs/test
	sed -i 's@#prefix0#buildroot/package@#prefix7#buildroot/package@' configs/test
# -------------------------------------------------------------------------
#                           Configuration targets
# -------------------------------------------------------------------------

help:
	@echo "Use make image or make fromscratch"

config: configupdate

ALL_CONFIGS = 	topaz_config \
		topaz_rfic6_config \
		topaz_host_config \
		topaz_pcie_config \
		topaz_np_config \
		topaz_rgmii_config \
		topaz_vzn_config \
		topaz_dbdc_config \
		topaz_msmr_config \
		topaz_msft_config

.PHONY: $(ALL_CONFIGS) config get_current_config get_current_platform board_config board_platform

ifeq ($(board_config),topaz_host_config)
QTN_FWS := dsp
else
QTN_FWS := muc auc dsp
endif

define copy_if_changed
	cmp --quiet $1 $2 || cp -v $1 $2
endef

board_config.mk.tmp: force
	rm -f $@
	echo "# Automatically generated file. Do not edit." >> $@
	echo "# use 'make xxxx_config' instead" >> $@
	echo "export board_config = $(board_config)" >> $@
	echo "export board_type = $(board_type)" >> $@
	echo "export board_platform = $(board_platform)" >> $@
	echo "export spi = $(spi)" >> $@
	echo "export qtn_hal_pm_corrupt_debug = $(qtn_hal_pm_corrupt_debug)" >> $@
	echo "export wmac_mode = $(wmac_mode)" >> $@
	echo "export arc770 = $(arc770)" >> $@
	echo "export arc_rev_ge_4_10 = $(arc_rev_ge_4_10)" >> $@
	echo "export pre = $(pre)" >> $@
	echo "export bb_only = $(bb_only)" >> $@
	@if [ "$(hw_config_id)" != "" ] ; then \
		echo "export hw_config_id = $(hw_config_id)" >> $@; \
	fi
	$(call copy_if_changed,$@,board_config.mk)
	rm $@

common/current_platform.h.tmp: force
	@echo "Board type: $(board_type) config: $(board_config) platform: $(board_platform) selected"
	rm -f $@
	touch $@
ifeq ($(board_platform),topaz)
	echo "#define TOPAZ_PLATFORM" >> $@
ifeq ($(board_type),asic)
	echo "#define TOPAZ_FPGA_PLATFORM 0" >> $@
else
	echo "#define TOPAZ_FPGA_PLATFORM 1" >> $@
endif	# board type
ifeq ($(filter topaz_pcie_config,$(board_config)),)
	echo "#define TOPAZ_EMAC_NULL_BUF_WR" >> $@
endif
ifeq ($(ddr),umctl1)
	@echo "DDR3 UMCTL-1 Configuration"
	echo "#define TOPAZ_FPGA_UMCTL1" >> $@
else
	@echo "DDR3 UMCTL-2 Configuration"
	echo "#undef TOPAZ_FPGA_UMCTL1" >> $@
endif
endif	# topaz
ifeq ($(spi),no)
	echo "#define PLATFORM_NOSPI $(spi)" >> $@
endif
ifneq ($(wmac_mode),)
	@echo "WMAC Mode $(wmac_mode)"
	echo "#define PLATFORM_WMAC_MODE $(wmac_mode)" >> $@
endif
ifneq ($(hw_config_id),)
	@echo "Default hw_config_id $(hw_config_id)"
	echo "#define PLATFORM_DEFAULT_BOARD_ID $(hw_config_id)" >> $@
else
	echo "#undef PLATFORM_DEFAULT_BOARD_ID" >> $@
endif
ifeq ($(arc770),yes)
	echo "#define PLATFORM_ARC7_MMU_VER 3" >> $@
endif
ifneq ($(arc_rev_ge_4_10),yes)
# For ARC hardware revisions prior to 4.10,
# there is a hardware bug causing an ITLB Miss exception even if a
# respective TLB entry already exists. This leads to duplicate TLB entries
# if an ITLB Miss exception handler does not check whether such an entry
# already exists:
#
# 1. Processor takes an ITLB Miss exception when the entry already exists.
#
# 2. ITLB Miss handler presumes that there is no such entry in TLB, and
# looks for a proper TLB way and set to store the entry.
#
# 3. Because the hardware way selection is pseudo-random, it can return a
# way number that may or may not match the way number of the existing
# entry. If the way number is different, a duplicate TLB entry is created,
# because the MMU checks for duplicate TLB entries only at the time of
# look-up but not at the time of adding entries.
#
# The following ITLB Miss exception is eventually triggered with a cause
# code indicating duplicated TLB entries. This may cause unexpected
# behavior of the software.
#
# This option enables a software workaround for this HW problem.
	echo "#define ARC_HW_REV_NEEDS_TLBMISS_FIX" >> $@
endif
ifeq ($(qtn_hal_pm_corrupt_debug),yes)
	echo "#define QTN_HAL_PM_CORRUPT_DEBUG" >> $@
endif
ifneq ($(hbm_skb_allocator),)
	echo "#define TOPAZ_HBM_SKB_ALLOCATOR_DEFAULT $(hbm_skb_allocator)" >> $@
endif
ifneq ($(hbm_payload_count_s),)
	echo "#define TOPAZ_HBM_PAYLOAD_COUNT_S $(hbm_payload_count_s)" >> $@
	echo "#define TOPAZ_HBM_PAYLOAD_PTR_SECTION" >> $@
endif
ifeq ($(board_config),topaz_host_config)
	echo "#define QTN_RC_ENABLE_HDP" >> $@
endif
ifeq ($(board_config),topaz_pcie_config)
	echo "#define TOPAZ_DISABLE_FWT_WAR" >> $@
endif

ifeq (topaz_rfic6,$(board_config:topaz_rfic6%=topaz_rfic6))
	echo "#define TOPAZ_RFIC6_PLATFORM" >> $@
	echo "#define FLASH_SUPPORT_256KB" >> $@
	echo "#define ENABLE_C0_CALIB" >> $@
	echo "#define ENABLE_C0" >> $@
else
ifeq ($(flash_env_size), 24)
	echo "#define FLASH_SUPPORT_64KB" >> $@
else
ifneq ($(flash_env_size), 64)
	$(error Wrong flash environment size)
endif
endif
endif
	echo "#define WPA_TKIP_SUPPORT $(tkip_support_enabled)" >> $@
	echo "#define SIGMA_TESTBED_SUPPORT $(sigma_testbed_support_enabled)" >> $@

	$(call copy_if_changed,$@,common/current_platform.h)
	rm $@

$(ALL_CONFIGS):
	@if [ ! -f $(kdir)/arch/arc/configs/$@ ]; then			\
		echo No kernel config found for board_config $@ ;	\
		false ;							\
	fi
	$(MAKE) -C . board_config=$@ board_platform=$(shell perl -e 'if ( "$@" =~ /topaz/ ) { print "topaz"; } else { print "ruby"; }') board_config.mk.tmp
	$(MAKE) -C . board_config=$@ board_platform=$(shell perl -e 'if ( "$@" =~ /topaz/ ) { print "topaz"; } else { print "ruby"; }') common/current_platform.h.tmp
	$(MAKE) -C . board_config=$@ board_platform=$(shell perl -e 'if ( "$@" =~ /topaz/ ) { print "topaz"; } else { print "ruby"; }') configupdate
	@if [ -d $(POWER_TABLES_DIR)/$@ ]; then				\
		(cd $(POWER_TABLES_DIR) && ln -nsf $@ board_config) ;	\
	else								\
		rm -f $(POWER_TABLES_DIR)/board_config ;		\
	fi
	echo "/* Automatically generated file. Do not edit. */" > $(QDRV_CONFIG_FILE);
	@echo -n "#define QDRV_CFG_PLATFORM_ID " >> $(QDRV_CONFIG_FILE)
	@echo $(shell grep CONFIG_PLATFORM_ID configs/$@ 2>/dev/null | cut -d= -f2) >> $(QDRV_CONFIG_FILE)
	echo "#define QDRV_CFG_TYPE \"$@\"" >> $(QDRV_CONFIG_FILE);
	@echo $(shell grep CONFIG_PLATFORM_ID configs/$@ 2>/dev/null | cut -d= -f2) > $(PLATFORM_ID_FILE)

get_current_config:
	@echo $(board_config)

get_current_platform:
	@echo $(board_platform)

# -------------------------------------------------------------------------
#                           Cleaning targets
# -------------------------------------------------------------------------

distcleans := commondistclean udistclean kdistclean hostdistclean macfwdistclean rdspfwdistclean aucfwdistclean
cleans     := commonclean uclean kclean hostclean macfwclean rdspfwclean aucfwclean

.PHONY: clean distclean fsclean $(cleans) $(distcleans) configclean

clean: $(QTN_LICENSE_CLEAN) fsclean $(cleans)

fsclean:
	$(MAKE) -C $(fsdir) CROSS_COMPILE=$(compile_prefix) clean

$(cleans):
	@if [ -d $($(strip $(patsubst %clean, %dir, $@))) ] ; then \
		$(MAKE) -C $($(strip $(patsubst %clean, %dir, $@))) CROSS_COMPILE=$(compile_prefix) clean ; \
	fi

distclean: $(QTN_LICENSE_CLEAN) fsdistclean $(distcleans) \
	driversclean cleantftpdir configclean doxygenclean fwstagingclean
	rm -f $(first_run)

configclean:
	rm -f $(fsdir)/package/busybox/$(busybox)/.config* .config* $(fsdir)/.config* $(kdir)/.config*
	rm -f board_config.mk
	rm -f common/current_platform.h
	rm -f $(QDRV_CONFIG_FILE)
	rm -f $(PLATFORM_ID_FILE)

fsdistclean:
	$(MAKE) -C $(fsdir) CROSS_COMPILE=$(compile_prefix) distclean

doxygenclean:
	rm -rf ./doxygen.tar.bz2 ./doxygen/*

fwstagingclean:
	rm -rf $(QTN_FW_STAGING)

driversclean:
ifeq ($(wildcard drivers),drivers)
	$(MAKE) -C drivers clean
	rm -f drivers/extra_kos.symvers
endif

$(distcleans):
	@if [ -d $($(strip $(patsubst %distclean, %dir, $@))) ] ; then \
	$(MAKE) -C $($(strip $(patsubst %distclean, %dir, $@))) CROSS_COMPILE=$(compile_prefix) distclean ; \
	fi

clean_linux_modules:
	rm -rf $(kdir)/modules/lib/modules/*

# -------------------------------------------------------------------------
#                           Building targets
# -------------------------------------------------------------------------

.PHONY: include_gen force all ruby topaz $(QTN_FWS) auc_topaz fromscratch cleantftpdir configupdate copyconfigs u-boot host
.PHONY: buildroot buildbot_package buildbot_nightly verify_binary_archive unpack_binary_archive clean_linux_modules kernel_modules external_modules
.PHONY: qtn_license

########### prerequisite image building
CHILD_BUILD_PATH ?= $(shell pwd)
export CHILD_BUILD_PATH

this_config_prereqs =
#this_config_prereqs += prereq_debug

# define prerequisite configs for images that require them
ifeq ($(QTN_LINUX_IMG_DIR),)
topaz_host_config_prereqs = topaz_pcie_config
topaz_np_config_prereqs = topaz_rgmii_config
endif

ifneq ($($(board_config)_prereqs),)
QTN_FWS += u-boot
endif

# override image maximum size for images that will contain child images
qtm710_np_config_maxsize = 7471104
qtm710_rc_config_maxsize = 7471104
# minimum ODM request for RGMII image size limit is 4.5M, including u-boot (128k)
qtm710_rgmii_config_maxsize = 4350000
qhs711_host_config_maxsize = 10000000

prereq_debug:
	## board_config: $(board_config) has prerequisites: $(this_config_prereqs)

# template recipe/rule for building child prerequisites
define PREREQ_TEMPLATE
prereq_$(1):
	$(syncconfigs)
	## target '$(board_config)' has prereq '$(1)', building '$(1)'...
	$(MAKE) -C $(CHILD_BUILD_PATH) board_config=$(1) configupdate
	$(MAKE) -C . board_config=$(1) common/current_platform.h.tmp
	+$(MAKE) -C $(CHILD_BUILD_PATH) board_config=$(1) image_no_prereq
	## target '$(board_config)' prereq '$(1)' completed.
	## restoring configs '$(board_config)'
	$(MAKE) -C . board_config=$(board_config) configupdate
	$(MAKE) -C . board_config=$(board_config) common/current_platform.h.tmp

.PHONY: prereq_$(1)
this_config_prereqs += prereq_$(1)
endef
$(foreach prereq,$($(board_config)_prereqs),$(eval $(call PREREQ_TEMPLATE,$(prereq))))

PHONY: check_packages
check_packages:
	if test -f tools/setup.sh; then tools/setup.sh --check-packages; fi

.PHONY: image_no_prereq
image_no_prereq: u-boot $(QTN_HOST_BOARD_UTILS) $(uncompressed-boot-image) qtn_license $(lzma-boot-image)

####### current image building
all image: check_packages $(BUILD_NAME_DEP)
	@for p in $(this_config_prereqs); do $(MAKE) $${p}; done;
	$(MAKE) image_no_prereq

qtn_license: $(QTN_LICENSE_FILE)

ruby topaz: $(uncompressed-boot-image) $(lzma-boot-image)

fromscratch:
	$(MAKE) -j1 distclean
	$(MAKE) $(board_config)
	$(MAKE) all

$(SPARSE):
	$(MAKE) -C host/utilities/sparse

$(tftpdir):
	mkdir -p $@

cleantftpdir:
	rm -rf $(tftpdir)/

configupdate: $(BUILD_CONFIGS) clean_linux_modules unpack_binary_archive
	$(syncconfigs)
	$(call setconfigs,$(board_config))
	@if [ "$(spi)" = "no" ] ; then \
		echo "BR2_PACKAGE_QTN_PREPOPULATE=y" >> $(fsdir)/.config; \
	fi
	@if [ "$(pre)" = "yes" ] ; then \
		echo "BR2_PACKAGE_QTN_PREPOPULATE=y" >> $(fsdir)/.config; \
	fi
	@if [ $(br2_jlevel) != '1' ] && [ "$(findstring j,$(MAKEFLAGS))" != "" ] ; then \
		sed --in-place "s/BR2_JLEVEL=.*/BR2_JLEVEL=$(br2_jlevel)/" $(fsdir)/.config ; \
	fi

copyconfigs: configupdate
	cd $(fsdir); yes "" | $(MAKE) config
	cd $(fsdir); mkdir -p dl; cp download_backup/* dl; chmod u+w dl/*

u-boot: common $(tftpdir)
##################TEY add for disable console ##############
	cp $(udir)/include/configs/ruby.h  $(udir)/include/configs/ruby.h.ori
	cp $(udir)/include/configs/ruby.h.consoleoff $(udir)/include/configs/ruby.h
############################################################
	$(MAKE) -C $(udir) CROSS_COMPILE=$(compile_prefix) ruby_config
	$(MAKE) -C $(udir) CROSS_COMPILE=$(compile_prefix) all
	$(MAKE) -C $(udir) CROSS_COMPILE=$(compile_prefix) ruby_mini_config
	$(MAKE) -C $(udir) CROSS_COMPILE=$(compile_prefix) ruby_mini
ifeq ($(tiny_uboot_enabled),1)
	$(MAKE) -C $(udir) -f qtn_tiny.mk CROSS_COMPILE=$(compile_prefix)
	$(MAKE) -C $(udir) -f piggy.mk CROSS_COMPILE=$(compile_prefix) TARGET=u-boot-tiny
endif
	$(MAKE) -C $(udir) -f piggy.mk CROSS_COMPILE=$(compile_prefix) TARGET=u-boot-mini
	$(MAKE) -C $(udir) -f piggy.mk CROSS_COMPILE=$(compile_prefix) TARGET=u-boot
	mv $(udir)/u-boot.bin $(udir)/u-boot.bin.full
ifneq ($(NO_CHECK_SIZE),1)
	$(CHECK_LZMA_SIZE) $(udir)/u-boot-mini-piggy.bin 20480
ifeq ($(tiny_uboot_enabled),1)
	$(CHECK_LZMA_SIZE) $(udir)/u-boot-tiny-piggy.bin 20480
endif
	$(CHECK_LZMA_SIZE) $(udir)/u-boot-piggy.bin 81920
endif
	cp $(udir)/u-boot-piggy.bin $(tftpdir)/u-boot.bin$(image_suffix)
ifeq ($(mini_uboot_enabled),1)
	cp $(udir)/u-boot-mini-piggy.bin $(tftpdir)/u-boot-mini-piggy.bin$(image_suffix)
endif
	@mkdir -p $(QTN_FW_STAGING)/
	cp $(udir)/u-boot-piggy.bin $(QTN_FW_STAGING)/u-boot.bin
##################TEY add for disable console ##############
	mv $(tftpdir)/u-boot.bin $(tftpdir)/u-boot.bin.consoleoff
############################################################

##################TEY add for enable console ##############
	cp $(udir)/include/configs/ruby.h.consoleon $(udir)/include/configs/ruby.h
	$(MAKE) -C $(udir) CROSS_COMPILE=$(compile_prefix) ruby_config
	$(MAKE) -C $(udir) CROSS_COMPILE=$(compile_prefix) all
	$(MAKE) -C $(udir) CROSS_COMPILE=$(compile_prefix) ruby_mini_config
	$(MAKE) -C $(udir) CROSS_COMPILE=$(compile_prefix) ruby_mini
	$(MAKE) -C $(udir) -f piggy.mk CROSS_COMPILE=$(compile_prefix) TARGET=u-boot-mini
	$(MAKE) -C $(udir) -f piggy.mk CROSS_COMPILE=$(compile_prefix) TARGET=u-boot
	mv $(udir)/u-boot.bin $(udir)/u-boot.bin.full
ifneq ($(NO_CHECK_SIZE),1)
	$(CHECK_LZMA_SIZE) $(udir)/u-boot-mini-piggy.bin 20480
	$(CHECK_LZMA_SIZE) $(udir)/u-boot-piggy.bin 81920
endif
	cp $(udir)/u-boot-piggy.bin $(tftpdir)/u-boot.bin$(image_suffix)
ifeq ($(mini_uboot_enabled),1)
	cp $(udir)/u-boot-mini-piggy.bin $(tftpdir)/u-boot-mini-piggy.bin$(image_suffix)
endif
	@mkdir -p $(QTN_FW_STAGING)/
	cp $(udir)/u-boot-piggy.bin $(QTN_FW_STAGING)/u-boot.bin

	mv tftp/u-boot.bin tftp/u-boot.bin.consoleon
	mv u-boot-2009.06/include/configs/ruby.h.ori  u-boot-2009.06/include/configs/ruby.h
##################################################################
ifeq ($(tiny_uboot_enabled),1)
	cp $(udir)/u-boot-tiny-piggy.bin $(tftpdir)/u-boot-tiny.bin$(image_suffix)
	$(udir)/tools/mkimage -A arc -C none -O u-boot -T firmware	\
		-name UBOOT_STAGE2	\
		-d $(udir)/u-boot-piggy.bin $(tftpdir)/u-boot-stage2.img$(image_suffix)
endif

host:
	if [ -d $(hostdir) ] ; then $(MAKE) -C $(hostdir) ; fi

.PHONY: do_buildroot
do_buildroot:
	@echo "################################### Do buildroot"
	-$(MAKE) -j1 -C $(fsdir) KERNEL_MODULES_PATH=$(INSTALL_MOD_PATH)/lib clean-fs
	@if [ "topaz_msft_config" = "$(board_config)" ]; then \
		$(MAKE) -j1 -C $(fsdir) $(TARGET_CC_FOR_BR) KERNEL_MODULES_PATH=$(INSTALL_MOD_PATH)/lib MSFT=y; \
	else \
		$(MAKE) -j1 -C $(fsdir) $(TARGET_CC_FOR_BR) KERNEL_MODULES_PATH=$(INSTALL_MOD_PATH)/lib; \
	fi
	$(syncconfigs)

buildroot: check_packages $(QTN_FWS) kernel_modules external_modules strip_modules $(QTN_REGULATORY_DB)
	$(MAKE) do_buildroot

kernel_image: buildroot
	@echo "################################### Making kernel image"
	@mkdir -p buildroot/build_arc/root/etc/default
	$(MAKE) -C $(kdir) ARCH=arc CROSS_COMPILE=$(compile_prefix) Image
	$(syncconfigs)

kernel_modules: $(first_run)
	@echo "################################### Making kernel modules"
	$(MAKE) -C $(kdir) ARCH=arc CROSS_COMPILE=$(compile_prefix) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) vmlinux modules
	$(MAKE) -C $(kdir) ARCH=arc CROSS_COMPILE=$(compile_prefix) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) modules_install
	$(syncconfigs)

verify_binary_archive:
	@if [ $$(echo quantenna-bin-*tar | wc -w) -gt 1 ];		\
	then								\
		echo "Too many binary archives, correct and restart";	\
		ls -l quantenna-bin-*tar;				\
		exit 1;							\
	fi
	@if [ $$(echo pcie-quantenna-bin-*tar | wc -w) -gt 1 ];                         \
	then                                                                            \
		echo "Too many binary archives for pcie build, correct and restart";	\
		ls -l pcie-quantenna-bin-*tar;                                          \
		exit 1;                                                                 \
	fi
	@if [ $$(echo pcie-host-quantenna-bin-*tar | wc -w) -gt 1 ];                    \
	then                                                                            \
		echo "Too many binary archives for pcie build, correct and restart";	\
		ls -l pcie-host-quantenna-bin-*tar;                                     \
		exit 1;                                                                 \
	fi
	@if [ $$(echo dbdc-quantenna-bin-*tar | wc -w) -gt 1 ];                    \
	then                                                                            \
		echo "Too many binary archives for dbdc build, correct and restart";	\
		ls -l dbdc-quantenna-bin-*tar;                                     \
		exit 1;                                                                 \
	fi
	@if [ $$(echo rgmii-quantenna-bin-*tar | wc -w) -gt 1 ];                        \
	then                                                                            \
		echo "Too many binary archives for rgmii build, correct and restart";   \
		ls -l rgmii-quantenna-bin-*tar;                                         \
		exit 1;                                                                 \
	fi
	@if [ $$(echo rfic6-quantenna-bin-*tar | wc -w) -gt 1 ];                        \
	then                                                                            \
		echo "Too many binary archives for rfic6 vb build, correct and restart";   \
		ls -l rfic6-quantenna-bin-*tar;                                         \
		exit 1;                                                                 \
	fi

unpack_binary_archive: verify_binary_archive
	@if [ "$(board_config)" = "topaz_pcie_config" ]; then					\
		if [ -f pcie-quantenna-bin-*.tar ] ; then tar xvf pcie-quantenna-bin-*.tar ;	\
		fi										\
	elif [ "$(board_config)" = "topaz_host_config" ]; then					\
		if [ -f pcie-host-quantenna-bin-*.tar ] ; then tar xvf pcie-host-quantenna-bin-*.tar ; \
		fi                                                                              \
	elif [ "$(board_config)" = "topaz_dbdc_config" ] ; then		                \
		if [ -f dbdc-quantenna-bin-*.tar ] ; then tar xvf dbdc-quantenna-bin-*.tar ; \
		fi                                                                              \
	elif [ "$(board_config:topaz_rfic6%=topaz_rfic6)" = "topaz_rfic6" ]; then					\
		if [ -f rfic6-quantenna-bin-*.tar ] ; then tar xvf rfic6-quantenna-bin-*.tar ;	\
		fi                                                                              \
	else                                                                                    \
		if [ -f quantenna-bin-*.tar ] ; then tar xvf quantenna-bin-*.tar ;              \
		fi                                                                              \
	fi

.PHONY: do_external_modules
do_external_modules: include_gen
	@echo "################################### Making external modules"
	if [ -d drivers ];										\
	then												\
		$(MAKE) -C drivers ARCH=arc CROSS_COMPILE=$(compile_prefix) CHECK=../$(SPARSE);	\
	fi
	if [ -d drivers ];										\
	then												\
		$(MAKE) -C drivers ARCH=arc CROSS_COMPILE=$(compile_prefix) install;			\
	fi
	$(syncconfigs)

external_modules: kernel_modules
	$(MAKE) do_external_modules

ifeq ($(board_platform),topaz)
QTN_AUCFW := auc_topaz
endif

ifeq ($(bb_only),1)
MUC_BUILD_TARGETS += bb_only
endif
MUC_BUILD_TARGETS += qtn_ruby

muc: $(QTN_FW_STAGING_REL) ${QTN_FW_PREBUILD_REL} $(QTN_FW_STAGING_REL)/$(QTN_MACFW_BIN)
dsp: $(QTN_FW_STAGING_REL) ${QTN_FW_PREBUILD_REL} $(QTN_FW_STAGING_REL)/$(QTN_RDSPFW_BIN)
auc: $(QTN_AUCFW)
auc_topaz: $(QTN_FW_STAGING_REL) ${QTN_FW_PREBUILD_REL} $(QTN_FW_STAGING_REL)/$(QTN_AUCFW_BIN)

$(QTN_FW_STAGING_REL) $(QTN_FW_PREBUILD_REL):
	@mkdir -p $@

uncompressed_kimage: common kernel_image $(udir)/tools/mkimage

$(gz_compressed_kimage): uncompressed_kimage
	rm -f $@
	gzip -c --best $(kimage) > $(gz_compressed_kimage)

$(lzma_compressed_kimage): uncompressed_kimage
	rm -f $@
	$(LZMA) -k --best $(kimage)

kernel_base_util := ./host/utilities/kernel_base_util
kernel_base_util_src := $(kernel_base_util).c
$(kernel_base_util): $(kernel_base_util_src) force
	gcc -Wall -Icommon $< -o $@

uboot_qtn_flags = 0x1

$(uncompressed-boot-image): uncompressed_kimage $(tftpdir) $(kernel_base_util)
	@echo "################################### Creating uncompressed U-Boot image"
	$(udir)/tools/mkimage -A arc -C none -T kernel	\
		-a `$(kernel_base_util) -a` -e `$(kernel_base_util) -e`	\
		-name $(board_platform)-linux -Q $(uboot_qtn_flags) \
		-d $(kimage) $(uncompressed-boot-image)

$(gz-boot-image): $(gz_compressed_kimage) $(tftpdir) $(kernel_base_util)
	@echo "################################### Creating gzip compressed U-Boot image"
	$(udir)/tools/mkimage -A arc -C gzip -T kernel	\
		-a `$(kernel_base_util) -e` -e `$(kernel_base_util) -e`	\
		-name $(board_platform)-linux -Q $(uboot_qtn_flags) \
		-d $(gz_compressed_kimage) $(gz-boot-image)

$(lzma-boot-image): $(lzma_compressed_kimage) $(tftpdir) $(kernel_base_util)
	@echo "################################### Creating lzma compressed U-Boot image"
	$(udir)/tools/mkimage -A arc -C lzma -T kernel	\
		-a `$(kernel_base_util) -e` -e `$(kernel_base_util) -e`	\
		-name $(board_platform)-linux -Q $(uboot_qtn_flags) \
		-d $(lzma_compressed_kimage) $(lzma-boot-image)
	@mkdir -p $(QTN_FW_STAGING)/$(board_config)
	cp $@ $(QTN_FW_STAGING)/$(board_config)/$(board_platform)-linux.lzma.img
ifneq ($(NO_CHECK_SIZE),1)
	$(CHECK_LZMA_SIZE) $@ $($(board_config)_maxsize)
endif

strip_modules: kernel_modules external_modules
	@echo "################################### Strip modules"
	if [ "`find $(INSTALL_MOD_PATH) -name '*\.ko'`" != "" ] ; then \
		$(compile_prefix)strip --strip-unneeded  `find $(INSTALL_MOD_PATH) -name '*\.ko'` ; \
	fi

common:
	$(MAKE) -C common

$(udir)/tools/mkimage: | u-boot
	@true # empty rule

$(first_run): | $(QTN_FWS) $(QTN_REGULATORY_DB)
	$(MAKE) copyconfigs
	$(MAKE) do_buildroot && touch $(first_run)

install:
	cp $(uncompressed-boot-image) /tftpboot
	cp $(lzma-boot-image) /tftpboot

all_doxygen:
	@echo "generate document of drivers,mucfw,qcsapi now...."
	rm -rf ./doxygen.tar.bz2 ./doxygen/*
	$(MAKE) -C drivers doxygen
	$(MAKE) -C macfw doxygen
	$(MAKE) -C rdspfw doxygen
	$(MAKE) -C buildroot/package/qcsapi/qcsapi-1.0.1 doxygen
	tar -cvjf doxygen.tar.bz2 ./doxygen/*

doxygen_pdf: force
	@echo "Auto-generated doxygen PDF files for QCSAPI and pktlogger"
	-rm -rf doxygen_pdf
	-rm -rf doxygen
	-rm -f doxygen-${doc_rev_ext}.tar
	mkdir doxygen_pdf
	REV_NUM=$(doc_rev_ext) $(MAKE) -e -C common/ doco
	find doxygen -name '*.pdf' | xargs -i cp {} doxygen_pdf/
	tar cvf doxygen-${doc_rev_ext}.tar doxygen_pdf/

include_gen: force
	(cd include/qtn && make)

show_configs:
	@echo ${ALL_CONFIGS}
