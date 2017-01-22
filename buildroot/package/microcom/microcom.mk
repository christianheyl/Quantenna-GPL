#############################################################
#
# microcom terminal emulator
#
# Maintainer: Tim Riker <Tim@Rikers.org>
#
#############################################################
# Copyright (C) 2001-2003 by Erik Andersen <andersen@codepoet.org>
# Copyright (C) 2002 by Tim Riker <Tim@Rikers.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Library General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA

MICROCOM_EXTRA_CFLAGS:=-DB230400=0010003 -DB460800=0010004

# TARGETS
# http://microcom.port5.com/m102.tar.gz
MICROCOM_SOURCE:=microcom_2009.6.orig.tar.gz
MICROCOM_UNPACK_DIR:=$(BUILD_DIR)/microcom_2009.6.orig/
MICROCOM_DIR:=$(MICROCOM_UNPACK_DIR)/microcom-2009.6/
MICROCOM_LINK:=http://ftp.us.debian.org/debian/pool/main/m/microcom/$(MICROCOM_SOURCE)

$(DL_DIR)/$(MICROCOM_SOURCE):
	$(WGET) -P $(DL_DIR) $(MICROCOM_LINK)

microcom-source: $(DL_DIR)/$(MICROCOM_SOURCE)

$(MICROCOM_DIR)/.unpacked: $(DL_DIR)/$(MICROCOM_SOURCE)
	mkdir -p $(MICROCOM_UNPACK_DIR)
	$(ZCAT) $(DL_DIR)/$(MICROCOM_SOURCE) | tar -C $(MICROCOM_UNPACK_DIR) $(TAR_OPTIONS) -
	touch $@

$(MICROCOM_DIR)/.configured: $(MICROCOM_DIR)/.unpacked
	touch $@

$(MICROCOM_DIR)/microcom: $(MICROCOM_DIR)/.configured
	$(TARGET_CONFIGURE_OPTS) CFLAGS="$(TARGET_CFLAGS) $(MICROCOM_EXTRA_CFLAGS)" $(MAKE) -C $(MICROCOM_DIR)
	$(STRIP) -s $@

$(TARGET_DIR)/usr/bin/microcom: $(MICROCOM_DIR)/microcom
	install -c $(MICROCOM_DIR)/microcom $(TARGET_DIR)/usr/bin/microcom

microcom-clean: 
	rm -f $(MICROCOM_DIR)/*.o $(MICROCOM_DIR)/microcom \
		$(TARGET_DIR)/usr/bin/microcom

microcom-dirclean: 
	rm -rf $(MICROCOM_DIR) 

microcom: uclibc $(TARGET_DIR)/usr/bin/microcom 

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_MICROCOM)),y)
TARGETS+=microcom
endif
