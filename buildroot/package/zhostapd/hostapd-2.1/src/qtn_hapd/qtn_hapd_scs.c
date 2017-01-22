/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2014 Quantenna Communications, Inc.          **
**                                                                           **
**  File        : qtn_hapd_scs.c                                             **
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
#include "qtn_hapd_scs.h"

uint16_t  network_checksum(void *addr, int count)
{
        /*
         * Compute Internet Checksum for "count" bytes
         * beginning at location "addr".
         */
        int32_t sum = 0;
        uint16_t *source = (uint16_t *) addr;

        while (count > 1)  {
                /* This is the inner loop */
                sum += *source++;
                count -= 2;
        }

        /*  Add left-over byte, if any */
        if (count > 0) {
                /*
                 * Make sure that the left-over byte is added correctly both
                 * with little and big endian hosts
                 */
                uint16_t tmp = 0;
                *(uint8_t*)&tmp = *(uint8_t*)source;
                sum += tmp;
        }

        /*  Fold 32-bit sum to 16 bits */
        while (sum >> 16)
                sum = (sum & 0xffff) + (sum >> 16);

        return ~sum;
}

int hostapd_scs_update_anounce_pkt(struct hostapd_data *hapd, struct scs_data *scs, int force_update_pkt)
{
        struct ifreq ifr;
        struct hostapd_iface *hapd_iface = hapd->iface;
        int udp_sock = hapd_iface->scs_brcm_sock;
        struct sockaddr_in *paddr;
        int changed = 0;

        os_memset(&ifr, 0, sizeof(ifr));
        os_strlcpy(ifr.ifr_name, SCS_BRCM_RECV_IFACE, sizeof(ifr.ifr_name));

        if (ioctl(udp_sock, SIOCGIFADDR, &ifr) != 0) {
                perror("ioctl(SIOCGIFADDR)");
                return 0;
        }
        paddr = (struct sockaddr_in *) &ifr.ifr_addr;
        if (paddr->sin_family != AF_INET) {
                printf("Invalid address family %i (SIOCGIFADDR)\n",
                       paddr->sin_family);
                return 0;
        }
        if (scs->own.s_addr != paddr->sin_addr.s_addr) {
                scs->own.s_addr = paddr->sin_addr.s_addr;
                changed = 1;
        }

        if (ioctl(udp_sock, SIOCGIFBRDADDR, &ifr) != 0) {
                perror("ioctl(SIOCGIFBRDADDR)");
                return 0;
        }
        paddr = (struct sockaddr_in *) &ifr.ifr_addr;
        if (paddr->sin_family != AF_INET) {
                printf("Invalid address family %i (SIOCGIFBRDADDR)\n",
                       paddr->sin_family);
                return 0;
        }
        if (scs->bcast.s_addr != paddr->sin_addr.s_addr) {
                scs->bcast.s_addr = paddr->sin_addr.s_addr;
                changed = 1;
        }

        if (changed || force_update_pkt) {
                struct ether_header *ethh;
                struct iphdr *iph;
                struct udphdr *uh;
                struct brcm_info_hdr *ah;
                /* prepare the ap bcast packet */
                memset(hapd_iface->scs_brcm_pkt_ap_bcast, 0x0, sizeof(hapd_iface->scs_brcm_pkt_ap_bcast));
                ethh = (struct ether_header *)hapd_iface->scs_brcm_pkt_ap_bcast;
                memset(ethh->ether_dhost, 0xff, ETH_ALEN);
                memcpy(ethh->ether_shost, hapd_iface->scs_brcm_rxif_mac, ETH_ALEN);
                ethh->ether_type = htons(0x0800);
                /* prepare udp and part of ip to calc udp checksum */
                iph = (struct iphdr *)(ethh + 1);
                uh = (struct udphdr *)(iph + 1);
                ah = (struct brcm_info_hdr *)(uh + 1);
                ah->type = host_to_le32(SCS_BRCM_PKT_TYPE_AP_BCAST);
                ah->len = host_to_le32(sizeof(struct brcm_info_hdr));
                iph->protocol = IPPROTO_UDP;
                iph->saddr = scs->own.s_addr;
                iph->daddr = 0xFFFFFFFF;
                uh->source = htons(SCS_BRCM_UDP_PORT);
                uh->dest = htons(SCS_BRCM_UDP_PORT);
                uh->len = htons((sizeof(struct udphdr) + sizeof(struct brcm_info_hdr)));
                iph->tot_len = uh->len;
                uh->check = network_checksum(iph, SCS_BRCM_PKT_IP_MIN_LEN);
                /* fill rest of ip and calc ip checksum */
                iph->tot_len = htons(SCS_BRCM_PKT_IP_MIN_LEN);
                iph->ihl = sizeof(struct iphdr) >> 2;
                iph->version = IPVERSION;
                iph->ttl = IPDEFTTL;
                iph->check = network_checksum(iph, sizeof(struct iphdr));
        }

        return 1;
}

static void hostapd_scs_brcm_bcast(struct hostapd_data *hapd)
{
        struct hostapd_iface *hapd_iface = hapd->iface;
        int udp_sock = hapd_iface->scs_brcm_sock;
        struct scs_data *scs = hapd->scs;

        char buf[sizeof(struct brcm_info_hdr)];
        struct brcm_info_hdr *hdr;
        struct sockaddr_in addr;

        if (scs->master && !hostapd_scs_update_anounce_pkt(hapd, scs, 0)) {
                perror("fail to update anounce packet");
                return;
                /* try later */
        }

        hostapd_logger(scs->hapd, NULL, HOSTAPD_MODULE_SCS,
                       HOSTAPD_LEVEL_DEBUG,
                       "Send brcm bcast to %s\n",
                       scs->iface_name);
        if (hapd->driver && hapd->driver->set_brcm_ioctl) {
                struct ieee80211req_brcm brcm;
                memset(&brcm, 0x0, sizeof(struct ieee80211req_brcm));
                brcm.ib_op = IEEE80211REQ_BRCM_PKT;
                brcm.ib_pkt = hapd_iface->scs_brcm_pkt_ap_bcast;
                brcm.ib_pkt_len = SCS_BRCM_PKT_IP_MIN_LEN + sizeof(struct ether_header);
                hapd->driver->set_brcm_ioctl(hapd->drv_priv, &brcm, sizeof(struct ieee80211req_brcm));
        }
}

static void hostapd_scs_receive_udp(int sock, void *eloop_ctx, void *sock_ctx)
{
        struct scs_data *scs = eloop_ctx;
        struct hostapd_data *hapd = scs->hapd;
        struct hostapd_iface *hapd_iface = hapd->iface;
        int udp_sock = hapd_iface->scs_brcm_sock;
        int len;
        unsigned char buf[128];
        struct sockaddr_in from;
        socklen_t fromlen;
        struct brcm_info_hdr *hdr;
        struct brcm_info_client_intf *info;
        int i;

        fromlen = sizeof(from);
        len = recvfrom(udp_sock, buf, sizeof(buf), 0,
                       (struct sockaddr *) &from, &fromlen);
        if (len < 0) {
                perror("recvfrom");
                return;
        }

        if (from.sin_addr.s_addr == scs->own.s_addr)
                return;

        hostapd_logger(scs->hapd, NULL, HOSTAPD_MODULE_SCS,
                       HOSTAPD_LEVEL_DEBUG,
                       "Received %d byte IAPP frame from %s\n",
                       len, inet_ntoa(from.sin_addr));

        if (len < (int) sizeof(*hdr))
                return;

        hdr = (struct brcm_info_hdr *) buf;
        hostapd_logger(scs->hapd, NULL, HOSTAPD_MODULE_SCS,
                       HOSTAPD_LEVEL_DEBUG,
                       "RX: type=%d len=%d\n",
                       hdr->type, hdr->len);

        switch (hdr->type) {
        case SCS_BRCM_PKT_TYPE_STA_BCAST:
                /* hard to know sta's mac, so broadcast in all bss */
                for (i = 0; i < hapd_iface->num_bss; i++) {
                        hostapd_scs_brcm_bcast(hapd_iface->bss[i]);
                }
                break;
        case SCS_BRCM_PKT_TYPE_STA_INTF:
                info = (struct brcm_info_client_intf*)(hdr + 1);
                hostapd_logger(scs->hapd, info->sta_mac, HOSTAPD_MODULE_SCS,
                       HOSTAPD_LEVEL_DEBUG,
                       "STA BRCM INTF: rssi=%d rxglitch=%d\n",
                       info->sta_rssi, info->rxglitch);

                if (hapd->driver && hapd->driver->set_brcm_ioctl) {
                        struct ieee80211req_brcm brcm;
                        memset(&brcm, 0x0, sizeof(struct ieee80211req_brcm));
                        brcm.ib_op = IEEE80211REQ_BRCM_INFO;
                        brcm.ib_rssi = info->sta_rssi;
                        brcm.ib_rxglitch = info->rxglitch;
                        memcpy(brcm.ib_macaddr, info->sta_mac, IEEE80211_ADDR_LEN);
                        hapd->driver->set_brcm_ioctl(hapd->drv_priv, &brcm, sizeof(struct ieee80211req_brcm));
                }
                break;
        default:
                break;
        }
}

struct scs_data * hostapd_scs_init(struct hostapd_data *hapd, const char *iface)
{
        struct hostapd_iface *hif = hapd->iface;
        struct ifreq ifr;
        struct sockaddr_ll addr;
        int ifindex;
        struct sockaddr_in *paddr, uaddr;
        struct scs_data *scs;
        struct ip_mreqn mreq;
        int on;
        int udp_sock;
	int reuse = 1;

        scs = os_zalloc(sizeof(*scs));
        if (scs == NULL)
                return NULL;
        scs->hapd = hapd;
        scs->packet_sock = -1;
        strcpy(scs->iface_name, iface);

        if (hif->scs_brcm_sock < 0) {
                udp_sock = socket(PF_INET, SOCK_DGRAM, 0);
                if (udp_sock < 0) {
                        perror("socket[PF_INET,SOCK_DGRAM]");
                        hostapd_scs_deinit(scs);
                        return NULL;
                }
                hif->scs_brcm_sock = udp_sock;
                scs->master = 1;

                os_memset(&ifr, 0, sizeof(ifr));
                os_strlcpy(ifr.ifr_name, SCS_BRCM_RECV_IFACE, sizeof(ifr.ifr_name));
                if (ioctl(udp_sock, SIOCGIFHWADDR, &ifr) != 0) {
                        perror("ioctl(SIOCGIFHWADDR)");
                        hostapd_scs_deinit(scs);
                        return NULL;
                }
                memcpy(hif->scs_brcm_rxif_mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

                if (!hostapd_scs_update_anounce_pkt(hapd, scs, 1)) {
                        perror("fail to update anounce pkt");
                        /* try later */
                }

		if (setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
			perror("setsockopt[SOCKET,SO_REUSEADDR]");
			hostapd_scs_deinit(scs);
			return NULL;
		}

                os_memset(&uaddr, 0, sizeof(uaddr));
                uaddr.sin_family = AF_INET;
                uaddr.sin_port = htons(SCS_BRCM_UDP_PORT);
                if (bind(udp_sock, (struct sockaddr *) &uaddr,
                         sizeof(uaddr)) < 0) {
                        perror("bind[UDP]");
                        hostapd_scs_deinit(scs);
                        return NULL;
                }

                if (setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
                        perror("setsockopt[SOCKET,SO_BROADCAST]");
                        hostapd_scs_deinit(scs);
                        return NULL;
                }

                if (eloop_register_read_sock(udp_sock, hostapd_scs_receive_udp,
                                     scs, NULL)) {
                        perror("Could not register read socket for SCS BRCM");
                        hostapd_scs_deinit(scs);
                        return NULL;
                }
        }

        return scs;
}

void hostapd_scs_deinit(struct scs_data *scs)
{
        struct hostapd_data *hapd;
        struct hostapd_iface *hif;

        if (scs == NULL)
                return;

        hapd = scs->hapd;
        hif = hapd->iface;

        if (hif->scs_brcm_sock >= 0) {
                eloop_unregister_read_sock(hif->scs_brcm_sock);
                close(hif->scs_brcm_sock);
                hif->scs_brcm_sock = -1;
        }
        if (hif->scs_ioctl_sock >= 0) {
                close(hif->scs_ioctl_sock);
                hif->scs_ioctl_sock = -1;
        }
        if (scs->packet_sock >= 0) {
                eloop_unregister_read_sock(scs->packet_sock);
                close(scs->packet_sock);
        }

        eloop_cancel_timeout(hostapd_scs_brcm_bcast, scs->hapd, NULL);

        os_free(scs);
}

