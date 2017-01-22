/*
 * Copyright (c) 2012 Quantenna Communications, Inc.
 * All rights reserved.
 *
 */

#ifndef _ASMARC_GPIO_H
#define _ASMARC_GPIO_H
#include <asm/board/gpio.h>
#include <asm-generic/gpio.h>

#ifdef CONFIG_GPIOLIB

/*
 * Just call gpiolib.
 */
static inline int gpio_get_value(unsigned int gpio)
{
        return __gpio_get_value(gpio);
}

static inline void gpio_set_value(unsigned int gpio, int value)
{
        __gpio_set_value(gpio, value);
}

static inline int gpio_cansleep(unsigned int gpio)
{
        return __gpio_cansleep(gpio);
}

static inline int gpio_to_irq(unsigned int gpio)
{
        return gpio;
}

static inline int irq_to_gpio(unsigned int irq)
{
        return irq;
}

#endif /* CONFIG_GPIOLIB */

#endif
