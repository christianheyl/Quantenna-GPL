/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_IRQFLAGS_H
#define __ASM_ARC_IRQFLAGS_H

#ifdef __KERNEL__

#include <asm/arcregs.h>

/******************************************************************
 * IRQ Control Macros
 ******************************************************************/

/*
 * Save IRQ state and disable IRQs
 */
#define local_irq_save(x) { x = _local_irq_save(); }

static inline long _local_irq_save(void) {
    unsigned long temp, flags;

    __asm__ __volatile__ (
        "lr  %1, [status32]\n\t"
        "bic %0, %1, %2\n\t"    // a BIC b = a AND ~b, but 4 byte insn instead of 8
        "and.f 0, %1, %2  \n\t"
        "flag.nz %0\n\t"
        :"=r" (temp), "=r" (flags)
        :"n" ((STATUS_E1_MASK | STATUS_E2_MASK))
        :"cc", "memory"
    );

    return flags;
}

/*
 * restore saved IRQ state
 */
static inline void local_irq_restore(unsigned long flags) {

    __asm__ __volatile__ (
        "flag %0\n\t"
        :
        :"r" (flags)
        :"memory"
    );
}

/*
 * restore saved IRQ state and immediately sleep
 */
static inline void local_irq_restore_and_sleep(unsigned long flags) {

	/*
	 * From ARC docs:
	 * Note that interrupts remain disabled until FLAG has completed its update
	 * of the flag registers in stage 4 of the ARCompact based pipeline. Hence,
	 * if SLEEP follows into the pipeline immediately behind FLAG, then no
	 * interrupt can be taken between the FLAG and SLEEP.
	 */
	__asm__ __volatile__ (
		"flag %0\n\t"
		"sleep\n\t"
		:
		:"r" (flags)
		:"memory"
	);
}

/*
 * Conditionally Enable IRQs
 */
extern void local_irq_enable(void);

/*
 * Unconditionally Disable IRQs
 */
static inline void local_irq_disable(void) {
    unsigned long temp;

    __asm__ __volatile__ (
        "lr  %0, [status32]\n\t"
        "and %0, %0, %1\n\t"  // {AND a,a,u7} is 4 byte so not conv to BIC
        "flag %0\n\t"
        :"=&r" (temp)
        :"n" (~(STATUS_E1_MASK | STATUS_E2_MASK))
        :"memory"
    );
}

/*
 * save IRQ state
 */
#define local_save_flags(x) { x = _local_save_flags(); }
static inline long _local_save_flags(void) {
    unsigned long temp;

    __asm__ __volatile__ (
        "lr  %0, [status32]\n\t"
        :"=&r" (temp)
    );

    return temp;
}

/*
 * mask/unmask an interrupt (@x = IRQ bitmap)
 * e.g. to Disable IRQ 3 and 4, pass 0x18
 *
 * mask = disable IRQ = CLEAR bit in AUX_I_ENABLE
 * unmask = enable IRQ = SET bit in AUX_I_ENABLE
 */

#define mask_interrupt(x)  __asm__ __volatile__ (   \
    "lr r20, [auxienable] \n\t"                     \
    "and    r20, r20, %0 \n\t"                      \
    "sr     r20,[auxienable] \n\t"                  \
    :                                               \
    :"r" (~(x))                                     \
    :"r20", "memory")

#define unmask_interrupt(x)  __asm__ __volatile__ ( \
    "lr r20, [auxienable] \n\t"                     \
    "or     r20, r20, %0 \n\t"                      \
    "sr     r20, [auxienable] \n\t"                 \
    :                                               \
    :"r" (x)                                        \
    :"r20", "memory")

/*
 * Query IRQ state
 */
static inline int irqs_disabled_flags(unsigned long flags)
{
    return (!(flags & (STATUS_E1_MASK
#ifdef CONFIG_ARCH_ARC_LV2_INTR
                        | STATUS_E2_MASK
#endif
            )));
}

static inline int irqs_disabled(void)
{
    unsigned long flags;
    local_save_flags(flags);
    return (!(flags & (STATUS_E1_MASK
#ifdef CONFIG_ARCH_ARC_LV2_INTR
                        | STATUS_E2_MASK
#endif
            )));
}

#endif

#endif
