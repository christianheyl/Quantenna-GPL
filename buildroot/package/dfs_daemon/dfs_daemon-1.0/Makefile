#
# Copyright (c) 2014 Quantenna Communications, Inc. All rights reserved.
#

# Install directories
VERSION=1.0
INSTALL_DIR= $(PREFIX)/sbin/
CFLAGS += -I$(TOOLCHAIN_DIR)/include/ -I$(BUILD_DIR)/staging_dir/include/ -I$(BUILD_DIR)/../../include/ -I$(BUILD_DIR)/../../drivers/include/shared/ -I$(BUILD_DIR)/../../ -I../../qcsapi/qcsapi-1.0.1/
CFLAGS += $(TARGET_CFLAGS) $(EXTRA_WARNINGS)
CROSS_COMPILE=$(TOOLCHAIN_DIR)/../bin/arc-linux-uclibc-
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
LDLIBS+= $(SYSLIBS)
LDFLAGS+= -Os -Wl,-rpath-link=$(BUILD_DIR)/staging_dir/lib
CFLAGS+= -Os
CFLAGS+= -DVERSION="$(VERSION)"
CFLAGS+= -DUSE_SYSLOG
CFLAGS+= -std=gnu99

DFS_DAEMON_OBJS = dfs_daemon.o ini.o

DFS_DAEMON = dfs_daemon
DFS_DAEMON_CONF = dfs_daemon.conf
all: $(DFS_DAEMON)

dfs_daemon: $(DFS_DAEMON_OBJS)
	$(CC) $(LDFLAGS) -Os -Wall -o $@ $(DFS_DAEMON_OBJS) -L$(BUILD_DIR)/staging_dir/lib/ -lm -lpthread -lqcsapi -lz $(LDLIBS) -Iinclude -D__LINUX

install: all
	install -m 755 -d $(INSTALL_DIR)
	install -m 755 $(DFS_DAEMON) $(INSTALL_DIR)
	cp -f $(DFS_DAEMON_CONF) $(PREFIX)/etc/

.c.o:
	$(CC) $(CFLAGS) -pipe -D__LINUX -Wall -fPIC -c -Iinclude -I.  $<
clean:
	rm -rf $(DFS_DAEMON) *.o *.so *.so.*

distclean: clean
	-rm -rf $(DFS_DAEMON)

