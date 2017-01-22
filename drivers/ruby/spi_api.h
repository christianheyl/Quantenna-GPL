#ifndef __SPI_H__
#define __SPI_H__
///////////////////////////////////////////////////////////////////////////////
//             Includes
///////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////
//              Types
///////////////////////////////////////////////////////////////////////////////

/******************************************************************************
* Function:spi_protect_mode
* Purpose:Initialize spi device - read device ID, if this matches return
* pointer to device information structure
* Returns:	0 or < 0 
* Note:
*  *****************************************************************************/
int spi_protect_mode(struct flash_info *device);


/******************************************************************************
* Function:spi_read_id
* Purpose:Reads spi device ID
* Returns: ID
* Note:
*  *****************************************************************************/
uint32_t spi_read_id(void);

/******************************************************************************
* Function:spi_read_status
* Purpose:Reads spi status reg
* Returns: Status
* Note:
*  *****************************************************************************/
uint32_t spi_read_status(void);

/******************************************************************************
* Function:spi_write_status
* Purpose:write spi status reg
* Returns: NONE
* Note:
*  *****************************************************************************/
int spi_write_status(uint32_t status);

/******************************************************************************
* Function:spi_lock
* Purpose:locks spi device
* Returns: NONE
* Note:
*  *****************************************************************************/
int spi_lock(void);

/******************************************************************************
* Function:spi_unlock
* Purpose:unlocks spi device
* Returns: 0 or < 1
* Note:
*  *****************************************************************************/
int spi_unlock(void);

/******************************************************************************
* Function:spi_write_prot_select
* Purpose:Select write protection
* Returns: 0 or < 0
* Note:
*  *****************************************************************************/
int spi_write_prot_select(struct flash_info *device);

/******************************************************************************
* Function:spi_read_scur
* Purpose:Read security register
* Returns: status
* Note:
*  *****************************************************************************/
uint32_t spi_read_scur(void);

/******************************************************************************
* Function:spi_write_scur
* Purpose:Write security register to set lockdown bit
* Returns: 0 or < 0
* Note:
*  *****************************************************************************/
uint32_t spi_write_scur(void);


/******************************************************************************
* Function:spi_gang_block_lock
* Purpose:Lock all DPB
* Returns: 0 or < 0
* Note:
*  *****************************************************************************/
int spi_gang_block_lock(void);

/******************************************************************************
* Function:spi_gang_block_unlock
* Purpose:Lock all DPB
* Returns: 0 or < 0
* Note:
*  *****************************************************************************/
int spi_gang_block_unlock(void);

/******************************************************************************
* Function:spi_clear_dpb_reg
* Purpose:unproctect individual sector
* Returns: 0 or < 0
* Note:
***********************************************************************************/
int spi_clear_dpb_reg(uint32_t addr);

/******************************************************************************
* Function:spi_read_dpb_reg
* Purpose:read individual sector
* Returns: o or < 0 
* Note:
**********************************************************************************/
uint32_t spi_read_dpb_reg(uint32_t addr);

/******************************************************************************
* Function:spi_api_flash_status
* Purpose: read status of the Flash
* Returns: status
* Note:
* ***********************************************************************************/
int spi_api_flash_status(void);

/******************************************************************************
* Function:spi_unprotect_all
* Purpose:unprotect the whole flash device
* Returns:0 or < 0
* Note:
*******************************************************************************/
int spi_unprotect_all(const struct flash_info *device);

/******************************************************************************
* Function:spi_unprotect_sector
* Purpose:unprotect the a individual sector
* Returns:0 or < 0
* Note:
*******************************************************************************/
int spi_unprotect_sector(const struct flash_info *device, uint32_t address);

/******************************************************************************
* Function:spi_protect_all
* Purpose:protect whole chipset device
* Returns: 0 or < 0
* Note:
******************************************************************************/
int spi_protect_all(const struct flash_info *device);

/******************************************************************************
* Function:spi_read_wps
* Purpose:Read register 3
* Returns: SPI Status Bits
* Note:
* ******************************************************************************/
uint32_t spi_read_wps(void);

/******************************************************************************
 * * Function:spi_write_wps
 * * Purpose:write security register
 * * Returns: SPI Status Bits
 * * Note:
 * ******************************************************************************/
uint32_t spi_write_wps(void);

void spi_device_resize(struct flash_info *device);

int spi_device_erase(struct flash_info *device, u32 flash_addr);

void spi_flash_write_enable(void);

int qtn_get_spi_protect_config(void);

int spi_flash_wait_ready(const struct flash_info *info);

#endif // __SPI_H__

