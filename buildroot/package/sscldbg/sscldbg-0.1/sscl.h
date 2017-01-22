#ifndef _QTN_SSCL_PRIV_H_
#define _QTN_SSCL_PRIV_H_

/* Turn this on for more debug info */
#define SSCL_DEBUG_ON

#define NUM_SSCL_RATES 		(16)
#define SSCL_NUM_SNR_BINS 	(8) 
#define SNR_BINS_NOT_INIT	(-2000)

#define SSCL_INIT_PER 		(0xFFFF)
#define SSCL_SNR_BIN_0_LO 	(0x0100)
#define SSCL_SNR_BIN_N_HI 	(0x1000)
#define SSCL_QUANT_SNR_TO_BIN(S) ((int)S >> 2)

#define SET_FLAG(X,F) (X | (0x1 << F))
#define CHK_FLAG(X,F) ((X>>F) & 0x1)

/* Alg Control Flags */
enum SSCL_ALG_CTRL_FLAGS {
	SSCL_CTRL_IGNORE_SNR_FB =0,	/* Ignore SNR and run in open loop */
	SSCL_CTRL_MRR_DESC_ONLY,	/* Force 2nd Best Tput MCS < Best Tput MCS */
	SSCL_CTRL_SWR_CONS,			/* Use 2nd rate and do not sample on sw retry */
	SSCL_CTRL_SAMP_LOW,			/* Sample lower rates than current with eq prob */
	SSCL_CTRL_BACK_PRES,		/* Flag to turn on the back pressure based rate lowering */
};

struct sscl_per_info {
#ifdef SSCL_DEBUG_ON
	u_int32_t hits;
	u_int16_t pad;
#endif
	u_int16_t per;
}__packed;

struct sscl_mcs_per_info {
	struct sscl_per_info def_per;	
	struct sscl_per_info snr_per[SSCL_NUM_SNR_BINS];
	int16_t start_snr;
	int16_t move_tbl_right;
}__packed;

struct sscl_selected_rates {
	char best_tput;
	char best_tput2;
	char best_perr;
	char base_rate;
	char bpres_rate;
	char pad;
}__packed;

struct sscl_snr_fb_info {
	u_int32_t ts;
	signed char snr;
	char pad[3];
}__packed;


struct sscl_state {
	u_int64_t next_update;
	struct sscl_selected_rates curr_rates;
	u_int16_t avg_util;
	struct sscl_snr_fb_info snr_fb[1];
	u_int32_t curr_pkts[NUM_SSCL_RATES];
	u_int32_t curr_pkerr[NUM_SSCL_RATES];
	struct sscl_mcs_per_info pervssnr_tbl[NUM_SSCL_RATES];
}__packed;


struct sscl_params {
	u_int32_t update_interval;
	u_int16_t max_tputs[NUM_SSCL_RATES];
	u_int16_t sample_spacing; 
	u_int16_t snr_valid_time;
	u_int16_t flags;
	u_int16_t util_lo;
	u_int16_t util_hi;
	u_int8_t ewma_shift;
	u_int8_t util_ewma_shift;	
	u_int8_t min_pkts_for_stat;
}__packed;

#endif

