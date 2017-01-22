#
# Let's make some utilities shall we
#
UTILITIES_DIR		=	package/utilities
UTILITIES_WORKDIR	=	$(BUILD_DIR)/utilities_workdir

PROG1				=	wifigun
SRCS1				=	wifigun.c
OBJS1				=	$(SRCS1:%.c=$(UTILITIES_DIR)/%.o)

THE_INCLUDES		= -I$(STAGING_DIR)/include-host -I$(STAGING_DIR_HOST)/include -include endian.h
THE_CFLAGS			=	$(TARGET_CFLAGS) $(TARGET_CFLAGS_EXTRA)

$(UTILITIES_DIR)/$(PROG1): $(OBJS1)
	$(TARGET_CC) $(THE_CFLAGS) $(THE_INCLUDES) $(TARGET_LDFLAGS) -o $@ $(OBJS1)
	cp -af $@ $(TARGET_DIR)/usr/sbin

$(UTILITIES_WORKDIR)/.dir:
	mkdir -p $(UTILITIES_WORKDIR)
	touch $@

$(UTILITIES_WORKDIR)/.built: $(UTILITIES_WORKDIR)/.dir $(UTILITIES_DIR)/$(PROG1)
	touch $@

$(UTILITIES_WORKDIR)/.installed: $(UTILITIES_WORKDIR)/.built
	cp -r $(UTILITIES_DIR)/$(PROG1) $(TARGET_DIR)/usr/sbin
	#touch $@

.PHONY: $(UTILITIES_WORKDIR)/.installed

utilities: uclibc $(UTILITIES_WORKDIR)/.installed

utilities-clean:
	rm -f $(UTILITIES_DIR)/*.o
	rm -f $(UTILITIES_DIR)/$(PROG1)
	rm -f $(UTILITIES_WORKDIR)/.[a-z]*

utilities-dirclean: utilities-clean
	rm -f $(TARGET_DIR)/usr/sbin/$(PROG1)
	rmdir $(UTILITIES_WORKDIR)

$(UTILITIES_DIR)/%.o: $(UTILITIES_DIR)/%.c
	$(TARGET_CC) -c $(THE_CFLAGS) $(THE_INCLUDES) -o $@ $<

ifeq ($(strip $(BR2_PACKAGE_WIFIGUN)),y)
TARGETS += utilities
endif
