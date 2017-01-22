/**
 * (C) Copyright 2013 Quantenna Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 **/

#include <linux/types.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/igmp.h>
#include <linux/hardirq.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ctype.h>
#include <linux/if_ether.h>
#include <linux/net/bridge/br_public.h>

#include <net80211/if_ethersubr.h>
#include <net80211/ieee80211.h>

#include <qtn/qtn_debug.h>
#include <qtn/qtn_uc_comm.h>
#include <qtn/qtn_net_packet.h>
#include <qtn/iputil.h>
#include <qtn/topaz_tqe_cpuif.h>
#include <qtn/topaz_fwt_cpuif.h>
#include <qtn/topaz_fwt_if.h>
#include <qtn/topaz_fwt_db.h>
#include <qtn/topaz_fwt.h>
#include <qtn/mproc_sync_base.h>

MODULE_DESCRIPTION("Forwarding Table Interface");
MODULE_AUTHOR("Quantenna Communications, Inc.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

#define PROC_NAME "topaz_fwt_if"

/* Success definition in FWT Interface is return positive value */
#define FWT_IF_SUCCESS(x)	((x) >= 0)
/* Error definition in FWT Interface is return negative value */
#define FWT_IF_ERROR(x)		(!(FWT_IF_SUCCESS(x)))

/* specific commands keywords - must match fwt_if_usr_cmd */
#define FWT_IF_KEY_CLEAR		"clear"
#define FWT_IF_KEY_ON			"on"
#define FWT_IF_KEY_OFF			"off"
#define FWT_IF_KEY_PRINT		"print"
#define FWT_IF_KEY_ADD_STATIC_MC	"add_static_mc"
#define FWT_IF_KEY_DEL_STATIC_MC	"del_static_mc"
#define FWT_IF_KEY_GET_MC_LIST		"get_mc_list"
#define FWT_IF_KEY_ADD			"add"
#define FWT_IF_KEY_DELETE		"del"
#define FWT_IF_KEY_AUTO			"auto"
#define FWT_IF_KEY_MANUAL		"manual"
#define FWT_IF_KEY_NODE			"node"
#define FWT_IF_KEY_4ADDR		"4addr"
#define FWT_IF_KEY_HELP			"help"
#define FWT_IF_KEY_DEBUG		"debug"
#define FWT_IF_KEY_AGEING		"ageing"

/* additional keywords */
#define FWT_IF_KEY_PORT			"port"
#define FWT_IF_KEY_ENABLE		"enable"
#define FWT_IF_KEY_MAC			"mac"

#define PRINT_FWT(a...)		do { if (g_debug) { printk(a); } } while(0)

const char *g_port_names[] = TOPAZ_TQE_PORT_NAMES;
static fwt_if_sw_cmd_hook g_fwt_if_cmd_hook = NULL;
static char *fwt_if_outbuf = NULL;
static spinlock_t fwt_if_outbuf_lock;	/* synchronise use of fwt_if_outbuf */

/* set command keywords */
static char *fwt_str_cmd[FWT_IF_MAX_CMD] = {
		FWT_IF_KEY_CLEAR,
		FWT_IF_KEY_ON,
		FWT_IF_KEY_OFF,
		FWT_IF_KEY_PRINT,
		FWT_IF_KEY_ADD_STATIC_MC,
		FWT_IF_KEY_DEL_STATIC_MC,
		FWT_IF_KEY_GET_MC_LIST,
		FWT_IF_KEY_ADD,
		FWT_IF_KEY_DELETE,
		FWT_IF_KEY_AUTO,
		FWT_IF_KEY_MANUAL,
		FWT_IF_KEY_4ADDR,
		FWT_IF_KEY_DEBUG,
		FWT_IF_KEY_HELP,
		FWT_IF_KEY_AGEING,
};

/* value of extracted keywords from user */
typedef	enum {
	FWT_IF_ID,
	FWT_IF_MAC,
	FWT_IF_PORT,
	FWT_IF_NODE,
	FWT_IF_ENABLE,
	FWT_IF_VALUE,
	FWT_IF_MAX_PARAM,
} fwt_if_usr_str;

static inline int fwt_if_split_words(char **words, char *str)
{
	int word_count = 0;

	/* skip leading space */
	while (str && *str && isspace(*str)) {
		str++;
	}

	while (str && *str) {
		words[word_count++] = str;

		/* skip this word */
		while (str && *str && !isspace(*str)) {
			str++;
		}

		/* replace spaces with NULL */
		while (str && *str && isspace(*str)) {
			*str = 0;
			str++;
		}
	}

	return word_count;
}

static int inline fwt_if_set_sw_cmd(fwt_if_usr_cmd cmd, struct fwt_if_common *data)
{
	if (g_fwt_if_cmd_hook) {
		return g_fwt_if_cmd_hook(cmd, data);
	}

	return -1;
}

static int fwt_if_extract_parm(char *str, fwt_if_usr_str cmd, void *var)
{
	struct fwt_if_id *id;
	uint16_t *ip6;
	uint8_t *ip4;
	uint8_t *mac;
	int i, node_num, param;
	uint8_t node[FWT_IF_USER_NODE_MAX];

	/* clear node array */
	memset(node, FWT_DB_INVALID_NODE, FWT_IF_USER_NODE_MAX);
	if (!str) {
		return -EINVAL;
	}

	switch (cmd) {
	case FWT_IF_MAC:
		id = var;
		mac = id->mac_be;
		if (sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
					&mac[0], &mac[1], &mac[2],
					&mac[3], &mac[4], &mac[5]) != ETH_ALEN) {
			return -EINVAL;
		}
		break;
	case FWT_IF_ID:
		/*
		 * This keyword takes a MAC address or an IPv4 or IPv6 address. If it is a multicast
		 * IP address it is also converted into a multicast layer 2 (MAC) address.
		 */
		id = var;
		ip6 = id->ip.u.ip6.in6_u.u6_addr16;
		ip4 = (void *) &id->ip.u.ip4;
		mac = id->mac_be;
		id->ip.proto = 0;

		if (sscanf(str, "%hx:%hx:%hx:%hx:%hx:%hx:%hx:%hx",
					&ip6[0], &ip6[1], &ip6[2], &ip6[3],
					&ip6[4], &ip6[5], &ip6[6], &ip6[7]) == 8) {
			id->ip.proto = htons(ETHERTYPE_IPV6);
			if (IPUTIL_V6_ADDR_MULTICAST(*ip6))
				fwt_mcast_to_mac(mac, &id->ip);
		} else if (sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
					&mac[0], &mac[1], &mac[2],
					&mac[3], &mac[4], &mac[5]) == ETH_ALEN) {
		} else if (sscanf(str, "%hhd.%hhd.%hhd.%hhd",
					&ip4[0], &ip4[1], &ip4[2],
					&ip4[3]) == FWT_DB_IPV4_SIZE) {
			id->ip.proto = htons(ETHERTYPE_IP);
			if (IPUTIL_V4_ADDR_MULTICAST(*ip4))
				fwt_mcast_to_mac(mac, &id->ip);
		} else {
			return -EINVAL;
		}
		break;
	case FWT_IF_PORT:
		/* Port array names order correspond to HW port numbers */
		for (i = 0; i < TOPAZ_TQE_NUM_PORTS; i++) {
			if (strcmp(str, g_port_names[i]) == 0) {
				*((uint8_t*)var) = i;
				return 1;
			}
		}
		return -ENOENT;
		break;
	case FWT_IF_NODE:
		node_num = sscanf(str, "%hhu:%hhu:%hhu:%hhu:%hhu:%hhu",
						&node[0], &node[1], &node[2],
						&node[3], &node[4], &node[5]);
		if (node_num <= 0) {
			return -EINVAL;
		}

		for (i = 0; i < node_num; i++) {
			if (node[i] >= QTN_NCIDX_MAX) {
				return -EINVAL;
			}
		}
		memcpy(var, node, node_num * sizeof(uint8_t));
		break;
	case FWT_IF_ENABLE:
		sscanf(str, "%u", &param);
		if ( (param == 0) || (param == 1)) {
			*((uint32_t*)var) = param;
		} else {
			return -EINVAL;
		}
		break;
	case FWT_IF_VALUE:
		sscanf(str, "%d", &param);
		*((int32_t*)var) = param;
		break;
	default:
		return -EINVAL;
	}

	return 1;
}

/*
 * Get the current value that attached to keyword definition
 * @param str: the string to find keyword
 * @param words: the target search
 * @param word_count: number of words to search from
 * @return: the value as a string
 */
static char *fwt_if_get_val_from_keyword(char *str, char **words, uint8_t word_count)
{
	while (words && --word_count) {
		if (strcmp(str, *words) == 0) {
			/* return the value attached to the keyword */
			return *(++words);
		}
		/* advance to next word */
		words++;
	}
	return NULL;
}

static void fwt_if_apply_user_print_help(void)
{
	printk("FWT commands\n");
	printk("clear        remove all entries from the SW and HW FWT tables\n");
	printk("on           enable the FWT interface\n");
	printk("off          disable the FWT interface\n");
	printk("print        print the contents of the SW FWT\n");
	printk("add_static_mc <ip> mac <mac>\n");
	printk("             add a static multicast entry\n");
	printk("del_static_mc <ip> mac <mac>\n");
	printk("             delete a static multicast entry\n");
	printk("get_mc_list  return a list of multicast addresses\n");
	printk("add <mac> port <port> node <node>\n");
	printk("             add a unicast entry to the FWT\n");
	printk("             e.g. add 01:ab:ac:34:67:20 port wmac node 3\n");
	printk("add <ip> port <port> node <node>\n");
	printk("             add a multicast entry to the FWT\n");
	printk("             e.g. add 224.1.2.3 port wmac node 3\n");
	printk("del <mac>    delete a table entry by MAC address\n");
	printk("del <ip>     delete a table entry by IP address\n");
	printk("auto         enable automatic generation of table entries by the bridge module\n");
	printk("manual       enable manual-only creation of table entries, with no restrictions\n");
	printk("4addr <mac> enable {0|1}\n");
	printk("             enable or disable 4-address support for a MAC address\n");
	printk("debug {0|1}  enable or disable debug messages\n");
	printk("ageing {0|1} enable or disable table entry ageing\n");
	printk("help         print this\n");
}
static int fwt_if_apply_user_debug_mode(char **words, uint8_t word_count)
{
	char *str_val;
	int rc;
	struct fwt_if_common data;

	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_DEBUG, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_ENABLE, &data.param);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}
	rc = fwt_if_set_sw_cmd(FWT_IF_CMD_DEBUG, &data);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}

	return 1;
}

static int fwt_if_apply_user_ageing(char **words, uint8_t word_count)
{
	char *str_val;
	int rc;
	struct fwt_if_common data;

	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_AGEING, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_ENABLE, &data.param);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}
	rc = fwt_if_set_sw_cmd(FWT_IF_CMD_AGEING, &data);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}

	return 1;
}

static int fwt_if_apply_user_4addr_mode(char **words, uint8_t word_count)
{
	char *str_val;
	int rc;
	struct fwt_if_common data;

	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_4ADDR, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_ID, &data.id);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}
	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_ENABLE, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_ENABLE, &data.param);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}
	rc = fwt_if_set_sw_cmd(FWT_IF_CMD_4ADDR, &data);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}

	return 1;
}

static int fwt_if_apply_user_del_entry(char **words, uint8_t word_count)
{
	int rc;
	struct fwt_if_common data;
	char *str_val;

	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_DELETE, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_ID, &data.id);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}
	rc = fwt_if_set_sw_cmd(FWT_IF_CMD_DELETE, &data);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}
	return 1;
}

static int fwt_if_apply_user_add_entry(char **words, uint8_t word_count)
{
	int rc;
	char *str_val = NULL;
	struct fwt_if_common data;

	memset(data.node, FWT_DB_INVALID_NODE, sizeof(data.node));

	/* ADD example: "add ab:34:be:af:34:42 port lhost node 2:4:3" */
	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_ADD, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_ID, &data.id);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}

	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_PORT, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_PORT, &data.port);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}

	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_NODE, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_NODE, &data.node);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}

	rc = fwt_if_set_sw_cmd(FWT_IF_CMD_ADD, &data);
	if (FWT_IF_ERROR(rc)) {
		return rc;
	}

	return 1;
}

static int fwt_if_apply_user_add_static_mc(char **words, uint8_t word_count)
{
	int rc;
	char *str_val = NULL;
	struct fwt_if_common data;
	struct fwt_if_id *id = &data.id;

	memset(data.node, FWT_DB_INVALID_NODE, sizeof(data.node));

	/* ADD_STATIC_MC example: "add_static_mc 224.51.2.3 mac ab:34:be:af:34:42*/
	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_ADD_STATIC_MC, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_ID, id);
	if (FWT_IF_ERROR(rc))
		return rc;

	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_MAC, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_MAC, id);
	if (FWT_IF_ERROR(rc))
		return rc;

	switch (id->ip.proto) {
	case htons(ETHERTYPE_IP):
		if (!IPUTIL_V4_ADDR_MULTICAST(id->ip.u.ip4))
			return -EINVAL;
		break;
	case htons(ETHERTYPE_IPV6):
		if (!IPUTIL_V6_ADDR_MULTICAST(*id->ip.u.ip6.in6_u.u6_addr16))
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	rc = fwt_if_set_sw_cmd(FWT_IF_CMD_ADD_STATIC_MC, &data);
	if (FWT_IF_ERROR(rc))
		return rc;

	return 1;
}

static int fwt_if_apply_user_del_static_mc(char **words, uint8_t word_count)
{
	int rc;
	char *str_val = NULL;
	struct fwt_if_common data;
	struct fwt_if_id *id = &data.id;

	memset(data.node, FWT_DB_INVALID_NODE, sizeof(data.node));

	/* DEL_STATIC_MC example: "del_static_mc 224.51.2.3 mac ab:34:be:af:34:42*/
	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_DEL_STATIC_MC, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_ID, id);
	if (FWT_IF_ERROR(rc))
		return rc;

	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_MAC, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_MAC, id);
	if (FWT_IF_ERROR(rc))
		return rc;

	switch (id->ip.proto) {
	case htons(ETHERTYPE_IP):
		if (!IPUTIL_V4_ADDR_MULTICAST(id->ip.u.ip4))
			return -EINVAL;
		break;
	case htons(ETHERTYPE_IPV6):
		if (!IPUTIL_V6_ADDR_MULTICAST(*id->ip.u.ip6.in6_u.u6_addr16))
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	rc = fwt_if_set_sw_cmd(FWT_IF_CMD_DEL_STATIC_MC, &data);
	if (FWT_IF_ERROR(rc))
		return rc;

	return 1;
}

#define FWT_IF_OUTBUF_LEN_MIN	32
#define FWT_IF_OUTBUF_LEN_MAX	8192

/* Must be called with the fwt_if_outbuf_lock held */
static int fwt_if_get_mc_list(char **words, uint8_t word_count)
{
	int rc;
	struct fwt_if_common data;
	char *str_val = NULL;

	if (fwt_if_outbuf) {
		/* a command was issued previously but the output was not retrieved */
		kfree(fwt_if_outbuf);
		fwt_if_outbuf = NULL;
	}

	str_val = fwt_if_get_val_from_keyword(FWT_IF_KEY_GET_MC_LIST, words, word_count);
	rc = fwt_if_extract_parm(str_val, FWT_IF_VALUE, &data.param);
	if (FWT_IF_ERROR(rc))
		return rc;

	if (data.param <= FWT_IF_OUTBUF_LEN_MIN || data.param > FWT_IF_OUTBUF_LEN_MAX)
		return -EINVAL;

	fwt_if_outbuf = kmalloc(data.param, GFP_KERNEL);
	if (!fwt_if_outbuf)
		return -ENOMEM;

	memset(fwt_if_outbuf, 0, data.param);
	data.extra = fwt_if_outbuf;

	fwt_if_set_sw_cmd(FWT_IF_CMD_GET_MC_LIST, &data);

	return 1;
}

/* Apply user command.
 * User command can control the FWT interface.
 * @param cmd_num: command number
 * @param words: the split words without spaces from the user space console interface
 * @param word_count: number of words after split
 * @return: status indication
 */
static int fwt_if_apply_user_command(fwt_if_usr_cmd cmd_num, char **words, uint8_t word_count)
{
	int rc = -EINVAL;
	struct fwt_if_common data;

	if ((word_count == 0) || (!words))
		goto cmd_failure;

	memset(&data, 0, sizeof(data));

	switch(cmd_num) {
		case FWT_IF_CMD_CLEAR:
		case FWT_IF_CMD_ON:
		case FWT_IF_CMD_OFF:
		case FWT_IF_CMD_PRINT:
		case FWT_IF_CMD_AUTO:
		case FWT_IF_CMD_MANUAL:
			rc = fwt_if_set_sw_cmd(cmd_num, &data);
			break;
		case FWT_IF_CMD_GET_MC_LIST:
			spin_lock(&fwt_if_outbuf_lock);
			rc = fwt_if_get_mc_list(words, word_count);
			spin_unlock(&fwt_if_outbuf_lock);
			break;
		case FWT_IF_CMD_ADD:
			rc = fwt_if_apply_user_add_entry(words, word_count);
			break;
		case FWT_IF_CMD_DELETE:
			/* Delete example: "del ab:34:be:af:34:42" */
			rc = fwt_if_apply_user_del_entry(words, word_count);
			break;
		case FWT_IF_CMD_ADD_STATIC_MC:
			rc = fwt_if_apply_user_add_static_mc(words, word_count);
			break;
		case FWT_IF_CMD_DEL_STATIC_MC:
			rc = fwt_if_apply_user_del_static_mc(words, word_count);
			break;
		case FWT_IF_CMD_4ADDR:
			rc = fwt_if_apply_user_4addr_mode(words, word_count);
			break;
		case FWT_IF_CMD_DEBUG:
			rc = fwt_if_apply_user_debug_mode(words, word_count);
			break;
		case FWT_IF_CMD_AGEING:
			rc = fwt_if_apply_user_ageing(words, word_count);
			break;
		case FWT_IF_CMD_HELP:
			rc = 0;
			fwt_if_apply_user_print_help();
			break;
		default:
			goto cmd_failure;
		break;
	}

	if (FWT_IF_ERROR(rc))
		goto cmd_failure;

	return 1;

cmd_failure:
	if (word_count)
		printk(KERN_INFO "Failed to parse command: %s\n", *words);
	else
		fwt_if_apply_user_print_help();

	return -EPERM;
}

static int fwt_if_read_proc(char *page, char **start, off_t off, int count, int *eof, void *_unused)
{
	int printed = 0;

	spin_lock(&fwt_if_outbuf_lock);

	if (fwt_if_outbuf) {
		printed = strnlen(fwt_if_outbuf, count - 1);
		strncpy(page, fwt_if_outbuf, printed);
		kfree(fwt_if_outbuf);
		fwt_if_outbuf = NULL;
	} else {
		page[0] = '\0';
	}

	spin_unlock(&fwt_if_outbuf_lock);

	page[count - 1] = '\0';
	*eof = 1;

	return printed;
}

static int fwt_if_write_proc(struct file *file, const char __user *buffer,
		unsigned long count, void *_unused)
{
	char *cmd;
	int rc, i;
	char **words;
	uint8_t word_count;
	fwt_if_usr_cmd cmd_num = 0;

	cmd = kmalloc(count, GFP_KERNEL);
	words = kmalloc(count * sizeof(char *) / 2, GFP_KERNEL);
	if (!cmd || !words) {
		rc = -ENOMEM;
		goto out;
	}

	if (copy_from_user(cmd, buffer, count)) {
		rc = -EFAULT;
		goto out;
	}

	cmd[count - 1] = '\0';

	word_count = fwt_if_split_words(words, cmd);
	for (i = 0; i < FWT_IF_MAX_CMD; i++, cmd_num++) {
		if (strcmp(words[0], fwt_str_cmd[i]) == 0)
			break;
	}

	local_bh_disable();

	rc = fwt_if_apply_user_command(cmd_num, words, word_count);

	local_bh_enable();

	out:
	if (cmd) {
		kfree(cmd);
	}
	if (words) {
		kfree(words);
	}
	return count;
}

void fwt_if_register_cbk_t(fwt_if_sw_cmd_hook cbk_func)
{
	g_fwt_if_cmd_hook = cbk_func;
}
EXPORT_SYMBOL(fwt_if_register_cbk_t);

static void __exit fwt_if_exit(void)
{
	remove_proc_entry(PROC_NAME, NULL);
}

static int __init topaz_fwt_if_create_proc(void)
{
	struct proc_dir_entry *entry = create_proc_entry(PROC_NAME, 0600, NULL);

	if (!entry)
		return -ENOMEM;

	entry->write_proc = fwt_if_write_proc;
	entry->read_proc = fwt_if_read_proc;

	return 0;
}

static int __init fwt_if_init(void)
{
	int rc;

	rc = topaz_fwt_if_create_proc();
	if (rc)
		return rc;

	spin_lock_init(&fwt_if_outbuf_lock);

	return 0;
}
module_init(fwt_if_init);
module_exit(fwt_if_exit);
