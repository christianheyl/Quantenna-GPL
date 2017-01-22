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

#ifndef _QLINK_PROTOCOL_H_
#define _QLINK_PROTOCOL_H_

/*
 * QLINK is a simple asynchronous protocol, peer sends commands
 * and listens for replys.
 */

#include <linux/if_ether.h>

#define QLINK_VERSION 3

#define QLINK_PROTO 0x0602

/*
 * Common.
 */

#define QLINK_GUID_BUF_SIZE 16
#define QLINK_IFNAME_BUF_SIZE 16
#define QLINK_CTRL_ADDR_BUF_SIZE 255
#define QLINK_CTRL_MSG_BUF_SIZE 4096
#define QLINK_MSG_BUF_SIZE (1024 * 1024)
#define QLINK_QOS_MAP_BUF_SIZE 64

#define QLINK_FLAG_WPS (1U << 0)
#define QLINK_FLAG_HS20 (1U << 1)

struct qlink_header
{
	__le32 seq;
	__le16 type;
	__le32 frag;
	__le16 size;
	__u8 is_last;
} __attribute__ ((packed));

#define QLINK_ACK_CMD 0x0
#define QLINK_ACK_REPLY 0x1

/*
 * Commands.
 */

#define QLINK_CMD_FIRST 0x2

typedef enum
{
	QLINK_CMD_HANDSHAKE = 0x2,
	QLINK_CMD_PING = 0x3,
	QLINK_CMD_UPDATE_SECURITY_CONFIG = 0x4,
	QLINK_CMD_BSS_ADD = 0x5,
	QLINK_CMD_BSS_REMOVE = 0x6,
	QLINK_CMD_HANDLE_BRIDGE_EAPOL = 0x7,
	QLINK_CMD_GET_DRIVER_CAPA = 0x8,
	QLINK_CMD_SET_SSID = 0x9,
	QLINK_CMD_SET_80211_PARAM = 0xA,
	QLINK_CMD_SET_80211_PRIV = 0xB,
	QLINK_CMD_COMMIT = 0xC,
	QLINK_CMD_SET_INTERWORKING = 0xD,
	QLINK_CMD_RECV_PACKET = 0xE,
	QLINK_CMD_RECV_IWEVENT = 0xF,
	QLINK_CMD_BRCM = 0x10,
	QLINK_CMD_CHECK_IF = 0x11,
	QLINK_CMD_SET_CHANNEL = 0x12,
	QLINK_CMD_GET_SSID = 0x13,
	QLINK_CMD_SEND_ACTION = 0x14,
	QLINK_CMD_DISCONNECT = 0x15,
	QLINK_CMD_INIT_CTRL = 0x16,
	QLINK_CMD_RECV_CTRL = 0x17,
	QLINK_CMD_SEND_CTRL = 0x18,
	QLINK_CMD_INVALIDATE_CTRL = 0x19,
	QLINK_CMD_VAP_ADD = 0x1A,
	QLINK_CMD_VLAN_SET_STA = 0x1B,
	QLINK_CMD_VLAN_SET_DYN = 0x1C,
	QLINK_CMD_VLAN_SET_GROUP = 0x1D,
	QLINK_CMD_READY = 0x1E,
	QLINK_CMD_SET_QOS_MAP = 0x1F,
} qlink_cmd;

struct qlink_cmd_handshake
{
	__u8 guid[QLINK_GUID_BUF_SIZE];
	__le32 version;
	__le32 ping_timeout;
	__le32 flags;
} __attribute__ ((packed));

struct qlink_cmd_update_security_config
{
	__u8 data[1];
} __attribute__ ((packed));

struct qlink_cmd_bss_add
{
	__le32 qid;
	char ifname[QLINK_IFNAME_BUF_SIZE];
	char brname[QLINK_IFNAME_BUF_SIZE];
} __attribute__ ((packed));

struct qlink_cmd_bss_remove
{
	__le32 qid;
} __attribute__ ((packed));

struct qlink_cmd_handle_bridge_eapol
{
	__le32 bss_qid;
} __attribute__ ((packed));

struct qlink_cmd_get_driver_capa
{
	__le32 bss_qid;
} __attribute__ ((packed));

struct qlink_cmd_set_ssid
{
	__le32 bss_qid;
	__u8 ssid[1];
} __attribute__ ((packed));

struct qlink_cmd_set_80211_param
{
	__le32 bss_qid;
	__le32 op;
	__le32 arg;
} __attribute__ ((packed));

struct qlink_cmd_set_80211_priv
{
	__le32 bss_qid;
	__le32 op;
	__u8 data[1];
} __attribute__ ((packed));

struct qlink_cmd_commit
{
	__le32 bss_qid;
} __attribute__ ((packed));

struct qlink_cmd_set_interworking
{
	__le32 bss_qid;
	__u8 eid;
	__u8 interworking;
	__u8 access_network_type;
	__u8 hessid[ETH_ALEN];
} __attribute__ ((packed));

struct qlink_cmd_recv_packet
{
	__le32 bss_qid;
	__le16 protocol;
	__u8 src_addr[ETH_ALEN];
	__u8 data[1];
} __attribute__ ((packed));

struct qlink_cmd_recv_iwevent
{
	__le32 bss_qid;
	__le16 cmd;
	__u8 data[1];
} __attribute__ ((packed));

struct qlink_cmd_brcm
{
	__le32 bss_qid;
	__u8 data[1];
} __attribute__ ((packed));

struct qlink_cmd_check_if
{
	__le32 bss_qid;
} __attribute__ ((packed));

struct qlink_cmd_set_channel
{
	__le32 bss_qid;
	__le32 channel;
} __attribute__ ((packed));

struct qlink_cmd_get_ssid
{
	__le32 bss_qid;
	__le32 len;
} __attribute__ ((packed));

struct qlink_cmd_send_action
{
	__le32 bss_qid;
	__u8 data[1];
} __attribute__ ((packed));

struct qlink_cmd_init_ctrl
{
	char ctrl_dir[QLINK_CTRL_ADDR_BUF_SIZE];
	char ctrl_name[QLINK_IFNAME_BUF_SIZE];
} __attribute__ ((packed));

struct qlink_cmd_recv_ctrl
{
	char addr[QLINK_CTRL_ADDR_BUF_SIZE];
	__u8 data[1];
} __attribute__ ((packed));

struct qlink_cmd_send_ctrl
{
	char addr[QLINK_CTRL_ADDR_BUF_SIZE];
	__u8 data[1];
} __attribute__ ((packed));

struct qlink_cmd_invalidate_ctrl
{
	char addr[QLINK_CTRL_ADDR_BUF_SIZE];
} __attribute__ ((packed));

struct qlink_cmd_vap_add
{
	__le32 qid;
	__u8 bssid[ETH_ALEN];
	char ifname[QLINK_IFNAME_BUF_SIZE];
	char brname[QLINK_IFNAME_BUF_SIZE];
} __attribute__ ((packed));

struct qlink_cmd_vlan_set_sta
{
	__u8 addr[ETH_ALEN];
	__le32 vlan_id;
} __attribute__ ((packed));

struct qlink_cmd_vlan_set_dyn
{
	char ifname[QLINK_IFNAME_BUF_SIZE];
	__u8 enable;
} __attribute__ ((packed));

struct qlink_cmd_vlan_set_group
{
	char ifname[QLINK_IFNAME_BUF_SIZE];
	__le32 vlan_id;
	__u8 enable;
} __attribute__ ((packed));

struct qlink_cmd_set_qos_map
{
	__le32 bss_qid;
	__u8 qos_map[QLINK_QOS_MAP_BUF_SIZE];
} __attribute__ ((packed));

/*
 * Replys.
 */

#define QLINK_REPLY_FIRST 0x1000

typedef enum
{
	QLINK_REPLY_STATUS = QLINK_REPLY_FIRST + 0x0,
	QLINK_REPLY_BSS_ADD = QLINK_REPLY_FIRST + 0x1,
	QLINK_REPLY_GET_DRIVER_CAPA = QLINK_REPLY_FIRST + 0x2,
	QLINK_REPLY_SET_80211_PRIV = QLINK_REPLY_FIRST + 0x3,
	QLINK_REPLY_BRCM = QLINK_REPLY_FIRST + 0x4,
	QLINK_REPLY_GET_SSID = QLINK_REPLY_FIRST + 0x5,
	QLINK_REPLY_VAP_ADD = QLINK_REPLY_FIRST + 0x6,
} qlink_reply;

struct qlink_reply_status
{
	__le32 status;
} __attribute__ ((packed));

struct qlink_reply_bss_add
{
	__u8 own_addr[ETH_ALEN];
} __attribute__ ((packed));

struct qlink_reply_get_driver_capa
{
	__le32 we_version;
	__u8 extended_capa_len;
	__u8 data[1];
} __attribute__ ((packed));

struct qlink_reply_set_80211_priv
{
	__u8 data[1];
} __attribute__ ((packed));

struct qlink_reply_brcm
{
	__u8 data[1];
} __attribute__ ((packed));

struct qlink_reply_get_ssid
{
	__u8 ssid[1];
} __attribute__ ((packed));

struct qlink_reply_vap_add
{
	__u8 own_addr[ETH_ALEN];
} __attribute__ ((packed));

#endif
