/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2014 Quantenna Communications, Inc.          **
**                                                                           **
**  File        : qtn_hapd_bss.c                                             **
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
#include "utils/includes.h"
#include <net/if.h>
#include <sys/ioctl.h>
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
#include "qtn_hapd_bss.h"

static int hostapd_set_ap_isolate(struct hostapd_data *hapd, int value)
{
        if (hapd->driver == NULL || hapd->driver->set_intra_bss == NULL) {
                return 0;
        }

        if (!hapd->primary_interface) {
                return 0;
        }

        return hapd->driver->set_intra_bss(hapd->drv_priv, value);
}

static int hostapd_set_intra_bss_isolate(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_intra_per_bss == NULL)
		return 0;

	return hapd->driver->set_intra_per_bss(hapd->drv_priv, value);
}

static int hostapd_set_bss_isolate(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_bss_isolate == NULL)
		return 0;

	return hapd->driver->set_bss_isolate(hapd->drv_priv, value);
}

static int hostapd_set_dynamic_vlan(struct hostapd_data *hapd, const char *ifname, int enable)
{
	if (hapd->driver == NULL || hapd->driver->set_dyn_vlan == NULL)
		return 0;

	return hapd->driver->set_dyn_vlan(hapd->drv_priv, ifname, enable);
}

int hostapd_set_total_assoc_limit(struct hostapd_data *hapd, int limit)
{
        if (hapd->driver == NULL || hapd->driver->set_total_assoc_limit == NULL) {
                return 0;
        }

        if (!hapd->primary_interface) {
                return 0;
        }

        return hapd->driver->set_total_assoc_limit(hapd->drv_priv, limit);
}

int hostapd_set_bss_assoc_limit(struct hostapd_data *hapd, int limit)
{
        if (hapd->driver == NULL || hapd->driver->set_bss_assoc_limit == NULL) {
                return 0;
        }

        return hapd->driver->set_bss_assoc_limit(hapd->drv_priv, limit);
}

int hostapd_set_bss_params(struct hostapd_data *hapd)
{
        int ret = 0;

        if (hostapd_set_ap_isolate(hapd, hapd->conf->isolate) &&
            hapd->conf->isolate) {
                wpa_printf(MSG_ERROR, "Could not enable AP isolation in "
                        "kernel driver");
                ret = -1;
        }

	if (hostapd_set_intra_bss_isolate(hapd, hapd->conf->intra_bss_isolate) &&
			hapd->conf->intra_bss_isolate) {
		wpa_printf(MSG_ERROR, "Could not enable intra-bss isolation in "
			"kernel driver");
		ret = -1;
	}

	if (hostapd_set_bss_isolate(hapd, hapd->conf->bss_isolate) &&
			hapd->conf->bss_isolate) {
		wpa_printf(MSG_ERROR, "Could not enable bss isolation in "
			   "kernel driver");
		ret = -1;
	}

	if (hostapd_set_dynamic_vlan(hapd, hapd->conf->iface, hapd->conf->ssid.dynamic_vlan)) {
		wpa_printf(MSG_ERROR, "Could not enable/disable BSS dynamic mode\n");
		ret = -1;
	}

        return ret;
}

int qtn_hapd_acl_reject(struct hostapd_data *hapd, const u8 *own_addr)
{
	int allowed, res;

	allowed = hostapd_allowed_address(hapd, own_addr, NULL, 0, NULL,
			NULL, NULL, NULL, NULL, NULL);
	if (allowed == HOSTAPD_ACL_REJECT) {
		hostapd_notif_disassoc(hapd, own_addr);
	}

	return (allowed == HOSTAPD_ACL_REJECT);
}

void hostapd_send_wlan_msg(struct hostapd_data *hapd, const char *msg)
{
	if (hapd->driver != NULL && hapd->driver->send_log != NULL) {
		hapd->driver->send_log(hapd->drv_priv, (char *)msg);
	}
}

int hostapd_set_broadcast_ssid(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_broadcast_ssid == NULL)
		return 0;
	return hapd->driver->set_broadcast_ssid(hapd->drv_priv, value);
}
