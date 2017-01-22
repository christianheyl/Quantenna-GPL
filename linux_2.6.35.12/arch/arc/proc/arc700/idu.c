/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Rajeshwar Ranga: Interrupt Distribution Unit API's
 */

#include <linux/smp.h>
#include <asm/idu.h>
#include <asm/arcregs.h>

void idu_irq_set_mode(uint8_t irq, uint8_t dest_mode, uint8_t trig_mode)
{
    uint32_t par = IDU_IRQ_MODE_PARAM(dest_mode, trig_mode);

    IDU_SET_PARAM(par);
    IDU_SET_COMMAND(irq,IDU_IRQ_WMODE);
}

idu_irq_config_t idu_irq_get_mode(uint8_t irq)
{
    uint32_t val;
    idu_irq_config_t config;

    IDU_SET_COMMAND(irq, IDU_IRQ_RMODE);
    val = IDU_GET_PARAM();

    config.irq = irq;
    config.dest_mode = val & 0xFF;
    config.trig_mode = (val >> 15) & 0x1;

    return config;
}

void idu_irq_set_tgtcpu(uint8_t irq, uint32_t mask)
{
    IDU_SET_PARAM(mask);
    IDU_SET_COMMAND(irq,IDU_IRQ_WBITMASK);
}

uint32_t  idu_irq_get_tgtcpu(uint8_t irq)
{
    IDU_SET_COMMAND(irq,IDU_IRQ_RBITMASK);
    return IDU_GET_PARAM();
}


idu_irq_status_t idu_irq_get_status (uint8_t irq)
{
    idu_irq_status_t status;
    uint32_t val;

    IDU_SET_COMMAND(irq, IDU_IRQ_STATUS);
    val = IDU_GET_PARAM();

    status.irq = irq;
    status.enabled = val & 0x01;
    status.status = val & 0x02;
    status.ack = val & 0x04;
    status.pend = val & 0x08;
    status.next_rr = (val & 0x1F00) >> 8;

    return status;
}


bool idu_irq_get_ack (uint8_t irq)
{
    uint32_t val;

    IDU_SET_COMMAND(irq, IDU_IRQ_ACK);
    val = IDU_GET_PARAM();

    return (val & (1 << irq));
}


bool idu_irq_get_pend (uint8_t irq)
{
    uint32_t val;

    IDU_SET_COMMAND(irq, IDU_IRQ_PEND);
    val = IDU_GET_PARAM();

    return (val & (1 << irq));
}

