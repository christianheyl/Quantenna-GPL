/**
  Copyright (c) 2008 - 2013 Quantenna Communications Inc
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
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_wlan.h"
#include "qdrv_pcap.h"

#if QTN_GENPCAP

#define PROC_NAME	"pcap"
static struct qtn_genpcap *pcap_state = NULL;

static int qdrv_pcap_seq_finished(const unsigned long *counter)
{
	if (pcap_state == NULL) {
		return 1;
	}

	return *counter >= (1 << pcap_state->payloads_count_s);
}

static void* qdrv_pcap_seq_start(struct seq_file *sfile, loff_t *pos)
{
	unsigned long *counter;

	if (pcap_state == NULL || pcap_state->active) {
		printk(KERN_ERR "%s: only take pcap when inactive\n", __FUNCTION__);
		return NULL;
	}

	counter = kmalloc(sizeof(*counter), GFP_KERNEL);
	if (counter == NULL) {
		return NULL;
	}

	*counter = *pos;

	if (qdrv_pcap_seq_finished(counter)) {
		kfree(counter);
		return NULL;
	}

	return counter;
}

static void* qdrv_pcap_seq_next(struct seq_file *sfile, void *v, loff_t *pos)
{
	unsigned long *counter = v;
	(*counter)++;
	(*pos)++;

	if (qdrv_pcap_seq_finished(counter)) {
		return NULL;
	}

	return counter;
}

static void qdrv_pcap_seq_stop(struct seq_file *sfile, void *v)
{
	kfree(v);
}

static int qdrv_pcap_seq_show(struct seq_file *sfile, void *v)
{
	unsigned long *counter = v;
	unsigned long pkt_index;
	struct qtn_pcap_hdr *qtnhdr;
	struct pcaprec_hdr rechdr;

	if (*counter == 0) {
		struct pcap_hdr file_hdr = qtn_pcap_mkhdr(qtn_pcap_max_payload(pcap_state));
		seq_write(sfile, &file_hdr, sizeof(file_hdr));
	}

	pkt_index = (*counter + pcap_state->payloads_written) %
		(1 << pcap_state->payloads_count_s);
	qtnhdr = (void *) (pcap_state->payloads_vaddr + ((1 << pcap_state->payload_size_s) * pkt_index));
	if (qtnhdr->incl) {
		rechdr.incl_len = qtnhdr->incl;
		rechdr.orig_len = qtnhdr->orig;
		rechdr.ts_sec = ((uint32_t) qtnhdr->tsf) / 1000000;
		rechdr.ts_usec = ((uint32_t) qtnhdr->tsf) % 1000000;
		seq_write(sfile, &rechdr, sizeof(rechdr));
		seq_write(sfile, (qtnhdr + 1), qtnhdr->incl);
	}

	return 0;
}


static struct seq_operations qdrv_pcap_seq_ops = {
	.start = qdrv_pcap_seq_start,
	.next  = qdrv_pcap_seq_next,
	.stop  = qdrv_pcap_seq_stop,
	.show  = qdrv_pcap_seq_show
};

static int qdrv_pcap_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &qdrv_pcap_seq_ops);
}

static struct file_operations qdrv_pcap_proc_ops = {
	.owner   = THIS_MODULE,
	.open    = qdrv_pcap_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

void qdrv_genpcap_exit(struct qdrv_wlan *qw)
{
	struct qtn_genpcap_args *gpa = &qw->genpcap_args;
	size_t alloc_sz;

	if (pcap_state && gpa->vaddr) {
		remove_proc_entry(PROC_NAME, NULL);

		alloc_sz = sizeof(*pcap_state) +
			(1 << (pcap_state->payloads_count_s + pcap_state->payload_size_s));
		dma_free_coherent(NULL, alloc_sz, gpa->vaddr, gpa->paddr);
		gpa->vaddr = NULL;
		gpa->paddr = 0;
		pcap_state = NULL;
	}
}

int qdrv_genpcap_set(struct qdrv_wlan *qw, int cfg, dma_addr_t *ctrl_dma)
{
	const uint8_t cfg_op = (cfg >> 16) & 0xff;
	const uint8_t cfg_pktsz_s = (cfg >> 8) & 0xff;
	const uint8_t cfg_pktcnt_s = (cfg >> 0) & 0xff;

	*ctrl_dma = 0;

	if (pcap_state && ((cfg_op == QTN_GENPCAP_OP_START) || (cfg_op == QTN_GENPCAP_OP_FREE))) {
		qdrv_genpcap_exit(qw);
	}

	if ((pcap_state == NULL) && (cfg_op == QTN_GENPCAP_OP_START)) {
		/* currently uninitialized, start requested */
		struct qtn_genpcap_args *gpa = &qw->genpcap_args;
		struct qtn_genpcap *ctrl;
		uint8_t *payloads_start;
		size_t payloads_total_size;
		size_t alloc_sz;

		if (cfg_pktsz_s < 5 || cfg_pktcnt_s < 1) {
			printk(KERN_ERR "%s: invalid settings\n", __FUNCTION__);
			return -EINVAL;
		}

		payloads_total_size = 1 << (cfg_pktsz_s + cfg_pktcnt_s);
		alloc_sz = payloads_total_size + sizeof(*ctrl);
		if ((gpa->vaddr = dma_alloc_coherent(NULL,
						alloc_sz, &gpa->paddr, GFP_KERNEL)) == NULL) {
			printk(KERN_ERR "%s: could not allocate %u bytes\n",
					__FUNCTION__, alloc_sz);
			return -ENOMEM;
		}

		memset(gpa->vaddr, 0, alloc_sz);

		payloads_start = gpa->vaddr;
		ctrl = (void *) (payloads_start + payloads_total_size);
		*ctrl_dma = gpa->paddr + payloads_total_size;
		pcap_state = ctrl;

		ctrl->active = 0;
		ctrl->payloads_count_s = cfg_pktcnt_s;
		ctrl->payload_size_s = cfg_pktsz_s;
		ctrl->payloads_vaddr = gpa->vaddr;
		ctrl->payloads_paddr = (void *) gpa->paddr;
		ctrl->payloads_written = 0;

		if (proc_create_data(PROC_NAME, 0, NULL, &qdrv_pcap_proc_ops, qw) == NULL) {
			printk(KERN_ERR "%s: could not create procfile %s\n",
					__FUNCTION__, PROC_NAME);
			return -1;
		}

		printk(KERN_INFO "%s: activated\n", __FUNCTION__);
		pcap_state->active = 1;
	}

	if (pcap_state && (cfg_op == QTN_GENPCAP_OP_STOP)) {
		printk(KERN_INFO "%s deactivated, %lu buffers captured (%u max)\n",
				__FUNCTION__,
				pcap_state->payloads_written,
				1 << pcap_state->payloads_count_s);
		pcap_state->active = 0;
	}

	return 0;
}

#endif	/* QTN_GENPCAP */
