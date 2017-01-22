#!/usr/bin/env python

import socket
import getopt
import select
import signal
import sys
import time
import logging

server_address = ('localhost', 8000)
ca_address = ('localhost', 9090)
tg_address = ('localhost', 9000)


def signal_handler(signal, frame):
    logging.debug('You pressed Ctrl+C!')
    sys.exit(0)


def set_keepalive(sock, after_idle_sec=3, interval_sec=3, max_fails=2):
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        sock.ioctl(socket.SIO_KEEPALIVE_VALS, (1, 1000, 1000))
    except:
        # depends on OS we may get an exception here.
        # ignore it since it is not so important for functionality.
        pass


def parse_command_line():
    global ca_address, server_address, tg_address
    ca_ip, ca_port = ca_address
    server_ip, server_port = server_address
    tg_ip, tg_port = tg_address

    options, remainder = getopt.getopt(sys.argv[1:], '', ['ca-ip=',
                                                          'ca-port=',
                                                          'server-ip=',
                                                          'server-port=',
                                                          'tg-ip=',
                                                          'tg-port='])

    for opt, arg in options:
        if opt in ('--ca-ip'):
            ca_ip = arg
        elif opt in ('--ca-port'):
            ca_port = int(arg)
        elif opt in ('--server-ip'):
            server_ip = arg
        elif opt in ('--server-port'):
            server_port = int(arg)
        elif opt in ('--tg-ip'):
            tg_ip = arg
        elif opt in ('--tg-port'):
            tg_port = int(arg)

    ca_address = (ca_ip, ca_port)
    server_address = (server_ip, server_port)
    tg_address = (tg_ip, tg_port)


signal.signal(signal.SIGINT, signal_handler)
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s %(levelname)-4s: %(message)s')

try:
    parse_command_line()
except getopt.GetoptError, (value, message):
    logging.error("can't parse command line: %s", value)
    exit(1)

serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
serversocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR,
                        serversocket.getsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR) | 1)

set_keepalive(serversocket)

serversocket.setblocking(0)
logging.info('starting up on %s port %s' % server_address)
serversocket.bind(server_address)

serversocket.listen(5)

ucc_to_ca = {}
ca_to_ucc = {}
ucc_to_tg = {}
tg_to_ucc = {}

# Sockets from which we expect to read
connections_from_ucc = [serversocket]

# Sockets to which we expect to write
outputs = []


def handle_incoming_connection():
    global ucc_socket, client_address, value, message, tg_socket
    ucc_socket, client_address = s.accept()
    logging.info("new connection from %s", client_address)
    set_keepalive(ucc_socket)
    connections_from_ucc.append(ucc_socket)
    ca_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        ca_socket.connect(ca_address)
    except socket.error, (value, message):
        logging.error("can't connect to native control %s, error: %s", ca_address, message)
        ca_socket.close()
        ca_socket = None
    if ca_socket is not None:
        set_keepalive(ca_socket)
        ucc_to_ca[ucc_socket] = ca_socket
        connections_from_ucc.append(ca_socket)
        ca_to_ucc[ca_socket] = ucc_socket
        logging.info("have new proxy to %s", ca_socket.getsockname())
    else:
        connections_from_ucc.remove(ucc_socket)
        ucc_socket.close()
        ucc_socket = None
        logging.info("close UCC socket since native CA is not available")
        pass
    if tg_address:
        logging.info("connect to tg %s", tg_address)
        tg_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            tg_socket.connect(tg_address)
        except socket.error, (value, message):
            logging.debug("can't connect to tg, %s", message)
            tg_socket.close()
            tg_socket = None

        if tg_socket:
            ucc_to_tg[ucc_socket] = tg_socket
            tg_to_ucc[tg_socket] = ucc_socket
            connections_from_ucc.append(tg_socket)


def shutdown_broken_socket(broken_socket):
    sockets_to_remove = [broken_socket]
    if broken_socket in ca_to_ucc:
        sockets_to_remove.append(ca_to_ucc[broken_socket])
        del ca_to_ucc[broken_socket]
    if broken_socket in ucc_to_ca:
        sockets_to_remove.append(ucc_to_ca[broken_socket])
        del ucc_to_ca[broken_socket]
    if broken_socket in ucc_to_tg:
        sockets_to_remove.append(ucc_to_tg[broken_socket])
        del ucc_to_tg[broken_socket]
    if broken_socket in tg_to_ucc:
        sockets_to_remove.append(tg_to_ucc[broken_socket])
        del tg_to_ucc[broken_socket]
    for s in sockets_to_remove:
        logging.info("closing socket %s", s.getsockname())
        s.close()
        if s in connections_from_ucc:
            connections_from_ucc.remove(s)


while connections_from_ucc:
    # Wait for at least one of the sockets to be ready for processing
    readable, writable, exceptional = select.select(connections_from_ucc, outputs, connections_from_ucc)

    for s in readable:
        if s is serversocket:
            handle_incoming_connection()
        else:
            try:
                data = s.recv(4096)
            except socket.error, (value, message):
                logging.warning("recv error: %s", message)
                data = None

            if not data:
                shutdown_broken_socket(s)
            else:
                if not data.lower().endswith("\r\n"):
                    logging.error("message is badly formatted: msg %d", data)
                try:
                    if s in ucc_to_ca:
                        if data.lower().startswith("traffic_agent"):
                            if s in ucc_to_tg:
                                logging.info("[ucc -> tg] %s", data)
                                ucc_to_tg[s].send(data)
                            else:
                                logging.info("try to connect to tg")
                                tg_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                                try:
                                    tg_socket.connect(tg_address)
                                except socket.error, (value, message):
                                    logging.debug("can't connect to tg, %s", message)
                                    tg_socket.close()
                                    tg_socket = None

                                if tg_socket:
                                    ucc_to_tg[s] = tg_socket
                                    tg_to_ucc[tg_socket] = s
                                    logging.info("[ucc -> tg] %s", data)
                                    tg_socket.send(data)
                                    connections_from_ucc.append(tg_socket)
                        else:
                            logging.debug("[ucc -> ca] %s", data)
                            ucc_to_ca[s].send(data)
                    elif s in tg_to_ucc:
                        logging.debug("[tg -> ucc] %s", data)
                        tg_to_ucc[s].send(data)
                    elif s in ca_to_ucc:
                        if data.lower().startswith("status,complete,after_action,wait_reboot"):
                            logging.info("wait device back after reboot")
                            s.close()
                            del ca_to_ucc[s]
                            connections_from_ucc.remove(s)
                            time.sleep(2)

                            for num in range(1, 100):
                                logging.debug("wait, tick %d", num)
                                ca_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                                try:
                                    ca_socket.connect(ca_address)
                                except socket.error, (value, message):
                                    logging.debug("can't connect to native control, %s", message)
                                    ca_socket.close()
                                    ca_socket = None
                                if ca_socket is None:
                                    time.sleep(1)
                                else:
                                    for item in ucc_to_ca.items():
                                        ucc_sock, ca_sock = item
                                        if ca_sock == s:
                                            ucc_to_ca[ucc_sock] = ca_socket
                                            ca_to_ucc[ca_socket] = ucc_sock
                                            connections_from_ucc.append(ca_socket)
                                            ucc_sock.send(data)
                                            break
                                    break
                        else:
                            logging.debug("[ca -> ucc] %s", data)
                            ca_to_ucc[s].send(data)
                except socket.error, (value, message):
                    logging.warning("get send exception %s", message)
                    shutdown_broken_socket(s)

logging.info("exit")
