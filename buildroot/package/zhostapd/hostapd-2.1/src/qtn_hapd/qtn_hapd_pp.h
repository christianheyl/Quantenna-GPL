/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2014 Quantenna Communications, Inc.          **
**                                                                           **
**  File        : qtn_hapd_pp.h	                                             **
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

#ifndef QTN_HAPD_PP
#define QTN_HAPD_PP


#define WPS_PP_NAME_CHK_REJECT	(0)
#define WPS_PP_NAME_CHK_PASS	(!WPS_PP_NAME_CHK_REJECT)
#define MAX_WPS_PP_DEVICE_NAME_LEN		256
#define MAX_WPS_STA_DEVICE_NAME_LEN		64
#define WPS_PP_ENABLE 1
#define WPS_PP_DISABLE 0

#define PAIRING_ID_MAX_LEN 32
#define PAIRING_HASH_LEN 32
#define PAIRING_HASH_ENABLE 1
#define PAIRING_HASH_DISABLE 0
#define PPS2_DISABLE 0
#define PPS2_MODE_ACCEPT 1
#define PPS2_MODE_DENY 2

#define IEEE80211_IOCTL_POSTEVENT	(SIOCIWFIRSTPRIV+19)

#define MD5_STR_BUF_LEN (MD5_MAC_LEN*2 + 1)
#ifndef string_64
typedef char string_64[65];
#endif

int wps_verify_device_name(const struct wps_context *wps,
                                  const u8 * dev_name,
                                  const size_t dev_name_len);
int hostapd_setup_pairing_hash(const char *pairing_id,
				  const u8 *own_addr,
				  u8 *pairing_hash);
int hostapd_drv_set_pairing_hash_ie(struct hostapd_data *hapd,
				  const u8 *hash_ie,
				  size_t ies_len);
int qtn_hapd_pp2_setup(struct hostapd_data *hapd);
int qtn_hapd_pairingie_handle(void *bss,
				  struct hostapd_data *hapd,
				  u8 *addr,
				  struct ieee80211req_wpaie *ie);
struct hostapd_data *qtn_hostapd_find_bss(void *iface,
				  const char *bss_name);
int qtn_hapd_cfg_wpa_passphrase(void *bss,
				  const char *pos,
				  int errors, int line);
int qtn_non_wps_pp_enable(struct hostapd_data *hapd, char *buf);
int qtn_non_wps_pp_status(struct hostapd_data *hapd,
				  char *buf,
				  char *reply, int reply_len);
int qtn_hapd_cfg_wpa_psk (void *bss, const char *pos,
				  int errors, int line);
const u8 *hostapd_get_psk_md5(const void *bss,
				  const u8 *addr, const u8 *p2p_dev_addr,
				  const u8 *prev_psk_md5);

#endif

