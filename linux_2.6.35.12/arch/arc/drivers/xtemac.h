/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define XTEMAC_BASE 0xc0fc2000
#define XEMAC_CTRL_ENB_HALF                     (0x04000000)

#define TX_DMA_COMPLETE                         (0x1)
#define TX_DMA_BRIDGE_BUSY                      (0x2)
#define TX_DMA_ERROR                            (0x4)
#define RX_DMA_COMPLETE                         (0x1)
#define RX_DMA_ERROR                            (0x4)


#define MAX_TX_PKTS                             (0x0004)
#define TX_ENABLE_BIT_POS                       (28)
#define RX_ENABLE_BIT_POS                       (28)
#define RECV_FRAME_ODD_BYTE_VALID_POS           (0x2000)
#define MAX_NUM_OF_DEVICES                      (0x0001)

#define DMA_START_OF_PACKET                     (0x00000004)
#define DMA_END_OF_PACKET                       (0x00000002)
#define DMA_WRITE_COMMAND                       (0x00000001)
#define DMA_READ_COMMAND                        (0x00000001)

#define CLEAR_INTR_REG                          (0x101C)
#define RESET_RX_FIFO                           (0x1)
#define RESET_TX_RX_FIFO                        (0x3)
#define ENABLE_RX                               (0x10000000)
#define ENABLE_TX                               (0x10000000)
#define RESET_FIFO                              (0x80000000)
#define HALF_DUPLEX                             (0x04000000)


#define MDIO_ENABLE                             (0x50)

#define ENABLE_ALL_INTR                         (0x131F)
#define ENABLE_PKT_RECV_INTR                    (0x0008)
#define ENABLE_RX_OVERRUN_INTR                  (0x0010)
#define ENABLE_MDIO_INTR                        (0x1000)

#define HOST_INTERFACE                          (0x04000000)
#define SET_1GBPS_MODE                          (0x80000000 | HOST_INTERFACE)
#define SET_100MBPS_MODE                        (0x40000000 | HOST_INTERFACE)
#define SET_10MBPS_MODE                         (0x00000000 | HOST_INTERFACE)

#define ENABLE_ADDR_FILTER                      (0x00000000)

#define INTR_STATUS_MDIO_BIT_POS                (0x00001000)
#define INTR_STATUS_TX_ERROR_BIT_POS            (0x00000400)
#define INTR_STATUS_DMA_TX_ERROR_BIT_POS        (0x00000200)
#define INTR_STATUS_DMA_TX_COMPLETE_BIT_POS     (0x00000100)
#define INTR_STATUS_RX_OVRN_INTR_BIT_POS        (0x00000010)
#define INTR_STATUS_RCV_INTR_BIT_POS            (0x00000008)
#define INTR_STATUS_RX_ERROR_BIT_POS            (0x00000004)
#define INTR_STATUS_DMA_RX_ERROR_BIT_POS        (0x00000002)
#define INTR_STATUS_DMA_RX_COMPLETE_BIT_POS     (0x00000001)


#define BSP_XEMAC1_PHY_ID                           (0x7)

/* Control register bit definitions */
#define MV_88E1111_CTRL_RESET               (0x8000)
#define MV_88E1111_CTRL_LOOPBACK            (0x4000)
#define MV_88E1111_CTRL_SPEED               (0x2000)
#define MV_88E1111_CTRL_AUTONEG             (0x1000)
#define MV_88E1111_CTRL_DUPLEX              (0x0100)
#define MV_88E1111_CTRL_RESTART_AUTO        (0x0200)
#define MV_88E1111_CTRL_COLL                (0x0080)
#define MV_88E1111_CTRL_DUPLEX_MODE         (0x0100)

/* Status register bit definitions */
#define MV_88E1111_STATUS_COMPLETE          (0x20)
#define MV_88E1111_STATUS_AUTONEG_ABLE      (0x04)
#define MV_88E1111_STATUS2_COMPLETE         (0x80)

/* Status2 register bit definitions */
#define MV_88E1111_STATUS2_FULL             (0x2000)
#define MV_88E1111_STATUS2_LINK_UP          (0x400)
#define MV_88E1111_STATUS2_100              (0x4000)
#define MV_88E1111_STATUS2_1000             (0x8000)

/* Auto-negatiation advertisement register bit definitions */
#define MV_88E1111_AUTONEG_ADV_100BTX_FULL  (0x100)
#define MV_88E1111_AUTONEG_ADV_100BTX       (0x80)
#define MV_88E1111_AUTONEG_ADV_10BTX_FULL   (0x40)
#define MV_88E1111_AUTONEG_ADV_10BT         (0x20)
#define AUTONEG_ADV_IEEE_8023               (0x1)

/* Auto-negatiation Link register bit definitions */
#define MV_88E1111_AUTONEG_LINK_100BTX_FULL (0x100)
#define MV_88E1111_AUTONEG_LINK_100BTX      (0x80)
#define MV_88E1111_AUTONEG_LINK_10BTX_FULL  (0x40)

/* MV_88E1111 Registers */
#define MV_88E1111_CTRL_REG                 (0x00)
#define MV_88E1111_STATUS_REG               (0x01)
#define MV_88E1111_AUTONEG_ADV_REG          (0x4)
#define MV_88E1111_AUTONEG_LINK_REG         (0x5)
#define MV_88E1111_MIRROR_REG               (0x10)
#define MV_88E1111_STATUS2_REG              (0x11)

struct xtemac_bridge {
     volatile unsigned int idReg;
     volatile unsigned int dmaTxCmdReg;
     volatile unsigned int dmaTxAddrReg;
     volatile unsigned int dmaTxStsReg;
     volatile unsigned int dmaTxClrReg;
     volatile unsigned int macTxFIFOStatusReg;
     volatile unsigned int dmaRxCmdReg;
     volatile unsigned int dmaRxAddrReg;
     volatile unsigned int dmaRxStsReg;
     volatile unsigned int dmaRxClrReg;
     volatile unsigned int txFifoRxFifoRstReg;
     volatile unsigned int intrStsReg;
     volatile unsigned int intrClrReg;
     volatile unsigned int pioDataCountReg;
     volatile unsigned int arbSelReg;
     volatile unsigned int dataReg;
 };

struct xtemac_configuration {
    volatile unsigned int rxCtrl0Reg;
    volatile unsigned int rxCtrl1Reg;
    volatile unsigned int txCtrlReg;
    volatile unsigned int flowCtrlConfigReg;
    volatile unsigned int macModeConfigReg;
    volatile unsigned int rgmsgmiiConfigReg;
    volatile unsigned int mgmtConfigReg;
};

struct xtemac_address_filter {
    volatile unsigned int unicastAddr0Reg;
    volatile unsigned int unicastAddr1Reg;
    volatile unsigned int genAddrTableAccess0Reg;
    volatile unsigned int genAddrTableAccess1Reg;
    volatile unsigned int addrFilterModeReg;
};

struct xtemac_bridge_extn{
    volatile unsigned int sizeReg;
    volatile unsigned int phyAddrReg;
    volatile unsigned int intrEnableReg;
    volatile unsigned int intrRawStsReg;
    volatile unsigned int macMDIOReg;
    volatile unsigned int reserved[31];
};

