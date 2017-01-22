/*
* Copyright c                  Realtek Semiconductor Corporation, 2006
* All rights reserved.
*
* Program : Control  smi connected RTL8366
* Abstract :
* Author : Yu-Mei Pan (ympan@realtek.com.cn)
*  $Id: smi.c 28599 2012-05-07 01:41:37Z kobe_wu $
*/

#include <common.h>
#include <malloc.h>
#include <net.h>
#include <asm/io.h>
#include "rtl8367b_smi.h"
#include "rtl8367b_init.h"
#include <asm/arch/platform.h>
#include <asm/arch/arasan_emac_ahb.h>

#define MDC_MDIO_DUMMY_ID		0
#define MDC_MDIO_CTRL0_REG		31
#define MDC_MDIO_START_REG		29
#define MDC_MDIO_CTRL1_REG		21
#define MDC_MDIO_ADDRESS_REG		23
#define MDC_MDIO_DATA_WRITE_REG		24
#define MDC_MDIO_DATA_READ_REG		25
#define MDC_MDIO_PREAMBLE_LEN		32

#define MDC_MDIO_START_OP		0xFFFF
#define MDC_MDIO_ADDR_OP		0x000E
#define MDC_MDIO_READ_OP		0x0001
#define MDC_MDIO_WRITE_OP		0x0003

#define MDC_MDIO_READ(len, phyId, regId, pData)	mdio_read(len, phyId, regId, pData)
#define MDC_MDIO_WRITE(len, phyId, regId, data)	mdio_write(len, phyId, regId, data)

static unsigned long cur_mdio_base = RUBY_ERROR_ADDR;

/* Function Name:
 *      mdio_read
 * Description:
 *      MDIO read request
 * Input:
 *      len     - Data length read
 *      phyId   - Physical PHY (0-7)
 *      regId   - Physical register to write data
 *      data    - Data read from device
 * Return:
 *      void
 * Note:
 *      None
 */
static void mdio_read(unsigned int len, unsigned int phyId, unsigned int regId, unsigned int *pData)
{
	mdio_postrd_raw(cur_mdio_base, phyId, regId);
	*pData = mdio_rdval_raw(cur_mdio_base, 1);
	if (MDIO_DEBUG) {
		printf("%s: phy %u reg %u data %u\n",
				__FUNCTION__, phyId, regId, *pData);
	}
}

/* Function Name:
 *      mdio_write
 * Description:
 *      MDIO write request
 * Input:
 *		len		- Data length
 *		phyId	- Physical PHY (0-7)
 *		regId	- Physical register to write data
 *		data	- Data to be written
 * Return:
 *      void
 * Note:
 *      None
 */
static void mdio_write(unsigned int len, unsigned int phyId, unsigned int regId, unsigned int data)
{
	mdio_postwr_raw(cur_mdio_base, phyId, regId, data);
	if (MDIO_DEBUG) {
		printf("%s: phy %u reg %u data %u\n",
				__FUNCTION__, phyId, regId, data);
	}
}

/* Function Name:
 *      mdio_base_set
 * Description:
 *      Store mdio base address
 * Input:
 *      mdio_base  - mdio base address
 * Return:
 *      void
 * Note:
 *      None
 */
void smi_mdio_base_set(unsigned long mdio_base)
{
	cur_mdio_base = mdio_base;
}

/* Function Name:
 *      smi_reset
 * Description:
 *      SMI reset control
 * Input:
 *      port     - Physical port number (0-7)
 *      pinRST   - RST pin
 * Return:
 *      RT_ERR_OK               - Success
 *      RT_ERR_FAILED           - SMI access error
 * Note:
 *      None
 */
uint32_t smi_reset(uint32_t port, uint32_t pinRST)
{
	return RT_ERR_OK;
}

/* Function Name:
 *      smi_init
 * Description:
 *      SMI init control
 * Input:
 *      port     - Physical port number (0-7)
 *      pinSCK   - SCK pin
 *      pinSDA   - SDA pin
 * Return:
 *      RT_ERR_OK               - Success
 *      RT_ERR_FAILED           - SMI access error
 * Note:
 *      None
 */
uint32_t smi_init(uint32_t port, uint32_t pinSCK, uint32_t pinSDA)
{
	return RT_ERR_OK;
}

/* Function Name:
 *      smi_read
 * Description:
 *      SMI read control
 * Input:
 *      mAddrs   - SMI 32-bit address
 *      rData    - Data read from device
 * Return:
 *      RT_ERR_OK               - Success
 *      RT_ERR_FAILED           - SMI access error
 * Note:
 *      None
 */
uint32_t smi_read(uint32_t mAddrs, uint32_t *rData)
{
	/* Write Start command to register 29 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_START_REG, MDC_MDIO_START_OP);

	/* Write address control code to register 31 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_CTRL0_REG, MDC_MDIO_ADDR_OP);

	/* Write Start command to register 29 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_START_REG, MDC_MDIO_START_OP);

	/* Write address to register 23 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_ADDRESS_REG, mAddrs);

	/* Write Start command to register 29 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_START_REG, MDC_MDIO_START_OP);

	/* Write read control code to register 21 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_CTRL1_REG, MDC_MDIO_READ_OP);

	/* Write Start command to register 29 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_START_REG, MDC_MDIO_START_OP);

	/* Read data from register 25 */
	MDC_MDIO_READ(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_DATA_READ_REG, rData);

	return RT_ERR_OK;
}

/* Function Name:
 *		smi_write
 * Description:
 *		SMI write control
 * Input:
 *      mAddrs   - SMI 32-bit address
 *      rData    - Data to be written
 * Return:
 *      RT_ERR_OK               - Success
 *      RT_ERR_FAILED           - SMI access error
 * Note:
 *      None
 */
uint32_t smi_write(uint32_t mAddrs, uint32_t rData)
{
	/* Write Start command to register 29 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_START_REG, MDC_MDIO_START_OP);

	/* Write address control code to register 31 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_CTRL0_REG, MDC_MDIO_ADDR_OP);

	/* Write Start command to register 29 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_START_REG, MDC_MDIO_START_OP);

	/* Write address to register 23 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_ADDRESS_REG, mAddrs);

	/* Write Start command to register 29 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_START_REG, MDC_MDIO_START_OP);

	/* Write data to register 24 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_DATA_WRITE_REG, rData);

	/* Write Start command to register 29 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_START_REG, MDC_MDIO_START_OP);

	/* Write data control code to register 21 */
	MDC_MDIO_WRITE(MDC_MDIO_PREAMBLE_LEN, MDC_MDIO_DUMMY_ID, MDC_MDIO_CTRL1_REG, MDC_MDIO_WRITE_OP);

	return RT_ERR_OK;
}

int do_smi_read(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	uint32_t addr;
	uint32_t data;

	addr = simple_strtoul (argv[1], NULL, 16);
	smi_read(addr, &data);
	printf("smi_read: 0x%x = 0x%x\n", addr, data);

	return 0;
}

int do_smi_sweep(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int i;
	uint32_t addr;
	uint32_t data;

	for (i = 0; i < SMI_SWEEP_MAX; i++) {
		if (ctrlc()) {
			break;
		}
		addr = i;
		smi_read(addr, &data);
		if ((data) && (data != 0xffff)) {
			printf("smi_sweep: 0x%x = 0x%x\n", addr, data);
		}
	}

	return 0;
}

int do_smi_write(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	uint32_t addr;
	uint32_t data;

	addr = simple_strtoul (argv[1], NULL, 16);
	data = simple_strtoul (argv[2], NULL, 16);
	printf("smi_write: 0x%x <- 0x%x\n", addr, data);
	smi_write(addr, data);

	return 0;
}

U_BOOT_CMD(smi_read, 6, 1, do_smi_read, "RTL8363EB smi read <reg>", "");
U_BOOT_CMD(smi_write, 6, 2, do_smi_write, "RTL8363EB smi write <reg> <val>", "");
U_BOOT_CMD(smi_sweep, 6, 1, do_smi_sweep, "RTL8363EB smi sweep", "");

