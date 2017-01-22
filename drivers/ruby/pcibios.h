/**
 * Copyright (c) 2009-2011 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/
#ifndef __RUBY_PCIBIOS_H__
#define __RUBY_PCIBIOS_H__

typedef union ruby_csr
{
    struct reg
    {
#if defined(__LITTLE_ENDIAN)
        int chipid          :8;
        int pci_dlink       :1;
        int pci_phylink     :1;
        int pci_phylink_isr :1;
        int pci_rst_req     :1;
        int rsvd_1          :2;
        int pci_clk_rem     :1;
        int pci_fatl_err    :1;
        int rsvd_2          :16;
#else
        int rsvd_2          :16;
        int pci_dlink       :1;
        int pci_phylink     :1;
        int pci_phylink_isr :1;
        int pci_rst_req     :1;
        int rsvd_1          :2;
        int pci_clk_rem     :1;
        int pci_fatl_err    :1;
        int chipid          :8;
#endif
    }r;

    uint32_t data;
}ruby_csr_t;

#define RC_MODE	1
#define EP_MODE	2

extern int pci_mode;

extern void
ruby_pci_create_sysfs (void);

#define DEBUG(...) do{}while(0);

#endif	/* __RUBY_PCIBIOS_H__ */
