/*
 * miidl
 *
 * Communicate with Qdevice in MII/RGMII bootrom
 *
 * Copyright (c) Quantenna Communications 2011
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/if_arp.h>

#define QTN_MIIBOOT_BUFLEN			256

#define QTN_MIIBOOT_PKT_FLAG			0
#define QTN_MIIBOOT_PKT_CRC			4
#define QTN_MIIBOOT_PKT_DESTADDR		8
#define QTN_MIIBOOT_PKT_PAYLOAD_LENGTH		14
#define QTN_MIIBOOT_PKT_PAYLOAD_START		16
#define QTN_MIIBOOT_PAYLOAD_MAXSIZE		(QTN_MIIBOOT_BUFLEN - QTN_MIIBOOT_PKT_PAYLOAD_START)

#define QTN_MIIBOOT_CRC_START			8
#define QTN_MIIBOOT_CRC_LEN			(QTN_MIIBOOT_BUFLEN - QTN_MIIBOOT_CRC_START)

#define QTN_MIIBOOT_ACK				0x2e
#define QTN_MIIBOOT_NAK				0x78
#define QTN_MIIBOOT_WRITE			0x80
#define QTN_MIIBOOT_READ			0xf0

#define DEST_ADDR_DEFAULT			0x80000000
#define IFNAME_DEFAULT				"eth0"

#define RX_TIMEOUT_SECS				2

#define MIN(x,y)	((x) < (y) ? (x) : (y))

struct miidl_cfg {
	int socket_fd;			/* file descriptor for socket */
	struct sockaddr_ll sll;		/* link level sockaddr */
	uint32_t dest_addr;		/* address to read/write on the target qdevice */
	int bytes_to_read;		/* bytes to read on read commands */
	int no_check_response;		/* do not check for ack/nak from qdevice, just dump packets */
	int verbose;			/* additional debug */
	int raw_read;			/* dump raw binary from read commands */
	const char* ifname;		/* interface name to dump packets */
	int ifindex;			/* interface index */
	const char* infile_name;	/* file to read from for image download */
	int put_dest_data;		/* is this operation putting a single data word? */
	uint32_t dest_data;		/* word to write */
	int start_exec;			/* jump to instruction */
};

/*
 * CRC32 checksums and lookup table
 * Uses the ANSI x3.66 polynomial, which is also used for
 * ethernet CRC (FCS)
 */
uint32_t crc_tab[256];
uint32_t chksum_crc32(const unsigned char *block, unsigned int length)
{
	unsigned long crc;
	unsigned long i;

	crc = 0xFFFFFFFF;
	for (i = 0; i < length; i++) {
		crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_tab[(crc ^ *block++) & 0xFF];
	}
	return (crc ^ 0xFFFFFFFF);
}

void chksum_crc32gentab(void)
{
	unsigned long crc;
	unsigned long poly;
	int i;
	int j;

	poly = 0xEDB88320L;
	for (i = 0; i < 256; i++) {
		crc = i;
		for (j = 8; j > 0; j--) {
			if (crc & 1) {
				crc = (crc >> 1) ^ poly;
			} else {
				crc >>= 1;
			}
		}
		crc_tab[i] = crc;
	}
}

static void put32(uint8_t *buf, uint32_t val)
{
	memcpy(buf, &val, sizeof(val));
}

static uint32_t get32(const uint8_t *buf)
{
	uint32_t val;
	memcpy(&val, buf, sizeof(val));
	return val;
}

static void print_payload(const struct miidl_cfg *cfg,
		const void *_payload, const size_t bytes, const uint32_t dest_addr)
{
	FILE *file;
	const uint8_t *payload = _payload;

	file = stdout;
	if (cfg->raw_read) {
		unsigned int i;
		for (i = 0; i < bytes; i++) {
			fputc(payload[i], file);
		}
	} else {
		unsigned int i;
		int words_per_line;
		words_per_line = 4;

		for (i = 0; i < bytes / sizeof(uint32_t); i++) {
			uint32_t addr = dest_addr + i * sizeof(uint32_t);
			if (i % words_per_line == 0) {
				if (i) {
					fprintf(file, "\n");
				}
				fprintf(file, "0x%08x:", addr);
			}
			fprintf(file, " 0x%08x", get32(&payload[i * sizeof(uint32_t)]));
		}
		fprintf(file, "\n");
	}
}

static void miidl_init(void)
{
	chksum_crc32gentab();
}

static void txbuf_set_writecmd(uint8_t *buf)
{
	buf[QTN_MIIBOOT_PKT_FLAG] = QTN_MIIBOOT_WRITE;
}

static void txbuf_set_readcmd(uint8_t *buf)
{
	buf[QTN_MIIBOOT_PKT_FLAG] = QTN_MIIBOOT_READ;
}

static void txbuf_set_checksum(uint8_t *buf)
{
	uint32_t crc32 = chksum_crc32(&buf[QTN_MIIBOOT_CRC_START], QTN_MIIBOOT_CRC_LEN);
	put32(&buf[QTN_MIIBOOT_PKT_CRC], crc32);
}

static int rxbuf_checksum_ok(const uint8_t *buf)
{
	/* rx checksum differs from tx; less bytes used, and inverted */
	uint32_t crc_calc = chksum_crc32(&buf[QTN_MIIBOOT_PKT_PAYLOAD_START], QTN_MIIBOOT_PAYLOAD_MAXSIZE);
	uint32_t crc_pkt = get32(&buf[QTN_MIIBOOT_PKT_CRC]);
	crc_calc = crc_calc ^ ~0;
	return crc_calc == crc_pkt;
}

static void txbuf_set_data(uint8_t *buf, uint32_t target_addr, const void *src, unsigned int bytes_req)
{
	size_t bytes_roundup = (bytes_req + 0x3) &~ 0x3;
	if (bytes_roundup != bytes_req) {
		fprintf(stderr, "%s: ONLY multiples of 4 bytes are legal, requested %u\n",
				__FUNCTION__, bytes_req);
	}
	if (bytes_roundup > QTN_MIIBOOT_PAYLOAD_MAXSIZE) {
		bytes_roundup = QTN_MIIBOOT_PAYLOAD_MAXSIZE;
		fprintf(stderr, "%s: too much data requested for 1 payload, reqested %u, max %d\n",
				__FUNCTION__, bytes_req, QTN_MIIBOOT_PAYLOAD_MAXSIZE);
	}
	if (bytes_roundup && src) {
		memcpy(&buf[QTN_MIIBOOT_PKT_PAYLOAD_START], src, bytes_roundup);
	}
	buf[QTN_MIIBOOT_PKT_PAYLOAD_LENGTH] = (uint8_t)bytes_roundup;
	put32(&buf[QTN_MIIBOOT_PKT_DESTADDR], target_addr);
}

static void buf_debug(const uint8_t *buf)
{
	const char* cmd = "BAD!";
	if (buf[QTN_MIIBOOT_PKT_FLAG] == QTN_MIIBOOT_WRITE) {
		cmd = "wr";
	} else if (buf[QTN_MIIBOOT_PKT_FLAG] == QTN_MIIBOOT_READ) {
		cmd = "rd";
	} else if (buf[QTN_MIIBOOT_PKT_FLAG] == QTN_MIIBOOT_ACK) {
		cmd = "ok";
	} else if (buf[QTN_MIIBOOT_PKT_FLAG] == QTN_MIIBOOT_NAK) {
		cmd = "NK";
	}

	fprintf(stdout, "Cmd: %s, addr 0x%08x len %d (0x%x) data 0x%08x\n",
			cmd,
			get32(&buf[QTN_MIIBOOT_PKT_DESTADDR]),
			buf[QTN_MIIBOOT_PKT_PAYLOAD_LENGTH],
			buf[QTN_MIIBOOT_PKT_PAYLOAD_LENGTH],
			get32(&buf[QTN_MIIBOOT_PKT_PAYLOAD_START]));
}

static int miidl_rx(struct miidl_cfg *cfg, uint8_t *rxbuf)
{
	ssize_t rxlen = -1;
	clock_t start_rx_clk;

	/* time at start of this rx */
	start_rx_clk = clock() / CLOCKS_PER_SEC;

	while (1) {
		clock_t now_rx_clk;
		struct sockaddr_ll rxsll;
		socklen_t rxsll_size = sizeof(rxsll);

		/* non blocking rx */
		rxlen = recvfrom(cfg->socket_fd, rxbuf, QTN_MIIBOOT_BUFLEN, MSG_DONTWAIT,
				(struct sockaddr*)&rxsll, &rxsll_size);
		if (rxlen < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			/* no data, loop again */
		} else if (rxlen < 0) {
			fprintf(stderr, "%s: error calling recvfrom: %s\n",
					__FUNCTION__, strerror(errno));
			return -1;
		} else if (rxsll.sll_ifindex != cfg->ifindex) {
			/* wrong interface, ignore */
		} else {
			/* correct interface, packet received */
			if (rxbuf[QTN_MIIBOOT_PKT_FLAG] == QTN_MIIBOOT_ACK) {
				break;
			} else if (rxbuf[QTN_MIIBOOT_PKT_FLAG] == QTN_MIIBOOT_NAK) {
				fprintf(stderr, "%s: qdevice returns NAK\n",
						__FUNCTION__);
				return -1;
			} else {
				/* unknown packet.. noise on link, strange, but ignore */
			}
		}

		/* check for timeout */
		now_rx_clk = clock() / CLOCKS_PER_SEC;
		if (now_rx_clk > start_rx_clk + RX_TIMEOUT_SECS) {
			fprintf(stderr, "%s: no response seen\n",
					__FUNCTION__);
			return -1;
		}
	}

	return rxlen;
}

static int miidl_do_sendfile(struct miidl_cfg *cfg, FILE *file, const int file_size)
{
	int file_read = 0;
	ssize_t txlen;
	uint8_t txbuf[QTN_MIIBOOT_BUFLEN] = {0};
	uint8_t rxbuf[QTN_MIIBOOT_BUFLEN] = {0};

	uint32_t writeptr = cfg->dest_addr;
	size_t bytes_remaining = file_size;

	while (bytes_remaining > 0) {

		uint8_t filebuf[QTN_MIIBOOT_PAYLOAD_MAXSIZE];
		size_t fbytes_read;
		size_t bytes_to_read = MIN(QTN_MIIBOOT_PAYLOAD_MAXSIZE, bytes_remaining);

		txbuf_set_writecmd(txbuf);
		/* send data from file */
		fbytes_read = fread(filebuf, 1, bytes_to_read, file);
		if (fbytes_read != bytes_to_read || ferror(file)) {
			fprintf(stderr, "%s error reading file: %s\n",
					__FUNCTION__, strerror(errno));
			return -1;
		}
		file_read += fbytes_read;
		/* round up data length for filesizes that aren't multiples of 4 */
		txbuf_set_data(txbuf, writeptr, filebuf, (fbytes_read + 0x3) &~ 0x3);
		writeptr += fbytes_read;
		bytes_remaining -= fbytes_read;
		txbuf_set_checksum(txbuf);

		/* transmit to qdevice */
		if (cfg->verbose) {
			buf_debug(txbuf);
		}
		txlen = sendto(cfg->socket_fd, txbuf, QTN_MIIBOOT_BUFLEN,
				0, (struct sockaddr*)&cfg->sll, sizeof(cfg->sll));
		if (txlen < 0) {
			fprintf(stderr, "%s error calling sendto: %s\n",
					__FUNCTION__, strerror(errno));
			return -1;
		}

		if (!cfg->no_check_response) {
			/* wait for ack */
			if (miidl_rx(cfg, rxbuf) < 0) {
				return -1;
			}
		}
	}

	return 0;
}

static int miidl_sendfile(struct miidl_cfg *cfg)
{
	FILE *file;
	int file_size;
	int ret;

	/* open binary to send to device */
	file = fopen(cfg->infile_name, "rb");
	if (!file) {
		fprintf(stderr, "%s: error opening file '%s': %s\n",
				__FUNCTION__, cfg->infile_name, strerror(errno));
		return -ENOENT;
	}

	/* find filesize */
	fseek(file, 0L, SEEK_END);
	file_size = ftell(file);
	fseek(file, 0L, SEEK_SET);

	ret = miidl_do_sendfile(cfg, file, file_size);

	fclose(file);

	return ret;
}

static int miidl_do_read(struct miidl_cfg *cfg, uint8_t *rxdata)
{
	ssize_t txlen;
	uint8_t txbuf[QTN_MIIBOOT_BUFLEN] = {0};
	uint8_t rxbuf[QTN_MIIBOOT_BUFLEN] = {0};

	uint32_t readptr = cfg->dest_addr;
	unsigned int bytes_read = 0;
	unsigned int bytes_remaining = cfg->bytes_to_read;

	while (bytes_remaining > 0) {
		size_t bytes_to_read = MIN(QTN_MIIBOOT_PAYLOAD_MAXSIZE, bytes_remaining);

		/* transmit read request */
		txbuf_set_readcmd(txbuf);
		txbuf_set_data(txbuf, readptr, NULL, bytes_to_read);
		txbuf_set_checksum(txbuf);

		if (cfg->verbose) {
			buf_debug(txbuf);
		}

		txlen = sendto(cfg->socket_fd, txbuf, QTN_MIIBOOT_BUFLEN,
				0, (struct sockaddr*)&cfg->sll, sizeof(cfg->sll));
		if (txlen < 0) {
			fprintf(stderr, "%s error calling sendto: %s\n",
					__FUNCTION__, strerror(errno));
			return -1;
		}

		if (!cfg->no_check_response) {
			/* wait for ack & response */
			if (miidl_rx(cfg, rxbuf) < 0) {
				return -1;
			}

			if (!rxbuf_checksum_ok(rxbuf)) {
				fprintf(stderr, "%s checksum fails\n",
						__FUNCTION__);
				return -1;
			}
		}

		/* accumulate answer in rxdata */
		memcpy(&rxdata[bytes_read], &rxbuf[QTN_MIIBOOT_PKT_PAYLOAD_START], bytes_to_read);
		readptr += bytes_to_read;
		bytes_read += bytes_to_read;
		bytes_remaining -= bytes_to_read;
	}

	return 0;
}

static int miidl_read(struct miidl_cfg *cfg)
{
	uint8_t *rxdata;
	int ret;

	rxdata = malloc(cfg->bytes_to_read);
	if (rxdata == NULL) {
		fprintf(stderr, "%s: could not allocate %d bytes\n",
				__FUNCTION__, cfg->bytes_to_read);
		return -ENOMEM;
	}

	ret = miidl_do_read(cfg, rxdata);

	if (ret == 0) {
		print_payload(cfg, rxdata, cfg->bytes_to_read, cfg->dest_addr);
	}

	free(rxdata);

	return ret;
}

static int miidl_putword(struct miidl_cfg *cfg, const uint32_t *word)
{
	ssize_t txlen;
	uint8_t txbuf[QTN_MIIBOOT_BUFLEN] = {0};
	uint8_t rxbuf[QTN_MIIBOOT_BUFLEN] = {0};

	/* transmit write word */
	txbuf_set_writecmd(txbuf);
	txbuf_set_data(txbuf, cfg->dest_addr, word, word ? sizeof(*word) : 0);
	txbuf_set_checksum(txbuf);

	if (cfg->verbose) {
		buf_debug(txbuf);
	}

	txlen = sendto(cfg->socket_fd, txbuf, QTN_MIIBOOT_BUFLEN,
			0, (struct sockaddr*)&cfg->sll, sizeof(cfg->sll));
	if (txlen < 0) {
		fprintf(stderr, "%s error calling sendto: %s\n",
				__FUNCTION__, strerror(errno));
		return -1;
	}

	if (!cfg->no_check_response) {
		/* wait for ack */
		if (miidl_rx(cfg, rxbuf) < 0) {
			return -1;
		}
	}

	return 0;
}

static int miidl_cfg_valid(const struct miidl_cfg *cfg)
{
	int put_file_only = cfg->infile_name != NULL &&
		cfg->bytes_to_read == 0;
	int put_data_only = cfg->put_dest_data &&
		cfg->infile_name == NULL &&
		cfg->bytes_to_read == 0;
	int read_only = cfg->infile_name == NULL &&
		cfg->put_dest_data == 0 &&
		cfg->bytes_to_read > 0 &&
		cfg->bytes_to_read % sizeof(uint32_t) == 0;
	int jump_only = cfg->infile_name == NULL &&
		cfg->put_dest_data == 0 &&
		cfg->bytes_to_read == 0 &&
		cfg->start_exec;

	return put_file_only || put_data_only || read_only || jump_only;
}

int miidl_interface_set_promisc(const struct miidl_cfg *cfg)
{
	int err = 0;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, cfg->ifname);
	err = ioctl(cfg->socket_fd, SIOCGIFFLAGS, &ifr);
	if (err < 0) {
		fprintf(stderr, "%s error setting interface to promiscuous mode: %s\n",
				__FUNCTION__, strerror(errno));
		return err;
	}
	ifr.ifr_flags |= IFF_PROMISC;
	err = ioctl(cfg->socket_fd, SIOCGIFFLAGS, &ifr);
	if (err < 0) {
		fprintf(stderr, "%s error setting interface to promiscuous mode: %s\n",
				__FUNCTION__, strerror(errno));
		return err;
	}

	return 0;
}

int miidl_socket_set_promisc(const struct miidl_cfg *cfg)
{
	int err = 0;
	struct packet_mreq mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.mr_ifindex = cfg->ifindex;
	mreq.mr_type = PACKET_MR_PROMISC;
	err = setsockopt(cfg->socket_fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
			&mreq, sizeof(mreq));
	if (err < 0) {
		fprintf(stderr, "%s error setting socket to promiscuous mode: %s\n",
				__FUNCTION__, strerror(errno));
		return err;
	}

	return 0;
}

static int miidl_run(struct miidl_cfg *cfg)
{
	int err = -EINVAL;

	/* find interface to dump files to */
	cfg->ifindex = if_nametoindex(cfg->ifname);
	if (!cfg->ifindex) {
		fprintf(stderr, "%s no index found for interface '%s'\n",
				__FUNCTION__, cfg->ifname);
		goto out;
	}

	/* create raw socket */
	cfg->socket_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (cfg->socket_fd < 0) {
		fprintf(stderr, "%s error creating socket: %s\n",
				__FUNCTION__, strerror(errno));
		goto out;
	}

	/* set up link layer sockaddr */
	memset(&cfg->sll, 0, sizeof(cfg->sll));
	cfg->sll.sll_family = PF_PACKET;
	cfg->sll.sll_protocol = 0;	/* protocol isn't used */
	cfg->sll.sll_ifindex = cfg->ifindex;
	cfg->sll.sll_hatype = ARPHRD_ETHER;
	cfg->sll.sll_pkttype = PACKET_OTHERHOST;
	cfg->sll.sll_halen = ETH_ALEN;

	/* put socket into promiscuous mode */
	if (miidl_socket_set_promisc(cfg) < 0) {
		goto out_close_socket;
	}

	if (cfg->bytes_to_read) {
		err = miidl_read(cfg);
	} else if (cfg->put_dest_data) {
		err = miidl_putword(cfg, &cfg->dest_data);
	} else if (cfg->infile_name) {
		err = miidl_sendfile(cfg);
		if (!err && cfg->start_exec) {
			err = miidl_putword(cfg, NULL);
		}
	} else if (cfg->start_exec) {
		err = miidl_putword(cfg, NULL);
	} else {
		fprintf(stderr, "%s error: invalid config??\n",
				__FUNCTION__);
		err = -EINVAL;
	}

out_close_socket:
	close(cfg->socket_fd);
out:
	return err;
}

static void miidl_help(void)
{
	FILE *file = stderr;

	fprintf(file, "miidl - download images to qdevices in mii boot\n"
			"Usage: to download a file to a qdevice at addr <m>:\n"
			"\tmiidl -i <ethX> -f <file> [-a <m>] [-j]\n"
			"    to read <b> bytes from a memory address <m>\n"
			"\tmiidl -i <ethX> -r <b> [-a <m>]\n"
			"    to write word <w> to a memory address <m>\n"
			"\tmiidl -i <ethX> -d <w> [-a <m>]\n"
			"    to jump to memory address <m>\n"
			"\tmiidl -i <ethX> -j [-a <m>]\n"
			"\n"
			"\tOptions:\n"
			"\t-i <ifname>	Interface name to send/recv on,\n"
			"\t		default is '%s'\n", IFNAME_DEFAULT);
	fprintf(file, "\t-j		jump to address specified in '-a' flag\n"
			"\t-a <addr>	address (in hex, should be 32bit aligned)\n"
			"\t		to read from / write to. Default 0x%x\n",
			DEST_ADDR_DEFAULT);
	fprintf(file, "\t-f <file>	File to use when downloading an image\n"
			"\t-r <bytes>	Bytes to read. Must be a multiple of 4\n"
			"\t-d <data>	Single 32bit word (hex) to write\n"
			"\t	-d and -a are hex only\n"
			"\t	-r is hex optionally with '0x' prefix.\n"
			"\t	-f, -d and -r are mutually exclusive\n");
	fprintf(file, "\t--raw		Dump raw binary to stdout when reading\n"
			"\t-n		Do not wait for response from qdevice\n"
			"\t-v		Verbose (for debugging)\n"
			"\t-h/--help	This message\n");

}

int main(int argc, char **argv)
{
	int i;
	int err = 0;
	int show_help = 0;

	struct miidl_cfg cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.dest_addr = DEST_ADDR_DEFAULT;
	cfg.ifname = IFNAME_DEFAULT;

	for (i = 0; i < argc; i++) {
		const char *arg;
		const char *nextarg = NULL;
		arg = argv[i];
		if (i < argc - 1) {
			nextarg = argv[i + 1];
		}

		if (!strcmp(arg, "-i") && nextarg) {
			cfg.ifname = nextarg;
			i++;
		} else if (!strcmp(arg, "-f") && nextarg) {
			cfg.infile_name = nextarg;
			i++;
		} else if (!strcmp(arg, "-a") && nextarg) {
			if (sscanf(nextarg, "%x", &cfg.dest_addr) != 1) {
				fprintf(stderr, "Error parsing number: '%s'\n", nextarg);
				show_help = 1;
			}
			i++;
		} else if (!strcmp(arg, "-r")) {
			if (sscanf(nextarg, "%i", &cfg.bytes_to_read) != 1) {
				fprintf(stderr, "Error parsing number: '%s'\n", nextarg);
				show_help = 1;
			}
			i++;
		} else if (!strcmp(arg, "-d")) {
			cfg.put_dest_data = 1;
			if (sscanf(nextarg, "%x", &cfg.dest_data) != 1) {
				fprintf(stderr, "Error parsing number: '%s'\n", nextarg);
				show_help = 1;
			}
			i++;
		} else if (!strcmp(arg, "--raw")) {
			cfg.raw_read = 1;
		} else if (!strcmp(arg, "-j")) {
			cfg.start_exec = 1;
		} else if (!strcmp(arg, "-n")) {
			cfg.no_check_response = 1;
		} else if (!strcmp(arg, "-v")) {
			cfg.verbose = 1;
		} else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
			show_help = 1;
		} else if (i > 0) {
			fprintf(stderr, "Invalid argument: '%s'\n", arg);
			show_help = 1;
			err = -EINVAL;
		}
	}

	if (show_help) {
		miidl_help();
	} else if (!miidl_cfg_valid(&cfg)) {
		err = -EINVAL;
		fprintf(stderr, "Config invalid\n");
		miidl_help();
	} else {
		miidl_init();
		err = miidl_run(&cfg);
	}

	return err;
}

