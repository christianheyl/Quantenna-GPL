#ifndef _MAC_ADDR_TARGET_H_
#define _MAC_ADDR_TARGET_H_

/* error return codes */
#define EQDRV_NOT_LOADED         	-2
#define ENO_MAC_ADDR_FILE           -3
#define ENO_DEFAULT_MAC_ADDR_FILE   -4 

/* Data Structures */
struct mac_addr_block {
	unsigned char	mac0_addr[6];
	unsigned char	mac1_addr[6];
	unsigned char	mac2_addr[6];
	unsigned char	mac3_addr[6];
};

/* prototypes */

static int init();
static int clean_up(void);
static int read_mac_addr_file(void);
static int read_mac_addr(void);
static int set_mac_address(void);

#endif /* _MAC_ADDR_TARGET_H_ */
