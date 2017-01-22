#############################################################
#
# Quantenna CGI Web interface
#
#############################################################

QWEBCGIIF_DIR=package/qwebcgiif

FORCE:

qwebcgiif: FORCE
	mkdir -p $(TARGET_DIR)/var/www
	cp -r $(QWEBCGIIF_DIR)/www/* $(TARGET_DIR)/var/www
	chmod -R u+w $(TARGET_DIR)/var/www/cgi/*
	cp -f $(QWEBCGIIF_DIR)/www/admin.conf $(TARGET_DIR)/etc

qwebcgiif-clean:

qwebcgiif-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_QWEBCGIIF)),y)
TARGETS+=qwebcgiif
endif

