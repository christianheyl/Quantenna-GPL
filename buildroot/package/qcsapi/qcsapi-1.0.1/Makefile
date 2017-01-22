
default: hop

QCSAPI_VERSION := $(shell basename `pwd` | sed 's/qcsapi-//')

RANLIB = ranlib

LIBS= -lpthread -lm -ldl

INCLUDES = -I. \
	   -I../../../../drivers/include/shared		\
	   -I../../../../include			\
	   -I../../../../				\
	   -I../../../../linux/include/			\
	   -I../../../../linux/arch/arc/include/	\
	   -I../../../../common/

CFLAGS += $(INCLUDES)

DYNAMIC = libqcsapi.so.$(QCSAPI_VERSION)
CLIENTLIB = qcsapi_sockrpc_client
CLIENTLIB_REALNAME = lib$(CLIENTLIB).so.$(QCSAPI_VERSION)
CALL_QCSAPILIB = call_qcsapi
CALL_QCSAPILIB_REALNAME = lib$(CALL_QCSAPILIB).so.$(QCSAPI_VERSION)

PROGS += call_qcsapi show_traffic_rates show_access_points monitor_wifi monitor_reset_device monitor_temperature qpm qfts
OTHER_PROGS += call_rftest monitor_rfenable sample_rpc_client
CALL_RPC_PROGS += call_qcsapi_sockrpc call_qcsapi_pcie call_qcsapi_rpcd call_qcsapi_sockraw
RPC_PROGS += qcsapi_sockrpc qcsapi_pcie qcsapi_sockraw qcsapi_rpcd

EXTRA_CFLAGS += -I.
EXTRA_CFLAGS += -DCONFIG_QTN_80211K_SUPPORT
ifeq ($(board_config), qtm710_rgmii_config)
EXTRA_CFLAGS += -DCUSTOMER_SPEC_RGMII
endif

ifneq ($(filter topaz_rgmii_config topaz_vzn_config,$(board_config)),)
EXTRA_CFLAGS += -DCUSTOMER_SPEC_BBIC4_RGMII -DCUSTOMER_SPEC_RGMII
endif

ifeq ($(board_config), topaz_config)
EXTRA_CFLAGS += -DCUSTOMER_SPEC_VB
endif

ifeq ($(board_config), topaz_amber_medium_config)
EXTRA_CFLAGS += -DTOPAZ_AMBER_IP_APP
endif

# Install directories
INSTALL_DIR = $(PREFIX)/sbin
INSTALL_LIB = $(PREFIX)/lib
INSTALL_DIR_STAGING = $(PREFIX)/../staging_dir/sbin
INSTALL_LIB_STAGING = $(PREFIX)/../staging_dir/lib
INSTALL_INC = $(PREFIX)/../../include
INSTALL_INCLUDE_STAGING = $(PREFIX)/../staging_dir/include/qtn
HFILES = qcsapi.h

LDFLAGS += -L$(INSTALL_LIB_STAGING)

# locations for rpcgen code
RPC_DEF = call_qcsapi_rpc
CALL_RPC_GEN_DIR = call_qcsapi_rpc/generated
CALL_RPC_GEN_PREFIX = $(CALL_RPC_GEN_DIR)/$(RPC_DEF)
CALL_RPC_SRCZIP = call_qcsapi_client_src.zip
RPC_CLIENT_SRCZIP = libqcsapi_client_src.zip
RPC_GEN_PREFIX = qcsapi_rpc/generated/qcsapi_rpc

# rpcgen generated code has unused variables
$(CALL_RPC_GEN_PREFIX)_xdr.o: EXTRA_CFLAGS += -Wno-unused-variable -Wno-strict-aliasing
$(RPC_GEN_PREFIX)_xdr.o: EXTRA_CFLAGS += -Wno-unused-variable -Wno-strict-aliasing
# Strict aliasing is required for the RPC client
call_qcsapi.o: EXTRA_CFLAGS += -DBUILDIN_TARGET_BOARD -Wstrict-aliasing


# force prerequisites for the automatically generated source files
$(CALL_RPC_GEN_PREFIX)_clnt.c $(CALL_RPC_GEN_PREFIX)_svc.c $(CALL_RPC_GEN_PREFIX)_xdr.c $(CALL_RPC_GEN_PREFIX).h: FORCE
	make -C call_qcsapi_rpc generate

$(RPC_GEN_PREFIX)_clnt.c $(RPC_GEN_PREFIX)_svc.c $(RPC_GEN_PREFIX)_xdr.c $(RPC_GEN_PREFIX).h $(RPC_GEN_PREFIX)_clnt_adapter.c $(RPC_GEN_PREFIX)_svc_adapter.c: FORCE
	make -C qcsapi_rpc generate

# force prerequisites that non generated code has on generated code
CALL_QCSAPI_DEPENDS_ON_GENERATED = $(shell grep -l /generated/ `find call_qcsapi_rpc/ -name \*.c` | grep -v /generated/)
QCSAPI_CLIENT_DEPENDS_ON_GENERATED = $(shell grep -l /generated/ `find qcsapi_rpc/ -name \*.c` | grep -v /generated/)
$(CALL_QCSAPI_DEPENDS_ON_GENERATED): $(CALL_RPC_GEN_PREFIX).h
$(QCSAPI_CLIENT_DEPENDS_ON_GENERATED): $(RPC_GEN_PREFIX).h

# include gcc automatic dependencies
include $(shell find . -name .\*.d)

# list dependencies based on generated dependency files
CHECKOUT_LEVEL = ../../../..
deps.txt: hop
	cat `find . -name .\*.d` | \
		perl -ne 's/[^\w-\/\.]/\n/g;print;' | \
		grep -vE '\.o$$' | \
		xargs -ixx readlink -f xx | \
		sed s@`cd $(CHECKOUT_LEVEL) ; pwd`/@@ | \
		grep -vE '^\/' | \
		sort | uniq | \
		cat > $@

# force all rebuild if the Makefile changes
%.o: Makefile

%.o: %.c
	${CC} ${XCFLAGS} ${CFLAGS} ${EXTRA_CFLAGS} $< -c -o $@ -MD -MF ${@D}/.${@F}.d

# canned recipes for building programs
define build-prog
	$(CC) $(LDFLAGS) $(XCFLAGS) $(filter %.o, $^) -o $@ -lqcsapi -lz $(LIBS)
endef

define build-prog-nolib
	$(CC) $(LDFLAGS) $(XCFLAGS) $(filter %.o, $^) -o $@
endef

define build-prog-clientlib
	$(CC) $(LDFLAGS) $(XCFLAGS) $(filter %.o, $^) -o $@ -l$(CLIENTLIB) $(LIBS)
endef

define build-prog-client
	$(CC) $(LDFLAGS) $(XCFLAGS) $(filter %.o, $^) -o $@ -l$(CLIENTLIB) -l$(CALL_QCSAPILIB) $(LIBS)
endef

define build-lib
	$(eval __SONAME = $(shell echo ${@F} | perl -ne 's/\.so\.(\d+)\.\d+\.\d+.*?$$/.so.$$1/;print;'))
	$(CC) -shared -o $@ -Wl,-soname,$(__SONAME) $(LIBS) -lc $^
endef

CALL_QCSAPI_OBJS = call_qcsapi.o qcsapi_driver.o qcsapi_output.o qcsapi_sem.o qcsapi_util.o
SAMPLE_RPC_CLIENT_OBJS = qcsapi_rpc_sample/c_rpc_qcsapi_sample.o
CALL_QCSAPI_RPC_OBJS = call_qcsapi_rpc/client/call_qcsapi_client.o $(CALL_RPC_GEN_PREFIX)_clnt.o $(CALL_RPC_GEN_PREFIX)_xdr.o
QCSAPI_COMMON_SERVER_OBJS = qcsapi_rpc_common/server/qtn_start_rpc_svc.o \
				qcsapi_rpc_common/server/rpc_raw_svc.o \
				qcsapi_rpc_common/common/rpc_raw.o \
				$(QCSAPI_PCIE_SERVER_OBJS)

QCSAPI_PCIE_CLIENT_OBJS = qcsapi_rpc_common/client/rpc_pci_clnt.o
QCSAPI_PCIE_CLIENT_SRC = qcsapi_rpc_common/client/rpc_pci_clnt.c

QCSAPI_PCIE_SERVER_OBJS = qcsapi_rpc_common/server/rpc_pci_svc.o
QCSAPI_PCIE_SERVER_SRC = qcsapi_rpc_common/server/rpc_pci_svc.c

QCSAPI_RAW_CLIENT_OBJS = qcsapi_rpc_common/client/rpc_raw_clnt.o qcsapi_rpc_common/common/rpc_raw.o

$(CLIENTLIB_REALNAME): $(RPC_GEN_PREFIX)_xdr.o $(RPC_GEN_PREFIX)_clnt_adapter.o
	$(call build-lib)

$(CALL_QCSAPILIB_REALNAME): $(CALL_QCSAPI_OBJS)
	$(call build-lib)

call_qcsapi: install_clientlib_staging call_qcsapi_local.o
	$(CC) $(LDFLAGS) $(XCFLAGS) $(filter %.o, $^) -o $@ -lqcsapi -lz -l$(CALL_QCSAPILIB) $(LIBS)

sample_rpc_client: $(SAMPLE_RPC_CLIENT_OBJS)  qcsapi_rpc_common/client/find_host_addr.o
	$(build-prog-clientlib)

call_rftest: call_rftest.o qcsapi_rftest.o
	$(build-prog)

monitor_reset_device: $(CALL_QCSAPI_RPC_OBJS) qcsapi_rpc_common/client/find_host_addr.o monitor_reset_device.o
	$(build-prog)

monitor_temperature: monitor_temperature.o
	$(build-prog)

qfts: qcsapi_rpc_common/common/rpc_raw.o qfts.o
	$(CC) $(LDFLAGS) $(XCFLAGS) $(filter %.o, $^) -o $@ -lz

show_traffic_rates: show_traffic_rates.o
	$(build-prog)

show_access_points: show_access_points.o
	$(build-prog)

monitor_wifi: monitor_wifi.o
	$(build-prog)

monitor_rfenable: $(CALL_QCSAPI_RPC_OBJS) qcsapi_rpc_common/client/find_host_addr.o monitor_rfenable.o
	$(build-prog)

call_qcsapi_sockrpc: $(CALL_QCSAPI_RPC_OBJS) \
				qcsapi_rpc_common/client/find_host_addr.o \
				qcsapi_rpc_common/client/qftc.o \
				call_qcsapi_rpc/client/socket/call_qcsapi_sockrpc.o \
				qcsapi_rpc_common/common/rpc_raw.o
	$(build-prog-nolib)

call_qcsapi_pcie_dep: FORCE
	${CC} ${XCFLAGS} ${CFLAGS} ${EXTRA_CFLAGS} -DPCIE_RPC_TYPE=RPC_TYPE_CALL_QCSAPI_PCIE \
		-c $(QCSAPI_PCIE_CLIENT_SRC) \
		-o $(QCSAPI_PCIE_CLIENT_OBJS) -MD -MF ${@D}/.${@F}.d

call_qcsapi_rpcd_dep: FORCE
	${CC} ${XCFLAGS} ${CFLAGS} ${EXTRA_CFLAGS} -DPCIE_RPC_TYPE=RPC_TYPE_CALL_QCSAPI_PCIE \
		-c $(QCSAPI_PCIE_SERVER_SRC) \
		-o $(QCSAPI_PCIE_SERVER_OBJS) -MD -MF ${@D}/.${@F}.d

qcsapi_pcie_dep: FORCE
	${CC} ${XCFLAGS} ${CFLAGS} ${EXTRA_CFLAGS} -DPCIE_RPC_TYPE=RPC_TYPE_QCSAPI_PCIE \
		-c $(QCSAPI_PCIE_CLIENT_SRC) \
		-o $(QCSAPI_PCIE_CLIENT_OBJS) -MD -MF ${@D}/.${@F}.d

qcsapi_rpcd_dep: FORCE
	${CC} ${XCFLAGS} ${CFLAGS} ${EXTRA_CFLAGS} -DPCIE_RPC_TYPE=RPC_TYPE_QCSAPI_PCIE \
		-c $(QCSAPI_PCIE_SERVER_SRC) \
		-o $(QCSAPI_PCIE_SERVER_OBJS) -MD -MF ${@D}/.${@F}.d

call_qcsapi_pcie: call_qcsapi_pcie_dep $(CALL_QCSAPI_RPC_OBJS) \
			call_qcsapi_rpc/client/pcie/call_qcsapi_pcie.o \
			$(QCSAPI_PCIE_CLIENT_OBJS)
	$(build-prog-nolib)

call_qcsapi_sockraw: $(CALL_QCSAPI_RPC_OBJS) \
			call_qcsapi_rpc/client/socket_raw/call_qcsapi_sockraw.o \
			qcsapi_rpc_common/client/qftc.o \
			$(QCSAPI_RAW_CLIENT_OBJS)
	$(build-prog-nolib)

call_qcsapi_rpcd: call_qcsapi_rpcd_dep $(CALL_QCSAPI_OBJS) \
			call_qcsapi_rpc/server/call_qcsapi_rpcd.o \
			$(CALL_RPC_GEN_PREFIX)_svc.o \
			$(CALL_RPC_GEN_PREFIX)_xdr.o $(QCSAPI_COMMON_SERVER_OBJS)
	$(build-prog)

qcsapi_sockrpc: install_clientlib_staging \
			qcsapi_rpc/client/socket/qcsapi_socket_rpc_client.o \
			qcsapi_rpc_common/client/find_host_addr.o
	$(build-prog-client)

qcsapi_pcie: install_clientlib_staging qcsapi_pcie_dep \
			qcsapi_rpc/client/pcie/qcsapi_pcie_rpc_client.o \
			$(QCSAPI_PCIE_CLIENT_OBJS)
	$(build-prog-client)

qcsapi_sockraw: install_clientlib_staging \
			qcsapi_rpc/client/socket_raw/qcsapi_socketraw_rpc_client.o \
			qcsapi_rpc_common/client/qftc.o \
			$(QCSAPI_RAW_CLIENT_OBJS)
	$(build-prog-client)

qcsapi_rpcd: qcsapi_rpcd_dep qcsapi_rpc/server/qcsapi_server.o $(RPC_GEN_PREFIX)_xdr.o \
			$(RPC_GEN_PREFIX)_svc.o $(RPC_GEN_PREFIX)_svc_adapter.o \
			$(QCSAPI_COMMON_SERVER_OBJS)
	$(build-prog)

COMMON_RPC_CLIENT_SRCS = $(shell find qcsapi_rpc_common/client/ qcsapi_rpc_common/common/ -name \*.[ch])

CALL_RPC_CLIENT_SRCS = \
	$(COMMON_RPC_CLIENT_SRCS)				\
	$(shell find call_qcsapi_rpc/client/ -name \*.[ch])	\
	$(CALL_RPC_GEN_PREFIX)_clnt.c				\
	$(CALL_RPC_GEN_PREFIX)_xdr.c				\
	$(CALL_RPC_GEN_PREFIX).h				\

RPC_CLIENT_SRCS = \
	$(COMMON_RPC_CLIENT_SRCS)				\
	$(shell find qcsapi_rpc/client/ -name \*.[ch])		\
	$(shell find qcsapi_rpc_sample/ -name \*.[ch])		\
	$(RPC_GEN_PREFIX)_clnt_adapter.c			\
	$(RPC_GEN_PREFIX)_xdr.c					\
	$(RPC_GEN_PREFIX).h					\
	$(patsubst %.o,%.c,$(CALL_QCSAPI_OBJS))			\
	$(patsubst %.o,%.h,$(CALL_QCSAPI_OBJS))			\
	qcsapi.h						\

$(CALL_RPC_SRCZIP): $(CALL_RPC_CLIENT_SRCS) FORCE
	rm -f $@
	zip $@ $(CALL_RPC_CLIENT_SRCS)
	zip -j $@ call_qcsapi_rpc/client/Makefile

TMPROOT=$(shell pwd)/.tmp/
TMPZIP=$(TMPROOT)/tmp.zip
TMPDIR=$(TMPROOT)/staging/

$(RPC_CLIENT_SRCZIP): $(RPC_CLIENT_SRCS) FORCE
	rm -rf $(TMPROOT)
	mkdir $(TMPROOT)
	rm -f $@
	for header in `gcc $(RPC_CLIENT_SRCS) -MM $(INCLUDES) -I. |	\
			perl -ne 's/\s+/\n/g;print;' |			\
			grep -E '\.h\s*$$' |				\
			sort | uniq | grep -v linux` ; do		\
		SRC=$$header ;\
		TARGET=`echo $$SRC | perl -ne 's/^.*include\///g;s/shared\///g;s/^.*common\//common\//g;print;'` ;\
		TARGET_DIR=`dirname $$TARGET` ;\
		mkdir -p $(TMPDIR)/$$TARGET_DIR && cp -v $$SRC $(TMPDIR)/$$TARGET ;\
		done
	zip -j $(TMPZIP) qcsapi_rpc/client/Makefile
	zip $(TMPZIP) $(RPC_CLIENT_SRCS)
	(cd $(TMPDIR) && zip -r $(TMPZIP) ./*)
	cp $(TMPZIP) $@
	rm -rf $(TMPROOT) $(TMPZIP)

all_srczips: $(CALL_RPC_SRCZIP) $(RPC_CLIENT_SRCZIP)

define __test_src_zip
	rm -rf $1
	mkdir -p $1
	cd $1 && unzip ../$<
	cd $1 && make
	cd $1 && make clean
	rm -rf $1
endef

test_call_qcsapi_srczip: $(CALL_RPC_SRCZIP)
	$(call __test_src_zip,__test_x86_$<)

test_clientlib_srczip: $(RPC_CLIENT_SRCZIP)
	$(call __test_src_zip,__test_x86_$<)

test_srczips: test_call_qcsapi_srczip test_clientlib_srczip


.PHONY: FORCE clean distclean install_inc install_lib install_lib_staging install_clientlib install_clientlib_staging	\
	install echo_vars all_srczips test_rpc_srczip test_call_qcsapi_srczip test_clientlib_srczip default hop

hop:
	make -C ../../../ qcsapi

clean distclean:
	rm -f ${PROGS} $(CALL_RPC_SRCZIP) $(RPC_CLIENT_SRCZIP) $(EXTRA_PROGS) $(RPC_PROGS) $(CALL_RPC_PROGS)
	rm -f *.so*
	rm -f deps.txt
	find . -type f -name \*.o -delete
	find . -type f -name .\*.d -delete
	-make -C call_qcsapi_rpc clean
	-make -C qcsapi_rpc clean

define do_liblinks
	$(eval __LDNAME = $(shell echo $2 | perl -ne 's/\.so\.(\d+)\.\d+\.\d+.*?$$/.so/;print;'))
	$(eval __SONAME = $(shell echo $2 | perl -ne 's/\.so\.(\d+)\.\d+\.\d+.*?$$/.so.$$1/;print;'))
	(cd $1 && ln -fs $2 $(__SONAME) && ln -fs $(__SONAME) $(__LDNAME))
endef

$(INSTALL_LIB)/%: $(INSTALL_LIB_STAGING)/%
	install -m 755 -d ${@D}
	install -m 644 $< ${@D}
	$(call do_liblinks,${@D},${<F})

$(INSTALL_LIB_STAGING)/%: %
	install -m 755 -d ${@D}
	install -m 644 $< ${@D}
	$(STRIP) $@
	$(call do_liblinks,${@D},${<F})

$(INSTALL_DIR)/%: $(INSTALL_DIR_STAGING)/%
	install -m 755 -d ${@D}
	install -m 755 $< ${@D}

$(INSTALL_DIR_STAGING)/%: %
	install -m 755 -d ${@D}
	install -m 755 $< ${@D}
	$(STRIP) $@

install_inc: qcsapi.h qcsapi_rftest.h
	install -m 755 -d $(INSTALL_INC)
	install -m 644 $^ $(INSTALL_INC)
	install -m 755 -d $(INSTALL_INCLUDE_STAGING)
	install -m 644 $^ $(INSTALL_INCLUDE_STAGING)

install_lib: $(INSTALL_LIB)/$(DYNAMIC)
install_lib_staging: $(INSTALL_LIB_STAGING)/$(DYNAMIC)
install_clientlib: $(INSTALL_LIB)/$(CLIENTLIB_REALNAME)
install_clientlib_staging: $(INSTALL_LIB_STAGING)/$(CLIENTLIB_REALNAME)
install_calllib: $(INSTALL_LIB)/$(CALL_QCSAPILIB_REALNAME)
install_calllib_staging: $(INSTALL_LIB_STAGING)/$(CALL_QCSAPILIB_REALNAME)

build_all: ${PROGS} ${RPC_PROGS} ${CALL_RPC_PROGS} ${EXTRA_PROGS}

install: ${PROGS:%=$(INSTALL_DIR)/%} ${EXTRA_PROGS:%=$(INSTALL_DIR)/%}

$(PROGS): install_lib_staging install_inc install_calllib_staging
$(PROGS:%=$(INSTALL_DIR)/%): install_lib install_calllib
$(RPC_PROGS:%=$(INSTALL_DIR)/%): install_clientlib install_calllib

echo_vars:
	echo $(QCSAPI_VERSION)

cscope:
	(find . -name \*.[ch] ; for f in `find . -name \*.d` ; do cat $$f ; done) | perl -ne 's/[\\\:]//g;s/\s+/\n/g;print;' | grep -vP '\.o\s*$$' | sort | xargs -ixx readlink -f xx | uniq > cscope.files
	cscope -b -q
	ctags -L cscope.files
