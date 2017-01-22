

INSTALL_DIR= $(PREFIX)/sbin/

SRC_DIR := $(TOPDIR)/../host/region_utils

Source_Name := regulatory_database_bin_print.c regulatory_database_bin_parser.c
SRC := $(addprefix $(SRC_DIR)/,$(Source_Name))
OBJ := $(Source_Name:%.c=%.o)
DEP := $(Source_Name:%.c=%.d)
Project := regulatory_database_bin_print

SOURCE_FILE_EXIST = $(findstring exist,$(foreach file,$(SRC),$(shell [ -f $(file) ] && echo 'exist')))



%.d: $(SRC_DIR)/%.c
	@set -e; rm -f $@; \
	$(COMPILE.c) -MM $< > $@.$$$$; \
	sed -e 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$
	@echo $@

%.o: $(SRC_DIR)/%.c
	$(COMPILE.c) $<


# build rules
.PHONY: install all dummy

ifneq ($(SOURCE_FILE_EXIST),)
all: $(Project)
else
all: dummy
	@echo "Source code in $(SRC_DIR) not found, use pre-build $(Project) instead"
endif

dummy:

$(Project) : $(OBJ) $(DEP)
	$(LINK.c) -o $@ $(filter-out %.d,$^)


install: all
	install -m 755 -d $(INSTALL_DIR)
	install -m 755 $(Project) $(INSTALL_DIR)


.PHONY: clean

clean:
ifneq ($(SOURCE_FILE_EXIST),)
	-rm $(OBJ) $(DEP) $(Project) 2>/dev/null
endif

-include $(DEP)
