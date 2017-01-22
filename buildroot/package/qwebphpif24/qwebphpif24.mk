#############################################################
#
# Quantenna Web interface for QV860
#
#############################################################

QWEBIF24_DIR=package/qwebphpif24

FORCE:

qwebphpif24: FORCE
	mkdir -p $(TARGET_DIR)/var/www
	cp -r $(QWEBIF24_DIR)/www/* $(TARGET_DIR)/var/www
	rm -rf $(TARGET_DIR)/var/www/themes/*
	cp $(QWEBIF24_DIR)/www/themes/style.css $(TARGET_DIR)/var/www/themes/style.css
	chmod -R u+w $(TARGET_DIR)/var/www/*
	mkdir -p $(TARGET_DIR)/usr/lib/cgi-bin
	cp -f $(QWEBIF24_DIR)/admin.conf $(TARGET_DIR)/etc

qwebphpif24-clean:

qwebphpif24-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_QWEBPHPIF24)),y)
TARGETS+=qwebphpif24
endif

