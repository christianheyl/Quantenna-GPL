
.PHONY: dfs_daemon dfs_daemon-clean

DFS_DAEMON_DIR=$(TOPDIR)/package/dfs_daemon
DFS_DAEMON_NAME:=dfs_daemon
DFS_DAEMON_VER:=1.0
DFS_DAEMON_PKG=$(DFS_DAEMON_NAME)-$(DFS_DAEMON_VER)
DFS_DAEMON_BUILD_DIR=$(DFS_DAEMON_DIR)/$(DFS_DAEMON_PKG)
EXTRA_WARNINGS= -Wall -Wshadow
QTN_DISTRIBUTION:="Distributed as source code in SDK"

dfs_daemon: zlib qcsapi
	$(MAKE) -C $(DFS_DAEMON_BUILD_DIR) PREFIX="$(TARGET_DIR)"  \
		BUILD_DIR="$(BUILD_DIR)" TOOLCHAIN_DIR="$(TOOLCHAIN_EXTERNAL_PATH)/$(TOOLCHAIN_EXTERNAL_PREFIX)"\
		install

dfs_daemon-clean:
	-$(MAKE) -C $(DFS_DAEMON_DIR) clean

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_DFS_DAEMON)),y)
TARGETS+=dfs_daemon
endif

