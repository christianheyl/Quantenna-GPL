/*
 * (C) Copyright 2010 Quantenna Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <linux/module.h>
#include <linux/io.h>
#include <linux/spinlock.h>

#include <asm/hardware.h>

#include <asm/board/platform.h>
#include <asm/board/gpio.h>
#include <asm-generic/gpio.h>

#define GPIO_MAX		ARCH_NR_GPIOS

#define to_arc_gpio_chip(d) container_of(d, struct arc_7XX_gpio, chip)

struct arc_gpio_controller {
	uint32_t	in_dat;
	uint32_t	out_msk;
	uint32_t	out_dat;
	uint32_t	mode1;
	uint32_t	mode2;
	uint32_t	afsel;
	uint32_t	def;
};

struct arc_7XX_gpio {
	struct gpio_chip		chip;
	struct arc_gpio_controller	*__iomem regs;
};


static DEFINE_SPINLOCK(gpio_spinlock);
static struct arc_7XX_gpio arc_gpio;

inline static void gpio_set_mode(void *reg, uint32_t offset, uint32_t cfg)
{
	uint32_t tmp = readl(IO_ADDRESS(reg));
	tmp &= ~(0x3 << offset);
	tmp |= (cfg << offset);
	writel(tmp, IO_ADDRESS(reg));
}

inline static void gpio_config_pin(struct arc_gpio_controller *g, uint32_t pin, uint32_t cfg)
{
	if(pin < RUBY_GPIO_MODE1_MAX) {
		gpio_set_mode(&g->mode1, pin * 2, cfg);
	} else {
		gpio_set_mode(&g->mode2, (pin - RUBY_GPIO_MODE1_MAX) * 2, cfg);
	}
}

static void _gpio_config(struct arc_gpio_controller *g, uint32_t pin, uint32_t cfg)
{
	unsigned long flags;

	spin_lock_irqsave(&gpio_spinlock, flags);
	if(cfg >= RUBY_GPIO_ALT_INPUT) {
		writel_or(RUBY_BIT(pin), IO_ADDRESS(&g->afsel));
	} else {
		writel_and(~RUBY_BIT(pin), IO_ADDRESS(&g->afsel));
	}
	gpio_config_pin(g, pin, cfg & 0x3);
	spin_unlock_irqrestore(&gpio_spinlock, flags);
}


static void gpio_output(struct arc_gpio_controller *g, uint32_t pin, uint32_t state)
{
	unsigned long flags;

	spin_lock_irqsave(&gpio_spinlock, flags);
	writel(RUBY_BIT(pin), IO_ADDRESS(&g->out_msk));
	writel(state << pin, IO_ADDRESS(&g->out_dat));
	writel(0, IO_ADDRESS(&g->out_msk));
	spin_unlock_irqrestore(&gpio_spinlock, flags);
}


static uint32_t gpio_input(struct arc_gpio_controller *g, uint32_t pin)
{
	/* As long function is simple 32-bit read, no locking is needed. */
	return ((readl(IO_ADDRESS(&g->in_dat)) >> pin) & 0x1);
}

static int arc_gpio_dir_in(struct gpio_chip *chip, unsigned offset)
{
	struct arc_7XX_gpio *arc_chip = to_arc_gpio_chip(chip);
	struct arc_gpio_controller *__iomem g = arc_chip->regs;

	_gpio_config(g, offset, GPIO_MODE_INPUT);

	return 0;
}

static int arc_gpio_dir_out(struct gpio_chip *chip, unsigned offset, int value)
{
	struct arc_7XX_gpio *arc_chip = to_arc_gpio_chip(chip);
	struct arc_gpio_controller *__iomem g = arc_chip->regs;
	gpio_output(g, offset, value);
	_gpio_config(g, offset, GPIO_MODE_OUTPUT);

	return 0;
}

static int arc_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct arc_7XX_gpio *arc_chip = to_arc_gpio_chip(chip);
	struct arc_gpio_controller *__iomem g = arc_chip->regs;

	return (int)gpio_input(g, offset);
}

static void arc_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct arc_7XX_gpio *arc_chip = to_arc_gpio_chip(chip);
	struct arc_gpio_controller *__iomem g = arc_chip->regs;

	gpio_output(g, offset, value);
}

static int __init arc_gpio_init(void)
{
	memset(&arc_gpio, 0, sizeof(arc_gpio));
	arc_gpio.chip.label = "arcgpio";
	arc_gpio.chip.direction_input = arc_gpio_dir_in;
	arc_gpio.chip.direction_output = arc_gpio_dir_out;
	arc_gpio.chip.set = arc_gpio_set;
	arc_gpio.chip.get = arc_gpio_get;
	arc_gpio.chip.base = 0;
	arc_gpio.chip.ngpio = GPIO_MAX;

	arc_gpio.regs = (struct arc_gpio_controller *) RUBY_GPIO_REGS_ADDR;

	gpiochip_add(&arc_gpio.chip);

	return 0;
}

void gpio_config(uint32_t pin, uint32_t cfg)
{
	struct arc_gpio_controller *__iomem g =
			(struct arc_gpio_controller *) RUBY_GPIO_REGS_ADDR;
	_gpio_config(g, pin, cfg);

}
EXPORT_SYMBOL(gpio_config);

void gpio_wowlan_output(uint32_t pin, uint32_t value)
{
#ifndef TOPAZ_AMBER_IP
	struct arc_gpio_controller *__iomem g =
			(struct arc_gpio_controller *) RUBY_GPIO_REGS_ADDR;
	gpio_output(g, pin, value);
#else
	/*
	 * In Amber WOWLAN is handled by WIFI2SOC interrupt.
	 */
#endif
}
EXPORT_SYMBOL(gpio_wowlan_output);

void gpio_uart0_config(void)
{
#ifndef TOPAZ_AMBER_IP
	gpio_config(RUBY_GPIO_UART0_SO, RUBY_GPIO_ALT_OUTPUT);
	gpio_config(RUBY_GPIO_UART0_SI, RUBY_GPIO_ALT_INPUT);
#else
	/*
	 * In Amber GPIO pins are not shared. No need to set up alternate function.
	 */
#endif
}

void gpio_uart1_config(void)
{
#ifndef TOPAZ_AMBER_IP
	gpio_config(RUBY_GPIO_UART1_SO, RUBY_GPIO_ALT_OUTPUT);
	gpio_config(RUBY_GPIO_UART1_SI, RUBY_GPIO_ALT_INPUT);
#else
	/*
	 * In Amber GPIO pins are not shared. No need to set up alternate function.
	 */
#endif
}

void gpio_spi_flash_config(void)
{
#ifndef TOPAZ_AMBER_IP
	gpio_config(RUBY_GPIO_SPI_MISO, RUBY_GPIO_ALT_OUTPUT);
	gpio_config(RUBY_GPIO_SPI_MOSI, RUBY_GPIO_ALT_INPUT);
	gpio_config(RUBY_GPIO_SPI_SCK, RUBY_GPIO_ALT_OUTPUT);
	gpio_config(RUBY_GPIO_SPI_nCS, RUBY_GPIO_ALT_OUTPUT);
#else
	/*
	 * In Amber GPIO pins are not shared. No need to set up alternate function.
	 */
#endif
}
EXPORT_SYMBOL(gpio_spi_flash_config);

void gpio_lna_toggle_config(void)
{
#ifndef TOPAZ_AMBER_IP
	gpio_config(RUBY_GPIO_LNA_TOGGLE, RUBY_GPIO_ALT_OUTPUT);
#else
	/*
	 * In Amber GPIO pins are not shared. No need to set up alternate function.
	 */
#endif
}
EXPORT_SYMBOL(gpio_lna_toggle_config);

inline static uint32_t _gpio_pin_pwm_reg(uint32_t pin)
{
	switch(pin) {
#ifndef TOPAZ_AMBER_IP
		case RUBY_GPIO_PIN1:
			return RUBY_GPIO1_PWM0;
		case RUBY_GPIO_PIN3:
			return RUBY_GPIO3_PWM1;
		case RUBY_GPIO_PIN9:
			return RUBY_GPIO9_PWM2;
		case RUBY_GPIO_PIN12:
			return RUBY_GPIO12_PWM3;
		case RUBY_GPIO_PIN13:
			return RUBY_GPIO13_PWM4;
		case RUBY_GPIO_PIN15:
			return RUBY_GPIO15_PWM5;
		case RUBY_GPIO_PIN16:
			return RUBY_GPIO16_PWM6;
#else
		case RUBY_GPIO_PIN11:
			return AMBER_GPIO11_PWM0;
		case RUBY_GPIO_PIN12:
			return AMBER_GPIO12_PWM1;
		case RUBY_GPIO_PIN13:
			return AMBER_GPIO13_PWM2;
		case RUBY_GPIO_PIN14:
			return AMBER_GPIO14_PWM3;
		case RUBY_GPIO_PIN15:
			return AMBER_GPIO15_PWM4;
		case RUBY_GPIO_PIN16:
			return AMBER_GPIO16_PWM5;
		case RUBY_GPIO_PIN17:
			return AMBER_GPIO17_PWM6;
#endif
	}
	return 0;
}

int gpio_enable_pwm(uint32_t pin, uint32_t high_count, uint32_t low_count)
{
	uint32_t gpio_pwm_reg_addr = 0;
	uint32_t gpio_pwm_reg_val = 0;

	if (high_count > RUBY_GPIO_PWM_MAX_COUNT || low_count > RUBY_GPIO_PWM_MAX_COUNT)
		return -1;

	gpio_pwm_reg_addr = _gpio_pin_pwm_reg(pin);
	if (gpio_pwm_reg_addr == 0)
		return -1;

	gpio_pwm_reg_val = (low_count << RUBY_GPIO_PWM_LOW_SHIFT) |
				(high_count << RUBY_GPIO_PWM_HIGH_SHIFT) | RUBY_GPIO_PWM_ENABLE;

	writel(gpio_pwm_reg_val, gpio_pwm_reg_addr);

	return 0;
}
EXPORT_SYMBOL(gpio_enable_pwm);

uint32_t gpio_disable_pwm(uint32_t pin)
{
	uint32_t gpio_pwm_reg_addr = 0;

	gpio_pwm_reg_addr = _gpio_pin_pwm_reg(pin);
	if (gpio_pwm_reg_addr == 0)
		return -1;

	writel(0, gpio_pwm_reg_addr);

	return 0;
}
EXPORT_SYMBOL(gpio_disable_pwm);

arch_initcall(arc_gpio_init);

MODULE_DESCRIPTION("ARC 7XX GPIO");
MODULE_AUTHOR("Quantenna");
MODULE_LICENSE("GPL");
