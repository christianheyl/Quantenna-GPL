/*
 * Copyright (c) 2011 Quantenna Communications, Inc.
 * All rights reserved.
 */


#ifndef _IF_QTN_TRACE_H_
#define _IF_QTN_TRACE_H_

enum qtn_trace_types {
	QTN_TRACE_EV_NONE		= 0x00000000,
	QTN_TRACE_EV_TX_PKT		= 0x01000001,
	QTN_TRACE_EV_TX_PKT_SZ		= 0x01000002,
	QTN_TRACE_EV_TX_PKT_BA		= 0x01000003,
	QTN_TRACE_EV_RX_PKT		= 0x02000001,
	QTN_TRACE_EV_RX_BAD_PKT		= 0x02000002,
	QTN_TRACE_EV_RX_NOT_VALID	= 0x02000003,
	QTN_TRACE_EV_RX_MAX_DUR		= 0x02000004,
	QTN_TRACE_EV_RX_BAD_LEN		= 0x02000005,
	QTN_TRACE_EV_RX_BAD_MCS_1	= 0x02000006,
	QTN_TRACE_EV_RX_BAD_MCS_2	= 0x02000008,
	QTN_TRACE_EV_RX_BAD_MCS_3	= 0x02000009,
	QTN_TRACE_EV_RX_BAD_MCS_4	= 0x0200000a,
	QTN_TRACE_EV_RX_BAD_MCS_5	= 0x0200000b,
	QTN_TRACE_EV_RX_PS_POLL		= 0x02000007,
	QTN_TRACE_EV_RX_INTR_SECOVRFL	= 0x03000001,
	QTN_TRACE_EV_RX_INTR_STRQOFLOW	= 0x03000002,
	QTN_TRACE_EV_RST_BCN		= 0x04000001,
	QTN_TRACE_EV_RST_TX		= 0x04000002,
	QTN_TRACE_EV_RST_RX		= 0x04000003,
	QTN_TRACE_EV_RST_PM		= 0x04000004,
	QTN_TRACE_EV_RST_SCHED1		= 0x04000005,
	QTN_TRACE_EV_RST_SCHED2		= 0x04000006,
	QTN_TRACE_EV_RST_START		= 0x04000007,
	QTN_TRACE_EV_RST_END		= 0x04000008,
	QTN_TRACE_EV_BB_INT		= 0x05000001,
	QTN_TRACE_EV_RX_DONE_INT	= 0x06000001,
	QTN_TRACE_EV_RX_TYPEDONE_INT	= 0x06000002,
	QTN_TRACE_EV_TX_DONE_INT	= 0x07000001,
	QTN_TRACE_EV_TX_DONE_DEPTH	= 0x07000002,
	QTN_TRACE_EV_TX_DONE_INHW	= 0x07000003,
	QTN_TRACE_EV_TX_DONE_CNT	= 0x07000004,
	QTN_TRACE_EV_TX_XATTEMPTS	= 0x07000005,
	QTN_TRACE_EV_TX_PROBE_RESP	= 0x07000006,
	QTN_TRACE_EV_WDOG_TX_START	= 0x08000001,
	QTN_TRACE_EV_WDOG_TX_DONE	= 0x08000002,
	QTN_TRACE_EV_MCST_DEFER		= 0x09000001,
	QTN_TRACE_EV_HW_WDOG_WARN	= 0x0A000001,
	QTN_TRACE_EV_PROBE_STATE	= 0x0B000001,
	QTN_TRACE_EV_PROBE_PPPC_START	= 0x0B000002,
	QTN_TRACE_EV_PROBE_PPPC_END	= 0x0B000003,
	QTN_TRACE_EV_PROBE_SGI_START	= 0x0B000004,
	QTN_TRACE_EV_PROBE_SGI_END	= 0x0B000005,
	QTN_TRACE_EV_PPPC_PWR_INDEX	= 0x0B000003,
	QTN_TRACE_EV_RA_START		= 0x0C000001,
	QTN_TRACE_EV_RA_END		= 0x0C000002,
	QTN_TRACE_EV_RA_MCS_SAMPLE	= 0x0C000003,
	QTN_TRACE_EV_RF_TXPWR_CAL_START	= 0x0D000001,
	QTN_TRACE_EV_RF_TXPWR_CAL_END	= 0x0D000002,
	QTN_TRACE_EV_RF_TXPD_CAL_START	= 0x0D000003,
	QTN_TRACE_EV_RF_TXPD_CAL_END	= 0x0D000004,
	QTN_TRACE_EV_RF_VCO_CAL_START	= 0x0D000005,
	QTN_TRACE_EV_RF_VCO_CAL_END	= 0x0D000006,
	QTN_TRACE_EV_RF_GAIN_AD_START	= 0x0D000007,
	QTN_TRACE_EV_RF_GAIN_AD_END	= 0x0D000008,
	QTN_TRACE_EV_PS_STATE		= 0x0E000001,
	QTN_TRACE_EV_RST_TCM		= 0x0E000002,
};

enum qtn_trace_trigger {
	QTN_TRACE_TRIGGER_DROP_QDRV_SCH = 0x00000001,
};

#if QTN_ENABLE_TRACE_BUFFER
/* Debugs for tracing activity */

#define QTN_TRACE_BUF_SIZE	75

extern uint32_t qtn_trace_index;
struct qtn_trace_record {
	uint32_t	tsf;
	uint32_t	event;
	uint32_t	data;
};
extern struct qtn_trace_record qtn_trace_buffer[QTN_TRACE_BUF_SIZE];

#define QTN_TRACE(sc, event, data)	qtn_trace((sc), (event), (uint32_t)(data))

#define QTN_TRACE_SET(field, value)	do { (field) = (value); } while(0)

# ifdef MUC_BUILD
#  include "qtn/if_qtnvar.h"
# endif

static __inline__
# ifdef MUC_BUILD
void qtn_trace(struct qtn_softc *sc, uint32_t event, uint32_t data)
{
	qtn_trace_index++;
	if (qtn_trace_index >= QTN_TRACE_BUF_SIZE) {
		qtn_trace_index = 0;
	}
	qtn_trace_buffer[qtn_trace_index].tsf = hal_get_tsf_lo(sc->sc_qh);
	qtn_trace_buffer[qtn_trace_index].event = event;
	qtn_trace_buffer[qtn_trace_index].data = data;
}
# else
void qtn_trace(struct qdrv_mac *mac, uint32_t event, uint32_t data)
{
	qtn_trace_index++;
	if (qtn_trace_index >= QTN_TRACE_BUF_SIZE) {
		qtn_trace_index = 0;
	}
	qtn_trace_buffer[qtn_trace_index].tsf = jiffies; /* FIXME: hal_get_tsf_lo(sc->sc_qh); */
	qtn_trace_buffer[qtn_trace_index].event = event;
	qtn_trace_buffer[qtn_trace_index].data = data;
}
# endif //MUC_BUILD

#else //QTN_ENABLE_TRACE_BUFFER

#define QTN_TRACE(sc, type, data)	do {} while(0)

#define QTN_TRACE_SET(field, value)	do {} while(0)

#endif //QTN_ENABLE_TRACE_BUFFER


#endif /* _IF_QTN_TRACE_H_ */

