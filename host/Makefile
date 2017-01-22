#
# (C) Copyright 2007 Quantenna Communications Inc.
#

modules := utilities
modules_clean := $(patsubst %, %_clean, $(modules)) umsdl_clean qcsapi-client_clean pktlogger
modules_distclean := $(patsubst %, %_distclean, $(modules)) umsdl_distclean qcsapi-client_distclean

.PHONY: umsdl stat_parser struct-extract qcsapi-client qevt-client qregcheck \
$(modules) $(modules_clean) $(modules_distclean)

POST_ANALYSIS=pktlogger/post_analysis_tools.tar

all: umsdl stat_parser struct-extract qcsapi-client qevt-client $(modules)

clean: $(modules_clean)

distclean: $(modules_distclean)

umsdl:
	make -C umsdl BUILD_TARBALL=y

qcsapi-client:
	make -C qcsapi-client clean zips

qregcheck:
	make -C qregcheck all

qevt-client:
	make -C qevt-client

ifneq ($(modules),)
$(modules):
	make -C $@
endif

$(modules_clean):
	make -C $(patsubst %_clean, %, $@) clean

$(modules_distclean):
	make -C $(patsubst %_distclean, %, $@) distclean

stat_parser: ${POST_ANALYSIS}
	make -C pktlogger all

struct-extract:
	if [ -d struct-extract ] ; then make -C struct-extract clean se_prepro nodeps ; fi

ll_trace:
	if [ -d ll_trace ] ; then
		echo "LL trace"
	fi

${POST_ANALYSIS}: struct-extract
	if [ -d struct-extract -a -d ${@D} ] ; then tar cfv $@ struct-extract/ ; fi

