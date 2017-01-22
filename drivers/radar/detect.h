/*
 * Copyright (c) 2009-2012 Quantenna Communications, Inc.
 * All rights reserved.
 */
#ifndef __DETECT_H__
#define __DETECT_H__

#include <linux/spinlock.h>
#include "radar.h"

/* structure to hold raw samples returned from hardware */
typedef struct {
	unsigned	start;
	unsigned	stop;
#if (RADAR_MAX_POW_TRACK)
	unsigned	max_pow;
#endif
	bool		 tagged;
} raw_pulse_t;

/* configuration */
typedef struct {
	u8		region;
	bool		sta_dfs;
	unsigned	maxPowTh;
	bool		maxPowEnable;

	unsigned	hwPwUBnd;
	unsigned	hwPwLBnd;

	unsigned	swPwUBnd;
	unsigned	swPwLBnd;

	unsigned	ssrPriUBnd;
	unsigned	ssrPriLBnd;

	unsigned	ssrPwUBnd;
	unsigned	ssrPwLBnd;

	unsigned	itlvPriUBnd;
	unsigned	itlvPriLBnd;

	unsigned	itlvPriDiffUBnd;
	unsigned	itlvPriDiffLBnd;

	unsigned	itlvPwUBnd;
	unsigned	itlvPwLBnd;

	unsigned	lsrPriUBnd;
	unsigned	lsrPriLBnd;

	unsigned	lsrPwUBnd;
	unsigned	lsrPwLBnd;

	unsigned	lsr_fsm_invalid_maxcnt;

	unsigned	maxPulseReadCnt;

	unsigned	ssrMaxPulseCnt;
	unsigned	maxFrbdnPri;
	unsigned	minFrbdnPri;

	unsigned	priNeighborRadius;
	unsigned	pwNeighborRadius;
	unsigned	lsrPwNeighborRadius;

	unsigned	priClusterRadius;
	unsigned	pwClusterRadius;

	unsigned	minFHpri;
	unsigned	maxFHpri;
	int		minFHpritol;
	int		maxFHpritol;
	unsigned	minFHw;
	unsigned	maxFHw;
	unsigned	localFHpercent;		/* detection percent */

	unsigned	mergeSeparationMax;

	unsigned	maxPulseCnt;
	unsigned	maxPulseCntLSR;
	unsigned	maxPulseCntFH;
	unsigned	minDetectTh;
	unsigned	lsrTimerShrt;

	unsigned	lsr_min_pw_diff;
	unsigned	lsr_min_pcnt_diff;
	unsigned	lsr_min_chirp_detect;
	unsigned	lsr_max_chirp_reject;
	unsigned	lsr_min_diff_det_rej;
	unsigned	chirp_town_freq_th;

	unsigned	ocac_lsr_min_chirp_detect;
	unsigned	ocac_lsr_burstcnt_lbnd;
	unsigned	ocac_lsrTimerShrt;

	bool		fhDetectEnable;
	bool		itlvDetectEnable;
	bool		lsrDetectEnable;
	bool		detected_lsr_flag;
	bool		lsrEnhancedAlg;
	bool		localFHdetect;
	bool		tag;

	bool		rateFilter;
	long		maxRate;
	int		maxInstRate;
	long		maxAllowedInstRate;	/* max instantaneous date rate for radar processing */

	unsigned	win_size;
	unsigned	win_per_pulse;

	unsigned	zcwin_per_pulse;
	unsigned	pw_2_zc_norm;
	unsigned	pulsetown_det_th;
	unsigned	pulsetown_det_th_eu_ocac;

	unsigned	max_radar_pulse_cnt;

	unsigned	max_lost_pulse_cnt;
	unsigned	lsr_timer_long;
	unsigned	lsr_burstcnt_lbnd;
	unsigned	lsr_burstcnt_ubnd;
	bool		lsr_off_center_det;
	bool		lsr_boost;
	bool		zc_diff_restrict;
	unsigned	zc_min_diff;
	char		region_str[7];

	unsigned	Korea2PriLbnd;
	unsigned	Korea2PriUbnd;
} detect_cfg_t;

/* statistics */
typedef struct {
	unsigned	detected_ssr;		/* SSR detections */
	unsigned	detected_itlv;		/* ITLV radar detections */
	unsigned	detected_lsr;		/* LSR detections */
	unsigned	detected_rjt;		/* detections rejected based on the pulse history */

	unsigned	numDetectReq;		/* radar detection requests */
	unsigned	numSsrCandidate;	/* SSR candidates */
	unsigned	numItlvCandidate;	/* interleaving radar candidates */
	unsigned	numItlvVerifyStop;	/* stopped itlv radar verification */

	unsigned	numRawPulse;		/* raw pulses recorded in the radar memory */
	unsigned	numMaxPollingPulse;	/* max raw pulses recorded at polling */
	unsigned	numProcRawPulse;	/* processed raw pulses recorded in the radar memory */
	unsigned	numProcMaxPollingPulse;	/* max processed raw pulses recorded at polling */
	unsigned	numReadPulse;		/* pulses read from the radar memory for processing */
	unsigned	numLeftPulse;		/* pulses after preprocessing of read ones */
	unsigned	numTagPulse;		/* tagged pulses among read ones */
	unsigned	numMergePulse;		/* merged pulses */
	unsigned	numPwFilterPulse;	/* pulses discarded because of out-of-range pulse width */
	unsigned	numPowFilterPulse;	/* pulses discarded because of low maximum power */

	unsigned	numLsrLikeBurst;	/* bursts that look part of a long sequence radar */

	unsigned	lastRadarMemIdx;

	unsigned	burst_cnt;
	unsigned	pulse_cnt;
	unsigned	invalid_cnt;

	unsigned	max_pw;
	unsigned	min_pw;
	unsigned	min_pulsecnt;
	unsigned	max_pulsecnt;

	unsigned	chirp_det_cnt;
	unsigned	chirp_rej_cnt;
	unsigned	chirp_nsi_cnt;

	int zc_index_hist[MAX_RADAR_PULSE_READ_CNT];


#define RADAR_PS_SIZE	10
	int		radar_ps_buf [RADAR_PS_SIZE];
	int		last_radar_ps_cnt;
	int		max_ps_cnt;

	/* info from the last detection */
	struct {
		unsigned	numPulse;
		unsigned	pri[3];
		unsigned	pw;		/* pulse width */
	} last;
} detect_sta_t;

struct detect_drv_sample_t {
	spinlock_t	lock;
	int		tx_pkts;	/* packets transmitted by driver in last sample period */
	int		tx_bytes;	/* bytes transmitted by driver in last sample period */
};

typedef struct {
	unsigned	start;
	unsigned	pw;
	unsigned	pri;
} pulse_info_t;

#define RADAR_REGION_US			(1)
#define RADAR_REGION_EU			(2)
#define RADAR_REGION_JP			(3)
#define PULSETOWN_POPULATION		(100)
#define PULSETOWN_SIZE_MAX		(11) /* 9 */
#define PULSETOWN_SIZE_MIN		(6)
#define OCAC_PULSETOWN_SIZE_MIN		(4)
#define OCAC_EU_WEATHER_PULSETOWN_SIZE_MIN		(2)
#define PULSETOWN_ITLV_SIZE_MAX		(18)
#define PULSETOWN_DETECTION_THRESHOLD2	(6)
#define OCAC_PULSETOWN_DETECTION_THRESHOLD2	(4)
#define OCAC_EU_WEATHER_PULSETOWN_DETECTION_THRESHOLD2	(2)
#define RADAR_PULSE_MOD			(0x08000000)
#define RADAR_PULSE_MASK		(RADAR_PULSE_MOD-1)
#define RADAR_PULSE_MOD_SHIFT_SIZE	(27)
#define DEBUG_PROC_PRINT_LENGTH		(PULSETOWN_ITLV_SIZE_MAX)
#define RADAR_CLUSTER_RADIUS		(4)
/* MAX_VALID_CLUSTERS >= floor(PULSETOEN_ITLV_SIZE_MAX / RADAR_CLUSTER_RADIUS) */
#define MAX_VALID_CLUSTERS		(4)
#define TOTAL_ITLV_RADAR_HITS		(11)
#define LSR_BURST_MINPULSECNT		(1)
#define LSR_BURST_MAXPULSECNT		(3)
#define RADAR_PRI_UPPER_BOUND_1		(13868)
#define RADAR_PRI_UPPER_BOUND_2		(14265)
#define RADAR_PRI_UPPER_BOUND_3		(39980)
#define RADAR_PRI_UPPER_BOUND_4		(38440)
#define RADAR_PRI_LOWER_BOUND_1		(5020)
#define RADAR_PRI_LOWER_BOUND_2		(13908)
#define RADAR_PRI_LOWER_BOUND_3		(14305)
#define RADAR_PRI_LOWER_BOUND_4		(38490)
#define W53_W56_SEPARATION_CHANNEL	(100)
#define MAX_CHANNEL_NO_FOR_JAPAN	(136)
#define MIN_CHANNEL_NO_FOR_JAPAN	(36)

typedef struct {
	pulse_info_t	pulse[PULSETOWN_POPULATION];
	unsigned	size;
} pulse_town_t;

/* control block */
typedef struct {
	detect_cfg_t	cfg;
	detect_sta_t	sta;

	pulse_town_t	town;
} detect_cb_t;

/* zero-crossing call-back block */
typedef struct {
	int		zc_mat[RADAR_ZERO_CROSS_PROC_PULSE_DEPTH][ZC_PER_PULSE];
	bool		is_zc_accurate;
} zc_cb_t;

extern bool detect_dbg_verbose;
extern bool detect_dbg_rdisp;

extern detect_cb_t *detect_cb;

void detect_init(detect_cb_t *cb);
void detect_reset(void);
bool detect_radar(raw_pulse_t *s, int count, int count_last_window, zc_cb_t *zc_cb);
bool detect_radar_poll(void);
bool isit_itlv(unsigned *pri_vec);
bool radar_post_detection_process(raw_pulse_t *s, int count);
struct detect_drv_sample_t *detect_drv_sample_loc_get(void);

static inline unsigned diff(unsigned a, unsigned b)
{
	return (a < b) ? (b-a) : (a-b);
}

#endif /* __DETECT_H__ */
