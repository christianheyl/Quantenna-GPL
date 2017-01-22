#ifndef QTN_DEFCONF_H_
#define QTN_DEFCONF_H_

int qtn_defconf_vht_testbed_sta(const char* ifname);
int qtn_defconf_vht_testbed_ap(const char* ifname);
int qtn_defconf_vht_dut_sta(const char* ifname);
int qtn_defconf_vht_dut_ap(const char* ifname);
int qtn_defconf_pmf_dut(const char* ifname);
int qtn_defconf_hs2_dut(const char* ifname);
int qtn_defconf_11n_dut(const char* ifname);
int qtn_defconf_11n_testbed(const char* ifname);
int qtn_defconf_tdls_dut(const char* ifname);

#endif /* QTN_DEFCONF_H_ */
