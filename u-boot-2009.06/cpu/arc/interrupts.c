/*
 *  Copyright Codito Technologies (www.codito.com)
 *
 *  cpu/arc/interrupts.c
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors : Sandeep Patil (sandeep.patil@codito.com)
 *		Pradeep Sawlani (pradeep.sawlani@codito.com)
 */
#include <asm/arcregs.h>
#include <common.h>

struct  irq_action {
        //interrupt_handler_t *handler;
        void *handler;
        void *arg;
        int count;
};

static struct irq_action irq_vecs[64];

//void disable_interrupts(void)
int disable_interrupts(void)
{
	unsigned int status;
	status = read_new_aux_reg(ARC_REG_STATUS32);
	status &= STATUS_DISABLE_INTERRUPTS;
/*	write_new_aux_reg(ARC_REG_STATUS32,status); */
	__asm__ __volatile__ (
		"FLAG %0"
		:
		:"r"(status)
		);

	return (status & (STATUS_E1_MASK | STATUS_E2_MASK));
}

void enable_interrupts(void)
{
	unsigned int status ;
	status = read_new_aux_reg(ARC_REG_STATUS32);
	status |= STATUS_E1_MASK;
	status |= STATUS_E2_MASK;
//	write_new_aux_reg(ARC_REG_STATUS32,status);
	__asm__ __volatile__ (
		"FLAG %0"
		:
		:"r"(status)
		);
}

//void irq_install_handler (int vec, interrupt_handler_t *handler, void *arg)
void irq_install_handler(int vec, interrupt_handler_t *handler, void *arg)
{
        struct irq_action *irqa = irq_vecs;
        int   i = vec;
        int flag;

        //if (irqa[i].handler != NULL) {
        if (irqa[i].handler != 0) {
                printf ("Interrupt vector %d: handler 0x%x "
                        "replacing 0x%x\n",
                        vec, (uint)handler, (uint)irqa[i].handler);
        }

        flag = disable_interrupts ();
        irqa[i].handler = handler;
        irqa[i].arg = arg;
        if (flag )
                enable_interrupts ();
}
