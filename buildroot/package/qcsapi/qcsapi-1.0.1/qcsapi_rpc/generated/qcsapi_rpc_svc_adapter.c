
/*
*  ########## DO NOT EDIT ###########

Automatically generated on Wed, 11 May 2016 19:26:05 -0700

*
* Adapter from qcsapi.h functions
* to RPC server functions.
*/

#include <inttypes.h>
#include <qcsapi.h>
#include "qcsapi_rpc.h"

static const int debug = 0;

#define ARG_ALLOC_SIZE	8192

static void *arg_alloc(void)
{
	void *mem = malloc(ARG_ALLOC_SIZE);
	if (mem) {
		memset(mem, 0, ARG_ALLOC_SIZE);
	}
	return mem;
}

static void arg_free(void *arg)
{
	free(arg);
}

static void* __rpc_prepare_data(void *input_ptr, int length)
{
	void *data = NULL;

	if (!input_ptr)
		return NULL;

	data = malloc(length);
	if (data)
		memcpy(data, input_ptr, length);

	return data;
}

bool_t qcsapi_bootcfg_get_parameter_remote_1_svc(qcsapi_bootcfg_get_parameter_rpcdata *__req, qcsapi_bootcfg_get_parameter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * param_name = (__req->param_name == NULL) ? NULL : __req->param_name->data;
	char * param_value = (__req->param_value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_bootcfg_get_parameter(param_name, param_value, __req->max_param_len);

	if (param_value) {
		__resp->param_value = malloc(sizeof(*__resp->param_value));
		__resp->param_value->data = param_value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_bootcfg_update_parameter_remote_1_svc(qcsapi_bootcfg_update_parameter_rpcdata *__req, qcsapi_bootcfg_update_parameter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * param_name = (__req->param_name == NULL) ? NULL : __req->param_name->data;
	char * param_value = (__req->param_value == NULL) ? NULL : __req->param_value->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_bootcfg_update_parameter(param_name, param_value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_bootcfg_commit_remote_1_svc(qcsapi_bootcfg_commit_rpcdata *__req, qcsapi_bootcfg_commit_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_bootcfg_commit();

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_telnet_enable_remote_1_svc(qcsapi_telnet_enable_rpcdata *__req, qcsapi_telnet_enable_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_telnet_enable(__req->onoff);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_service_name_enum_remote_1_svc(qcsapi_get_service_name_enum_rpcdata *__req, qcsapi_get_service_name_enum_rpcdata *__resp, struct svc_req *rqstp)
{
	char * lookup_service = (__req->lookup_service == NULL) ? NULL : __req->lookup_service->data;
	qcsapi_service_name * serv_name=(qcsapi_service_name *)__req->serv_name;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_service_name_enum(lookup_service, serv_name);

	__resp->serv_name = __rpc_prepare_data(__req->serv_name, sizeof(*__resp->serv_name));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_service_action_enum_remote_1_svc(qcsapi_get_service_action_enum_rpcdata *__req, qcsapi_get_service_action_enum_rpcdata *__resp, struct svc_req *rqstp)
{
	char * lookup_action = (__req->lookup_action == NULL) ? NULL : __req->lookup_action->data;
	qcsapi_service_action * serv_action=(qcsapi_service_action *)__req->serv_action;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_service_action_enum(lookup_action, serv_action);

	__resp->serv_action = __rpc_prepare_data(__req->serv_action, sizeof(*__resp->serv_action));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_service_control_remote_1_svc(qcsapi_service_control_rpcdata *__req, qcsapi_service_control_rpcdata *__resp, struct svc_req *rqstp)
{
	qcsapi_service_name service=(qcsapi_service_name)__req->service;
	qcsapi_service_action action=(qcsapi_service_action)__req->action;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_service_control(service, action);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wfa_cert_mode_enable_remote_1_svc(qcsapi_wfa_cert_mode_enable_rpcdata *__req, qcsapi_wfa_cert_mode_enable_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wfa_cert_mode_enable(__req->enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scs_cce_channels_remote_1_svc(qcsapi_wifi_get_scs_cce_channels_rpcdata *__req, qcsapi_wifi_get_scs_cce_channels_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scs_cce_channels(ifname, __req->p_prev_channel, __req->p_cur_channel);

	__resp->p_prev_channel = __rpc_prepare_data(__req->p_prev_channel, sizeof(*__resp->p_prev_channel));
	__resp->p_cur_channel = __rpc_prepare_data(__req->p_cur_channel, sizeof(*__resp->p_cur_channel));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_scs_enable_remote_1_svc(qcsapi_wifi_scs_enable_rpcdata *__req, qcsapi_wifi_scs_enable_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_scs_enable(ifname, __req->enable_val);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_scs_switch_channel_remote_1_svc(qcsapi_wifi_scs_switch_channel_rpcdata *__req, qcsapi_wifi_scs_switch_channel_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_scs_switch_channel(ifname, __req->pick_flags);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scs_verbose_remote_1_svc(qcsapi_wifi_set_scs_verbose_rpcdata *__req, qcsapi_wifi_set_scs_verbose_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scs_verbose(ifname, __req->enable_val);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scs_status_remote_1_svc(qcsapi_wifi_get_scs_status_rpcdata *__req, qcsapi_wifi_get_scs_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scs_status(ifname, __req->p_scs_status);

	__resp->p_scs_status = __rpc_prepare_data(__req->p_scs_status, sizeof(*__resp->p_scs_status));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scs_smpl_enable_remote_1_svc(qcsapi_wifi_set_scs_smpl_enable_rpcdata *__req, qcsapi_wifi_set_scs_smpl_enable_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scs_smpl_enable(ifname, __req->enable_val);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scs_smpl_dwell_time_remote_1_svc(qcsapi_wifi_set_scs_smpl_dwell_time_rpcdata *__req, qcsapi_wifi_set_scs_smpl_dwell_time_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scs_smpl_dwell_time(ifname, __req->scs_sample_time);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scs_sample_intv_remote_1_svc(qcsapi_wifi_set_scs_sample_intv_rpcdata *__req, qcsapi_wifi_set_scs_sample_intv_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scs_sample_intv(ifname, __req->scs_sample_intv);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scs_intf_detect_intv_remote_1_svc(qcsapi_wifi_set_scs_intf_detect_intv_rpcdata *__req, qcsapi_wifi_set_scs_intf_detect_intv_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scs_intf_detect_intv(ifname, __req->scs_intf_detect_intv);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scs_thrshld_remote_1_svc(qcsapi_wifi_set_scs_thrshld_rpcdata *__req, qcsapi_wifi_set_scs_thrshld_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * scs_param_name = (__req->scs_param_name == NULL) ? NULL : __req->scs_param_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scs_thrshld(ifname, scs_param_name, __req->scs_threshold);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scs_report_only_remote_1_svc(qcsapi_wifi_set_scs_report_only_rpcdata *__req, qcsapi_wifi_set_scs_report_only_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scs_report_only(ifname, __req->scs_report_only);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scs_stat_report_remote_1_svc(qcsapi_wifi_get_scs_stat_report_rpcdata *__req, qcsapi_wifi_get_scs_stat_report_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scs_stat_report(ifname, (struct qcsapi_scs_ranking_rpt *)__req->scs_rpt);

	__resp->scs_rpt = __rpc_prepare_data(__req->scs_rpt, sizeof(*__resp->scs_rpt));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scs_score_report_remote_1_svc(qcsapi_wifi_get_scs_score_report_rpcdata *__req, qcsapi_wifi_get_scs_score_report_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scs_score_report(ifname, (struct qcsapi_scs_score_rpt *)__req->scs_rpt);

	__resp->scs_rpt = __rpc_prepare_data(__req->scs_rpt, sizeof(*__resp->scs_rpt));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scs_currchan_report_remote_1_svc(qcsapi_wifi_get_scs_currchan_report_rpcdata *__req, qcsapi_wifi_get_scs_currchan_report_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scs_currchan_report(ifname, (struct qcsapi_scs_currchan_rpt *)__req->scs_currchan_rpt);

	__resp->scs_currchan_rpt = __rpc_prepare_data(__req->scs_currchan_rpt, sizeof(*__resp->scs_currchan_rpt));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scs_stats_remote_1_svc(qcsapi_wifi_set_scs_stats_rpcdata *__req, qcsapi_wifi_set_scs_stats_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scs_stats(ifname, __req->start);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_autochan_report_remote_1_svc(qcsapi_wifi_get_autochan_report_rpcdata *__req, qcsapi_wifi_get_autochan_report_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_autochan_report(ifname, (struct qcsapi_autochan_rpt *)__req->autochan_rpt);

	__resp->autochan_rpt = __rpc_prepare_data(__req->autochan_rpt, sizeof(*__resp->autochan_rpt));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scs_cca_intf_smth_fctr_remote_1_svc(qcsapi_wifi_set_scs_cca_intf_smth_fctr_rpcdata *__req, qcsapi_wifi_set_scs_cca_intf_smth_fctr_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scs_cca_intf_smth_fctr(ifname, __req->smth_fctr_noxp, __req->smth_fctr_xped);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scs_chan_mtrc_mrgn_remote_1_svc(qcsapi_wifi_set_scs_chan_mtrc_mrgn_rpcdata *__req, qcsapi_wifi_set_scs_chan_mtrc_mrgn_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scs_chan_mtrc_mrgn(ifname, __req->chan_mtrc_mrgn);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scs_cca_intf_remote_1_svc(qcsapi_wifi_get_scs_cca_intf_rpcdata *__req, qcsapi_wifi_get_scs_cca_intf_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scs_cca_intf(ifname, __req->the_channel, __req->p_cca_intf);

	__resp->p_cca_intf = __rpc_prepare_data(__req->p_cca_intf, sizeof(*__resp->p_cca_intf));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scs_param_report_remote_1_svc(qcsapi_wifi_get_scs_param_report_rpcdata *__req, qcsapi_wifi_get_scs_param_report_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scs_param_report(ifname, (struct qcsapi_scs_param_rpt *)__req->p_scs_param_rpt, __req->param_num);

	__resp->p_scs_param_rpt = __rpc_prepare_data(__req->p_scs_param_rpt, sizeof(*__resp->p_scs_param_rpt));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scs_dfs_reentry_request_remote_1_svc(qcsapi_wifi_get_scs_dfs_reentry_request_rpcdata *__req, qcsapi_wifi_get_scs_dfs_reentry_request_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scs_dfs_reentry_request(ifname, __req->p_scs_dfs_reentry_request);

	__resp->p_scs_dfs_reentry_request = __rpc_prepare_data(__req->p_scs_dfs_reentry_request, sizeof(*__resp->p_scs_dfs_reentry_request));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_start_ocac_remote_1_svc(qcsapi_wifi_start_ocac_rpcdata *__req, qcsapi_wifi_start_ocac_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_start_ocac(ifname, __req->channel);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_stop_ocac_remote_1_svc(qcsapi_wifi_stop_ocac_rpcdata *__req, qcsapi_wifi_stop_ocac_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_stop_ocac(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_ocac_status_remote_1_svc(qcsapi_wifi_get_ocac_status_rpcdata *__req, qcsapi_wifi_get_ocac_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_ocac_status(ifname, __req->status);

	__resp->status = __rpc_prepare_data(__req->status, sizeof(*__resp->status));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_ocac_dwell_time_remote_1_svc(qcsapi_wifi_set_ocac_dwell_time_rpcdata *__req, qcsapi_wifi_set_ocac_dwell_time_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_ocac_dwell_time(ifname, __req->dwell_time);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_ocac_duration_remote_1_svc(qcsapi_wifi_set_ocac_duration_rpcdata *__req, qcsapi_wifi_set_ocac_duration_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_ocac_duration(ifname, __req->duration);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_ocac_cac_time_remote_1_svc(qcsapi_wifi_set_ocac_cac_time_rpcdata *__req, qcsapi_wifi_set_ocac_cac_time_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_ocac_cac_time(ifname, __req->cac_time);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_ocac_report_only_remote_1_svc(qcsapi_wifi_set_ocac_report_only_rpcdata *__req, qcsapi_wifi_set_ocac_report_only_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_ocac_report_only(ifname, __req->enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_ocac_thrshld_remote_1_svc(qcsapi_wifi_set_ocac_thrshld_rpcdata *__req, qcsapi_wifi_set_ocac_thrshld_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * param_name = (__req->param_name == NULL) ? NULL : __req->param_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_ocac_thrshld(ifname, param_name, __req->threshold);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_start_dfs_s_radio_remote_1_svc(qcsapi_wifi_start_dfs_s_radio_rpcdata *__req, qcsapi_wifi_start_dfs_s_radio_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_start_dfs_s_radio(ifname, __req->channel);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_stop_dfs_s_radio_remote_1_svc(qcsapi_wifi_stop_dfs_s_radio_rpcdata *__req, qcsapi_wifi_stop_dfs_s_radio_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_stop_dfs_s_radio(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_dfs_s_radio_status_remote_1_svc(qcsapi_wifi_get_dfs_s_radio_status_rpcdata *__req, qcsapi_wifi_get_dfs_s_radio_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_dfs_s_radio_status(ifname, __req->status);

	__resp->status = __rpc_prepare_data(__req->status, sizeof(*__resp->status));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_dfs_s_radio_availability_remote_1_svc(qcsapi_wifi_get_dfs_s_radio_availability_rpcdata *__req, qcsapi_wifi_get_dfs_s_radio_availability_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_dfs_s_radio_availability(ifname, __req->available);

	__resp->available = __rpc_prepare_data(__req->available, sizeof(*__resp->available));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dfs_s_radio_dwell_time_remote_1_svc(qcsapi_wifi_set_dfs_s_radio_dwell_time_rpcdata *__req, qcsapi_wifi_set_dfs_s_radio_dwell_time_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_dfs_s_radio_dwell_time(ifname, __req->dwell_time);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dfs_s_radio_duration_remote_1_svc(qcsapi_wifi_set_dfs_s_radio_duration_rpcdata *__req, qcsapi_wifi_set_dfs_s_radio_duration_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_dfs_s_radio_duration(ifname, __req->duration);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dfs_s_radio_wea_duration_remote_1_svc(qcsapi_wifi_set_dfs_s_radio_wea_duration_rpcdata *__req, qcsapi_wifi_set_dfs_s_radio_wea_duration_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_dfs_s_radio_wea_duration(ifname, __req->duration);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dfs_s_radio_cac_time_remote_1_svc(qcsapi_wifi_set_dfs_s_radio_cac_time_rpcdata *__req, qcsapi_wifi_set_dfs_s_radio_cac_time_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_dfs_s_radio_cac_time(ifname, __req->cac_time);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dfs_s_radio_wea_cac_time_remote_1_svc(qcsapi_wifi_set_dfs_s_radio_wea_cac_time_rpcdata *__req, qcsapi_wifi_set_dfs_s_radio_wea_cac_time_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_dfs_s_radio_wea_cac_time(ifname, __req->cac_time);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dfs_s_radio_wea_dwell_time_remote_1_svc(qcsapi_wifi_set_dfs_s_radio_wea_dwell_time_rpcdata *__req, qcsapi_wifi_set_dfs_s_radio_wea_dwell_time_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_dfs_s_radio_wea_dwell_time(ifname, __req->dwell_time);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dfs_s_radio_report_only_remote_1_svc(qcsapi_wifi_set_dfs_s_radio_report_only_rpcdata *__req, qcsapi_wifi_set_dfs_s_radio_report_only_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_dfs_s_radio_report_only(ifname, __req->enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dfs_s_radio_thrshld_remote_1_svc(qcsapi_wifi_set_dfs_s_radio_thrshld_rpcdata *__req, qcsapi_wifi_set_dfs_s_radio_thrshld_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * param_name = (__req->param_name == NULL) ? NULL : __req->param_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_dfs_s_radio_thrshld(ifname, param_name, __req->threshold);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scs_leavedfs_chan_mtrc_mrgn_remote_1_svc(qcsapi_wifi_set_scs_leavedfs_chan_mtrc_mrgn_rpcdata *__req, qcsapi_wifi_set_scs_leavedfs_chan_mtrc_mrgn_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scs_leavedfs_chan_mtrc_mrgn(ifname, __req->leavedfs_chan_mtrc_mrgn);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_init_remote_1_svc(qcsapi_init_rpcdata *__req, qcsapi_init_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_init();

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_console_disconnect_remote_1_svc(qcsapi_console_disconnect_rpcdata *__req, qcsapi_console_disconnect_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_console_disconnect();

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_startprod_remote_1_svc(qcsapi_wifi_startprod_rpcdata *__req, qcsapi_wifi_startprod_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_startprod();

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_is_startprod_done_remote_1_svc(qcsapi_is_startprod_done_rpcdata *__req, qcsapi_is_startprod_done_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_is_startprod_done(__req->p_status);

	__resp->p_status = __rpc_prepare_data(__req->p_status, sizeof(*__resp->p_status));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_system_get_time_since_start_remote_1_svc(qcsapi_system_get_time_since_start_rpcdata *__req, qcsapi_system_get_time_since_start_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_system_get_time_since_start(__req->p_elapsed_time);

	__resp->p_elapsed_time = __rpc_prepare_data(__req->p_elapsed_time, sizeof(*__resp->p_elapsed_time));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_system_status_remote_1_svc(qcsapi_get_system_status_rpcdata *__req, qcsapi_get_system_status_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_system_status(__req->p_status);

	__resp->p_status = __rpc_prepare_data(__req->p_status, sizeof(*__resp->p_status));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_random_seed_remote_1_svc(qcsapi_get_random_seed_rpcdata *__req, qcsapi_get_random_seed_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_random_seed((struct qcsapi_data_512bytes *)__req->random_buf);

	__resp->random_buf = __rpc_prepare_data(__req->random_buf, sizeof(*__resp->random_buf));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_random_seed_remote_1_svc(qcsapi_set_random_seed_rpcdata *__req, qcsapi_set_random_seed_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_random_seed((const struct qcsapi_data_512bytes *)__req->random_buf, __req->entropy);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_carrier_id_remote_1_svc(qcsapi_get_carrier_id_rpcdata *__req, qcsapi_get_carrier_id_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_carrier_id(__req->p_carrier_id);

	__resp->p_carrier_id = __rpc_prepare_data(__req->p_carrier_id, sizeof(*__resp->p_carrier_id));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_carrier_id_remote_1_svc(qcsapi_set_carrier_id_rpcdata *__req, qcsapi_set_carrier_id_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_carrier_id(__req->carrier_id, __req->update_uboot);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_spinor_jedecid_remote_1_svc(qcsapi_wifi_get_spinor_jedecid_rpcdata *__req, qcsapi_wifi_get_spinor_jedecid_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_spinor_jedecid(ifname, __req->p_jedecid);

	__resp->p_jedecid = __rpc_prepare_data(__req->p_jedecid, sizeof(*__resp->p_jedecid));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_bb_param_remote_1_svc(qcsapi_wifi_get_bb_param_rpcdata *__req, qcsapi_wifi_get_bb_param_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_bb_param(ifname, __req->p_jedecid);

	__resp->p_jedecid = __rpc_prepare_data(__req->p_jedecid, sizeof(*__resp->p_jedecid));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_bb_param_remote_1_svc(qcsapi_wifi_set_bb_param_rpcdata *__req, qcsapi_wifi_set_bb_param_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_bb_param(ifname, __req->p_jedecid);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_optim_stats_remote_1_svc(qcsapi_wifi_set_optim_stats_rpcdata *__req, qcsapi_wifi_set_optim_stats_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_optim_stats(ifname, __req->p_jedecid);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_sys_time_remote_1_svc(qcsapi_wifi_set_sys_time_rpcdata *__req, qcsapi_wifi_set_sys_time_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_sys_time(__req->timestamp);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_sys_time_remote_1_svc(qcsapi_wifi_get_sys_time_rpcdata *__req, qcsapi_wifi_get_sys_time_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_sys_time(__req->timestamp);

	__resp->timestamp = __rpc_prepare_data(__req->timestamp, sizeof(*__resp->timestamp));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_soc_mac_addr_remote_1_svc(qcsapi_set_soc_mac_addr_rpcdata *__req, qcsapi_set_soc_mac_addr_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * soc_mac_addr = (__req->soc_mac_addr == NULL) ? NULL : __req->soc_mac_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_soc_mac_addr(ifname, soc_mac_addr);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_custom_value_remote_1_svc(qcsapi_get_custom_value_rpcdata *__req, qcsapi_get_custom_value_rpcdata *__resp, struct svc_req *rqstp)
{
	char * custom_key = (__req->custom_key == NULL) ? NULL : __req->custom_key->data;
	char * custom_value = (__req->custom_value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_custom_value(custom_key, custom_value);

	if (custom_value) {
		__resp->custom_value = malloc(sizeof(*__resp->custom_value));
		__resp->custom_value->data = custom_value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_custom_value_remote_1_svc(qcsapi_set_custom_value_rpcdata *__req, qcsapi_set_custom_value_rpcdata *__resp, struct svc_req *rqstp)
{
	char * custom_key = (__req->custom_key == NULL) ? NULL : __req->custom_key->data;
	char * custom_value = (__req->custom_value == NULL) ? NULL : __req->custom_value->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_custom_value(custom_key, custom_value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_vap_default_state_remote_1_svc(qcsapi_wifi_get_vap_default_state_rpcdata *__req, qcsapi_wifi_get_vap_default_state_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_vap_default_state(__req->enable);

	__resp->enable = __rpc_prepare_data(__req->enable, sizeof(*__resp->enable));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_vap_default_state_remote_1_svc(qcsapi_wifi_set_vap_default_state_rpcdata *__req, qcsapi_wifi_set_vap_default_state_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_vap_default_state(__req->enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_vap_state_remote_1_svc(qcsapi_wifi_get_vap_state_rpcdata *__req, qcsapi_wifi_get_vap_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_vap_state(ifname, __req->enable);

	__resp->enable = __rpc_prepare_data(__req->enable, sizeof(*__resp->enable));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_vap_state_remote_1_svc(qcsapi_wifi_set_vap_state_rpcdata *__req, qcsapi_wifi_set_vap_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_vap_state(ifname, __req->enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_ep_status_remote_1_svc(qcsapi_get_ep_status_rpcdata *__req, qcsapi_get_ep_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_ep_status(ifname, __req->ep_status);

	__resp->ep_status = __rpc_prepare_data(__req->ep_status, sizeof(*__resp->ep_status));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_config_get_parameter_remote_1_svc(qcsapi_config_get_parameter_rpcdata *__req, qcsapi_config_get_parameter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * param_name = (__req->param_name == NULL) ? NULL : __req->param_name->data;
	char * param_value = (__req->param_value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_config_get_parameter(ifname, param_name, param_value, __req->max_param_len);

	if (param_value) {
		__resp->param_value = malloc(sizeof(*__resp->param_value));
		__resp->param_value->data = param_value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_config_update_parameter_remote_1_svc(qcsapi_config_update_parameter_rpcdata *__req, qcsapi_config_update_parameter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * param_name = (__req->param_name == NULL) ? NULL : __req->param_name->data;
	char * param_value = (__req->param_value == NULL) ? NULL : __req->param_value->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_config_update_parameter(ifname, param_name, param_value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_config_get_ssid_parameter_remote_1_svc(qcsapi_config_get_ssid_parameter_rpcdata *__req, qcsapi_config_get_ssid_parameter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * param_name = (__req->param_name == NULL) ? NULL : __req->param_name->data;
	char * param_value = (__req->param_value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_config_get_ssid_parameter(ifname, param_name, param_value, __req->max_param_len);

	if (param_value) {
		__resp->param_value = malloc(sizeof(*__resp->param_value));
		__resp->param_value->data = param_value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_config_update_ssid_parameter_remote_1_svc(qcsapi_config_update_ssid_parameter_rpcdata *__req, qcsapi_config_update_ssid_parameter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * param_name = (__req->param_name == NULL) ? NULL : __req->param_name->data;
	char * param_value = (__req->param_value == NULL) ? NULL : __req->param_value->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_config_update_ssid_parameter(ifname, param_name, param_value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_file_path_get_config_remote_1_svc(qcsapi_file_path_get_config_rpcdata *__req, qcsapi_file_path_get_config_rpcdata *__resp, struct svc_req *rqstp)
{
	const qcsapi_file_path_config e_file_path=(const qcsapi_file_path_config)__req->e_file_path;
	char * file_path = (__req->file_path == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_file_path_get_config(e_file_path, file_path, __req->path_size);

	if (file_path) {
		__resp->file_path = malloc(sizeof(*__resp->file_path));
		__resp->file_path->data = file_path;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_file_path_set_config_remote_1_svc(qcsapi_file_path_set_config_rpcdata *__req, qcsapi_file_path_set_config_rpcdata *__resp, struct svc_req *rqstp)
{
	const qcsapi_file_path_config e_file_path=(const qcsapi_file_path_config)__req->e_file_path;
	char * new_path = (__req->new_path == NULL) ? NULL : __req->new_path->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_file_path_set_config(e_file_path, new_path);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_restore_default_config_remote_1_svc(qcsapi_restore_default_config_rpcdata *__req, qcsapi_restore_default_config_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_restore_default_config(__req->flag);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_store_ipaddr_remote_1_svc(qcsapi_store_ipaddr_rpcdata *__req, qcsapi_store_ipaddr_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_store_ipaddr(__req->ipaddr, __req->netmask);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_interface_enable_remote_1_svc(qcsapi_interface_enable_rpcdata *__req, qcsapi_interface_enable_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_interface_enable(ifname, __req->enable_flag);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_interface_get_status_remote_1_svc(qcsapi_interface_get_status_rpcdata *__req, qcsapi_interface_get_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * interface_status = (__req->interface_status == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_interface_get_status(ifname, interface_status);

	if (interface_status) {
		__resp->interface_status = malloc(sizeof(*__resp->interface_status));
		__resp->interface_status->data = interface_status;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_interface_set_ip4_remote_1_svc(qcsapi_interface_set_ip4_rpcdata *__req, qcsapi_interface_set_ip4_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * if_param = (__req->if_param == NULL) ? NULL : __req->if_param->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_interface_set_ip4(ifname, if_param, __req->if_param_val);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_interface_get_ip4_remote_1_svc(qcsapi_interface_get_ip4_rpcdata *__req, qcsapi_interface_get_ip4_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * if_param = (__req->if_param == NULL) ? NULL : __req->if_param->data;
	char * if_param_val = (__req->if_param_val == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_interface_get_ip4(ifname, if_param, if_param_val);

	if (if_param_val) {
		__resp->if_param_val = malloc(sizeof(*__resp->if_param_val));
		__resp->if_param_val->data = if_param_val;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_interface_get_counter_remote_1_svc(qcsapi_interface_get_counter_rpcdata *__req, qcsapi_interface_get_counter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_counter_type qcsapi_counter=(qcsapi_counter_type)__req->qcsapi_counter;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_interface_get_counter(ifname, qcsapi_counter, __req->p_counter_value);

	__resp->p_counter_value = __rpc_prepare_data(__req->p_counter_value, sizeof(*__resp->p_counter_value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_interface_get_counter64_remote_1_svc(qcsapi_interface_get_counter64_rpcdata *__req, qcsapi_interface_get_counter64_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_counter_type qcsapi_counter=(qcsapi_counter_type)__req->qcsapi_counter;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_interface_get_counter64(ifname, qcsapi_counter, __req->p_counter_value);

	__resp->p_counter_value = __rpc_prepare_data(__req->p_counter_value, sizeof(*__resp->p_counter_value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_interface_get_mac_addr_remote_1_svc(qcsapi_interface_get_mac_addr_rpcdata *__req, qcsapi_interface_get_mac_addr_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * current_mac_addr = (__req->current_mac_addr == NULL) ? NULL : __req->current_mac_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_interface_get_mac_addr(ifname, current_mac_addr);

	__resp->current_mac_addr = __rpc_prepare_data(__req->current_mac_addr, sizeof(*__resp->current_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_interface_set_mac_addr_remote_1_svc(qcsapi_interface_set_mac_addr_rpcdata *__req, qcsapi_interface_set_mac_addr_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * interface_mac_addr = (__req->interface_mac_addr == NULL) ? NULL : __req->interface_mac_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_interface_set_mac_addr(ifname, interface_mac_addr);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_pm_get_counter_remote_1_svc(qcsapi_pm_get_counter_rpcdata *__req, qcsapi_pm_get_counter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_counter_type qcsapi_counter=(qcsapi_counter_type)__req->qcsapi_counter;
	char * pm_interval = (__req->pm_interval == NULL) ? NULL : __req->pm_interval->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_pm_get_counter(ifname, qcsapi_counter, pm_interval, __req->p_counter_value);

	__resp->p_counter_value = __rpc_prepare_data(__req->p_counter_value, sizeof(*__resp->p_counter_value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_pm_get_elapsed_time_remote_1_svc(qcsapi_pm_get_elapsed_time_rpcdata *__req, qcsapi_pm_get_elapsed_time_rpcdata *__resp, struct svc_req *rqstp)
{
	char * pm_interval = (__req->pm_interval == NULL) ? NULL : __req->pm_interval->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_pm_get_elapsed_time(pm_interval, __req->p_elapsed_time);

	__resp->p_elapsed_time = __rpc_prepare_data(__req->p_elapsed_time, sizeof(*__resp->p_elapsed_time));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_eth_phy_power_control_remote_1_svc(qcsapi_eth_phy_power_control_rpcdata *__req, qcsapi_eth_phy_power_control_rpcdata *__resp, struct svc_req *rqstp)
{
	char * interface = (__req->interface == NULL) ? NULL : __req->interface->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_eth_phy_power_control(__req->on_off, interface);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_emac_switch_remote_1_svc(qcsapi_get_emac_switch_rpcdata *__req, qcsapi_get_emac_switch_rpcdata *__resp, struct svc_req *rqstp)
{
	char * buf = (__req->buf == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_emac_switch(buf);

	if (buf) {
		__resp->buf = malloc(sizeof(*__resp->buf));
		__resp->buf->data = buf;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_emac_switch_remote_1_svc(qcsapi_set_emac_switch_rpcdata *__req, qcsapi_set_emac_switch_rpcdata *__resp, struct svc_req *rqstp)
{
	qcsapi_emac_switch value=(qcsapi_emac_switch)__req->value;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_emac_switch(value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_eth_dscp_map_remote_1_svc(qcsapi_eth_dscp_map_rpcdata *__req, qcsapi_eth_dscp_map_rpcdata *__resp, struct svc_req *rqstp)
{
	qcsapi_eth_dscp_oper oper=(qcsapi_eth_dscp_oper)__req->oper;
	char * eth_type = (__req->eth_type == NULL) ? NULL : __req->eth_type->data;
	char * level = (__req->level == NULL) ? NULL : __req->level->data;
	char * value = (__req->value == NULL) ? NULL : __req->value->data;
	char * buf = (__req->buf == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_eth_dscp_map(oper, eth_type, level, value, buf, __req->size);

	if (buf) {
		__resp->buf = malloc(sizeof(*__resp->buf));
		__resp->buf->data = buf;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_eth_info_remote_1_svc(qcsapi_get_eth_info_rpcdata *__req, qcsapi_get_eth_info_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	const qcsapi_eth_info_type eth_info_type=(const qcsapi_eth_info_type)__req->eth_info_type;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_eth_info(ifname, eth_info_type);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_client_mac_list_remote_1_svc(qcsapi_get_client_mac_list_rpcdata *__req, qcsapi_get_client_mac_list_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_client_mac_list(ifname, __req->index, (struct qcsapi_mac_list *)__req->mac_address_list);

	__resp->mac_address_list = __rpc_prepare_data(__req->mac_address_list, sizeof(*__resp->mac_address_list));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_igmp_snooping_state_remote_1_svc(qcsapi_get_igmp_snooping_state_rpcdata *__req, qcsapi_get_igmp_snooping_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_igmp_snooping_state(ifname, __req->igmp_snooping_state);

	__resp->igmp_snooping_state = __rpc_prepare_data(__req->igmp_snooping_state, sizeof(*__resp->igmp_snooping_state));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_igmp_snooping_state_remote_1_svc(qcsapi_set_igmp_snooping_state_rpcdata *__req, qcsapi_set_igmp_snooping_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_igmp_snooping_state(ifname, __req->igmp_snooping_state);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_aspm_l1_remote_1_svc(qcsapi_set_aspm_l1_rpcdata *__req, qcsapi_set_aspm_l1_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_aspm_l1(__req->enable, __req->latency);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_l1_remote_1_svc(qcsapi_set_l1_rpcdata *__req, qcsapi_set_l1_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_l1(__req->enter);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_mode_remote_1_svc(qcsapi_wifi_get_mode_rpcdata *__req, qcsapi_wifi_get_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_wifi_mode * p_wifi_mode=(qcsapi_wifi_mode *)__req->p_wifi_mode;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_mode(ifname, p_wifi_mode);

	__resp->p_wifi_mode = __rpc_prepare_data(__req->p_wifi_mode, sizeof(*__resp->p_wifi_mode));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_mode_remote_1_svc(qcsapi_wifi_set_mode_rpcdata *__req, qcsapi_wifi_set_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	const qcsapi_wifi_mode new_wifi_mode=(const qcsapi_wifi_mode)__req->new_wifi_mode;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_mode(ifname, new_wifi_mode);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_phy_mode_remote_1_svc(qcsapi_wifi_get_phy_mode_rpcdata *__req, qcsapi_wifi_get_phy_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_wifi_phy_mode = (__req->p_wifi_phy_mode == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_phy_mode(ifname, p_wifi_phy_mode);

	if (p_wifi_phy_mode) {
		__resp->p_wifi_phy_mode = malloc(sizeof(*__resp->p_wifi_phy_mode));
		__resp->p_wifi_phy_mode->data = p_wifi_phy_mode;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_phy_mode_remote_1_svc(qcsapi_wifi_set_phy_mode_rpcdata *__req, qcsapi_wifi_set_phy_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * new_phy_mode = (__req->new_phy_mode == NULL) ? NULL : __req->new_phy_mode->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_phy_mode(ifname, new_phy_mode);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_reload_in_mode_remote_1_svc(qcsapi_wifi_reload_in_mode_rpcdata *__req, qcsapi_wifi_reload_in_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	const qcsapi_wifi_mode new_wifi_mode=(const qcsapi_wifi_mode)__req->new_wifi_mode;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_reload_in_mode(ifname, new_wifi_mode);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_rfenable_remote_1_svc(qcsapi_wifi_rfenable_rpcdata *__req, qcsapi_wifi_rfenable_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_rfenable(__req->onoff);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_rfstatus_remote_1_svc(qcsapi_wifi_rfstatus_rpcdata *__req, qcsapi_wifi_rfstatus_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_rfstatus(__req->rfstatus);

	__resp->rfstatus = __rpc_prepare_data(__req->rfstatus, sizeof(*__resp->rfstatus));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_bw_remote_1_svc(qcsapi_wifi_get_bw_rpcdata *__req, qcsapi_wifi_get_bw_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_bw(ifname, __req->p_bw);

	__resp->p_bw = __rpc_prepare_data(__req->p_bw, sizeof(*__resp->p_bw));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_bw_remote_1_svc(qcsapi_wifi_set_bw_rpcdata *__req, qcsapi_wifi_set_bw_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_bw(ifname, __req->bw);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_24g_bw_remote_1_svc(qcsapi_wifi_get_24g_bw_rpcdata *__req, qcsapi_wifi_get_24g_bw_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_24g_bw(ifname, __req->p_bw);

	__resp->p_bw = __rpc_prepare_data(__req->p_bw, sizeof(*__resp->p_bw));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_24g_bw_remote_1_svc(qcsapi_wifi_set_24g_bw_rpcdata *__req, qcsapi_wifi_set_24g_bw_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_24g_bw(ifname, __req->bw);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_vht_remote_1_svc(qcsapi_wifi_set_vht_rpcdata *__req, qcsapi_wifi_set_vht_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_vht(ifname, __req->the_vht);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_vht_remote_1_svc(qcsapi_wifi_get_vht_rpcdata *__req, qcsapi_wifi_get_vht_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_vht(ifname, __req->vht);

	__resp->vht = __rpc_prepare_data(__req->vht, sizeof(*__resp->vht));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_channel_remote_1_svc(qcsapi_wifi_get_channel_rpcdata *__req, qcsapi_wifi_get_channel_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_channel(ifname, __req->p_current_channel);

	__resp->p_current_channel = __rpc_prepare_data(__req->p_current_channel, sizeof(*__resp->p_current_channel));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_channel_remote_1_svc(qcsapi_wifi_set_channel_rpcdata *__req, qcsapi_wifi_set_channel_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_channel(ifname, __req->new_channel);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_chan_pri_inactive_remote_1_svc(qcsapi_wifi_get_chan_pri_inactive_rpcdata *__req, qcsapi_wifi_get_chan_pri_inactive_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_chan_pri_inactive(ifname, (struct qcsapi_data_256bytes *)__req->buffer);

	__resp->buffer = __rpc_prepare_data(__req->buffer, sizeof(*__resp->buffer));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_chan_pri_inactive_remote_1_svc(qcsapi_wifi_set_chan_pri_inactive_rpcdata *__req, qcsapi_wifi_set_chan_pri_inactive_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_chan_pri_inactive(ifname, __req->channel, __req->inactive);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_chan_pri_inactive_ext_remote_1_svc(qcsapi_wifi_set_chan_pri_inactive_ext_rpcdata *__req, qcsapi_wifi_set_chan_pri_inactive_ext_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_chan_pri_inactive_ext(ifname, __req->channel, __req->inactive, __req->option_flags);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_chan_control_remote_1_svc(qcsapi_wifi_chan_control_rpcdata *__req, qcsapi_wifi_chan_control_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_chan_control(ifname, (const struct qcsapi_data_256bytes *)__req->chans, __req->cnt, __req->flag);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_chan_disabled_remote_1_svc(qcsapi_wifi_get_chan_disabled_rpcdata *__req, qcsapi_wifi_get_chan_disabled_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_chan_disabled(ifname, (struct qcsapi_data_256bytes *)__req->p_chans, __req->p_cnt);

	__resp->p_chans = __rpc_prepare_data(__req->p_chans, sizeof(*__resp->p_chans));
	__resp->p_cnt = __rpc_prepare_data(__req->p_cnt, sizeof(*__resp->p_cnt));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_supported_freq_bands_remote_1_svc(qcsapi_wifi_get_supported_freq_bands_rpcdata *__req, qcsapi_wifi_get_supported_freq_bands_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_bands = (__req->p_bands == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_supported_freq_bands(ifname, p_bands);

	if (p_bands) {
		__resp->p_bands = malloc(sizeof(*__resp->p_bands));
		__resp->p_bands->data = p_bands;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_beacon_interval_remote_1_svc(qcsapi_wifi_get_beacon_interval_rpcdata *__req, qcsapi_wifi_get_beacon_interval_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_beacon_interval(ifname, __req->p_current_intval);

	__resp->p_current_intval = __rpc_prepare_data(__req->p_current_intval, sizeof(*__resp->p_current_intval));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_beacon_interval_remote_1_svc(qcsapi_wifi_set_beacon_interval_rpcdata *__req, qcsapi_wifi_set_beacon_interval_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_beacon_interval(ifname, __req->new_intval);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_dtim_remote_1_svc(qcsapi_wifi_get_dtim_rpcdata *__req, qcsapi_wifi_get_dtim_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_dtim(ifname, __req->p_dtim);

	__resp->p_dtim = __rpc_prepare_data(__req->p_dtim, sizeof(*__resp->p_dtim));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dtim_remote_1_svc(qcsapi_wifi_set_dtim_rpcdata *__req, qcsapi_wifi_set_dtim_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_dtim(ifname, __req->new_dtim);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_assoc_limit_remote_1_svc(qcsapi_wifi_get_assoc_limit_rpcdata *__req, qcsapi_wifi_get_assoc_limit_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_assoc_limit(ifname, __req->p_assoc_limit);

	__resp->p_assoc_limit = __rpc_prepare_data(__req->p_assoc_limit, sizeof(*__resp->p_assoc_limit));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_bss_assoc_limit_remote_1_svc(qcsapi_wifi_get_bss_assoc_limit_rpcdata *__req, qcsapi_wifi_get_bss_assoc_limit_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_bss_assoc_limit(__req->group, __req->p_assoc_limit);

	__resp->p_assoc_limit = __rpc_prepare_data(__req->p_assoc_limit, sizeof(*__resp->p_assoc_limit));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_assoc_limit_remote_1_svc(qcsapi_wifi_set_assoc_limit_rpcdata *__req, qcsapi_wifi_set_assoc_limit_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_assoc_limit(ifname, __req->new_assoc_limit);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_bss_assoc_limit_remote_1_svc(qcsapi_wifi_set_bss_assoc_limit_rpcdata *__req, qcsapi_wifi_set_bss_assoc_limit_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_bss_assoc_limit(__req->group, __req->limit);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_ssid_group_id_remote_1_svc(qcsapi_wifi_set_SSID_group_id_rpcdata *__req, qcsapi_wifi_set_SSID_group_id_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_SSID_group_id(ifname, __req->group);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_ssid_group_id_remote_1_svc(qcsapi_wifi_get_SSID_group_id_rpcdata *__req, qcsapi_wifi_get_SSID_group_id_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_SSID_group_id(ifname, __req->p_group);

	__resp->p_group = __rpc_prepare_data(__req->p_group, sizeof(*__resp->p_group));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_ssid_assoc_reserve_remote_1_svc(qcsapi_wifi_set_SSID_assoc_reserve_rpcdata *__req, qcsapi_wifi_set_SSID_assoc_reserve_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_SSID_assoc_reserve(__req->group, __req->value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_ssid_assoc_reserve_remote_1_svc(qcsapi_wifi_get_SSID_assoc_reserve_rpcdata *__req, qcsapi_wifi_get_SSID_assoc_reserve_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_SSID_assoc_reserve(__req->group, __req->p_value);

	__resp->p_value = __rpc_prepare_data(__req->p_value, sizeof(*__resp->p_value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_bssid_remote_1_svc(qcsapi_wifi_get_BSSID_rpcdata *__req, qcsapi_wifi_get_BSSID_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * current_BSSID = (__req->current_BSSID == NULL) ? NULL : __req->current_BSSID->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_BSSID(ifname, current_BSSID);

	__resp->current_BSSID = __rpc_prepare_data(__req->current_BSSID, sizeof(*__resp->current_BSSID));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_config_bssid_remote_1_svc(qcsapi_wifi_get_config_BSSID_rpcdata *__req, qcsapi_wifi_get_config_BSSID_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * config_BSSID = (__req->config_BSSID == NULL) ? NULL : __req->config_BSSID->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_config_BSSID(ifname, config_BSSID);

	__resp->config_BSSID = __rpc_prepare_data(__req->config_BSSID, sizeof(*__resp->config_BSSID));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_ssid_get_bssid_remote_1_svc(qcsapi_wifi_ssid_get_bssid_rpcdata *__req, qcsapi_wifi_ssid_get_bssid_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * ssid_str = (__req->ssid_str == NULL) ? NULL : __req->ssid_str->data;
	uint8_t * bssid = (__req->bssid == NULL) ? NULL : __req->bssid->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_ssid_get_bssid(ifname, ssid_str, bssid);

	__resp->bssid = __rpc_prepare_data(__req->bssid, sizeof(*__resp->bssid));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_ssid_set_bssid_remote_1_svc(qcsapi_wifi_ssid_set_bssid_rpcdata *__req, qcsapi_wifi_ssid_set_bssid_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * ssid_str = (__req->ssid_str == NULL) ? NULL : __req->ssid_str->data;
	uint8_t * bssid = (__req->bssid == NULL) ? NULL : __req->bssid->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_ssid_set_bssid(ifname, ssid_str, bssid);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_ssid_remote_1_svc(qcsapi_wifi_get_SSID_rpcdata *__req, qcsapi_wifi_get_SSID_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * SSID_str = (__req->SSID_str == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_SSID(ifname, SSID_str);

	if (SSID_str) {
		__resp->SSID_str = malloc(sizeof(*__resp->SSID_str));
		__resp->SSID_str->data = SSID_str;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_ssid_remote_1_svc(qcsapi_wifi_set_SSID_rpcdata *__req, qcsapi_wifi_set_SSID_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * SSID_str = (__req->SSID_str == NULL) ? NULL : __req->SSID_str->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_SSID(ifname, SSID_str);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_ieee_802_11_standard_remote_1_svc(qcsapi_wifi_get_IEEE_802_11_standard_rpcdata *__req, qcsapi_wifi_get_IEEE_802_11_standard_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * IEEE_802_11_standard = (__req->IEEE_802_11_standard == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_IEEE_802_11_standard(ifname, IEEE_802_11_standard);

	if (IEEE_802_11_standard) {
		__resp->IEEE_802_11_standard = malloc(sizeof(*__resp->IEEE_802_11_standard));
		__resp->IEEE_802_11_standard->data = IEEE_802_11_standard;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_list_channels_remote_1_svc(qcsapi_wifi_get_list_channels_rpcdata *__req, qcsapi_wifi_get_list_channels_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * list_of_channels = (__req->list_of_channels == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_list_channels(ifname, list_of_channels);

	if (list_of_channels) {
		__resp->list_of_channels = malloc(sizeof(*__resp->list_of_channels));
		__resp->list_of_channels->data = list_of_channels;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_supp_chans_remote_1_svc(qcsapi_wifi_get_supp_chans_rpcdata *__req, qcsapi_wifi_get_supp_chans_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * mac_addr = (__req->mac_addr == NULL) ? NULL : __req->mac_addr->data;
	char * list_of_channels = (__req->list_of_channels == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_supp_chans(ifname, mac_addr, list_of_channels);

	__resp->mac_addr = __rpc_prepare_data(__req->mac_addr, sizeof(*__resp->mac_addr));
	if (list_of_channels) {
		__resp->list_of_channels = malloc(sizeof(*__resp->list_of_channels));
		__resp->list_of_channels->data = list_of_channels;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_mode_switch_remote_1_svc(qcsapi_wifi_get_mode_switch_rpcdata *__req, qcsapi_wifi_get_mode_switch_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_mode_switch(__req->p_wifi_mode_switch_setting);

	__resp->p_wifi_mode_switch_setting = __rpc_prepare_data(__req->p_wifi_mode_switch_setting, sizeof(*__resp->p_wifi_mode_switch_setting));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_disassociate_remote_1_svc(qcsapi_wifi_disassociate_rpcdata *__req, qcsapi_wifi_disassociate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_disassociate(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_disassociate_sta_remote_1_svc(qcsapi_wifi_disassociate_sta_rpcdata *__req, qcsapi_wifi_disassociate_sta_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * mac = (__req->mac == NULL) ? NULL : __req->mac->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_disassociate_sta(ifname, mac);

	__resp->mac = __rpc_prepare_data(__req->mac, sizeof(*__resp->mac));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_reassociate_remote_1_svc(qcsapi_wifi_reassociate_rpcdata *__req, qcsapi_wifi_reassociate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_reassociate(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_disconn_info_remote_1_svc(qcsapi_wifi_get_disconn_info_rpcdata *__req, qcsapi_wifi_get_disconn_info_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_disconn_info(ifname, (qcsapi_disconn_info *)__req->disconn_info);

	__resp->disconn_info = __rpc_prepare_data(__req->disconn_info, sizeof(*__resp->disconn_info));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_disable_wps_remote_1_svc(qcsapi_wifi_disable_wps_rpcdata *__req, qcsapi_wifi_disable_wps_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_disable_wps(ifname, __req->disable_wps);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_associate_remote_1_svc(qcsapi_wifi_associate_rpcdata *__req, qcsapi_wifi_associate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * join_ssid = (__req->join_ssid == NULL) ? NULL : __req->join_ssid->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_associate(ifname, join_ssid);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_start_cca_remote_1_svc(qcsapi_wifi_start_cca_rpcdata *__req, qcsapi_wifi_start_cca_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_start_cca(ifname, __req->channel, __req->duration);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_noise_remote_1_svc(qcsapi_wifi_get_noise_rpcdata *__req, qcsapi_wifi_get_noise_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_noise(ifname, __req->p_noise);

	__resp->p_noise = __rpc_prepare_data(__req->p_noise, sizeof(*__resp->p_noise));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_rssi_by_chain_remote_1_svc(qcsapi_wifi_get_rssi_by_chain_rpcdata *__req, qcsapi_wifi_get_rssi_by_chain_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_rssi_by_chain(ifname, __req->rf_chain, __req->p_rssi);

	__resp->p_rssi = __rpc_prepare_data(__req->p_rssi, sizeof(*__resp->p_rssi));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_avg_snr_remote_1_svc(qcsapi_wifi_get_avg_snr_rpcdata *__req, qcsapi_wifi_get_avg_snr_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_avg_snr(ifname, __req->p_snr);

	__resp->p_snr = __rpc_prepare_data(__req->p_snr, sizeof(*__resp->p_snr));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_primary_interface_remote_1_svc(qcsapi_get_primary_interface_rpcdata *__req, qcsapi_get_primary_interface_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_primary_interface(ifname, __req->maxlen);

	if (ifname) {
		__resp->ifname = malloc(sizeof(*__resp->ifname));
		__resp->ifname->data = ifname;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_interface_by_index_remote_1_svc(qcsapi_get_interface_by_index_rpcdata *__req, qcsapi_get_interface_by_index_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_interface_by_index(__req->if_index, ifname, __req->maxlen);

	if (ifname) {
		__resp->ifname = malloc(sizeof(*__resp->ifname));
		__resp->ifname->data = ifname;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_wifi_macaddr_remote_1_svc(qcsapi_wifi_set_wifi_macaddr_rpcdata *__req, qcsapi_wifi_set_wifi_macaddr_rpcdata *__resp, struct svc_req *rqstp)
{
	uint8_t * new_mac_addr = (__req->new_mac_addr == NULL) ? NULL : __req->new_mac_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_wifi_macaddr(new_mac_addr);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_interface_get_bssid_remote_1_svc(qcsapi_interface_get_BSSID_rpcdata *__req, qcsapi_interface_get_BSSID_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * current_BSSID = (__req->current_BSSID == NULL) ? NULL : __req->current_BSSID->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_interface_get_BSSID(ifname, current_BSSID);

	__resp->current_BSSID = __rpc_prepare_data(__req->current_BSSID, sizeof(*__resp->current_BSSID));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_rates_remote_1_svc(qcsapi_wifi_get_rates_rpcdata *__req, qcsapi_wifi_get_rates_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_rate_type rate_type=(qcsapi_rate_type)__req->rate_type;
	char * supported_rates = (__req->supported_rates == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_rates(ifname, rate_type, supported_rates);

	if (supported_rates) {
		__resp->supported_rates = malloc(sizeof(*__resp->supported_rates));
		__resp->supported_rates->data = supported_rates;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_rates_remote_1_svc(qcsapi_wifi_set_rates_rpcdata *__req, qcsapi_wifi_set_rates_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_rate_type rate_type=(qcsapi_rate_type)__req->rate_type;
	char * current_rates = (__req->current_rates == NULL) ? NULL : __req->current_rates->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_rates(ifname, rate_type, current_rates, __req->num_rates);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_max_bitrate_remote_1_svc(qcsapi_get_max_bitrate_rpcdata *__req, qcsapi_get_max_bitrate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * max_bitrate = (__req->max_bitrate == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_max_bitrate(ifname, max_bitrate, __req->max_str_len);

	if (max_bitrate) {
		__resp->max_bitrate = malloc(sizeof(*__resp->max_bitrate));
		__resp->max_bitrate->data = max_bitrate;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_max_bitrate_remote_1_svc(qcsapi_set_max_bitrate_rpcdata *__req, qcsapi_set_max_bitrate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * max_bitrate = (__req->max_bitrate == NULL) ? NULL : __req->max_bitrate->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_max_bitrate(ifname, max_bitrate);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_qos_get_param_remote_1_svc(qcsapi_wifi_qos_get_param_rpcdata *__req, qcsapi_wifi_qos_get_param_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_qos_get_param(ifname, __req->the_queue, __req->the_param, __req->ap_bss_flag, __req->p_value);

	__resp->p_value = __rpc_prepare_data(__req->p_value, sizeof(*__resp->p_value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_qos_set_param_remote_1_svc(qcsapi_wifi_qos_set_param_rpcdata *__req, qcsapi_wifi_qos_set_param_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_qos_set_param(ifname, __req->the_queue, __req->the_param, __req->ap_bss_flag, __req->value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_wmm_ac_map_remote_1_svc(qcsapi_wifi_get_wmm_ac_map_rpcdata *__req, qcsapi_wifi_get_wmm_ac_map_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * mapping_table = (__req->mapping_table == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_wmm_ac_map(ifname, mapping_table);

	if (mapping_table) {
		__resp->mapping_table = malloc(sizeof(*__resp->mapping_table));
		__resp->mapping_table->data = mapping_table;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_wmm_ac_map_remote_1_svc(qcsapi_wifi_set_wmm_ac_map_rpcdata *__req, qcsapi_wifi_set_wmm_ac_map_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_wmm_ac_map(ifname, __req->user_prio, __req->ac_index);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_dscp_8021p_map_remote_1_svc(qcsapi_wifi_get_dscp_8021p_map_rpcdata *__req, qcsapi_wifi_get_dscp_8021p_map_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * mapping_table = (__req->mapping_table == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_dscp_8021p_map(ifname, mapping_table);

	if (mapping_table) {
		__resp->mapping_table = malloc(sizeof(*__resp->mapping_table));
		__resp->mapping_table->data = mapping_table;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_dscp_ac_map_remote_1_svc(qcsapi_wifi_get_dscp_ac_map_rpcdata *__req, qcsapi_wifi_get_dscp_ac_map_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_dscp_ac_map(ifname, (struct qcsapi_data_64bytes *)__req->mapping_table);

	__resp->mapping_table = __rpc_prepare_data(__req->mapping_table, sizeof(*__resp->mapping_table));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dscp_8021p_map_remote_1_svc(qcsapi_wifi_set_dscp_8021p_map_rpcdata *__req, qcsapi_wifi_set_dscp_8021p_map_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * ip_dscp_list = (__req->ip_dscp_list == NULL) ? NULL : __req->ip_dscp_list->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_dscp_8021p_map(ifname, ip_dscp_list, __req->dot1p_up);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dscp_ac_map_remote_1_svc(qcsapi_wifi_set_dscp_ac_map_rpcdata *__req, qcsapi_wifi_set_dscp_ac_map_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_dscp_ac_map(ifname, (const struct qcsapi_data_64bytes *)__req->dscp_list, __req->dscp_list_len, __req->ac);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_qos_map_remote_1_svc(qcsapi_wifi_set_qos_map_rpcdata *__req, qcsapi_wifi_set_qos_map_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * qos_map_str = (__req->qos_map_str == NULL) ? NULL : __req->qos_map_str->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_qos_map(ifname, qos_map_str);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_del_qos_map_remote_1_svc(qcsapi_wifi_del_qos_map_rpcdata *__req, qcsapi_wifi_del_qos_map_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_del_qos_map(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_qos_map_remote_1_svc(qcsapi_wifi_get_qos_map_rpcdata *__req, qcsapi_wifi_get_qos_map_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * value = (__req->value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_qos_map(ifname, value);

	if (value) {
		__resp->value = malloc(sizeof(*__resp->value));
		__resp->value->data = value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_send_qos_map_conf_remote_1_svc(qcsapi_wifi_send_qos_map_conf_rpcdata *__req, qcsapi_wifi_send_qos_map_conf_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * sta_mac_addr = (__req->sta_mac_addr == NULL) ? NULL : __req->sta_mac_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_send_qos_map_conf(ifname, sta_mac_addr);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_dscp_tid_map_remote_1_svc(qcsapi_wifi_get_dscp_tid_map_rpcdata *__req, qcsapi_wifi_get_dscp_tid_map_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_dscp_tid_map(ifname, (struct qcsapi_data_64bytes *)__req->dscp2tid_ptr);

	__resp->dscp2tid_ptr = __rpc_prepare_data(__req->dscp2tid_ptr, sizeof(*__resp->dscp2tid_ptr));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_priority_remote_1_svc(qcsapi_wifi_get_priority_rpcdata *__req, qcsapi_wifi_get_priority_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_priority(ifname, __req->p_priority);

	__resp->p_priority = __rpc_prepare_data(__req->p_priority, sizeof(*__resp->p_priority));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_priority_remote_1_svc(qcsapi_wifi_set_priority_rpcdata *__req, qcsapi_wifi_set_priority_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_priority(ifname, __req->priority);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_airfair_remote_1_svc(qcsapi_wifi_get_airfair_rpcdata *__req, qcsapi_wifi_get_airfair_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_airfair(ifname, __req->p_airfair);

	__resp->p_airfair = __rpc_prepare_data(__req->p_airfair, sizeof(*__resp->p_airfair));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_airfair_remote_1_svc(qcsapi_wifi_set_airfair_rpcdata *__req, qcsapi_wifi_set_airfair_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_airfair(ifname, __req->airfair);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tx_power_remote_1_svc(qcsapi_wifi_get_tx_power_rpcdata *__req, qcsapi_wifi_get_tx_power_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tx_power(ifname, __req->the_channel, __req->p_tx_power);

	__resp->p_tx_power = __rpc_prepare_data(__req->p_tx_power, sizeof(*__resp->p_tx_power));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_tx_power_remote_1_svc(qcsapi_wifi_set_tx_power_rpcdata *__req, qcsapi_wifi_set_tx_power_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_tx_power(ifname, __req->the_channel, __req->tx_power);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_bw_power_remote_1_svc(qcsapi_wifi_get_bw_power_rpcdata *__req, qcsapi_wifi_get_bw_power_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_bw_power(ifname, __req->the_channel, __req->p_power_20M, __req->p_power_40M, __req->p_power_80M);

	__resp->p_power_20M = __rpc_prepare_data(__req->p_power_20M, sizeof(*__resp->p_power_20M));
	__resp->p_power_40M = __rpc_prepare_data(__req->p_power_40M, sizeof(*__resp->p_power_40M));
	__resp->p_power_80M = __rpc_prepare_data(__req->p_power_80M, sizeof(*__resp->p_power_80M));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_bw_power_remote_1_svc(qcsapi_wifi_set_bw_power_rpcdata *__req, qcsapi_wifi_set_bw_power_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_bw_power(ifname, __req->the_channel, __req->power_20M, __req->power_40M, __req->power_80M);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_bf_power_remote_1_svc(qcsapi_wifi_get_bf_power_rpcdata *__req, qcsapi_wifi_get_bf_power_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_bf_power(ifname, __req->the_channel, __req->number_ss, __req->p_power_20M, __req->p_power_40M, __req->p_power_80M);

	__resp->p_power_20M = __rpc_prepare_data(__req->p_power_20M, sizeof(*__resp->p_power_20M));
	__resp->p_power_40M = __rpc_prepare_data(__req->p_power_40M, sizeof(*__resp->p_power_40M));
	__resp->p_power_80M = __rpc_prepare_data(__req->p_power_80M, sizeof(*__resp->p_power_80M));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_bf_power_remote_1_svc(qcsapi_wifi_set_bf_power_rpcdata *__req, qcsapi_wifi_set_bf_power_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_bf_power(ifname, __req->the_channel, __req->number_ss, __req->power_20M, __req->power_40M, __req->power_80M);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tx_power_ext_remote_1_svc(qcsapi_wifi_get_tx_power_ext_rpcdata *__req, qcsapi_wifi_get_tx_power_ext_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tx_power_ext(ifname, __req->the_channel, __req->bf_on, __req->number_ss, __req->p_power_20M, __req->p_power_40M, __req->p_power_80M);

	__resp->p_power_20M = __rpc_prepare_data(__req->p_power_20M, sizeof(*__resp->p_power_20M));
	__resp->p_power_40M = __rpc_prepare_data(__req->p_power_40M, sizeof(*__resp->p_power_40M));
	__resp->p_power_80M = __rpc_prepare_data(__req->p_power_80M, sizeof(*__resp->p_power_80M));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_tx_power_ext_remote_1_svc(qcsapi_wifi_set_tx_power_ext_rpcdata *__req, qcsapi_wifi_set_tx_power_ext_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_tx_power_ext(ifname, __req->the_channel, __req->bf_on, __req->number_ss, __req->power_20M, __req->power_40M, __req->power_80M);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_chan_power_table_remote_1_svc(qcsapi_wifi_get_chan_power_table_rpcdata *__req, qcsapi_wifi_get_chan_power_table_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_chan_power_table(ifname, (qcsapi_channel_power_table *)__req->chan_power_table);

	__resp->chan_power_table = __rpc_prepare_data(__req->chan_power_table, sizeof(*__resp->chan_power_table));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_chan_power_table_remote_1_svc(qcsapi_wifi_set_chan_power_table_rpcdata *__req, qcsapi_wifi_set_chan_power_table_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_chan_power_table(ifname, (qcsapi_channel_power_table *)__req->chan_power_table);

	__resp->chan_power_table = __rpc_prepare_data(__req->chan_power_table, sizeof(*__resp->chan_power_table));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_power_selection_remote_1_svc(qcsapi_wifi_get_power_selection_rpcdata *__req, qcsapi_wifi_get_power_selection_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_power_selection(__req->p_power_selection);

	__resp->p_power_selection = __rpc_prepare_data(__req->p_power_selection, sizeof(*__resp->p_power_selection));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_power_selection_remote_1_svc(qcsapi_wifi_set_power_selection_rpcdata *__req, qcsapi_wifi_set_power_selection_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_power_selection(__req->power_selection);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_carrier_interference_remote_1_svc(qcsapi_wifi_get_carrier_interference_rpcdata *__req, qcsapi_wifi_get_carrier_interference_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_carrier_interference(ifname, __req->ci);

	__resp->ci = __rpc_prepare_data(__req->ci, sizeof(*__resp->ci));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_congestion_index_remote_1_svc(qcsapi_wifi_get_congestion_index_rpcdata *__req, qcsapi_wifi_get_congestion_index_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_congestion_index(ifname, __req->ci);

	__resp->ci = __rpc_prepare_data(__req->ci, sizeof(*__resp->ci));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_supported_tx_power_levels_remote_1_svc(qcsapi_wifi_get_supported_tx_power_levels_rpcdata *__req, qcsapi_wifi_get_supported_tx_power_levels_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * available_percentages = (__req->available_percentages == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_supported_tx_power_levels(ifname, available_percentages);

	if (available_percentages) {
		__resp->available_percentages = malloc(sizeof(*__resp->available_percentages));
		__resp->available_percentages->data = available_percentages;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_current_tx_power_level_remote_1_svc(qcsapi_wifi_get_current_tx_power_level_rpcdata *__req, qcsapi_wifi_get_current_tx_power_level_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_current_tx_power_level(ifname, __req->p_current_percentage);

	__resp->p_current_percentage = __rpc_prepare_data(__req->p_current_percentage, sizeof(*__resp->p_current_percentage));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_current_tx_power_level_remote_1_svc(qcsapi_wifi_set_current_tx_power_level_rpcdata *__req, qcsapi_wifi_set_current_tx_power_level_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_current_tx_power_level(ifname, __req->txpower_percentage);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_power_constraint_remote_1_svc(qcsapi_wifi_set_power_constraint_rpcdata *__req, qcsapi_wifi_set_power_constraint_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_power_constraint(ifname, __req->pwr_constraint);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_power_constraint_remote_1_svc(qcsapi_wifi_get_power_constraint_rpcdata *__req, qcsapi_wifi_get_power_constraint_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_power_constraint(ifname, __req->p_pwr_constraint);

	__resp->p_pwr_constraint = __rpc_prepare_data(__req->p_pwr_constraint, sizeof(*__resp->p_pwr_constraint));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_tpc_interval_remote_1_svc(qcsapi_wifi_set_tpc_interval_rpcdata *__req, qcsapi_wifi_set_tpc_interval_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_tpc_interval(ifname, __req->tpc_interval);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tpc_interval_remote_1_svc(qcsapi_wifi_get_tpc_interval_rpcdata *__req, qcsapi_wifi_get_tpc_interval_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tpc_interval(ifname, __req->p_tpc_interval);

	__resp->p_tpc_interval = __rpc_prepare_data(__req->p_tpc_interval, sizeof(*__resp->p_tpc_interval));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_assoc_records_remote_1_svc(qcsapi_wifi_get_assoc_records_rpcdata *__req, qcsapi_wifi_get_assoc_records_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_assoc_records(ifname, __req->reset, (qcsapi_assoc_records *)__req->records);

	__resp->records = __rpc_prepare_data(__req->records, sizeof(*__resp->records));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_ap_isolate_remote_1_svc(qcsapi_wifi_get_ap_isolate_rpcdata *__req, qcsapi_wifi_get_ap_isolate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_ap_isolate(ifname, __req->p_ap_isolate);

	__resp->p_ap_isolate = __rpc_prepare_data(__req->p_ap_isolate, sizeof(*__resp->p_ap_isolate));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_ap_isolate_remote_1_svc(qcsapi_wifi_set_ap_isolate_rpcdata *__req, qcsapi_wifi_set_ap_isolate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_ap_isolate(ifname, __req->new_ap_isolate);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_intra_bss_isolate_remote_1_svc(qcsapi_wifi_get_intra_bss_isolate_rpcdata *__req, qcsapi_wifi_get_intra_bss_isolate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_intra_bss_isolate(ifname, __req->p_ap_isolate);

	__resp->p_ap_isolate = __rpc_prepare_data(__req->p_ap_isolate, sizeof(*__resp->p_ap_isolate));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_intra_bss_isolate_remote_1_svc(qcsapi_wifi_set_intra_bss_isolate_rpcdata *__req, qcsapi_wifi_set_intra_bss_isolate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_intra_bss_isolate(ifname, __req->new_ap_isolate);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_bss_isolate_remote_1_svc(qcsapi_wifi_get_bss_isolate_rpcdata *__req, qcsapi_wifi_get_bss_isolate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_bss_isolate(ifname, __req->p_ap_isolate);

	__resp->p_ap_isolate = __rpc_prepare_data(__req->p_ap_isolate, sizeof(*__resp->p_ap_isolate));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_bss_isolate_remote_1_svc(qcsapi_wifi_set_bss_isolate_rpcdata *__req, qcsapi_wifi_set_bss_isolate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_bss_isolate(ifname, __req->new_ap_isolate);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_disable_dfs_channels_remote_1_svc(qcsapi_wifi_disable_dfs_channels_rpcdata *__req, qcsapi_wifi_disable_dfs_channels_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_disable_dfs_channels(ifname, __req->disable_dfs, __req->channel);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_is_ready_remote_1_svc(qcsapi_wifi_is_ready_rpcdata *__req, qcsapi_wifi_is_ready_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_is_ready(__req->p_value);

	__resp->p_value = __rpc_prepare_data(__req->p_value, sizeof(*__resp->p_value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_create_restricted_bss_remote_1_svc(qcsapi_wifi_create_restricted_bss_rpcdata *__req, qcsapi_wifi_create_restricted_bss_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * mac_addr = (__req->mac_addr == NULL) ? NULL : __req->mac_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_create_restricted_bss(ifname, mac_addr);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_create_bss_remote_1_svc(qcsapi_wifi_create_bss_rpcdata *__req, qcsapi_wifi_create_bss_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * mac_addr = (__req->mac_addr == NULL) ? NULL : __req->mac_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_create_bss(ifname, mac_addr);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_remove_bss_remote_1_svc(qcsapi_wifi_remove_bss_rpcdata *__req, qcsapi_wifi_remove_bss_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_remove_bss(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wds_add_peer_remote_1_svc(qcsapi_wds_add_peer_rpcdata *__req, qcsapi_wds_add_peer_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * peer_address = (__req->peer_address == NULL) ? NULL : __req->peer_address->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wds_add_peer(ifname, peer_address);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wds_add_peer_encrypt_remote_1_svc(qcsapi_wds_add_peer_encrypt_rpcdata *__req, qcsapi_wds_add_peer_encrypt_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * peer_address = (__req->peer_address == NULL) ? NULL : __req->peer_address->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wds_add_peer_encrypt(ifname, peer_address, __req->encryption);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wds_remove_peer_remote_1_svc(qcsapi_wds_remove_peer_rpcdata *__req, qcsapi_wds_remove_peer_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * peer_address = (__req->peer_address == NULL) ? NULL : __req->peer_address->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wds_remove_peer(ifname, peer_address);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wds_get_peer_address_remote_1_svc(qcsapi_wds_get_peer_address_rpcdata *__req, qcsapi_wds_get_peer_address_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * peer_address = (__req->peer_address == NULL) ? NULL : __req->peer_address->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wds_get_peer_address(ifname, __req->index, peer_address);

	__resp->peer_address = __rpc_prepare_data(__req->peer_address, sizeof(*__resp->peer_address));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wds_set_psk_remote_1_svc(qcsapi_wds_set_psk_rpcdata *__req, qcsapi_wds_set_psk_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * peer_address = (__req->peer_address == NULL) ? NULL : __req->peer_address->data;
	char * pre_shared_key = (__req->pre_shared_key == NULL) ? NULL : __req->pre_shared_key->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wds_set_psk(ifname, peer_address, pre_shared_key);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wds_set_mode_remote_1_svc(qcsapi_wds_set_mode_rpcdata *__req, qcsapi_wds_set_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * peer_address = (__req->peer_address == NULL) ? NULL : __req->peer_address->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wds_set_mode(ifname, peer_address, __req->mode);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wds_get_mode_remote_1_svc(qcsapi_wds_get_mode_rpcdata *__req, qcsapi_wds_get_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wds_get_mode(ifname, __req->index, __req->mode);

	__resp->mode = __rpc_prepare_data(__req->mode, sizeof(*__resp->mode));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_extender_params_remote_1_svc(qcsapi_wifi_set_extender_params_rpcdata *__req, qcsapi_wifi_set_extender_params_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	const qcsapi_extender_type type=(const qcsapi_extender_type)__req->type;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_extender_params(ifname, type, __req->param_value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_extender_params_remote_1_svc(qcsapi_wifi_get_extender_params_rpcdata *__req, qcsapi_wifi_get_extender_params_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	const qcsapi_extender_type type=(const qcsapi_extender_type)__req->type;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_extender_params(ifname, type, __req->p_value);

	__resp->p_value = __rpc_prepare_data(__req->p_value, sizeof(*__resp->p_value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_beacon_type_remote_1_svc(qcsapi_wifi_get_beacon_type_rpcdata *__req, qcsapi_wifi_get_beacon_type_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_current_beacon = (__req->p_current_beacon == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_beacon_type(ifname, p_current_beacon);

	if (p_current_beacon) {
		__resp->p_current_beacon = malloc(sizeof(*__resp->p_current_beacon));
		__resp->p_current_beacon->data = p_current_beacon;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_beacon_type_remote_1_svc(qcsapi_wifi_set_beacon_type_rpcdata *__req, qcsapi_wifi_set_beacon_type_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_new_beacon = (__req->p_new_beacon == NULL) ? NULL : __req->p_new_beacon->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_beacon_type(ifname, p_new_beacon);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_wep_key_index_remote_1_svc(qcsapi_wifi_get_WEP_key_index_rpcdata *__req, qcsapi_wifi_get_WEP_key_index_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_WEP_key_index(ifname, __req->p_key_index);

	__resp->p_key_index = __rpc_prepare_data(__req->p_key_index, sizeof(*__resp->p_key_index));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_wep_key_index_remote_1_svc(qcsapi_wifi_set_WEP_key_index_rpcdata *__req, qcsapi_wifi_set_WEP_key_index_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_WEP_key_index(ifname, __req->key_index);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_wep_key_passphrase_remote_1_svc(qcsapi_wifi_get_WEP_key_passphrase_rpcdata *__req, qcsapi_wifi_get_WEP_key_passphrase_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_passphrase = (__req->current_passphrase == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_WEP_key_passphrase(ifname, current_passphrase);

	if (current_passphrase) {
		__resp->current_passphrase = malloc(sizeof(*__resp->current_passphrase));
		__resp->current_passphrase->data = current_passphrase;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_wep_key_passphrase_remote_1_svc(qcsapi_wifi_set_WEP_key_passphrase_rpcdata *__req, qcsapi_wifi_set_WEP_key_passphrase_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * new_passphrase = (__req->new_passphrase == NULL) ? NULL : __req->new_passphrase->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_WEP_key_passphrase(ifname, new_passphrase);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_wep_encryption_level_remote_1_svc(qcsapi_wifi_get_WEP_encryption_level_rpcdata *__req, qcsapi_wifi_get_WEP_encryption_level_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_encryption_level = (__req->current_encryption_level == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_WEP_encryption_level(ifname, current_encryption_level);

	if (current_encryption_level) {
		__resp->current_encryption_level = malloc(sizeof(*__resp->current_encryption_level));
		__resp->current_encryption_level->data = current_encryption_level;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_basic_encryption_modes_remote_1_svc(qcsapi_wifi_get_basic_encryption_modes_rpcdata *__req, qcsapi_wifi_get_basic_encryption_modes_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * encryption_modes = (__req->encryption_modes == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_basic_encryption_modes(ifname, encryption_modes);

	if (encryption_modes) {
		__resp->encryption_modes = malloc(sizeof(*__resp->encryption_modes));
		__resp->encryption_modes->data = encryption_modes;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_basic_encryption_modes_remote_1_svc(qcsapi_wifi_set_basic_encryption_modes_rpcdata *__req, qcsapi_wifi_set_basic_encryption_modes_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * encryption_modes = (__req->encryption_modes == NULL) ? NULL : __req->encryption_modes->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_basic_encryption_modes(ifname, encryption_modes);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_basic_authentication_mode_remote_1_svc(qcsapi_wifi_get_basic_authentication_mode_rpcdata *__req, qcsapi_wifi_get_basic_authentication_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * authentication_mode = (__req->authentication_mode == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_basic_authentication_mode(ifname, authentication_mode);

	if (authentication_mode) {
		__resp->authentication_mode = malloc(sizeof(*__resp->authentication_mode));
		__resp->authentication_mode->data = authentication_mode;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_basic_authentication_mode_remote_1_svc(qcsapi_wifi_set_basic_authentication_mode_rpcdata *__req, qcsapi_wifi_set_basic_authentication_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * authentication_mode = (__req->authentication_mode == NULL) ? NULL : __req->authentication_mode->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_basic_authentication_mode(ifname, authentication_mode);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_wep_key_remote_1_svc(qcsapi_wifi_get_WEP_key_rpcdata *__req, qcsapi_wifi_get_WEP_key_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_passphrase = (__req->current_passphrase == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_WEP_key(ifname, __req->key_index, current_passphrase);

	if (current_passphrase) {
		__resp->current_passphrase = malloc(sizeof(*__resp->current_passphrase));
		__resp->current_passphrase->data = current_passphrase;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_wep_key_remote_1_svc(qcsapi_wifi_set_WEP_key_rpcdata *__req, qcsapi_wifi_set_WEP_key_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * new_passphrase = (__req->new_passphrase == NULL) ? NULL : __req->new_passphrase->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_WEP_key(ifname, __req->key_index, new_passphrase);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_wpa_encryption_modes_remote_1_svc(qcsapi_wifi_get_WPA_encryption_modes_rpcdata *__req, qcsapi_wifi_get_WPA_encryption_modes_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * encryption_modes = (__req->encryption_modes == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_WPA_encryption_modes(ifname, encryption_modes);

	if (encryption_modes) {
		__resp->encryption_modes = malloc(sizeof(*__resp->encryption_modes));
		__resp->encryption_modes->data = encryption_modes;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_wpa_encryption_modes_remote_1_svc(qcsapi_wifi_set_WPA_encryption_modes_rpcdata *__req, qcsapi_wifi_set_WPA_encryption_modes_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * encryption_modes = (__req->encryption_modes == NULL) ? NULL : __req->encryption_modes->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_WPA_encryption_modes(ifname, encryption_modes);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_wpa_authentication_mode_remote_1_svc(qcsapi_wifi_get_WPA_authentication_mode_rpcdata *__req, qcsapi_wifi_get_WPA_authentication_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * authentication_mode = (__req->authentication_mode == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_WPA_authentication_mode(ifname, authentication_mode);

	if (authentication_mode) {
		__resp->authentication_mode = malloc(sizeof(*__resp->authentication_mode));
		__resp->authentication_mode->data = authentication_mode;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_wpa_authentication_mode_remote_1_svc(qcsapi_wifi_set_WPA_authentication_mode_rpcdata *__req, qcsapi_wifi_set_WPA_authentication_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * authentication_mode = (__req->authentication_mode == NULL) ? NULL : __req->authentication_mode->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_WPA_authentication_mode(ifname, authentication_mode);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_interworking_remote_1_svc(qcsapi_wifi_get_interworking_rpcdata *__req, qcsapi_wifi_get_interworking_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * interworking = (__req->interworking == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_interworking(ifname, interworking);

	if (interworking) {
		__resp->interworking = malloc(sizeof(*__resp->interworking));
		__resp->interworking->data = interworking;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_interworking_remote_1_svc(qcsapi_wifi_set_interworking_rpcdata *__req, qcsapi_wifi_set_interworking_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * interworking = (__req->interworking == NULL) ? NULL : __req->interworking->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_interworking(ifname, interworking);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_80211u_params_remote_1_svc(qcsapi_wifi_get_80211u_params_rpcdata *__req, qcsapi_wifi_get_80211u_params_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * u_param = (__req->u_param == NULL) ? NULL : __req->u_param->data;
	char * p_buffer = (__req->p_buffer == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_80211u_params(ifname, u_param, p_buffer);

	if (p_buffer) {
		__resp->p_buffer = malloc(sizeof(*__resp->p_buffer));
		__resp->p_buffer->data = p_buffer;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_80211u_params_remote_1_svc(qcsapi_wifi_set_80211u_params_rpcdata *__req, qcsapi_wifi_set_80211u_params_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * param = (__req->param == NULL) ? NULL : __req->param->data;
	char * value1 = (__req->value1 == NULL) ? NULL : __req->value1->data;
	char * value2 = (__req->value2 == NULL) ? NULL : __req->value2->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_80211u_params(ifname, param, value1, value2);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_get_nai_realms_remote_1_svc(qcsapi_security_get_nai_realms_rpcdata *__req, qcsapi_security_get_nai_realms_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_value = (__req->p_value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_get_nai_realms(ifname, p_value);

	if (p_value) {
		__resp->p_value = malloc(sizeof(*__resp->p_value));
		__resp->p_value->data = p_value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_add_nai_realm_remote_1_svc(qcsapi_security_add_nai_realm_rpcdata *__req, qcsapi_security_add_nai_realm_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * nai_realm = (__req->nai_realm == NULL) ? NULL : __req->nai_realm->data;
	char * eap_method = (__req->eap_method == NULL) ? NULL : __req->eap_method->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_add_nai_realm(ifname, __req->encoding, nai_realm, eap_method);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_del_nai_realm_remote_1_svc(qcsapi_security_del_nai_realm_rpcdata *__req, qcsapi_security_del_nai_realm_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * nai_realm = (__req->nai_realm == NULL) ? NULL : __req->nai_realm->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_del_nai_realm(ifname, nai_realm);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_get_roaming_consortium_remote_1_svc(qcsapi_security_get_roaming_consortium_rpcdata *__req, qcsapi_security_get_roaming_consortium_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_value = (__req->p_value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_get_roaming_consortium(ifname, p_value);

	if (p_value) {
		__resp->p_value = malloc(sizeof(*__resp->p_value));
		__resp->p_value->data = p_value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_add_roaming_consortium_remote_1_svc(qcsapi_security_add_roaming_consortium_rpcdata *__req, qcsapi_security_add_roaming_consortium_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_value = (__req->p_value == NULL) ? NULL : __req->p_value->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_add_roaming_consortium(ifname, p_value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_del_roaming_consortium_remote_1_svc(qcsapi_security_del_roaming_consortium_rpcdata *__req, qcsapi_security_del_roaming_consortium_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_value = (__req->p_value == NULL) ? NULL : __req->p_value->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_del_roaming_consortium(ifname, p_value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_get_venue_name_remote_1_svc(qcsapi_security_get_venue_name_rpcdata *__req, qcsapi_security_get_venue_name_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_value = (__req->p_value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_get_venue_name(ifname, p_value);

	if (p_value) {
		__resp->p_value = malloc(sizeof(*__resp->p_value));
		__resp->p_value->data = p_value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_add_venue_name_remote_1_svc(qcsapi_security_add_venue_name_rpcdata *__req, qcsapi_security_add_venue_name_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * lang_code = (__req->lang_code == NULL) ? NULL : __req->lang_code->data;
	char * venue_name = (__req->venue_name == NULL) ? NULL : __req->venue_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_add_venue_name(ifname, lang_code, venue_name);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_del_venue_name_remote_1_svc(qcsapi_security_del_venue_name_rpcdata *__req, qcsapi_security_del_venue_name_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * lang_code = (__req->lang_code == NULL) ? NULL : __req->lang_code->data;
	char * venue_name = (__req->venue_name == NULL) ? NULL : __req->venue_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_del_venue_name(ifname, lang_code, venue_name);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_get_oper_friendly_name_remote_1_svc(qcsapi_security_get_oper_friendly_name_rpcdata *__req, qcsapi_security_get_oper_friendly_name_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_value = (__req->p_value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_get_oper_friendly_name(ifname, p_value);

	if (p_value) {
		__resp->p_value = malloc(sizeof(*__resp->p_value));
		__resp->p_value->data = p_value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_add_oper_friendly_name_remote_1_svc(qcsapi_security_add_oper_friendly_name_rpcdata *__req, qcsapi_security_add_oper_friendly_name_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * lang_code = (__req->lang_code == NULL) ? NULL : __req->lang_code->data;
	char * oper_friendly_name = (__req->oper_friendly_name == NULL) ? NULL : __req->oper_friendly_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_add_oper_friendly_name(ifname, lang_code, oper_friendly_name);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_del_oper_friendly_name_remote_1_svc(qcsapi_security_del_oper_friendly_name_rpcdata *__req, qcsapi_security_del_oper_friendly_name_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * lang_code = (__req->lang_code == NULL) ? NULL : __req->lang_code->data;
	char * oper_friendly_name = (__req->oper_friendly_name == NULL) ? NULL : __req->oper_friendly_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_del_oper_friendly_name(ifname, lang_code, oper_friendly_name);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_get_hs20_conn_capab_remote_1_svc(qcsapi_security_get_hs20_conn_capab_rpcdata *__req, qcsapi_security_get_hs20_conn_capab_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_value = (__req->p_value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_get_hs20_conn_capab(ifname, p_value);

	if (p_value) {
		__resp->p_value = malloc(sizeof(*__resp->p_value));
		__resp->p_value->data = p_value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_add_hs20_conn_capab_remote_1_svc(qcsapi_security_add_hs20_conn_capab_rpcdata *__req, qcsapi_security_add_hs20_conn_capab_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * ip_proto = (__req->ip_proto == NULL) ? NULL : __req->ip_proto->data;
	char * port_num = (__req->port_num == NULL) ? NULL : __req->port_num->data;
	char * status = (__req->status == NULL) ? NULL : __req->status->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_add_hs20_conn_capab(ifname, ip_proto, port_num, status);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_del_hs20_conn_capab_remote_1_svc(qcsapi_security_del_hs20_conn_capab_rpcdata *__req, qcsapi_security_del_hs20_conn_capab_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * ip_proto = (__req->ip_proto == NULL) ? NULL : __req->ip_proto->data;
	char * port_num = (__req->port_num == NULL) ? NULL : __req->port_num->data;
	char * status = (__req->status == NULL) ? NULL : __req->status->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_del_hs20_conn_capab(ifname, ip_proto, port_num, status);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_hs20_status_remote_1_svc(qcsapi_wifi_get_hs20_status_rpcdata *__req, qcsapi_wifi_get_hs20_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_hs20 = (__req->p_hs20 == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_hs20_status(ifname, p_hs20);

	if (p_hs20) {
		__resp->p_hs20 = malloc(sizeof(*__resp->p_hs20));
		__resp->p_hs20->data = p_hs20;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_hs20_status_remote_1_svc(qcsapi_wifi_set_hs20_status_rpcdata *__req, qcsapi_wifi_set_hs20_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * hs20_val = (__req->hs20_val == NULL) ? NULL : __req->hs20_val->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_hs20_status(ifname, hs20_val);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_proxy_arp_remote_1_svc(qcsapi_wifi_get_proxy_arp_rpcdata *__req, qcsapi_wifi_get_proxy_arp_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_proxy_arp = (__req->p_proxy_arp == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_proxy_arp(ifname, p_proxy_arp);

	if (p_proxy_arp) {
		__resp->p_proxy_arp = malloc(sizeof(*__resp->p_proxy_arp));
		__resp->p_proxy_arp->data = p_proxy_arp;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_proxy_arp_remote_1_svc(qcsapi_wifi_set_proxy_arp_rpcdata *__req, qcsapi_wifi_set_proxy_arp_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * proxy_arp_val = (__req->proxy_arp_val == NULL) ? NULL : __req->proxy_arp_val->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_proxy_arp(ifname, proxy_arp_val);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_l2_ext_filter_remote_1_svc(qcsapi_wifi_get_l2_ext_filter_rpcdata *__req, qcsapi_wifi_get_l2_ext_filter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * param = (__req->param == NULL) ? NULL : __req->param->data;
	char * value = (__req->value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_l2_ext_filter(ifname, param, value);

	if (value) {
		__resp->value = malloc(sizeof(*__resp->value));
		__resp->value->data = value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_l2_ext_filter_remote_1_svc(qcsapi_wifi_set_l2_ext_filter_rpcdata *__req, qcsapi_wifi_set_l2_ext_filter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * param = (__req->param == NULL) ? NULL : __req->param->data;
	char * value = (__req->value == NULL) ? NULL : __req->value->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_l2_ext_filter(ifname, param, value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_hs20_params_remote_1_svc(qcsapi_wifi_get_hs20_params_rpcdata *__req, qcsapi_wifi_get_hs20_params_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * hs_param = (__req->hs_param == NULL) ? NULL : __req->hs_param->data;
	char * p_buffer = (__req->p_buffer == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_hs20_params(ifname, hs_param, p_buffer);

	if (p_buffer) {
		__resp->p_buffer = malloc(sizeof(*__resp->p_buffer));
		__resp->p_buffer->data = p_buffer;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_hs20_params_remote_1_svc(qcsapi_wifi_set_hs20_params_rpcdata *__req, qcsapi_wifi_set_hs20_params_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * hs_param = (__req->hs_param == NULL) ? NULL : __req->hs_param->data;
	char * value1 = (__req->value1 == NULL) ? NULL : __req->value1->data;
	char * value2 = (__req->value2 == NULL) ? NULL : __req->value2->data;
	char * value3 = (__req->value3 == NULL) ? NULL : __req->value3->data;
	char * value4 = (__req->value4 == NULL) ? NULL : __req->value4->data;
	char * value5 = (__req->value5 == NULL) ? NULL : __req->value5->data;
	char * value6 = (__req->value6 == NULL) ? NULL : __req->value6->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_hs20_params(ifname, hs_param, value1, value2, value3, value4, value5, value6);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_remove_11u_param_remote_1_svc(qcsapi_remove_11u_param_rpcdata *__req, qcsapi_remove_11u_param_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * param = (__req->param == NULL) ? NULL : __req->param->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_remove_11u_param(ifname, param);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_remove_hs20_param_remote_1_svc(qcsapi_remove_hs20_param_rpcdata *__req, qcsapi_remove_hs20_param_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * hs_param = (__req->hs_param == NULL) ? NULL : __req->hs_param->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_remove_hs20_param(ifname, hs_param);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_add_hs20_icon_remote_1_svc(qcsapi_security_add_hs20_icon_rpcdata *__req, qcsapi_security_add_hs20_icon_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * lang_code = (__req->lang_code == NULL) ? NULL : __req->lang_code->data;
	char * icon_type = (__req->icon_type == NULL) ? NULL : __req->icon_type->data;
	char * icon_name = (__req->icon_name == NULL) ? NULL : __req->icon_name->data;
	char * file_path = (__req->file_path == NULL) ? NULL : __req->file_path->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_add_hs20_icon(ifname, __req->icon_width, __req->icon_height, lang_code, icon_type, icon_name, file_path);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_get_hs20_icon_remote_1_svc(qcsapi_security_get_hs20_icon_rpcdata *__req, qcsapi_security_get_hs20_icon_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * value = (__req->value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_get_hs20_icon(ifname, value);

	if (value) {
		__resp->value = malloc(sizeof(*__resp->value));
		__resp->value->data = value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_del_hs20_icon_remote_1_svc(qcsapi_security_del_hs20_icon_rpcdata *__req, qcsapi_security_del_hs20_icon_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * icon_name = (__req->icon_name == NULL) ? NULL : __req->icon_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_del_hs20_icon(ifname, icon_name);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_add_osu_server_uri_remote_1_svc(qcsapi_security_add_osu_server_uri_rpcdata *__req, qcsapi_security_add_osu_server_uri_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * osu_server_uri = (__req->osu_server_uri == NULL) ? NULL : __req->osu_server_uri->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_add_osu_server_uri(ifname, osu_server_uri);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_get_osu_server_uri_remote_1_svc(qcsapi_security_get_osu_server_uri_rpcdata *__req, qcsapi_security_get_osu_server_uri_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * value = (__req->value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_get_osu_server_uri(ifname, value);

	if (value) {
		__resp->value = malloc(sizeof(*__resp->value));
		__resp->value->data = value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_del_osu_server_uri_remote_1_svc(qcsapi_security_del_osu_server_uri_rpcdata *__req, qcsapi_security_del_osu_server_uri_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * osu_server_uri = (__req->osu_server_uri == NULL) ? NULL : __req->osu_server_uri->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_del_osu_server_uri(ifname, osu_server_uri);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_add_osu_server_param_remote_1_svc(qcsapi_security_add_osu_server_param_rpcdata *__req, qcsapi_security_add_osu_server_param_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * osu_server_uri = (__req->osu_server_uri == NULL) ? NULL : __req->osu_server_uri->data;
	char * param = (__req->param == NULL) ? NULL : __req->param->data;
	char * value = (__req->value == NULL) ? NULL : __req->value->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_add_osu_server_param(ifname, osu_server_uri, param, value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_get_osu_server_param_remote_1_svc(qcsapi_security_get_osu_server_param_rpcdata *__req, qcsapi_security_get_osu_server_param_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * osu_server_uri = (__req->osu_server_uri == NULL) ? NULL : __req->osu_server_uri->data;
	char * param = (__req->param == NULL) ? NULL : __req->param->data;
	char * value = (__req->value == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_get_osu_server_param(ifname, osu_server_uri, param, value);

	if (value) {
		__resp->value = malloc(sizeof(*__resp->value));
		__resp->value->data = value;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_security_del_osu_server_param_remote_1_svc(qcsapi_security_del_osu_server_param_rpcdata *__req, qcsapi_security_del_osu_server_param_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * osu_server_uri = (__req->osu_server_uri == NULL) ? NULL : __req->osu_server_uri->data;
	char * param = (__req->param == NULL) ? NULL : __req->param->data;
	char * value = (__req->value == NULL) ? NULL : __req->value->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_security_del_osu_server_param(ifname, osu_server_uri, param, value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_ieee11i_encryption_modes_remote_1_svc(qcsapi_wifi_get_IEEE11i_encryption_modes_rpcdata *__req, qcsapi_wifi_get_IEEE11i_encryption_modes_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * encryption_modes = (__req->encryption_modes == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_IEEE11i_encryption_modes(ifname, encryption_modes);

	if (encryption_modes) {
		__resp->encryption_modes = malloc(sizeof(*__resp->encryption_modes));
		__resp->encryption_modes->data = encryption_modes;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_ieee11i_encryption_modes_remote_1_svc(qcsapi_wifi_set_IEEE11i_encryption_modes_rpcdata *__req, qcsapi_wifi_set_IEEE11i_encryption_modes_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * encryption_modes = (__req->encryption_modes == NULL) ? NULL : __req->encryption_modes->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_IEEE11i_encryption_modes(ifname, encryption_modes);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_ieee11i_authentication_mode_remote_1_svc(qcsapi_wifi_get_IEEE11i_authentication_mode_rpcdata *__req, qcsapi_wifi_get_IEEE11i_authentication_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * authentication_mode = (__req->authentication_mode == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_IEEE11i_authentication_mode(ifname, authentication_mode);

	if (authentication_mode) {
		__resp->authentication_mode = malloc(sizeof(*__resp->authentication_mode));
		__resp->authentication_mode->data = authentication_mode;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_ieee11i_authentication_mode_remote_1_svc(qcsapi_wifi_set_IEEE11i_authentication_mode_rpcdata *__req, qcsapi_wifi_set_IEEE11i_authentication_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * authentication_mode = (__req->authentication_mode == NULL) ? NULL : __req->authentication_mode->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_IEEE11i_authentication_mode(ifname, authentication_mode);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_michael_errcnt_remote_1_svc(qcsapi_wifi_get_michael_errcnt_rpcdata *__req, qcsapi_wifi_get_michael_errcnt_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_michael_errcnt(ifname, __req->errcount);

	__resp->errcount = __rpc_prepare_data(__req->errcount, sizeof(*__resp->errcount));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_pre_shared_key_remote_1_svc(qcsapi_wifi_get_pre_shared_key_rpcdata *__req, qcsapi_wifi_get_pre_shared_key_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * pre_shared_key = (__req->pre_shared_key == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_pre_shared_key(ifname, __req->key_index, pre_shared_key);

	if (pre_shared_key) {
		__resp->pre_shared_key = malloc(sizeof(*__resp->pre_shared_key));
		__resp->pre_shared_key->data = pre_shared_key;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_pre_shared_key_remote_1_svc(qcsapi_wifi_set_pre_shared_key_rpcdata *__req, qcsapi_wifi_set_pre_shared_key_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * pre_shared_key = (__req->pre_shared_key == NULL) ? NULL : __req->pre_shared_key->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_pre_shared_key(ifname, __req->key_index, pre_shared_key);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_add_radius_auth_server_cfg_remote_1_svc(qcsapi_wifi_add_radius_auth_server_cfg_rpcdata *__req, qcsapi_wifi_add_radius_auth_server_cfg_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * radius_auth_server_ipaddr = (__req->radius_auth_server_ipaddr == NULL) ? NULL : __req->radius_auth_server_ipaddr->data;
	char * radius_auth_server_port = (__req->radius_auth_server_port == NULL) ? NULL : __req->radius_auth_server_port->data;
	char * radius_auth_server_sh_key = (__req->radius_auth_server_sh_key == NULL) ? NULL : __req->radius_auth_server_sh_key->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_add_radius_auth_server_cfg(ifname, radius_auth_server_ipaddr, radius_auth_server_port, radius_auth_server_sh_key);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_del_radius_auth_server_cfg_remote_1_svc(qcsapi_wifi_del_radius_auth_server_cfg_rpcdata *__req, qcsapi_wifi_del_radius_auth_server_cfg_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * radius_auth_server_ipaddr = (__req->radius_auth_server_ipaddr == NULL) ? NULL : __req->radius_auth_server_ipaddr->data;
	char * constp_radius_port = (__req->constp_radius_port == NULL) ? NULL : __req->constp_radius_port->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_del_radius_auth_server_cfg(ifname, radius_auth_server_ipaddr, constp_radius_port);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_radius_auth_server_cfg_remote_1_svc(qcsapi_wifi_get_radius_auth_server_cfg_rpcdata *__req, qcsapi_wifi_get_radius_auth_server_cfg_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * radius_auth_server_cfg = (__req->radius_auth_server_cfg == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_radius_auth_server_cfg(ifname, radius_auth_server_cfg);

	if (radius_auth_server_cfg) {
		__resp->radius_auth_server_cfg = malloc(sizeof(*__resp->radius_auth_server_cfg));
		__resp->radius_auth_server_cfg->data = radius_auth_server_cfg;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_own_ip_addr_remote_1_svc(qcsapi_wifi_set_own_ip_addr_rpcdata *__req, qcsapi_wifi_set_own_ip_addr_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * own_ip_addr = (__req->own_ip_addr == NULL) ? NULL : __req->own_ip_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_own_ip_addr(ifname, own_ip_addr);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_key_passphrase_remote_1_svc(qcsapi_wifi_get_key_passphrase_rpcdata *__req, qcsapi_wifi_get_key_passphrase_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * passphrase = (__req->passphrase == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_key_passphrase(ifname, __req->key_index, passphrase);

	if (passphrase) {
		__resp->passphrase = malloc(sizeof(*__resp->passphrase));
		__resp->passphrase->data = passphrase;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_key_passphrase_remote_1_svc(qcsapi_wifi_set_key_passphrase_rpcdata *__req, qcsapi_wifi_set_key_passphrase_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * passphrase = (__req->passphrase == NULL) ? NULL : __req->passphrase->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_key_passphrase(ifname, __req->key_index, passphrase);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_group_key_interval_remote_1_svc(qcsapi_wifi_get_group_key_interval_rpcdata *__req, qcsapi_wifi_get_group_key_interval_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_group_key_interval(ifname, __req->p_key_interval);

	__resp->p_key_interval = __rpc_prepare_data(__req->p_key_interval, sizeof(*__resp->p_key_interval));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_pairwise_key_interval_remote_1_svc(qcsapi_wifi_get_pairwise_key_interval_rpcdata *__req, qcsapi_wifi_get_pairwise_key_interval_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_pairwise_key_interval(ifname, __req->p_key_interval);

	__resp->p_key_interval = __rpc_prepare_data(__req->p_key_interval, sizeof(*__resp->p_key_interval));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_group_key_interval_remote_1_svc(qcsapi_wifi_set_group_key_interval_rpcdata *__req, qcsapi_wifi_set_group_key_interval_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_group_key_interval(ifname, __req->key_interval);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_pairwise_key_interval_remote_1_svc(qcsapi_wifi_set_pairwise_key_interval_rpcdata *__req, qcsapi_wifi_set_pairwise_key_interval_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_pairwise_key_interval(ifname, __req->key_interval);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_pmf_remote_1_svc(qcsapi_wifi_get_pmf_rpcdata *__req, qcsapi_wifi_get_pmf_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_pmf(ifname, __req->p_pmf_cap);

	__resp->p_pmf_cap = __rpc_prepare_data(__req->p_pmf_cap, sizeof(*__resp->p_pmf_cap));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_pmf_remote_1_svc(qcsapi_wifi_set_pmf_rpcdata *__req, qcsapi_wifi_set_pmf_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_pmf(ifname, __req->pmf_cap);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_wpa_status_remote_1_svc(qcsapi_wifi_get_wpa_status_rpcdata *__req, qcsapi_wifi_get_wpa_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * wpa_status = (__req->wpa_status == NULL) ? NULL : arg_alloc();
	char * mac_addr = (__req->mac_addr == NULL) ? NULL : __req->mac_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_wpa_status(ifname, wpa_status, mac_addr, __req->max_len);

	if (wpa_status) {
		__resp->wpa_status = malloc(sizeof(*__resp->wpa_status));
		__resp->wpa_status->data = wpa_status;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_psk_auth_failures_remote_1_svc(qcsapi_wifi_get_psk_auth_failures_rpcdata *__req, qcsapi_wifi_get_psk_auth_failures_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_psk_auth_failures(ifname, __req->count);

	__resp->count = __rpc_prepare_data(__req->count, sizeof(*__resp->count));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_auth_state_remote_1_svc(qcsapi_wifi_get_auth_state_rpcdata *__req, qcsapi_wifi_get_auth_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * mac_addr = (__req->mac_addr == NULL) ? NULL : __req->mac_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_auth_state(ifname, mac_addr, __req->auth_state);

	__resp->auth_state = __rpc_prepare_data(__req->auth_state, sizeof(*__resp->auth_state));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_security_defer_mode_remote_1_svc(qcsapi_wifi_set_security_defer_mode_rpcdata *__req, qcsapi_wifi_set_security_defer_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_security_defer_mode(ifname, __req->defer);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_security_defer_mode_remote_1_svc(qcsapi_wifi_get_security_defer_mode_rpcdata *__req, qcsapi_wifi_get_security_defer_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_security_defer_mode(ifname, __req->defer);

	__resp->defer = __rpc_prepare_data(__req->defer, sizeof(*__resp->defer));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_apply_security_config_remote_1_svc(qcsapi_wifi_apply_security_config_rpcdata *__req, qcsapi_wifi_apply_security_config_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_apply_security_config(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_mac_address_filtering_remote_1_svc(qcsapi_wifi_set_mac_address_filtering_rpcdata *__req, qcsapi_wifi_set_mac_address_filtering_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	const qcsapi_mac_address_filtering new_mac_address_filtering=(const qcsapi_mac_address_filtering)__req->new_mac_address_filtering;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_mac_address_filtering(ifname, new_mac_address_filtering);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_mac_address_filtering_remote_1_svc(qcsapi_wifi_get_mac_address_filtering_rpcdata *__req, qcsapi_wifi_get_mac_address_filtering_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_mac_address_filtering * current_mac_address_filtering=(qcsapi_mac_address_filtering *)__req->current_mac_address_filtering;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_mac_address_filtering(ifname, current_mac_address_filtering);

	__resp->current_mac_address_filtering = __rpc_prepare_data(__req->current_mac_address_filtering, sizeof(*__resp->current_mac_address_filtering));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_authorize_mac_address_remote_1_svc(qcsapi_wifi_authorize_mac_address_rpcdata *__req, qcsapi_wifi_authorize_mac_address_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * address_to_authorize = (__req->address_to_authorize == NULL) ? NULL : __req->address_to_authorize->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_authorize_mac_address(ifname, address_to_authorize);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_authorize_mac_address_list_remote_1_svc(qcsapi_wifi_authorize_mac_address_list_rpcdata *__req, qcsapi_wifi_authorize_mac_address_list_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * address_list_to_authorize = (__req->address_list_to_authorize == NULL) ? NULL : __req->address_list_to_authorize->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_authorize_mac_address_list(ifname, __req->num, address_list_to_authorize);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_deny_mac_address_remote_1_svc(qcsapi_wifi_deny_mac_address_rpcdata *__req, qcsapi_wifi_deny_mac_address_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * address_to_deny = (__req->address_to_deny == NULL) ? NULL : __req->address_to_deny->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_deny_mac_address(ifname, address_to_deny);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_deny_mac_address_list_remote_1_svc(qcsapi_wifi_deny_mac_address_list_rpcdata *__req, qcsapi_wifi_deny_mac_address_list_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * address_list_to_deny = (__req->address_list_to_deny == NULL) ? NULL : __req->address_list_to_deny->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_deny_mac_address_list(ifname, __req->num, address_list_to_deny);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_remove_mac_address_remote_1_svc(qcsapi_wifi_remove_mac_address_rpcdata *__req, qcsapi_wifi_remove_mac_address_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * address_to_remove = (__req->address_to_remove == NULL) ? NULL : __req->address_to_remove->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_remove_mac_address(ifname, address_to_remove);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_remove_mac_address_list_remote_1_svc(qcsapi_wifi_remove_mac_address_list_rpcdata *__req, qcsapi_wifi_remove_mac_address_list_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * address_list_to_remove = (__req->address_list_to_remove == NULL) ? NULL : __req->address_list_to_remove->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_remove_mac_address_list(ifname, __req->num, address_list_to_remove);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_is_mac_address_authorized_remote_1_svc(qcsapi_wifi_is_mac_address_authorized_rpcdata *__req, qcsapi_wifi_is_mac_address_authorized_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * address_to_verify = (__req->address_to_verify == NULL) ? NULL : __req->address_to_verify->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_is_mac_address_authorized(ifname, address_to_verify, __req->p_mac_address_authorized);

	__resp->p_mac_address_authorized = __rpc_prepare_data(__req->p_mac_address_authorized, sizeof(*__resp->p_mac_address_authorized));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_authorized_mac_addresses_remote_1_svc(qcsapi_wifi_get_authorized_mac_addresses_rpcdata *__req, qcsapi_wifi_get_authorized_mac_addresses_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * list_mac_addresses = (__req->list_mac_addresses == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_authorized_mac_addresses(ifname, list_mac_addresses, __req->sizeof_list);

	if (list_mac_addresses) {
		__resp->list_mac_addresses = malloc(sizeof(*__resp->list_mac_addresses));
		__resp->list_mac_addresses->data = list_mac_addresses;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_denied_mac_addresses_remote_1_svc(qcsapi_wifi_get_denied_mac_addresses_rpcdata *__req, qcsapi_wifi_get_denied_mac_addresses_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * list_mac_addresses = (__req->list_mac_addresses == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_denied_mac_addresses(ifname, list_mac_addresses, __req->sizeof_list);

	if (list_mac_addresses) {
		__resp->list_mac_addresses = malloc(sizeof(*__resp->list_mac_addresses));
		__resp->list_mac_addresses->data = list_mac_addresses;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_accept_oui_filter_remote_1_svc(qcsapi_wifi_set_accept_oui_filter_rpcdata *__req, qcsapi_wifi_set_accept_oui_filter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * oui = (__req->oui == NULL) ? NULL : __req->oui->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_accept_oui_filter(ifname, oui, __req->flag);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_accept_oui_filter_remote_1_svc(qcsapi_wifi_get_accept_oui_filter_rpcdata *__req, qcsapi_wifi_get_accept_oui_filter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * oui_list = (__req->oui_list == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_accept_oui_filter(ifname, oui_list, __req->sizeof_list);

	if (oui_list) {
		__resp->oui_list = malloc(sizeof(*__resp->oui_list));
		__resp->oui_list->data = oui_list;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_clear_mac_address_filters_remote_1_svc(qcsapi_wifi_clear_mac_address_filters_rpcdata *__req, qcsapi_wifi_clear_mac_address_filters_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_clear_mac_address_filters(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_authorize_mac_address_list_ext_remote_1_svc(qcsapi_wifi_authorize_mac_address_list_ext_rpcdata *__req, qcsapi_wifi_authorize_mac_address_list_ext_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_authorize_mac_address_list_ext(ifname, (struct qcsapi_mac_list *)__req->auth_mac_list);

	__resp->auth_mac_list = __rpc_prepare_data(__req->auth_mac_list, sizeof(*__resp->auth_mac_list));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_deny_mac_address_list_ext_remote_1_svc(qcsapi_wifi_deny_mac_address_list_ext_rpcdata *__req, qcsapi_wifi_deny_mac_address_list_ext_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_deny_mac_address_list_ext(ifname, (struct qcsapi_mac_list *)__req->deny_mac_list);

	__resp->deny_mac_list = __rpc_prepare_data(__req->deny_mac_list, sizeof(*__resp->deny_mac_list));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_remove_mac_address_list_ext_remote_1_svc(qcsapi_wifi_remove_mac_address_list_ext_rpcdata *__req, qcsapi_wifi_remove_mac_address_list_ext_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_remove_mac_address_list_ext(ifname, (struct qcsapi_mac_list *)__req->remove_mac_list);

	__resp->remove_mac_list = __rpc_prepare_data(__req->remove_mac_list, sizeof(*__resp->remove_mac_list));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_mac_address_reserve_remote_1_svc(qcsapi_wifi_set_mac_address_reserve_rpcdata *__req, qcsapi_wifi_set_mac_address_reserve_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * addr = (__req->addr == NULL) ? NULL : __req->addr->data;
	char * mask = (__req->mask == NULL) ? NULL : __req->mask->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_mac_address_reserve(ifname, addr, mask);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_mac_address_reserve_remote_1_svc(qcsapi_wifi_get_mac_address_reserve_rpcdata *__req, qcsapi_wifi_get_mac_address_reserve_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * buf = (__req->buf == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_mac_address_reserve(ifname, buf);

	if (buf) {
		__resp->buf = malloc(sizeof(*__resp->buf));
		__resp->buf->data = buf;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_clear_mac_address_reserve_remote_1_svc(qcsapi_wifi_clear_mac_address_reserve_rpcdata *__req, qcsapi_wifi_clear_mac_address_reserve_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_clear_mac_address_reserve(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_option_remote_1_svc(qcsapi_wifi_get_option_rpcdata *__req, qcsapi_wifi_get_option_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_option_type qcsapi_option=(qcsapi_option_type)__req->qcsapi_option;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_option(ifname, qcsapi_option, __req->p_current_option);

	__resp->p_current_option = __rpc_prepare_data(__req->p_current_option, sizeof(*__resp->p_current_option));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_option_remote_1_svc(qcsapi_wifi_set_option_rpcdata *__req, qcsapi_wifi_set_option_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_option_type qcsapi_option=(qcsapi_option_type)__req->qcsapi_option;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_option(ifname, qcsapi_option, __req->new_option);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_board_parameter_remote_1_svc(qcsapi_get_board_parameter_rpcdata *__req, qcsapi_get_board_parameter_rpcdata *__resp, struct svc_req *rqstp)
{
	qcsapi_board_parameter_type board_param=(qcsapi_board_parameter_type)__req->board_param;
	char * p_buffer = (__req->p_buffer == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_board_parameter(board_param, p_buffer);

	if (p_buffer) {
		__resp->p_buffer = malloc(sizeof(*__resp->p_buffer));
		__resp->p_buffer->data = p_buffer;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_swfeat_list_remote_1_svc(qcsapi_get_swfeat_list_rpcdata *__req, qcsapi_get_swfeat_list_rpcdata *__resp, struct svc_req *rqstp)
{
	char * p_buffer = (__req->p_buffer == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_swfeat_list(p_buffer);

	if (p_buffer) {
		__resp->p_buffer = malloc(sizeof(*__resp->p_buffer));
		__resp->p_buffer->data = p_buffer;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_parameter_remote_1_svc(qcsapi_wifi_get_parameter_rpcdata *__req, qcsapi_wifi_get_parameter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_wifi_param_type type=(qcsapi_wifi_param_type)__req->type;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_parameter(ifname, type, __req->p_value);

	__resp->p_value = __rpc_prepare_data(__req->p_value, sizeof(*__resp->p_value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_parameter_remote_1_svc(qcsapi_wifi_set_parameter_rpcdata *__req, qcsapi_wifi_set_parameter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_wifi_param_type type=(qcsapi_wifi_param_type)__req->type;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_parameter(ifname, type, __req->value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_create_ssid_remote_1_svc(qcsapi_SSID_create_SSID_rpcdata *__req, qcsapi_SSID_create_SSID_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * new_SSID = (__req->new_SSID == NULL) ? NULL : __req->new_SSID->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_create_SSID(ifname, new_SSID);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_remove_ssid_remote_1_svc(qcsapi_SSID_remove_SSID_rpcdata *__req, qcsapi_SSID_remove_SSID_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * del_SSID = (__req->del_SSID == NULL) ? NULL : __req->del_SSID->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_remove_SSID(ifname, del_SSID);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_verify_ssid_remote_1_svc(qcsapi_SSID_verify_SSID_rpcdata *__req, qcsapi_SSID_verify_SSID_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_verify_SSID(ifname, current_SSID);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_rename_ssid_remote_1_svc(qcsapi_SSID_rename_SSID_rpcdata *__req, qcsapi_SSID_rename_SSID_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * new_SSID = (__req->new_SSID == NULL) ? NULL : __req->new_SSID->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_rename_SSID(ifname, current_SSID, new_SSID);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_get_ssid_list_remote_1_svc(qcsapi_SSID_get_SSID_list_rpcdata *__req, qcsapi_SSID_get_SSID_list_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	unsigned int i;
	char **list_SSID = malloc(sizeof(char *) * __req->arrayc);
	for (i = 0; i < __req->arrayc && list_SSID; i++) {
		list_SSID[i] = arg_alloc();
	}

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_get_SSID_list(ifname, __req->arrayc, list_SSID);

	__resp->list_SSID.list_SSID_val = list_SSID;
	__resp->list_SSID.list_SSID_len = __req->arrayc;
	for (i = 0; i < __req->arrayc; i++) {
		if (list_SSID[i][0] == '\0') {
			__resp->list_SSID.list_SSID_len = i;
			break;
		}
	}
	for (i = __resp->list_SSID.list_SSID_len; i < __req->arrayc; i++) {
		arg_free(list_SSID[i]);
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_set_protocol_remote_1_svc(qcsapi_SSID_set_protocol_rpcdata *__req, qcsapi_SSID_set_protocol_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * new_protocol = (__req->new_protocol == NULL) ? NULL : __req->new_protocol->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_set_protocol(ifname, current_SSID, new_protocol);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_get_protocol_remote_1_svc(qcsapi_SSID_get_protocol_rpcdata *__req, qcsapi_SSID_get_protocol_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * current_protocol = (__req->current_protocol == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_get_protocol(ifname, current_SSID, current_protocol);

	if (current_protocol) {
		__resp->current_protocol = malloc(sizeof(*__resp->current_protocol));
		__resp->current_protocol->data = current_protocol;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_get_encryption_modes_remote_1_svc(qcsapi_SSID_get_encryption_modes_rpcdata *__req, qcsapi_SSID_get_encryption_modes_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * encryption_modes = (__req->encryption_modes == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_get_encryption_modes(ifname, current_SSID, encryption_modes);

	if (encryption_modes) {
		__resp->encryption_modes = malloc(sizeof(*__resp->encryption_modes));
		__resp->encryption_modes->data = encryption_modes;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_set_encryption_modes_remote_1_svc(qcsapi_SSID_set_encryption_modes_rpcdata *__req, qcsapi_SSID_set_encryption_modes_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * encryption_modes = (__req->encryption_modes == NULL) ? NULL : __req->encryption_modes->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_set_encryption_modes(ifname, current_SSID, encryption_modes);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_get_group_encryption_remote_1_svc(qcsapi_SSID_get_group_encryption_rpcdata *__req, qcsapi_SSID_get_group_encryption_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * encryption_mode = (__req->encryption_mode == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_get_group_encryption(ifname, current_SSID, encryption_mode);

	if (encryption_mode) {
		__resp->encryption_mode = malloc(sizeof(*__resp->encryption_mode));
		__resp->encryption_mode->data = encryption_mode;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_set_group_encryption_remote_1_svc(qcsapi_SSID_set_group_encryption_rpcdata *__req, qcsapi_SSID_set_group_encryption_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * encryption_mode = (__req->encryption_mode == NULL) ? NULL : __req->encryption_mode->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_set_group_encryption(ifname, current_SSID, encryption_mode);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_get_authentication_mode_remote_1_svc(qcsapi_SSID_get_authentication_mode_rpcdata *__req, qcsapi_SSID_get_authentication_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * authentication_mode = (__req->authentication_mode == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_get_authentication_mode(ifname, current_SSID, authentication_mode);

	if (authentication_mode) {
		__resp->authentication_mode = malloc(sizeof(*__resp->authentication_mode));
		__resp->authentication_mode->data = authentication_mode;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_set_authentication_mode_remote_1_svc(qcsapi_SSID_set_authentication_mode_rpcdata *__req, qcsapi_SSID_set_authentication_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * authentication_mode = (__req->authentication_mode == NULL) ? NULL : __req->authentication_mode->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_set_authentication_mode(ifname, current_SSID, authentication_mode);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_get_pre_shared_key_remote_1_svc(qcsapi_SSID_get_pre_shared_key_rpcdata *__req, qcsapi_SSID_get_pre_shared_key_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * pre_shared_key = (__req->pre_shared_key == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_get_pre_shared_key(ifname, current_SSID, __req->key_index, pre_shared_key);

	if (pre_shared_key) {
		__resp->pre_shared_key = malloc(sizeof(*__resp->pre_shared_key));
		__resp->pre_shared_key->data = pre_shared_key;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_set_pre_shared_key_remote_1_svc(qcsapi_SSID_set_pre_shared_key_rpcdata *__req, qcsapi_SSID_set_pre_shared_key_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * pre_shared_key = (__req->pre_shared_key == NULL) ? NULL : __req->pre_shared_key->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_set_pre_shared_key(ifname, current_SSID, __req->key_index, pre_shared_key);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_get_key_passphrase_remote_1_svc(qcsapi_SSID_get_key_passphrase_rpcdata *__req, qcsapi_SSID_get_key_passphrase_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * passphrase = (__req->passphrase == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_get_key_passphrase(ifname, current_SSID, __req->key_index, passphrase);

	if (passphrase) {
		__resp->passphrase = malloc(sizeof(*__resp->passphrase));
		__resp->passphrase->data = passphrase;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_set_key_passphrase_remote_1_svc(qcsapi_SSID_set_key_passphrase_rpcdata *__req, qcsapi_SSID_set_key_passphrase_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;
	char * passphrase = (__req->passphrase == NULL) ? NULL : __req->passphrase->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_set_key_passphrase(ifname, current_SSID, __req->key_index, passphrase);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_update_bss_cfg_remote_1_svc(qcsapi_wifi_update_bss_cfg_rpcdata *__req, qcsapi_wifi_update_bss_cfg_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	const qcsapi_wifi_mode wifi_mode=(const qcsapi_wifi_mode)__req->wifi_mode;
	char * ssid = (__req->ssid == NULL) ? NULL : __req->ssid->data;
	char * param_name = (__req->param_name == NULL) ? NULL : __req->param_name->data;
	char * param_value = (__req->param_value == NULL) ? NULL : __req->param_value->data;
	char * param_type = (__req->param_type == NULL) ? NULL : __req->param_type->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_update_bss_cfg(ifname, wifi_mode, ssid, param_name, param_value, param_type);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_get_pmf_remote_1_svc(qcsapi_SSID_get_pmf_rpcdata *__req, qcsapi_SSID_get_pmf_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_SSID = (__req->current_SSID == NULL) ? NULL : __req->current_SSID->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_get_pmf(ifname, current_SSID, __req->p_pmf_cap);

	__resp->p_pmf_cap = __rpc_prepare_data(__req->p_pmf_cap, sizeof(*__resp->p_pmf_cap));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_set_pmf_remote_1_svc(qcsapi_SSID_set_pmf_rpcdata *__req, qcsapi_SSID_set_pmf_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * SSID_str = (__req->SSID_str == NULL) ? NULL : __req->SSID_str->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_set_pmf(ifname, SSID_str, __req->pmf_cap);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_ssid_get_wps_ssid_remote_1_svc(qcsapi_SSID_get_wps_SSID_rpcdata *__req, qcsapi_SSID_get_wps_SSID_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * wps_SSID = (__req->wps_SSID == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_SSID_get_wps_SSID(ifname, wps_SSID);

	if (wps_SSID) {
		__resp->wps_SSID = malloc(sizeof(*__resp->wps_SSID));
		__resp->wps_SSID->data = wps_SSID;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_vlan_config_remote_1_svc(qcsapi_wifi_vlan_config_rpcdata *__req, qcsapi_wifi_vlan_config_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_vlan_cmd cmd=(qcsapi_vlan_cmd)__req->cmd;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_vlan_config(ifname, cmd, __req->vlanid);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_show_vlan_config_remote_1_svc(qcsapi_wifi_show_vlan_config_rpcdata *__req, qcsapi_wifi_show_vlan_config_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * flag = (__req->flag == NULL) ? NULL : __req->flag->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_show_vlan_config(ifname, (struct qcsapi_data_2Kbytes *)__req->vcfg, flag);

	__resp->vcfg = __rpc_prepare_data(__req->vcfg, sizeof(*__resp->vcfg));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_vlan_promisc_remote_1_svc(qcsapi_wifi_set_vlan_promisc_rpcdata *__req, qcsapi_wifi_set_vlan_promisc_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_vlan_promisc(__req->enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_registrar_report_button_press_remote_1_svc(qcsapi_wps_registrar_report_button_press_rpcdata *__req, qcsapi_wps_registrar_report_button_press_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_registrar_report_button_press(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_registrar_report_pin_remote_1_svc(qcsapi_wps_registrar_report_pin_rpcdata *__req, qcsapi_wps_registrar_report_pin_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * wps_pin = (__req->wps_pin == NULL) ? NULL : __req->wps_pin->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_registrar_report_pin(ifname, wps_pin);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_registrar_get_pp_devname_remote_1_svc(qcsapi_wps_registrar_get_pp_devname_rpcdata *__req, qcsapi_wps_registrar_get_pp_devname_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * pp_devname = (__req->pp_devname == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_registrar_get_pp_devname(ifname, __req->blacklist, pp_devname);

	if (pp_devname) {
		__resp->pp_devname = malloc(sizeof(*__resp->pp_devname));
		__resp->pp_devname->data = pp_devname;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_registrar_set_pp_devname_remote_1_svc(qcsapi_wps_registrar_set_pp_devname_rpcdata *__req, qcsapi_wps_registrar_set_pp_devname_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * pp_devname = (__req->pp_devname == NULL) ? NULL : __req->pp_devname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_registrar_set_pp_devname(ifname, __req->update_blacklist, pp_devname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_enrollee_report_button_press_remote_1_svc(qcsapi_wps_enrollee_report_button_press_rpcdata *__req, qcsapi_wps_enrollee_report_button_press_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * bssid = (__req->bssid == NULL) ? NULL : __req->bssid->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_enrollee_report_button_press(ifname, bssid);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_enrollee_report_pin_remote_1_svc(qcsapi_wps_enrollee_report_pin_rpcdata *__req, qcsapi_wps_enrollee_report_pin_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * bssid = (__req->bssid == NULL) ? NULL : __req->bssid->data;
	char * wps_pin = (__req->wps_pin == NULL) ? NULL : __req->wps_pin->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_enrollee_report_pin(ifname, bssid, wps_pin);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_enrollee_generate_pin_remote_1_svc(qcsapi_wps_enrollee_generate_pin_rpcdata *__req, qcsapi_wps_enrollee_generate_pin_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * bssid = (__req->bssid == NULL) ? NULL : __req->bssid->data;
	char * wps_pin = (__req->wps_pin == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_enrollee_generate_pin(ifname, bssid, wps_pin);

	if (wps_pin) {
		__resp->wps_pin = malloc(sizeof(*__resp->wps_pin));
		__resp->wps_pin->data = wps_pin;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_get_ap_pin_remote_1_svc(qcsapi_wps_get_ap_pin_rpcdata *__req, qcsapi_wps_get_ap_pin_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * wps_pin = (__req->wps_pin == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_get_ap_pin(ifname, wps_pin, __req->force_regenerate);

	if (wps_pin) {
		__resp->wps_pin = malloc(sizeof(*__resp->wps_pin));
		__resp->wps_pin->data = wps_pin;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_set_ap_pin_remote_1_svc(qcsapi_wps_set_ap_pin_rpcdata *__req, qcsapi_wps_set_ap_pin_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * wps_pin = (__req->wps_pin == NULL) ? NULL : __req->wps_pin->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_set_ap_pin(ifname, wps_pin);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_save_ap_pin_remote_1_svc(qcsapi_wps_save_ap_pin_rpcdata *__req, qcsapi_wps_save_ap_pin_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_save_ap_pin(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_enable_ap_pin_remote_1_svc(qcsapi_wps_enable_ap_pin_rpcdata *__req, qcsapi_wps_enable_ap_pin_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_enable_ap_pin(ifname, __req->enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_get_sta_pin_remote_1_svc(qcsapi_wps_get_sta_pin_rpcdata *__req, qcsapi_wps_get_sta_pin_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * wps_pin = (__req->wps_pin == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_get_sta_pin(ifname, wps_pin);

	if (wps_pin) {
		__resp->wps_pin = malloc(sizeof(*__resp->wps_pin));
		__resp->wps_pin->data = wps_pin;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_get_state_remote_1_svc(qcsapi_wps_get_state_rpcdata *__req, qcsapi_wps_get_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * wps_state = (__req->wps_state == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_get_state(ifname, wps_state, __req->max_len);

	if (wps_state) {
		__resp->wps_state = malloc(sizeof(*__resp->wps_state));
		__resp->wps_state->data = wps_state;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_get_configured_state_remote_1_svc(qcsapi_wps_get_configured_state_rpcdata *__req, qcsapi_wps_get_configured_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * wps_state = (__req->wps_state == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_get_configured_state(ifname, wps_state, __req->max_len);

	if (wps_state) {
		__resp->wps_state = malloc(sizeof(*__resp->wps_state));
		__resp->wps_state->data = wps_state;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_get_runtime_state_remote_1_svc(qcsapi_wps_get_runtime_state_rpcdata *__req, qcsapi_wps_get_runtime_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * state = (__req->state == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_get_runtime_state(ifname, state, __req->max_len);

	if (state) {
		__resp->state = malloc(sizeof(*__resp->state));
		__resp->state->data = state;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_set_configured_state_remote_1_svc(qcsapi_wps_set_configured_state_rpcdata *__req, qcsapi_wps_set_configured_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_set_configured_state(ifname, __req->state);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_get_param_remote_1_svc(qcsapi_wps_get_param_rpcdata *__req, qcsapi_wps_get_param_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_wps_param_type wps_type=(qcsapi_wps_param_type)__req->wps_type;
	char * wps_str = (__req->wps_str == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_get_param(ifname, wps_type, wps_str, __req->max_len);

	if (wps_str) {
		__resp->wps_str = malloc(sizeof(*__resp->wps_str));
		__resp->wps_str->data = wps_str;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_set_timeout_remote_1_svc(qcsapi_wps_set_timeout_rpcdata *__req, qcsapi_wps_set_timeout_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_set_timeout(ifname, __req->value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_on_hidden_ssid_remote_1_svc(qcsapi_wps_on_hidden_ssid_rpcdata *__req, qcsapi_wps_on_hidden_ssid_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_on_hidden_ssid(ifname, __req->value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_on_hidden_ssid_status_remote_1_svc(qcsapi_wps_on_hidden_ssid_status_rpcdata *__req, qcsapi_wps_on_hidden_ssid_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * state = (__req->state == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_on_hidden_ssid_status(ifname, state, __req->max_len);

	if (state) {
		__resp->state = malloc(sizeof(*__resp->state));
		__resp->state->data = state;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_upnp_enable_remote_1_svc(qcsapi_wps_upnp_enable_rpcdata *__req, qcsapi_wps_upnp_enable_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_upnp_enable(ifname, __req->value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_upnp_status_remote_1_svc(qcsapi_wps_upnp_status_rpcdata *__req, qcsapi_wps_upnp_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * reply = (__req->reply == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_upnp_status(ifname, reply, __req->reply_len);

	if (reply) {
		__resp->reply = malloc(sizeof(*__resp->reply));
		__resp->reply->data = reply;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_allow_pbc_overlap_remote_1_svc(qcsapi_wps_allow_pbc_overlap_rpcdata *__req, qcsapi_wps_allow_pbc_overlap_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_allow_pbc_overlap(ifname, __req->allow);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_get_allow_pbc_overlap_status_remote_1_svc(qcsapi_wps_get_allow_pbc_overlap_status_rpcdata *__req, qcsapi_wps_get_allow_pbc_overlap_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_get_allow_pbc_overlap_status(ifname, __req->status);

	__resp->status = __rpc_prepare_data(__req->status, sizeof(*__resp->status));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_set_access_control_remote_1_svc(qcsapi_wps_set_access_control_rpcdata *__req, qcsapi_wps_set_access_control_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_set_access_control(ifname, __req->ctrl_state);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_get_access_control_remote_1_svc(qcsapi_wps_get_access_control_rpcdata *__req, qcsapi_wps_get_access_control_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_get_access_control(ifname, __req->ctrl_state);

	__resp->ctrl_state = __rpc_prepare_data(__req->ctrl_state, sizeof(*__resp->ctrl_state));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_set_param_remote_1_svc(qcsapi_wps_set_param_rpcdata *__req, qcsapi_wps_set_param_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	const qcsapi_wps_param_type param_type=(const qcsapi_wps_param_type)__req->param_type;
	char * param_value = (__req->param_value == NULL) ? NULL : __req->param_value->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_set_param(ifname, param_type, param_value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_cancel_remote_1_svc(qcsapi_wps_cancel_rpcdata *__req, qcsapi_wps_cancel_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_cancel(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_set_pbc_in_srcm_remote_1_svc(qcsapi_wps_set_pbc_in_srcm_rpcdata *__req, qcsapi_wps_set_pbc_in_srcm_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_set_pbc_in_srcm(ifname, __req->enabled);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_get_pbc_in_srcm_remote_1_svc(qcsapi_wps_get_pbc_in_srcm_rpcdata *__req, qcsapi_wps_get_pbc_in_srcm_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_get_pbc_in_srcm(ifname, __req->p_enabled);

	__resp->p_enabled = __rpc_prepare_data(__req->p_enabled, sizeof(*__resp->p_enabled));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_registrar_set_default_pbc_bss_remote_1_svc(qcsapi_registrar_set_default_pbc_bss_rpcdata *__req, qcsapi_registrar_set_default_pbc_bss_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_registrar_set_default_pbc_bss(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_registrar_get_default_pbc_bss_remote_1_svc(qcsapi_registrar_get_default_pbc_bss_rpcdata *__req, qcsapi_registrar_get_default_pbc_bss_rpcdata *__resp, struct svc_req *rqstp)
{
	char * default_bss = (__req->default_bss == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_registrar_get_default_pbc_bss(default_bss, __req->len);

	if (default_bss) {
		__resp->default_bss = malloc(sizeof(*__resp->default_bss));
		__resp->default_bss->data = default_bss;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_set_default_pbc_bss_remote_1_svc(qcsapi_wps_set_default_pbc_bss_rpcdata *__req, qcsapi_wps_set_default_pbc_bss_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_set_default_pbc_bss(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wps_get_default_pbc_bss_remote_1_svc(qcsapi_wps_get_default_pbc_bss_rpcdata *__req, qcsapi_wps_get_default_pbc_bss_rpcdata *__resp, struct svc_req *rqstp)
{
	char * default_bss = (__req->default_bss == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wps_get_default_pbc_bss(default_bss, __req->len);

	if (default_bss) {
		__resp->default_bss = malloc(sizeof(*__resp->default_bss));
		__resp->default_bss->data = default_bss;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_gpio_set_config_remote_1_svc(qcsapi_gpio_set_config_rpcdata *__req, qcsapi_gpio_set_config_rpcdata *__resp, struct svc_req *rqstp)
{
	const qcsapi_gpio_config new_gpio_config=(const qcsapi_gpio_config)__req->new_gpio_config;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_gpio_set_config(__req->gpio_pin, new_gpio_config);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_gpio_get_config_remote_1_svc(qcsapi_gpio_get_config_rpcdata *__req, qcsapi_gpio_get_config_rpcdata *__resp, struct svc_req *rqstp)
{
	qcsapi_gpio_config * p_gpio_config=(qcsapi_gpio_config *)__req->p_gpio_config;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_gpio_get_config(__req->gpio_pin, p_gpio_config);

	__resp->p_gpio_config = __rpc_prepare_data(__req->p_gpio_config, sizeof(*__resp->p_gpio_config));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_led_get_remote_1_svc(qcsapi_led_get_rpcdata *__req, qcsapi_led_get_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_led_get(__req->led_ident, __req->p_led_setting);

	__resp->p_led_setting = __rpc_prepare_data(__req->p_led_setting, sizeof(*__resp->p_led_setting));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_led_set_remote_1_svc(qcsapi_led_set_rpcdata *__req, qcsapi_led_set_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_led_set(__req->led_ident, __req->new_led_setting);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_led_pwm_enable_remote_1_svc(qcsapi_led_pwm_enable_rpcdata *__req, qcsapi_led_pwm_enable_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_led_pwm_enable(__req->led_ident, __req->onoff, __req->high_count, __req->low_count);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_led_brightness_remote_1_svc(qcsapi_led_brightness_rpcdata *__req, qcsapi_led_brightness_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_led_brightness(__req->led_ident, __req->level);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_gpio_enable_wps_push_button_remote_1_svc(qcsapi_gpio_enable_wps_push_button_rpcdata *__req, qcsapi_gpio_enable_wps_push_button_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_gpio_enable_wps_push_button(__req->wps_push_button, __req->active_logic, __req->use_interrupt_flag);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_count_associations_remote_1_svc(qcsapi_wifi_get_count_associations_rpcdata *__req, qcsapi_wifi_get_count_associations_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_count_associations(ifname, __req->p_association_count);

	__resp->p_association_count = __rpc_prepare_data(__req->p_association_count, sizeof(*__resp->p_association_count));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_associated_device_mac_addr_remote_1_svc(qcsapi_wifi_get_associated_device_mac_addr_rpcdata *__req, qcsapi_wifi_get_associated_device_mac_addr_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	uint8_t * device_mac_addr = (__req->device_mac_addr == NULL) ? NULL : __req->device_mac_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_associated_device_mac_addr(ifname, __req->device_index, device_mac_addr);

	__resp->device_mac_addr = __rpc_prepare_data(__req->device_mac_addr, sizeof(*__resp->device_mac_addr));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_associated_device_ip_addr_remote_1_svc(qcsapi_wifi_get_associated_device_ip_addr_rpcdata *__req, qcsapi_wifi_get_associated_device_ip_addr_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_associated_device_ip_addr(ifname, __req->device_index, __req->ip_addr);

	__resp->ip_addr = __rpc_prepare_data(__req->ip_addr, sizeof(*__resp->ip_addr));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_link_quality_remote_1_svc(qcsapi_wifi_get_link_quality_rpcdata *__req, qcsapi_wifi_get_link_quality_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_link_quality(ifname, __req->association_index, __req->p_link_quality);

	__resp->p_link_quality = __rpc_prepare_data(__req->p_link_quality, sizeof(*__resp->p_link_quality));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_link_quality_max_remote_1_svc(qcsapi_wifi_get_link_quality_max_rpcdata *__req, qcsapi_wifi_get_link_quality_max_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_link_quality_max(ifname, __req->p_max_quality);

	__resp->p_max_quality = __rpc_prepare_data(__req->p_max_quality, sizeof(*__resp->p_max_quality));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_rx_bytes_per_association_remote_1_svc(qcsapi_wifi_get_rx_bytes_per_association_rpcdata *__req, qcsapi_wifi_get_rx_bytes_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_rx_bytes_per_association(ifname, __req->association_index, __req->p_rx_bytes);

	__resp->p_rx_bytes = __rpc_prepare_data(__req->p_rx_bytes, sizeof(*__resp->p_rx_bytes));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tx_bytes_per_association_remote_1_svc(qcsapi_wifi_get_tx_bytes_per_association_rpcdata *__req, qcsapi_wifi_get_tx_bytes_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tx_bytes_per_association(ifname, __req->association_index, __req->p_tx_bytes);

	__resp->p_tx_bytes = __rpc_prepare_data(__req->p_tx_bytes, sizeof(*__resp->p_tx_bytes));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_rx_packets_per_association_remote_1_svc(qcsapi_wifi_get_rx_packets_per_association_rpcdata *__req, qcsapi_wifi_get_rx_packets_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_rx_packets_per_association(ifname, __req->association_index, __req->p_rx_packets);

	__resp->p_rx_packets = __rpc_prepare_data(__req->p_rx_packets, sizeof(*__resp->p_rx_packets));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tx_packets_per_association_remote_1_svc(qcsapi_wifi_get_tx_packets_per_association_rpcdata *__req, qcsapi_wifi_get_tx_packets_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tx_packets_per_association(ifname, __req->association_index, __req->p_tx_packets);

	__resp->p_tx_packets = __rpc_prepare_data(__req->p_tx_packets, sizeof(*__resp->p_tx_packets));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tx_err_packets_per_association_remote_1_svc(qcsapi_wifi_get_tx_err_packets_per_association_rpcdata *__req, qcsapi_wifi_get_tx_err_packets_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tx_err_packets_per_association(ifname, __req->association_index, __req->p_tx_err_packets);

	__resp->p_tx_err_packets = __rpc_prepare_data(__req->p_tx_err_packets, sizeof(*__resp->p_tx_err_packets));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_rssi_per_association_remote_1_svc(qcsapi_wifi_get_rssi_per_association_rpcdata *__req, qcsapi_wifi_get_rssi_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_rssi_per_association(ifname, __req->association_index, __req->p_rssi);

	__resp->p_rssi = __rpc_prepare_data(__req->p_rssi, sizeof(*__resp->p_rssi));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_rssi_in_dbm_per_association_remote_1_svc(qcsapi_wifi_get_rssi_in_dbm_per_association_rpcdata *__req, qcsapi_wifi_get_rssi_in_dbm_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_rssi_in_dbm_per_association(ifname, __req->association_index, __req->p_rssi);

	__resp->p_rssi = __rpc_prepare_data(__req->p_rssi, sizeof(*__resp->p_rssi));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_bw_per_association_remote_1_svc(qcsapi_wifi_get_bw_per_association_rpcdata *__req, qcsapi_wifi_get_bw_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_bw_per_association(ifname, __req->association_index, __req->p_bw);

	__resp->p_bw = __rpc_prepare_data(__req->p_bw, sizeof(*__resp->p_bw));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tx_phy_rate_per_association_remote_1_svc(qcsapi_wifi_get_tx_phy_rate_per_association_rpcdata *__req, qcsapi_wifi_get_tx_phy_rate_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tx_phy_rate_per_association(ifname, __req->association_index, __req->p_tx_phy_rate);

	__resp->p_tx_phy_rate = __rpc_prepare_data(__req->p_tx_phy_rate, sizeof(*__resp->p_tx_phy_rate));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_rx_phy_rate_per_association_remote_1_svc(qcsapi_wifi_get_rx_phy_rate_per_association_rpcdata *__req, qcsapi_wifi_get_rx_phy_rate_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_rx_phy_rate_per_association(ifname, __req->association_index, __req->p_rx_phy_rate);

	__resp->p_rx_phy_rate = __rpc_prepare_data(__req->p_rx_phy_rate, sizeof(*__resp->p_rx_phy_rate));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tx_mcs_per_association_remote_1_svc(qcsapi_wifi_get_tx_mcs_per_association_rpcdata *__req, qcsapi_wifi_get_tx_mcs_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tx_mcs_per_association(ifname, __req->association_index, __req->p_mcs);

	__resp->p_mcs = __rpc_prepare_data(__req->p_mcs, sizeof(*__resp->p_mcs));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_rx_mcs_per_association_remote_1_svc(qcsapi_wifi_get_rx_mcs_per_association_rpcdata *__req, qcsapi_wifi_get_rx_mcs_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_rx_mcs_per_association(ifname, __req->association_index, __req->p_mcs);

	__resp->p_mcs = __rpc_prepare_data(__req->p_mcs, sizeof(*__resp->p_mcs));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_achievable_tx_phy_rate_per_association_remote_1_svc(qcsapi_wifi_get_achievable_tx_phy_rate_per_association_rpcdata *__req, qcsapi_wifi_get_achievable_tx_phy_rate_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_achievable_tx_phy_rate_per_association(ifname, __req->association_index, __req->p_achievable_tx_phy_rate);

	__resp->p_achievable_tx_phy_rate = __rpc_prepare_data(__req->p_achievable_tx_phy_rate, sizeof(*__resp->p_achievable_tx_phy_rate));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_achievable_rx_phy_rate_per_association_remote_1_svc(qcsapi_wifi_get_achievable_rx_phy_rate_per_association_rpcdata *__req, qcsapi_wifi_get_achievable_rx_phy_rate_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_achievable_rx_phy_rate_per_association(ifname, __req->association_index, __req->p_achievable_rx_phy_rate);

	__resp->p_achievable_rx_phy_rate = __rpc_prepare_data(__req->p_achievable_rx_phy_rate, sizeof(*__resp->p_achievable_rx_phy_rate));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_auth_enc_per_association_remote_1_svc(qcsapi_wifi_get_auth_enc_per_association_rpcdata *__req, qcsapi_wifi_get_auth_enc_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_auth_enc_per_association(ifname, __req->association_index, __req->p_auth_enc);

	__resp->p_auth_enc = __rpc_prepare_data(__req->p_auth_enc, sizeof(*__resp->p_auth_enc));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tput_caps_remote_1_svc(qcsapi_wifi_get_tput_caps_rpcdata *__req, qcsapi_wifi_get_tput_caps_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tput_caps(ifname, __req->association_index, (struct ieee8011req_sta_tput_caps *)__req->tput_caps);

	__resp->tput_caps = __rpc_prepare_data(__req->tput_caps, sizeof(*__resp->tput_caps));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_connection_mode_remote_1_svc(qcsapi_wifi_get_connection_mode_rpcdata *__req, qcsapi_wifi_get_connection_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_connection_mode(ifname, __req->association_index, __req->connection_mode);

	__resp->connection_mode = __rpc_prepare_data(__req->connection_mode, sizeof(*__resp->connection_mode));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_vendor_per_association_remote_1_svc(qcsapi_wifi_get_vendor_per_association_rpcdata *__req, qcsapi_wifi_get_vendor_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_vendor_per_association(ifname, __req->association_index, __req->p_vendor);

	__resp->p_vendor = __rpc_prepare_data(__req->p_vendor, sizeof(*__resp->p_vendor));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_max_mimo_remote_1_svc(qcsapi_wifi_get_max_mimo_rpcdata *__req, qcsapi_wifi_get_max_mimo_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * p_max_mimo = (__req->p_max_mimo == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_max_mimo(ifname, __req->association_index, p_max_mimo);

	if (p_max_mimo) {
		__resp->p_max_mimo = malloc(sizeof(*__resp->p_max_mimo));
		__resp->p_max_mimo->data = p_max_mimo;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_snr_per_association_remote_1_svc(qcsapi_wifi_get_snr_per_association_rpcdata *__req, qcsapi_wifi_get_snr_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_snr_per_association(ifname, __req->association_index, __req->p_snr);

	__resp->p_snr = __rpc_prepare_data(__req->p_snr, sizeof(*__resp->p_snr));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_time_associated_per_association_remote_1_svc(qcsapi_wifi_get_time_associated_per_association_rpcdata *__req, qcsapi_wifi_get_time_associated_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_time_associated_per_association(ifname, __req->association_index, __req->time_associated);

	__resp->time_associated = __rpc_prepare_data(__req->time_associated, sizeof(*__resp->time_associated));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_node_param_remote_1_svc(qcsapi_wifi_get_node_param_rpcdata *__req, qcsapi_wifi_get_node_param_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_per_assoc_param param_type=(qcsapi_per_assoc_param)__req->param_type;
	char * input_param_str = (__req->input_param_str == NULL) ? NULL : __req->input_param_str->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_node_param(ifname, __req->node_index, param_type, __req->local_remote_flag, input_param_str, (qcsapi_measure_report_result *)&(__req->report_result->__rpc_qcsapi_measure_report_result_u));

	__resp->report_result = __rpc_prepare_data(__req->report_result, sizeof(*__resp->report_result));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_node_counter_remote_1_svc(qcsapi_wifi_get_node_counter_rpcdata *__req, qcsapi_wifi_get_node_counter_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_counter_type counter_type=(qcsapi_counter_type)__req->counter_type;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_node_counter(ifname, __req->node_index, counter_type, __req->local_remote_flag, __req->p_value);

	__resp->p_value = __rpc_prepare_data(__req->p_value, sizeof(*__resp->p_value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_node_stats_remote_1_svc(qcsapi_wifi_get_node_stats_rpcdata *__req, qcsapi_wifi_get_node_stats_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_node_stats(ifname, __req->node_index, __req->local_remote_flag, (struct qcsapi_node_stats *)__req->p_stats);

	__resp->p_stats = __rpc_prepare_data(__req->p_stats, sizeof(*__resp->p_stats));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_max_queued_remote_1_svc(qcsapi_wifi_get_max_queued_rpcdata *__req, qcsapi_wifi_get_max_queued_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_max_queued(ifname, __req->node_index, __req->local_remote_flag, __req->reset_flag, __req->max_queued);

	__resp->max_queued = __rpc_prepare_data(__req->max_queued, sizeof(*__resp->max_queued));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_hw_noise_per_association_remote_1_svc(qcsapi_wifi_get_hw_noise_per_association_rpcdata *__req, qcsapi_wifi_get_hw_noise_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_hw_noise_per_association(ifname, __req->association_index, __req->p_hw_noise);

	__resp->p_hw_noise = __rpc_prepare_data(__req->p_hw_noise, sizeof(*__resp->p_hw_noise));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_mlme_stats_per_mac_remote_1_svc(qcsapi_wifi_get_mlme_stats_per_mac_rpcdata *__req, qcsapi_wifi_get_mlme_stats_per_mac_rpcdata *__resp, struct svc_req *rqstp)
{
	uint8_t * client_mac_addr = (__req->client_mac_addr == NULL) ? NULL : __req->client_mac_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_mlme_stats_per_mac(client_mac_addr, (qcsapi_mlme_stats *)__req->stats);

	__resp->stats = __rpc_prepare_data(__req->stats, sizeof(*__resp->stats));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_mlme_stats_per_association_remote_1_svc(qcsapi_wifi_get_mlme_stats_per_association_rpcdata *__req, qcsapi_wifi_get_mlme_stats_per_association_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_mlme_stats_per_association(ifname, __req->association_index, (qcsapi_mlme_stats *)__req->stats);

	__resp->stats = __rpc_prepare_data(__req->stats, sizeof(*__resp->stats));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_mlme_stats_macs_list_remote_1_svc(qcsapi_wifi_get_mlme_stats_macs_list_rpcdata *__req, qcsapi_wifi_get_mlme_stats_macs_list_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_mlme_stats_macs_list((qcsapi_mlme_stats_macs *)__req->macs_list);

	__resp->macs_list = __rpc_prepare_data(__req->macs_list, sizeof(*__resp->macs_list));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_sample_all_clients_remote_1_svc(qcsapi_wifi_sample_all_clients_rpcdata *__req, qcsapi_wifi_sample_all_clients_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_sample_all_clients(ifname, __req->sta_count);

	__resp->sta_count = __rpc_prepare_data(__req->sta_count, sizeof(*__resp->sta_count));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_per_assoc_data_remote_1_svc(qcsapi_wifi_get_per_assoc_data_rpcdata *__req, qcsapi_wifi_get_per_assoc_data_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_per_assoc_data(ifname, (struct qcsapi_sample_assoc_data *)__req->ptr, __req->num_entry, __req->offset);

	__resp->ptr = __rpc_prepare_data(__req->ptr, sizeof(*__resp->ptr));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_list_regulatory_regions_remote_1_svc(qcsapi_wifi_get_list_regulatory_regions_rpcdata *__req, qcsapi_wifi_get_list_regulatory_regions_rpcdata *__resp, struct svc_req *rqstp)
{
	char * list_regulatory_regions = (__req->list_regulatory_regions == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_list_regulatory_regions(list_regulatory_regions);

	if (list_regulatory_regions) {
		__resp->list_regulatory_regions = malloc(sizeof(*__resp->list_regulatory_regions));
		__resp->list_regulatory_regions->data = list_regulatory_regions;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_get_list_regulatory_regions_remote_1_svc(qcsapi_regulatory_get_list_regulatory_regions_rpcdata *__req, qcsapi_regulatory_get_list_regulatory_regions_rpcdata *__resp, struct svc_req *rqstp)
{
	char * list_regulatory_regions = (__req->list_regulatory_regions == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_get_list_regulatory_regions(list_regulatory_regions);

	if (list_regulatory_regions) {
		__resp->list_regulatory_regions = malloc(sizeof(*__resp->list_regulatory_regions));
		__resp->list_regulatory_regions->data = list_regulatory_regions;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_list_regulatory_channels_remote_1_svc(qcsapi_wifi_get_list_regulatory_channels_rpcdata *__req, qcsapi_wifi_get_list_regulatory_channels_rpcdata *__resp, struct svc_req *rqstp)
{
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;
	char * list_of_channels = (__req->list_of_channels == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_list_regulatory_channels(region_by_name, __req->bw, list_of_channels);

	if (list_of_channels) {
		__resp->list_of_channels = malloc(sizeof(*__resp->list_of_channels));
		__resp->list_of_channels->data = list_of_channels;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_get_list_regulatory_channels_remote_1_svc(qcsapi_regulatory_get_list_regulatory_channels_rpcdata *__req, qcsapi_regulatory_get_list_regulatory_channels_rpcdata *__resp, struct svc_req *rqstp)
{
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;
	char * list_of_channels = (__req->list_of_channels == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_get_list_regulatory_channels(region_by_name, __req->bw, list_of_channels);

	if (list_of_channels) {
		__resp->list_of_channels = malloc(sizeof(*__resp->list_of_channels));
		__resp->list_of_channels->data = list_of_channels;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_get_list_regulatory_bands_remote_1_svc(qcsapi_regulatory_get_list_regulatory_bands_rpcdata *__req, qcsapi_regulatory_get_list_regulatory_bands_rpcdata *__resp, struct svc_req *rqstp)
{
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;
	char * list_of_bands = (__req->list_of_bands == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_get_list_regulatory_bands(region_by_name, list_of_bands);

	if (list_of_bands) {
		__resp->list_of_bands = malloc(sizeof(*__resp->list_of_bands));
		__resp->list_of_bands->data = list_of_bands;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_regulatory_tx_power_remote_1_svc(qcsapi_wifi_get_regulatory_tx_power_rpcdata *__req, qcsapi_wifi_get_regulatory_tx_power_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_regulatory_tx_power(ifname, __req->the_channel, region_by_name, __req->p_tx_power);

	__resp->p_tx_power = __rpc_prepare_data(__req->p_tx_power, sizeof(*__resp->p_tx_power));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_get_regulatory_tx_power_remote_1_svc(qcsapi_regulatory_get_regulatory_tx_power_rpcdata *__req, qcsapi_regulatory_get_regulatory_tx_power_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_get_regulatory_tx_power(ifname, __req->the_channel, region_by_name, __req->p_tx_power);

	__resp->p_tx_power = __rpc_prepare_data(__req->p_tx_power, sizeof(*__resp->p_tx_power));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_configured_tx_power_remote_1_svc(qcsapi_wifi_get_configured_tx_power_rpcdata *__req, qcsapi_wifi_get_configured_tx_power_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_configured_tx_power(ifname, __req->the_channel, region_by_name, __req->bw, __req->p_tx_power);

	__resp->p_tx_power = __rpc_prepare_data(__req->p_tx_power, sizeof(*__resp->p_tx_power));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_get_configured_tx_power_remote_1_svc(qcsapi_regulatory_get_configured_tx_power_rpcdata *__req, qcsapi_regulatory_get_configured_tx_power_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_get_configured_tx_power(ifname, __req->the_channel, region_by_name, __req->bw, __req->p_tx_power);

	__resp->p_tx_power = __rpc_prepare_data(__req->p_tx_power, sizeof(*__resp->p_tx_power));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_get_configured_tx_power_ext_remote_1_svc(qcsapi_regulatory_get_configured_tx_power_ext_rpcdata *__req, qcsapi_regulatory_get_configured_tx_power_ext_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;
	const qcsapi_bw the_bw=(const qcsapi_bw)__req->the_bw;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_get_configured_tx_power_ext(ifname, __req->the_channel, region_by_name, the_bw, __req->bf_on, __req->number_ss, __req->p_tx_power);

	__resp->p_tx_power = __rpc_prepare_data(__req->p_tx_power, sizeof(*__resp->p_tx_power));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_regulatory_region_remote_1_svc(qcsapi_wifi_set_regulatory_region_rpcdata *__req, qcsapi_wifi_set_regulatory_region_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_regulatory_region(ifname, region_by_name);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_set_regulatory_region_remote_1_svc(qcsapi_regulatory_set_regulatory_region_rpcdata *__req, qcsapi_regulatory_set_regulatory_region_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_set_regulatory_region(ifname, region_by_name);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_restore_regulatory_tx_power_remote_1_svc(qcsapi_regulatory_restore_regulatory_tx_power_rpcdata *__req, qcsapi_regulatory_restore_regulatory_tx_power_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_restore_regulatory_tx_power(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_regulatory_region_remote_1_svc(qcsapi_wifi_get_regulatory_region_rpcdata *__req, qcsapi_wifi_get_regulatory_region_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_regulatory_region(ifname, region_by_name);

	if (region_by_name) {
		__resp->region_by_name = malloc(sizeof(*__resp->region_by_name));
		__resp->region_by_name->data = region_by_name;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_overwrite_country_code_remote_1_svc(qcsapi_regulatory_overwrite_country_code_rpcdata *__req, qcsapi_regulatory_overwrite_country_code_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * curr_country_name = (__req->curr_country_name == NULL) ? NULL : __req->curr_country_name->data;
	char * new_country_name = (__req->new_country_name == NULL) ? NULL : __req->new_country_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_overwrite_country_code(ifname, curr_country_name, new_country_name);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_regulatory_channel_remote_1_svc(qcsapi_wifi_set_regulatory_channel_rpcdata *__req, qcsapi_wifi_set_regulatory_channel_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_regulatory_channel(ifname, __req->the_channel, region_by_name, __req->tx_power_offset);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_set_regulatory_channel_remote_1_svc(qcsapi_regulatory_set_regulatory_channel_rpcdata *__req, qcsapi_regulatory_set_regulatory_channel_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_set_regulatory_channel(ifname, __req->the_channel, region_by_name, __req->tx_power_offset);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_get_db_version_remote_1_svc(qcsapi_regulatory_get_db_version_rpcdata *__req, qcsapi_regulatory_get_db_version_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_get_db_version(__req->p_version, __req->index);

	__resp->p_version = __rpc_prepare_data(__req->p_version, sizeof(*__resp->p_version));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_apply_tx_power_cap_remote_1_svc(qcsapi_regulatory_apply_tx_power_cap_rpcdata *__req, qcsapi_regulatory_apply_tx_power_cap_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_apply_tx_power_cap(__req->capped);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_list_dfs_channels_remote_1_svc(qcsapi_wifi_get_list_DFS_channels_rpcdata *__req, qcsapi_wifi_get_list_DFS_channels_rpcdata *__resp, struct svc_req *rqstp)
{
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;
	char * list_of_channels = (__req->list_of_channels == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_list_DFS_channels(region_by_name, __req->DFS_flag, __req->bw, list_of_channels);

	if (list_of_channels) {
		__resp->list_of_channels = malloc(sizeof(*__resp->list_of_channels));
		__resp->list_of_channels->data = list_of_channels;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_get_list_dfs_channels_remote_1_svc(qcsapi_regulatory_get_list_DFS_channels_rpcdata *__req, qcsapi_regulatory_get_list_DFS_channels_rpcdata *__resp, struct svc_req *rqstp)
{
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;
	char * list_of_channels = (__req->list_of_channels == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_get_list_DFS_channels(region_by_name, __req->DFS_flag, __req->bw, list_of_channels);

	if (list_of_channels) {
		__resp->list_of_channels = malloc(sizeof(*__resp->list_of_channels));
		__resp->list_of_channels->data = list_of_channels;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_is_channel_dfs_remote_1_svc(qcsapi_wifi_is_channel_DFS_rpcdata *__req, qcsapi_wifi_is_channel_DFS_rpcdata *__resp, struct svc_req *rqstp)
{
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_is_channel_DFS(region_by_name, __req->the_channel, __req->p_channel_is_DFS);

	__resp->p_channel_is_DFS = __rpc_prepare_data(__req->p_channel_is_DFS, sizeof(*__resp->p_channel_is_DFS));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_regulatory_is_channel_dfs_remote_1_svc(qcsapi_regulatory_is_channel_DFS_rpcdata *__req, qcsapi_regulatory_is_channel_DFS_rpcdata *__resp, struct svc_req *rqstp)
{
	char * region_by_name = (__req->region_by_name == NULL) ? NULL : __req->region_by_name->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_regulatory_is_channel_DFS(region_by_name, __req->the_channel, __req->p_channel_is_DFS);

	__resp->p_channel_is_DFS = __rpc_prepare_data(__req->p_channel_is_DFS, sizeof(*__resp->p_channel_is_DFS));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_dfs_cce_channels_remote_1_svc(qcsapi_wifi_get_dfs_cce_channels_rpcdata *__req, qcsapi_wifi_get_dfs_cce_channels_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_dfs_cce_channels(ifname, __req->p_prev_channel, __req->p_cur_channel);

	__resp->p_prev_channel = __rpc_prepare_data(__req->p_prev_channel, sizeof(*__resp->p_prev_channel));
	__resp->p_cur_channel = __rpc_prepare_data(__req->p_cur_channel, sizeof(*__resp->p_cur_channel));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_dfs_alt_channel_remote_1_svc(qcsapi_wifi_get_DFS_alt_channel_rpcdata *__req, qcsapi_wifi_get_DFS_alt_channel_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_DFS_alt_channel(ifname, __req->p_dfs_alt_chan);

	__resp->p_dfs_alt_chan = __rpc_prepare_data(__req->p_dfs_alt_chan, sizeof(*__resp->p_dfs_alt_chan));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dfs_alt_channel_remote_1_svc(qcsapi_wifi_set_DFS_alt_channel_rpcdata *__req, qcsapi_wifi_set_DFS_alt_channel_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_DFS_alt_channel(ifname, __req->dfs_alt_chan);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_start_dfs_reentry_remote_1_svc(qcsapi_wifi_start_dfs_reentry_rpcdata *__req, qcsapi_wifi_start_dfs_reentry_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_start_dfs_reentry(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_start_scan_ext_remote_1_svc(qcsapi_wifi_start_scan_ext_rpcdata *__req, qcsapi_wifi_start_scan_ext_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_start_scan_ext(ifname, __req->scan_flag);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_csw_records_remote_1_svc(qcsapi_wifi_get_csw_records_rpcdata *__req, qcsapi_wifi_get_csw_records_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_csw_records(ifname, __req->reset, (qcsapi_csw_record *)__req->record);

	__resp->record = __rpc_prepare_data(__req->record, sizeof(*__resp->record));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_radar_status_remote_1_svc(qcsapi_wifi_get_radar_status_rpcdata *__req, qcsapi_wifi_get_radar_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_radar_status(ifname, (qcsapi_radar_status *)__req->rdstatus);

	__resp->rdstatus = __rpc_prepare_data(__req->rdstatus, sizeof(*__resp->rdstatus));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_cac_status_remote_1_svc(qcsapi_wifi_get_cac_status_rpcdata *__req, qcsapi_wifi_get_cac_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_cac_status(ifname, __req->cacstatus);

	__resp->cacstatus = __rpc_prepare_data(__req->cacstatus, sizeof(*__resp->cacstatus));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_results_ap_scan_remote_1_svc(qcsapi_wifi_get_results_AP_scan_rpcdata *__req, qcsapi_wifi_get_results_AP_scan_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_results_AP_scan(ifname, __req->p_count_APs);

	__resp->p_count_APs = __rpc_prepare_data(__req->p_count_APs, sizeof(*__resp->p_count_APs));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_count_aps_scanned_remote_1_svc(qcsapi_wifi_get_count_APs_scanned_rpcdata *__req, qcsapi_wifi_get_count_APs_scanned_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_count_APs_scanned(ifname, __req->p_count_APs);

	__resp->p_count_APs = __rpc_prepare_data(__req->p_count_APs, sizeof(*__resp->p_count_APs));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_properties_ap_remote_1_svc(qcsapi_wifi_get_properties_AP_rpcdata *__req, qcsapi_wifi_get_properties_AP_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_properties_AP(ifname, __req->index_AP, (qcsapi_ap_properties *)__req->p_ap_properties);

	__resp->p_ap_properties = __rpc_prepare_data(__req->p_ap_properties, sizeof(*__resp->p_ap_properties));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scan_chk_inv_remote_1_svc(qcsapi_wifi_set_scan_chk_inv_rpcdata *__req, qcsapi_wifi_set_scan_chk_inv_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scan_chk_inv(ifname, __req->scan_chk_inv);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scan_chk_inv_remote_1_svc(qcsapi_wifi_get_scan_chk_inv_rpcdata *__req, qcsapi_wifi_get_scan_chk_inv_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scan_chk_inv(ifname, __req->p);

	__resp->p = __rpc_prepare_data(__req->p, sizeof(*__resp->p));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scan_buf_max_size_remote_1_svc(qcsapi_wifi_set_scan_buf_max_size_rpcdata *__req, qcsapi_wifi_set_scan_buf_max_size_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scan_buf_max_size(ifname, __req->max_buf_size);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scan_buf_max_size_remote_1_svc(qcsapi_wifi_get_scan_buf_max_size_rpcdata *__req, qcsapi_wifi_get_scan_buf_max_size_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scan_buf_max_size(ifname, __req->max_buf_size);

	__resp->max_buf_size = __rpc_prepare_data(__req->max_buf_size, sizeof(*__resp->max_buf_size));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_scan_table_max_len_remote_1_svc(qcsapi_wifi_set_scan_table_max_len_rpcdata *__req, qcsapi_wifi_set_scan_table_max_len_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_scan_table_max_len(ifname, __req->max_table_len);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scan_table_max_len_remote_1_svc(qcsapi_wifi_get_scan_table_max_len_rpcdata *__req, qcsapi_wifi_get_scan_table_max_len_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scan_table_max_len(ifname, __req->max_table_len);

	__resp->max_table_len = __rpc_prepare_data(__req->max_table_len, sizeof(*__resp->max_table_len));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_dwell_times_remote_1_svc(qcsapi_wifi_set_dwell_times_rpcdata *__req, qcsapi_wifi_set_dwell_times_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_dwell_times(ifname, __req->max_dwell_time_active_chan, __req->min_dwell_time_active_chan, __req->max_dwell_time_passive_chan, __req->min_dwell_time_passive_chan);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_dwell_times_remote_1_svc(qcsapi_wifi_get_dwell_times_rpcdata *__req, qcsapi_wifi_get_dwell_times_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_dwell_times(ifname, __req->p_max_dwell_time_active_chan, __req->p_min_dwell_time_active_chan, __req->p_max_dwell_time_passive_chan, __req->p_min_dwell_time_passive_chan);

	__resp->p_max_dwell_time_active_chan = __rpc_prepare_data(__req->p_max_dwell_time_active_chan, sizeof(*__resp->p_max_dwell_time_active_chan));
	__resp->p_min_dwell_time_active_chan = __rpc_prepare_data(__req->p_min_dwell_time_active_chan, sizeof(*__resp->p_min_dwell_time_active_chan));
	__resp->p_max_dwell_time_passive_chan = __rpc_prepare_data(__req->p_max_dwell_time_passive_chan, sizeof(*__resp->p_max_dwell_time_passive_chan));
	__resp->p_min_dwell_time_passive_chan = __rpc_prepare_data(__req->p_min_dwell_time_passive_chan, sizeof(*__resp->p_min_dwell_time_passive_chan));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_bgscan_dwell_times_remote_1_svc(qcsapi_wifi_set_bgscan_dwell_times_rpcdata *__req, qcsapi_wifi_set_bgscan_dwell_times_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_bgscan_dwell_times(ifname, __req->dwell_time_active_chan, __req->dwell_time_passive_chan);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_bgscan_dwell_times_remote_1_svc(qcsapi_wifi_get_bgscan_dwell_times_rpcdata *__req, qcsapi_wifi_get_bgscan_dwell_times_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_bgscan_dwell_times(ifname, __req->p_dwell_time_active_chan, __req->p_dwell_time_passive_chan);

	__resp->p_dwell_time_active_chan = __rpc_prepare_data(__req->p_dwell_time_active_chan, sizeof(*__resp->p_dwell_time_active_chan));
	__resp->p_dwell_time_passive_chan = __rpc_prepare_data(__req->p_dwell_time_passive_chan, sizeof(*__resp->p_dwell_time_passive_chan));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_start_scan_remote_1_svc(qcsapi_wifi_start_scan_rpcdata *__req, qcsapi_wifi_start_scan_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_start_scan(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_cancel_scan_remote_1_svc(qcsapi_wifi_cancel_scan_rpcdata *__req, qcsapi_wifi_cancel_scan_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_cancel_scan(ifname, __req->force);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scan_status_remote_1_svc(qcsapi_wifi_get_scan_status_rpcdata *__req, qcsapi_wifi_get_scan_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scan_status(ifname, __req->scanstatus);

	__resp->scanstatus = __rpc_prepare_data(__req->scanstatus, sizeof(*__resp->scanstatus));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_enable_bgscan_remote_1_svc(qcsapi_wifi_enable_bgscan_rpcdata *__req, qcsapi_wifi_enable_bgscan_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_enable_bgscan(ifname, __req->enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_bgscan_status_remote_1_svc(qcsapi_wifi_get_bgscan_status_rpcdata *__req, qcsapi_wifi_get_bgscan_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_bgscan_status(ifname, __req->enable);

	__resp->enable = __rpc_prepare_data(__req->enable, sizeof(*__resp->enable));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_wait_scan_completes_remote_1_svc(qcsapi_wifi_wait_scan_completes_rpcdata *__req, qcsapi_wifi_wait_scan_completes_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_wait_scan_completes(ifname, __req->timeout);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_threshold_of_neighborhood_type_remote_1_svc(qcsapi_wifi_set_threshold_of_neighborhood_type_rpcdata *__req, qcsapi_wifi_set_threshold_of_neighborhood_type_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_threshold_of_neighborhood_type(ifname, __req->type, __req->threshold);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_threshold_of_neighborhood_type_remote_1_svc(qcsapi_wifi_get_threshold_of_neighborhood_type_rpcdata *__req, qcsapi_wifi_get_threshold_of_neighborhood_type_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_threshold_of_neighborhood_type(ifname, __req->type, __req->threshold);

	__resp->threshold = __rpc_prepare_data(__req->threshold, sizeof(*__resp->threshold));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_neighborhood_type_remote_1_svc(qcsapi_wifi_get_neighborhood_type_rpcdata *__req, qcsapi_wifi_get_neighborhood_type_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_neighborhood_type(ifname, __req->type, __req->count);

	__resp->type = __rpc_prepare_data(__req->type, sizeof(*__resp->type));
	__resp->count = __rpc_prepare_data(__req->count, sizeof(*__resp->count));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_backoff_fail_max_remote_1_svc(qcsapi_wifi_backoff_fail_max_rpcdata *__req, qcsapi_wifi_backoff_fail_max_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_backoff_fail_max(ifname, __req->fail_max);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_backoff_timeout_remote_1_svc(qcsapi_wifi_backoff_timeout_rpcdata *__req, qcsapi_wifi_backoff_timeout_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_backoff_timeout(ifname, __req->timeout);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_mcs_rate_remote_1_svc(qcsapi_wifi_get_mcs_rate_rpcdata *__req, qcsapi_wifi_get_mcs_rate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * current_mcs_rate = (__req->current_mcs_rate == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_mcs_rate(ifname, current_mcs_rate);

	if (current_mcs_rate) {
		__resp->current_mcs_rate = malloc(sizeof(*__resp->current_mcs_rate));
		__resp->current_mcs_rate->data = current_mcs_rate;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_mcs_rate_remote_1_svc(qcsapi_wifi_set_mcs_rate_rpcdata *__req, qcsapi_wifi_set_mcs_rate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * new_mcs_rate = (__req->new_mcs_rate == NULL) ? NULL : __req->new_mcs_rate->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_mcs_rate(ifname, new_mcs_rate);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_pairing_id_remote_1_svc(qcsapi_wifi_set_pairing_id_rpcdata *__req, qcsapi_wifi_set_pairing_id_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * pairing_id = (__req->pairing_id == NULL) ? NULL : __req->pairing_id->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_pairing_id(ifname, pairing_id);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_pairing_id_remote_1_svc(qcsapi_wifi_get_pairing_id_rpcdata *__req, qcsapi_wifi_get_pairing_id_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * pairing_id = (__req->pairing_id == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_pairing_id(ifname, pairing_id);

	if (pairing_id) {
		__resp->pairing_id = malloc(sizeof(*__resp->pairing_id));
		__resp->pairing_id->data = pairing_id;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_pairing_enable_remote_1_svc(qcsapi_wifi_set_pairing_enable_rpcdata *__req, qcsapi_wifi_set_pairing_enable_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * enable = (__req->enable == NULL) ? NULL : __req->enable->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_pairing_enable(ifname, enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_pairing_enable_remote_1_svc(qcsapi_wifi_get_pairing_enable_rpcdata *__req, qcsapi_wifi_get_pairing_enable_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * enable = (__req->enable == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_pairing_enable(ifname, enable);

	if (enable) {
		__resp->enable = malloc(sizeof(*__resp->enable));
		__resp->enable->data = enable;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_non_wps_set_pp_enable_remote_1_svc(qcsapi_non_wps_set_pp_enable_rpcdata *__req, qcsapi_non_wps_set_pp_enable_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_non_wps_set_pp_enable(ifname, __req->ctrl_state);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_non_wps_get_pp_enable_remote_1_svc(qcsapi_non_wps_get_pp_enable_rpcdata *__req, qcsapi_non_wps_get_pp_enable_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_non_wps_get_pp_enable(ifname, __req->ctrl_state);

	__resp->ctrl_state = __rpc_prepare_data(__req->ctrl_state, sizeof(*__resp->ctrl_state));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_vendor_fix_remote_1_svc(qcsapi_wifi_set_vendor_fix_rpcdata *__req, qcsapi_wifi_set_vendor_fix_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_vendor_fix(ifname, __req->fix_param, __req->value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_errno_get_message_remote_1_svc(qcsapi_errno_get_message_rpcdata *__req, qcsapi_errno_get_message_rpcdata *__resp, struct svc_req *rqstp)
{
	char * error_msg = (__req->error_msg == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_errno_get_message(__req->qcsapi_retval, error_msg, __req->msglen);

	if (error_msg) {
		__resp->error_msg = malloc(sizeof(*__resp->error_msg));
		__resp->error_msg->data = error_msg;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_vco_lock_detect_mode_remote_1_svc(qcsapi_wifi_get_vco_lock_detect_mode_rpcdata *__req, qcsapi_wifi_get_vco_lock_detect_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_vco_lock_detect_mode(ifname, __req->p_jedecid);

	__resp->p_jedecid = __rpc_prepare_data(__req->p_jedecid, sizeof(*__resp->p_jedecid));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_vco_lock_detect_mode_remote_1_svc(qcsapi_wifi_set_vco_lock_detect_mode_rpcdata *__req, qcsapi_wifi_set_vco_lock_detect_mode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_vco_lock_detect_mode(ifname, __req->p_jedecid);

	__resp->p_jedecid = __rpc_prepare_data(__req->p_jedecid, sizeof(*__resp->p_jedecid));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_interface_stats_remote_1_svc(qcsapi_get_interface_stats_rpcdata *__req, qcsapi_get_interface_stats_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_interface_stats(ifname, (qcsapi_interface_stats *)__req->stats);

	__resp->stats = __rpc_prepare_data(__req->stats, sizeof(*__resp->stats));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_phy_stats_remote_1_svc(qcsapi_get_phy_stats_rpcdata *__req, qcsapi_get_phy_stats_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_phy_stats(ifname, (qcsapi_phy_stats *)__req->stats);

	__resp->stats = __rpc_prepare_data(__req->stats, sizeof(*__resp->stats));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_reset_all_counters_remote_1_svc(qcsapi_reset_all_counters_rpcdata *__req, qcsapi_reset_all_counters_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_reset_all_counters(ifname, __req->node_index, __req->local_remote_flag);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_uboot_info_remote_1_svc(qcsapi_get_uboot_info_rpcdata *__req, qcsapi_get_uboot_info_rpcdata *__resp, struct svc_req *rqstp)
{
	char * uboot_version = (__req->uboot_version == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_uboot_info(uboot_version, (struct early_flash_config *)__req->ef_config);

	if (uboot_version) {
		__resp->uboot_version = malloc(sizeof(*__resp->uboot_version));
		__resp->uboot_version->data = uboot_version;
	}
	__resp->ef_config = __rpc_prepare_data(__req->ef_config, sizeof(*__resp->ef_config));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_firmware_get_version_remote_1_svc(qcsapi_firmware_get_version_rpcdata *__req, qcsapi_firmware_get_version_rpcdata *__resp, struct svc_req *rqstp)
{
	char * firmware_version = (__req->firmware_version == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_firmware_get_version(firmware_version, __req->version_size);

	if (firmware_version) {
		__resp->firmware_version = malloc(sizeof(*__resp->firmware_version));
		__resp->firmware_version->data = firmware_version;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_flash_image_update_remote_1_svc(qcsapi_flash_image_update_rpcdata *__req, qcsapi_flash_image_update_rpcdata *__resp, struct svc_req *rqstp)
{
	char * image_file = (__req->image_file == NULL) ? NULL : __req->image_file->data;
	qcsapi_flash_partiton_type partition_to_upgrade=(qcsapi_flash_partiton_type)__req->partition_to_upgrade;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_flash_image_update(image_file, partition_to_upgrade);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_send_file_remote_1_svc(qcsapi_send_file_rpcdata *__req, qcsapi_send_file_rpcdata *__resp, struct svc_req *rqstp)
{
	char * image_file_path = (__req->image_file_path == NULL) ? NULL : __req->image_file_path->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_send_file(image_file_path, __req->image_flags);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_pm_set_mode_remote_1_svc(qcsapi_pm_set_mode_rpcdata *__req, qcsapi_pm_set_mode_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_pm_set_mode(__req->mode);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_pm_dual_emac_set_mode_remote_1_svc(qcsapi_pm_dual_emac_set_mode_rpcdata *__req, qcsapi_pm_dual_emac_set_mode_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_pm_dual_emac_set_mode(__req->mode);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_pm_get_mode_remote_1_svc(qcsapi_pm_get_mode_rpcdata *__req, qcsapi_pm_get_mode_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_pm_get_mode(__req->mode);

	__resp->mode = __rpc_prepare_data(__req->mode, sizeof(*__resp->mode));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_pm_dual_emac_get_mode_remote_1_svc(qcsapi_pm_dual_emac_get_mode_rpcdata *__req, qcsapi_pm_dual_emac_get_mode_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_pm_dual_emac_get_mode(__req->mode);

	__resp->mode = __rpc_prepare_data(__req->mode, sizeof(*__resp->mode));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_qpm_level_remote_1_svc(qcsapi_get_qpm_level_rpcdata *__req, qcsapi_get_qpm_level_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_qpm_level(__req->qpm_level);

	__resp->qpm_level = __rpc_prepare_data(__req->qpm_level, sizeof(*__resp->qpm_level));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_host_state_remote_1_svc(qcsapi_set_host_state_rpcdata *__req, qcsapi_set_host_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_host_state(ifname, __req->host_state);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_get_state_remote_1_svc(qcsapi_qtm_get_state_rpcdata *__req, qcsapi_qtm_get_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_get_state(ifname, __req->param, __req->value);

	__resp->value = __rpc_prepare_data(__req->value, sizeof(*__resp->value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_get_state_all_remote_1_svc(qcsapi_qtm_get_state_all_rpcdata *__req, qcsapi_qtm_get_state_all_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_get_state_all(ifname, (struct qcsapi_data_128bytes *)__req->value, __req->max);

	__resp->value = __rpc_prepare_data(__req->value, sizeof(*__resp->value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_set_state_remote_1_svc(qcsapi_qtm_set_state_rpcdata *__req, qcsapi_qtm_set_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_set_state(ifname, __req->param, __req->value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_get_config_remote_1_svc(qcsapi_qtm_get_config_rpcdata *__req, qcsapi_qtm_get_config_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_get_config(ifname, __req->param, __req->value);

	__resp->value = __rpc_prepare_data(__req->value, sizeof(*__resp->value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_get_config_all_remote_1_svc(qcsapi_qtm_get_config_all_rpcdata *__req, qcsapi_qtm_get_config_all_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_get_config_all(ifname, (struct qcsapi_data_1Kbytes *)__req->value, __req->max);

	__resp->value = __rpc_prepare_data(__req->value, sizeof(*__resp->value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_set_config_remote_1_svc(qcsapi_qtm_set_config_rpcdata *__req, qcsapi_qtm_set_config_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_set_config(ifname, __req->param, __req->value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_add_rule_remote_1_svc(qcsapi_qtm_add_rule_rpcdata *__req, qcsapi_qtm_add_rule_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_add_rule(ifname, (const struct qcsapi_data_128bytes *)__req->entry);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_del_rule_remote_1_svc(qcsapi_qtm_del_rule_rpcdata *__req, qcsapi_qtm_del_rule_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_del_rule(ifname, (const struct qcsapi_data_128bytes *)__req->entry);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_del_rule_index_remote_1_svc(qcsapi_qtm_del_rule_index_rpcdata *__req, qcsapi_qtm_del_rule_index_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_del_rule_index(ifname, __req->index);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_get_rule_remote_1_svc(qcsapi_qtm_get_rule_rpcdata *__req, qcsapi_qtm_get_rule_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_get_rule(ifname, (struct qcsapi_data_3Kbytes *)__req->entries, __req->max_entries);

	__resp->entries = __rpc_prepare_data(__req->entries, sizeof(*__resp->entries));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_get_strm_remote_1_svc(qcsapi_qtm_get_strm_rpcdata *__req, qcsapi_qtm_get_strm_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_get_strm(ifname, (struct qcsapi_data_4Kbytes *)__req->strms, __req->max_entries, __req->show_all);

	__resp->strms = __rpc_prepare_data(__req->strms, sizeof(*__resp->strms));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_get_stats_remote_1_svc(qcsapi_qtm_get_stats_rpcdata *__req, qcsapi_qtm_get_stats_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_get_stats(ifname, (struct qcsapi_data_512bytes *)__req->stats);

	__resp->stats = __rpc_prepare_data(__req->stats, sizeof(*__resp->stats));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_get_inactive_flags_remote_1_svc(qcsapi_qtm_get_inactive_flags_rpcdata *__req, qcsapi_qtm_get_inactive_flags_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_get_inactive_flags(ifname, __req->flags);

	__resp->flags = __rpc_prepare_data(__req->flags, sizeof(*__resp->flags));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_safe_get_state_all_remote_1_svc(qcsapi_qtm_safe_get_state_all_rpcdata *__req, qcsapi_qtm_safe_get_state_all_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_safe_get_state_all(ifname, (struct qcsapi_int_array32 *)__req->value, __req->max);

	__resp->value = __rpc_prepare_data(__req->value, sizeof(*__resp->value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_safe_get_config_all_remote_1_svc(qcsapi_qtm_safe_get_config_all_rpcdata *__req, qcsapi_qtm_safe_get_config_all_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_safe_get_config_all(ifname, (struct qcsapi_int_array256 *)__req->value, __req->max);

	__resp->value = __rpc_prepare_data(__req->value, sizeof(*__resp->value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_safe_add_rule_remote_1_svc(qcsapi_qtm_safe_add_rule_rpcdata *__req, qcsapi_qtm_safe_add_rule_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_safe_add_rule(ifname, (const struct qcsapi_int_array32 *)__req->entry);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_safe_del_rule_remote_1_svc(qcsapi_qtm_safe_del_rule_rpcdata *__req, qcsapi_qtm_safe_del_rule_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_safe_del_rule(ifname, (const struct qcsapi_int_array32 *)__req->entry);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_safe_get_rule_remote_1_svc(qcsapi_qtm_safe_get_rule_rpcdata *__req, qcsapi_qtm_safe_get_rule_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_safe_get_rule(ifname, (struct qcsapi_int_array768 *)__req->entries, __req->max_entries);

	__resp->entries = __rpc_prepare_data(__req->entries, sizeof(*__resp->entries));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_safe_get_strm_remote_1_svc(qcsapi_qtm_safe_get_strm_rpcdata *__req, qcsapi_qtm_safe_get_strm_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_safe_get_strm(ifname, (struct qvsp_strms *)__req->strms, __req->show_all);

	__resp->strms = __rpc_prepare_data(__req->strms, sizeof(*__resp->strms));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qtm_safe_get_stats_remote_1_svc(qcsapi_qtm_safe_get_stats_rpcdata *__req, qcsapi_qtm_safe_get_stats_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qtm_safe_get_stats(ifname, (struct qcsapi_int_array128 *)__req->stats);

	__resp->stats = __rpc_prepare_data(__req->stats, sizeof(*__resp->stats));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_run_script_remote_1_svc(qcsapi_wifi_run_script_rpcdata *__req, qcsapi_wifi_run_script_rpcdata *__resp, struct svc_req *rqstp)
{
	char * scriptname = (__req->scriptname == NULL) ? NULL : __req->scriptname->data;
	char * param = (__req->param == NULL) ? NULL : __req->param->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_run_script(scriptname, param);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_test_traffic_remote_1_svc(qcsapi_wifi_test_traffic_rpcdata *__req, qcsapi_wifi_test_traffic_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_test_traffic(ifname, __req->period);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_add_multicast_remote_1_svc(qcsapi_wifi_add_multicast_rpcdata *__req, qcsapi_wifi_add_multicast_rpcdata *__resp, struct svc_req *rqstp)
{
	uint8_t * mac = (__req->mac == NULL) ? NULL : __req->mac->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_add_multicast(__req->ipaddr, mac);

	__resp->mac = __rpc_prepare_data(__req->mac, sizeof(*__resp->mac));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_del_multicast_remote_1_svc(qcsapi_wifi_del_multicast_rpcdata *__req, qcsapi_wifi_del_multicast_rpcdata *__resp, struct svc_req *rqstp)
{
	uint8_t * mac = (__req->mac == NULL) ? NULL : __req->mac->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_del_multicast(__req->ipaddr, mac);

	__resp->mac = __rpc_prepare_data(__req->mac, sizeof(*__resp->mac));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_multicast_list_remote_1_svc(qcsapi_wifi_get_multicast_list_rpcdata *__req, qcsapi_wifi_get_multicast_list_rpcdata *__resp, struct svc_req *rqstp)
{
	char * buf = (__req->buf == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_multicast_list(buf, __req->buflen);

	if (buf) {
		__resp->buf = malloc(sizeof(*__resp->buf));
		__resp->buf->data = buf;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_add_ipff_remote_1_svc(qcsapi_wifi_add_ipff_rpcdata *__req, qcsapi_wifi_add_ipff_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_add_ipff(__req->ipaddr);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_del_ipff_remote_1_svc(qcsapi_wifi_del_ipff_rpcdata *__req, qcsapi_wifi_del_ipff_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_del_ipff(__req->ipaddr);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_ipff_remote_1_svc(qcsapi_wifi_get_ipff_rpcdata *__req, qcsapi_wifi_get_ipff_rpcdata *__resp, struct svc_req *rqstp)
{
	char * buf = (__req->buf == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_ipff(buf, __req->buflen);

	if (buf) {
		__resp->buf = malloc(sizeof(*__resp->buf));
		__resp->buf->data = buf;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_rts_threshold_remote_1_svc(qcsapi_wifi_get_rts_threshold_rpcdata *__req, qcsapi_wifi_get_rts_threshold_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_rts_threshold(ifname, __req->rts_threshold);

	__resp->rts_threshold = __rpc_prepare_data(__req->rts_threshold, sizeof(*__resp->rts_threshold));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_rts_threshold_remote_1_svc(qcsapi_wifi_set_rts_threshold_rpcdata *__req, qcsapi_wifi_set_rts_threshold_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_rts_threshold(ifname, __req->rts_threshold);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_nss_cap_remote_1_svc(qcsapi_wifi_set_nss_cap_rpcdata *__req, qcsapi_wifi_set_nss_cap_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	const qcsapi_mimo_type modulation=(const qcsapi_mimo_type)__req->modulation;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_nss_cap(ifname, modulation, __req->nss);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_nss_cap_remote_1_svc(qcsapi_wifi_get_nss_cap_rpcdata *__req, qcsapi_wifi_get_nss_cap_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	const qcsapi_mimo_type modulation=(const qcsapi_mimo_type)__req->modulation;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_nss_cap(ifname, modulation, __req->nss);

	__resp->nss = __rpc_prepare_data(__req->nss, sizeof(*__resp->nss));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tx_amsdu_remote_1_svc(qcsapi_wifi_get_tx_amsdu_rpcdata *__req, qcsapi_wifi_get_tx_amsdu_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tx_amsdu(ifname, __req->enable);

	__resp->enable = __rpc_prepare_data(__req->enable, sizeof(*__resp->enable));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_tx_amsdu_remote_1_svc(qcsapi_wifi_set_tx_amsdu_rpcdata *__req, qcsapi_wifi_set_tx_amsdu_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_tx_amsdu(ifname, __req->enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_disassoc_reason_remote_1_svc(qcsapi_wifi_get_disassoc_reason_rpcdata *__req, qcsapi_wifi_get_disassoc_reason_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_disassoc_reason(ifname, __req->reason);

	__resp->reason = __rpc_prepare_data(__req->reason, sizeof(*__resp->reason));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_block_bss_remote_1_svc(qcsapi_wifi_block_bss_rpcdata *__req, qcsapi_wifi_block_bss_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_block_bss(ifname, __req->flag);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_block_bss_remote_1_svc(qcsapi_wifi_get_block_bss_rpcdata *__req, qcsapi_wifi_get_block_bss_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_block_bss(ifname, __req->pvalue);

	__resp->pvalue = __rpc_prepare_data(__req->pvalue, sizeof(*__resp->pvalue));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_verify_repeater_mode_remote_1_svc(qcsapi_wifi_verify_repeater_mode_rpcdata *__req, qcsapi_wifi_verify_repeater_mode_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_verify_repeater_mode();

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_ap_interface_name_remote_1_svc(qcsapi_wifi_set_ap_interface_name_rpcdata *__req, qcsapi_wifi_set_ap_interface_name_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_ap_interface_name(ifname);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_ap_interface_name_remote_1_svc(qcsapi_wifi_get_ap_interface_name_rpcdata *__req, qcsapi_wifi_get_ap_interface_name_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_ap_interface_name(ifname);

	if (ifname) {
		__resp->ifname = malloc(sizeof(*__resp->ifname));
		__resp->ifname->data = ifname;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_pref_band_remote_1_svc(qcsapi_wifi_set_pref_band_rpcdata *__req, qcsapi_wifi_set_pref_band_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_pref_band(ifname, __req->pref_band);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_pref_band_remote_1_svc(qcsapi_wifi_get_pref_band_rpcdata *__req, qcsapi_wifi_get_pref_band_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_pref_band(ifname, __req->pref_band);

	__resp->pref_band = __rpc_prepare_data(__req->pref_band, sizeof(*__resp->pref_band));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_txba_disable_remote_1_svc(qcsapi_wifi_set_txba_disable_rpcdata *__req, qcsapi_wifi_set_txba_disable_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_txba_disable(ifname, __req->value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_txba_disable_remote_1_svc(qcsapi_wifi_get_txba_disable_rpcdata *__req, qcsapi_wifi_get_txba_disable_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_txba_disable(ifname, __req->value);

	__resp->value = __rpc_prepare_data(__req->value, sizeof(*__resp->value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_rxba_decline_remote_1_svc(qcsapi_wifi_set_rxba_decline_rpcdata *__req, qcsapi_wifi_set_rxba_decline_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_rxba_decline(ifname, __req->value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_rxba_decline_remote_1_svc(qcsapi_wifi_get_rxba_decline_rpcdata *__req, qcsapi_wifi_get_rxba_decline_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_rxba_decline(ifname, __req->value);

	__resp->value = __rpc_prepare_data(__req->value, sizeof(*__resp->value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_txburst_remote_1_svc(qcsapi_wifi_set_txburst_rpcdata *__req, qcsapi_wifi_set_txburst_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_txburst(ifname, __req->enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_txburst_remote_1_svc(qcsapi_wifi_get_txburst_rpcdata *__req, qcsapi_wifi_get_txburst_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_txburst(ifname, __req->enable);

	__resp->enable = __rpc_prepare_data(__req->enable, sizeof(*__resp->enable));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_sec_chan_remote_1_svc(qcsapi_wifi_get_sec_chan_rpcdata *__req, qcsapi_wifi_get_sec_chan_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_sec_chan(ifname, __req->chan, __req->p_sec_chan);

	__resp->p_sec_chan = __rpc_prepare_data(__req->p_sec_chan, sizeof(*__resp->p_sec_chan));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_sec_chan_remote_1_svc(qcsapi_wifi_set_sec_chan_rpcdata *__req, qcsapi_wifi_set_sec_chan_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_sec_chan(ifname, __req->chan, __req->offset);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_node_tx_airtime_accum_control_remote_1_svc(qcsapi_wifi_node_tx_airtime_accum_control_rpcdata *__req, qcsapi_wifi_node_tx_airtime_accum_control_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_airtime_control control=(qcsapi_airtime_control)__req->control;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_node_tx_airtime_accum_control(ifname, __req->node_index, control);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_tx_airtime_accum_control_remote_1_svc(qcsapi_wifi_tx_airtime_accum_control_rpcdata *__req, qcsapi_wifi_tx_airtime_accum_control_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_airtime_control control=(qcsapi_airtime_control)__req->control;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_tx_airtime_accum_control(ifname, control);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_node_get_txrx_airtime_remote_1_svc(qcsapi_wifi_node_get_txrx_airtime_rpcdata *__req, qcsapi_wifi_node_get_txrx_airtime_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_node_get_txrx_airtime(ifname, __req->node_index, (qcsapi_node_txrx_airtime *)__req->node_txrx_airtime);

	__resp->node_txrx_airtime = __rpc_prepare_data(__req->node_txrx_airtime, sizeof(*__resp->node_txrx_airtime));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_txrx_airtime_remote_1_svc(qcsapi_wifi_get_txrx_airtime_rpcdata *__req, qcsapi_wifi_get_txrx_airtime_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * buffer = (__req->buffer == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_txrx_airtime(ifname, buffer);

	if (buffer) {
		__resp->buffer = malloc(sizeof(*__resp->buffer));
		__resp->buffer->data = buffer;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_max_bcast_pps_remote_1_svc(qcsapi_wifi_set_max_bcast_pps_rpcdata *__req, qcsapi_wifi_set_max_bcast_pps_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_max_bcast_pps(ifname, __req->max_bcast_pps);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_is_weather_channel_remote_1_svc(qcsapi_wifi_is_weather_channel_rpcdata *__req, qcsapi_wifi_is_weather_channel_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_is_weather_channel(ifname, __req->channel);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tx_max_amsdu_remote_1_svc(qcsapi_wifi_get_tx_max_amsdu_rpcdata *__req, qcsapi_wifi_get_tx_max_amsdu_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tx_max_amsdu(ifname, __req->max_len);

	__resp->max_len = __rpc_prepare_data(__req->max_len, sizeof(*__resp->max_len));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_tx_max_amsdu_remote_1_svc(qcsapi_wifi_set_tx_max_amsdu_rpcdata *__req, qcsapi_wifi_set_tx_max_amsdu_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_tx_max_amsdu(ifname, __req->max_len);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_temperature_info_remote_1_svc(qcsapi_get_temperature_info_rpcdata *__req, qcsapi_get_temperature_info_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_temperature_info(__req->temp_exter, __req->temp_inter, __req->temp_bbic);

	__resp->temp_exter = __rpc_prepare_data(__req->temp_exter, sizeof(*__resp->temp_exter));
	__resp->temp_inter = __rpc_prepare_data(__req->temp_inter, sizeof(*__resp->temp_inter));
	__resp->temp_bbic = __rpc_prepare_data(__req->temp_bbic, sizeof(*__resp->temp_bbic));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_set_test_mode_remote_1_svc(qcsapi_calcmd_set_test_mode_rpcdata *__req, qcsapi_calcmd_set_test_mode_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_set_test_mode(__req->channel, __req->antenna, __req->mcs, __req->bw, __req->pkt_size, __req->eleven_n, __req->bf);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_show_test_packet_remote_1_svc(qcsapi_calcmd_show_test_packet_rpcdata *__req, qcsapi_calcmd_show_test_packet_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_show_test_packet(__req->tx_packet_num, __req->rx_packet_num, __req->crc_packet_num);

	__resp->tx_packet_num = __rpc_prepare_data(__req->tx_packet_num, sizeof(*__resp->tx_packet_num));
	__resp->rx_packet_num = __rpc_prepare_data(__req->rx_packet_num, sizeof(*__resp->rx_packet_num));
	__resp->crc_packet_num = __rpc_prepare_data(__req->crc_packet_num, sizeof(*__resp->crc_packet_num));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_send_test_packet_remote_1_svc(qcsapi_calcmd_send_test_packet_rpcdata *__req, qcsapi_calcmd_send_test_packet_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_send_test_packet(__req->to_transmit_packet_num);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_stop_test_packet_remote_1_svc(qcsapi_calcmd_stop_test_packet_rpcdata *__req, qcsapi_calcmd_stop_test_packet_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_stop_test_packet();

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_send_dc_cw_signal_remote_1_svc(qcsapi_calcmd_send_dc_cw_signal_rpcdata *__req, qcsapi_calcmd_send_dc_cw_signal_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_send_dc_cw_signal(__req->channel);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_stop_dc_cw_signal_remote_1_svc(qcsapi_calcmd_stop_dc_cw_signal_rpcdata *__req, qcsapi_calcmd_stop_dc_cw_signal_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_stop_dc_cw_signal();

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_get_test_mode_antenna_sel_remote_1_svc(qcsapi_calcmd_get_test_mode_antenna_sel_rpcdata *__req, qcsapi_calcmd_get_test_mode_antenna_sel_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_get_test_mode_antenna_sel(__req->antenna_bit_mask);

	__resp->antenna_bit_mask = __rpc_prepare_data(__req->antenna_bit_mask, sizeof(*__resp->antenna_bit_mask));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_get_test_mode_mcs_remote_1_svc(qcsapi_calcmd_get_test_mode_mcs_rpcdata *__req, qcsapi_calcmd_get_test_mode_mcs_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_get_test_mode_mcs(__req->test_mode_mcs);

	__resp->test_mode_mcs = __rpc_prepare_data(__req->test_mode_mcs, sizeof(*__resp->test_mode_mcs));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_get_test_mode_bw_remote_1_svc(qcsapi_calcmd_get_test_mode_bw_rpcdata *__req, qcsapi_calcmd_get_test_mode_bw_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_get_test_mode_bw(__req->test_mode_bw);

	__resp->test_mode_bw = __rpc_prepare_data(__req->test_mode_bw, sizeof(*__resp->test_mode_bw));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_get_tx_power_remote_1_svc(qcsapi_calcmd_get_tx_power_rpcdata *__req, qcsapi_calcmd_get_tx_power_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_get_tx_power((qcsapi_calcmd_tx_power_rsp *)__req->tx_power);

	__resp->tx_power = __rpc_prepare_data(__req->tx_power, sizeof(*__resp->tx_power));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_set_tx_power_remote_1_svc(qcsapi_calcmd_set_tx_power_rpcdata *__req, qcsapi_calcmd_set_tx_power_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_set_tx_power(__req->tx_power);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_get_test_mode_rssi_remote_1_svc(qcsapi_calcmd_get_test_mode_rssi_rpcdata *__req, qcsapi_calcmd_get_test_mode_rssi_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_get_test_mode_rssi((qcsapi_calcmd_rssi_rsp *)__req->test_mode_rssi);

	__resp->test_mode_rssi = __rpc_prepare_data(__req->test_mode_rssi, sizeof(*__resp->test_mode_rssi));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_set_mac_filter_remote_1_svc(qcsapi_calcmd_set_mac_filter_rpcdata *__req, qcsapi_calcmd_set_mac_filter_rpcdata *__resp, struct svc_req *rqstp)
{
	uint8_t * mac_addr = (__req->mac_addr == NULL) ? NULL : __req->mac_addr->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_set_mac_filter(__req->q_num, __req->sec_enable, mac_addr);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_get_antenna_count_remote_1_svc(qcsapi_calcmd_get_antenna_count_rpcdata *__req, qcsapi_calcmd_get_antenna_count_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_get_antenna_count(__req->antenna_count);

	__resp->antenna_count = __rpc_prepare_data(__req->antenna_count, sizeof(*__resp->antenna_count));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_clear_counter_remote_1_svc(qcsapi_calcmd_clear_counter_rpcdata *__req, qcsapi_calcmd_clear_counter_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_clear_counter();

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_calcmd_get_info_remote_1_svc(qcsapi_calcmd_get_info_rpcdata *__req, qcsapi_calcmd_get_info_rpcdata *__resp, struct svc_req *rqstp)
{
	char * output_info = (__req->output_info == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_calcmd_get_info(output_info);

	if (output_info) {
		__resp->output_info = malloc(sizeof(*__resp->output_info));
		__resp->output_info->data = output_info;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wowlan_set_match_type_remote_1_svc(qcsapi_wowlan_set_match_type_rpcdata *__req, qcsapi_wowlan_set_match_type_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wowlan_set_match_type(ifname, __req->wowlan_match);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wowlan_set_l2_type_remote_1_svc(qcsapi_wowlan_set_L2_type_rpcdata *__req, qcsapi_wowlan_set_L2_type_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wowlan_set_L2_type(ifname, __req->ether_type);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wowlan_set_udp_port_remote_1_svc(qcsapi_wowlan_set_udp_port_rpcdata *__req, qcsapi_wowlan_set_udp_port_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wowlan_set_udp_port(ifname, __req->udp_port);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wowlan_set_magic_pattern_remote_1_svc(qcsapi_wowlan_set_magic_pattern_rpcdata *__req, qcsapi_wowlan_set_magic_pattern_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wowlan_set_magic_pattern(ifname, (struct qcsapi_data_256bytes *)__req->pattern, __req->len);

	__resp->pattern = __rpc_prepare_data(__req->pattern, sizeof(*__resp->pattern));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_wowlan_get_host_state_remote_1_svc(qcsapi_wifi_wowlan_get_host_state_rpcdata *__req, qcsapi_wifi_wowlan_get_host_state_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_wowlan_get_host_state(ifname, __req->p_value, __req->len);

	__resp->p_value = __rpc_prepare_data(__req->p_value, sizeof(*__resp->p_value));
	__resp->len = __rpc_prepare_data(__req->len, sizeof(*__resp->len));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_wowlan_get_match_type_remote_1_svc(qcsapi_wifi_wowlan_get_match_type_rpcdata *__req, qcsapi_wifi_wowlan_get_match_type_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_wowlan_get_match_type(ifname, __req->p_value, __req->len);

	__resp->p_value = __rpc_prepare_data(__req->p_value, sizeof(*__resp->p_value));
	__resp->len = __rpc_prepare_data(__req->len, sizeof(*__resp->len));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_wowlan_get_l2_type_remote_1_svc(qcsapi_wifi_wowlan_get_l2_type_rpcdata *__req, qcsapi_wifi_wowlan_get_l2_type_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_wowlan_get_l2_type(ifname, __req->p_value, __req->len);

	__resp->p_value = __rpc_prepare_data(__req->p_value, sizeof(*__resp->p_value));
	__resp->len = __rpc_prepare_data(__req->len, sizeof(*__resp->len));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_wowlan_get_udp_port_remote_1_svc(qcsapi_wifi_wowlan_get_udp_port_rpcdata *__req, qcsapi_wifi_wowlan_get_udp_port_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_wowlan_get_udp_port(ifname, __req->p_value, __req->len);

	__resp->p_value = __rpc_prepare_data(__req->p_value, sizeof(*__resp->p_value));
	__resp->len = __rpc_prepare_data(__req->len, sizeof(*__resp->len));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_wowlan_get_magic_pattern_remote_1_svc(qcsapi_wifi_wowlan_get_magic_pattern_rpcdata *__req, qcsapi_wifi_wowlan_get_magic_pattern_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_wowlan_get_magic_pattern(ifname, (struct qcsapi_data_256bytes *)__req->p_value, __req->len);

	__resp->p_value = __rpc_prepare_data(__req->p_value, sizeof(*__resp->p_value));
	__resp->len = __rpc_prepare_data(__req->len, sizeof(*__resp->len));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_enable_mu_remote_1_svc(qcsapi_wifi_set_enable_mu_rpcdata *__req, qcsapi_wifi_set_enable_mu_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_enable_mu(ifname, __req->mu_enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_enable_mu_remote_1_svc(qcsapi_wifi_get_enable_mu_rpcdata *__req, qcsapi_wifi_get_enable_mu_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_enable_mu(ifname, __req->mu_enable);

	__resp->mu_enable = __rpc_prepare_data(__req->mu_enable, sizeof(*__resp->mu_enable));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_mu_use_precode_remote_1_svc(qcsapi_wifi_set_mu_use_precode_rpcdata *__req, qcsapi_wifi_set_mu_use_precode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_mu_use_precode(ifname, __req->grp, __req->prec_enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_mu_use_precode_remote_1_svc(qcsapi_wifi_get_mu_use_precode_rpcdata *__req, qcsapi_wifi_get_mu_use_precode_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_mu_use_precode(ifname, __req->grp, __req->prec_enable);

	__resp->prec_enable = __rpc_prepare_data(__req->prec_enable, sizeof(*__resp->prec_enable));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_mu_use_eq_remote_1_svc(qcsapi_wifi_set_mu_use_eq_rpcdata *__req, qcsapi_wifi_set_mu_use_eq_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_mu_use_eq(ifname, __req->eq_enable);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_mu_use_eq_remote_1_svc(qcsapi_wifi_get_mu_use_eq_rpcdata *__req, qcsapi_wifi_get_mu_use_eq_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_mu_use_eq(ifname, __req->meq_enable);

	__resp->meq_enable = __rpc_prepare_data(__req->meq_enable, sizeof(*__resp->meq_enable));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_mu_groups_remote_1_svc(qcsapi_wifi_get_mu_groups_rpcdata *__req, qcsapi_wifi_get_mu_groups_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	char * buf = (__req->buf == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_mu_groups(ifname, buf, __req->size);

	if (buf) {
		__resp->buf = malloc(sizeof(*__resp->buf));
		__resp->buf->data = buf;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_enable_tdls_remote_1_svc(qcsapi_wifi_enable_tdls_rpcdata *__req, qcsapi_wifi_enable_tdls_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_enable_tdls(ifname, __req->enable_tdls);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_enable_tdls_over_qhop_remote_1_svc(qcsapi_wifi_enable_tdls_over_qhop_rpcdata *__req, qcsapi_wifi_enable_tdls_over_qhop_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_enable_tdls_over_qhop(ifname, __req->tdls_over_qhop_en);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tdls_status_remote_1_svc(qcsapi_wifi_get_tdls_status_rpcdata *__req, qcsapi_wifi_get_tdls_status_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tdls_status(ifname, __req->p_tdls_status);

	__resp->p_tdls_status = __rpc_prepare_data(__req->p_tdls_status, sizeof(*__resp->p_tdls_status));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_tdls_params_remote_1_svc(qcsapi_wifi_set_tdls_params_rpcdata *__req, qcsapi_wifi_set_tdls_params_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_tdls_type type=(qcsapi_tdls_type)__req->type;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_tdls_params(ifname, type, __req->param_value);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_tdls_params_remote_1_svc(qcsapi_wifi_get_tdls_params_rpcdata *__req, qcsapi_wifi_get_tdls_params_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_tdls_type type=(qcsapi_tdls_type)__req->type;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_tdls_params(ifname, type, __req->p_value);

	__resp->p_value = __rpc_prepare_data(__req->p_value, sizeof(*__resp->p_value));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_tdls_operate_remote_1_svc(qcsapi_wifi_tdls_operate_rpcdata *__req, qcsapi_wifi_tdls_operate_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_tdls_oper operate=(qcsapi_tdls_oper)__req->operate;
	char * mac_addr_str = (__req->mac_addr_str == NULL) ? NULL : __req->mac_addr_str->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_tdls_operate(ifname, operate, mac_addr_str, __req->cs_interval);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_qwe_command_remote_1_svc(qcsapi_qwe_command_rpcdata *__req, qcsapi_qwe_command_rpcdata *__resp, struct svc_req *rqstp)
{
	char * command = (__req->command == NULL) ? NULL : __req->command->data;
	char * param1 = (__req->param1 == NULL) ? NULL : __req->param1->data;
	char * param2 = (__req->param2 == NULL) ? NULL : __req->param2->data;
	char * param3 = (__req->param3 == NULL) ? NULL : __req->param3->data;
	char * output = (__req->output == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_qwe_command(command, param1, param2, param3, output, __req->max_len);

	if (output) {
		__resp->output = malloc(sizeof(*__resp->output));
		__resp->output->data = output;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_scan_ies_remote_1_svc(qcsapi_wifi_get_scan_IEs_rpcdata *__req, qcsapi_wifi_get_scan_IEs_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_scan_IEs(ifname, (struct qcsapi_data_1Kbytes *)__req->buf, __req->block_id, __req->ulength);

	__resp->buf = __rpc_prepare_data(__req->buf, sizeof(*__resp->buf));
	__resp->ulength = __rpc_prepare_data(__req->ulength, sizeof(*__resp->ulength));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_core_dump_size_remote_1_svc(qcsapi_get_core_dump_size_rpcdata *__req, qcsapi_get_core_dump_size_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_core_dump_size(__req->core_dump_size);

	__resp->core_dump_size = __rpc_prepare_data(__req->core_dump_size, sizeof(*__resp->core_dump_size));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_core_dump_remote_1_svc(qcsapi_get_core_dump_rpcdata *__req, qcsapi_get_core_dump_rpcdata *__resp, struct svc_req *rqstp)
{
	char * buf = (__req->buf == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_core_dump(buf, __req->bytes_to_copy, __req->start_offset, __req->bytes_copied);

	if (buf) {
		__resp->buf = malloc(sizeof(*__resp->buf));
		__resp->buf->data = buf;
	}
	__resp->bytes_copied = __rpc_prepare_data(__req->bytes_copied, sizeof(*__resp->bytes_copied));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_app_core_dump_size_remote_1_svc(qcsapi_get_app_core_dump_size_rpcdata *__req, qcsapi_get_app_core_dump_size_rpcdata *__resp, struct svc_req *rqstp)
{
	char * file = (__req->file == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_app_core_dump_size(file, __req->core_dump_size);

	if (file) {
		__resp->file = malloc(sizeof(*__resp->file));
		__resp->file->data = file;
	}
	__resp->core_dump_size = __rpc_prepare_data(__req->core_dump_size, sizeof(*__resp->core_dump_size));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_app_core_dump_remote_1_svc(qcsapi_get_app_core_dump_rpcdata *__req, qcsapi_get_app_core_dump_rpcdata *__resp, struct svc_req *rqstp)
{
	char * file = (__req->file == NULL) ? NULL : arg_alloc();
	char * buf = (__req->buf == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_app_core_dump(file, buf, __req->bytes_to_copy, __req->offset, __req->bytes_copied);

	if (file) {
		__resp->file = malloc(sizeof(*__resp->file));
		__resp->file->data = file;
	}
	if (buf) {
		__resp->buf = malloc(sizeof(*__resp->buf));
		__resp->buf->data = buf;
	}
	__resp->bytes_copied = __rpc_prepare_data(__req->bytes_copied, sizeof(*__resp->bytes_copied));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_log_level_remote_1_svc(qcsapi_set_log_level_rpcdata *__req, qcsapi_set_log_level_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_log_module_name index=(qcsapi_log_module_name)__req->index;
	char * params = (__req->params == NULL) ? NULL : __req->params->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_log_level(ifname, index, params);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_get_log_level_remote_1_svc(qcsapi_get_log_level_rpcdata *__req, qcsapi_get_log_level_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;
	qcsapi_log_module_name index=(qcsapi_log_module_name)__req->index;
	char * params = (__req->params == NULL) ? NULL : arg_alloc();

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_get_log_level(ifname, index, params);

	if (params) {
		__resp->params = malloc(sizeof(*__resp->params));
		__resp->params->data = params;
	}
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_remote_logging_remote_1_svc(qcsapi_set_remote_logging_rpcdata *__req, qcsapi_set_remote_logging_rpcdata *__resp, struct svc_req *rqstp)
{
	qcsapi_remote_log_action action_type=(qcsapi_remote_log_action)__req->action_type;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_remote_logging(action_type, __req->ipaddr);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_set_console_remote_1_svc(qcsapi_set_console_rpcdata *__req, qcsapi_set_console_rpcdata *__resp, struct svc_req *rqstp)
{
	qcsapi_console_action action_type=(qcsapi_console_action)__req->action_type;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_set_console(action_type);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_do_system_action_remote_1_svc(qcsapi_do_system_action_rpcdata *__req, qcsapi_do_system_action_rpcdata *__resp, struct svc_req *rqstp)
{
	char * action = (__req->action == NULL) ? NULL : __req->action->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_do_system_action(action);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_max_boot_cac_duration_remote_1_svc(qcsapi_wifi_set_max_boot_cac_duration_rpcdata *__req, qcsapi_wifi_set_max_boot_cac_duration_rpcdata *__resp, struct svc_req *rqstp)
{
	char * ifname = (__req->ifname == NULL) ? NULL : __req->ifname->data;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_max_boot_cac_duration(ifname, __req->max_boot_cac_duration);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_set_br_isolate_remote_1_svc(qcsapi_wifi_set_br_isolate_rpcdata *__req, qcsapi_wifi_set_br_isolate_rpcdata *__resp, struct svc_req *rqstp)
{
	qcsapi_br_isolate_cmd cmd=(qcsapi_br_isolate_cmd)__req->cmd;

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_set_br_isolate(cmd, __req->arg);

	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

bool_t qcsapi_wifi_get_br_isolate_remote_1_svc(qcsapi_wifi_get_br_isolate_rpcdata *__req, qcsapi_wifi_get_br_isolate_rpcdata *__resp, struct svc_req *rqstp)
{

	memset(__resp, 0, sizeof(*__resp));
	if (debug) { fprintf(stderr, "%s:%d %s pre\n", __FILE__, __LINE__, __FUNCTION__); }

	__resp->return_code = qcsapi_wifi_get_br_isolate(__req->result);

	__resp->result = __rpc_prepare_data(__req->result, sizeof(*__resp->result));
	if (debug) { fprintf(stderr, "%s:%d %s post\n", __FILE__, __LINE__, __FUNCTION__); }

	return 1;
}

