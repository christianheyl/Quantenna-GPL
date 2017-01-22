#############################################################
#
# Quantenna Web interface
#
#############################################################

QWEBIF_DIR=package/qwebphpif

FORCE:

qwebphpif: FORCE
	mkdir -p $(TARGET_DIR)/var/www
	cp -r $(QWEBIF_DIR)/www/* $(TARGET_DIR)/var/www
	rm -rf $(TARGET_DIR)/var/www/themes/*
	cp $(QWEBIF_DIR)/www/themes/style.css $(TARGET_DIR)/var/www/themes/style.css
	chmod -R u+w $(TARGET_DIR)/var/www/*
	mkdir -p $(TARGET_DIR)/usr/lib/cgi-bin
	cp -f $(QWEBIF_DIR)/admin.conf $(TARGET_DIR)/etc

qwebphpif-clean:

qwebphpif-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_QWEBPHPIF)),y)
TARGETS+=qwebphpif
endif

