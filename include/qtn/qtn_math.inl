/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2008 - 2008 Quantenna Communications Inc            **
**                            All Rights Reserved                            **
**                                                                           **
**  Author      : Quantenna                                                  **
**  Date        : 12/04/08                                                   **
**  File        : qdrv_math.c                                                **
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

#include <qtn/qtn_math.h>
#include <qtn/muc_phy_stats.h>

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#endif

#define NUM_VALID_ONE_STREAM 1
#define NUM_VALID_TWO_STREAM 2
#define NUM_VALID_THREE_STREAM 3
#define NUM_VALID_FOUR_STREAM 4

#define NUM_BITS_FOR_FRACTION 4
#define NUM_BITS_EVM_MANT_ONE 2048
#define NUM_BITS_EVM_MANT_SHIFT 12
#define NUM_BITS_EVM_EXP_SHIFT 16
#define NUM_BITS_GUARD_TINY_SHIFT 7

static const u_int16_t lut_1024_10log10[] = {
	#include "./log_table/1024_10log10_table.txt"
};

u_int8_t highest_one_bit_pos(u_int32_t val)
{
	u_int32_t shift;
	u_int32_t pos = 0;

	if (val == 0) return 0;

	shift = (val & 0xFFFF0000) ? 16 : 0; val >>= shift; pos |= shift;
	shift = (val & 0xFF00    ) ?  8 : 0; val >>= shift; pos |= shift;
	shift = (val & 0xF0      ) ?  4 : 0; val >>= shift; pos |= shift;
	shift = (val & 0xC       ) ?  2 : 0; val >>= shift; pos |= shift;
	shift = (val & 0x2       ) ?  1 : 0;                pos |= shift;
	pos++;
	return (u_int8_t) pos;
}

u_int32_t rshift_round(u_int32_t x, int shift)
{
	u_int32_t z;
		
	if (shift == 0)
		return x;

	z = x >> (shift - 1);
	z += (z & 1);
	z >>= 1;
	return z;
}

int linear_to_10log10(u_int32_t x, int8_t nbit_frac_in, int8_t nbit_frac_out)
{
	u_int8_t shift;

	if (x <= 0)
		return (int)0x80000001;  // 10*log10(0) = -infinity

	shift = MAX(highest_one_bit_pos(x) - 8, 0);

	//printk("shift = %d , x = %d, lut_1024_10log10[(x >> shift) - 1] = %d \n\n", shift, x, lut_1024_10log10[(x >> shift) - 1]);

	// y = round((1024*10*log10(x/2^shift) + (shift-nbit_frac_in)*1024*10*log10(2)) / 2^(10-nbit_frac_out))
	// 49321 = round(16*1024*10*log10(2))
	return rshift_round((int)(lut_1024_10log10[(x >> shift) - 1] + (((shift - nbit_frac_in) * 49321) >> 4)), 10 - nbit_frac_out) ;
}

int divide_by_16_x_10000(int x)
{
	return (x * 625); //10000/16
};


u_int16_t conv_linear_mantissa(long val, short se)
{
	u_int16_t evm_out;
	short     sht_count;
	long shift_mask;

	if (val == 0) return 0;

	if (se < 1 || se > 32)
		return 0; // se is in [1:32]

	shift_mask = 0x000007FF; // MSB mask for 2048

	if (se > NUM_BITS_EVM_MANT_SHIFT ) {
		sht_count = se - NUM_BITS_EVM_MANT_SHIFT  ;
		shift_mask = shift_mask << sht_count;
		evm_out = (u_int16_t) (val & shift_mask) >> sht_count;
	} else {
		sht_count = NUM_BITS_EVM_MANT_SHIFT  - se ;
		shift_mask = shift_mask >> sht_count;
		evm_out = (u_int16_t) (val & shift_mask) << sht_count;
	}

	return  (evm_out);
}

void average_evm_db(const uint32_t *evm_array, int n_sym, int *evm_int, int *evm_frac)
{
	int  man[4], exp[4], evm_exp_val[4];
	int  y, x, evm_4bit_fraction, fraction, db_sign = 0;
	int  k, valid_evm_cnt = 0, min_exp_val =0, guard_bit_shift = 0;
	int  evm_exp_sum=0, evm_exp_mul=0, evm_mant_sum=0, evm_mant_mul=0;
	long evm_tmp_sum=0, evm_tmp_mul=0, linear_evm_val[4] ={0,0,0,0};

	if (n_sym < 3)
		return;

	for ( k = 0; k < 4; k++) {
		if (evm_array[k] != MUC_PHY_ERR_SUM_NOT_AVAIL) {// invalid EVM values

			valid_evm_cnt++;
			man[k] = (u_int32_t)(evm_array[k] >> 5);
			exp[k] = (evm_array[k] - man[k] * 32);
			man[k] += NUM_BITS_EVM_MANT_ONE;

			if ( exp[k] > NUM_BITS_EVM_EXP_SHIFT )
				linear_evm_val[k] =  (man[k]) << (exp[k] - NUM_BITS_EVM_EXP_SHIFT );
			else
				linear_evm_val[k] = (man[k]) >> (NUM_BITS_EVM_EXP_SHIFT - exp[k]);

			exp[k] = exp[k] - NUM_BITS_EVM_EXP_SHIFT;

			//printk("reg=%x, man = %d, exp = %d, n_sym=%d\n", evm_array[k], man[k], exp[k], n_sym);
		}
	}

	if ( valid_evm_cnt == NUM_VALID_ONE_STREAM ) { // only one stream
		y = linear_to_10log10((man[0]), 0, NUM_BITS_FOR_FRACTION)  +
			((exp[0] - 11) * linear_to_10log10(2, 0, NUM_BITS_FOR_FRACTION)) ;
	} else {

		switch (valid_evm_cnt) {

			case NUM_VALID_TWO_STREAM: //log(b+a) - log(a*b)
				// get log(a + b)
				evm_mant_sum = (long) (linear_evm_val[0]);
				evm_mant_sum += (long) (linear_evm_val[1]);
				evm_exp_sum = (int) highest_one_bit_pos(evm_mant_sum );
				evm_mant_sum = conv_linear_mantissa(evm_mant_sum, evm_exp_sum);
				evm_exp_sum -= NUM_BITS_EVM_MANT_SHIFT ;
				// get log(a*b)
				evm_tmp_mul = (long) (man[0]*man[1]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;

				evm_exp_mul = (int) highest_one_bit_pos( evm_mant_mul );
				evm_mant_mul = conv_linear_mantissa(evm_mant_mul, evm_exp_mul);
				evm_exp_mul =  (1) + (evm_exp_mul - NUM_BITS_EVM_MANT_SHIFT );  // 1 time shift 12 bits
				evm_exp_mul += (exp[0]+exp[1]);

				break;

			case NUM_VALID_THREE_STREAM: // log(bc+ab+ac) -log(abc)
				// get 3 terms product: bc
				evm_tmp_mul = (long) (man[1]*man[2]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;
				evm_exp_val[0] = 1+(exp[1]+exp[2]);

				min_exp_val = MIN(evm_exp_val[0], min_exp_val);
				linear_evm_val[0] = (evm_mant_mul);

				// get 3 terms product: ac
				evm_tmp_mul = (long) (man[0]*man[2]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;
				evm_exp_val[1] = 1+(exp[0]+exp[2]);

				min_exp_val = MIN(evm_exp_val[1], min_exp_val);
				linear_evm_val[1] = (evm_mant_mul);

				// get 3 terms product: ab
				evm_tmp_mul = (long) (man[0]*man[1]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;
				evm_exp_val[2] = 1+(exp[0]+exp[1]);

				min_exp_val = MIN(evm_exp_val[2], min_exp_val);
				linear_evm_val[2] = (evm_mant_mul);

				// check the tiny input cases
				guard_bit_shift = min_exp_val + NUM_BITS_EVM_MANT_SHIFT;

				if ( guard_bit_shift < NUM_BITS_GUARD_TINY_SHIFT )
				       // min 7 bits for log table
				       guard_bit_shift  = (-min_exp_val - NUM_BITS_EVM_MANT_SHIFT) + NUM_BITS_GUARD_TINY_SHIFT;
				else   guard_bit_shift = 0;

				// left-shift up to bit 30
				guard_bit_shift = MIN(guard_bit_shift, (30-NUM_BITS_EVM_MANT_SHIFT));

				for ( k = 0; k < valid_evm_cnt ; k++) {
				  linear_evm_val[k] = (long)(linear_evm_val[k]) << guard_bit_shift;
				  if ( evm_exp_val[k] >= 0 )
				    linear_evm_val[k] <<=  (evm_exp_val[k]);
				  else
				    linear_evm_val[k] >>=  (-evm_exp_val[k]);
				}


				// get summatioon of t2 terms products: bc + ac +ab
				evm_tmp_sum = (long) (linear_evm_val[0]);
				evm_tmp_sum += (long) (linear_evm_val[1]);
				evm_tmp_sum += (long) (linear_evm_val[2]);
				evm_exp_sum = (int) highest_one_bit_pos( evm_tmp_sum );
				evm_mant_sum = conv_linear_mantissa(evm_tmp_sum, evm_exp_sum);
				evm_exp_sum -= (NUM_BITS_EVM_MANT_SHIFT  + guard_bit_shift);

				// get 3 term products : a*b*c
				evm_tmp_mul = (long) (man[0]*man[1]) ;
				evm_mant_mul = (int) ( evm_tmp_mul) >> NUM_BITS_EVM_MANT_SHIFT ;
				evm_tmp_mul = (long) (evm_mant_mul * man[2]);
				evm_mant_mul = (int) ( evm_tmp_mul) >> NUM_BITS_EVM_MANT_SHIFT ;

				evm_exp_mul = (int) highest_one_bit_pos( evm_mant_mul );
				evm_mant_mul = conv_linear_mantissa(evm_mant_mul, evm_exp_mul);
				evm_exp_mul =  (2) + (evm_exp_mul - NUM_BITS_EVM_MANT_SHIFT );  // 2 times shift 12 bits 
				evm_exp_mul += (exp[0]+exp[1]+exp[2]);

				break;

			case NUM_VALID_FOUR_STREAM: // log(bcd+acd+abd+abc) -log(abcd)

				// get 3 terms product: bcd
				evm_tmp_mul = (long) (man[1]*man[2]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;
				evm_tmp_mul = (long) evm_mant_mul * (man[3]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;

				evm_exp_val[0] = 2+(exp[1]+exp[2]+exp[3]);
				min_exp_val = MIN(evm_exp_val[0], min_exp_val);
				linear_evm_val[0] = (long) (evm_mant_mul);


				// get 3 terms product: acd
				evm_tmp_mul = (long) (man[0]*man[2]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;
				evm_tmp_mul = (long) evm_mant_mul * (man[3]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;

				evm_exp_val[1] = 2+(exp[0]+exp[2]+exp[3]);
				min_exp_val = MIN(evm_exp_val[1], min_exp_val);
				linear_evm_val[1] = (long) (evm_mant_mul);

				// get 3 terms product: abd
				evm_tmp_mul = (long) (man[0]*man[1]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;
				evm_tmp_mul = (long) evm_mant_mul * (man[3]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;

				evm_exp_val[2] = 2+(exp[0]+exp[1]+exp[3]);
				min_exp_val = MIN(evm_exp_val[2], min_exp_val);
				linear_evm_val[2] = (long) (evm_mant_mul);

				// get 3 terms product: abc
				evm_tmp_mul = (long) (man[0]*man[1]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;
				evm_tmp_mul = (long) evm_mant_mul * (man[2]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;

				evm_exp_val[3] = 2+(exp[0]+exp[1]+exp[2]);
				min_exp_val = MIN(evm_exp_val[3], min_exp_val);
				linear_evm_val[3] = (long) (evm_mant_mul);

				// check the tiny input cases
				guard_bit_shift = min_exp_val + NUM_BITS_EVM_MANT_SHIFT;

				if ( guard_bit_shift < NUM_BITS_GUARD_TINY_SHIFT )
				       // min 7 bits for log table
				       guard_bit_shift  = (-min_exp_val - NUM_BITS_EVM_MANT_SHIFT) + NUM_BITS_GUARD_TINY_SHIFT;
				else   guard_bit_shift = 0;

				// left-shift up to bit 30
				guard_bit_shift = MIN(guard_bit_shift, (30-NUM_BITS_EVM_MANT_SHIFT));

				for ( k = 0; k < valid_evm_cnt ; k++) {
				  linear_evm_val[k] = (long)(linear_evm_val[k]) << guard_bit_shift;
				  if ( evm_exp_val[k] >= 0 )
				    linear_evm_val[k] <<=  (evm_exp_val[k]);
				  else
				    linear_evm_val[k] >>=  (-evm_exp_val[k]);
				}


				// summation of 3 term products : bcd+acd+abd+abc
				evm_mant_sum  = (long) (linear_evm_val[0]);
				evm_mant_sum += (long) (linear_evm_val[1]);
				evm_mant_sum += (long) (linear_evm_val[2]);
				evm_mant_sum += (long) (linear_evm_val[3]);

				evm_exp_sum = (int) highest_one_bit_pos( evm_mant_sum );
				evm_mant_sum = conv_linear_mantissa(evm_mant_sum, evm_exp_sum);
				evm_exp_sum -= (NUM_BITS_EVM_MANT_SHIFT +  guard_bit_shift);

				// get 4 terms products : a*b*c*d
				evm_tmp_mul = (long) (man[0]*man[1]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;
				evm_tmp_mul = (long) evm_mant_mul * (man[2]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;
				evm_tmp_mul = (long) evm_mant_mul * (man[3]);
				evm_mant_mul = (int) evm_tmp_mul >> NUM_BITS_EVM_MANT_SHIFT ;

				evm_exp_mul = (int) highest_one_bit_pos( evm_mant_mul );
				evm_mant_mul = conv_linear_mantissa(evm_mant_mul, evm_exp_mul);
				evm_exp_mul =  (3) + (evm_exp_mul - NUM_BITS_EVM_MANT_SHIFT );  // 3 times shift 12 bits 
				evm_exp_mul += (exp[0]+exp[1]+exp[2]+exp[3]);

				break;
		}

		//printk("sum   = (%d, %d)\n", evm_mant_sum, evm_exp_sum);
		//printk("prod  = (%d, %d)\n", (int) evm_mant_mul, evm_exp_mul);

		y  = -linear_to_10log10((evm_mant_sum + NUM_BITS_EVM_MANT_ONE), 0, NUM_BITS_FOR_FRACTION);
		y -=  (evm_exp_sum) * linear_to_10log10(2, 0, NUM_BITS_FOR_FRACTION);

		y +=  linear_to_10log10((evm_mant_mul + NUM_BITS_EVM_MANT_ONE), 0, NUM_BITS_FOR_FRACTION);
		y +=  (evm_exp_mul) * linear_to_10log10(2, 0, NUM_BITS_FOR_FRACTION);
	}

	x = linear_to_10log10(n_sym-3, 0, NUM_BITS_FOR_FRACTION);

	evm_4bit_fraction = y - x;

	// fix bug: negative dB shift error
	if ( evm_4bit_fraction < 0 ) {
		db_sign = 1;
		evm_4bit_fraction = ABS(evm_4bit_fraction);
	}

	y = (evm_4bit_fraction >> NUM_BITS_FOR_FRACTION);
	fraction = evm_4bit_fraction - (y << NUM_BITS_FOR_FRACTION);

	if ( db_sign ==1 )
		*evm_int = -y;
	else
		*evm_int = y;

	*evm_frac = divide_by_16_x_10000(fraction);

	//printk("int = %d, frac = %d\n", *evm_int, *evm_frac);
}

void convert_evm_db(u_int32_t evm_reg, int n_sym, int *evm_int, int *evm_frac)
{
	int man, exp, log_table_index, y, x, evm_4bit_fraction, fraction,db_sign = 0;

	if (n_sym < 3)
		return;

	man = (u_int32_t)(evm_reg >> 5);
	exp = (evm_reg - man * 32);

	//printk("man = %d, exp = %d\n", man, exp);


	log_table_index = (2048 + man) ;

	y = linear_to_10log10(log_table_index, 0, NUM_BITS_FOR_FRACTION);
	y += ((exp - 16) * linear_to_10log10(2, 0, NUM_BITS_FOR_FRACTION));
	y -= (11 * linear_to_10log10(2, 0, NUM_BITS_FOR_FRACTION));
	x = linear_to_10log10(n_sym-3, 0, NUM_BITS_FOR_FRACTION);

	//printk("y = %d, x = %d\n", y, x);

	evm_4bit_fraction = y - x;

	// fix bug: negative dB shift error
	if ( evm_4bit_fraction < 0 ) {
		db_sign = 1;
		evm_4bit_fraction = ABS(evm_4bit_fraction);
	}

	y = (evm_4bit_fraction >> NUM_BITS_FOR_FRACTION);
	fraction = evm_4bit_fraction - (y << NUM_BITS_FOR_FRACTION);

	if ( db_sign==1 )
		*evm_int = -y;
	else
		*evm_int =  y;

	*evm_frac = divide_by_16_x_10000(fraction);

	//printk("int = %d, frac = %d\n", *evm_int, *evm_frac);
}

#ifdef FLOAT_SUPPORT
inline double pow_int(int x, int y)
{
	unsigned int n;
	double z;

	if (y >= 0)
		n = (unsigned int)y;
	else
		n = (unsigned int)(-y);

	for (z = 1; ; x *= x) {
		if ((n & 1) != 0)
			z *= x;
		if ((n >>= 1) == 0)
			return (y < 0 ? 1 / z : z);
	}
};
#endif //#ifdef FLOAT_SUPPORT

