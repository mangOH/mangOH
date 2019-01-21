/*
 * An I2C driver for the PCF85063 RTC
 * Copyright 2014 Rose Technology
 *
 * Author: Søren Andersen <san@rosetechnology.dk>
 * Maintainers: http://www.nslu2-linux.org/
 *
 * based on the other drivers in this same directory.
 *
 * Sierra Wireless Inc. - Added on functionality to allow
 * control of the square-wave clkout frequencies via sysfs entries.
 * Note, this was developed in the context of the MangOH Yellow board
 * which has the square-wave clkout pin connected to a buzzer. Thus,
 * one can drive the buzzer for the allowable frequencies, note
 * humans can only recognize 20 - 20KHz (some of us, like your author
 * even less).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/module.h>

/*
 * Information for this driver was pulled from the following datasheets.
 *
 *  http://www.nxp.com/documents/data_sheet/PCF85063A.pdf
 *  http://www.nxp.com/documents/data_sheet/PCF85063TP.pdf
 *
 *  PCF85063A -- Rev. 6 — 18 November 2015
 *  PCF85063TP -- Rev. 4 — 6 May 2015
*/

#define PCF85063_REG_CTRL1		0x00 /* status */
#define PCF85063_REG_CTRL1_STOP		BIT(5)
#define PCF85063_REG_CTRL2		0x01
#define CLKOUT_FREQ_MASK		0x0007

#define PCF85063_REG_SC			0x04 /* datetime */
#define PCF85063_REG_SC_OS		0x80
#define PCF85063_REG_MN			0x05
#define PCF85063_REG_HR			0x06
#define PCF85063_REG_DM			0x07
#define PCF85063_REG_DW			0x08
#define PCF85063_REG_MO			0x09
#define PCF85063_REG_YR			0x0A

/* Lookup for square-wave clkout frequencies */
#define NUM_FREQUENCIES			8
static const int clkout_freq_table[NUM_FREQUENCIES][2] = {
	{32768, 0}, {16384, 1}, {8192, 2}, {4096, 3},
	{2048, 4}, {1024, 5}, {1, 6}, {0, 7}
};

static struct i2c_driver pcf85063_driver;

/*
 * We need to define a mutex here because the rtc_sync module in MangOH
 * can call into rtc_set_time while sysfs is changing the square-wave
 * output frequencies. Ditto for rtc_read_time from rtc_sync.
 * We wouldn't need the mutex if a workQ handler was defined for this device
 * but then the rtc_sync module would not be generic. Just be aware that
 * any rtc chip that allows rtc_sync to call into it needs a mutex if any
 * other threaded access will concurrently exist.
 */
static DEFINE_MUTEX(pcf85063_rtc_mutex);

static int pcf85063_get_clkout_freq(struct device *dev, s32 *freq)
{
        struct i2c_client *client = to_i2c_client(dev);
        int i;

	mutex_lock(&pcf85063_rtc_mutex);
        *freq = i2c_smbus_read_byte_data(client, PCF85063_REG_CTRL2);
	mutex_unlock(&pcf85063_rtc_mutex);

        if (*freq < 0) {
                dev_err(&client->dev, "Failed to read PCF85063_REG_CTRL2\n");
                return -EIO;
        }

	*freq &= CLKOUT_FREQ_MASK;

	for(i = 0 ; i < NUM_FREQUENCIES &&
		clkout_freq_table[i][1] != *freq ; i++)
		;
        if (i == NUM_FREQUENCIES)
                return -EINVAL;
        *freq = clkout_freq_table[i][0];

        return 0;
}

static int pcf85063_set_clkout_freq(struct device *dev, int freq)
{
        struct i2c_client *client = to_i2c_client(dev);
	int i, ret;

        for(i = 0 ; i < NUM_FREQUENCIES &&
                clkout_freq_table[i][0] != freq ; i++)
                ;

        if (i == NUM_FREQUENCIES)
                return -EINVAL;

        freq = clkout_freq_table[i][1];
	freq &= CLKOUT_FREQ_MASK;

	mutex_lock(&pcf85063_rtc_mutex);
        ret = i2c_smbus_write_byte_data(client, PCF85063_REG_CTRL2, (u8) freq);
	mutex_unlock(&pcf85063_rtc_mutex);

        if (ret) {
                dev_err(&client->dev, "Failed to write register PCF85063_REG_CTRL2\n");
        	return ret;
        }

        return 0;
}

static int pcf85063_stop_clock(struct i2c_client *client, u8 *ctrl1)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, PCF85063_REG_CTRL1);
	if (ret < 0) {
		dev_err(&client->dev, "Failing to stop the clock\n");
		return -EIO;
	}

	/* stop the clock */
	ret |= PCF85063_REG_CTRL1_STOP;

	ret = i2c_smbus_write_byte_data(client, PCF85063_REG_CTRL1, ret);
	if (ret < 0) {
		dev_err(&client->dev, "Failing to stop the clock\n");
		return -EIO;
	}

	*ctrl1 = ret;

	return 0;
}

static int pcf85063_start_clock(struct i2c_client *client, u8 ctrl1)
{
	s32 ret;

	/* start the clock */
	ctrl1 &= ~PCF85063_REG_CTRL1_STOP;

	ret = i2c_smbus_write_byte_data(client, PCF85063_REG_CTRL1, ctrl1);
	if (ret < 0) {
		dev_err(&client->dev, "Failing to start the clock\n");
		return -EIO;
	}

	return 0;
}

static int pcf85063_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct i2c_client *client = to_i2c_client(dev);
	int rc;
	int ret = 0;
	u8 regs[7];

	mutex_lock(&pcf85063_rtc_mutex);
	/*
	 * while reading, the time/date registers are blocked and not updated
	 * anymore until the access is finished. To not lose a second
	 * event, the access must be finished within one second. So, read all
	 * time/date registers in one turn.
	 */
	rc = i2c_smbus_read_i2c_block_data(client, PCF85063_REG_SC,
					   sizeof(regs), regs);
	if (rc != sizeof(regs)) {
		dev_err(&client->dev, "date/time register read error\n");
		ret = -EIO;
		goto exit;
	}

	/* if the clock has lost its power it makes no sense to use its time */
	if (regs[0] & PCF85063_REG_SC_OS) {
		dev_warn(&client->dev, "Power loss detected, invalid time\n");
		ret = -EINVAL;
		goto exit;
	}

	tm->tm_sec = bcd2bin(regs[0] & 0x7F);
	tm->tm_min = bcd2bin(regs[1] & 0x7F);
	tm->tm_hour = bcd2bin(regs[2] & 0x3F); /* rtc hr 0-23 */
	tm->tm_mday = bcd2bin(regs[3] & 0x3F);
	tm->tm_wday = regs[4] & 0x07;
	tm->tm_mon = bcd2bin(regs[5] & 0x1F) - 1; /* rtc mn 1-12 */
	tm->tm_year = bcd2bin(regs[6]);
	tm->tm_year += 100;

exit:
	mutex_unlock(&pcf85063_rtc_mutex);
	return ret;
}

static int pcf85063_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct i2c_client *client = to_i2c_client(dev);
	int rc = 0;
	u8 regs[7];
	u8 ctrl1;

	mutex_lock(&pcf85063_rtc_mutex);
	if ((tm->tm_year < 100) || (tm->tm_year > 199)) {
		rc = -EINVAL;
		goto exit;
	}

	/*
	 * to accurately set the time, reset the divider chain and keep it in
	 * reset state until all time/date registers are written
	 */
	rc = pcf85063_stop_clock(client, &ctrl1);
	if (rc != 0)
		goto exit;

	/* hours, minutes and seconds */
	regs[0] = bin2bcd(tm->tm_sec) & 0x7F; /* clear OS flag */

	regs[1] = bin2bcd(tm->tm_min);
	regs[2] = bin2bcd(tm->tm_hour);

	/* Day of month, 1 - 31 */
	regs[3] = bin2bcd(tm->tm_mday);

	/* Day, 0 - 6 */
	regs[4] = tm->tm_wday & 0x07;

	/* month, 1 - 12 */
	regs[5] = bin2bcd(tm->tm_mon + 1);

	/* year and century */
	regs[6] = bin2bcd(tm->tm_year - 100);

	/* write all registers at once */
	rc = i2c_smbus_write_i2c_block_data(client, PCF85063_REG_SC,
					    sizeof(regs), regs);
	if (rc < 0) {
		dev_err(&client->dev, "date/time register write error\n");
		goto exit;
	}

	/*
	 * Write the control register as a separate action since the size of
	 * the register space is different between the PCF85063TP and
	 * PCF85063A devices.  The rollover point can not be used.
	 */
	rc = pcf85063_start_clock(client, ctrl1);
exit:
	mutex_unlock(&pcf85063_rtc_mutex);
	return rc;
}

static const struct rtc_class_ops pcf85063_rtc_ops = {
	.read_time	= pcf85063_rtc_read_time,
	.set_time	= pcf85063_rtc_set_time
};

static ssize_t pcf85063_sysfs_show_clkout_freq(struct device *dev,
                                              struct device_attribute *attr,
                                              char *buf)
{
        int err;
	s32 freq;

        err = pcf85063_get_clkout_freq(dev, &freq);
        if (err)
                return err;

        return sprintf(buf, "%d\n", (int) freq);
}

static ssize_t pcf85063_sysfs_store_clkout_freq(struct device *dev,
                                               struct device_attribute *attr,
                                               const char *buf, size_t count)
{
        int freq, err;

        if (sscanf(buf, "%i", &freq) != 1)
                return -EINVAL;

        err = pcf85063_set_clkout_freq(dev, freq);

        return err ? err : count;
}

static DEVICE_ATTR(clkout_freq, S_IRUGO | S_IWUSR,
                   pcf85063_sysfs_show_clkout_freq,
                   pcf85063_sysfs_store_clkout_freq);

static int pcf85063_sysfs_register(struct device *dev)
{
        return device_create_file(dev, &dev_attr_clkout_freq);
}

static void pcf85063_sysfs_unregister(struct device *dev)
{
        device_remove_file(dev, &dev_attr_clkout_freq);
}

static int pcf85063_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct rtc_device *rtc;
	int err;

	dev_dbg(&client->dev, "%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	err = i2c_smbus_read_byte_data(client, PCF85063_REG_CTRL1);
	if (err < 0) {
		dev_err(&client->dev, "RTC chip is not present\n");
		return err;
	}

	rtc = devm_rtc_device_register(&client->dev,
				       pcf85063_driver.driver.name,
				       &pcf85063_rtc_ops, THIS_MODULE);
        if (IS_ERR(rtc)) {
                err = PTR_ERR(rtc);
                goto exit;
        }

	/* By default this chip output's square waves at 32768 Hz - let's
	 * set to off or COF[2:0] == 7
	 */
        err = i2c_smbus_write_byte_data(client, PCF85063_REG_CTRL2, (u8) 0x7);
        if (err) {
                dev_err(&client->dev, "Failed to write register PCF85063_REG_CTRL2\n");
                goto exit;
        }

        err = pcf85063_sysfs_register(&client->dev);
        if (err)
                goto exit;

        return 0;

exit:
        return err;
}

static int pcf85063_remove(struct i2c_client *client)
{
        pcf85063_sysfs_unregister(&client->dev);
        return 0;
}

static const struct i2c_device_id pcf85063_id[] = {
	{ "pcf85063", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pcf85063_id);

#ifdef CONFIG_OF
static const struct of_device_id pcf85063_of_match[] = {
	{ .compatible = "nxp,pcf85063" },
	{}
};
MODULE_DEVICE_TABLE(of, pcf85063_of_match);
#endif

static struct i2c_driver pcf85063_driver = {
	.driver		= {
		.name	= "rtc-pcf85063",
		.of_match_table = of_match_ptr(pcf85063_of_match),
	},
	.probe		= pcf85063_probe,
	.remove         = pcf85063_remove,
	.id_table	= pcf85063_id,
};

module_i2c_driver(pcf85063_driver);

MODULE_AUTHOR("Søren Andersen <san@rosetechnology.dk>");
MODULE_DESCRIPTION("PCF85063 RTC driver");
MODULE_LICENSE("GPL");
