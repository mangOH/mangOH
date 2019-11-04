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
/*
 * EEPROMs on IoT cards contain information about the device(s) on the
 * cards and busses it uses. For a detailed description of EEPROM
 * format, please refer to the IoT card specification on mangOH web
 * site www.mangoh.io.
 *
 * Each IoT card EEPROM contains a master header that (among other
 * information) contains the EEPROM magic 0xAA55 that identifies a
 * valid programmed EEPROM, and the version number. Parsing of EEPROM
 * contents depends on the EEPROM version number, so this code makes
 * maximum attempts to support backwards-compatibility of format.
 *
 * IoT cards use at24 compatible EEPROMs. IoT card configuration
 * starts by loading EEPROM contents into memory and creating an
 * EEPROM map to easily parse sections. At the top of the EEPROM there
 * is a master header section that contains the board manufacturer,
 * board name, serial number, etc. This section is followed by one or
 * more card interface sections that describes busses and devices
 * used on the IoT card. This information should be sufficient to
 * identify and load the driver(s) for device(s) on the card.
 *
 * EEPROM map looks as follows:		+----struct eeprom_map----+
 * +--------------------------+<--------+-------buffer            |
 * |                          |      +--+-------interfaces;       |
 * |      Master Header       |      |  +-------------------------+
 * |                          |      |
 * +--------------------------+<---+ |  +--struct eeprom_if_map---+
 * | Interface Description 1  |    +-+--+-----contents            |
 * +--------------------------+<-+   +--+---->list                |
 * | Interface Description 2  |  |   |  +-------------------------+
 * |--------------------------+  |   |
 * |          ...             |	 |   |	+--struct eeprom_if_map---+
 * +--------------------------+	 +---+--+-----contents            |
 * | Interface Description N  |      +--+---->list                |
 * +--------------------------+	     |  +-------------------------+
 *				     .		...
 * The master eeprom map and eeprom interface maps are used for easily
 * locating the corresponding buffers, to reference each other, as
 * well as to back-reference the eeprom device struct(s).
 */
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/gpio/driver.h>
#include <linux/platform_data/at24.h>

#include "iot-slot-eeprom.h"
#include "iot-slot-eeprom-1v0.h"

#define HOST_UINT8(buffer, offset)	(*(uint8_t*)(&(buffer)[(offset)]))
#define HOST_UINT16(buffer, offset)	ntohs(*(uint16_t*)(&(buffer)[(offset)]))
#define HOST_UINT32(buffer, offset)	ntohl(*(uint32_t*)(&(buffer)[(offset)]))

#define EEPROM_VERSION(major, minor)	(((major)&0xff) << 8 | ((minor)&0xff))
#define EEPROM_VERSION_MAJOR(version)	(((version) & 0xff00) >> 8)
#define EEPROM_VERSION_MINOR(version)	((version) & 0xff)

#define IOT_EEPROM_SIZE	4096
/*
 * Map in which EEPROM contents are read. This is global so make sure
 * buffer is invalidated beforehand and EEPROMs are read out one-by-one.
 */
static struct eeprom_map {
	uint8_t buffer[IOT_EEPROM_SIZE];
	struct list_head interfaces;
} this_eeprom;

struct eeprom_if_map {
	struct i2c_client *eeprom;	/* back pointer to eeprom */
	uint8_t *contents;		/* pointer to interface description */
	struct list_head list;		/* interface list */
};

#define to_eeprom_if_map(item) container_of((item), struct eeprom_if_map, list)

static void at24_eeprom_setup(struct memory_accessor *mem_acc, void *context)
{
	struct eeprom_map *map = (struct eeprom_map *)context;
	/* Invalidate buffer before reading */
	if (!map) {
		pr_err("%s: Invalid buffer %p.\n", __func__, map);
		return;
	}
	INIT_LIST_HEAD(&map->interfaces);
	if (mem_acc->read(mem_acc, map->buffer, 0, sizeof(map->buffer))
		 != sizeof(map->buffer)) {
		/* Invalidate buffer again in case of failed/partial read */
		pr_err("%s: Error reading from EEPROM.\n", __func__);
		memset(map->buffer, 0xff, IOT_EEPROM_SIZE);
		return;
	}
}

static struct at24_platform_data at24_eeprom_data = {
	.byte_len = IOT_EEPROM_SIZE,
	.page_size = 32,
	.flags = AT24_FLAG_ADDR16,
	.setup = at24_eeprom_setup,
	.context = &this_eeprom,
};
static struct i2c_board_info at24_eeprom_info = {
	I2C_BOARD_INFO("at24", 0x52),
	.platform_data = &at24_eeprom_data,
};

static inline uint8_t *to_eeprom_buffer(struct i2c_client *eeprom)
{
	struct at24_platform_data *pdata = dev_get_platdata(&eeprom->dev);
	uint8_t *buffer = ((struct eeprom_map *)pdata->context)->buffer;

	if (0xAA != buffer[0] || 0x55 != buffer[1]) {
		dev_err(&eeprom->dev, "Invalid header: %02x%02x.\n",
			buffer[0], buffer[1]);
		return NULL;
	}
	return buffer;
}

static inline struct list_head *to_eeprom_if_list(struct i2c_client *eeprom)
{
	struct at24_platform_data *pdata = dev_get_platdata(&eeprom->dev);
	struct eeprom_map *map = (struct eeprom_map *)pdata->context;
	return &map->interfaces;
}

#define VERSION_OFFSET 2
static uint16_t eeprom_version(struct i2c_client *eeprom)
{
	uint8_t *buffer = to_eeprom_buffer(eeprom);
	return HOST_UINT16(buffer, VERSION_OFFSET);
}

static void *eeprom_if_first(struct i2c_client *eeprom)
{
	switch (eeprom_version(eeprom)) {
	case EEPROM_VERSION(1, 0): {
		uint8_t *buffer = to_eeprom_buffer(eeprom);
		eeprom_if_1v0 *ifc =
			(eeprom_if_1v0*)&buffer[EEPROM_1V0_INTERFACE_OFFSET];
		return (ifc->type == EEPROM_IF_LAST ? NULL : ifc);
	}
	default:
		BUG();
		return NULL;
	}
}

static void *eeprom_if_next(struct i2c_client *eeprom, void *prev)
{
	switch (eeprom_version(eeprom)) {
	case EEPROM_VERSION(1, 0): {
		eeprom_if_1v0 *ifc = (eeprom_if_1v0*)prev;
		return (ifc->type == EEPROM_IF_LAST ? NULL : ++ifc);
	}
	default:
		BUG();
		return NULL;
	}
}

static void eeprom_free_interfaces(struct i2c_client *eeprom)
{
	while (!list_empty(to_eeprom_if_list(eeprom))) {
		struct eeprom_if_map *m;
		m = list_first_entry(to_eeprom_if_list(eeprom),
				     struct eeprom_if_map, list);
		list_del(&m->list);
		kfree(m);
	}
}

static int eeprom_load_interfaces(struct i2c_client *eeprom)
{
	int count = 0;
	void *ifc = eeprom_if_first(eeprom);
	while (ifc) {
		struct eeprom_if_map *m = kzalloc(sizeof(*m), GFP_KERNEL);
		if (!m) {
			dev_info(&eeprom->dev, "Out of memory.");
			eeprom_free_interfaces(eeprom);
			return -1;
		}
		m->eeprom = eeprom;
		m->contents = ifc;
		list_add_tail(&m->list, to_eeprom_if_list(eeprom));
		count++;
		ifc = eeprom_if_next(eeprom, ifc);
	}
	dev_info(&eeprom->dev, "%d interface(s) detected\n", count);
	return count;
}

/* Public functions */
struct i2c_client *eeprom_load(struct i2c_adapter *i2c_adapter)
{
	struct i2c_client *eeprom;

	if (!i2c_adapter)
		return NULL;

	/* This automatically runs the setup() function */
	eeprom = i2c_new_device(i2c_adapter, &at24_eeprom_info);

	/* If no eeprom is detected, return NULL */
	if (eeprom == NULL)
		return NULL;

	/* Validate EEPROM header */
	if (!to_eeprom_buffer(eeprom)) {
		dev_err(&eeprom->dev, "Header not found.\n");
		i2c_unregister_device(eeprom);
		return NULL;
	}
	return (eeprom_load_interfaces(eeprom) > 0 ? eeprom : NULL);
}

void eeprom_unload(struct i2c_client *eeprom)
{
	uint8_t *buffer = to_eeprom_buffer(eeprom);

	/* Free interface list and invalidate buffer */
	eeprom_free_interfaces(eeprom);
	memset(buffer, 0xff, IOT_EEPROM_SIZE);
	i2c_unregister_device(eeprom);
}

int eeprom_num_slots(struct i2c_client *eeprom)
{
	return 1; /* for now */
}


struct list_head *eeprom_if_list(struct i2c_client *eeprom)
{
	return to_eeprom_if_list(eeprom);
}

enum EepromInterface eeprom_if_type(struct list_head *item)
{
	enum EepromInterface result;
	struct eeprom_if_map *m = to_eeprom_if_map(item);
	switch (eeprom_version(m->eeprom)) {
	case EEPROM_VERSION(1, 0):
	{
		eeprom_if_1v0 *eif = (eeprom_if_1v0*)m->contents;
		result = eif->type;
		break;
	}
	default:
		BUG();
		result = EEPROM_IF_LAST;
		break;
	}

	return result;
}

uint8_t eeprom_if_gpio_cfg(struct list_head *item, unsigned int pin)
{
	struct eeprom_if_map *m = to_eeprom_if_map(item);
	switch (eeprom_version(m->eeprom)) {
	case EEPROM_VERSION(1, 0): {
		eeprom_if_1v0 *eif = (eeprom_if_1v0*)m->contents;
		BUG_ON(eif->type != EEPROM_IF_GPIO);
		BUG_ON(pin >= ARRAY_SIZE(eif->ifc.gpio.cfg));
		return eif->ifc.gpio.cfg[pin];
	}
	default:
		BUG();
		return 0xff;
	}
}

char *eeprom_if_spi_modalias(struct list_head *item)
{
	struct eeprom_if_map *m = to_eeprom_if_map(item);
	switch (eeprom_version(m->eeprom)) {
	case EEPROM_VERSION(1, 0): {
		eeprom_if_1v0 *eif = (eeprom_if_1v0*)m->contents;
		BUG_ON(eif->type != EEPROM_IF_SPI);
		return (eif->ifc.spi.modalias);
	}
	default:
		/* unsupported eeprom version */
		BUG();
		return NULL;
	}
}

int eeprom_if_spi_irq_gpio(struct list_head *item)
{
	struct eeprom_if_map *m = to_eeprom_if_map(item);
	switch (eeprom_version(m->eeprom)) {
	case EEPROM_VERSION(1, 0): {
		eeprom_if_1v0 *eif = (eeprom_if_1v0*)m->contents;
		BUG_ON(eif->type != EEPROM_IF_SPI);
		return eif->ifc.spi.irq_gpio;
	}
	default:
		/* unsupported eeprom version */
		BUG();
		return -1;
	}
}

char *eeprom_if_i2c_modalias(struct list_head *item)
{
	struct eeprom_if_map *m = to_eeprom_if_map(item);
	switch (eeprom_version(m->eeprom)) {
	case EEPROM_VERSION(1, 0): {
		eeprom_if_1v0 *eif = (eeprom_if_1v0*)m->contents;
		BUG_ON(eif->type != EEPROM_IF_I2C);
		return (eif->ifc.i2c.modalias);
	}
	default:
		/* unsupported eeprom version */
		BUG();
		return NULL;
	}
}

int eeprom_if_i2c_irq_gpio(struct list_head *item)
{
	struct eeprom_if_map *m = to_eeprom_if_map(item);
	switch (eeprom_version(m->eeprom)) {
	case EEPROM_VERSION(1, 0): {
		eeprom_if_1v0 *eif = (eeprom_if_1v0*)m->contents;
		BUG_ON(eif->type != EEPROM_IF_I2C);
		return eif->ifc.i2c.irq_gpio;
	}
	default:
		/* unsupported eeprom version */
		BUG();
		return -1;
	}
}

uint8_t eeprom_if_i2c_address(struct list_head *item)
{
	struct eeprom_if_map *m = to_eeprom_if_map(item);
	switch (eeprom_version(m->eeprom)) {
	case EEPROM_VERSION(1, 0): {
		eeprom_if_1v0 *eif = (eeprom_if_1v0*)m->contents;
		BUG_ON(eif->type != EEPROM_IF_I2C);
		return (eif->ifc.i2c.address);
	}
	default:
		/* unsupported eeprom version */
		BUG();
		return -1;
	}
}
