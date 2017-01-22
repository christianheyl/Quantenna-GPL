
.PHONY: all clean distclean zips

QCSAPI_DIR = ../../buildroot/package/qcsapi/qcsapi-1.0.1/

CALL_QCSAPI_SRCZIP = call_qcsapi_client_src.zip
QCSAPI_CLIENT_SRCZIP = libqcsapi_client_src.zip

ZIPS = $(CALL_QCSAPI_SRCZIP) $(QCSAPI_CLIENT_SRCZIP)
DIRS = $(ZIPS:%.zip=%)

all: $(ZIPS) $(DIRS)

zips: $(ZIPS)

%.zip:
	make -C $(QCSAPI_DIR) ${@F}
	cat $(QCSAPI_DIR)/${@F} > $@

%: %.zip
	rm -rf $@
	mkdir $@
	cd $@ && unzip ../$< && make

clean:
	rm -rf $(DIRS) $(ZIPS)

distclean: clean
