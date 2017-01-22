#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "compat.h"
#include "sscl.h"

#define QDRV_INPUT_FILE "/sys/devices/qdrv/control"
#define QDRV_OUTPUT_FILE "/proc/qdrvdata"

struct sscl_state g_alg_state;

void sscl_dumpstate(struct sscl_state *state, int disp_snr_bins)
{
	int mcs_indx,snr_indx,num_snr_bins;
	struct sscl_mcs_per_info *mcs_info;
	
	num_snr_bins = disp_snr_bins?SSCL_NUM_SNR_BINS:0;

	printf("Current MRR: %d, %d, %d, %d\n",
			state->curr_rates.best_tput,
			state->curr_rates.best_tput2,
			state->curr_rates.best_perr,
			state->curr_rates.base_rate
		  );

	printf("Avg Util: %f, BP rate %d\n",
				(float)state->avg_util / (1<<16),
				state->curr_rates.bpres_rate);

	printf("SNR Info: SNR %d, Tstamp %08X(ms)\n",
			state->snr_fb[0].snr,state->snr_fb[0].ts);

	printf("Next Update: %08X(ms)\n",(u_int32_t)state->next_update); 

	printf("PER vs SNR Table::\n");
	printf("MCS\tDef\t\tSNROff\t");
	for(snr_indx=0;snr_indx < num_snr_bins; snr_indx++){
		printf("+%d\t\t",snr_indx);
	}
	printf("\n");

	for(mcs_indx=0;mcs_indx<NUM_SSCL_RATES;mcs_indx++){
		mcs_info = &state->pervssnr_tbl[mcs_indx];
		printf("%d\t%f\t%d\t",mcs_indx,
			(float)mcs_info->def_per.per / (1<<16),mcs_info->start_snr);

		for(snr_indx=0;snr_indx < num_snr_bins; snr_indx++){
			printf("%f\t",(float)mcs_info->snr_per[snr_indx].per / (1<<16));
		}
		printf("\n");

		
		printf("Hits\t%d\t\t\t",mcs_info->def_per.hits);
		for(snr_indx=0;snr_indx < num_snr_bins; snr_indx++){
			printf("%d\t\t",mcs_info->snr_per[snr_indx].hits);
		}
		printf("\n");
	}
}

int get_mem_dump(u_int32_t start_address, int num_bytes, char *buff)
{

	FILE *fp;
	int num_words = (num_bytes + 0) / sizeof(u_int32_t);
	int num_words_curr;
	u_int32_t addr,word;

	//printf("Reading %d words from %08X\n",num_words,start_address);

	while(num_words > 0){

		num_words_curr = (num_words > 64)?64:num_words;

		//printf("Reading %d words from %08X\n",num_words_curr,start_address);

		fp = fopen(QDRV_INPUT_FILE,"w");
		fprintf(fp,"read addr %08X %d",start_address, num_words_curr);
		fflush(fp);
		fclose(fp);

		start_address += num_words_curr * sizeof(u_int32_t);
		num_words -= num_words_curr;

		sleep(1);

		//mem[0x80001000] = 0x00000000
		fp = fopen(QDRV_OUTPUT_FILE,"r");
		while(num_words_curr){
			fscanf(fp,"mem[0x%X] = 0x%X\n",&addr, &word);
			//printf("0x%08X\n",word);
			*(u_int32_t*)buff = word;
			buff+= 4;
			num_words_curr--;
		}

	
#if 0
		if(num_bytes > 0){
			fscanf(fp,"mem[0x%X] = 0x%X\n",&addr, &word);
			while(num_bytes--){
				/* LE Processor */
				*buff++ = word & 0xFF;
				word >>= 8;
			}
		}
#endif

		fclose(fp);


	}


	return(0);
}

int main(int argc, char *argv[])
{
	if(argc < 2 ){
		printf("Usage: sscldbg <State Address> <full>\n");
		return(0);
	}

	get_mem_dump((unsigned)strtoul(argv[1],NULL,0), sizeof(struct sscl_state), 
			(char*)&g_alg_state);
	sscl_dumpstate(&g_alg_state, (argc > 2) );
	return(0);
}




