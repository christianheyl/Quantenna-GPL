/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2009 Quantenna Communications Inc            **
**                            All Rights Reserved                            **
**                                                                           **
**  File        : qcsapi_rftest.h                                            **
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
EH1*/

#ifndef _QCSAPI_RFTEST_H
#define _QCSAPI_RFTEST_H

#include <stdio.h>
#include <errno.h>

#include "qcsapi.h"

#ifdef __WIN32__
    #define inline
#else
    #ifndef _stdcall
    #define _stdcall
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int8_t	qcsapi_s8;
typedef uint8_t	qcsapi_u8;

/*
 * These  three bits need to be set always in the antenna mask
 */
#define SPECIAL_ANTENNA_MASK	0x70

typedef enum {
	qcsapi_dump_HTTP_POST,
	qcsapi_dump_ascii_text,
	qcsapi_nosuch_dump_format
} qcsapi_dump_format;

typedef enum
{
	e_qcsapi_rftest_chan,
	e_qcsapi_packet_size,
	e_qcsapi_packet_count,
	e_qcsapi_packet_type,
	e_qcsapi_cw_pattern,
	e_qcsapi_rfpath,
	e_qcsapi_SSB_setting,
	e_qcsapi_tx_power_cal,
	e_qcsapi_rftest_bandwidth,
	e_qcsapi_tx_power,
	e_qcsapi_antenna_mask,
	e_qcsapi_harmonics,
	e_qcsapi_chip_index,
	e_qcsapi_rftest_mcs_rate,
	e_qcsapi_nosuch_rftest_param
} qcsapi_enum_rftest_param;

typedef enum
{
	qscapi_packet_legacy = 0,
	qscapi_packet_802_11n = 1,
	qcsapi_nosuch_packet
} qcsapi_packet_type;

typedef enum
{
	qcscapi_cw_625KHz_0dB = 0,
	qcscapi_cw_625KHz_3dB = 1,
	qcscapi_cw_625KHz_6dB = 2,
	qcscapi_cw_625KHz_9dB = 3,
	qcscapi_cw_1MHz_0dB = 4,
	qcscapi_cw_1MHz_3dB = 5,
	qcscapi_cw_1MHz_6dB = 6,
	qcscapi_cw_1MHz_9dB = 7,
	qcscapi_nosuch_cw_pattern
} qcsapi_cw_pattern;

typedef enum
{
	qcsapi_rfpath_chain0_main = 0,
	qcsapi_rfpath_chain0_aux = 1,
	qcsapi_rfpath_chain1_main = 2,
	qcsapi_rfpath_chain1_aux = 3,
	qcsapi_nosuch_rfpath
} qcsapi_rfpath;

typedef enum
{
	qcsapi_select_SSB = 0,
	qcsapi_select_DSB = 1,
} qcsapi_SSB_setting;

typedef enum
{
	qcsapi_disable_tx_power_cal = 0,
	qcsapi_enable_tx_power_cal = 1,
} qcsapi_tx_power_cal;

/*
 * Distinguish from RF test operation (below).
 * This enum tracks what the Q device is currently doing.
 */

typedef enum
{
	e_qcsapi_rftest_packet_test = 1,
	e_qcsapi_rftest_send_cw,
	e_qcsapi_rftest_calibraton,
	e_qcsapi_rftest_no_test = 0
} qcsapi_rftest_test_type;

/*
 * Field names that start with "rftest_" are specific for the RF test.
 */

typedef struct qcsapi_rftest_params
{
	unsigned int		rftest_magic;
	qcsapi_rftest_test_type	current_test;
	qcsapi_unsigned_int	rftest_chan;
	qcsapi_unsigned_int	packet_size;
	qcsapi_unsigned_int	packet_count;
	qcsapi_packet_type	packet_type;
	qcsapi_cw_pattern	cw_pattern;
	qcsapi_rfpath		rfpath;
	qcsapi_SSB_setting	SSB_setting;
	qcsapi_tx_power_cal	tx_power_cal;
	qcsapi_bw		rftest_bandwidth;
	qcsapi_s8		tx_power;
	qcsapi_u8		antenna_mask;
	qcsapi_u8		harmonics;
	qcsapi_u8		chip_index;
	qcsapi_mcs_rate		rftest_mcs_rate;
} qcsapi_rftest_params;

typedef enum
{
	qcsapi_rftest_rx_packets = 1,
	qcsapi_rftest_tx_packets,
	qcsapi_nosuch_rftest_counter = 0
} qcsapi_rftest_counter_type;

typedef struct qcsapi_rftest_packet_counters
{
	qcsapi_unsigned_int	tx_packets;
	qcsapi_unsigned_int	rx_packets;
} qcsapi_rftest_packet_counters;

typedef qcsapi_rftest_packet_counters	qcsapi_rftest_packet_report[ 2 ];

typedef enum
{
	qcsapi_rftest_complete_calibration = 1,
	qcsapi_nosuch_rftest_calibration = 0
} qcsapi_rftest_calibration_test;

typedef enum
{
	e_qcsapi_rftest_dump_params = 1,
	e_qcsapi_rftest_dump_counters,
	e_qcsapi_rftest_setup,
	e_qcsapi_rftest_start_packet_receive,
	e_qcsapi_rftest_start_packet_xmit,
	e_qcsapi_rftest_stop_packet_test,
	e_qcsapi_rftest_get_pkt_counters,
	e_qcsapi_rftest_start_send_cw,
	e_qcsapi_rftest_stop_send_cw,
	e_qcsapi_rftest_stop_RF_tests,
	e_qcsapi_rftest_calibrate,
	e_qcsapi_rftest_calibrate_blocking,
	e_qcsapi_rftest_one_channel_calibrate,
	e_qcsapi_rftest_monitor_calibration,
	e_qcsapi_nosuch_rftest = 0
} qcsapi_rftest_operation;


static inline int _stdcall
qcsapi_rftest_set_chan( qcsapi_rftest_params *p_rftest_params, qcsapi_unsigned_int new_chan )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->rftest_chan = new_chan;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_packet_size( qcsapi_rftest_params *p_rftest_params, qcsapi_unsigned_int new_packet_size )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->packet_size = new_packet_size;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_packet_count( qcsapi_rftest_params *p_rftest_params, qcsapi_unsigned_int new_packet_count )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->packet_count = new_packet_count;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_packet_type( qcsapi_rftest_params *p_rftest_params, qcsapi_packet_type new_packet_type )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->packet_type = new_packet_type;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_cw_pattern( qcsapi_rftest_params *p_rftest_params, qcsapi_cw_pattern new_cw_pattern )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->cw_pattern = new_cw_pattern;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_rfpath( qcsapi_rftest_params *p_rftest_params, qcsapi_rfpath new_rfpath )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->rfpath = new_rfpath;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_SSB_setting( qcsapi_rftest_params *p_rftest_params, qcsapi_SSB_setting new_SSB_setting )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->SSB_setting = new_SSB_setting;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_tx_power_cal( qcsapi_rftest_params *p_rftest_params, qcsapi_tx_power_cal new_tx_power_cal )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->tx_power_cal = new_tx_power_cal;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_bandwidth( qcsapi_rftest_params *p_rftest_params, qcsapi_bw new_bandwidth )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->rftest_bandwidth = new_bandwidth;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_tx_power( qcsapi_rftest_params *p_rftest_params, qcsapi_s8 new_tx_power )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->tx_power = new_tx_power;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_antenna_mask( qcsapi_rftest_params *p_rftest_params, qcsapi_u8 new_antenna_mask )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->antenna_mask = new_antenna_mask | SPECIAL_ANTENNA_MASK;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_harmonics( qcsapi_rftest_params *p_rftest_params, qcsapi_u8 new_harmonics )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->harmonics = new_harmonics;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_chip_index( qcsapi_rftest_params *p_rftest_params, qcsapi_u8 new_chip_index )
{
	int	retval = 0;

	if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		p_rftest_params->chip_index = new_chip_index;
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_mcs_rate( qcsapi_rftest_params *p_rftest_params, qcsapi_mcs_rate new_mcs_rate )
{
	int	retval = 0;

	if (p_rftest_params == NULL || new_mcs_rate == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		strncpy( p_rftest_params->rftest_mcs_rate, new_mcs_rate, sizeof( qcsapi_mcs_rate ) );
	}

	return( retval );
}

static inline int _stdcall
qcsapi_rftest_set_integer_mcs_rate( qcsapi_rftest_params *p_rftest_params, unsigned int new_mcs_rate )
{
	int	retval = 0;

	if (new_mcs_rate > 63)
	{
		retval = -EINVAL;
	}
	else if (p_rftest_params == NULL)
	{
		retval = -EFAULT;
	}
	else
	{
		sprintf( p_rftest_params->rftest_mcs_rate, "MCS%u", new_mcs_rate );
	}

	return( retval );
}

/*
 * These symbols need to be exported; they are the way individual parameters get updated.
 */

extern int _stdcall	qcsapi_rftest_update_one_param( qcsapi_rftest_params *p_rftest_params, char *param_name_val );
extern int _stdcall	qcsapi_rftest_update_params( qcsapi_rftest_params *p_rftest_params, int argc, char *argv[] );

/*
 * These 2 symbols probably do not need to be exported.  My application called both of them; thus the _stdcall construct
 */

extern int _stdcall	qcsapi_rftest_dump_counters( const qcsapi_rftest_packet_report packet_report, FILE *fh, const qcsapi_dump_format the_format );   
extern void _stdcall	list_rftest_params( FILE *fh );

/*
 * None of these symbols need to be exported.
 */

extern int	qcsapi_rftest_init_params( qcsapi_rftest_params *p_rftest_params );
extern int	qcsapi_rftest_dump_params( const qcsapi_rftest_params *p_rftest_params, FILE *fh, const qcsapi_dump_format the_format );

extern int	qcsapi_rftest_setup( qcsapi_rftest_params *p_rftest_params );
extern int	qcsapi_rftest_start_packet_receive( qcsapi_rftest_params *p_rftest_params );
extern int	qcsapi_rftest_start_packet_xmit( qcsapi_rftest_params *p_rftest_params );
extern int	qcsapi_rftest_stop_packet_test( qcsapi_rftest_params *p_rftest_params );
extern int	qcsapi_rftest_get_pkt_counters( qcsapi_rftest_params *p_rftest_params, qcsapi_rftest_packet_report packet_report );
extern int	qcsapi_rftest_start_send_cw( qcsapi_rftest_params *p_rftest_params );
extern int	qcsapi_rftest_stop_send_cw( qcsapi_rftest_params *p_rftest_params );
extern int	qcsapi_rftest_stop_RF_tests( qcsapi_rftest_params *p_rftest_params );

extern int	qcsapi_rftest_start_calibration( int wait_flag );
extern int	qcsapi_rftest_calibrate_one_channel(
			qcsapi_rftest_params *p_rftest_params,
			qcsapi_rftest_calibration_test rf_calibration_test,
			int wait_flag
		);
extern int	qcsapi_rftest_monitor_calibration( qcsapi_rftest_params *p_rftest_params, int *p_test_complete );
#ifdef __cplusplus
}
#endif

#endif /* _QCSAPI_RFTEST_H */
