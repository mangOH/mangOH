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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/usb.h>
#include <linux/gpio.h>
#include "spi-cp2130.h"
#include "interrupt.h"
#include "queue.h"
#include "spi.h"

static int mt7697spi_write_then_read(struct spi_device *spi, const void *txbuf,
				     void *rxbuf, unsigned len)
{
	struct spi_transfer transfer = {
		.tx_buf		= txbuf,
		.rx_buf		= rxbuf,
		.len		= len,
	};

	return spi_sync_transfer(spi, &transfer, 1);
}

static void mt7697spi_reset(struct spi_device *spi)
{

}

static void mt7697spi_enable_irq(struct spi_device *spi)
{
	struct mt7697q_info *qinfo = spi_get_drvdata(spi);
	WARN_ON(!qinfo);
	enable_irq(qinfo->irq);
}

static void mt7697spi_disable_irq(struct spi_device *spi)
{
	struct mt7697q_info *qinfo = spi_get_drvdata(spi);
	WARN_ON(!qinfo);
	disable_irq(qinfo->irq);
}

static const struct mt7697spi_hw_ops hw_ops =
{
	.write			= spi_write,
	.read			= spi_read,
	.write_then_read	= mt7697spi_write_then_read,
	.reset			= mt7697spi_reset,
	.enable_irq		= mt7697spi_enable_irq,
	.disable_irq		= mt7697spi_disable_irq,
};

static int __init mt7697spi_init(void)
{
	char str[32];
	struct spi_master *master = NULL;
	struct device *dev;
	struct spi_device *spi;
	struct mt7697q_info *qinfo = NULL;
	int bus_num = MT7697_SPI_BUS_NUM;
	int ret = 0;

	pr_info(DRVNAME" %s(): '%s' initialize\n", __func__, DRVNAME);

	while (!master && (bus_num >= 0)) {
		master = spi_busnum_to_master(bus_num);
		if (!master) 
			bus_num--;
	}

	if (!master) {
		pr_err(DRVNAME" spi_busnum_to_master() failed\n");
		ret = -EINVAL;
		goto cleanup;
	}

	ret = cp2130_update_ch_config(master, MT7697_SPI_CONFIG);
	if (ret < 0) {
		dev_err(&master->dev, 
			"%s(): cp2130_update_ch_config() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

	snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev), MT7697_SPI_CS);
	dev_info(&master->dev, "%s(): find SPI device('%s')\n", __func__, str);
	dev = bus_find_device_by_name(&spi_bus_type, NULL, str);
	if (!dev) {
		dev_err(&master->dev, 
			"%s(): bus_find_device_by_name('%s') failed\n", 
			__func__, str);

		ret = -EINVAL;
		goto cleanup;
	}

	spi = container_of(dev, struct spi_device, dev);
	if (!spi) {
		dev_err(&master->dev, "%s(): get SPI device failed\n", 
			__func__);
		ret = -EINVAL;
		goto cleanup;
	}

	dev_info(&master->dev, "%s(): init dev('%s') mode(%d) max speed(%d) "
		"CS(%d) bits/word(%d)\n", 
		__func__, spi->modalias, spi->mode, spi->max_speed_hz, 
		spi->chip_select, spi->bits_per_word);

	qinfo = kzalloc(sizeof(struct mt7697q_info), GFP_KERNEL);
	if (!qinfo) {
		dev_err(&master->dev, "%s(): create queue info failed\n", 
			__func__);
		ret = -ENOMEM;
		goto cleanup;
	}

	memset(qinfo, 0, sizeof(struct mt7697q_info));
	qinfo->dev = &spi->dev;

        qinfo->hw_priv = spi;
        qinfo->hw_ops = &hw_ops;

	mutex_init(&qinfo->mutex);
	INIT_DELAYED_WORK(&qinfo->irq_delayed_work, mt7697_irq_delayed_work);
	INIT_WORK(&qinfo->irq_work, mt7697_irq_work);

	qinfo->irq_workq = alloc_workqueue(DRVNAME"wq", 
		WQ_HIGHPRI | WQ_MEM_RECLAIM | WQ_UNBOUND, 1);
	if (!qinfo->irq_workq) {
		dev_err(qinfo->dev, "%s(): alloc_workqueue() failed\n", 
			__func__);
		ret = -ENOMEM;
		goto cleanup;
	}

	ret = gpio_request(MT7697_SPI_INTR_GPIO_PIN, MT7697_GPIO_IRQ_NAME);
        if (ret < 0) {
                dev_err(qinfo->dev, "%s(): gpio_request() failed(%d)", 
			__func__, ret);
                goto failed_workqueue;
        }

	gpio_direction_input(MT7697_SPI_INTR_GPIO_PIN);
	qinfo->irq = gpio_to_irq(MT7697_SPI_INTR_GPIO_PIN);
	dev_info(qinfo->dev, "%s(): request irq(%d)\n", __func__, qinfo->irq);
	ret = request_irq(qinfo->irq, mt7697_isr, 0, DRVNAME, qinfo);
        if (ret < 0) {
                dev_err(qinfo->dev, "%s(): request_irq() failed(%d)", 
			__func__, ret);
                goto failed_gpio_req;
        }

	irq_set_irq_type(qinfo->irq, IRQ_TYPE_EDGE_BOTH);

	spi_set_drvdata(spi, qinfo);

	dev_info(qinfo->dev, "%s(): '%s' initialized\n", __func__, DRVNAME);
	return 0;

failed_workqueue:
	destroy_workqueue(qinfo->irq_workq);

failed_gpio_req:
	gpio_free(MT7697_SPI_INTR_GPIO_PIN);

cleanup:
	if (qinfo) kfree(qinfo);
	return ret;
}

static void __exit mt7697spi_exit(void)
{
	char str[32];
	struct spi_master *master = NULL;
	struct device *dev;
	struct spi_device *spi;
	struct mt7697q_info *qinfo;
	int bus_num = MT7697_SPI_BUS_NUM;

	while (!master && (bus_num >= 0)) {
		master = spi_busnum_to_master(bus_num);
		if (!master)
			bus_num--;
	}

	if (!master) {
		pr_err(DRVNAME" spi_busnum_to_master() failed\n");
		goto cleanup;
	}

	snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev), MT7697_SPI_CS);
	dev_info(&master->dev, "%s(): find SPI device('%s')\n", __func__, str);
	dev = bus_find_device_by_name(&spi_bus_type, NULL, str);
	if (!dev) {
		dev_err(&master->dev, 
			"%s(): '%s' bus_find_device_by_name() failed\n", 
			__func__, str);
		goto cleanup;
	}

	spi = container_of(dev, struct spi_device, dev);
	if (!spi) {
		dev_err(dev, "%s():  get SPI device failed\n", 
			__func__);
		goto cleanup;
	}

	qinfo = spi_get_drvdata(spi);
	if (!qinfo) {
		dev_err(dev, "%s():  SPI device no queue info\n", 
			__func__);
		goto cleanup;
	}

	dev_info(qinfo->dev, "%s(): remove '%s'\n", __func__, DRVNAME);
	cancel_delayed_work_sync(&qinfo->irq_delayed_work);
	cancel_work_sync(&qinfo->irq_work);
	flush_workqueue(qinfo->irq_workq);
	destroy_workqueue(qinfo->irq_workq);

	free_irq(qinfo->irq, qinfo);
	gpio_free(MT7697_SPI_INTR_GPIO_PIN);
	kfree(qinfo);

cleanup:
	return;
}

module_init(mt7697spi_init);
module_exit(mt7697spi_exit);

MODULE_AUTHOR( "Sierra Wireless Corporation" );
MODULE_LICENSE( "GPL" );
MODULE_DESCRIPTION( "MediaTek7697 SPI master" );
