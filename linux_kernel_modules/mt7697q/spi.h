/*
 * This file is part of mt7697
 *
 * Copyright (C) 2017 Sierra Wireless Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __MT7697_SPI_H__
#define __MT7697_SPI_H__

#include <linux/spi/spi.h>

#define DRVNAME					"mt7697q"
#define MT7697_SPI_GPIO_IRQ_NAME		"mt7697q irq"

#if defined(CONFIG_ARCH_MSM9615) /* For WPX5XX */
#define MT7697_SPI_INTR_GPIO_PIN		50
#elif defined(CONFIG_ARCH_MDM9607) /* For WP76XX */
#define MT7697_SPI_INTR_GPIO_PIN		8
#endif
#define MT7697_SPI_INTR_GPIO_PIN_INVALID	-1
#define MT7697_SPI_BUS_NUM			32766
#define MT7697_SPI_CS				0

#define MT7697_SPI_CONFIG			"0,2,-1,0,0,1,0,0,0,0,0,mt7697"

struct mt7697spi_hw_ops {
	int (*write)(struct spi_device*, const void*, size_t);
	int (*read)(struct spi_device*, void*, size_t);
	int (*write_then_read)(struct spi_device*, const void*, void*, unsigned);
	void (*reset)(struct spi_device*);
	void (*enable_irq)(struct spi_device*);
	void (*disable_irq)(struct spi_device*);
};

#endif /* __MT7697_SPI_H__ */
