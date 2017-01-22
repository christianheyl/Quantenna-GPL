/*
 * Copyright (c) 2010 Quantenna Communications, Inc.
 * All rights reserved.
 *
 *  SPI driver
 */


///////////////////////////////////////////////////////////////////////////////
//             Includes
///////////////////////////////////////////////////////////////////////////////

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <common/ruby_spi_api.h>
#include "spi_api.h"


///////////////////////////////////////////////////////////////////////////////
//              Types
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//             Globals
///////////////////////////////////////////////////////////////////////////////

/******************************************************************************
Function:spi_protect_mode
Purpose:check the id if we support, and see if the device support protec mode
Returns:0 or < 0
Note:
*****************************************************************************/
int spi_protect_mode(struct flash_info *device)
{
	uint32_t spi_ctrl_val;
	int status = EOPNOTSUPP;

	printk(KERN_INFO "SPI device:%0x\n",device->jedec_id);
	switch(device->single_unprotect_mode){
		case MACRONIX:
			spi_ctrl_val = readl(RUBY_SPI_CONTROL);
			if (spi_read_status() & SPI_SR_QUAD_MODE) {
				printk(KERN_INFO "SPI device is in Quad mode\n");
				writel((SPI_SR_QUAD_MODE_MASK(spi_ctrl_val)), RUBY_SPI_CONTROL);
			} else {
				writel(SPI_SR_SINGLE_MODE_MASK(spi_ctrl_val), RUBY_SPI_CONTROL);
			}

			if (spi_read_scur() & SPI_SCUR_WPSEL) {
				printk(KERN_INFO "SPI Device Is In Protected Mode\n");
				status = 0;
			} else {
				printk(KERN_INFO "Setting SPI Device To Protected Mode..... \n");
				spi_write_prot_select(device);
				/*
				 * This is the place where we check if Device
				 * support individual unprotect sector
				 */

				if (spi_read_scur() & SPI_SCUR_WPSEL) {
					printk(KERN_INFO "SPI Device Is In Protected Mode\n");
					status = 0;
				} else {
					printk(KERN_INFO "SPI Device Do Not Support Individual Unprotect Mode\n");
					status = EOPNOTSUPP;
				}
			}
			break;
		case WINBOND:
                        if (((spi_read_wps()) & SPI_WPS_SELECT)){
				printk(KERN_INFO "SPI Device Is In Protected Mode\n");
                                status = 0;
                        } else {
                                spi_write_prot_select(device);
                                if ((spi_read_wps() & SPI_WPS_SELECT)) {
					printk(KERN_INFO "SPI Device Is In Protected Mode\n");
                                        status = 0;
                                } else {
					printk(KERN_INFO "SPI Device Do Not Support Individual Unprotect Mode\n");
                                        status = EOPNOTSUPP;
                                }
                        }

                        break;

		default:
			printk(KERN_INFO "SPI Device Do Not Support Individual Unprotect Mode\n");
			status =  EOPNOTSUPP;
	}

	return status;
}

/******************************************************************************
* Function:spi_read_scur
* Purpose:Read security register
* Returns: SPI Status Bits
* Note:
******************************************************************************/
uint32_t spi_read_scur(void)
{
	uint32_t spi_ctrl_val;

	spi_ctrl_val = readl(RUBY_SPI_CONTROL);
	writel(RUBY_SPI_READ_SCUR_MASK(spi_ctrl_val), RUBY_SPI_CONTROL);
	writel(0, RUBY_SPI_READ_SCUR);
	writel(spi_ctrl_val, RUBY_SPI_CONTROL);
	return (SWAP32(readl(RUBY_SPI_COMMIT))&RUBY_SPI_READ_STATUS_MASK);
}

/******************************************************************************
* Function:spi_read_dpb_reg
* Purpose: read dynamic protect block mode
* Returns:status of the dynamic protect mode
* Note:
******************************************************************************/
uint32_t spi_read_dpb_reg(uint32_t addr)
{
	uint32_t spi_ctrl_val;
	uint32_t log_addr, sector_addr;

	spi_ctrl_val = readl(RUBY_SPI_CONTROL);
	log_addr = addr & ADDRESS_MASK;
	sector_addr = log_addr & SECTOR_MASK;
	writel(RUBY_SPI_READ_DPB_MASK(spi_ctrl_val), RUBY_SPI_CONTROL);
	writel(sector_addr, RUBY_SPI_READ_DPB);
	writel(spi_ctrl_val, RUBY_SPI_CONTROL);
	return (SWAP32(readl(RUBY_SPI_COMMIT))&RUBY_SPI_READ_STATUS_MASK);
}

/******************************************************************************
* Function:spi_gang_block_lock
* Purpose:protect whole chipset
* Returns: 0 or < 0
* Note:
*******************************************************************************/
int spi_gang_block_lock(void)
{
	uint32_t spi_ctrl_val;

	spi_unlock();
	spi_ctrl_val = readl(RUBY_SPI_CONTROL);
	writel(RUBY_SPI_GBLOCK_LOCK_MASK(spi_ctrl_val), RUBY_SPI_CONTROL);
	writel(0, RUBY_SPI_GBLOCK_LOCK);
	writel(spi_ctrl_val, RUBY_SPI_CONTROL);
	if ((spi_api_flash_status()) == -1){
		printk(KERN_ERR "Time Out On Write Operation\n");
		spi_lock();
		return ETIME;
	}
	spi_lock();
	return 0;
}

/******************************************************************************
* Function:spi_gang_block_unlock
* Purpose:unprotect whole chipset
* Returns:0 or < 0
* Note:
********************************************************************************/
int spi_gang_block_unlock(void)
{
	uint32_t spi_ctrl_val;

	spi_unlock();
	spi_ctrl_val = readl(RUBY_SPI_CONTROL);
	writel(RUBY_SPI_GBLOCK_UNLOCK_MASK(spi_ctrl_val), RUBY_SPI_CONTROL);
	writel(0, RUBY_SPI_GBLOCK_UNLOCK);
	writel(spi_ctrl_val, RUBY_SPI_CONTROL);
	if ((spi_api_flash_status()) == -1){
		printk(KERN_ERR "Time Out On Write Operation\n");
		spi_lock();
		return ETIME;
	}
	spi_lock();
	return 0;
}

/******************************************************************************
* Function:spi_write_prot_select
* Purpose:Check if the device support individual unprotect mode
* Returns:0 or < 0
* Note:
*********************************************************************************/
int spi_write_prot_select(struct flash_info *device)
{
	uint32_t spi_ctrl_val;

	switch(device->single_unprotect_mode){
	case MACRONIX:
		spi_unlock();
		if (spi_read_scur() & SPI_SCUR_WPSEL) {
			spi_lock();
			printk(KERN_INFO "Individual Unprotedted Mode Is Enabled \n");
			return 0;
		}
		spi_ctrl_val = readl(RUBY_SPI_CONTROL);
		writel(RUBY_SPI_WRITE_PRO_SEL_MASK(spi_ctrl_val), RUBY_SPI_CONTROL);
		writel(0, RUBY_SPI_WRITE_PRO_SEL);
		writel(spi_ctrl_val, RUBY_SPI_CONTROL);
		if ((spi_api_flash_status()) == -1){
			printk(KERN_ERR "Time Out On Write Operation\n");
			spi_lock();
			return ETIME;
		}
		spi_lock();
		if (spi_read_scur() & SPI_SCUR_WPSEL) {
			printk(KERN_INFO "Individual Unprotected Mode Is Enabled\n");
			return 0;
		} else {
			printk(KERN_INFO "Individual Unprotected Mode Is Disabled \n");
			return EOPNOTSUPP;
		}
		break;
	case WINBOND:
                        spi_unlock();
                        spi_ctrl_val = readl(RUBY_SPI_CONTROL);
                        writel(RUBY_SPI_WRITE_WPS_SEL_MASK(spi_ctrl_val), RUBY_SPI_CONTROL);
                        writel(SPI_WPS_ENABLE, RUBY_SPI_WRITE_REG3);
                        writel(spi_ctrl_val, RUBY_SPI_CONTROL);
                        spi_lock();
			return 0;
	default:
		return EOPNOTSUPP;
	}
}

/******************************************************************************
* Function:spi_clear_dpb_reg
* Purpose:unproctect individual sector
* Returns: 0 or < 0
* Note:
*********************************************************************************/
int spi_clear_dpb_reg(uint32_t addr)
{
	uint32_t spi_ctrl_val;
	uint32_t log_addr, sector_addr;

	spi_unlock();
	spi_ctrl_val = readl(RUBY_SPI_CONTROL);
	log_addr = addr & ADDRESS_MASK;
	sector_addr = log_addr & SECTOR_MASK;
	writel(RUBY_SPI_WRITE_DPB_MASK(spi_ctrl_val), RUBY_SPI_CONTROL);
	writel(sector_addr, RUBY_SPI_WRITE_DPB);
	writel(spi_ctrl_val, RUBY_SPI_CONTROL);
	if ((spi_api_flash_status()) == -1){
		printk(KERN_ERR "Time Out On Write Operation\n");
		spi_lock();
		return ETIME;
	}
	spi_lock();
	return 0;
}

/******************************************************************************
* Function:spi_clear_ibup_reg
* Purpose:unproctect individual sector
* Returns:0 or < 0
* Note:
**********************************************************************************/
int spi_clear_ibup_reg(uint32_t addr)
{
        uint32_t spi_ctrl_val;
        uint32_t log_addr, sector_addr;

        spi_unlock();
        spi_ctrl_val = readl(RUBY_SPI_CONTROL);
        log_addr = addr & ADDRESS_MASK;
        sector_addr = log_addr & SECTOR_MASK;
        writel(RUBY_SPI_WRITE_IBUP_MASK(spi_ctrl_val), RUBY_SPI_CONTROL);
        writel(sector_addr, RUBY_SPI_WRITE_IBUP);
        writel(spi_ctrl_val, RUBY_SPI_CONTROL);
        if ((spi_api_flash_status()) == -1){
                printk(KERN_ERR "Time Out On Write Operation\n");
                spi_lock();
                return ETIME;
        }

        spi_lock();
        return 0;
}

/******************************************************************************
* Function:spi_lock
* Purpose:issue a lock after write is complete
* Returns: 0
* Note:
*********************************************************************************/
int spi_lock(void)
{
	writel(0, RUBY_SPI_WRITE_DIS);
	return 0;
}

/******************************************************************************
* Function:spi_unlock
* Purpose:issue a unlock before any write to flash
* Returns: 0
* Note:
*********************************************************************************/
int spi_unlock(void)
{
	writel(0, RUBY_SPI_WRITE_EN);
	return 0;
}

/******************************************************************************
Function:spi_read_id
Purpose:Reads spi device ID
Returns: ID
Note:
*****************************************************************************/
uint32_t spi_read_id(void)
{
	return SWAP32(readl(RUBY_SPI_READ_ID))&(RUBY_SPI_READ_ID_MASK);
}

/******************************************************************************
Function:spi_read_status
Purpose:Reads spi status reg
Returns:Flash status
Note:
*****************************************************************************/
uint32_t spi_read_status(void)
{
	return SWAP32(readl(RUBY_SPI_READ_STATUS))&(RUBY_SPI_READ_STATUS_MASK);
}

/******************************************************************************
Function:spi_write_status
Purpose:write spi status reg
Returns:0
Note:
*****************************************************************************/
int spi_write_status(uint32_t status)
{
	writel(status, RUBY_SPI_WRITE_STATUS);
	return 0;
}

/******************************************************************************
* Function:spi_unprotect_all
* Purpose:unprotect the whole flash device
* Returns: 0 or < 0
* Note:
******************************************************************************/
int spi_unprotect_all(const struct flash_info *device)
{
	int ret = EOPNOTSUPP;

	switch(device->single_unprotect_mode){
		case MACRONIX:
		case WINBOND:
			ret = spi_gang_block_unlock();
			break;
		default:
			ret = 0;
	}

	return ret;
}

/******************************************************************************
* Function:spi_unprotect_sector
* Purpose:unprotect the a individual sector
* Returns:0 or < 0
* Note:
******************************************************************************/
int spi_unprotect_sector(const struct flash_info *device, uint32_t flash_addr)
{
	int ret = EOPNOTSUPP;

	switch(device->single_unprotect_mode){
	case MACRONIX:
		ret = spi_clear_dpb_reg(flash_addr) ;
		break;
	case WINBOND:
                ret = spi_clear_ibup_reg(flash_addr) ;
                break;

	default:
		ret = 0;
	}

	return ret;
}

/******************************************************************************
* Function:spi_protect_all
* Purpose:protect whole chipset device
* Returns: 0 or < 0
* Note:
******************************************************************************/
int spi_protect_all(const struct flash_info *device)
{
	int ret = EOPNOTSUPP;

	switch(device->single_unprotect_mode){
	case MACRONIX:
	case WINBOND:
		if ((spi_api_flash_status()) == -1){
			printk(KERN_ERR "Time Out On Write Operation\n");
			spi_lock();
			return ETIME;
		}

		ret = spi_gang_block_lock();
		break;
	default:
		ret = 0;
	}

	return ret;
}

/******************************************************************************
* Function:spi_read_wps
* Purpose:Read security register
* Returns: SPI Status Bits
* Note:
******************************************************************************/
uint32_t spi_read_wps(void)
{
        uint32_t spi_ctrl_val;

        spi_ctrl_val = readl(RUBY_SPI_CONTROL);
        writel(RUBY_SPI_READ_SCUR_MASK(spi_ctrl_val), RUBY_SPI_CONTROL);
        writel(0, RUBY_SPI_READ_REG3);
        writel(spi_ctrl_val, RUBY_SPI_CONTROL);
        return (SWAP32(readl(RUBY_SPI_COMMIT))&RUBY_SPI_READ_STATUS_MASK);
}

int spi_device_erase(struct flash_info *device, u32 flash_addr)
{
        int ret = 0;
        int i;
	int n_of_64k;

        switch(device->single_unprotect_mode) {
        case MACRONIX:
        case WINBOND:

	/* check if the address is below the first lower 64K or upper end 64K,
	 * Flash specs for proctect mode is lower 64K you can protect/unprotect
	 * 4K chuncks and anywhere is 64K
	 * to make our life easeir we will use default 64K size, but will build some
	 * intelleigent to erase the lower 64K or upper 64K of the flash
	 */
		n_of_64k = flash_addr / SPI_SECTOR_64K;
		if ((n_of_64k == 0) || (n_of_64k == device->n_sectors - 1)) {
			for (i = 0; i < SPI_SECTOR_INDEX; i++) {
                                ret = spi_unprotect_sector(device, flash_addr) ;
                                if (ret){
                                        printk(KERN_INFO "ERROR: Failed to unprotect Sector %x \n", flash_addr);
                                        return -1;
                                }
                                spi_flash_write_enable();
				if ( device->sector_size * device->n_sectors > RUBY_SPI_BOUNDARY_4B ){
					writel(SPI_MEM_ADDR_4B(flash_addr), RUBY_SPI_SECTOR_ERASE_20_4B);
				} else {
					writel(SPI_MEM_ADDR(flash_addr), RUBY_SPI_SECTOR_ERASE_20);
				}
				ret = spi_flash_wait_ready(device);
				if (ret || device->sector_size == SPI_SECTOR_4K)
					break;
                                flash_addr += SPI_SECTOR_4K;
                        }

                } else {
                        ret = spi_unprotect_sector(device, flash_addr) ;
                        if (ret){
                                printk(KERN_INFO "ERROR: Failed to unprotect Sector %x \n", flash_addr);
                                return -1;
                        }
                        spi_flash_write_enable();
			if ( device->sector_size * device->n_sectors > RUBY_SPI_BOUNDARY_4B ){
				writel(SPI_MEM_ADDR_4B(flash_addr), RUBY_SPI_SECTOR_ERASE_D8_4B);
			} else {
				writel(SPI_MEM_ADDR(flash_addr), RUBY_SPI_SECTOR_ERASE_D8);
			}
                }
                break;

        default:
                spi_flash_write_enable();
		if ( device->sector_size * device->n_sectors > RUBY_SPI_BOUNDARY_4B ){
			if (device->flags & SECTOR_ERASE_OP20) {
				writel(SPI_MEM_ADDR_4B(flash_addr), RUBY_SPI_SECTOR_ERASE_20_4B);
			} else {
				writel(SPI_MEM_ADDR_4B(flash_addr), RUBY_SPI_SECTOR_ERASE_D8_4B);
			}
		} else {
			if (device->flags & SECTOR_ERASE_OP20) {
				writel(SPI_MEM_ADDR(flash_addr), RUBY_SPI_SECTOR_ERASE_20);
			} else {
				writel(SPI_MEM_ADDR(flash_addr), RUBY_SPI_SECTOR_ERASE_D8);
			}
		}
        }

        return ret;
}


