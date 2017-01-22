/****************************************************************************
*
* Copyright (c) 2015  Quantenna Communications, Inc.
*
* Permission to use, copy, modify, and/or distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
* SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
* RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
* NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
* USE OR PERFORMANCE OF THIS SOFTWARE.
*
*****************************************************************************/

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "qtn_cmd_parser.h"
#include "wfa_types.h"
#include "wfa_tlv.h"

#define QTN_REQ_MAXBUF_SIZE	512
#define QTN_RESP_MINADD_SIZE	12
#define QTN_RESP_MINBUF_SIZE	32

int qtn_ca_encode_cmd(int cmd_tag, char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	char req_buf[QTN_REQ_MAXBUF_SIZE];
	int req_len;

	if (!tlv_buf)
		return WFA_FAILURE;

	req_len = qtn_parse_params_encode_request(cmd_str, req_buf, sizeof(req_buf));

	if (req_len < 0)
		return WFA_FAILURE;

	/* TODO: do we need to check tlv_buf overrun? */

	wfaEncodeTLV((unsigned short)cmd_tag, (unsigned short)req_len, (unsigned char *)req_buf,
		tlv_buf);

	*tlv_len = WFA_TLV_HDR_LEN + req_len;

	return WFA_SUCCESS;
}

int qtn_ca_make_response(unsigned char *tlv_buf, char *outbuf_ptr, int outbuf_size)
{
	wfaTLV tlv_head;
	char *resp_buf = NULL;
	int resp_len = 0;

	if (!tlv_buf || !outbuf_ptr || (outbuf_size < QTN_RESP_MINBUF_SIZE))
		return WFA_ERROR;

	memcpy(&tlv_head, tlv_buf, sizeof(tlv_head));

	if (tlv_head.len > 0) {
		resp_buf = (char *)tlv_buf + sizeof(tlv_head);
		resp_len = qtn_validate_response_get_length(resp_buf, tlv_head.len);
	}

	if ((resp_len > 0) && ((resp_len + QTN_RESP_MINADD_SIZE) < outbuf_size)) {
		strcpy(outbuf_ptr, "status,");
		strncat(outbuf_ptr, resp_buf, resp_len);
		strcat(outbuf_ptr, "\r\n");
	} else {
		sprintf(outbuf_ptr, "status,ERROR,errorCode,%d\r\n", -EPERM);
	}

	return WFA_SUCCESS;
}
