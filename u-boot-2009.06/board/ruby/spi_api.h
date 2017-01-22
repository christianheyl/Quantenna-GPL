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
* Purpose:Check device ID, and probe if the device support protect mode
* pointer to device information structure
* Returns:0 or < 0 if error
* Note:
*  *****************************************************************************/
int spi_protect_mode(struct flash_info *device);

/******************************************************************************
* Function:spi_read_id
* Purpose:Reads spi device ID
* Returns: ID of a device
* Note:
*  *****************************************************************************/
uint32_t spi_read_id(void);

/******************************************************************************
* Function:spi_read_status
* Purpose:Reads spi status reg
* Returns: status
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
* Returns:0 or < 0
* Note:
*  *****************************************************************************/
int spi_lock(void);

/******************************************************************************
* Function:spi_unlock
* Purpose:unlocks spi device
* Returns: 0 or < 0
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
* Returns:status of the secutiry register
* Note:
*  *****************************************************************************/
uint32_t spi_read_scur(void);

/******************************************************************************
* Function:spi_gang_block_lock
* Purpose:Lock all DPB
* Returns:0 < 0
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
* Returns:0 or < 0
* Note:
***********************************************************************************/
int spi_clear_dpb_reg(uint32_t addr);

/******************************************************************************
* Function:spi_read_dpb_reg
* Purpose:read individual sector
* Returns: status of the dynamic register
* Note:
**********************************************************************************/
uint32_t spi_read_dpb_reg(uint32_t addr);

/******************************************************************************
* Function:spi_api_flash_status
* Purpose: read statu of te Flash
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
int spi_unprotect_all(struct flash_info *device);

/******************************************************************************
* Function:spi_unprotect_sector
* Purpose:unprotect the a individual sector
* Returns:0 or < 0
* Note:
*******************************************************************************/
int spi_unprotect_sector(struct flash_info *device, uint32_t address);

/******************************************************************************
* Function:spi_protect_all
* Purpose:protect whole chipset device
* Returns:0 or < 0
* Note:
******************************************************************************/
int spi_protect_all(struct flash_info *device);

/******************************************************************************
* Function:spi_flash_wait_ready
* Purpose:delay number of seconds
* Returns:0 or < 0
* Note:
*******************************************************************************/
int spi_flash_wait_ready(int sec);

/******************************************************************************
* Function:spi_read_wps
* Purpose:Read register 3
* Returns: SPI Status Bits
* Note:
*******************************************************************************/
uint32_t spi_read_wps(void);

/******************************************************************************
* Function:spi_write_wps
* Purpose:Read register 3
* Returns: 0
* Note:
******************************************************************************/
uint32_t spi_write_wps(void);

int spi_device_erase(struct flash_info *device, u32 flash_addr);

void spi_flash_write_enable(void);

void spi_protect_mode_on(void);

void spi_protect_mode_off(void);

#endif // __SPI_H__
