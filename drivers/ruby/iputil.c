/**
 * Copyright (c) 2012 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#include <compat.h>
#include <net/ip.h>
#ifdef CONFIG_IPV6
#include <net/ipv6.h>
#endif

#include <qtn/iputil.h>

#ifdef CONFIG_IPV6
/* optimisation of the ipv6_skip_exthdr() kernel function */
int __sram_text
iputil_v6_skip_exthdr(const struct ipv6hdr *ipv6h, int start, uint8_t *nexthdrp,
			int total_len, __be32 *ip_id, uint8_t *more_frags)
{
	uint8_t nexthdr = ipv6h->nexthdr;
	struct frag_hdr *frag_hdrp;
	struct ipv6_opt_hdr *hp;
	int hdrlen;

	while ((start < total_len) && ipv6_ext_hdr(nexthdr) && (nexthdr != NEXTHDR_NONE)) {
		hp = (struct ipv6_opt_hdr *)((char *)ipv6h + start);

		if (unlikely(nexthdr == NEXTHDR_FRAGMENT)) {
			frag_hdrp = (struct frag_hdr *)hp;

			if (ip_id != NULL) {
				*ip_id = ntohl(get_unaligned(&frag_hdrp->identification));
			}

			KASSERT((((int)frag_hdrp) & 0x1) == 0,
				("iputil: frag hdr is not on 2-octet boundary"));

			if (more_frags != NULL) {
				*more_frags = IPUTIL_V6_FRAG_MF(frag_hdrp);
			}
			if (IPUTIL_V6_FRAG_OFFSET(frag_hdrp)) {
				/* not start of packet - does not contain protocol hdr */
				break;
			}
			hdrlen = 8;
		} else if (unlikely(nexthdr == NEXTHDR_AUTH)) {
			hdrlen = (hp->hdrlen + 2) << 2;
		} else {
			hdrlen = ipv6_optlen(hp);
		}

		if ((start + hdrlen) > total_len) {
			nexthdr = NEXTHDR_NONE;
			break;
		}
		nexthdr = hp->nexthdr;
		start += hdrlen;
	}

	*nexthdrp = nexthdr;

	return start;
}
EXPORT_SYMBOL(iputil_v6_skip_exthdr);

int iputil_v6_ntop(char *buf, const struct in6_addr *addr)
{
	char *p = buf;
	int i = 0;
	int zstart = 0;
	int bestzstart = 0;
	int bestzend = 0;
	int bestzlen = 0;
	int inz = 0;
	int donez = 0;
	const int addr16len = (int)(sizeof(struct in6_addr) / sizeof(uint16_t));

	/* parse address, looking for longest substring of 0s */
	for (i = 0; i < addr16len; i++) {
		if (addr->s6_addr16[i] == 0) {
			if (!inz) {
				zstart = i;
			} else {
				int zlen;
				int zend;
				zend = i;
				zlen = zend - zstart + 1;
				if (zlen > bestzlen) {
					bestzlen = zlen;
					bestzstart = zstart;
					bestzend = i;
				}
			}
			inz = 1;
		} else {
			inz = 0;
		}
	}

	/* when only the last 32 bits contain an address, format as an ::ipv4 */
	if (bestzstart == 0 && bestzlen == 6) {
		p += sprintf(p, "::%d.%d.%d.%d",
				addr->s6_addr[12], addr->s6_addr[13],
				addr->s6_addr[14], addr->s6_addr[15]);
		return p - buf;
	}

	/* otherwise format as normal ipv6 */
	for (i = 0; i < addr16len; i++) {
		uint16_t s = ntohs(addr->s6_addr16[i]);
		if ((bestzlen == 0) || (i < bestzstart) || (i > bestzend) || s) {
			const char *colon;
			if ((i == (addr16len - 1)) || (i == (bestzstart - 1))) {
				colon = "";
			} else {
				colon = ":";
			}
			p += sprintf(p, "%x%s", s, colon);
		} else if (bestzlen && (i == bestzstart)) {
			inz = 1;
		} else if (bestzlen && (i == bestzend)) {
			p += sprintf(p, "::");
			inz = 0;
			donez = 1;
		} else if (inz) {
		} else {
			WARN_ONCE(1, "%s: implementation error", __FUNCTION__);
		}
	}

	return p - buf;
}
EXPORT_SYMBOL(iputil_v6_ntop);

int iputil_v6_ntop_port(char *buf, const struct in6_addr *addr, __be16 port)
{
	char *p = buf;
	p += sprintf(p, "[");
	p += iputil_v6_ntop(p, addr);
	p += sprintf(p, "]:%u", ntohs(port));
	return p - buf;
}
EXPORT_SYMBOL(iputil_v6_ntop_port);

int iputil_eth_is_v6_mld(void *iphdr, uint32_t data_len)
{
	struct ipv6hdr *ip6hdr_p = iphdr;
	uint8_t nexthdr;
	int nhdr_off;
	struct icmp6hdr *icmp6hdr;
	int is_ipv6_mld = 0;

	nhdr_off = iputil_v6_skip_exthdr(ip6hdr_p, sizeof(struct ipv6hdr),
		&nexthdr, data_len, NULL, NULL);

	if (unlikely(nexthdr == IPPROTO_ICMPV6)) {
		icmp6hdr = (struct icmp6hdr*)((__u8 *)ip6hdr_p + nhdr_off);

		if (icmp6hdr->icmp6_type == ICMPV6_MGM_QUERY ||
			icmp6hdr->icmp6_type == ICMPV6_MGM_REPORT ||
			icmp6hdr->icmp6_type == ICMPV6_MGM_REDUCTION ||
			icmp6hdr->icmp6_type == ICMPV6_MLD2_REPORT) {
			is_ipv6_mld = 1;
		}
	}

	return is_ipv6_mld;
}
EXPORT_SYMBOL(iputil_eth_is_v6_mld);
#endif /* CONFIG_IPV6 */

/*
 * Return IP packet protocol information
 */
uint8_t __sram_text
iputil_proto_info(void *iph, void *data, void **proto_data, uint32_t *ip_id, uint8_t *more_frags)
{
	const struct iphdr *ipv4h = iph;
	u_int8_t nexthdr;
	int start;
	uint16_t frag_off;
	uint32_t data_len;
	struct sk_buff *skb = data;

	if (ipv4h->version == 4) {
		if (skb->len < (ipv4h->ihl << 2))
			return 0;
		frag_off = ntohs(get_unaligned((u16 *)&ipv4h->frag_off));
		if (ip_id && (frag_off & (IP_OFFSET | IP_MF)) != 0) {
			*ip_id = (uint32_t)ntohs(ipv4h->id);
		}
		if (more_frags) {
			*more_frags = !!(frag_off & IP_MF);
		}
		*proto_data = (char *)iph + (ipv4h->ihl << 2);

		if ((frag_off & IP_OFFSET) != 0) {
			/* not start of packet - does not contain protocol hdr */
			return NEXTHDR_FRAGMENT;
		}

		return ipv4h->protocol;
	}

#ifdef CONFIG_IPV6
	if (ipv4h->version == 6) {
		data_len = skb->len - ((uint8_t*)iph - (skb->data));
		start = iputil_v6_skip_exthdr((struct ipv6hdr *)iph, sizeof(struct ipv6hdr),
				&nexthdr, data_len, ip_id, more_frags);
		*proto_data = (char *)iph + start;

		return nexthdr;
	}
#endif

	return 0;
}
EXPORT_SYMBOL(iputil_proto_info);

int iputil_v4_ntop_port(char *buf, __be32 addr, __be16 port)
{
	return sprintf(buf, NIPQUAD_FMT ":%u", NIPQUAD(addr), ntohs(port));
}
EXPORT_SYMBOL(iputil_v4_ntop_port);

int iputil_v4_pton(const char *ip_str, __be32 *ipaddr)
{
	int i;
	uint32_t tmp_array[IPUTIL_V4_ADDR_LEN];
	uint8_t *ipaddr_p = (uint8_t *)ipaddr;

	if (ip_str == NULL)
		return -1;

	if (sscanf(ip_str, "%d.%d.%d.%d",
			&tmp_array[0],
			&tmp_array[1],
			&tmp_array[2],
			&tmp_array[3]) != 4) {
		return -1;
	}

	for (i = 0; i < IPUTIL_V4_ADDR_LEN; i++) {
		if (tmp_array[i] > 0xff) {
			return -1;
		}
	}

	ipaddr_p[0] = tmp_array[0];
	ipaddr_p[1] = tmp_array[1];
	ipaddr_p[2] = tmp_array[2];
	ipaddr_p[3] = tmp_array[3];

	return 0;
}
EXPORT_SYMBOL(iputil_v4_pton);

#ifdef CONFIG_IPV6
int iputil_ipv6_is_neigh_msg(struct ipv6hdr *ipv6, struct icmp6hdr *icmpv6)
{
	if (ipv6->hop_limit != 255) {
		return 0;
	}

	if (icmpv6->icmp6_code != 0) {
		return 0;
	}

	return 1;
}
EXPORT_SYMBOL(iputil_ipv6_is_neigh_msg);

int iputil_ipv6_is_neigh_sol_msg(uint8_t dup_addr_detect, struct nd_msg *msg, struct ipv6hdr *ipv6)
{
	const struct in6_addr *daddr = &ipv6->daddr;

	if (ipv6_addr_is_multicast(&msg->target)) {
		return 0;
	}

	if (dup_addr_detect && !(daddr->s6_addr32[0] == htonl(0xff020000) &&
			daddr->s6_addr32[1] == htonl(0x00000000) &&
			daddr->s6_addr32[2] == htonl(0x00000001) &&
			daddr->s6_addr [12] == 0xff )) {
		return 0;
	}

	return 1;
}
EXPORT_SYMBOL(iputil_ipv6_is_neigh_sol_msg);
#endif
