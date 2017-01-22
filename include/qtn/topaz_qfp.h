/*
 * (C) Copyright 2015 Quantenna Communications Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __TOPAZ_QFP_H__
#define __TOPAZ_QFP_H__


#define ioremap ioremap_nocache

/**
 * \brief Initialize topaz PCIe QFP interface.
 *
 * Initialize topaz PCIe QFP interface. This function should be called before
 * any other QFP API call.
 *
 * \param pci_dev pci_dev pci_dev structure point to PCIe adapter
 * \param msi flag for using legacy interrupt (0) or MSI (1)
 *
 * \return 0 on success.
 *
 */
extern int qfp_init(struct pci_dev * pci_dev, int msi);

/**
 * \brief De-initialize topaz PCIe QFP interface.
 *
 * De-initialize topaz PCIe QFP interface.
 *
 * \param pci_dev pci_dev pci_dev structure point to PCIe adapter
 * \param msi flag for using legacy interrupt (0) or MSI (1)
 *
 *
 */
extern void qfp_deinit(struct pci_dev * pci_dev, int msi);

/**
 * \brief register master netdev to QFP
 *
 * Register master netdev to QFP. After calling this function, packets received
 * or transmit through this netdef will be accelerated by QFP.
 *
 * The caller should call this function right before calling register_netdev()
 * for the master netdev.
 *
 * \param netdev pointer to master netdev
 *
 * \return 0 on success and other for failure
 */
extern int qfp_register_netdev(struct net_device * net_dev);

/**
 * \brief un-register master netdev from QFP
 *
 * Un-register master netdev from QFP.
 *
 * The caller should call this function right after calling unregister_netdev()
 * for the master netdev.
 *
 * \param netdev pointer to master netdev
 *
 * \return 0 on success and other for failure
 */
extern void qfp_unregister_netdev(struct net_device * net_dev);

/**
 * \brief register virtual netdev to QFP
 *
 * Register virtual netdev to QFP. After calling this function, packets
 * received or transmit through this netdef will be accelerated by QFP. This
 * function is used to create virtual netdev for VAP.
 *
 * The caller should call this function right before calling register_netdev()
 * for the virtual netdev.
 *
 * \param netdev pointer to virtual netdev
 *
 * \return 0 on success and other for failure
 */
extern int qfp_register_virtual_netdev(struct net_device * net_dev);

/**
 * \brief un-register virtual netdev from QFP
 *
 * Un-register virtual netdev from QFP.
 *
 * The caller should call this function right after calling unregister_netdev()
 * for the virtual netdev.
 *
 * \param netdev pointer to virtual netdev
 *
 * \return 0 on success and other for failure
 */
extern void qfp_unregister_virtual_netdev(struct net_device * net_dev);

/**
 * \brief allocate skb.
 *
 * Allocate a skb from QFP, and all skb will be received to QFP must allocate by
 * calling this function. The caller should call this function instead of any
 * linux skb allocation function for RX packets.
 *
 * \param size max size of bytes for payload of skb
 *
 * \return pointer to a skb or NULL for failure
 */
extern struct sk_buff * qfp_alloc_skb(unsigned int size);

/**
 * \brief Receive skb to QFP.
 *
 * Received a skb which allocate by calling qfp_alloc_skb() to QFP. The caller
 * should call this function instead of calling netif_rx() or netif_receive_skb()
 * The caller loses reference to the skb when this function return successfu. And
 * caller should still call netif_rx() or netif_receive_skb() when this function
 * return failure.
 *
 * \param skb pointer to skb need to received to QFP
 *
 * \return 0 on success; -1 on failure
 */
extern int qfp_rx(struct sk_buff * skb);

#endif

