DOXYGEN_PDF_INPUT_FILES = ../../../drivers/qdrv/qdrv_wlan.h
DOXYGEN_PDF_INPUT_FILES += ../../../include/qtn/muc_txrx_stats.h
DOXYGEN_PDF_INPUT_FILES += ../../../include/qtn/muc_phy_stats.h
DOXYGEN_PDF_INPUT_FILES += ../../../include/qtn/skb_recycle.h
DOXYGEN_PDF_INPUT_FILES += chapter1.txt chapter2.txt chapter3.txt chapter4.txt
DOXYGEN_EXCLUDE_STRUCTURES = qdrv_wlan_netdebug qdrv_wlan_ratedebug qdrv_wlan_memdebug memdebug_watchpt qtn_channel qtn_ratetable qtn_rateentry host_txif _temp_info host_fifo_if host_rxif host_scanif muc_bwa_stats qdrv_wlan qdrv_netdebug_event qdrv_netdebug_mem qdrv_netdebug_rate qdrv_netdebug_stats qdrv_netdebug_txbf qtn_node tx_power_cal qtn_stats qtn_stats_log qtn_skb_recycle_list qtn_chan_adm_stats
DOXYGEN_EXCLUDE_STRUCTURES_EXT = muc_tx_stats muc_rx_stats muc_rx_rates qtn_rate_stats_mcs_data qtn_rate_tx_stats qtn_rate_gen_stats qtn_rate_tx_stats_per_sec MUC_HT_NUM_RATES muc_bwa_stats qdrv_muc_rx_rates qdrv_radar_stats qdrv_rx_evm_array
TARGET_DIR = ../../../doxygen/pktlogger/latex

REFMAN:=Quantenna_pktlogger-INTERNAL-ONLY.pdf
REFMAN_EXT_NM:=Quantenna_pktlogger-external-no-muc.pdf
REFMAN_EXT:=Quantenna_pktlogger.pdf

all: doxygen_pdf

Doxyfile_pdf_internal:
	perl ../qtn_doxyfile_pdf.pl "Doxyfile_pdf_internal" "Pktlogger (INTERNAL ONLY)" "pktlogger" "$(REV_NUM)" \
		--internal	\
		$(DOXYGEN_PDF_INPUT_FILES) > $@

$(REFMAN): $(TARGET_DIR)/$(REFMAN)

$(TARGET_DIR)/$(REFMAN): clean Doxyfile_pdf_internal
	doxygen Doxyfile_pdf_internal
	./qtn_latex.sh
	make -C $(TARGET_DIR)
	pdftk $(TARGET_DIR)/refman.pdf background ../watermark.pdf output $(TARGET_DIR)/refman2.pdf
	pdftk ../pktlogger-title.pdf stamp ../../rev-num.pdf output title.pdf
	pdftk A=$(TARGET_DIR)/refman2.pdf B=./title.pdf cat B1 A2-end output xxx.pdf
	mv xxx.pdf $@
	-rm title.pdf $(TARGET_DIR)/refman.pdf $(TARGET_DIR)/refman2.pdf

Doxyfile_pdf_external:
	perl ../qtn_doxyfile_pdf.pl "Doxyfile_pdf" "Pktlogger" "pktlogger" "$(REV_NUM)"			\
		--exclusions "$(DOXYGEN_EXCLUDE_STRUCTURES)" \
		$(DOXYGEN_PDF_INPUT_FILES) > $@

$(REFMAN_EXT): $(TARGET_DIR)/$(REFMAN_EXT)

$(TARGET_DIR)/$(REFMAN_EXT): clean Doxyfile_pdf_external
	doxygen Doxyfile_pdf_external
	./qtn_latex.sh
	make -C $(TARGET_DIR)
	pdftk ../pktlogger-title.pdf stamp ../../rev-num.pdf output title.pdf
	pdftk A=$(TARGET_DIR)/refman.pdf B=./title.pdf cat B1 A2-end output xxx.pdf
	mv xxx.pdf $@
	-rm title.pdf $(TARGET_DIR)/refman.pdf

Doxyfile_pdf_no_muc:
	perl ../qtn_doxyfile_pdf.pl "Doxyfile_pdf" "Pktlogger (no MuC)" "pktlogger" "$(REV_NUM)" 	\
		--exclusions "$(DOXYGEN_EXCLUDE_STRUCTURES) $(DOXYGEN_EXCLUDE_STRUCTURES_EXT)"		\
		$(DOXYGEN_PDF_INPUT_FILES) > $@

$(REFMAN_EXT_NM): $(TARGET_DIR)/$(REFMAN_EXT_NM)

$(TARGET_DIR)/$(REFMAN_EXT_NM): clean Doxyfile_pdf_no_muc
	doxygen Doxyfile_pdf_no_muc
	./qtn_latex.sh
	make -C $(TARGET_DIR)
	pdftk ../pktlogger-title.pdf stamp ../../rev-num.pdf output title.pdf
	pdftk A=$(TARGET_DIR)/refman.pdf B=./title.pdf cat B1 A2-end output xxx.pdf
	mv xxx.pdf $@
	-rm title.pdf $(TARGET_DIR)/refman.pdf

clean:
	-rm -f Doxyfile_pdf_external
	-rm -f Doxyfile_pdf_internal
	-rm -f Doxyfile_pdf_no_muc
