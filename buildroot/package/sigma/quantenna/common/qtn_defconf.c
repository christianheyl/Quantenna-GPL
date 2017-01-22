#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "drivers/qdrv/qdrv_bld.h"
#include "qtn/qcsapi.h"
#include <linux/wireless.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <sys/ioctl.h>

#include "qsigma_log.h"
#include "qtn_dut_common.h"
#include "qtn_defconf.h"

enum {
	DEFAULT_VHT_CHANNEL = 36
};

static void reset_rts_cts_settings(struct qtn_dut_config *conf, const char* ifname)
{
	char tmpbuf[64];
	conf->bws_enable = 0;
	conf->bws_dynamic = 0;
	conf->force_rts = 0;
	conf->update_settings = 0;

	/* disable CTS with fixed bw */
	snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_cts_bw 0", ifname);
	system(tmpbuf);

	qtn_set_rts_settings(ifname, conf);
}

static void enable_rx_bw_signaling_support(const char* ifname)
{
	char tmpbuf[64];
	snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_rx_bws_ndpa 1", ifname);
	system(tmpbuf);
}

int qtn_defconf_vht_testbed_sta(const char* ifname)
{
	int ret;
	struct qtn_dut_config *conf;
	char tmpbuf[64];

	qtn_log("START: qtn_defconf_vht_testbed_sta");

	/*  Table 138: Testbed Default Mode STA
	 * ---------------------------------------------------------
	 * #  | Mode name                   | Default | Notes
	 * ---------------------------------------------------------
	 * 1  | Spatial streams             | 1       |
	 * 2  | Bandwidth                   | 80 MHz  |
	 * 3  | VHT MCS Set                 | 0-7     | MCS 8-9 off
	 * 4  | Short GI for 20 MHz         | Off     | for both Tx/Rx
	 * 5  | Short GI for 40 MHz         | Off     | for both Tx/Rx
	 * 6  | Short GI for 80 MHz         | Off     | for both Tx/Rx
	 * 7  | SU Transmit Beamforming     | Off     |
	 * 8  | SU Transmit Beamformee      | Off     |
	 * 9  | MU Transmit Beamformer      | Off     |
	 * 10 | MU Transmit Beamformee      | Off     |
	 * 11 | Transmit A-MSDU             | Off     |
	 * 12 | Receive A-MPDU with A-MSDU  | Off     |
	 * 13 | STBC 2x1 Transmit           | Off     |
	 * 14 | STBC 2x1 Receive            | Off     |
	 * 15 | LDPC                        | Off     |
	 * 16 | Operating Mode Notification | Off     | Transmit
	 * 17 | RTS with Bandwidth Signaling| Off     |
	 * 18 | Two-character Country Code  | Off     |
	 * 19 | Transmit Power Control      | Off     |
	 * 20 | Channel Switching           | Off     |
	 * ---------------------------------------------------------
	 */

	ret = qcsapi_wifi_set_phy_mode(ifname , "11ac");
	if (ret < 0) {
		qtn_error("error: cannot set 11ac, errcode %d", ret);
		return ret;
	}

	/* VHT mode */
	ret = qcsapi_wifi_set_vht(ifname, 1);
	if (ret < 0) {
		qtn_error("error: cannot enable vht, errcode %d", ret);
		return ret;
	}

	/* 1. Spatial streams, 1 */
	ret = qcsapi_wifi_set_nss_cap(ifname, qcsapi_mimo_vht, 1);

	if (ret < 0) {
		qtn_error("error: cannot set NSS capability, errcode %d", ret);
		return ret;
	}

	/* 2. Bandwidth, 80Mhz */
	ret = qcsapi_wifi_set_bw(ifname, qcsapi_bw_80MHz);
	if (ret < 0) {
		qtn_error("error: cannot set bw capability %d, errcode %d", qcsapi_bw_80MHz, ret);
		return ret;
	}

	/* 3. VHT MCS Set, 0-7 */
	snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_vht_mcs_cap %d",
			ifname,
			IEEE80211_VHT_MCS_0_7);
	system(tmpbuf);

	/* 4. Short GI for 20 MHz, Off, for both Tx/Rx
	 * 5. Short GI for 40 MHz, Off, for both Tx/Rx
	 * 6. Short GI for 80 MHz, Off, for both Tx/Rx
	 */

	/* disable dynamic GI selection */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_GI_probing, 0);
	if (ret < 0) {
		/* ignore error since qcsapi_GI_probing does not work for RFIC6 */
		qtn_error("error: disable dynamic GI selection, errcode %d", ret);
	}

	/* disable short GI */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_short_GI, 0);
	if (ret < 0) {
		qtn_error("error: disable short GI, errcode %d", ret);
		return ret;
	}

	/* 7. SU Transmit Beamforming, Off */
	/* 8. SU Transmit Beamformee, Off */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_beamforming, 0);
	if (ret < 0) {
		qtn_error("error: disable beamforming, errcode %d", ret);
		return ret;
	}

	/* 9. MU Transmit Beamformer, Off */
	/* 10. MU Transmit Beamformee, 0ff */
	enable_rx_bw_signaling_support(ifname);


	/* restore Ndpa_stainfo_mac to default */
	system("mu sta0 00:00:00:00:00:00");

	ret = qtn_set_mu_enable(0);
	if (ret < 0) {
		qtn_error("error: disable MU beamforming, errcode %d", ret);
		return ret;
	}

	/* 11. Transmit A-MSDU, Off
	 * 12. Receive A-MPDU with A-MSDU, Off
	 */
	ret = qcsapi_wifi_set_tx_amsdu(ifname, 0);
	if (ret < 0) {
		qtn_error("error: disable tx amsdu, errcode %d", ret);
		return ret;
	}

	/* 13. STBC 2x1 Transmit, Off
	 * 14. STBC 2x1 Receive, Off
	 */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_stbc, 0);
	if (ret < 0) {
		qtn_error("error: cannot set stbc, errcode %d", ret);
		return ret;
	}

	/* 15. LDPC, Off */
	snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_ldpc %d", ifname, 0);
	system(tmpbuf);

	/* 16. Operating Mode Notification, Off, Transmit */
	snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_vht_opmntf %d",
			ifname,
			0xFFFF);
	system(tmpbuf);

	/* 17. RTS with Bandwidth Signaling, Off */
	conf = qtn_dut_get_config(ifname);

	if (conf) {
		reset_rts_cts_settings(conf, ifname);

	} else {
		ret = -EFAULT;
		qtn_error("error: cannot get config, errcode %d", ret);
		return ret;
	}

	/* 18. Two-character Country Code, Off */
	/* 19. Transmit Power Control, Off */
	/* 20. Channel Switching, Off */

	qtn_log("END: qtn_defconf_vht_testbed_sta");

	return 0;
}


int qtn_defconf_vht_testbed_ap(const char* ifname)
{
	int ret;
	struct qtn_dut_config *conf;
	char tmpbuf[64];

	qtn_log("qtn_defconf_vht_testbed_ap");

	/*  Table 137: Testbed Default Mode AP
	 * ---------------------------------------------------------
	 * #  | Mode name                   | Default | Notes
	 * ---------------------------------------------------------
	 * 1  | Spatial streams             | 2       |
	 * 2  | Bandwidth                   | 80 MHz  |
	 * 3  | VHT MCS Set                 | 0-7     | MCS 8-9 off
	 * 4  | Short GI for 20 MHz         | Off     | for both Tx/Rx
	 * 5  | Short GI for 40 MHz         | Off     | for both Tx/Rx
	 * 6  | Short GI for 80 MHz         | Off     | for both Tx/Rx
	 * 7  | SU Transmit Beamforming     | Off     |
	 * 8  | SU Transmit Beamformee      | Off     |
	 * 9  | MU Transmit Beamformer      | Off     |
	 * 10 | MU Transmit Beamformee      | Off     |
	 * 11 | Transmit A-MSDU             | Off     |
	 * 12 | Receive A-MPDU with A-MSDU  | Off     |
	 * 13 | STBC 2x1 Transmit           | Off     |
	 * 14 | STBC 2x1 Receive            | Off     |
	 * 15 | LDPC                        | Off     |
	 * 16 | Operating Mode Notification | Off     | Transmit
	 * 17 | RTS with Bandwidth Signaling| Off     |
	 * 18 | Two-character Country Code  | Any     |
	 * 19 | Transmit Power Control      | Any     |
	 * 20 | Channel Switching           | Any     |
	 * ---------------------------------------------------------
	 */

	ret = qcsapi_wifi_set_phy_mode(ifname , "11ac");
	if (ret < 0) {
		qtn_error("error: cannot set 11ac, errcode %d", ret);
		return ret;
	}

	/* VHT mode */
	ret = qcsapi_wifi_set_vht(ifname, 1);
	if (ret < 0) {
		qtn_error("error: cannot enable vht, errcode %d", ret);
		return ret;
	}

	/* 1. Spatial streams, 2 */
	ret = qcsapi_wifi_set_nss_cap(ifname, qcsapi_mimo_vht, 2);

	if (ret < 0) {
		qtn_error("error: cannot set NSS capability, errcode %d", ret);
		return ret;
	}

	/* 2. Bandwidth, 80Mhz */
	ret = qcsapi_wifi_set_bw(ifname, qcsapi_bw_80MHz);
	if (ret < 0) {
		qtn_error("error: cannot set bw capability %d, errcode %d", qcsapi_bw_80MHz, ret);
		return ret;
	}

	/* 3. VHT MCS Set, 0-7 */
	snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_vht_mcs_cap %d",
			ifname,
			IEEE80211_VHT_MCS_0_7);
	system(tmpbuf);

	/* 4. Short GI for 20 MHz, Off, for both Tx/Rx
	 * 5. Short GI for 40 MHz, Off, for both Tx/Rx
	 * 6. Short GI for 80 MHz, Off, for both Tx/Rx
	 */

	/* disable dynamic GI selection */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_GI_probing, 0);
	if (ret < 0) {
		/* ignore error since qcsapi_GI_probing does not work for RFIC6 */
		qtn_error("error: disable dynamic GI selection, errcode %d", ret);
	}

	/* disable short GI */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_short_GI, 0);
	if (ret < 0) {
		qtn_error("error: disable short GI, errcode %d", ret);
		return ret;
	}

	/* 7. SU Transmit Beamforming, Off */
	/* 8. SU Transmit Beamformee, Off */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_beamforming, 0);
	if (ret < 0) {
		qtn_error("error: disable beamforming, errcode %d", ret);
		return ret;
	}

	/* 9. MU Transmit Beamformer, Off */
	/* 10. MU Transmit Beamformee, 0ff */
	enable_rx_bw_signaling_support(ifname);

	/* restore Ndpa_stainfo_mac to default */
	system("mu sta0 00:00:00:00:00:00");

	ret = qtn_set_mu_enable(0);
	if (ret < 0) {
		qtn_error("error: disable MU beamforming, errcode %d", ret);
		return ret;
	}

	/* 11. Transmit A-MSDU, Off
	 * 12. Receive A-MPDU with A-MSDU, Off
	 */
	ret = qcsapi_wifi_set_tx_amsdu(ifname, 0);
	if (ret < 0) {
		qtn_error("error: disable tx amsdu, errcode %d", ret);
		return ret;
	}

	/* 13. STBC 2x1 Transmit, Off
	 * 14. STBC 2x1 Receive, Off
	 */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_stbc, 0);
	if (ret < 0) {
		qtn_error("error: cannot set stbc, errcode %d", ret);
		return ret;
	}

	/* 15. LDPC, Off */
	snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_ldpc %d", ifname, 0);
	system(tmpbuf);

	/* 16. Operating Mode Notification, Off, Transmit */
	snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_vht_opmntf %d",
			ifname,
			0xFFFF);
	system(tmpbuf);

	/* 17. RTS with Bandwidth Signaling, Off */
	conf = qtn_dut_get_config(ifname);

	if (conf) {
		reset_rts_cts_settings(conf, ifname);
	} else {
		ret = -EFAULT;
		qtn_error("error: cannot get config, errcode %d", ret);
		return ret;
	}

	/* 18. Two-character Country Code, Any */
	/* 19. Transmit Power Control, Any */
	/* 20. Channel Switching, Any */

	return 0;
}

int qtn_defconf_vht_dut_sta(const char* ifname)
{
	int ret;
	struct qtn_dut_config *conf;
	char tmpbuf[64];

	qtn_log("qtn_defconf_vht_dut_sta, ifname %s", ifname);

	/*  Table 142: STAUT Default Mode
	 * ---------------------------------------------------------
	 * #  | Mode name                   | Default | Notes
	 * ---------------------------------------------------------
	 * 1  | Spatial streams             | 3       |
	 * 2  | Bandwidth                   | 80 MHz  |
	 * 3  | VHT MCS Set                 | 0-9     |
	 * 4  | Short GI for 20 MHz         | On      | for both Tx/Rx
	 * 5  | Short GI for 40 MHz         | On      | for both Tx/Rx
	 * 6  | Short GI for 80 MHz         | On      | for both Tx/Rx
	 * 7  | SU Transmit Beamformer      | On      |
	 * 8  | SU Transmit Beamformee      | On      |
	 * 9  | MU Transmit Beamformer      | Off     |
	 * 10 | MU Transmit Beamformee      | Off     |
	 * 11 | Transmit A-MSDU             | On      |
	 * 12 | Receive A-MPDU with A-MSDU  | On      |
	 * 13 | Tx STBC 2x1                 | On      |
	 * 14 | Rx STBC 2x1                 | On      |
	 * 15 | Tx LDPC                     | On      |
	 * 16 | Rx LDPC                     | On      |
	 * 17 | Operating Mode Notification | On      | Transmit
	 * 18 | RTS with Bandwidth Signaling| On      |
	 * 19 | Two-character Country Code  | On      |
	 * 20 | Transmit Power Control      | On      |
	 * 21 | Channel Switching           | On      |
	 * ---------------------------------------------------------
	 */

	ret = qcsapi_wifi_cancel_scan(ifname, 0);
	if (ret < 0) {
		qtn_error("error: can't cancel scan, error %d", ret);
	}

	ret = qcsapi_wifi_set_phy_mode(ifname , "11ac");
	if (ret < 0) {
		qtn_error("error: cannot set 11ac, errcode %d", ret);
		return ret;
	}

	ret = qcsapi_wifi_set_channel(ifname, DEFAULT_VHT_CHANNEL);
	if (ret < 0) {
		qtn_error("error: can't set channel, error %d", ret);
	}

	/* VHT mode */
	ret = qcsapi_wifi_set_vht(ifname, 1);
	if (ret < 0) {
		qtn_error("error: cannot enable vht, errcode %d", ret);
		return ret;
	}

	/* 1. Spatial streams, 3 */
	ret = qcsapi_wifi_set_nss_cap(ifname, qcsapi_mimo_vht, 3);

	if (ret < 0) {
		qtn_error("error: cannot set NSS capability, errcode %d", ret);
		return ret;
	}

	/* 2. Bandwidth, 80Mhz */
	ret = qcsapi_wifi_set_bw(ifname, qcsapi_bw_80MHz);
	if (ret < 0) {
		qtn_error("error: cannot set bw capability %d, errcode %d", qcsapi_bw_80MHz, ret);
		return ret;
	}

	/* 3. VHT MCS Set, 0-9 */
	snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_vht_mcs_cap %d",
			ifname,
			IEEE80211_VHT_MCS_0_9);
	system(tmpbuf);

	/* 4. Short GI for 20 MHz, Off, for both Tx/Rx
	 * 5. Short GI for 40 MHz, Off, for both Tx/Rx
	 * 6. Short GI for 80 MHz, Off, for both Tx/Rx
	 */

	/* enable dynamic GI selection */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_GI_probing, 1);
	if (ret < 0) {
		/* not supported on RFIC6, ignore error for now. */
		qtn_error("error: enable dynamic GI selection, errcode %d", ret);
	}

	/* enable short GI */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_short_GI, 1);
	if (ret < 0) {
		qtn_error("error: enable short GI, errcode %d", ret);
		return ret;
	}

	/* 7. SU Transmit Beamformer, On */
	/* 8. SU Transmit Beamformee, On */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_beamforming, 1);
	if (ret < 0) {
		qtn_error("error: enable beamforming, errcode %d", ret);
		return ret;
	}

	/* 9. MU Transmit Beamformer, Off */
	/* 10. MU Transmit Beamformee, 0ff */
	enable_rx_bw_signaling_support(ifname);

	/* restore Ndpa_stainfo_mac to default */
	system("mu sta0 00:00:00:00:00:00");

	ret = qtn_set_mu_enable(0);
	if (ret < 0) {
		qtn_error("error: disable MU beamforming, errcode %d", ret);
		return ret;
	}

	/* 11. Transmit A-MSDU, On
	 * 12. Receive A-MPDU with A-MSDU, On
	 */
	ret = qcsapi_wifi_set_tx_amsdu(ifname, 1);
	if (ret < 0) {
		qtn_error("error: disable tx amsdu, errcode %d", ret);
		return ret;
	}

	/* 13. Tx STBC 2x1, On
	 * 14. Rx STBC 2x1, On
	 */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_stbc, 1);
	if (ret < 0) {
		qtn_error("error: cannot set stbc, errcode %d", ret);
		return ret;
	}

	/* 15. Tx LDPC, On
	 * 16. Rx LDPC, On
	 */
	snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_ldpc %d", ifname, 1);
	system(tmpbuf);

	/* 17. Operating Mode Notification, On (if supported) */
	snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_vht_opmntf %d",
			ifname,
			0xFFFF);
	system(tmpbuf);

	/* 18. RTS with Bandwidth Signaling, On (if supported) */
	conf = qtn_dut_get_config(ifname);

	if (conf) {
		reset_rts_cts_settings(conf, ifname);
	} else {
		ret = -EFAULT;
		qtn_error("error: cannot get config, errcode %d", ret);
		return ret;
	}

	/* 19. Two-character Country Code, On (if supported) */
	/* 20. Transmit Power Control, On (if supported) */
	/* 21. Channel Switching, On (if supported) */

	return 0;
}

int qtn_defconf_vht_dut_ap(const char* ifname)
{
	qtn_log("qtn_defconf_vht_dut_ap");

	/*  Table 141: APUT Default Mode
	 * ---------------------------------------------------------
	 * #  | Mode name                   | Default | Notes
	 * ---------------------------------------------------------
	 * 1  | Spatial streams             | 3       |
	 * 2  | Bandwidth                   | 80 MHz  |
	 * 3  | VHT MCS Set                 | 0-9     |
	 * 4  | Short GI for 20 MHz         | On      | for both Tx/Rx
	 * 5  | Short GI for 40 MHz         | On      | for both Tx/Rx
	 * 6  | Short GI for 80 MHz         | On      | for both Tx/Rx
	 * 7  | SU Transmit Beamformer      | On      |
	 * 8  | SU Transmit Beamformee      | On      |
	 * 9  | MU Transmit Beamformer      | Off     |
	 * 10 | MU Transmit Beamformee      | Off     |
	 * 11 | Transmit A-MSDU             | On      |
	 * 12 | Receive A-MPDU with A-MSDU  | On      |
	 * 13 | Tx STBC 2x1                 | On      |
	 * 14 | Rx STBC 2x1                 | On      |
	 * 15 | Tx LDPC                     | On      |
	 * 16 | Rx LDPC                     | On      |
	 * 17 | Operating Mode Notification | On      | Transmit
	 * 18 | RTS with Bandwidth Signaling| On      |
	 * 19 | Two-character Country Code  | On      |
	 * 20 | Transmit Power Control      | On      |
	 * 21 | Channel Switching           | On      |
	 * ---------------------------------------------------------
	 */

	return qtn_defconf_vht_dut_sta(ifname);
}

int qtn_defconf_pmf_dut(const char* ifname)
{
	int ret;

	qtn_log("qtn_defconf_pmf_dut: ifname %s", ifname);


	ret = qcsapi_wifi_set_phy_mode(ifname , "11na");
	if (ret < 0) {
		qtn_error("error: cannot set 11an, errcode %d", ret);
		return ret;
	}


	/* 1. Spatial streams, 2 */
	ret = qcsapi_wifi_set_nss_cap(ifname, qcsapi_mimo_ht, 2);
	if (ret < 0) {
		qtn_error("error: cannot set NSS capability, errcode %d", ret);
		return ret;
	}

	/* 2. Bandwidth, 20Mhz */
	ret = qcsapi_wifi_set_bw(ifname, qcsapi_bw_20MHz);
	if (ret < 0) {
		qtn_error("error: cannot set bw capability %d, errcode %d", qcsapi_bw_20MHz, ret);
		return ret;
	}

	/* enable dynamic GI selection */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_GI_probing, 1);
	if (ret < 0) {
		/* not supported on RFIC6, ignore error for now. */
		qtn_error("error: enable dynamic GI selection, errcode %d", ret);
	}

	/* enable short GI */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_short_GI, 1);
	if (ret < 0) {
		qtn_error("error: enable short GI, errcode %d", ret);
		return ret;
	}

	return 0;
}

int qtn_defconf_hs2_dut(const char* ifname)
{
	int ret;

	qtn_log("qtn_defconf_hs2_dut, ifname %s", ifname);

	/* restore default hostapd config */
	system("test -e /scripts/hostapd.conf && "
		"cat /scripts/hostapd.conf > /mnt/jffs2/hostapd.conf && "
		"hostapd_cli reconfigure");

	ret = qcsapi_wifi_set_phy_mode(ifname , "11na");
	if (ret < 0) {
		qtn_error("error: cannot set 11ac, errcode %d", ret);
		return ret;
	}

	/* VHT mode */
	ret = qcsapi_wifi_set_vht(ifname, 0);
	if (ret < 0) {
		qtn_error("error: cannot enable vht, errcode %d", ret);
		return ret;
	}

	/* 1. Spatial streams, 2 */
	ret = qcsapi_wifi_set_nss_cap(ifname, qcsapi_mimo_ht, 2);

	if (ret < 0) {
		qtn_error("error: cannot set NSS capability, errcode %d", ret);
		return ret;
	}

	/* 2. Bandwidth, 20Mhz */
	ret = qcsapi_wifi_set_bw(ifname, qcsapi_bw_20MHz);
	if (ret < 0) {
		qtn_error("error: cannot set bw capability %d, errcode %d", qcsapi_bw_20MHz, ret);
		return ret;
	}

	/* 4. Short GI for 20 MHz, Off, for both Tx/Rx
	 * 5. Short GI for 40 MHz, Off, for both Tx/Rx
	 * 6. Short GI for 80 MHz, Off, for both Tx/Rx
	 */

	/* enable dynamic GI selection */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_GI_probing, 1);
	if (ret < 0) {
		/* not supported on RFIC6, ignore error for now. */
		qtn_error("error: enable dynamic GI selection, errcode %d", ret);
	}

	/* enable short GI */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_short_GI, 1);
	if (ret < 0) {
		qtn_error("error: enable short GI, errcode %d", ret);
		return ret;
	}

	ret = qcsapi_wifi_set_hs20_status(ifname, "1");
	if (ret < 0) {
		qtn_error("error: enable hs20, errcode %d", ret);
		return ret;
	}

	return 0;
}

int qtn_defconf_11n_dut(const char* ifname)
{
	int ret;

	qtn_log("qtn_defconf_11n_dut, ifname %s", ifname);

	ret = qcsapi_wifi_set_phy_mode(ifname , "11na");
	if (ret < 0) {
		qtn_error("error: cannot set 11na, errcode %d", ret);
		return ret;
	}

	/* Spatial streams, 4 */
	ret = qcsapi_wifi_set_nss_cap(ifname, qcsapi_mimo_ht, 4);
	if (ret < 0) {
		qtn_error("error: cannot set NSS capability, errcode %d", ret);
		return ret;
	}

	/* restore automatic bandwidth selection */
	system("set_fixed_bw -b auto");

	/* Bandwidth, 40Mhz */
	ret = qcsapi_wifi_set_bw(ifname, qcsapi_bw_40MHz);
	if (ret < 0) {
		qtn_error("error: cannot set bw capability %d, errcode %d", qcsapi_bw_40MHz, ret);
		return ret;
	}

	/* enable dynamic GI selection */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_GI_probing, 1);
	if (ret < 0) {
		/* not supported on RFIC6, ignore error for now. */
		qtn_error("error: enable dynamic GI selection, errcode %d", ret);
	}

	/* enable short GI */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_short_GI, 1);
	if (ret < 0) {
		qtn_error("error: enable short GI, errcode %d", ret);
		return ret;
	}

	/* SU Transmit Beamformer, Off */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_beamforming, 0);
	if (ret < 0) {
		qtn_error("error: enable beamforming, errcode %d", ret);
		return ret;
	}

	/* Transmit A-MSDU, On
	 * Receive A-MPDU with A-MSDU, On
	 */
	ret = qcsapi_wifi_set_tx_amsdu(ifname, 1);
	if (ret < 0) {
		qtn_error("error: enable tx amsdu, errcode %d", ret);
		return ret;
	}

	/* Tx/Rx STBC 2x1, On */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_stbc, 1);
	if (ret < 0) {
		qtn_error("error: cannot set stbc, errcode %d", ret);
		return ret;
	}

	return 0;
}

int qtn_defconf_11n_testbed(const char* ifname)
{
	int ret;
	char tmpbuf[128];

	qtn_log("qtn_defconf_11n_testbed, ifname %s", ifname);

	ret = qcsapi_wifi_set_phy_mode(ifname , "11na");
	if (ret < 0) {
		qtn_error("error: cannot set 11na, errcode %d", ret);
		return ret;
	}

	/* Spatial streams, 2 */
	ret = qcsapi_wifi_set_nss_cap(ifname, qcsapi_mimo_ht, 2);
	if (ret < 0) {
		qtn_error("error: cannot set NSS capability, errcode %d", ret);
		return ret;
	}

	/* Bandwidth, 40Mhz */
	system("set_fixed_bw -b auto");
	ret = qcsapi_wifi_set_bw(ifname, qcsapi_bw_40MHz);
	if (ret < 0) {
		qtn_error("error: cannot set bw capability %d, errcode %d", qcsapi_bw_40MHz, ret);
		return ret;
	}

	/* disable dynamic GI selection */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_GI_probing, 0);
	if (ret < 0) {
		/* not supported on RFIC6, ignore error for now. */
		qtn_error("error: enable dynamic GI selection, errcode %d", ret);
	}

	/* disable short GI */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_short_GI, 0);
	if (ret < 0) {
		qtn_error("error: enable short GI, errcode %d", ret);
		return ret;
	}

	/* SU Transmit Beamformer, Off */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_beamforming, 0);
	if (ret < 0) {
		qtn_error("error: enable beamforming, errcode %d", ret);
		return ret;
	}

	/* Transmit A-MSDU, Off
	 * Receive A-MPDU with A-MSDU, Off
	 */
	ret = qcsapi_wifi_set_tx_amsdu(ifname, 0);
	if (ret < 0) {
		qtn_error("error: enable tx amsdu, errcode %d", ret);
		return ret;
	}

	/* Tx/Rx STBC 2x1, Off*/
	ret = qcsapi_wifi_set_option(ifname, qcsapi_stbc, 0);
	if (ret < 0) {
		qtn_error("error: cannot set stbc, errcode %d", ret);
		return ret;
	}

	/* 15. Tx LDPC, On
	 * 16. Rx LDPC, On
	 */
	snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_ldpc %d", ifname, 1);
	system(tmpbuf);

	return 0;
}

int qtn_defconf_tdls_dut(const char* ifname)
{
	int ret;

	qtn_log("qtn_defconf_tdls_dut, ifname %s", ifname);

	ret = qcsapi_wifi_set_phy_mode(ifname , "11na");
	if (ret < 0) {
		qtn_error("error: cannot set 11ac, errcode %d", ret);
		return ret;
	}

	/* VHT mode */
	ret = qcsapi_wifi_set_vht(ifname, 0);
	if (ret < 0) {
		qtn_error("error: cannot enable vht, errcode %d", ret);
		return ret;
	}

	/* 1. Spatial streams, 2 */
	ret = qcsapi_wifi_set_nss_cap(ifname, qcsapi_mimo_ht, 2);

	if (ret < 0) {
		qtn_error("error: cannot set NSS capability, errcode %d", ret);
		return ret;
	}

	/* 2. Bandwidth, 20Mhz */
	ret = qcsapi_wifi_set_bw(ifname, qcsapi_bw_20MHz);
	if (ret < 0) {
		qtn_error("error: cannot set bw capability %d, errcode %d", qcsapi_bw_20MHz, ret);
		return ret;
	}

	/* 4. Short GI for 20 MHz, Off, for both Tx/Rx
	 * 5. Short GI for 40 MHz, Off, for both Tx/Rx
	 * 6. Short GI for 80 MHz, Off, for both Tx/Rx
	 */

	/* enable dynamic GI selection */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_GI_probing, 1);
	if (ret < 0) {
		/* not supported on RFIC6, ignore error for now. */
		qtn_error("error: enable dynamic GI selection, errcode %d", ret);
	}

	/* enable short GI */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_short_GI, 1);
	if (ret < 0) {
		qtn_error("error: enable short GI, errcode %d", ret);
		return ret;
	}

	ret = qcsapi_wifi_enable_tdls(ifname, 1);
	if (ret < 0) {
		qtn_error("error: can't enable TDLS, errcode %d", ret);
		return ret;
	}

	ret = qcsapi_wifi_set_tdls_params(ifname, qcsapi_tdls_discovery_interval, 0);
	if (ret < 0) {
		qtn_error("error: can't set discovery_interval, errcode %d", ret);
	}

	ret = qcsapi_wifi_set_tdls_params(ifname, qcsapi_tdls_mode, 1);
	if (ret < 0) {
		qtn_error("error: can't set tdls_mode, errcode %d", ret);
	}

	return 0;
}
