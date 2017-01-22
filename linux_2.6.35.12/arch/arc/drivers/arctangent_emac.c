/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MAC driver for the ARCTangent EMAC 10100 (Rev 5)
 *
 * vineetg: June 2011
 *  -Issues when working with 64b cache line size
 *      = BD rings point to aligned location in an internal buffer
 *      = added support for cache coherent BD Ring memory
 *
 * vineetg: May 2010
 *  -Reduced foot-print of the main ISR (handling for error cases moved out
 *      into a separate non-inline function).
 *  -Driver Tx path optimized for small packets (which fit into 1 BD = 2K).
 *      Any specifics for chaining are in a separate block of code.
 *
 * vineetg: Nov 2009
 *  -Unified NAPI and Non-NAPI Code.
 *  -API changes since 2.6.26 for making NAPI independent of netdevice
 *  -Cutting a few checks in main rx poll routine
 *  -Tweaked NAPI implementation:
 *      In poll mode, Original driver would always start sweeping BD chain
 *      from BD-0 upto poll budget (40). And if it got over-budget it would
 *      drop remiander of packets.
 *      Instead now we remember last BD polled and in next
 *      cycle, we resume from next BD onwards. That way in case of over-budget
 *      no packet needs to be dropped.
 *
 * vineetg: Nov 2009
 *  -Rewrote the driver register access macros so that multiple accesses
 *   in same function use "anchor" reg to save the base addr causing
 *   shorter instructions
 *
 * Amit bhor, Sameer Dhavale: Codito Technologies 2004
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>


#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/init.h>
#include <linux/inetdevice.h>
#include <linux/inet.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <asm/cacheflush.h>
#include <asm/irq.h>

#ifdef ARC_EMAC_COH_MEM
/* The BDs are allocated in cache coherent memory - thus normal "C" code
 * can be used to read/write them - MMU magic takes care of making them
 * uncached
 */
#define arc_emac_read(addr)         *(addr)
#define arc_emac_write(addr, val)   *(addr) = (val)
#else
/* BDs in normal memory - thus needed special mode of ld/st insn ".di"
 * to make the accesses uncached
 */
#define arc_emac_read(addr)         arc_read_uncached_32(addr)
#define arc_emac_write(addr, val)   arc_write_uncached_32(addr, val)
#endif

/* clock speed is set while parsing parameters in setup.c */
extern unsigned long clk_speed;

/* Timeout time for tranmission */
#define TX_TIMEOUT (400*HZ/1000)

#define AUTO_NEG_TIMEOUT 2000

#define SKB_PREALLOC    256	/* Number of cached SKB's */
#define NAPI_WEIGHT 40      /* workload for NAPI */


// Pre - allocated(cached) SKB 's.  Improve driver performance by allocating SKB' s
// in advance of when they are needed.
volatile struct sk_buff *skb_prealloc[SKB_PREALLOC];

//pre - allocated SKB 's
volatile unsigned int skb_count = 0;

//#define EMAC_STATS 1
#ifdef EMAC_STATS
//EMAC stats.Turn these on to get more information from the EMAC
// Turn them off to get better performance.
// note:If you see UCLO it 's bad.

	unsigned int    emac_drop = 0;
	unsigned int    emac_defr = 0;
	unsigned int    emac_ltcl = 0;
	unsigned int    emac_uflo = 0;
	unsigned int    skb_not_preallocated = 0;
#endif
    unsigned int    emac_txfull = 0;


/*
 * Size of RX buffers, min = 0 (pointless) max = 2048 (MAX_RX_BUFFER_LEN) MAC
 * reference manual recommends a value slightly greater than the maximum size
 * of the packet expected other wise it will chain a zero size buffer desc if
 * a packet of exactly RX_BUFFER_LEN comes. VMAC will chain buffers if a
 * packet bigger than this arrives.
 */
#define RX_BUFFER_LEN	(ETH_FRAME_LEN + 4)

#define MAX_RX_BUFFER_LEN	0x800	/* 2^11 = 2048 = 0x800 */
#define MAX_TX_BUFFER_LEN	0x800	/* 2^11 = 2048 = 0x800 */

/* Assuming MTU=1500 (IP pkt: hdr+payload), we round off to next cache-line
 * boundary: 1536 % {32,64,128} == 0
 * This provides enough space for ethernet header (14) and also ensures that
 * buf-size passed to EMAC > max pkt rx (1514). Latter is due to a EMAC quirk
 * wherein it can generate a chained pkt (with all data in first part, and
 * an empty 2nd part - albiet with last bit set) when Rx-pkt-sz is exactly
 * equal to buf-size: hence the need to keep buf-size sligtly bigger than
 * largest pkt.
 */
#define	VMAC_BUFFER_PAD 36

/* VMAC register definitions */
typedef volatile struct
{
    unsigned int id, status, enable, ctrl, pollrate, rxerr, miss,
                       tx_ring, rx_ring, addrl, addrh, lafl, lafh, mdio;
} arc_emac_reg;

#define ASSUME_1_EMAC

#ifdef ASSUME_1_EMAC

#ifdef ARC_SIMPLE_REG_ACCESS

#define EMAC_REG(ap)   ((arc_emac_reg *)(VMAC_REG_BASEADDR))

#else  /* ! ARC_SIMPLE_REG_ACCESS */

static inline arc_emac_reg *const EMAC_REG(void * ap)   \
{                                                                       \
    arc_emac_reg *p = (arc_emac_reg *)VMAC_REG_BASEADDR;                \
    asm ("; fix %0": "+r" (p));                                         \
    return p;                                                           \
}
#endif

#else  /* ! ASSUME_1_EMAC */
#define EMAC_REG(ap)   (ap->reg_base_addr)
#endif

/* STATUS and ENABLE Register bit masks */
#define TXINT_MASK	(1<<0)	/* Transmit interrupt */
#define RXINT_MASK	(1<<1)	/* Recieve interrupt */
#define ERR_MASK	(1<<2)	/* Error interrupt */
#define TXCH_MASK	(1<<3)	/* Transmit chaining error interrupt */
#define MSER_MASK	(1<<4)	/* Missed packet counter error */
#define RXCR_MASK	(1<<8)	/* RXCRCERR counter rolled over	 */
#define RXFR_MASK	(1<<9)	/* RXFRAMEERR counter rolled over */
#define RXFL_MASK	(1<<10)	/* RXOFLOWERR counter rolled over */
#define MDIO_MASK	(1<<12)	/* MDIO complete */
#define TXPL_MASK	(1<<31)	/* TXPOLL */

/* CONTROL Register bit masks */
#define EN_MASK		(1<<0)	/* VMAC enable */
#define TXRN_MASK	(1<<3)	/* TX enable */
#define RXRN_MASK	(1<<4)	/* RX enable */
#define DSBC_MASK	(1<<8)	/* Disable recieve broadcast */
#define ENFL_MASK	(1<<10)	/* Enable Full Duplex */
#define PROM_MASK	(1<<11)	/* Promiscuous mode */

/* Buffer descriptor INFO bit masks */
#define OWN_MASK	(1<<31)	/* 0-CPU owns buffer, 1-VMAC owns buffer */
#define FRST_MASK	(1<<16)	/* First buffer in chain */
#define LAST_MASK	(1<<17)	/* Last buffer in chain */
#define LEN_MASK	0x000007FF	/* last 11 bits */
#define CRLS            (1<<21)
#define DEFR            (1<<22)
#define DROP            (1<<23)
#define RTRY            (1<<24)
#define LTCL            (1<<28)
#define UFLO            (1<<29)

#define FOR_EMAC        OWN_MASK
#define FOR_CPU         0

/* ARCangel board PHY Identifier */
#define PHY_ID	0x3

/* LXT971 register definitions */
#define LXT971A_CTRL_REG               0x00
#define LXT971A_STATUS_REG             0x01
#define LXT971A_AUTONEG_ADV_REG        0x04
#define LXT971A_AUTONEG_LINK_REG       0x05
#define LXT971A_MIRROR_REG             0x10
#define LXT971A_STATUS2_REG            0x11

/* LXT971A control register bit definitions */
#define LXT971A_CTRL_RESET         0x8000
#define LXT971A_CTRL_LOOPBACK      0x4000
#define LXT971A_CTRL_SPEED         0x2000
#define LXT971A_CTRL_AUTONEG       0x1000
#define LXT971A_CTRL_DUPLEX        0x0100
#define LXT971A_CTRL_RESTART_AUTO  0x0200
#define LXT971A_CTRL_COLL          0x0080
#define LXT971A_CTRL_DUPLEX_MODE   0x0100

/* Auto-negatiation advertisement register bit definitions */
#define LXT971A_AUTONEG_ADV_100BTX_FULL     0x100
#define LXT971A_AUTONEG_ADV_100BTX          0x80
#define LXT971A_AUTONEG_ADV_10BTX_FULL      0x40
#define LXT971A_AUTONEG_ADV_10BT            0x20
#define AUTONEG_ADV_IEEE_8023               0x1

/* Auto-negatiation Link register bit definitions */
#define LXT971A_AUTONEG_LINK_100BTX_FULL     0x100
#define LXT971A_AUTONEG_LINK_100BTX          0x80
#define LXT971A_AUTONEG_LINK_10BTX_FULL      0x40

/* Status register bit definitions */
#define LXT971A_STATUS_COMPLETE     0x20
#define LXT971A_STATUS_AUTONEG_ABLE 0x04

/* Status2 register bit definitions */
#define LXT971A_STATUS2_COMPLETE    0x80
#define LXT971A_STATUS2_POLARITY    0x20
#define LXT971A_STATUS2_NEGOTIATED  0x100
#define LXT971A_STATUS2_FULL        0x200
#define LXT971A_STATUS2_LINK_UP     0x400
#define LXT971A_STATUS2_100         0x4000


// #define ARCTANGENT_EMAC_DEBUG

#ifdef ARCTANGENT_EMAC_DEBUG
#define dbg_printk(fmt, args...)	\
		printk ("ARCTangent emac: "); \
		printk (fmt, ## args);
#else
#define dbg_printk(fmt, args...)
#endif

#define __mdio_write(priv, phy_reg, val)	\
{                                                   \
    priv->mdio_complete = 0;				\
	EMAC_REG(priv)->mdio = (0x50020000 | (PHY_ID << 23) | \
					          (phy_reg << 18) | (val & 0xffff)) ; \
	while (!priv->mdio_complete);			\
}

#define __mdio_read(priv, phy_reg, val)	\
{                                                   \
    priv->mdio_complete = 0;				\
	EMAC_REG(priv)->mdio = (0x60020000 | (PHY_ID << 23) | (phy_reg << 18));	\
	while (!priv->mdio_complete);			\
	val = EMAC_REG(priv)->mdio; \
	val &= 0xffff;              \
}

typedef struct {
	unsigned int    info;
	void           *data;
}
arc_emac_bd_t;

#define RX_BD_NUM	128	/* Number of recieve BD's */
#define TX_BD_NUM	128	/* Number of transmit BD's */

#define RX_RING_SZ  (RX_BD_NUM * sizeof(arc_emac_bd_t))
#define TX_RING_SZ  (TX_BD_NUM * sizeof(arc_emac_bd_t))

struct arc_emac_priv
{
		struct net_device_stats stats;
		/* base address of the register set for this device */
		arc_emac_reg  *reg_base_addr;
		spinlock_t      lock;

        /* pointers to BD Rings - CPU side */
		arc_emac_bd_t *rxbd;
		arc_emac_bd_t *txbd;

#ifdef ARC_EMAC_COH_MEM
        /* pointers to BD rings (bus addr) - Device side */
		dma_addr_t rxbd_dma_hdl, txbd_dma_hdl;
#else
        /* BD Ring memory - above point somewhere in here */
        char buffer[RX_RING_SZ + RX_RING_SZ + L1_CACHE_BYTES];
#endif

		/* The actual socket buffers */
		struct sk_buff *rx_skbuff[RX_BD_NUM];
		struct sk_buff *tx_skbuff[TX_BD_NUM];
		unsigned int    txbd_curr;
		unsigned int    txbd_dirty;

        /* Remember where driver last saw a pkt, so next iternation it
         * starts from here and not 0
         */
        unsigned int    last_rx_bd;
		/*
		 * Set by interrupt handler to indicate completion of mdio
		 * operation. reset by reader (see __mdio_read())
		 */
		volatile unsigned int mdio_complete;
        struct napi_struct napi;
};

/*
 * This MAC addr is given to the first opened EMAC, the last byte will be
 * incremented by 1 each time so succesive emacs will get a different
 * hardware address
 * Intialised while  processing parameters in setup.c
 */
	extern struct sockaddr mac_addr;


static void noinline emac_nonimp_intr(struct arc_emac_priv *ap,
        unsigned int status);
static int arc_thread(void *unused);
static int arc_emac_tx_clean(struct arc_emac_priv *ap);
void arc_emac_update_stats(struct arc_emac_priv * ap);

static int arc_emac_clean(struct net_device *dev
#ifdef CONFIG_EMAC_NAPI
    ,unsigned int work_to_do
#endif
);


#ifdef EMAC_STATS
//Stats proc file system
static int read_proc(char *sysbuf, char **start,
			    off_t off, int count, int *eof, void *data);
struct proc_dir_entry *myproc;
#endif

static void     dump_phy_status(unsigned int status)
{

/* Intel LXT971A STATUS2, bit 5 says "Polarity" is reversed if set. */
/* Not exactly sure what this means, but if I invert the bits they seem right */

    if (status & LXT971A_STATUS2_POLARITY)
        status=~status;

	printk("LXT971A: 0x%08x ", status);

	if (status & LXT971A_STATUS2_100)
		printk("100mbps, ");
	else
		printk("10mbps, ");

	if (status & LXT971A_STATUS2_FULL)
		printk("full duplex, ");
	else
		printk("half duplex, ");

	if (status & LXT971A_STATUS2_NEGOTIATED)
	{
		printk("auto-negotiation ");
		if (status & LXT971A_STATUS2_COMPLETE)
			printk("complete, ");
		else
			printk("in progress, ");
	} else
		printk("manual mode, ");

	if (status & LXT971A_STATUS2_LINK_UP)
		printk("link is up\n");
	else
		printk("link is down\n");
}


#ifdef CONFIG_EMAC_NAPI

static int arc_emac_poll (struct napi_struct *napi, int budget)
{
    struct net_device *dev = napi->dev;
	struct arc_emac_priv *ap = netdev_priv(dev);
    unsigned int work_done;

    work_done = arc_emac_clean(dev, budget);

    if(work_done < budget)
    {
        napi_complete(napi);
        EMAC_REG(ap)->enable |= RXINT_MASK;
    }

//    printk("work done %u budget %u\n", work_done, budget);
    return(work_done);
}

#endif

//int debug_emac = 1;

static int arc_emac_clean(struct net_device *dev
#ifdef CONFIG_EMAC_NAPI
    ,unsigned int work_to_do
#endif
)
{
	struct arc_emac_priv *ap = netdev_priv(dev);
	unsigned int len, info;
	struct sk_buff *skb, *skbnew;
    int work_done=0, i, loop;

    /* Loop thru the BD chain, but not always from 0.
     * start from right after where we last saw a pkt
     */
    i = ap->last_rx_bd;

    for (loop = 0; loop < RX_BD_NUM; loop++)
	{
        i = (i + 1) & (RX_BD_NUM - 1);

		info = arc_emac_read(&ap->rxbd[i].info);

        /* BD contains a packet for CPU to grab */
        if (likely((info & OWN_MASK) == FOR_CPU))
        {
            /* Make a note that we saw a pkt at this BD.
             * So next time, driver starts from this + 1
             */
            ap->last_rx_bd = i;

            /* Packet fits in one BD (Non Fragmented) */
			if (likely((info & (FRST_MASK|LAST_MASK)) == (FRST_MASK|LAST_MASK)))
			{
				len = info & LEN_MASK;
				ap->stats.rx_packets++;
				ap->stats.rx_bytes += len;
                skb = ap->rx_skbuff[i];

                /* Get a new SKB for replenishing BD for next cycle */
				if (likely(skb_count)) { /* Cached skbs ready to go */
					skbnew = skb_prealloc[skb_count];
					skb_count--;
				}
                else { /* get it from stack */
					if (!(skbnew = dev_alloc_skb(dev->mtu + VMAC_BUFFER_PAD)))
					{
						printk(KERN_INFO "Out of Memory, dropping packet\n");

						/* return buffer to VMAC */
						arc_emac_write(&ap->rxbd[i].info,
							      (FOR_EMAC| (dev->mtu + VMAC_BUFFER_PAD)));
						ap->stats.rx_dropped++;
                        continue;
					}
#ifdef EMAC_STATS
                    else {
                        // Not fatal, purely for statistical purposes
						skb_not_preallocated++;
					}
#endif
				}

                /* Actually preparing the BD for next cycle */

				skb_reserve(skbnew, 2); /* IP hdr align, eth is 14 bytes */
				ap->rx_skbuff[i] = skbnew;

				arc_emac_write(&ap->rxbd[i].data, skbnew->data);
				arc_emac_write(&ap->rxbd[i].info,
							      (FOR_EMAC | (dev->mtu + VMAC_BUFFER_PAD)));

                /* Prepare arrived pkt for delivery to stack */

				inv_dcache_range((unsigned long)skb->data,
                                 (unsigned long)skb->data + len);

				skb->dev = dev;
				skb_put(skb, len - 4);	/* Make room for data */
				skb->protocol = eth_type_trans(skb, dev);

#if 0
                if (debug_emac) {

extern void print_hex_dump(const char *level, const char *prefix_str,
    int prefix_type, int rowsize, int groupsize,
    const void *buf, size_t len, bool ascii);
extern void print_hex_dump_bytes(const char *prefix_str, int prefix_type,
   const void *buf, size_t len);
                            printk("\n--------------\n");
                            print_hex_dump_bytes("", DUMP_PREFIX_NONE,
                                skb->data, 64);

                }
#endif

#ifdef CONFIG_EMAC_NAPI
		        /* Correct NAPI smenatics: If work quota exceeded return
                 * don't dequeue any further packets
                 */
                work_done++;
                netif_receive_skb(skb);
                if(work_done >= work_to_do) break;
#else
                netif_rx(skb);
#endif
            }
            else {
	/*
	 * We dont allow chaining of recieve packets. We want to do "zero
	 * copy" and sk_buff structure is not chaining friendly when we dont
	 * want to copy data. We preallocate buffers of MTU size so incoming
	 * packets wont be chained
	 */
				printk(KERN_INFO "Rx chained, Packet bigger than device MTU\n");
				/* Return buffer to VMAC */
				arc_emac_write(&ap->rxbd[i].info,
							      (FOR_EMAC| (dev->mtu + VMAC_BUFFER_PAD)));
				ap->stats.rx_length_errors++;
		    }

        }  // BD for CPU
    } // BD chain loop

    return work_done;
}

static irqreturn_t arc_emac_intr (int irq, void *dev_instance)
{
   	struct net_device *dev = (struct net_device *) dev_instance;
	struct arc_emac_priv *ap = netdev_priv(dev);
	unsigned int status, enable;

	status = EMAC_REG(ap)->status;
    EMAC_REG(ap)->status = status;
    enable = EMAC_REG(ap)->enable;

    if (likely(status & (RXINT_MASK|TXINT_MASK))) {
	if (status & RXINT_MASK)
	{

#ifdef CONFIG_EMAC_NAPI
        if(likely(napi_schedule_prep(&ap->napi)))
        {
            EMAC_REG(ap)->enable &= ~RXINT_MASK; // no more interrupts.
            __napi_schedule(&ap->napi);
        }
#else
        arc_emac_clean(dev);
#endif
    }
    if (status & TXINT_MASK)
    {
        arc_emac_tx_clean(ap);
    }
    }
    else {
        emac_nonimp_intr(ap, status);
    }
    return IRQ_HANDLED;
}

static void noinline
emac_nonimp_intr(struct arc_emac_priv *ap, unsigned int status)
{
    if (status & MDIO_MASK)
	{
		/* Mark the mdio operation as complete.
		 * This is reset by the MDIO reader
		 */
		ap->mdio_complete = 1;
	}


	if (status & ERR_MASK)
	{
		if (status & TXCH_MASK)
		{
			ap->stats.tx_errors++;
			ap->stats.tx_aborted_errors++;
			printk(KERN_ERR "Transmit_chaining error! txbd_dirty = %u\n", ap->txbd_dirty);
		} else if (status & MSER_MASK)
		{
			ap->stats.rx_missed_errors += 255;
			ap->stats.rx_errors += 255;
		} else if (status & RXCR_MASK)
		{
			ap->stats.rx_crc_errors += 255;
			ap->stats.rx_errors += 255;
		} else if (status & RXFR_MASK)
		{
			ap->stats.rx_frame_errors += 255;
			ap->stats.rx_errors += 255;
		} else if (status & RXFL_MASK)
		{
			ap->stats.rx_over_errors += 255;
			ap->stats.rx_errors += 255;
		} else
		{
			printk(KERN_ERR "ARCTangent Vmac: Unkown Error status = 0x%x\n", status);
		}
	}
}


static int arc_emac_tx_clean(struct arc_emac_priv *ap)
{
	unsigned int i, info;
	struct sk_buff *skb;

		/*
		 * Kind of short circuiting code taking advantage of the fact
		 * that the ARC emac will release multiple "sent" packets in
		 * order. So if this interrupt was not for the current
		 * (txbd_dirty) packet then no other "next" packets were
		 * sent. The manual says that TX interrupt can occur even if
		 * no packets were sent and there may not be 1 to 1
		 * correspondence of interrupts and number of packets queued
		 * to send
		 */
		for (i = 0; i < TX_BD_NUM; i++)
		{
			info = arc_emac_read(&ap->txbd[ap->txbd_dirty].info);

#ifdef EMAC_STATS

            if ( info & (DROP|DEFR|LTCL|UFLO)) {
			    if (info & DROP) emac_drop++;
			    if (info & DEFR) emac_defr++;
			    if (info & LTCL) emac_ltcl++;
			    if (info & UFLO) emac_uflo++;
            }
#endif
			if ((info & FOR_EMAC) ||
			    !(arc_emac_read(&ap->txbd[ap->txbd_dirty].data)))
			{
	            return IRQ_HANDLED;
			}

			if (info & LAST_MASK)
			{
				skb = ap->tx_skbuff[ap->txbd_dirty];
				ap->stats.tx_packets++;
				ap->stats.tx_bytes += skb->len;
				/* return the sk_buff to system */
				dev_kfree_skb_irq(skb);
			}
			arc_emac_write(&ap->txbd[ap->txbd_dirty].data, 0x0);
			arc_emac_write(&ap->txbd[ap->txbd_dirty].info, 0x0);
			ap->txbd_dirty = (ap->txbd_dirty + 1) % TX_BD_NUM;
		}

	return IRQ_HANDLED;
}

/* arc emac open routine */
int
arc_emac_open(struct net_device * dev)
{
	struct arc_emac_priv *ap;
	arc_emac_bd_t *bd;
	struct sk_buff *skb;
	int             i;
	unsigned int    temp, duplex;
	int             noauto;

	ap = netdev_priv(dev);
	if (ap == NULL)
		return -ENODEV;

	/* Register interrupt handler for device */
	request_irq(dev->irq, arc_emac_intr, 0, dev->name, dev);

	EMAC_REG(ap)->enable |= MDIO_MASK;	/* MDIO Complete Interrupt Mask */

	/* Reset the PHY */
	__mdio_write(ap, LXT971A_CTRL_REG, LXT971A_CTRL_RESET);

	/* Wait till the PHY has finished resetting */
	i = 0;
	do
	{
		__mdio_read(ap, LXT971A_CTRL_REG, temp);
		i++;
	} while (i < AUTO_NEG_TIMEOUT && (temp & LXT971A_CTRL_RESET));

	noauto = (i >= AUTO_NEG_TIMEOUT);	/* A bit hacky this, but we
						 * want to be able to enable
						 * networking without the
						 * cable being plugged in */
	if (noauto)
		__mdio_write(ap, LXT971A_CTRL_REG, 0);

	/* Advertize capabilities */
	temp = LXT971A_AUTONEG_ADV_10BT | AUTONEG_ADV_IEEE_8023;

	/* If the system clock is greater than 25Mhz then advertize 100 */
	if (clk_speed > 25000000)
		temp |= LXT971A_AUTONEG_ADV_100BTX;

	__mdio_write(ap, LXT971A_AUTONEG_ADV_REG, temp);

	if (!noauto)
	{
		/* Start Auto-Negotiation */
		__mdio_write(ap, LXT971A_CTRL_REG,
			(LXT971A_CTRL_AUTONEG | LXT971A_CTRL_RESTART_AUTO));

		/* Wait for Auto Negotiation to complete */
		i = 0;
		do
		{
			__mdio_read(ap, LXT971A_STATUS2_REG, temp);
			i++;
		} while ((i < AUTO_NEG_TIMEOUT) && !(temp & LXT971A_STATUS2_COMPLETE));

		if (i < AUTO_NEG_TIMEOUT)
		{
			/*
			 * Check if full duplex is supported and set the vmac
			 * accordingly
			 */
			if (temp & LXT971A_STATUS2_FULL)
				duplex = ENFL_MASK;
			else
				duplex = 0;
		} else
		{
			noauto = 1;
		}
	}
	if (noauto)
	{
		/*
		 * Auto-negotiation timed out - this happens if the network
		 * cable is unplugged Force 10mbps, half-duplex.
		 */
		printk("Emac - forcing manual PHY config.\n");
		__mdio_write(ap, LXT971A_CTRL_REG, 0);
		duplex = 0;
	}
	__mdio_read(ap, LXT971A_STATUS2_REG, temp);
	dump_phy_status(temp);

    printk("EMAC MTU %d\n", dev->mtu);

	/* Allocate and set buffers for rx BD's */
	bd = ap->rxbd;
	for (i = 0; i < RX_BD_NUM; i++)
	{
		skb = dev_alloc_skb(dev->mtu + VMAC_BUFFER_PAD);
		/* IP header Alignment (14 byte Ethernet header) */
		skb_reserve(skb, 2);
		ap->rx_skbuff[i] = skb;
		arc_emac_write(&bd->data, skb->data);
		/* VMAC owns rx descriptors */
		arc_emac_write(&bd->info, FOR_EMAC | (dev->mtu + VMAC_BUFFER_PAD));
		bd++;
	}

    /* setup last seen to MAX, so driver starts from 0 */
    ap->last_rx_bd = RX_BD_NUM - 1;

	/* Allocate tx BD's similar to rx BD's */
	/* All TX BD's owned by CPU */
	bd = ap->txbd;
	for (i = 0; i < TX_BD_NUM; i++)
	{
		arc_emac_write(&bd->data, 0);
		arc_emac_write(&bd->info, 0);
		bd++;
	}
	/* Initialize logical address filter */
	EMAC_REG(ap)->lafl = 0x0;
	EMAC_REG(ap)->lafh = 0x0;

	/* Set BD ring pointers for device side */
#ifdef ARC_EMAC_COH_MEM
	EMAC_REG(ap)->rx_ring = (unsigned int) ap->rxbd_dma_hdl;
	EMAC_REG(ap)->tx_ring = (unsigned int) ap->rxbd_dma_hdl + RX_RING_SZ;
#else
	EMAC_REG(ap)->rx_ring = (unsigned int) ap->rxbd;
	EMAC_REG(ap)->tx_ring = (unsigned int) ap->txbd;
#endif

	/* Set Poll rate so that it polls every 1 ms */
	EMAC_REG(ap)->pollrate = (clk_speed / 1000000);

	/*
	 * Enable interrupts. Note: interrupts wont actually come till we set
	 * CONTROL below.
	 */
	/* FIXME :: enable all intrs later */
	EMAC_REG(ap)->enable = TXINT_MASK |	/* Transmit interrupt */
		RXINT_MASK |	/* Recieve interrupt */
		ERR_MASK |	/* Error interrupt */
		TXCH_MASK |	/* Transmit chaining error interrupt */
        MSER_MASK |
		MDIO_MASK;	/* MDIO Complete Intereupt Mask */

	/* Set CONTROL */
	EMAC_REG(ap)->ctrl = (RX_BD_NUM << 24) |	/* RX buffer desc table len */
		(TX_BD_NUM << 16) |	/* TX buffer des tabel len */
		TXRN_MASK |	/* TX enable */
		RXRN_MASK |	/* RX enable */
		duplex;		/* Full Duplex enable */

	EMAC_REG(ap)->ctrl |= EN_MASK;	/* VMAC enable */

#ifdef CONFIG_EMAC_NAPI
    netif_wake_queue(dev);
    napi_enable(&ap->napi);
#else
	netif_start_queue(dev);
#endif

	printk(KERN_INFO "%s up\n", dev->name);

	//ARC Emac helper thread.
    kthread_run(arc_thread, 0, "EMAC helper");

#ifdef EMAC_STATS
    myproc = create_proc_entry("emac", 0644, NULL);
	if (myproc)
	{
		myproc->read_proc = read_proc;
	}
#endif

	return 0;
}

/* arc_emac close routine */
int
arc_emac_stop(struct net_device * dev)
{
	struct arc_emac_priv *ap = netdev_priv(dev);

#ifdef CONFIG_EMAC_NAPI
    napi_disable(&ap->napi);
#endif

	/* Disable VMAC */
	EMAC_REG(ap)->ctrl &= (~EN_MASK);

	netif_stop_queue(dev);	/* stop the queue for this device */

	free_irq(dev->irq, dev);

	/* close code here */
	printk(KERN_INFO "%s down\n", dev->name);

	return 0;
}

/* arc emac ioctl commands */
int
arc_emac_ioctl(struct net_device * dev, struct ifreq * rq, int cmd)
{
	dbg_printk("ioctl called\n");
	/* FIXME :: not ioctls yet :( */
	return (-EOPNOTSUPP);
}

/* arc emac get statistics */
struct net_device_stats *
arc_emac_stats(struct net_device * dev)
{
	unsigned long flags;
	struct arc_emac_priv *ap = netdev_priv(dev);

	spin_lock_irqsave(&ap->lock, flags);
	arc_emac_update_stats(ap);
	spin_unlock_irqrestore(&ap->lock, flags);

	return (&ap->stats);
}

void
arc_emac_update_stats(struct arc_emac_priv * ap)
{
	unsigned long   miss, rxerr, rxfram, rxcrc, rxoflow;

	rxerr = EMAC_REG(ap)->rxerr;
	miss = EMAC_REG(ap)->miss;

	rxcrc = (rxerr & 0xff);
	rxfram = (rxerr >> 8 & 0xff);
	rxoflow = (rxerr >> 16 & 0xff);

	ap->stats.rx_errors += miss;
	ap->stats.rx_errors += rxcrc + rxfram + rxoflow;

	ap->stats.rx_over_errors += rxoflow;
	ap->stats.rx_frame_errors += rxfram;
	ap->stats.rx_crc_errors += rxcrc;
	ap->stats.rx_missed_errors += miss;

	/* update the stats from the VMAC */
	return;
}

/* arc emac transmit routine */
int
arc_emac_tx(struct sk_buff * skb, struct net_device * dev)
{
	int             len, bitmask;
	unsigned int    info;
	char           *pkt;
	struct arc_emac_priv *ap = netdev_priv(dev);

	len = skb->len < ETH_ZLEN ? ETH_ZLEN : skb->len;
	pkt = skb->data;
	dev->trans_start = jiffies;

	flush_dcache_range((unsigned long)pkt, (unsigned long)pkt + len);

	/* Set First bit */
	bitmask = FRST_MASK;

tx_next_chunk:

    info = arc_emac_read(&ap->txbd[ap->txbd_curr].info);
	if (likely((info & OWN_MASK) == FOR_CPU))
	{
		arc_emac_write(&ap->txbd[ap->txbd_curr].data, pkt);

        /* This case handles 2 scenarios:
         * 1. pkt fits into 1 BD (2k)
         * 2. Last chunk of pkt (in multiples of 2k)
         */
        if (likely(len <= MAX_TX_BUFFER_LEN))
        {
	        ap->tx_skbuff[ap->txbd_curr] = skb;

			/*
			 * Set data length, bit mask and give ownership to
			 * VMAC This should be the last thing we do. Vmac
			 * might immediately start sending
			 */
			arc_emac_write(&ap->txbd[ap->txbd_curr].info,
					      FOR_EMAC | bitmask | LAST_MASK | len);

	        /* Set TXPOLL bit to force a poll */
	        EMAC_REG(ap)->status |= TXPL_MASK;

		    ap->txbd_curr = (ap->txbd_curr + 1) % TX_BD_NUM;
            return 0;
        }
        else {
            /* if pkt > 2k, this case handles all non last chunks */

			arc_emac_write(&ap->txbd[ap->txbd_curr].info,
					      FOR_EMAC | bitmask | (len & (MAX_TX_BUFFER_LEN-1)));

            /* clear the FIRST CHUNK bit */
            bitmask = 0;

            len -= MAX_TX_BUFFER_LEN;
        }

		ap->txbd_curr = (ap->txbd_curr + 1) % TX_BD_NUM;
        goto tx_next_chunk;

	}
    else
	{
        //if (!netif_queue_stopped(dev))
        //{
		//    printk(KERN_INFO "Out of TX buffers\n");
        //}
        emac_txfull++;
        return NETDEV_TX_BUSY;
    }
}

/* the transmission timeout function */
void
arc_emac_tx_timeout(struct net_device * dev)
{
	printk(KERN_CRIT "transmission timeout\n");
	return;
}

/* the set multicast list method */
void
arc_emac_set_multicast_list(struct net_device * dev)
{
	dbg_printk("set multicast list called\n");
	return;
}

/*
 * Set MAC address of the interface. Called from te core after a SIOCSIFADDR
 * ioctl and from open above
 */
int
arc_emac_set_address(struct net_device * dev, void *p)
{
	struct sockaddr *addr = p;
	struct arc_emac_priv *ap = netdev_priv(dev);

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	EMAC_REG(ap)->addrl = *(unsigned int *) dev->dev_addr;
	EMAC_REG(ap)->addrh = (*(unsigned int *) &dev->dev_addr[4]) & 0x0000ffff;

//	printk(KERN_INFO "MAC address set to %s",print_mac(buf, dev->dev_addr));

	return 0;
}

static const struct net_device_ops arc_emac_netdev_ops = {
	.ndo_open 			= arc_emac_open,
	.ndo_stop 			= arc_emac_stop,
	.ndo_start_xmit 	= arc_emac_tx,
	.ndo_set_multicast_list = arc_emac_set_multicast_list,
	.ndo_tx_timeout 	= arc_emac_tx_timeout,
	.ndo_set_mac_address= arc_emac_set_address,
    .ndo_get_stats      = arc_emac_stats,
    // .ndo_do_ioctl    = arc_emac_ioctl;
	// .ndo_change_mtu 	= arc_emac_change_mtu, FIXME:  not allowed
};

static int __devinit arc_emac_probe(struct platform_device *pdev)
{
	struct net_device *dev;
	struct arc_emac_priv *priv;
	int err = -ENODEV;
	arc_emac_reg *reg;
	unsigned int id;

	/* Probe for all the vmac's */

	/* Calculate the register base address of this instance (num) */
	reg = (arc_emac_reg *)(VMAC_REG_BASEADDR);
	id = reg->id;

	/* Check for VMAC revision 5 or 7, magic number */
	if (!(id == 0x0005fd02 || id == 0x0007fd02)) {
		printk_init("***ARC EMAC [NOT] detected, skipping EMAC init\n");
		return -ENODEV;
	}

	printk_init("ARCTangent EMAC detected\n");

    /*
     * Allocate a net device structure and the priv structure and
     * intitialize to generic ethernet values
     */
    dev = alloc_etherdev(sizeof(struct arc_emac_priv));
    if (dev == NULL)
        return -ENOMEM;

    SET_NETDEV_DEV(dev, &pdev->dev);
	platform_set_drvdata(pdev, dev);

    priv = netdev_priv(dev);
	priv->reg_base_addr = reg;

#ifdef ARC_EMAC_COH_MEM
    /* alloc cache coheret memory for BD Rings - to avoid need to do
     * explicit uncached ".di" accesses, which can potentially screwup
     */
	priv->rxbd = dma_alloc_coherent(&dev->dev,
                        (RX_RING_SZ + TX_RING_SZ),
                        &priv->rxbd_dma_hdl, GFP_KERNEL);

    /* to keep things simple - we just do 1 big alloc, instead of 2
     * seperate ones for Rx and Tx Rings resp
     */
    priv->txbd_dma_hdl = priv->rxbd_dma_hdl + RX_RING_SZ;
#else
    /* setup ring pointers to first L1 cache aligned location in buffer[]
     * which has enough gutter to accomodate the space lost to alignement
     */
    priv->rxbd = (arc_emac_bd_t *)L1_CACHE_ALIGN(priv->buffer);
#endif

    priv->txbd = priv->rxbd + RX_BD_NUM;

    printk_init("EMAC pvt data %x (%lx bytes), Rx Ring [%x], Tx Ring[%x]\n",
        (unsigned int)priv, sizeof(struct arc_emac_priv),
        (unsigned int)priv->rxbd,(unsigned int)priv->txbd);

#ifdef ARC_EMAC_COH_MEM
    printk_init("EMAC Device addr: Rx Ring [0x%x], Tx Ring[%x]\n",
        (unsigned int)priv->rxbd_dma_hdl,
        (unsigned int)priv->txbd_dma_hdl);

#else
    printk_init("EMAC %x %x\n", (unsigned int)&priv->buffer[0],
                                (unsigned int)&priv->rx_skbuff[0]);
#endif

	/* populate our net_device structure */
    dev->netdev_ops = &arc_emac_netdev_ops;

	dev->watchdog_timeo = TX_TIMEOUT;
	/* FIXME :: no multicast support yet */
	dev->flags &= ~IFF_MULTICAST;
	dev->irq = VMAC_IRQ;

	/* Set EMAC hardware address */
	arc_emac_set_address(dev, &mac_addr);

	spin_lock_init(&priv->lock);

#ifdef CONFIG_EMAC_NAPI
    netif_napi_add(dev,&priv->napi,arc_emac_poll, NAPI_WEIGHT);
#endif

	err = register_netdev(dev);

	if (err) {
	    free_netdev(dev);
    }

	return err;
}

static int arc_emac_remove(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);

    /* unregister the network device */
    unregister_netdev(dev);

#ifdef ARC_EMAC_COH_MEM
    {
        struct arc_emac_priv *priv = netdev_priv(dev);
	    dma_free_coherent(&dev->dev, (RX_RING_SZ + TX_RING_SZ),
                  priv->rxbd, priv->rxbd_dma_hdl);
    }
#endif

    free_netdev(dev);

    return 0;
}

static struct platform_device *arc_emac_dev;
static struct platform_driver arc_emac_driver = {
     .driver = {
         .name = "arc_emac",
     },
     .probe = arc_emac_probe,
     .remove = arc_emac_remove
};

int __init arc_emac_init(void)
{
    int err;

    if ((err = platform_driver_register(&arc_emac_driver))) {
        printk_init("emac driver registration failed\n");
        return err;
    }

    if (!(arc_emac_dev = platform_device_alloc("arc_emac",0))) {
        printk_init("emac dev alloc failed\n");
        return -ENOMEM;
    }

    err = platform_device_add(arc_emac_dev);

    return err;
}

void __exit arc_emac_exit(void) {
	platform_device_unregister(arc_emac_dev);
	platform_driver_unregister(&arc_emac_driver);
}

module_init(arc_emac_init);
module_exit(arc_emac_exit);

static int arc_thread(void *unused) //helps with interrupt mitigation.
{
	unsigned int    i;
	while (1)
	{
		msleep(100);

		if (!skb_count)
		{

			for (i = 1; i != SKB_PREALLOC; i++)
			{
				skb_prealloc[i] = dev_alloc_skb(1500 + VMAC_BUFFER_PAD);
				//MTU
					if (!skb_prealloc[i])
					printk(KERN_CRIT "Failed to get an SKB\n");

			}
			skb_count = SKB_PREALLOC - 1;
		}
	}

    return 0;
}

#ifdef EMAC_STATS
static int read_proc(char *sysbuf, char **start,
			    off_t off, int count, int *eof, void *data)
{
	int  len;

	len = sprintf(sysbuf, "\nARC EMAC STATISTICS\n");
	len += sprintf(sysbuf + len, "-------------------\n");
	len += sprintf(sysbuf + len, "SKB Pre-allocated available buffers : %u\n", skb_count);
	len += sprintf(sysbuf + len, "SKB Pre-allocated maximum           : %u\n", SKB_PREALLOC);
	len += sprintf(sysbuf + len, "Number of intr allocated SKB's used : %u\n", skb_not_preallocated);
	len += sprintf(sysbuf + len, "EMAC DEFR count : %u\n", emac_defr);
	len += sprintf(sysbuf + len, "EMAC DROP count : %u\n", emac_drop);
	len += sprintf(sysbuf + len, "EMAC LTCL count : %u\n", emac_ltcl);
	len += sprintf(sysbuf + len, "EMAC UFLO count : %u\n", emac_uflo);
	len += sprintf(sysbuf + len, "EMAC TxFull count : %u\n", emac_txfull);
	return (len);
}
#endif
