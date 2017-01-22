/**
  Copyright (c) 2008 - 2015 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/

#include "hapr_utils.h"
#include "hapr_log.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/sockios.h>

int hapr_linux_set_iface_flags(int sock, const char *ifname, int dev_up)
{
	struct ifreq ifr;
	int ret;

	if (sock < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);

	if (ioctl(sock, SIOCGIFFLAGS, &ifr) != 0) {
		ret = errno ? -errno : -999;
		HAPR_LOG(ERROR, "Could not read interface %s flags: %s", ifname, strerror(errno));
		return ret;
	}

	if (dev_up) {
		if (ifr.ifr_flags & IFF_UP)
			return 0;
		ifr.ifr_flags |= IFF_UP;
	} else {
		if (!(ifr.ifr_flags & IFF_UP))
			return 0;
		ifr.ifr_flags &= ~IFF_UP;
	}

	if (ioctl(sock, SIOCSIFFLAGS, &ifr) != 0) {
		ret = errno ? -errno : -999;
		HAPR_LOG(ERROR, "Could not set interface %s flags (%s): "
			   "%s",
			   ifname, dev_up ? "UP" : "DOWN", strerror(errno));
		return ret;
	}

	return 0;
}

int hapr_linux_br_get(char *brname, const char *ifname)
{
	char path[128], brlink[128], *pos;
	ssize_t res;

	snprintf(path, sizeof(path), "/sys/class/net/%s/brport/bridge",
		ifname);
	res = readlink(path, brlink, sizeof(brlink));
	if (res < 0 || (size_t) res >= sizeof(brlink))
		return -1;
	brlink[res] = '\0';
	pos = strrchr(brlink, '/');
	if (pos == NULL)
		return -1;
	pos++;
	memset(brname, 0, IFNAMSIZ);
	strncpy(brname, pos, IFNAMSIZ - 1);
	return 0;
}

int hapr_linux_br_del_if(int sock, const char *brname, const char *ifname)
{
	struct ifreq ifr;
	int ifindex;

	ifindex = if_nametoindex(ifname);
	if (ifindex == 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, brname, sizeof(ifr.ifr_name) - 1);
	ifr.ifr_ifindex = ifindex;
	if (ioctl(sock, SIOCBRDELIF, &ifr) < 0) {
		HAPR_LOG(DEBUG, "Could not remove interface %s from "
			"bridge %s: %s", ifname, brname, strerror(errno));
		return -1;
	}

	return 0;
}

int hapr_linux_br_add_if(int sock, const char *brname, const char *ifname)
{
	struct ifreq ifr;
	int ifindex;

	ifindex = if_nametoindex(ifname);
	if (ifindex == 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, brname, sizeof(ifr.ifr_name) - 1);
	ifr.ifr_ifindex = ifindex;
	if (ioctl(sock, SIOCBRADDIF, &ifr) < 0) {
		HAPR_LOG(DEBUG, "Could not add interface %s into bridge "
			"%s: %s", ifname, brname, strerror(errno));
		return -1;
	}

	return 0;
}

int hapr_linux_iface_up(int sock, const char *ifname)
{
	struct ifreq ifr;
	int ret;

	if (sock < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);

	if (ioctl(sock, SIOCGIFFLAGS, &ifr) != 0) {
		ret = errno ? -errno : -999;
		HAPR_LOG(ERROR, "Could not read interface %s flags: %s",
			   ifname, strerror(errno));
		return ret;
	}

	return !!(ifr.ifr_flags & IFF_UP);
}

int hapr_qdrv_vap_start(const uint8_t *bssid, const char *ifname)
{
	char bssid_str[20] = {0};
	FILE *qdrv_control;
	char buf[MADWIFI_CMD_BUF_SIZE];
	int ret;

	/* TODO: replace with proper QCSAPI set mode call */
	qdrv_control = fopen("/sys/devices/qdrv/control", "w");
	if (!qdrv_control) {
		HAPR_LOG(ERROR, "cannot open /sys/devices/qdrv/control");
		return -1;
	}

	if (memcmp(bssid, "\x00\x00\x00\x00\x00\x00", ETH_ALEN) == 0) {
		snprintf(buf, sizeof(buf), "start 0 ap %s\n", ifname);
	} else {
		snprintf(bssid_str, sizeof(bssid_str), "%02x:%02x:%02x:%02x:%02x:%02x",
			bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
		snprintf(buf, sizeof(buf), "start 0 ap %s %s\n", ifname, bssid_str);
	}
	ret = fwrite(buf, strlen(buf), 1, qdrv_control);
	fclose(qdrv_control);

	if (!ret) {
		HAPR_LOG(ERROR, "VAP create failed, couldn't write to qdrv control");
		return -1;
	}

	snprintf(buf, sizeof(buf), "echo 1 > /proc/sys/net/ipv6/conf/%s/disable_ipv6", ifname);
	system(buf);

	return 0;
}

int hapr_qdrv_vap_stop(const char *ifname)
{
	char buf[MADWIFI_CMD_BUF_SIZE];
	FILE *qdrv_control;
	int ret;

	/* TODO: replace with proper QCSAPI set mode call */
	qdrv_control = fopen("/sys/devices/qdrv/control", "w");
	if (!qdrv_control) {
		HAPR_LOG(ERROR, "cannot open /sys/devices/qdrv/control");
		return -1;
	}

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf) - 1, "stop 0 %s\n", ifname);
	ret = fwrite(buf, strlen(buf), 1, qdrv_control);
	fclose(qdrv_control);

	if (!ret) {
		HAPR_LOG(ERROR, "VAP remove failed, couldn't write to qdrv control");
		return -1;
	}

	return 0;
}

int hapr_qdrv_write_control(const char *buf)
{
	static const char *const file = "/sys/devices/qdrv/control";

	FILE *qdrv_control;
	int ret;

	qdrv_control = fopen(file, "w");
	if (!qdrv_control)
		return -1;

	ret = fwrite(buf, strlen(buf), 1, qdrv_control);
	fclose(qdrv_control);

	if (!ret)
		return -1;

	return 0;
}

char *hapr_read_config_file(const char *path)
{
	FILE *f;
	int len;
	char *config = NULL;

	f = fopen(path, "r");
	if (f == NULL) {
		HAPR_LOG(ERROR, "Could not open configuration file '%s' "
			   "for reading.", path);
		goto out_open;
	}

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (len < 0) {
		HAPR_LOG(ERROR, "cannot get file '%s' size", path);
		goto out_alloc;
	}

	if (len > (QLINK_MSG_BUF_SIZE - 1)) {
		HAPR_LOG(ERROR, "file '%s' is too big", path);
		goto out_alloc;
	}

	config = malloc(len + 1);
	if (!config) {
		HAPR_LOG(ERROR, "cannot alloc config buffer, no mem");
		goto out_alloc;
	}

	if (fread(config, 1, len, f) != len) {
		HAPR_LOG(ERROR, "error reading config file");
		free(config);
		config = NULL;
		goto out_alloc;
	}

	config[len] = '\0';

out_alloc:
	fclose(f);
out_open:

	return config;
}

int hapr_write_config_file(const char *buff, const char *path, const char *tmp_path)
{
	FILE *f;
	int len = strlen(buff);

	f = fopen(tmp_path, "w");
	if (f == NULL) {
		HAPR_LOG(ERROR, "Could not open temporary configuration file '%s' "
			   "for writing.", tmp_path);
		return -1;
	}

	if (fwrite(buff, 1, len, f) != len) {
		HAPR_LOG(ERROR, "error writing tmp config file %s", tmp_path);
		fclose(f);
		unlink(tmp_path);
		return -1;
	}

	fclose(f);

	if (rename(tmp_path, path) != 0) {
		HAPR_LOG(ERROR, "cannot rename %s to %s", tmp_path, path);
		unlink(tmp_path);
		return -1;
	}

	return 0;
}

void hapr_signal_daemon_parent(int fd, int value)
{
	char c = value;

	while (write(fd, &c, 1) == -1) {
		if (errno != EINTR) {
			break;
		}
	}
}
