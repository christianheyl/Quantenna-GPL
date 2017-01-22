.PHONY: qevt_server qevt_send_event qproc_mon qproc_mon_send_event

PROGS= show_rfcal_version restore_bootcfg_env writerfmem

INSTALL_DIR= $(PREFIX)/sbin/

install: $(PROGS)
	install -m 755 -d $(INSTALL_DIR)
	install -m 755 $(PROGS) $(INSTALL_DIR)

qevt_server.o: qevt_server.c qevt_server.h qevt_event_id.h
	$(CC) $(CFLAGS) -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX \
	 -I../../zhostapd/hostapd-git/src/utils  -I../../zhostapd/hostapd-git/src/common \
	 -I./ -c qevt_server.c

qevt_event_id.o: qevt_event_id.c qevt_event_id.h qevt_server.h
	$(CC) $(CFLAGS) -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX \
	 -I../../zhostapd/hostapd-git/src/utils  -I../../zhostapd/hostapd-git/src/common \
	 -I./ -c qevt_event_id.c

wpa_ctrl.o: ../../zhostapd/hostapd-git/src/common/wpa_ctrl.c
	$(CC) $(CFLAGS) -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX \
	 -I../../zhostapd/hostapd-git/src/utils -I../../zhostapd/hostapd-git/src/common \
	 -c ../../zhostapd/hostapd-git/src/common/wpa_ctrl.c

os_unix.o: ../../zhostapd/hostapd-git/src/utils/os_unix.c
	$(CC) $(CFLAGS) -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX \
	 -I../../zhostapd/hostapd-git/src/utils -I../../zhostapd/hostapd-git/src/common \
	 -c ../../zhostapd/hostapd-git/src/utils/os_unix.c

qevt_server: qevt_server.o qevt_event_id.o wpa_ctrl.o os_unix.o
	$(CC) $(CFLAGS) $(LDFLAGS) -liw -pthread -o qevt_server qevt_server.o qevt_event_id.o wpa_ctrl.o os_unix.o

qevt_send_system_event.o: qevt_send_system_event.c qevt_server.h
	$(CC) $(CFLAGS) -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX \
	 -I../../zhostapd/hostapd-git/src/utils -I../../zhostapd/hostapd-git/src/common \
	 -I./ -c qevt_send_system_event.c

qevt_send_event: qevt_send_system_event.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o qevt_send_event qevt_send_system_event.o

qproc_mon.o: qproc_mon.c qproc_mon.h
	$(CC) $(CFLAGS) -DCONFIG_QPROC_MONITOR_LOG -I./ -c qproc_mon.c

qproc_mon: qproc_mon.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o qproc_mon qproc_mon.o

qproc_mon_send_event.o: qproc_mon_send_event.c qproc_mon.h
	$(CC) $(CFLAGS) -DCONFIG_QPROC_MONITOR_LOG -I./ -c qproc_mon_send_event.c

qproc_mon_send_event: qproc_mon_send_event.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o qproc_mon_send_event qproc_mon_send_event.o

install_qevent: qevt_server qevt_send_event qproc_mon qproc_mon_send_event
	install -m 755 -d $(INSTALL_DIR)
	install -m 755 qevt_server $(INSTALL_DIR)
	install -m 755 qevt_server_event.conf $(PREFIX)/etc/
	install -m 755 qevt_send_event $(INSTALL_DIR)
	install -m 755 qproc_mon $(INSTALL_DIR)
	install -m 755 qproc_mon.conf $(PREFIX)/etc/
	install -m 755 qproc_mon_send_event $(INSTALL_DIR)

clean:
	rm -f $(PROGS)
	rm -f qevt_server wpa_ctrl.o qevt_server.o os_unix.o qevt_event_id.o
	rm -f qevt_send_event qevt_send_system_event.o
	rm -f qproc_mon qproc_mon.o
	rm -f qproc_mon_send_event qproc_mon_send_event.o

