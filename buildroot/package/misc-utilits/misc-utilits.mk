#############################################################
#
# misc-utilits - misc utilits
#
#############################################################


QTN_DISTRIBUTION:="Distributed as binary only."


MISC_UTLS_DIR = $(TOPDIR)/package/misc-utilits

# set your sub dir at here
MISC_UTILITS_BUILD_DIR := $(MISC_UTLS_DIR)/regulatory_database_bin_print




.PHONY: misc-utilits $(MISC_UTILITS_BUILD_DIR)


misc-utilits: $(MISC_UTILITS_BUILD_DIR)

$(MISC_UTILITS_BUILD_DIR):
	$(MAKE) -C $@ PREFIX="$(TARGET_DIR)" \
		CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS)" TOPDIR=$(abspath $(TOPDIR)) \
		all install

misc-utilits-clean:
	$(foreach dir,$(MISC_UTILITS_BUILD_DIR),-$(MAKE) -C $(dir) clean)

misc-utilits-dirclean: misc-utilits-clean

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_RUBY)),y)
TARGETS+=misc-utilits
endif
