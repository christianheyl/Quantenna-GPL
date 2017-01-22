/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2012 Quantenna Communications, Inc           **
**                                                                           **
**  File        : ipv6mgmt.c						     **
**  Description :                                                            **
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
EH0*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fnmatch.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>
#include <linux/rtnetlink.h>
#include <linux/netlink.h>
#include <linux/if_link.h>
#include <signal.h>
#include <errno.h>

#define IF_RA_OTHERCONF	0x80
#define IF_RA_MANAGED	0x40
#define IF_RA_RCVD	0x20
#define IF_RS_SENT	0x10
#define IF_READY	0x80000000

#define IPV6MGMT_TIMER_INTERVAL 5
#define IPV6MGMT_CLEAN_DHCPV6_CLIENT "dhclient -x -pf /var/lib/dhcp/dhclient6.pid "
#define IPV6MGMT_STATELESS_DHCP_START "dhclient -6 -S -pf /var/lib/dhcp/dhclient6.pid "
#define IPV6MGMT_STATEFUL_DHCP_START "dhclient -6 -nw -cf /etc/dhcp/dhclient-dhcpv6.conf -pf /var/lib/dhcp/dhclient6.pid -lf /var/lib/dhcp/dhclient6.leases "

int rth_fd;
u_int32_t nlmsg_seq = 0;
u_int32_t g_link_flag = 0;
u_int32_t if_probe = 0;
char ipv6mgmt_config_ifname[16];
static int ra_recv_timeout = 0;
static int ra_recvd = 0;

struct ipv6mgmt_idxmap
{
	struct ipv6mgmt_idxmap * next;
	u_int32_t	index;
	char		name[16];
};

static struct ipv6mgmt_idxmap *ipv6mgmt_idxmap[16];

int ipv6mgmt_store_index2name(char *name, u_int32_t if_index)
{
	int h;
	h = if_index&0xF;
	struct ipv6mgmt_idxmap *im, **imp;

	for (imp=&ipv6mgmt_idxmap[h]; (im=*imp)!=NULL; imp = &im->next)
		if (im->index == if_index)
			break;

	if (im == NULL) {
		im = malloc(sizeof(*im));
		if (im == NULL)
			return 0;
		im->next = *imp;
		im->index = if_index;
		*imp = im;
	}
	strcpy(im->name, name);
	return 0;
}
const char *ipv6mgmt_ll_idx_n2a(u_int32_t idx, char *buf)
{
	struct ipv6mgmt_idxmap *im;

	if (idx == 0)
		return "*";
	for (im = ipv6mgmt_idxmap[idx&0xF]; im; im = im->next) {
		if (im->index == idx)
			return im->name;
	}
	return NULL;
}
const char *ipv6mgmt_ll_index_to_name(u_int32_t idx)
{
	static char nbuf[16];

	return ipv6mgmt_ll_idx_n2a(idx, nbuf);
}
static int ipv6mgmt_nl_link_request()
{
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg g;
	} req;
	struct sockaddr_nl nladdr;
	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

	req.nlh.nlmsg_len = sizeof(req);
	req.nlh.nlmsg_type = RTM_GETLINK;
	req.nlh.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
	req.nlh.nlmsg_pid = 0;
	req.nlh.nlmsg_seq = nlmsg_seq;
	req.g.rtgen_family = AF_INET6;
	nlmsg_seq ++;

	return sendto(rth_fd, (void*)&req, sizeof(req), 0,
		      (struct sockaddr*)&nladdr, sizeof(nladdr));
}

static void ipv6mgmt_nl_signal_hdl()
{
	/* RA received, stop link request polling */
	if (ra_recvd)
		return;

	alarm(IPV6MGMT_TIMER_INTERVAL);
	if_probe ++;
	if (ipv6mgmt_nl_link_request() < 0)
		perror("ipv6-mgmt request netlink\n");
}

static int ipv6mgmt_nl_signal_init()
{
	if (signal(SIGALRM, &ipv6mgmt_nl_signal_hdl) == SIG_ERR) {
				printf("Couldn't register signal handler for SIGALRM \n");
		return -1;
	}
	alarm(IPV6MGMT_TIMER_INTERVAL);
	return 0;
}

static int ipv6mgmt_nl_init()
{
	struct sockaddr_nl local;

	rth_fd= socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (rth_fd < 0) {
		perror("ip6-mgmt netlink socket alloc failed)");
		return -1;
	}
	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_groups = RTMGRP_LINK;

	if (bind(rth_fd, (struct sockaddr *) &local, sizeof(local)) < 0) {
		perror("ipv6-mgmt bind(netlink)");
		close(rth_fd);
		return -1;
	}
	return 0;
}

static void ipv6mgmt_nl_addr_config_action(u_int32_t link_flag)
{
	char buf[200];
	if ((link_flag & (IF_RA_MANAGED | IF_RA_OTHERCONF)) == IF_RA_OTHERCONF) {
		if_probe = 0;
		sprintf(buf, IPV6MGMT_CLEAN_DHCPV6_CLIENT "%s", ipv6mgmt_config_ifname);
		system(buf);

		sprintf(buf, IPV6MGMT_STATELESS_DHCP_START "%s", ipv6mgmt_config_ifname);
		system(buf);
	} else if ((link_flag & (IF_RA_MANAGED | IF_RA_OTHERCONF)) == (IF_RA_MANAGED | IF_RA_OTHERCONF)) {
		if_probe = 0;
		sprintf(buf, IPV6MGMT_CLEAN_DHCPV6_CLIENT "%s", ipv6mgmt_config_ifname);
		system(buf);

		sprintf(buf, IPV6MGMT_STATEFUL_DHCP_START "%s", ipv6mgmt_config_ifname);
		system(buf);
	}else {
		sprintf(buf, IPV6MGMT_CLEAN_DHCPV6_CLIENT "%s", ipv6mgmt_config_ifname);
		system(buf);
	}
}

static void ip6mgt_nl_paser(struct nlmsghdr *h, size_t len)
{
	int attrlen, nlmsg_len, rta_len, ifla_pro_attrlen;
	struct rtattr * attr;
	struct ifinfomsg *ifi;
	struct rtattr * ifla_pro_attr;
	u_int32_t link_flag = 0;

	if (len < sizeof(*ifi))
		return;

	ifi = NLMSG_DATA(h);

	if ((if_probe > 15) && (!(g_link_flag & IF_RA_RCVD)) && !ra_recv_timeout) {
		ra_recv_timeout = 1;
		perror("No IPv6 Router present, using DHCPv6 as default\n");
		ipv6mgmt_nl_addr_config_action(IF_RA_MANAGED | IF_RA_OTHERCONF);
		return;
	}

	nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	attrlen = h->nlmsg_len - nlmsg_len;
	if (attrlen < 0)
		return;

	attr = (struct rtattr *) (((char *) ifi) + nlmsg_len);

	rta_len = RTA_ALIGN(sizeof(struct rtattr));

	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_IFNAME) {
			ipv6mgmt_store_index2name((char *)RTA_DATA(attr), ifi->ifi_index);
		}

		/* Monitor interface down/up, check RA flag and reconfigure IPV6 address */
		if ((ifi->ifi_family == AF_UNSPEC) &&
				(!strcmp(ipv6mgmt_ll_index_to_name(ifi->ifi_index), ipv6mgmt_config_ifname)) &&
				(ifi->ifi_flags & IFF_UP)) {
			if_probe = 0;
			g_link_flag = 0;
			ra_recv_timeout = 0;
			ra_recvd = 0;
			alarm(IPV6MGMT_TIMER_INTERVAL);
			break;
		}

		if (attr->rta_type == IFLA_PROTINFO) {
				ifla_pro_attr = (struct rtattr *)RTA_DATA(attr);
				ifla_pro_attrlen = RTA_PAYLOAD(attr);

				while (RTA_OK(ifla_pro_attr, ifla_pro_attrlen)) {
					if (ifla_pro_attr->rta_type == IFLA_INET6_FLAGS) {
						if (strcmp(ipv6mgmt_ll_index_to_name(ifi->ifi_index), ipv6mgmt_config_ifname)) {
							break;
						}
						link_flag = *((u_int32_t *)(RTA_DATA(ifla_pro_attr)));

						/* RA received */
						if ((!(g_link_flag & IF_RA_RCVD)) &&
								(link_flag & IF_RA_RCVD)) {
							ipv6mgmt_nl_addr_config_action(link_flag);
							g_link_flag = link_flag;
							ra_recvd = 1;
						}
						break;
					}
				}
				ifla_pro_attr = RTA_NEXT(ifla_pro_attr, ifla_pro_attrlen);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}

static void ipv6mgmt_nl_event_poll()
{
	char buf[8192];
	int left;
	struct sockaddr_nl from;
	socklen_t fromlen;
	struct nlmsghdr *h;

	fromlen = sizeof(from);
	left = recvfrom(rth_fd, buf, sizeof(buf), MSG_WAITALL,
			(struct sockaddr *) &from, &fromlen);
	if (left < 0) {
		if (errno != EINTR && errno != EAGAIN)
			perror("recvfrom(netlink)");
		return;
	}

	h = (struct nlmsghdr *) buf;

	while (left >= (int) sizeof(*h)) {
		int len, plen;

		len = h->nlmsg_len;
		plen = len - sizeof(*h);
		if (len > left || plen < 0) {
			perror("Malformed netlink message");
			break;
		}
		if (h->nlmsg_type == RTM_NEWLINK) {
			ip6mgt_nl_paser(h, plen);
		}

		len = NLMSG_ALIGN(len);
		left -= len;
		h = (struct nlmsghdr *) ((char *) h + len);
	}

	if (left > 0) {
		perror("extra bytes in the end of netlink "
			   "message");
	}
}

int main(int argc, char **argv)
{
	strcpy(ipv6mgmt_config_ifname, argv[1]);
	ipv6mgmt_nl_init();
	ipv6mgmt_nl_signal_init();
	while (1) {
		ipv6mgmt_nl_event_poll();
	}
	return 0;
}
