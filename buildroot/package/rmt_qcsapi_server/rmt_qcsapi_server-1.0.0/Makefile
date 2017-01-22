RANLIB = ranlib

LIBS= -lpthread -lm

PROGS = rmt_qcsapi_server

# Install directories
INSTALL_DIR= $(PREFIX)/sbin/

rmt_qcsapi_server: rmobj
	$(CC) $(LDFLAGS) -Wl,-s $(XCFLAGS) -o $@ rmt_qcsapi_server.c -L ../../qcsapi/qcsapi-1.0.1 $(LIBS)

install: $(PROGS)
	install -m 755 -d $(INSTALL_DIR)
	install -m 755 $(PROGS) $(INSTALL_DIR)

rmobj:
	rm -rf rmt_qcsapi_server
	
