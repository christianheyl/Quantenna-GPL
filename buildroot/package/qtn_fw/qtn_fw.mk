#############################################################
#
# Quantenna - include qtn build artefacts inside a parent image
#
#############################################################

define copy_fw
	mkdir -p ${TARGET_DIR}/$2/
	for file in $1 ; do		\
		cp $$file ${TARGET_DIR}/$2/`basename $$file` ;	\
		chmod 444 ${TARGET_DIR}/$2/`basename $$file` ;	\
	done
endef

QTN_DISTRIBUTION:="Distributed as binary only."

#############################################################
# Firmware relevant to current build

qtn_macfw:
	$(call copy_fw,$(QTN_FW_STAGING)/qtn_driver.*.bin,/etc/firmware)

qtn_dspfw:
	$(call copy_fw,$(QTN_FW_STAGING)/rdsp_driver.*.bin,/etc/firmware)

qtn_uboot:
ifeq ($(wildcard $(QTN_LINUX_IMG_DIR)/u-boot.bin),)
	$(call copy_fw,$(QTN_FW_STAGING)/u-boot*,/etc/firmware)
else
	$(call copy_fw,$(QTN_LINUX_IMG_DIR)/u-boot.bin,/etc/firmware)
endif

qtn_aucfw:
	$(call copy_fw,$(QTN_FW_STAGING)/auc_driver.*.bin,/etc/firmware)

qtn_prepopulate:
	# Prepopulating target rootfs with files...
	rsync -auv ../prepopulate/* ${TARGET_DIR}/

define copy_updater
	rm -f $1.update.sh
	make -f ../host/utilities/create_fwupdate_sh.mk $1.update.sh
	$(call copy_fw,$1.update.sh,/etc/firmware)
endef

qtn_uboot_updater:
	$(call copy_updater,../u-boot/u-boot.bin)

qtn_mini_uboot_updater:
	$(call copy_updater,../u-boot/u-boot-mini-piggy.bin)

ifeq ($(strip $(BR2_PACKAGE_QTN_MACFW)),y)
TARGETS += qtn_macfw
endif

ifeq ($(strip $(BR2_PACKAGE_QTN_DSPFW)),y)
TARGETS += qtn_dspfw
endif

ifeq ($(strip $(BR2_PACKAGE_QTN_UBOOT)),y)
TARGETS += qtn_uboot
endif

ifeq ($(strip $(BR2_PACKAGE_QTN_AUCFW)),y)
TARGETS += qtn_aucfw
endif

ifeq ($(strip $(BR2_PACKAGE_QTN_UBOOT_UPGRADE_SCRIPT)),y)
TARGETS += qtn_uboot_updater
endif

ifeq ($(strip $(BR2_PACKAGE_QTN_PREPOPULATE)),y)
TARGETS += qtn_prepopulate
endif

ifeq ($(strip $(BR2_PACKAGE_QTN_MINI_UBOOT_UPGRADE_SCRIPT)),y)
TARGETS += qtn_mini_uboot_updater
endif

#############################################################
# Firmware relevant to child build
CHILD_BUILD_PATH ?= ..

qtn_linux_image:
ifeq ($(QTN_LINUX_IMG_DIR),)
	$(call copy_fw,$(CHILD_BUILD_PATH)/$(QTN_FW_STAGING_REL)/$(BR2_PACKAGE_QTN_LINUX_IMG_CONFIG)/$(board_platform)-linux.lzma.img,/etc/firmware)
else
	$(call copy_fw,$(QTN_LINUX_IMG_DIR)/$(board_platform)-linux.lzma.img,/etc/firmware)
endif

ifeq ($(strip $(BR2_PACKAGE_QTN_LINUX_IMG)),y)
TARGETS += qtn_linux_image
endif

