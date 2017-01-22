/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2014 Quantenna Communications, Inc.          **
**                                                                           **
**  File        : qtn_hapd_scs.h                                             **
**  Description :                                                            **
**                                                                           **
*******************************************************************************
**  Copyright 1992-2014 The FreeBSD Project. All rights reserved.            **
**  Redistribution and use in source and binary forms, with or without       **
**  modification, are permitted provided that the following conditions       **
**  are met:                                                                 **
**  1. Redistributions of source code must retain the above copyright        **
**     notice, this list of conditions and the following disclaimer.         **
**  2. Redistributions in binary form must reproduce the above copyright     **
**     notice, this list of conditions and the following disclaimer in the   **
**     documentation and/or other materials provided with the distribution.  **
**  3. The name of the author may not be used to endorse or promote products **
**     derived from this software without specific prior written permission. **
**                                                                           **
**  Alternatively, this software may also be distributed under the terms of  **
**  the GNU General Public License ("GPL") version 2, or (at your option)    **
**  any later version as published by the Free Software Foundation.          **
**                                                                           **
**  In the case this software is distributed under the GPL license,          **
**  you should have received a copy of the GNU General Public License        **
**  along with this software; if not, write to the Free Software             **
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA  **
**                                                                           **
**  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR       **
**  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES**
**  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  **
**  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,         **
**  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT **
**  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,**
**  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY    **
**  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT      **
**  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF **
**  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.        **
**                                                                           **
*******************************************************************************
EH1*/

#ifndef QTN_HAPD_SCS
#define QTN_HAPD_SCS

#include "utils/includes.h"
#include <net/if.h>
#include <sys/ioctl.h>
#ifdef USE_KERNEL_HEADERS
#include <linux/if_packet.h>
#else /* USE_KERNEL_HEADERS */
#include <netpacket/packet.h>
#endif /* USE_KERNEL_HEADERS */
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <net80211/ieee80211_ioctl.h>

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "radius/radius_client.h"
#include "drivers/driver.h"
#include "ap/hostapd.h"
#include "ap/authsrv.h"
#include "ap/sta_info.h"
#include "ap/accounting.h"
#include "ap/ap_list.h"
#include "ap/beacon.h"
#include "ap/iapp.h"
#include "ap/ieee802_1x.h"
#include "ap/ieee802_11_auth.h"
#include "ap/vlan_init.h"
#include "ap/wpa_auth.h"
#include "ap/wps_hostapd.h"
#include "ap/hw_features.h"
#include "ap/wpa_auth_glue.h"
#include "ap/ap_drv_ops.h"
#include "ap/ap_config.h"
#include "ap/p2p_hostapd.h"


#define SCS_BRCM_RECV_IFACE                  "br0"
#define SCS_BRCM_UDP_PORT                    49300
#define SCS_BRCM_BCAST_INTVL                  4
#define SCS_BRCM_PKT_TYPE_STA_BCAST           0
#define SCS_BRCM_PKT_TYPE_STA_UNDISCLOSED     1
#define SCS_BRCM_PKT_TYPE_STA_INTF            2
#define SCS_BRCM_PKT_TYPE_AP_BCAST            3
#define SCS_BRCM_PKT_IP_MIN_LEN               (sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct brcm_info_hdr))
#define HOSTAPD_MODULE_SCS		0x00000080


struct scs_data {
        struct hostapd_data *hapd;
        int master;
        char iface_name[IFNAMSIZ];
        struct in_addr own, bcast;
        int packet_sock;
};

struct brcm_info_hdr {
        uint32_t type;
        uint32_t len;
};

/* unpacked format */
struct brcm_info_client_intf {
        uint32_t alert_level;
        unsigned char sta_mac[ETH_ALEN];
        int sta_rssi;
        unsigned rxglitch;
};

struct scs_data * hostapd_scs_init(struct hostapd_data *hapd, const char *iface);
void hostapd_scs_deinit(struct scs_data *scs);

#endif
