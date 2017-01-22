/*
 * Copyright (c) 2009-2012 Quantenna Communications, Inc.
 * All rights reserved.
 */
#ifndef __RADAR_H__
#define __RADAR_H__

#include <qtn/registers.h>
#include <qtn/shared_params.h>
#include <linux/spinlock.h>

#define MIN_PULSE_SEP				(5)	/* 5 usec is minimum pulse separation */
							/* Anything less is merged */

/* Radar registers */
#define QT4_GLBL_BASE_ADDR			(0xe6000000)
#define QT4_TD_0_BASE_ADDR			(0xe6090000)
#define QT4_TD_1_BASE_ADDR			(0xe6091000)
#define QT4_RADAR_TH1_ADDR			(QT4_TD_1_BASE_ADDR + 0x4e0)
#define QT4_RADAR_TH2_ADDR			(QT4_TD_1_BASE_ADDR + 0x4e4)
#define QT4_RADAR_M_ADDR			(QT4_TD_1_BASE_ADDR + 0x4c8)
#define QT4_RADAR_N_ADDR			(QT4_TD_1_BASE_ADDR + 0x4cc)
#define QT4_RADAR_DOWNSAMPLE_3_0_ADDR		(QT4_TD_1_BASE_ADDR + 0x4bc)
#define QT4_RADAR_NUMDELAY_3_0_ADDR		(QT4_TD_1_BASE_ADDR + 0x4d0)
#define QT4_RADAR_TAGMODE_ADDR			(QT4_TD_1_BASE_ADDR + 0x4dc)
#define QT4_RADAR_MINWIDTH_ADDR			(QT4_TD_1_BASE_ADDR + 0x4c4)
#define QT4_RADAR_MAXWIDTH_ADDR			(QT4_TD_1_BASE_ADDR + 0x4c0)
#define QT4_RADAR_RX_EXTENSION_AFTER_ADDR	(QT4_GLBL_BASE_ADDR + 0x0e0)
#define REG_RADAR_RX_EXTENSION_AFTER_DEFAULT	(0x00ff)
#define QT4_RADAR_RX_EXTENSION_BEFORE_ADDR	(QT4_TD_1_BASE_ADDR + 0x568)
#define REG_RADAR_RX_EXTENSION_BEFORE_DEFAULT	(0x00ff)
#define QT4_RADAR_RX_IN_PROG_EN_ADDR		(QT4_TD_1_BASE_ADDR + 0x4d4)
#define REG_RADAR_RX_IN_PROG_EN_DEFAULT		(0x1)
#define QT4_RADAR_SPRAM_IN_PROG_EN_ADDR		(QT4_TD_1_BASE_ADDR + 0x560)
#define REG_RADAR_SPRAM_IN_PROG_EN_DEFAULT	(0x0) /*(0x1) */
#define QT4_RADAR_INTR_EN_ADDR			(QT4_TD_1_BASE_ADDR + 0X524)
#define QT4_RADAR_CNT_L				(QT4_TD_1_BASE_ADDR + 0x518)
#define QT4_RADAR_PTR_L				(QT4_TD_1_BASE_ADDR + 0x51c)
#define QT4_RADAR_PTR_H				(QT4_TD_1_BASE_ADDR + 0x520)
#define QT4_RADAR_MEM_L				(0xe6401000)
#define QT4_RADAR_MEM_H				(0xe6401400)
#define ZC_OVERFLOW_ADDR_OFFSET			(64) /* 4096 % (21 * 4) */
#define ZC_ADDR_MASK				(0x7FF)
#define RADAR_ZC_MEM_SHIFT			(2)
#define RADAR_ZC_MASK_1				(0x000000FF)
#define RADAR_ZC_MASK_2				(0x0000FF00)
#define RADAR_ZC_MASK_3				(0x00FF0000)
#define RADAR_ZC_MASK_4				(0xFF000000)
#define RADAR_ZERO_CROSS_MEM_DEPTH		(896)
#define RADAR_ZERO_CROSS_MEM_PULSE_DEPTH	(48) /* floor(1024 / 21) */
#define RADAR_ZERO_CROSS_PROC_PULSE_DEPTH	(49) /* ceil(1024 / 21) */
#define RADAR_ZERO_CROSS_MEM_VALID_DEPTH	(42) /* floor(896 / 21) */
#define PULSE_MEM_INC_STEP			(0x4)
#define RADAR_PULSE_MEM_DEPTH			(256)
#define QT4_RADAR_MAXWR				(QT4_TD_1_BASE_ADDR + 0x514)
#define QT4_RADAR_ZWIN				(QT4_TD_1_BASE_ADDR + 0x510)
#define QT4_RADAR_ZC_ADR			(0xe6400000)
#define QT4_RADAR_ZC_ADR_INVLD			(0xe6400e00)
#define QT4_RADAR_ZC_ADR_END			(0xe6401000)
#define QT_BB_TD_RADAR_ZC_MAX_DEPTH		(0x1000)
#define QT4_RADAR_CUR_CHMEM_ADR			(QT4_TD_1_BASE_ADDR + 0x530)
#define QT4_RADAR_NEW_MODE			(QT4_TD_1_BASE_ADDR + 0x564)
#define QT_RADAR_NEW_MODE_DEFAULT		(0x1)
#define QT4_RADAR_NEW_MODE_MEM_L_PTR		(QT4_TD_1_BASE_ADDR + 0x570)
#define QT4_RADAR_NEW_MODE_START_PTR		(QT4_TD_1_BASE_ADDR + 0x56c)
#define FIFO_SIZE				(RADAR_PULSE_MEM_DEPTH)
#define RADAR_TAG_BIT				(0x08000000)
#define TH1_MAX					(0x7ffffff)
#define TH1_DEFAULT				(0x34000)
#define TH2_DEFAULT				(0x8000)
#define DSAMPLE_DEFAULT				(5)
#define NDELAY_DEFAULT				(5)
#define RXINPROGEN_DEFAULT			(0x1)
#define RADAR_IIR_HPF_ORDER_DEFAULT		(10)
#define RADAR_IIR_LPF_SHIFT_DEFAULT		(0)
#define RADAR_GAIN_NORM_DEFAULT			(0x82)
#define RADAR_GAIN_NORM_TARGET_GAIN_HWVAL	(0x23)
#define RADAR_GAIN_NORM_TARGET_GAIN_DEFAULT	(0x19)
#define QT4_RADAR_IIR_HPF_ORDER			(QT4_TD_1_BASE_ADDR + 0x540)
#define QT4_RADAR_IIR_LPF_SHIFT			(QT4_TD_1_BASE_ADDR + 0x504)
#define QT4_RADAR_GAIN_NORM_ADDR		(QT4_TD_1_BASE_ADDR + 0x508)
#define QT4_RADAR_GAIN_NORM_TARGET_GAIN_ADDR	(QT4_TD_1_BASE_ADDR + 0x534)
#define QT4_RADAR_AGC_IN_PROG_ADDR		(QT4_TD_1_BASE_ADDR + 0x4d8)
#define QT4_RADAR_TIMER_REC_EN_ADDR		(QT4_TD_0_BASE_ADDR + 0x530)
#define TIMER_REC_EN_ADDR_MASK			(0x80000000)
#define RADAR_AGC_IN_PROG_DEFAULT		(0x0)
#define QT4_RADAR_DC_LUT_ADDR			(QT4_TD_1_BASE_ADDR + 0x558)
#define RADAR_DC_LUT_DEFAULT			(0x0)
#define WIN_PER_PULSE				(28)
#define ZC_PER_PULSE				(4 * WIN_PER_PULSE)
#define ZC_TO_READ_PER_POLLING			(18)
#define MAX_PROC_PULSE_WRITE			(64)
#define MAX_PROC_ZC_WRITE			(16)
#define MAX_ZC_BAD_STATE_CTR			(2)
#define MAX_PULSE_CNT_TO_RESET			(1)
#define MAX_POLLING_CNT_BEFORE_RESET		(100)
#define ABS_MAX_POLLING_CNT_BEFORE_RESET	(160)
#define QT4_RADAR_NUM_OF_PULSES_B4_INT		(QT4_TD_1_BASE_ADDR + 0x4c8)
#define QT4_RADAR_CLEAR_INT_STATUS		(QT4_GLBL_BASE_ADDR + 0x320)
#define CLEAR_INT_STATUS_REG_RADAR_VAL		(0x2)
#define RADAR_PROC_SIZE				(8192)
#define RADAR_FH_PRI				(3330)
#define RADAR_MAX_POW_TRACK			(1)
#define RADAR_MAX_POW_MASK			(0xf8000000)
#define MAX_RADAR_PULSE_READ_CNT		(128)
#define MAX_RADAR_PULSE_READ_CNT_MASK		(0x7F)
#define QT4_RADAR_IIR_A1			(QT4_TD_1_BASE_ADDR + 0x4e8)
#define QT4_RADAR_IIR_A2			(QT4_TD_1_BASE_ADDR + 0x4ec)
#define QT4_RADAR_IIR_A3			(QT4_TD_1_BASE_ADDR + 0x4f0)
#define QT4_RADAR_IIR_B0			(QT4_TD_1_BASE_ADDR + 0x4f4)
#define QT4_RADAR_IIR_B1			(QT4_TD_1_BASE_ADDR + 0x4f8)
#define QT4_RADAR_IIR_B2			(QT4_TD_1_BASE_ADDR + 0x4fc)
#define QT4_RADAR_IIR_B3			(QT4_TD_1_BASE_ADDR + 0x500)

/*#define RADAR_IIR_A1_40				0x3015
#define RADAR_IIR_A2_40				0xc92
#define RADAR_IIR_A3_40				0x3c55
#define RADAR_IIR_B0_40				0x20
#define RADAR_IIR_B1_40				0x5f
#define RADAR_IIR_B2_40				0x5f
#define RADAR_IIR_B3_40				0x20

#define RADAR_IIR_A1_80				0x39eb
#define RADAR_IIR_A2_80				0x59e
#define RADAR_IIR_A3_80				0x3e54
#define RADAR_IIR_B0_80				0xbc
#define RADAR_IIR_B1_80				0x233
#define RADAR_IIR_B2_80				0x233
#define RADAR_IIR_B3_80				0xbc  // cover 80% of bw [-32,32] for 80MHz, [-16, 16] for 40MHz*/

#define RADAR_IIR_A1_40				0x33ec
#define RADAR_IIR_A2_40				0x929
#define RADAR_IIR_A3_40				0x3d4d
#define RADAR_IIR_B0_40				0x4c
#define RADAR_IIR_B1_40				0xe5
#define RADAR_IIR_B2_40				0xe5
#define RADAR_IIR_B3_40				0x4c 

#define RADAR_IIR_A1_80				0x3F7E
#define RADAR_IIR_A2_80				0x421
#define RADAR_IIR_A3_80				0x3F0F
#define RADAR_IIR_B0_80				0x156
#define RADAR_IIR_B1_80				0x401
#define RADAR_IIR_B2_80				0x401
#define RADAR_IIR_B3_80				0x156 //cover 100% bw [-40, 40] for 80MHz, [-20, 20] for 40MHz


#define QT4_RADAR_TX_EXT			(QT4_TD_1_BASE_ADDR + 0x53c)
#define QT4_RADAR_TX_EXT_VALUE			0x140	// unit is 0.00625us


#define QT3_RADAR_TH1_ADDR			(0xe6090520)
#define QT3_RADAR_TH2_ADDR			(0xe6090524)
#define QT3_RADAR_M_ADDR			(0xe6090508)
#define QT3_RADAR_N_ADDR			(0xe609050c)
#define QT3_RADAR_DOWNSAMPLE_3_0_ADDR		(0xe60904fc)
#define QT3_RADAR_NUMDELAY_3_0_ADDR		(0xe6090510)
#define QT3_RADAR_TAGMODE_ADDR			(0xe609051c)
#define QT3_RADAR_MINWIDTH_ADDR			(0xe6090504)
#define QT3_RADAR_MAXWIDTH_ADDR			(0xe6090500)
#define QT3_RADAR_RX_EXTENSION_AFTER_ADDR	(0xe60000e0)
#define QT3_RADAR_RX_EXTENSION_BEFORE_ADDR	(0xe60905a8)
#define QT3_RADAR_RX_IN_PROG_EN_ADDR		(0xe6090514)
#define QT3_RADAR_SPRAM_IN_PROG_EN_ADDR		(0xe60905a0)
#define QT3_RADAR_INTR_EN_ADDR			(0xe6090564)
#define QT3_RADAR_CNT_L				(0xe6090558)
#define QT3_RADAR_PTR_L				(0xe609055c)
#define QT3_RADAR_PTR_H				(0xe6090560)
#define QT3_RADAR_MEM_L				(0xe6401000)
#define QT3_RADAR_MEM_H				(0xe6401400)
#define QT3_RADAR_MAXWR				(0xe6090554)
#define QT3_RADAR_ZWIN				(0xe6090550)
#define QT3_RADAR_ZC_ADR			(0xe6400000)
#define QT3_RADAR_ZC_ADR_INVLD			(0xe6400e00)
#define QT3_RADAR_ZC_ADR_END			(0xe6401000)
#define QT3_RADAR_CUR_CHMEM_ADR			(0xe6090570)
#define QT3_RADAR_NEW_MODE			(0xe60905a4)
#define QT3_RADAR_NEW_MODE_MEM_L_PTR		(0xe60905b0)
#define QT3_RADAR_NEW_MODE_START_PTR		(0xe60905ac)
#define QT3_RADAR_IIR_HPF_ORDER			(0xe6090580)
#define QT3_RADAR_IIR_LPF_SHIFT			(0xe6090544)
#define QT3_RADAR_GAIN_NORM_ADDR		(0xe6090548)
#define QT3_RADAR_GAIN_NORM_TARGET_GAIN_ADDR	(0xe6090574)
#define QT3_RADAR_AGC_IN_PROG_ADDR		(0xe6090518)
#define QT3_RADAR_TIMER_REC_EN_ADDR		(0xe60905f4)
#define QT3_RADAR_DC_LUT_ADDR			(0xe6090598)
#define QT3_RADAR_NUM_OF_PULSES_B4_INT		(0xe6090508)
#define QT3_RADAR_CLEAR_INT_STATUS		(0xe6000320)
#define QT3_TD_0_BASE_ADDR                      (0xe6090000)
#define QT3_RADAR_IIR_A1                        (QT3_TD_0_BASE_ADDR + 0x528)
#define QT3_RADAR_IIR_A2                        (QT3_TD_0_BASE_ADDR + 0x52c)
#define QT3_RADAR_IIR_A3                        (QT3_TD_0_BASE_ADDR + 0x530)
#define QT3_RADAR_IIR_B0                        (QT3_TD_0_BASE_ADDR + 0x534)
#define QT3_RADAR_IIR_B1                        (QT3_TD_0_BASE_ADDR + 0x538)
#define QT3_RADAR_IIR_B2                        (QT3_TD_0_BASE_ADDR + 0x53c)
#define QT3_RADAR_IIR_B3                        (QT3_TD_0_BASE_ADDR + 0x540)
#define QT3_RADAR_TX_EXT                        (QT3_TD_0_BASE_ADDR + 0x57c)
#define QT3_RADAR_TX_EXT_VALUE                  0x00

# define QT_RADAR(x)		QT4_RADAR_##x


typedef struct {
	u32		base[2];
	u32		ptr;
	u32		bb_base;
	int		irq;
	u32		flags;
	u32		n_irq;

	u32		poll_freq;

	u32		th1;			/* threshold 1 */
	u32		th2;			/* threshold 2 */

	u32		flush;			/* timer wrap */
	u32		irq_en;
	spinlock_t	lock;
	u32		chip;			/* chipID */

	u32		radar_en;
	void		(*callback)(void);
	bool		(*radar_is_dfs_chan)(uint8_t wifi_chan);
	bool		(*radar_is_dfs_weather_chan)(uint8_t wifi_chan);

	void		*radar_stats_send_arg;
	void		(*radar_stats_send)(void *data,
	int		(*pulse_copy_iter)(void *dest, void *src, int pulse_indx),
	void		*pulse_buf, int num_pulses);

	/*
	 * Fields for pulse history maintenance.
	 * - all fields set to 0 during initialization
	 * - at each polling, ph_buf[ph_idx] is updated with a new pulse count and then ph_idx is
	 *   changed to next (ring)
	 * - at each polling, ph_sum is updated
	 */
#define RADAR_PH_BUFLEN			20	/* The radar pulse history buffer length */
#define RADAR_PH_BUFLEN_LSR		240	/* The LSR pulse history buffer length */
#define RADAR_TIMER_BAD			9	/* Wait time in the bad state */
#define RADAR_TIMER_BAD_MAX		24	/* Wait time when a rejection happens */
#define RADAR_TIMER_STEP_DETECT		2	/* Radar timer step in the bad state when the detected radars match */
#define RADAR_PS_TIMER_MAX		20
#define RADAR_MIN_DETECT_PULSE_CNT	14	/* If number of pulses in a window are bigger than RADAR_MIN_DETECT_PULSE_CNT, there should be a valid detection */

	u16		ph_buf[RADAR_PH_BUFLEN_LSR]; /* pulse count hisotry buffer (ring) */
	u16		ph_idx;			/* where in 'ph_buf' to write a new pulse count */
	u32		ph_sum;			/* sum of all pulse counts in 'ph_buf' */
	u16		ph_idx_LSR;		/* where in 'ph_buf' to write a new pulse count */
	u32		ph_sum_LSR;		/* sum of all pulse counts in 'ph_buf' */
	unsigned	last_rej_w;		/* Width of the last radar detection */
	unsigned	last_rej_pri[3];	/* PRI of the last radar detection */
	bool		curr_radar_state;	/* Current state of the radar logic; true -> good state, and false -> bad state */
	int		bad_state_timer;	/* Current time at the bad state */
	bool		bad_state_detection;	/* Has there been any radar detections in the bad state? */

	unsigned	poll_cnt_after_reset;
	unsigned	pulse_timer_reset_time;
	unsigned	pulse_timer_reset_cnt;

	u8		wifi_channel;
	uint32_t	bw;
} radar_cb_t;

#define RADAR_OCAC_BEGIN	BIT(1)
#define RADAR_OCAC_END		BIT(0)

#define RADAR_OCAC_PTS_SIZE	10

struct radar_fifo_pt_s {
        uint8_t			ocac_status;
        uint32_t		fifo_pt;
};

/*
 * Shared by QDRV and RADAR
 */
struct radar_ocac_info_s {
	spinlock_t		lock;
	bool			ocac_enabled;
	uint8_t			ocac_scan_chan;
	struct radar_fifo_pt_s	ocac_radar_pts[RADAR_OCAC_PTS_SIZE];
	uint8_t			array_ps;	/* array is ocac_radar_pts */
	bool			weather_channel_yes;	/*0: not weather channel, 1: weather channel */
};

#define DFS_RQMT_UNKNOWN        0
#define DFS_RQMT_EU             1
#define DFS_RQMT_US             2
#define DFS_RQMT_JP             3
#define DFS_RQMT_AU             4
#define DFS_RQMT_BR		5
#define DFS_RQMT_CL		6

int radar_register(void (*dfs_mark_callback)(void));
int radar_register_is_dfs_chan(
	bool (*qdrv_radar_is_dfs_chan)(uint8_t wifi_chan));
int radar_register_is_dfs_weather_chan(
	bool (*qdrv_radar_is_dfs_weather_chan)(uint8_t wifi_chan));
void radar_disable(void);
void radar_enable(void);
bool radar_start(const char *region);
void radar_stop(void);
int radar_register_statcb(void (*stats_send)(void *data,
	int (*pulse_copy_iter)(void *dest, void *src, int pulse_indx),
	void *pulse_buf, int num_pulses),
	void *stats_send_arg);
unsigned dfs_rqmt_code(const char *region);
void radar_set_shared_params(struct shared_params *p_params);
int radar_set_chan(uint8_t chan);
uint8_t radar_get_chan(void);
bool sta_dfs_is_region_required(void);
void qdrv_sta_dfs_enable(int sta_dfs_enable);
void qdrv_cac_instant_completed(void);
struct radar_ocac_info_s *radar_ocac_info_addr_get(void);
void radar_record_buffer_pt(uint32_t *pt);
bool radar_get_status(void);
void radar_set_bw(uint32_t bw);

#endif /* __RADAR_H__ */
