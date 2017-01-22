#ifndef __RTL8367B_SMI_H__
#define __RTL8367B_SMI_H__

#include "rtl8367b_init.h"

uint32_t smi_reset(uint32_t port, uint32_t pinRST);
uint32_t smi_init(uint32_t port, uint32_t pinSCK, uint32_t pinSDA);
uint32_t smi_read(uint32_t mAddrs, uint32_t *rData);
uint32_t smi_write(uint32_t mAddrs, uint32_t rData);
void smi_mdio_base_set(unsigned long addr);

#endif /* __RTL8367B_SMI_H__ */
