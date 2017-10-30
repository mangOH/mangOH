/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef IOT_SLOT_EEPROM_H
#define IOT_SLOT_EEPROM_H

#include <linux/i2c.h>


#define IRQ_GPIO_UNUSED (0xFF)

#define EEPROM_GPIO_CFG_INPUT_PULL_UP	(0x1)
#define EEPROM_GPIO_CFG_INPUT_PULL_DOWN	(0x2)
#define EEPROM_GPIO_CFG_INPUT_FLOATING	(0x3)
#define EEPROM_GPIO_CFG_OUTPUT_LOW	(0x4)
#define EEPROM_GPIO_CFG_OUTPUT_HIGH	(0x5)

enum EepromInterface
{
	EEPROM_IF_GPIO,
	EEPROM_IF_I2C,
	EEPROM_IF_SPI,
	EEPROM_IF_USB,
	EEPROM_IF_SDIO,
	EEPROM_IF_ADC,
	EEPROM_IF_PCM,
	EEPROM_IF_CLK,
	EEPROM_IF_UART,
	EEPROM_IF_PLAT,
/* add more interface types here */
	EEPROM_IF_LAST_SUPPORTED,
	EEPROM_IF_LAST = 0xFF,
};

struct i2c_client *eeprom_load(struct i2c_adapter *i2c_adapter);
void eeprom_unload(struct i2c_client *eeprom);
struct list_head *eeprom_if_list(struct i2c_client *eeprom);
int eeprom_num_slots(struct i2c_client *eeprom);


enum EepromInterface eeprom_if_type(struct list_head *item);
uint8_t eeprom_if_gpio_cfg(struct list_head *item, unsigned int pin);
char *eeprom_if_spi_modalias(struct list_head *item);
int eeprom_if_spi_irq_gpio(struct list_head *item);
char *eeprom_if_i2c_modalias(struct list_head *item);
int eeprom_if_i2c_irq_gpio(struct list_head *item);
uint8_t eeprom_if_i2c_address(struct list_head *item);

#endif /* IOT_SLOT_EEPROM_H */

