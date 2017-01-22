/*
 * Copyright(c) Realtek Semiconductor Corporation, 2008
 * All rights reserved.
 *
 * $Revision: 28599 $
 * $Date: 2012-05-07 09:41:37 +0800 (星期一, 07 五月 2012) $
 *
 * Purpose : Definition function prototype of RTK API.
 *
 * Feature : Function prototype definition
 *
 */

#ifndef __RTL8367B_INIT_H__
#define __RTL8367B_INIT_H__

#include "../ruby.h"
#include "malloc.h"

#define	_LITTLE_ENDIAN
#undef	CHIP_RTL8365MB
#define	CHIP_RTL8363SB

#define	RTL8367B_REG_DIGITAL_INTERFACE_SELECT    0x1305
#define	RTL8367B_SELECT_GMII_1_OFFSET            4
#define	RTL8367B_SELECT_GMII_1_MASK              0xF0
#define	RTL8367B_SELECT_GMII_0_OFFSET            0
#define	RTL8367B_SELECT_GMII_0_MASK              0xF
#define	RTL8367B_REG_DIGITAL_INTERFACE_SELECT_1  0x13c3
#define	RTL8367B_SKIP_MII_2_RXER_OFFSET          4
#define	RTL8367B_SKIP_MII_2_RXER_MASK            0x10
#define	RTL8367B_SELECT_RGMII_2_OFFSET           0
#define	RTL8367B_SELECT_RGMII_2_MASK             0xF
#define	RTL8367B_REG_EXT0_RGMXF                  0x1306
#define	RTL8367B_REG_EXT1_RGMXF                  0x1307
#define	RTL8367B_REG_EXT2_RGMXF                  0x13c5
#define	RTL8367B_REGBITLENGTH                    16
#define	RTL8367B_REGDATAMAX                      0xFFFF
#define	RTL8367B_MAC7                            7
#define	RTL8367B_EXTNO                           3
#define	RTL8367B_EXT0_RGTX_INV_OFFSET            6
#define	RTL8367B_EXT0_RGTX_INV_MASK              0x40
#define	RTL8367B_EXT0_RGRX_INV_OFFSET            5
#define	RTL8367B_EXT0_RGRX_INV_MASK              0x20
#define	RTL8367B_EXT0_RGMXF_OFFSET               0
#define	RTL8367B_EXT0_RGMXF_MASK                 0x1F
#define	RTL8367B_EXT1_RGTX_INV_OFFSET            6
#define	RTL8367B_EXT1_RGTX_INV_MASK              0x40
#define	RTL8367B_EXT1_RGRX_INV_OFFSET            5
#define	RTL8367B_EXT1_RGRX_INV_MASK              0x20
#define	RTL8367B_EXT1_RGMXF_OFFSET               0
#define	RTL8367B_EXT1_RGMXF_MASK                 0x1F
#define	RTL8367B_REG_EXT_TXC_DLY                 0x13f9
#define	RTL8367B_EXT1_GMII_TX_DELAY_OFFSET       12
#define	RTL8367B_EXT1_GMII_TX_DELAY_MASK         0x7000
#define	RTL8367B_EXT0_GMII_TX_DELAY_OFFSET       9
#define	RTL8367B_EXT0_GMII_TX_DELAY_MASK         0xE00
#define	RTL8367B_EXT2_RGMII_TX_DELAY_OFFSET      6
#define	RTL8367B_EXT2_RGMII_TX_DELAY_MASK        0x1C0
#define	RTL8367B_EXT1_RGMII_TX_DELAY_OFFSET      3
#define	RTL8367B_EXT1_RGMII_TX_DELAY_MASK        0x38
#define	RTL8367B_EXT0_RGMII_TX_DELAY_OFFSET      0
#define	RTL8367B_EXT0_RGMII_TX_DELAY_MASK        0x7
#define	RTL8367B_REG_BYPASS_LINE_RATE            0x03f7
#define	RTL8367B_REG_DIGITAL_INTERFACE0_FORCE    0x1310
#define	RTL8367B_GMII_0_FORCE_OFFSET             12
#define	RTL8367B_GMII_0_FORCE_MASK               0x1000
#define	RTL8367B_RGMII_0_FORCE_OFFSET            0
#define	RTL8367B_RGMII_0_FORCE_MASK              0xFFF
#define	RTL8367B_REG_DIGITAL_INTERFACE1_FORCE    0x1311
#define	RTL8367B_GMII_1_FORCE_OFFSET             12
#define	RTL8367B_GMII_1_FORCE_MASK               0x1000
#define	RTL8367B_RGMII_1_FORCE_OFFSET            0
#define	RTL8367B_RGMII_1_FORCE_MASK              0xFFF
#define	RTL8367B_REG_DIGITAL_INTERFACE2_FORCE    0x13c4
#define	RTL8367B_GMII_2_FORCE_OFFSET             12
#define	RTL8367B_GMII_2_FORCE_MASK               0x1000
#define	RTL8367B_RGMII_2_FORCE_OFFSET            0
#define	RTL8367B_RGMII_2_FORCE_MASK              0xFFF
#define	RTK_INDRECT_ACCESS_CRTL                  0x1f00
#define	RTK_INDRECT_ACCESS_STATUS                0x1f01
#define	RTK_INDRECT_ACCESS_ADDRESS               0x1f02
#define	RTK_INDRECT_ACCESS_WRITE_DATA            0x1f03
#define	RTK_INDRECT_ACCESS_READ_DATA             0x1f04
#define	RTK_INDRECT_ACCESS_DELAY                 0x1f80
#define	RTK_INDRECT_ACCESS_BURST                 0x1f81
#define	RTK_RW_MASK                              0x2
#define	RTK_CMD_MASK                             0x1
#define	RTK_PHY_BUSY_OFFSET                      2
#define	RTK_IVL_MODE_FID                         0xFFFF
#define	RTL8367B_REG_INDRECT_ACCESS_CTRL         0x1f00
#define	RTL8367B_RW_OFFSET                       1
#define	RTL8367B_RW_MASK                         0x2
#define	RTL8367B_CMD_OFFSET                      0
#define	RTL8367B_CMD_MASK                        0x1
#define	RTL8367B_REG_INDRECT_ACCESS_STATUS       0x1f01
#define	RTL8367B_INDRECT_ACCESS_STATUS_OFFSET    2
#define	RTL8367B_INDRECT_ACCESS_STATUS_MASK      0x4
#define	RTL8367B_REG_INDRECT_ACCESS_ADDRESS      0x1f02
#define	RTL8367B_REG_INDRECT_ACCESS_WRITE_DATA   0x1f03
#define	RTL8367B_REG_INDRECT_ACCESS_READ_DATA    0x1f04

typedef	uint32_t	rtk_api_ret_t;
typedef	uint32_t	ret_t;
typedef	uint64_t	rtk_u_long_t;
typedef	uint32_t	rtk_data_t;

#define HERE	do { if (0) printf("%s:%d:%s\n", __FILE__, __LINE__, __FUNCTION__); } while(0)
#define MDIO_DEBUG		0
#define RGMII_TIMING_DEBUG	0
#define SMI_SWEEP_MAX		0x8000

/*
 * Data Type Declaration
 */
typedef enum rt_error_code_e
{
	RT_ERR_FAILED = -1,                         /* General Error																	*/

	/* 0x0000xxxx for common error code */
	RT_ERR_OK = 0,                              /* 0x00000000, OK                                                                   */
	RT_ERR_INPUT,                               /* 0x00000001, invalid input parameter                                              */
	RT_ERR_UNIT_ID,                             /* 0x00000002, invalid unit id                                                      */
	RT_ERR_PORT_ID,                             /* 0x00000003, invalid port id                                                      */
	RT_ERR_PORT_MASK,                           /* 0x00000004, invalid port mask                                                    */
	RT_ERR_PORT_LINKDOWN,                       /* 0x00000005, link down port status                                                */
	RT_ERR_ENTRY_INDEX,                         /* 0x00000006, invalid entry index                                                  */
	RT_ERR_NULL_POINTER,                        /* 0x00000007, input parameter is null pointer                                      */
	RT_ERR_QUEUE_ID,                            /* 0x00000008, invalid queue id                                                     */
	RT_ERR_QUEUE_NUM,                           /* 0x00000009, invalid queue number                                                 */
	RT_ERR_BUSYWAIT_TIMEOUT,                    /* 0x0000000a, busy watting time out                                                */
	RT_ERR_MAC,                                 /* 0x0000000b, invalid mac address                                                  */
	RT_ERR_OUT_OF_RANGE,                        /* 0x0000000c, input parameter out of range                                         */
	RT_ERR_CHIP_NOT_SUPPORTED,                  /* 0x0000000d, functions not supported by this chip model                           */
	RT_ERR_SMI,                                 /* 0x0000000e, SMI error                                                            */
	RT_ERR_NOT_INIT,                            /* 0x0000000f, The module is not initial                                            */
	RT_ERR_CHIP_NOT_FOUND,                      /* 0x00000010, The chip can not found                                               */
	RT_ERR_NOT_ALLOWED,                         /* 0x00000011, actions not allowed by the function                                  */
	RT_ERR_DRIVER_NOT_FOUND,                    /* 0x00000012, The driver can not found                                             */
	RT_ERR_SEM_LOCK_FAILED,                     /* 0x00000013, Failed to lock semaphore                                             */
	RT_ERR_SEM_UNLOCK_FAILED,                   /* 0x00000014, Failed to unlock semaphore                                           */
	RT_ERR_ENABLE,                              /* 0x00000015, invalid enable parameter                                             */
	RT_ERR_TBL_FULL,                            /* 0x00000016, input table full														*/

	/* 0x000exxxx for port ability */
	RT_ERR_PHY_PAGE_ID = 0x000e0000,            /* 0x000e0000, invalid PHY page id                                                  */
	RT_ERR_PHY_REG_ID,                          /* 0x000e0001, invalid PHY reg id                                                   */
	RT_ERR_PHY_DATAMASK,                        /* 0x000e0002, invalid PHY data mask                                                */
	RT_ERR_PHY_AUTO_NEGO_MODE,                  /* 0x000e0003, invalid PHY auto-negotiation mode*/
	RT_ERR_PHY_SPEED,                           /* 0x000e0004, invalid PHY speed setting                                            */
	RT_ERR_PHY_DUPLEX,                          /* 0x000e0005, invalid PHY duplex setting                                           */
	RT_ERR_PHY_FORCE_ABILITY,                   /* 0x000e0006, invalid PHY force mode ability parameter                             */
	RT_ERR_PHY_FORCE_1000,                      /* 0x000e0007, invalid PHY force mode 1G speed setting                              */
	RT_ERR_PHY_TXRX,                            /* 0x000e0008, invalid PHY tx/rx                                                    */
	RT_ERR_PHY_ID,                              /* 0x000e0009, invalid PHY id                                                       */
	RT_ERR_PHY_RTCT_NOT_FINISH,                 /* 0x000e000a, PHY RTCT in progress                                                 */
} rt_error_code_t;

typedef enum rtk_enable_e
{
    DISABLED = 0,
    ENABLED,
    RTK_ENABLE_END
} rtk_enable_t;

typedef enum rtk_port_linkStatus_e
{
    PORT_LINKDOWN = 0,
    PORT_LINKUP,
    PORT_LINKSTATUS_END
} rtk_port_linkStatus_t;

/* enum for port current link speed */
enum SPEEDMODE
{
	SPD_10M = 0,
	SPD_100M,
	SPD_1000M
};

/* enum for mac link mode */
enum LINKMODE
{
	MAC_NORMAL = 0,
	MAC_FORCE,
};

/* enum for port current link duplex mode */
enum DUPLEXMODE
{
	HALF_DUPLEX = 0,
	FULL_DUPLEX
};

/* enum for port current MST mode */
enum MSTMODE
{
	SLAVE_MODE= 0,
	MASTER_MODE
};

enum EXTMODE
{
    EXT_DISABLE = 0,
    EXT_RGMII,
    EXT_MII_MAC,
    EXT_MII_PHY,
    EXT_TMII_MAC,
    EXT_TMII_PHY,
    EXT_GMII,
    EXT_RMII_MAC,
    EXT_RMII_PHY,
    EXT_END
};

typedef struct  rtk_port_mac_ability_s
{
    uint32_t forcemode;
    uint32_t speed;
    uint32_t duplex;
    uint32_t link;
    uint32_t nway;
    uint32_t txpause;
    uint32_t rxpause;
} rtk_port_mac_ability_t;

typedef enum rtk_mode_ext_e
{
    MODE_EXT_DISABLE = 0,
    MODE_EXT_RGMII,
    MODE_EXT_MII_MAC,
    MODE_EXT_MII_PHY,
    MODE_EXT_TMII_MAC,
    MODE_EXT_TMII_PHY,
    MODE_EXT_GMII,
    MODE_EXT_RMII_MAC,
    MODE_EXT_RMII_PHY,
    MODE_EXT_RGMII_33V,
    MODE_EXT_END
} rtk_mode_ext_t;

typedef enum rtk_ext_port_e
{
    EXT_PORT_0 = 0,
    EXT_PORT_1,
    EXT_PORT_2,
    EXT_PORT_END
} rtk_ext_port_t;

typedef struct  rtl8367b_port_ability_s {
#ifdef _LITTLE_ENDIAN
uint16_t speed:2;
uint16_t duplex:1;
uint16_t reserve1:1;
uint16_t link:1;
uint16_t rxpause:1;
uint16_t txpause:1;
uint16_t nway:1;
uint16_t mstmode:1;
uint16_t mstfault:1;
uint16_t reserve2:2;
uint16_t forcemode:1;
uint16_t reserve3:3;
#else
uint16_t reserve3:3;
uint16_t forcemode:1;
uint16_t reserve2:2;
uint16_t mstfault:1;
uint16_t mstmode:1;
uint16_t nway:1;
uint16_t txpause:1;
uint16_t rxpause:1;
uint16_t link:1;
uint16_t reserve1:1;
uint16_t duplex:1;
uint16_t speed:2;
#endif
} rtl8367b_port_ability_t;

#define RTK_MAX_NUM_OF_INTERRUPT_TYPE               2
#define RTK_TOTAL_NUM_OF_WORD_FOR_1BIT_PORT_LIST    1
#define RTK_MAX_NUM_OF_PRIORITY                     8
#define RTK_MAX_NUM_OF_QUEUE                        8
#define RTK_MAX_NUM_OF_TRUNK_HASH_VAL               1
#define RTK_MAX_NUM_OF_PORT                         8
#define RTK_PORT_ID_MAX                             (RTK_MAX_NUM_OF_PORT-1)
#define RTK_PHY_ID_MAX                              (RTK_MAX_NUM_OF_PORT-4)
#define RTK_PORT_COMBO_ID                           4
#define RTK_MAX_NUM_OF_PROTO_TYPE                   0xFFFF
#define RTK_MAX_NUM_OF_MSTI                         0xF
#define RTK_MAX_NUM_OF_LEARN_LIMIT                  0x840
#define RTK_MAX_PORT_MASK                           0xFF
#define RTK_EFID_MAX                                0x7
#define RTK_FID_MAX                                 0xF
#define RTK_MAX_NUM_OF_FILTER_TYPE                  5
#define RTK_MAX_NUM_OF_FILTER_FIELD                 8
#define RTL8367B_PHY_INTERNALNOMAX                  0x4
#define RTL8367B_PHY_REGNOMAX                       0x1F
#define RTL8367B_PHY_EXTERNALMAX                    0x7
#define RTL8367B_PHY_BASE                           0x2000
#define RTL8367B_PHY_EXT_BASE                       0xA000
#define RTL8367B_PHY_OFFSET                         5
#define RTL8367B_PHY_EXT_OFFSET                     9
#define RTL8367B_PHY_PAGE_ADDRESS                   31

#define RTL8367B_QTN_EXT_PHY_ADDR_MIN	1
#define RTL8367B_QTN_EXT_PHY_ADDR_MAX	2

rtk_api_ret_t rtk_port_macForceLinkExt_set(rtk_ext_port_t port, rtk_mode_ext_t mode, rtk_port_mac_ability_t *pPortability);
rtk_api_ret_t rtk_port_rgmiiDelayExt_set(rtk_ext_port_t port, rtk_data_t txDelay, rtk_data_t rxDelay);
rtk_api_ret_t rtk_switch_init(void);

struct emac_private;
ret_t rtl8367b_poll_linkup(struct emac_private *priv);
ret_t rtl8367b_init(struct emac_private *priv, uint32_t emac_cfg);
ret_t rtl8367b_setAsicRegBit(uint32_t reg, uint32_t bit, uint32_t value);
ret_t rtl8367b_getAsicRegBit(uint32_t reg, uint32_t bit, uint32_t *pValue);
ret_t rtl8367b_setAsicRegBits(uint32_t reg, uint32_t bits, uint32_t value);
ret_t rtl8367b_getAsicRegBits(uint32_t reg, uint32_t bits, uint32_t *pValue);
ret_t rtl8367b_setAsicReg(uint32_t reg, uint32_t value);
ret_t rtl8367b_getAsicReg(uint32_t reg, uint32_t *pValue);
ret_t rtl8367b_setAsicPortExtMode(uint32_t id, uint32_t mode);
ret_t rtl8367b_getAsicPortForceLinkExt(uint32_t id, rtl8367b_port_ability_t *pPortAbility);
ret_t rtl8367b_setAsicPortForceLinkExt(uint32_t id, rtl8367b_port_ability_t *pPortAbility);
ret_t rtl8367b_setAsicPHYReg(uint32_t phyNo, uint32_t phyAddr, uint32_t regData);
ret_t rtl8367b_getAsicPHYReg(uint32_t phyNo, uint32_t phyAddr, uint32_t* pRegData);
void mdio_base_set(unsigned int mdio_base);

#endif /* __RTL8367B_INIT_H__ */
