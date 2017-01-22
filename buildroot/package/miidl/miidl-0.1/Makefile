PHONY += clean
.PHONY: $(PHONY)

CC ?= gcc
MIIDL_ARCH=$(shell basename $(CC))
MIIDL=build/$(MIIDL_ARCH)/miidl
ALL_BINARIES = $(MIIDL)

all:	${ALL_BINARIES}

$(MIIDL): $(MIIDL).o
	$(CC) $^ -o $@

-include $(shell find . -name \*.d)

build/$(MIIDL_ARCH)/%.o: %.c
	-mkdir -p $(@D)
	$(CC) $(CFLAGS) -Wall -Wextra -Wshadow -Werror -ansi -pedantic -O2 -g $< -c -o $@ -MD -MF $@.d

clean:
	rm -rf build *.d *.o
