/*
 * Copyright (c) 2013 Quantenna Communications, Inc.
 * All rights reserved.
 */

#ifndef __QTN_PCAP_H__
#define __QTN_PCAP_H__

#define QTN_GENPCAP	0

#ifdef MUC_BUILD
	#define qtn_pcap_memcpy uc_memcpy
	extern struct qtn_genpcap *g_qtn_genpcap_state;
#else
	#define qtn_pcap_memcpy memcpy
#endif	/* MUC_BUILD */

struct qtn_genpcap {
	uint8_t active;
	uint8_t payloads_count_s;
	uint8_t payload_size_s;
	uint8_t ___pad;
	uint8_t *payloads_vaddr;
	uint8_t *payloads_paddr;
	unsigned long payloads_written;
};

struct qtn_genpcap_args {
	void *vaddr;
	dma_addr_t paddr;
};


struct qtn_pcap_hdr {
	uint64_t tsf;
	uint16_t incl;
	uint16_t orig;
};

static __inline__ unsigned long qtn_pcap_max_payload(const struct qtn_genpcap *state)
{
	return (1 << state->payload_size_s) - sizeof(struct qtn_pcap_hdr);
}

static __inline__ struct qtn_pcap_hdr *
qtn_pcap_add_packet_start(struct qtn_genpcap *state, uint64_t tsf)
{
	unsigned int pkt_index;
	struct qtn_pcap_hdr *hdr;

	pkt_index = state->payloads_written % (1 << state->payloads_count_s);
	state->payloads_written++;

	hdr = (void *) (state->payloads_paddr + ((1 << state->payload_size_s) * pkt_index));
	hdr->tsf = tsf;

	return hdr;
}

static __inline__ void qtn_pcap_add_packet(struct qtn_genpcap *state,
		const void *payload, uint16_t len, uint64_t tsf)
{
	struct qtn_pcap_hdr *hdr;

	hdr = qtn_pcap_add_packet_start(state, tsf);
	hdr->orig = len;
	if (len >= qtn_pcap_max_payload(state))
		hdr->incl = qtn_pcap_max_payload(state);
	else
		hdr->incl = len;

	qtn_pcap_memcpy((hdr + 1), payload, hdr->incl);
}

#endif	/* __QTN_PCAP_H__ */

