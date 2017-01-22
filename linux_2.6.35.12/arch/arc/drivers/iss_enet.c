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
#include "iss_enet.h"

#define DRIVER_NAME         "arc_iss"
#define TX_RING_LEN         2
#define RX_RING_LEN         32


/* Prototypes */

int iss_open(struct net_device *);
int iss_tx(struct sk_buff *, struct net_device *);
void iss_tx_timeout(struct net_device *);
void iss_set_multicast_list (struct net_device *);
void iss_tx_timeout(struct net_device *);
struct net_device_stats * iss_stats(struct net_device *);
int iss_stop(struct net_device *);
int iss_set_address(struct net_device *, void *);
void print_packet(unsigned int, unsigned char *);

static int read_proc(char *sysbuf, char **start, off_t off, int count, int *eof, void *data);
static int write_proc(char *sysbuf, char ** buffer, off_t count, int l_sysbuf, int zero);

struct proc_dir_entry *myproc;

static const struct net_device_ops iss_netdev_ops = {
	.ndo_open 			= iss_open,
	.ndo_stop 			= iss_stop,
	.ndo_start_xmit 	= iss_tx,
	.ndo_set_multicast_list = iss_set_multicast_list,
	.ndo_tx_timeout 	= iss_tx_timeout,
	.ndo_set_mac_address= iss_set_address,
    .ndo_get_stats      = iss_stats,
};


struct iss_stats
{
    unsigned int rx_packets;
    unsigned int rx_bytes;
};





volatile struct iss_priv
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
        struct  iss_stats phy_stats;     // PHY stats.

	};

volatile unsigned int debug=0;

void iss_update_stats(struct iss_priv * ap);
volatile struct emwsim_struct *ewsim = 0xc0fc2000; // HW access


extern struct sockaddr mac_addr;	/* Intialised while


						 * processing parameters in
						 * setup.c */


/****************************/
/* ISS interrupt handler */
/***************************/

static irqreturn_t
iss_intr(int irq, void *dev_instance)
{

    volatile unsigned int    status;
    struct iss_priv  *priv = netdev_priv(dev_instance);
    struct sk_buff  *skb;
    unsigned int rxlen;

    if(ewsim->STATUS & EMWSIM_STATUS_RX_RECEIVED)
    {
        rxlen = ewsim->RX_SIZE;
	if (rxlen > 64000) {
		 printk (KERN_ALERT "Received packet to large! (rxlen = %d)\n",
			 rxlen);
	}
        skb = alloc_skb(rxlen + 32, GFP_ATOMIC);
	if (!skb) {
		printk ("COULDN'T ALLOCATE SKB!!\n");
	}
        skb_reserve(skb,NET_IP_ALIGN); // Align header
        memcpy(skb->data, priv->rx_buff, rxlen);
        skb_put(skb,rxlen);
        skb->dev = dev_instance;
        skb->protocol = eth_type_trans(skb,dev_instance);
        skb->ip_summed = CHECKSUM_NONE;
        netif_rx(skb);
        priv->stats.rx_packets++;

//        printk("Packet @ %x\n", ewsim->RX_BUFFER);
//       printk("priv->rx_buff # %x\n", priv->rx_buff);
//        print_packet(rxlen,priv->rx_buff);

    }
    else
        printk("Unknown reason to interrupt\n");
    ewsim->CONTROL |= EMWSIM_CONTROL_RECEIVED;
    ewsim->RX_BUFFER = priv->rx_buff;
    ewsim->CONTROL |= EMWSIM_CONTROL_READY_TO_RX;
	return IRQ_HANDLED;
}

/***************/
/* ISS    open */
/***************/

int
iss_open(struct net_device * dev)
{

    struct iss_priv *priv = netdev_priv(dev);
    myproc = create_proc_entry("iss_mac", 0644, NULL);
    if (myproc)
    {
        myproc->read_proc = read_proc;
        myproc->write_proc = write_proc;
    }

// Setup RX and TX buffers in device.

    ewsim->RX_BUFFER = priv->rx_buff;
    ewsim->TX_BUFFER = priv->tx_buff;
    ewsim->CONTROL |= EMWSIM_CONTROL_INITIALIZE;
    if(!(ewsim->STATUS & EMWSIM_STATUS_INITIALIZED))
    {
        printk("Device Failed to Init\n");
        return(-ENODEV);
    }

// Enable interrupts
//    ewsim->INTERRUPT_CONFIGURATION=0x00010006;
//    printk("Opened ISS MAC, Interrupt Configuration %x\n", ewsim->INTERRUPT_CONFIGURATION);
    ewsim->CONTROL=0;
    request_irq((ewsim->INTERRUPT_CONFIGURATION & 0xff), iss_intr,0, dev->name, dev);
    ewsim->CONTROL |= EMWSIM_CONTROL_INT_ENABLE;
    ewsim->CONTROL |= EMWSIM_CONTROL_READY_TO_RX;


return 0;
}

/* ISS close routine */
int
iss_stop(struct net_device * dev)
{
    ewsim->CONTROL &= ~EMWSIM_CONTROL_INT_ENABLE;
    ewsim->CONTROL &= ~EMWSIM_CONTROL_READY_TO_RX;
	return 0;
}

/* ISS ioctl commands */
int
iss_ioctl(struct net_device * dev, struct ifreq * rq, int cmd)
{
	printk("ioctl called\n");
	/* FIXME :: not ioctls yet :( */
	return (-EOPNOTSUPP);
}

/* ISS transmit routine */


int
iss_tx(struct sk_buff * skb, struct net_device * dev)
{
//printk("Transmit...\n");


    struct iss_priv *priv = netdev_priv(dev);
#if 0
    if(priv->tx_skb)
    {
       printk("Dropping due to previous TX not complete\n");
        return NETDEV_TX_BUSY;
    }
#endif
    if(!(ewsim->STATUS & EMWSIM_STATUS_TX_AVAILABLE))
    {
        printk("TX UNAVAILABLE\n");
        return NETDEV_TX_BUSY;
    }

    memcpy(priv->tx_buff, skb->data, skb->len);
    ewsim->TX_BUFFER=priv->tx_buff;
    ewsim->TX_SIZE = skb->len;
    ewsim->CONTROL |= EMWSIM_CONTROL_TRANSMIT;


//    print_packet(skb->len, priv->tx_buff);
//    printk("\n");
    flush_and_inv_dcache_range(priv->tx_buff, priv->tx_buff + skb->len);
    priv->stats.tx_packets++;
    priv->tx_skb = skb;
    dev_kfree_skb(priv->tx_skb);
//printk("Transmit done\n");
    return(0);

}


/* the transmission timeout function */
void
iss_tx_timeout(struct net_device * dev)
{
	printk("transmission timed out\n");
	return;
}

/* the set multicast list method */
void
iss_set_multicast_list(struct net_device * dev)
{
	return;
}

// ISS MAC address set.

int
iss_set_address(struct net_device * dev, void *p)
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

	return 0;
}

void iss_update_stats( struct iss_priv *ap)
{
//
// Code to read the current stats from the ISS  and add to ap->stats
//


}

struct net_device_stats *
iss_stats(struct net_device * dev)

{
    unsigned long   flags;
    struct iss_priv *ap = netdev_priv(dev);

    spin_lock_irqsave(&ap->lock, flags);
    iss_update_stats(ap);
    spin_unlock_irqrestore(&ap->lock, flags);

    return (&ap->stats);

}


static int __devinit iss_probe(struct platform_device *dev)
{

    struct net_device *ndev;
    struct iss_priv *priv;
    unsigned int rc;

    printk("ARC VMAC (simulated) Probing...\n");

// Setup a new netdevice

    ndev = alloc_etherdev(sizeof(struct iss_priv));
    if (!ndev)
    {
        printk("Failed to allocated net device\n");
        return -ENOMEM;
    }

    dev_set_drvdata(dev,ndev);

    ndev->irq = 6;
    iss_set_address(dev,&mac_addr);
    priv = netdev_priv(ndev);
    priv->ndev = ndev;
    priv->tx_skb = 0;

    ndev->netdev_ops = &iss_netdev_ops;
    ndev->watchdog_timeo = (400*HZ/1000);
    ndev->flags &= ~IFF_MULTICAST;


    // Setup RX buffer
    priv->rx_buff = kmalloc(65536,GFP_ATOMIC | GFP_DMA);
    priv->tx_buff = kmalloc(65536,GFP_ATOMIC | GFP_DMA);

    printk("RX Buffer @ %x , TX Buffer @ %x\n", (unsigned int)priv->rx_buff,(unsigned int) priv->tx_buff);

    if(!priv->rx_buff || !priv->tx_buff)
    {
        printk("Failed to alloc FIFO buffers\n");
        return -ENOMEM;
    }

    rc = register_netdev(ndev);
    if (rc)
    {
        printk("Didn't register the netdev.\n");
        return(rc);
    }


   spin_lock_init(&((struct iss_priv *) netdev_priv(ndev))->lock);

   return(0);

}

static void iss_remove(struct net_device *dev)
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



static int read_proc(char *sysbuf, char **start,
                 off_t off, int count, int *eof, void *data)
{
    int  len;

    len = sprintf(sysbuf, "\nARC ISS STATISTICS\n");
    len += sprintf(sysbuf + len, "==================\n");
    return(len);
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
    else if (from_proc[0]='o')
    {
//        request_irq((ewsim->INTERRUPT_CONFIGURATION & 0xff), iss_intr,0, dev->name, dev);
        ewsim->CONTROL |= EMWSIM_CONTROL_INT_ENABLE;
        ewsim->CONTROL |= EMWSIM_CONTROL_READY_TO_RX;
    }


return count;

}

static struct platform_driver iss_driver = {
     .driver = {
         .name = DRIVER_NAME,
     },
     .probe = iss_probe,
     .remove = iss_remove
};

extern int running_on_hw;

int __init
iss_module_init(void)
{
    // So that when running on hardware, it doesn't register
    if (!running_on_hw)
        return platform_driver_register(&iss_driver);
    else {
        printk_init("***ARC VMAC [NOT] detected, skipping init...\n");
        return -1;
    }
}

void __exit
iss_module_cleanup(void)
{
	return;

}


module_init(iss_module_init);
module_exit(iss_module_cleanup);
