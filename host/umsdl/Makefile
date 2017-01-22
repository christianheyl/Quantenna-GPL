#
# (C) Copyright 2007 Quantenna Communications Inc.
#

umsdlsrcfiles := umsdl.c libumsdl.c
umsfiles := $(umsdlsrcfiles) umsdl.h ums_platform.h Makefile bin2ums.c 

.PHONY: umsdl bin2ums

alltargets = bin2ums umsdl

ifeq ($(BUILD_TARBALL),y)
alltargets += umsdl-tarball
endif

all: $(alltargets)

umsdl: ums_platform.h
	gcc $(umsdlsrcfiles) -Wall -o umsdl	

ums_platform.h:
	cp ../../common/ums_platform.h .

umsdl-tarball: ums_platform.h
	tar czf umsdl.tar.gz $(umsfiles)
	
bin2ums:
	gcc bin2ums.c libumsdl.c -Wall -o bin2ums	

clean:
	rm -f *.o umsdl bin2ums umsdl.tar.gz ums_platform.h

distclean: clean
