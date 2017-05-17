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
#include "interrupt.h"
#include "queue.h"
#include "spi.h"

static void mt7697spi_reset(struct spi_device *spi)
{

}

static void mt7697spi_enable_irq(struct spi_device *spi)
{
	enable_irq(spi->irq);
}

static void mt7697spi_disable_irq(struct spi_device *spi)
{
	disable_irq(spi->irq);
}

static const struct mt7697spi_hw_ops hw_ops =
{
	.write			= spi_write,
	.read			= spi_read,
	.write_then_read	= spi_write_then_read,
	.reset			= mt7697spi_reset,
	.enable_irq		= mt7697spi_enable_irq,
	.disable_irq		= mt7697spi_disable_irq,
};

static int mt7697spi_probe(struct spi_device *spi)
{
	struct mt7697q_info *qinfo;
	int ret = 0;

	pr_info(DRVNAME" init dev('%s') mode(%d) max speed(%d) "
		"CS(%d) GPIO(%d) bits/word(%d)\n", 
		spi->modalias, spi->mode, spi->max_speed_hz, spi->chip_select, 
		spi->cs_gpio, spi->bits_per_word);

	qinfo = kzalloc(sizeof(struct mt7697q_info), GFP_KERNEL);
	if (!qinfo) {
		pr_err(DRVNAME" create queue info failed\n");
		ret = -ENOMEM;
		goto cleanup;
	}

	memset(qinfo, 0, sizeof(struct mt7697q_info));
	qinfo->dev = &spi->dev;

        qinfo->hw_priv = spi;
        qinfo->hw_ops = &hw_ops;

	mutex_init(&qinfo->mutex);

	qinfo->irq_workq = create_workqueue(DRVNAME);
	if (!qinfo) {
		pr_err(DRVNAME" create_workqueue() failed\n");
		ret = -ENOMEM;
		goto cleanup;
	}

	INIT_DELAYED_WORK(&qinfo->irq_work, mt7697_irq_work);
//	INIT_WORK(&qinfo->irq_work, mt7697_irq_work);
	qinfo->irq = spi->irq;
	dev_info(qinfo->dev, "%s(): '%s' request irq(%d)\n", 
		__func__, spi->modalias, spi->irq);
/*	ret = request_irq(spi->irq, mt7697_irq,
        	IRQF_SHARED| IRQF_NO_SUSPEND, spi->modalias, qinfo);
        if (ret < 0) {
                pr_err(DRVNAME" request_irq() failed(%d)", ret);
                goto failed_irq;
        }
*/
	spi_set_drvdata(spi, qinfo);

	ret = queue_delayed_work(qinfo->irq_workq, &qinfo->irq_work, 
		msecs_to_jiffies(1000));
	if (ret < 0) {
		dev_err(qinfo->dev, 
			"%s(): queue_delayed_work() failed(%d)\n", 
			__func__, ret);
    	}
	return 0;

/*failed_irq:
	destroy_workqueue(qinfo->irq_workq);
	free_irq(spi->irq, qinfo);*/
cleanup:
	kfree(qinfo);
	return ret;
}

static int mt7697spi_remove(struct spi_device *spi)
{
	struct mt7697q_info *qinfo = spi_get_drvdata(spi);
	if (qinfo) {
		dev_info(qinfo->dev, "%s(): remove\n", __func__);
		free_irq(spi->irq, qinfo);
		kfree(qinfo);
	}

	return 0;
}

static const struct of_device_id dt_ids[] = {
        { .compatible = "MediaTek,mt7697" },
        {},
};

MODULE_DEVICE_TABLE(of, dt_ids);

static struct spi_driver mt7697spi_driver = {
	.driver = {
		.name = DRVNAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(dt_ids),
	},

	.probe = mt7697spi_probe,
	.remove	= mt7697spi_remove,
};

module_spi_driver(mt7697spi_driver)

MODULE_AUTHOR( "Sierra Wireless Corporation" );
MODULE_LICENSE( "GPL" );
MODULE_DESCRIPTION( "MediaTek7697 SPI master" );
