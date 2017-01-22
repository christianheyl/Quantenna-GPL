/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2012 Quantenna Communications. Inc.                 **
**                            All Rights Reserved                            **
**                                                                           **
*******************************************************************************
**                                                                           **
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
**  Alternatively, this software may be distributed under the terms of the   **
**  GNU General Public License ("GPL") version 2, or (at your option) any    **
**  later version as published by the Free Software Foundation.              **
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

#ifndef _IPUTIL_H_
#define _IPUTIL_H_

#include <linux/inetdevice.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <net80211/if_ethersubr.h>
#if defined(CONFIG_IPV6)
#include <net/ipv6.h>
#include <linux/in6.h>
#include <linux/inet.h>
#include <net/addrconf.h>
#endif

#define IPUTIL_HDR_VER_4	4
#define IPUTIL_HDR_VER_6	6

#define IPUTIL_V4_ADDR_SSDP		htonl(0xEFFFFFFA) /* 239.255.255.250 */
#define IPUTIL_V4_ADDR_MULTICAST(_addr)	\
	((_addr & htonl(0xF0000000)) == htonl(0xE0000000)) /* 224.0.0.0/4 */
#define IPUTIL_V6_ADDR_MULTICAST(_addr)	\
	((_addr & htonl(0xFF000000)) == htonl(0xFF000000)) /* ff00::/8 - see __ipv6_addr_type() */
#define IPUTIL_V4_ADDR_LNCB(_addr)	\
	((_addr & htonl(0xFFFFFF00)) == htonl(0xE0000000)) /* 224.0.0.0/24 */

#define IPUTIL_V4_FRAG_OFFSET(_fh)	(ntohs(_fh->frag_off) & ~0x7)
#define IPUTIL_V4_FRAG_MF(_fh)		(ntohs(_fh->frag_off) & IP6_MF)

#define IPUTIL_V6_FRAG_OFFSET(_fh)	(ntohs(_fh->frag_off) & ~0x7)
#define IPUTIL_V6_FRAG_MF(_fh)		(ntohs(_fh->frag_off) & IP6_MF)

#define NIPV6OCTA_FMT "%pI6"
#define NIPV6OCTA(_ipv6_addr_) _ipv6_addr_

#define IPUTIL_V4_ADDR_LEN 4

#ifdef CONFIG_IPV6
int iputil_v6_skip_exthdr(const struct ipv6hdr *ipv6h, int start, uint8_t *nexthdrp,
				int total_len, __be32 *ip_id, uint8_t *more_frags);
int iputil_v6_ntop(char *buf, const struct in6_addr *addr);
int iputil_v6_ntop_port(char *buf, const struct in6_addr *addr, __be16 port);
int iputil_eth_is_v6_mld(void *iphdr, uint32_t data_len);

int iputil_ipv6_is_neigh_msg(struct ipv6hdr *ipv6, struct icmp6hdr *icmpv6);
int iputil_ipv6_is_neigh_sol_msg(uint8_t dad, struct nd_msg *msg, struct ipv6hdr *ipv6);
#endif

int iputil_v4_pton(const char *ip_str, __be32 *ipaddr);
int iputil_v4_ntop_port(char *buf, __be32 addr, __be16 port);

/*
 * IPv6 broadcasts are scoped multicast.
 * +--------+----+----+---------------------------------------------+
 * | 8      | 4  | 4  |                 112 bits                    |
 * +------ -+----+----+---------------------------------------------+
 * |11111111|flgs|scop|                 group ID                    |
 * +--------+----+----+---------------------------------------------+
 *
 *  Scope:
 *  1: Interface-Local (loopback)
 *  2: Link-Local
 *  4: Admin-Local
 *  5: Site-Local
 *  8: Organization-Local
 *  E: Global
 *  0,3,F: reserved
 *  others: unassigned, are available for administrators to define additional multicast regions.
 *
 *  RFC4291 http://www.iana.org/assignments/ipv6-multicast-addresses/ipv6-multicast-addresses.xml
 */
#ifdef CONFIG_IPV6
static inline int iputil_mac_is_v6_local(struct ipv6hdr *ipv6h)
{
	struct in6_addr *ipaddr = &ipv6h->daddr;

	return ((ipaddr->in6_u.u6_addr8[0] == 0xff) &&
		(ipaddr->in6_u.u6_addr8[1] > 0x01) &&
		(ipaddr->in6_u.u6_addr8[1] < 0x0E));
}
#endif

static inline int iputil_is_v4_ssdp(const void *addr, void *iph)
{
	static const char ssdp_addr[] = {0x01, 0x00, 0x5E, 0x7F, 0xFF, 0xFA};

	if (unlikely(!memcmp(addr, ssdp_addr, sizeof(ssdp_addr)))) {
		struct iphdr *ipv4h = iph;
		uint32_t daddr = get_unaligned((uint32_t *)&ipv4h->daddr);

		if (daddr == IPUTIL_V4_ADDR_SSDP) {
			return 1;
		}
	}

	return 0;
}

#ifdef CONFIG_IPV6
/*
 * IPv6 SSDP is 0xff0?::c
 */
static inline int iputil_is_v6_ssdp(const unsigned char *dest, struct ipv6hdr *ipv6h)
{
	static const uint8_t ssdp6_addr[ETH_ALEN] = {0x33, 0x33, 0x00, 0x00, 0x00, 0x0c};
	struct in6_addr *ipaddr = &ipv6h->daddr;

	return ((memcmp(dest, &ssdp6_addr, sizeof(ssdp6_addr)) == 0) &&
		((__constant_ntohl(ipaddr->in6_u.u6_addr32[0]) & 0xfff0ffff) == 0xff000000) &&
		(ipaddr->in6_u.u6_addr32[1] == 0) && (ipaddr->in6_u.u6_addr32[2] == 0) &&
		(ipaddr->in6_u.u6_addr32[3] == __constant_htonl(0xc)));
}
#endif

static inline int iputil_is_ssdp(const void *addr, void *iph)
{
	if (iputil_is_v4_ssdp(addr, iph)) {
		return 1;
	}

#ifdef CONFIG_IPV6
	if (unlikely(iputil_is_v6_ssdp(addr, iph))) {
		return 1;
	}
#endif
	return 0;
}

#ifdef CONFIG_IPV6
/*
 * IPv6 all-nodes multicast address
 * the link-local scope address to reach all nodes is 0xff02::1
 */
static inline int iputil_ipv6_is_ll_all_nodes_mc(const unsigned char *dest, void *iph)
{
	struct ipv6hdr *ipv6h = (struct ipv6hdr *)iph;
	static const uint8_t ll_all_nodes_mac_addr[ETH_ALEN] = {0x33, 0x33, 0x00, 0x00, 0x00, 0x01};
	struct in6_addr *ipaddr = &ipv6h->daddr;

	return ((memcmp(dest, ll_all_nodes_mac_addr, sizeof(ll_all_nodes_mac_addr)) == 0) &&
		(__constant_ntohl(ipaddr->in6_u.u6_addr32[0]) == 0xff020000) &&
		(ipaddr->in6_u.u6_addr32[1] == 0) && (ipaddr->in6_u.u6_addr32[2] == 0) &&
		(ipaddr->in6_u.u6_addr32[3] == __constant_htonl(0x1)));
}
#endif

/* Check for a local network control block MAC address */
static inline int iputil_is_lncb(const uint8_t *addr, const void *iph)
{
	static const char lncb_addr[] = {0x01, 0x00, 0x5E, 0x00, 0x00};

	if (unlikely(!memcmp(addr, lncb_addr, sizeof(lncb_addr)))) {
		const struct iphdr *ipv4h = iph;
		uint32_t daddr = get_unaligned((uint32_t *)&ipv4h->daddr);

		if (IPUTIL_V4_ADDR_LNCB(daddr)) {
			return 1;
		}
	}
	return 0;
}

static inline int iputil_is_multicast(void *iph)
{
	const struct iphdr *ipv4h = iph;

	if (ipv4h->version == 4) {
		uint32_t daddr = get_unaligned((uint32_t *)&ipv4h->daddr);

		return IPUTIL_V4_ADDR_MULTICAST(daddr);
	}

#ifdef CONFIG_IPV6
	if (ipv4h->version == 6) {
		struct ipv6hdr *ipv6h = iph;
		__be32 daddr = get_unaligned(ipv6h->daddr.s6_addr32);

		return IPUTIL_V6_ADDR_MULTICAST(daddr);
	}
#endif
	return 0;
}

static inline size_t iputil_hdrlen(void *iph, uint32_t data_len)
{
	const struct iphdr *ipv4h = iph;
#ifdef CONFIG_IPV6
	const struct ipv6hdr *ip6hdr_p = iph;
	uint8_t nexthdr;
	int nhdr_off;
#endif

	if (likely(ipv4h->version == 4)) {
		return (ipv4h->ihl << 2);
	}

#ifdef CONFIG_IPV6
	/*
	 * This is the base IPv6 header. If the next header is an option header, its length must be
	 * accounted for explicitly elsewhere.
	 */
	if (ipv4h->version == 6) {
		nhdr_off = iputil_v6_skip_exthdr(ip6hdr_p,
			sizeof(struct ipv6hdr),
			&nexthdr, data_len, NULL, NULL);
		return nhdr_off;
	}
#endif
	return 0;
}

static inline int iputil_mac_is_v6_multicast(const uint8_t *mac)
{
	const char ipmc6_addr[] = {0x33, 0x33};

	return mac[0] == ipmc6_addr[0] &&
		mac[1] == ipmc6_addr[1];
}

static inline int iputil_mac_is_v4_multicast(const uint8_t *mac)
{
	const char ipmc4_addr[] = {0x01, 0x00, 0x5E};

	return mac[0] == ipmc4_addr[0] &&
		mac[1] == ipmc4_addr[1] &&
		mac[2] == ipmc4_addr[2];
}

static inline int iputil_eth_is_type(const struct ether_header *eh, const uint16_t ether_type)
{
	if (eh->ether_type == __constant_htons(ETH_P_8021Q)) {
		return (*(&eh->ether_type + 2) == ether_type);
	}

	return (eh->ether_type == ether_type);
}

static inline int iputil_eth_is_v6_multicast(const struct ether_header *eh)
{

	return iputil_eth_is_type(eh, __constant_htons(ETH_P_IPV6)) &&
		iputil_mac_is_v6_multicast(eh->ether_dhost);
}

static inline int iputil_eth_is_v4_multicast(const struct ether_header *eh)
{
	return iputil_eth_is_type(eh, __constant_htons(ETH_P_IP)) &&
		iputil_mac_is_v4_multicast(eh->ether_dhost);
}

static inline int iputil_eth_is_multicast(const struct ether_header *eh)
{
	if (iputil_eth_is_v4_multicast(eh)) {
		return 1;
	}

#ifdef CONFIG_IPV6
	if (iputil_eth_is_v6_multicast(eh)) {
		return 1;
	}
#endif
	return 0;
}

static inline int iputil_eth_is_ipv4or6(uint16_t ether_type)
{
	return ether_type == __constant_htons(ETH_P_IP) ||
		ether_type == __constant_htons(ETH_P_IPV6);
}

/* Multicast data traffic, with the most common types of non-streaming mc filtered out */
static inline int iputil_is_mc_data(const struct ether_header *eh, void *iph)
{
	return iputil_eth_is_multicast(eh) &&
		!iputil_is_lncb(eh->ether_dhost, iph) &&
		!iputil_is_ssdp(eh->ether_dhost, iph);
}

uint8_t iputil_proto_info(void *iph, void *data,
	void **proto_data, uint32_t *ip_id, uint8_t *more_frags);

static inline struct igmphdr *iputil_igmp_hdr(struct iphdr *p_iphdr)
{
	return (struct igmphdr *)((unsigned int*)p_iphdr + p_iphdr->ihl);
}

struct dhcp_message {
	uint8_t op;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	uint32_t xid;
	uint16_t secs;
	uint16_t flags;
	uint32_t ciaddr;
	uint32_t yiaddr;
	uint32_t siaddr;
	uint32_t giaddr;
	uint8_t chaddr[16];
	uint8_t sname[64];
	uint8_t file[128];
	uint32_t cookie;
	uint8_t options[0];
}__attribute__ ((packed));

#define DHCPSERVER_PORT		67
#define DHCPCLIENT_PORT		68

#define DHCPV6SERVER_PORT	547
#define DHCPV6CLIENT_PORT	546

#define BOOTREQUEST		1
#define DHCPREQUEST		3
#define ARPHRD_ETHER		1
#define DHCP_BROADCAST_FLAG	0x8000

#endif
