
CFLAGS = $(TARGET_CFLAGS) -D_REENTRANT -Wall -Werror -std=gnu99
SIGMA_DIR = ../wfa/Sigma_Sample_DUT_Code-Linux_v8.1.1

# include dirs
CFLAGS += -I./
CFLAGS += -I$(SIGMA_DIR)/inc
CFLAGS += -I$(STAGING_DIR)/include
CFLAGS += -I$(STAGING_DIR)/../../../include/
CFLAGS += -I$(STAGING_DIR)/../../../drivers/include/shared
CFLAGS += -I$(STAGING_DIR)/../../../

# defines
CFLAGS += -D__LINUX

# Generate dependency information
CFLAGS += -MD -MP -MF .dep/$(@F).d

LIBS = -lqcsapi -lpthread -lm
LIBS += -lwfa
LDFLAGS += -L$(STAGING_DIR)/lib
LDFLAGS += -L$(SIGMA_DIR)/lib


CA_OBJ_LIST += \
	ap/qtn_ap.o   \
	sta/qtn_sta.o \
	qtn_ca_maps.o \
	qtn_ca.o      \
	common/qtn_ca_common.o

DUT_OBJ_LIST += \
	qtn_dut.o             \
	qtn_dut_ap_handler.o  \
	qtn_dut_sta_handler.o \
	qtn_dut_request.o     \
	common/qtn_dut_common.o \
	common/qtn_defconf.o

COMMON_OBJ_LIST += common/qtn_cmd_parser.o

all: qtn_ca qtn_dut

## TODO: add -Wl,-z,now
## -Wl,--as-needed

qtn_ca: $(CA_OBJ_LIST) $(COMMON_OBJ_LIST) $(SIGMA_DIR)/ca/wfa_ca.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS) -lz -lwfa_ca

qtn_dut: $(DUT_OBJ_LIST) $(COMMON_OBJ_LIST) $(SIGMA_DIR)/dut/wfa_dut.o $(SIGMA_DIR)/dut/wfa_dut_init.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS) -lz -lwfa_dut

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f qtn_ca qtn_dut $(CA_OBJ_LIST) $(DUT_OBJ_LIST) $(COMMON_OBJ_LIST)

.PHONY : all clean

# Include the dependency files, should be the last of the makefile
-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)
