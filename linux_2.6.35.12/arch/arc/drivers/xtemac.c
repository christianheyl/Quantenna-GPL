/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Simon Spooner - Dec 2009 - Original version.
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

#include <asm/cacheflush.h>
#include <asm/irq.h>
#include <linux/platform_device.h>
#include "xtemac.h"

#define DRIVER_NAME         "xilinx_temac"
#define TX_RING_LEN         2
#define RX_RING_LEN         32


/* Prototypes */

int xtemac_open(struct net_device *);
int xtemac_tx(struct sk_buff *, struct net_device *);
void xtemac_tx_timeout(struct net_device *);
void xtemac_set_multicast_list (struct net_device *);
void xtemac_tx_timeout(struct net_device *);
struct net_device_stats * xtemac_stats(struct net_device *);
int xtemac_stop(struct net_device *);
int xtemac_set_address(struct net_device *, void *);
void print_phy_status2(unsigned int );
void print_packet(unsigned int, unsigned char *);

static int read_proc(char *sysbuf, char **start, off_t off, int count, int *eof, void *data);
static int write_proc(char *sysbuf, char ** buffer, off_t count, int l_sysbuf, int zero);

struct proc_dir_entry *myproc;

static const struct net_device_ops xtemac_netdev_ops = {
	.ndo_open 			= xtemac_open,
	.ndo_stop 			= xtemac_stop,
	.ndo_start_xmit 	= xtemac_tx,
	.ndo_set_multicast_list = xtemac_set_multicast_list,
	.ndo_tx_timeout 	= xtemac_tx_timeout,
	.ndo_set_mac_address= xtemac_set_address,
    .ndo_get_stats      = xtemac_stats,
    // .ndo_do_ioctl    = aa3_emac_ioctl;
	// .ndo_change_mtu 	= aa3_emac_change_mtu, FIXME:  not allowed
};


struct xtemac_stats
{
    unsigned int rx_packets;
    unsigned int rx_bytes;
};



struct xtemac_ptr
{
        struct xtemac_bridge bridge;
        struct xtemac_configuration config;
        struct xtemac_address_filter addrFilter;
        struct xtemac_bridge_extn bridgeExtn;

};

volatile struct xtemac_ptr *xtemac = (volatile struct xtemac_ptr *)XTEMAC_BASE;

volatile struct xtemac_priv
	{
		struct net_device_stats stats;
        spinlock_t      lock;
        unsigned char   address[6];
        unsigned int    RxEntries;
        unsigned int    RxCurrent;
        struct sk_buff  *tx_skb;
        struct net_device      *ndev;   // This device.
        void *          tx_buff;        // Area packets TX from
        void *          rx_buff;        // Area packets DMA into
        unsigned int    rx_len;
        struct  xtemac_stats phy_stats;     // PHY stats.

	};

volatile unsigned int debug=0;

void XEMAC_mdio_write (volatile struct xtemac_ptr *,unsigned int, unsigned int , unsigned int);
void xtemac_update_stats(struct xtemac_priv * ap);
unsigned int XEMAC_mdio_read ( volatile struct xtemac_ptr * , unsigned int, unsigned int, volatile unsigned int *);



extern struct sockaddr mac_addr;	/* Intialised while


						 * processing parameters in
						 * setup.c */


/****************************/
/* XTEMAC interrupt handler */
/***************************/

static irqreturn_t
xtemac_emac_intr(int irq, void *dev_instance)
{

    volatile unsigned int    status;
    struct xtemac_priv  *priv = netdev_priv(dev_instance);
    struct sk_buff  *skb;

    status = arc_read_uncached_32(&xtemac->bridge.intrStsReg);


    if(status & INTR_STATUS_MDIO_BIT_POS)
    {
        if(debug)
            printk("TEMAC MDIO INT\n");

        arc_write_uncached_32(&xtemac->bridge.intrClrReg, INTR_STATUS_MDIO_BIT_POS);
    }


    if(status & INTR_STATUS_RX_ERROR_BIT_POS)
    {
        printk("TEMAC RX_ERROR\n");

        arc_write_uncached_32(&xtemac->bridge.intrClrReg, INTR_STATUS_RX_ERROR_BIT_POS);
        arc_write_uncached_32(&xtemac->bridgeExtn.intrEnableReg, ENABLE_ALL_INTR & (~ENABLE_RX_OVERRUN_INTR));
        printk("intrEnableReg = %x\n", arc_read_uncached_32(&xtemac->bridgeExtn.intrEnableReg));
    }

    if(status & INTR_STATUS_TX_ERROR_BIT_POS)
    {
        printk("TEMAC TX ERROR\n");
        arc_write_uncached_32(&xtemac->bridge.intrClrReg ,INTR_STATUS_TX_ERROR_BIT_POS);
    }

    if(status & INTR_STATUS_DMA_TX_ERROR_BIT_POS)
    {
        printk("TEMAC DMA TX ERROR\n");
        arc_write_uncached_32(&xtemac->bridge.intrClrReg, INTR_STATUS_DMA_TX_ERROR_BIT_POS);
    }
    if(status & INTR_STATUS_DMA_RX_ERROR_BIT_POS)
    {
        printk("TEMAC DMA RX ERROR\n");
        arc_write_uncached_32(&xtemac->bridge.intrClrReg, INTR_STATUS_DMA_RX_ERROR_BIT_POS);
    }

// RX FIFO DMA'd to main memory.
// ack the interrupt
// invalidate the cache line
// setup an SKB and pass in the packet into the kernel.
// re-enable RX interrupts to allow next packet in.


    if(status & INTR_STATUS_DMA_RX_COMPLETE_BIT_POS)
    {
        if(debug)
            printk("TEMAC DMA RX COMPLETE\n");
        arc_write_uncached_32(&xtemac->bridge.dmaRxClrReg, 1);
//        arc_write_uncached_32(&xtemac->bridge.intrClrReg, INTR_STATUS_DMA_RX_COMPLETE_BIT_POS);
        arc_write_uncached_32(&xtemac->bridgeExtn.intrEnableReg,ENABLE_ALL_INTR & (~ENABLE_RX_OVERRUN_INTR));

        priv->stats.rx_packets++;
        flush_and_inv_dcache_range(priv->rx_buff, priv->rx_buff +priv->rx_len);
// was in

        skb=alloc_skb(priv->rx_len + 32, GFP_ATOMIC);
        skb_reserve(skb,NET_IP_ALIGN);      // Align header
        memcpy(skb->data, priv->rx_buff, priv->rx_len);
#if 0
        printk("RX:");
        print_packet(priv->rx_len, priv->rx_buff);
        printk("\n");
#endif
        skb_put(skb,priv->rx_len);
        skb->dev = dev_instance;
        skb->protocol = eth_type_trans(skb,dev_instance);
        skb->ip_summed = CHECKSUM_NONE;
        netif_rx(skb);

    }

// TX FIFO DMA'd out.
// ack the interrupt, reset the TX buffer


     if(status & INTR_STATUS_DMA_TX_COMPLETE_BIT_POS)
    {
        if(debug)
            printk("TEMAC DMA TX COMPLETE\n");
        arc_write_uncached_32(&xtemac->bridge.dmaTxClrReg , 1);
        priv->tx_skb = 0;
    }


// Receive a packet.
// Start to DMA into a physically contiguous buffer.
// Switch off RX interrupts until DMA complete.

    if(status & INTR_STATUS_RCV_INTR_BIT_POS )
    {
        if(debug)
            printk("TEMAC RCV INTR\n");
        arc_write_uncached_32(&xtemac->bridge.intrClrReg , INTR_STATUS_RCV_INTR_BIT_POS);
//        arc_write_uncached_32(&xtemac->config.rxCtrl1Reg, 0); // Disable RX
        arc_write_uncached_32(&xtemac->bridgeExtn.intrEnableReg , ENABLE_ALL_INTR & (~ENABLE_PKT_RECV_INTR ) & (~ENABLE_RX_OVERRUN_INTR) );

        priv->rx_len = arc_read_uncached_32(&xtemac->bridgeExtn.sizeReg);
        arc_write_uncached_32(&xtemac->bridge.dmaRxAddrReg , priv->rx_buff);
        inv_dcache_range(priv->rx_buff, priv->rx_buff + priv->rx_len);
        arc_write_uncached_32(&xtemac->bridge.dmaRxCmdReg , (priv->rx_len << 8 | DMA_READ_COMMAND));
    }


// Handle an RX FIFO overrun.
// ack the interrupt, handle this as a regular RX and DMA
// the packet into the RX buffer

    if(status & INTR_STATUS_RX_OVRN_INTR_BIT_POS)
    {
        printk("TEMAC RX OVERRUN\n");
        arc_write_uncached_32(&xtemac->bridge.dmaRxClrReg,1);
        arc_write_uncached_32(&xtemac->bridge.intrClrReg , INTR_STATUS_RX_OVRN_INTR_BIT_POS);
        arc_write_uncached_32(&xtemac->config.rxCtrl1Reg,0);
        arc_write_uncached_32(&xtemac->bridge.txFifoRxFifoRstReg, RESET_TX_RX_FIFO);
        arc_write_uncached_32(&xtemac->bridge.txFifoRxFifoRstReg, 0);

    }

	return IRQ_HANDLED;
}

/***************/
/* XTEAMC open */
/***************/

int
xtemac_open(struct net_device * dev)
{

    unsigned int speed;
    unsigned int temp;

// OK, now switch on the hardware.
    arc_write_uncached_32(&xtemac->config.rxCtrl1Reg,RESET_FIFO);
    arc_write_uncached_32(&xtemac->bridgeExtn.intrEnableReg,0);
    arc_write_uncached_32(&xtemac->bridge.intrClrReg , CLEAR_INTR_REG);
    arc_write_uncached_32(&xtemac->bridge.txFifoRxFifoRstReg, RESET_TX_RX_FIFO);
    arc_write_uncached_32(&xtemac->bridge.txFifoRxFifoRstReg,0);
    request_irq(dev->irq, xtemac_emac_intr, 0, dev->name, dev);
    arc_write_uncached_32(&xtemac->config.rxCtrl1Reg ,  ENABLE_RX );//| HALF_DUPLEX);
    arc_write_uncached_32(&xtemac->config.txCtrlReg ,  ENABLE_TX); // | HALF_DUPLEX);


//    arc_write_uncached_32(&xtemac->config.macModeConfigReg, 0x40000000); // 100mbit
    arc_write_uncached_32(&xtemac->config.macModeConfigReg, SET_100MBPS_MODE); // 100mbit


// Probe the Phy to see what speed is negotiated.
printk("Setup phy\n");
     XEMAC_mdio_read(xtemac, BSP_XEMAC1_PHY_ID,MV_88E1111_STATUS2_REG, &speed);
    printk("Phy setup\n");
    print_phy_status2(speed);

    myproc = create_proc_entry("temac", 0644, NULL);
    if (myproc)
    {
        myproc->read_proc = read_proc;
        myproc->write_proc = write_proc;
    }
printk("TEMAC Opened\n");

// Enable Address Filter

    xtemac->addrFilter.addrFilterModeReg= ENABLE_ADDR_FILTER;

    printk("XTEMAC - Address filter enabled\n");

// Set the mac address

    xtemac_set_address(dev,&mac_addr);


// Reset the transceiver

    printk("XTEMAC - Reset the transceiver\n");

     XEMAC_mdio_write(xtemac,BSP_XEMAC1_PHY_ID , MV_88E1111_CTRL_REG, MV_88E1111_CTRL_RESET);
     do {
        XEMAC_mdio_read(xtemac,BSP_XEMAC1_PHY_ID,MV_88E1111_CTRL_REG, &temp);
        } while(temp & MV_88E1111_CTRL_RESET);

    printk("XTEMAC - Transceiver reset\n");

 /* Advertise capabilities */

    temp = MV_88E1111_AUTONEG_ADV_100BTX_FULL | MV_88E1111_AUTONEG_ADV_100BTX |                AUTONEG_ADV_IEEE_8023;

    XEMAC_mdio_write(xtemac, BSP_XEMAC1_PHY_ID, MV_88E1111_AUTONEG_ADV_REG, temp);

    printk("XTEMAC - Advertise capabilities\n");

// Autonegotiate the connection
    XEMAC_mdio_write(xtemac,BSP_XEMAC1_PHY_ID,MV_88E1111_CTRL_REG, (MV_88E1111_CTRL_AUTONEG | MV_88E1111_CTRL_RESTART_AUTO));
// Wait for autoneg to complete

    do {
        XEMAC_mdio_read(xtemac,BSP_XEMAC1_PHY_ID,MV_88E1111_STATUS_REG, &temp);
        } while( !(temp & MV_88E1111_STATUS_COMPLETE));
    printk("XTEMAC - Autonegotiate complete\n");

   XEMAC_mdio_read(xtemac,BSP_XEMAC1_PHY_ID,MV_88E1111_STATUS2_REG, &temp);

   if (temp & MV_88E1111_STATUS2_FULL)
        printk("XTEMAC - Full Duplex\n");
    else
        printk("XTEMAC - Not Full Duplex\n");

// Go !

    arc_write_uncached_32(&xtemac->bridgeExtn.intrEnableReg , ENABLE_ALL_INTR & (~ENABLE_RX_OVERRUN_INTR)); // (ENABLE_ALL_INTR & (~ENABLE_RX_OVERRUN_INTR) & (~ENABLE_MDIO_INTR)));

return 0;
}

/* XTEMAC close routine */
int
xtemac_stop(struct net_device * dev)
{
	return 0;
}

/* XTEMAC ioctl commands */
int
xtemac_ioctl(struct net_device * dev, struct ifreq * rq, int cmd)
{
	printk("ioctl called\n");
	/* FIXME :: not ioctls yet :( */
	return (-EOPNOTSUPP);
}

/* XTEMAC transmit routine */


int
xtemac_tx(struct sk_buff * skb, struct net_device * dev)
{
//printk("Transmit...\n");


    struct xtemac_priv *priv = netdev_priv(dev);
    unsigned int tx_space;

    if(priv->tx_skb)
    {
//        printk("Dropping due to previous TX not complete\n");
        return NETDEV_TX_BUSY;
    }

    tx_space = arc_read_uncached_32(&xtemac->bridge.macTxFIFOStatusReg);
    if(tx_space < skb->len)
    {
//        printk("Dropping due to not enough room in the TXFIFO\n");
        return NETDEV_TX_BUSY;   // not enough space in the TX FIFO, throw away
    }

    memcpy(priv->tx_buff, skb->data, skb->len);
//    print_packet(skb->len, priv->tx_buff);
//    printk("\n");
    flush_and_inv_dcache_range(priv->tx_buff, priv->tx_buff + skb->len);
    arc_write_uncached_32(&xtemac->bridge.dmaTxAddrReg , priv->tx_buff);
    arc_write_uncached_32(&xtemac->bridge.dmaTxCmdReg , (skb->len <<8 | DMA_WRITE_COMMAND | DMA_START_OF_PACKET | DMA_END_OF_PACKET));
    priv->stats.tx_packets++;

    priv->tx_skb = skb;
    dev_kfree_skb(priv->tx_skb);
    return(0);

}


/* the transmission timeout function */
void
xtemac_tx_timeout(struct net_device * dev)
{
	printk("transmission timed out\n");
	printk(KERN_CRIT "transmission timeout\n");
	return;
}

/* the set multicast list method */
void
xtemac_set_multicast_list(struct net_device * dev)
{
	printk("set multicast list called\n");
	return;
}

// XTEMAC MAC address set.

int
xtemac_set_address(struct net_device * dev, void *p)
{

    int             i;
    struct sockaddr *addr = p;
    unsigned int temp;

    memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
    memcpy(mac_addr.sa_data, addr->sa_data, dev->addr_len); // store the mac address.
    printk(KERN_INFO "MAC address set to ");
    for (i = 0; i < 6; i++)
        printk("%02x:", dev->dev_addr[i]);
    printk("\n");

    temp = dev->dev_addr[3];
    temp = ((temp << 8) | (dev->dev_addr[2]));
    temp = ((temp << 8) | (dev->dev_addr[1]));
    temp = ((temp << 8) | (dev->dev_addr[0]));
    arc_write_uncached_32(&xtemac->addrFilter.unicastAddr0Reg ,temp);

    temp = dev->dev_addr[5];
    temp = ((temp << 8) | (dev->dev_addr[4]));

    arc_write_uncached_32(&xtemac->addrFilter.unicastAddr1Reg, temp);

	return 0;
}

void xtemac_update_stats( struct xtemac_priv *ap)
{
//
// Code to read the current stats from the XTEMAC and add to ap->stats
//


}

struct net_device_stats *
xtemac_stats(struct net_device * dev)
{

    unsigned long   flags;
    struct xtemac_priv *ap = netdev_priv(dev);

    spin_lock_irqsave(&ap->lock, flags);
    xtemac_update_stats(ap);
    spin_unlock_irqrestore(&ap->lock, flags);

    return (&ap->stats);



}


static int __devinit xtemac_probe(struct platform_device *dev)
{

    struct net_device *ndev;
    struct xtemac_priv *priv;
    unsigned int rc;

printk("Probing...\n");

// Setup a new netdevice

    ndev = alloc_etherdev(sizeof(struct xtemac_priv));
    if (!ndev)
    {
        printk("Failed to allocated net device\n");
        return -ENOMEM;
    }

    dev_set_drvdata(dev,ndev);

    ndev->irq = 6;
    xtemac_set_address(dev,&mac_addr);
    priv = netdev_priv(ndev);
    priv->ndev = ndev;
    priv->tx_skb = 0;

    ndev->netdev_ops = &xtemac_netdev_ops;
    ndev->watchdog_timeo = (400*HZ/1000);
    ndev->flags &= ~IFF_MULTICAST;

// Setup RX buffer

    priv->rx_buff = kmalloc(4096,GFP_ATOMIC | GFP_DMA);
    priv->tx_buff = kmalloc(4096,GFP_ATOMIC | GFP_DMA);

    printk("RX Buffer @ %x , TX Buffer @ %x\n", (unsigned int)priv->rx_buff,(unsigned int) priv->tx_buff);

    if(!priv->rx_buff || !priv->tx_buff)
    {
        printk("Failed to alloc FIFO buffers\n");
        return -ENOMEM;
    }

    spin_lock_init(&((struct xtemac_priv *) netdev_priv(ndev))->lock);


// Register net device with kernel, now ifconfig should see the device

    rc = register_netdev(ndev);
    if (rc)
    {
        printk("Didn't register the netdev.\n");
        return(rc);
    }

   return(0);

}

static void xtemac_remove(struct net_device *dev)
{
    struct net_device *ndev = dev_get_drvdata(dev);
    unregister_netdev(ndev);

}



void print_packet(unsigned int len, unsigned char * packet)
{
    unsigned int    n;
    printk("Printing packet\nLen = %u\n", len);

    for(n=0;n<len;n++)
    {
        if(! (n %20))
            printk("\n");
        printk("%02x-",*packet++);
    }
    printk("\n");
}



void print_phy_status2(unsigned int status)
{

    printk("Status is %x\n", status);

    if(status & MV_88E1111_STATUS2_FULL)
        printk("Full duplex\n");
    if(status & MV_88E1111_STATUS2_LINK_UP)
        printk("Link up\n");
    if(status & MV_88E1111_STATUS2_100)
        printk("100Mbps\n");
    if(status & MV_88E1111_STATUS2_1000)
        printk("1000Mbps\n");
}

void  XEMAC_mdio_write
    (
      /* [IN] the device physical address */
      volatile struct xtemac_ptr *dev_ptr,

      /* [IN] The device physical address */
      unsigned int              phy_addr,

      /* [IN] the register index (0-31)*/
      unsigned int              reg_index,

      /* [IN] The data to be written to register */
      unsigned int              data
   )
{ /* Body */
    unsigned int          value;

    arc_write_uncached_32(&dev_ptr->config.mgmtConfigReg, MDIO_ENABLE);
    arc_write_uncached_32(&dev_ptr->bridgeExtn.phyAddrReg, phy_addr);
    arc_write_uncached_32((&dev_ptr->bridgeExtn.macMDIOReg + reg_index), data);

    value = arc_read_uncached_32(&dev_ptr->bridgeExtn.intrRawStsReg);

    while (!(value & INTR_STATUS_MDIO_BIT_POS))
    {
        value = arc_read_uncached_32(&dev_ptr->bridgeExtn.intrRawStsReg);
    }

    arc_write_uncached_32(&dev_ptr->bridge.intrClrReg, INTR_STATUS_MDIO_BIT_POS);
//    printk("Finished MDIO write\n");

} /* Endbody */




/* [IN] The device physical address */
/* [IN] the register index (0-31)*/
/* [IN]/[OUT] The data pointer */
unsigned int XEMAC_mdio_read ( volatile struct xtemac_ptr * dev_ptr,
     unsigned int                phy_addr,
     unsigned int                reg_index,
     volatile unsigned int                * data_ptr
  )
{
    unsigned int value = 0;
    unsigned int count=0;

    arc_write_uncached_32(&dev_ptr->config.mgmtConfigReg , MDIO_ENABLE);
    arc_write_uncached_32(&dev_ptr->bridgeExtn.phyAddrReg , phy_addr);
   *data_ptr = arc_read_uncached_32(&dev_ptr->bridgeExtn.macMDIOReg + reg_index);

   value = arc_read_uncached_32(&dev_ptr->bridgeExtn.intrRawStsReg);

   while (!(value & INTR_STATUS_MDIO_BIT_POS))
   {
       count++;
       value = arc_read_uncached_32(&dev_ptr->bridgeExtn.intrRawStsReg);
   }

   arc_write_uncached_32(&dev_ptr->bridge.intrClrReg ,  INTR_STATUS_MDIO_BIT_POS);
//    printk("read MDIO status in %x counts\n", count);
    return 0;
}

static int read_proc(char *sysbuf, char **start,
                 off_t off, int count, int *eof, void *data)
{
    int  len;
    unsigned int status;
    unsigned int perid;
    unsigned int dma_tx_status_reg;
    unsigned int dma_rx_status_reg;
    unsigned int intr_enable_reg;

    status = arc_read_uncached_32(&xtemac->bridge.intrStsReg);
    perid =  arc_read_uncached_32(&xtemac->bridge.idReg);
    dma_tx_status_reg = arc_read_uncached_32(&xtemac->bridge.dmaTxStsReg);
    dma_rx_status_reg = arc_read_uncached_32(&xtemac->bridge.dmaRxStsReg);
    intr_enable_reg = arc_read_uncached_32(&xtemac->bridgeExtn.intrEnableReg);


    len = sprintf(sysbuf, "\nARC AA5 TEMAC STATISTICS\n");
    len += sprintf(sysbuf + len, "------------------------\n");
    len += sprintf(sysbuf + len, "Base address of bridge regs %x\n", (unsigned int)&xtemac->bridge);
    len += sprintf(sysbuf + len, "idReg             %x\n", arc_read_uncached_32(&xtemac->bridge.idReg));

    len += sprintf(sysbuf + len, "dmaTxCmdReg       %x\n", arc_read_uncached_32(&xtemac->bridge.dmaTxCmdReg));


    len += sprintf(sysbuf + len, "dmaTxAddrReg      %x\n", arc_read_uncached_32(&xtemac->bridge.dmaTxAddrReg));

    len += sprintf(sysbuf + len, "dmaTxStsReg       %x\n", arc_read_uncached_32(&xtemac->bridge.dmaTxStsReg));

    len += sprintf(sysbuf + len, "dmaTxClrReg       %x\n", arc_read_uncached_32(&xtemac->bridge.dmaTxClrReg));


    len += sprintf(sysbuf + len, "macTxFIFOStatusReg %x\n", arc_read_uncached_32(&xtemac->bridge.macTxFIFOStatusReg));


    len += sprintf(sysbuf + len, "dmaRxCmdReg        %x\n", arc_read_uncached_32(&xtemac->bridge.dmaRxCmdReg));

    len += sprintf(sysbuf + len, "dmaRxAddrReg        %x\n", arc_read_uncached_32(&xtemac->bridge.dmaRxAddrReg));

    len += sprintf(sysbuf + len, "dmaRxStsReg        %x\n", arc_read_uncached_32(&xtemac->bridge.dmaRxStsReg));
    len += sprintf(sysbuf + len, "dmaRxClrReg        %x\n", arc_read_uncached_32(&xtemac->bridge.dmaRxClrReg));
    len += sprintf(sysbuf + len, "txFifoRxFifoRstReg %x\n", arc_read_uncached_32(&xtemac->bridge.txFifoRxFifoRstReg));
    len += sprintf(sysbuf + len, "intrStsReg         %x\n", arc_read_uncached_32(&xtemac->bridge.intrStsReg));
//    len += sprintf(sysbuf + len, "intrClrReg         %x\n", arc_read_uncached_32(&xtemac->bridge.intrClrReg));
    len += sprintf(sysbuf + len, "pioDataCountReg    %x\n", arc_read_uncached_32(&xtemac->bridge.pioDataCountReg));
    len += sprintf(sysbuf + len, "arbSelReg          %x\n", arc_read_uncached_32(&xtemac->bridge.arbSelReg));
//    len += sprintf(sysbuf + len, "dataReg            %x\n", arc_read_uncached_32(&xtemac->bridge.dataReg));


// Config regs

    len += sprintf(sysbuf + len, "\n\nConfig regs @ %x\n", (unsigned int)&xtemac->config);

    len += sprintf(sysbuf + len, "rxCtrl0Reg         %x\n", arc_read_uncached_32(&xtemac->config.rxCtrl0Reg));
    len += sprintf(sysbuf + len, "rxCtrl1Reg         %x\n", arc_read_uncached_32(&xtemac->config.rxCtrl1Reg));
    len += sprintf(sysbuf + len, "txCtrlReg          %x\n", arc_read_uncached_32(&xtemac->config.txCtrlReg));
    len += sprintf(sysbuf + len, "flowCtrlConfigReg  %x\n", arc_read_uncached_32(&xtemac->config.flowCtrlConfigReg));
    len += sprintf(sysbuf + len, "macModeConfigReg   %x\n", arc_read_uncached_32(&xtemac->config.macModeConfigReg));
    len += sprintf(sysbuf + len, "rgmsgmiiConfigReg  %x\n", arc_read_uncached_32(&xtemac->config.rgmsgmiiConfigReg));
    len += sprintf(sysbuf + len, "mgmtConfigReg      %x\n", arc_read_uncached_32(&xtemac->config.mgmtConfigReg));

// Filter regs

    len += sprintf(sysbuf + len, "\n\naddrFilter regs @ %x\n", (unsigned int)&xtemac->addrFilter);
    len += sprintf(sysbuf + len, "unicastAddr0Reg    %x\n", arc_read_uncached_32(&xtemac->addrFilter.unicastAddr0Reg));
    len += sprintf(sysbuf + len, "unicastAddr1Reg    %x\n", arc_read_uncached_32(&xtemac->addrFilter.unicastAddr1Reg));
    len += sprintf(sysbuf + len, "genAddrTableAccess0Reg %x\n", arc_read_uncached_32(&xtemac->addrFilter.genAddrTableAccess0Reg));
    len += sprintf(sysbuf + len, "genAddrTableAccess1Reg %x\n", arc_read_uncached_32(&xtemac->addrFilter.genAddrTableAccess1Reg));
    len += sprintf(sysbuf + len, "addrFilterModeReg  %x\n", arc_read_uncached_32(&xtemac->addrFilter.addrFilterModeReg));


//Bridge Extn

    len += sprintf(sysbuf + len, "\n\nBridgeExtn regs @ %x\n", (unsigned int)&xtemac->bridgeExtn);

    len += sprintf(sysbuf + len, "sizeReg            %x\n", arc_read_uncached_32(&xtemac->bridgeExtn.sizeReg));
    len += sprintf(sysbuf + len, "phyAddrReg         %x\n", arc_read_uncached_32(&xtemac->bridgeExtn.phyAddrReg));
    len += sprintf(sysbuf + len, "intrEnableReg      %x\n", arc_read_uncached_32(&xtemac->bridgeExtn.intrEnableReg));
    len += sprintf(sysbuf + len, "IntrRawStsReg      %x\n", arc_read_uncached_32(&xtemac->bridgeExtn.intrRawStsReg));
//    len += sprintf(sysbuf + len, "macMDIOReg         %x\n", arc_read_uncached_32(&xtemac->bridgeExtn.macMDIOReg));
    return (len);

}

static int write_proc(char *sysbuf, char ** buffer, off_t count, int l_sysbuf, int zero)
{

    volatile unsigned int   reg;
    volatile unsigned int   val;
    char from_proc[80];     // bad, buffer overflow !

    if(copy_from_user(from_proc, buffer, count))
        return -EFAULT;

    from_proc[count-1]=0;

     /* First character indicates read or write */

     if(from_proc[0]=='w')
     {   /* WRITE AUX REG */

         sscanf(from_proc, "w%x=%x", &reg, &val);
         printk("writing %x to %x\n", val,reg);
         arc_write_uncached_32(reg, val);
     }
    else if (from_proc[0]=='d')
    {
        debug = ~debug;  // invert debug status.
    }



return count;

}

static struct platform_driver xtemac_driver = {
     .driver = {
         .name = DRIVER_NAME,
     },
     .probe = xtemac_probe,
     .remove = xtemac_remove
};



int __init
xtemac_module_init(void)
{

printk("*** Init XTEMAC ***\n");
    return platform_driver_register(&xtemac_driver);


}

void __exit
xtemac_module_cleanup(void)
{
	return;

}


module_init(xtemac_module_init);
module_exit(xtemac_module_cleanup);
