OBJS = ipv6mgmt.o
INSTALL_DIR= $(PREFIX)/sbin/

install: ipv6-mgmt
	install -m 755 -d $(INSTALL_DIR)
	install -m 755 ipv6-mgmt $(INSTALL_DIR)

ipv6-mgmt: ${OBJS}
	$(CC) $(LDFLAGS) -Wl,-s $(XCFLAGS) $(filter %.o, $^) -o $@

%.o: %.c %.h
	${CC} ${XCFLAGS} ${CFLAGS} ${EXTRA_CFLAGS} $< -c -o $@ -MD -MF ${@D}/.${@F}.d
clean:
	-rm -rf *.o
