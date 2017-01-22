##
## Please check the configurion parameters below
##

## Installation directory. By default, go in /usr/local.
## Distributions should probably use /, but they probably know better...
ifndef PREFIX
	PREFIX = /usr/local
endif

## by a few percent but make debug worse...
# BUILD_STRIPPING = y

# ***************************************************************************
# ***** Most users should not need to change anything beyond this point *****
# ***************************************************************************

# Targets to build
PROGS= qeventd

# Install directories
INSTALL_DIR= $(PREFIX)/sbin/

# Various commands
RM = rm -f
RM_CMD = $(RM) *.BAK *.bak *.d *.o *.so ,* *~ *.a *.orig *.rej *.out *.gch
LDCONFIG = ldconfig

# Stripping or not ?
ifdef BUILD_STRIPPING
	STRIPFLAGS= -Wl,-s
else
	STRIPFLAGS=
endif

# Other flags
OBJS = qeventd.o sha1.o sha1-internal.o sha1-pbkdf2.o sha256-internal.o md5-internal.o

HEADERS = wireless.h sha1.h sha1_i.h sha256_i.h md5_i.h

INCLUDES = -I.	\
	   -I../../../../drivers/include/shared/	\
	   -I../../qcsapi/qcsapi-1.0.1/			\
	   -I../../../../include/			\
	   -I../../../../

LIBS = -L$(PREFIX)/lib -lqcsapi -lpthread -lz -ldl

CFLAGS += -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wshadow \
	-Wpointer-arith -Wcast-qual -Winline -Werror

DEPFLAGS += -MMD
XCFLAGS += $(CFLAGS) $(DEPFLAGS) $(WARN) $(WELIB_FLAG)

# Standard compilation targets
all:: $(PROGS)

qeventd: $(OBJS)
	$(CC) $(LDFLAGS) $(STRIPFLAGS) $(XCFLAGS) $(INCLUDES) $(LIBS) -o $@ $^ -MD -MF $@.d

$(OBJS): %.o: Makefile

$(OBJS): %.o: %.c $(HEADERS)
	$(CC) $(XCFLAGS) $(INCLUDES) -c $< -MD -MF $@.d

# All the binaries.
install:: all
	install -m 755 -d $(INSTALL_DIR)
	install -m 755 $(PROGS) $(INSTALL_DIR)

clean::
	$(RM_CMD)
	$(RM) $(PROGS)

uninstall::
	for f in $(PROGS); do \
		$(RM) $(INSTALL_DIR)/$$f; \
	done

# Include dependancies
-include *.d
