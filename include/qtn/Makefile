#
# Copyright (C)2014 Quantenna Communications
#

TOPDIR=../..

MUC_STATS_HEADER=./muc_txrx_stats.h
MUC_COMMON_STAT_HEADER=./qtn_muc_stats_print.h

.PHONY: FORCE

all: ${MUC_COMMON_STAT_HEADER}

${MUC_COMMON_STAT_HEADER}: ${MUC_STATS_HEADER} ${MUC_STATS_HEADER}.raw

${MUC_STATS_HEADER}: ${MUC_STATS_HEADER}.raw

${MUC_STATS_HEADER}.raw: FORCE
	@cat ${MUC_STATS_HEADER} | \
		${TOPDIR}/host/scripts/struct_get.sh | \
		grep -v "^[[:blank:]]*$$" > $@
	./muc_dbg_parse

