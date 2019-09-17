/**
 * opt300x.c - Texas Instruments OPT300x Light Sensor
 *
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com
 *
 * Author: Andreas Dannenberg <dannenberg@ti.com>
 * Based on previous work from: Felipe Balbi <balbi@ti.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 of the License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <linux/iio/events.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#define OPT300x_RESULT		0x00
#define OPT300x_CONFIGURATION	0x01
#define OPT300x_LOW_LIMIT	0x02
#define OPT300x_HIGH_LIMIT	0x03
#define OPT300x_MANUFACTURER_ID	0x7e
#define OPT3001_DEVICE_ID	0x7f

#define MANUFACTURER_ID_TI	0x5449

#define OPT300x_CONFIGURATION_RN_MASK	(0xf << 12)
#define OPT300x_CONFIGURATION_RN_AUTO	(0xc << 12)

#define OPT300x_CONFIGURATION_CT	BIT(11)

#define OPT300x_CONFIGURATION_M_MASK	(3 << 9)
#define OPT300x_CONFIGURATION_M_SHUTDOWN (0 << 9)
#define OPT300x_CONFIGURATION_M_SINGLE	(1 << 9)
#define OPT300x_CONFIGURATION_M_CONTINUOUS (2 << 9) /* also 3 << 9 */

#define OPT300x_CONFIGURATION_OVF	BIT(8)
#define OPT300x_CONFIGURATION_CRF	BIT(7)
#define OPT300x_CONFIGURATION_FH	BIT(6)
#define OPT300x_CONFIGURATION_FL	BIT(5)
#define OPT300x_CONFIGURATION_L		BIT(4)
#define OPT300x_CONFIGURATION_POL	BIT(3)
#define OPT300x_CONFIGURATION_ME	BIT(2)

#define OPT300x_CONFIGURATION_FC_MASK	(3 << 0)

/* The end-of-conversion enable is located in the low-limit register */
#define OPT300x_LOW_LIMIT_EOC_ENABLE	0xc000

#define OPT300x_REG_EXPONENT(n)		((n) >> 12)
#define OPT300x_REG_MANTISSA(n)		((n) & 0xfff)

#define OPT300x_INT_TIME_LONG		800000
#define OPT300x_INT_TIME_SHORT		100000

/*
 * Time to wait for conversion result to be ready. The device datasheet
 * sect. 6.5 states results are ready after total integration time plus 3ms.
 * This results in worst-case max values of 113ms or 883ms, respectively.
 * Add some slack to be on the safe side.
 */
#define OPT300x_RESULT_READY_SHORT	150
#define OPT300x_RESULT_READY_LONG	1000


struct opt300x_scale {
	int val;
	int val2;
};

struct opt300x_chip {
	int device_id;
	u8 scaler_numerator;
	u8 scaler_denominator;
	const struct opt300x_scale table[12];
	const struct iio_chan_spec channels[2];
};

struct opt300x {
	struct i2c_client	*client;
	struct device		*dev;

	struct opt300x_chip	*chip;
	struct mutex		lock;
	bool			ok_to_ignore_lock;
	bool			result_ready;
	wait_queue_head_t	result_ready_queue;
	u16			result;

	u32			int_time;
	u32			mode;

	u16			high_thresh_mantissa;
	u16			low_thresh_mantissa;

	u8			high_thresh_exp;
	u8			low_thresh_exp;

	bool			use_irq;
};


static IIO_CONST_ATTR_INT_TIME_AVAIL("0.1 0.8");

static struct attribute *opt300x_attributes[] = {
	&iio_const_attr_integration_time_available.dev_attr.attr,
	NULL
};

static const struct attribute_group opt300x_attribute_group = {
	.attrs = opt300x_attributes,
};

static const struct iio_event_spec opt300x_event_spec[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_RISING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
		BIT(IIO_EV_INFO_ENABLE),
	},
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_FALLING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
		BIT(IIO_EV_INFO_ENABLE),
	},
};

static const struct opt300x_chip opt3001_chip = {
	.device_id = OPT3001_DEVICE_ID,
	.scaler_numerator = 1,
	.scaler_denominator = 100,
	.table = {
		{ .val =    40, .val2 = 950000, },
		{ .val =    81, .val2 = 900000, },
		{ .val =   163, .val2 = 800000, },
		{ .val =   327, .val2 = 600000, },
		{ .val =   655, .val2 = 200000, },
		{ .val =  1310, .val2 = 400000, },
		{ .val =  2620, .val2 = 800000, },
		{ .val =  5241, .val2 = 600000, },
		{ .val = 10483, .val2 = 200000, },
		{ .val = 20966, .val2 = 400000, },
		{ .val = 41932, .val2 = 800000, },
		{ .val = 83865, .val2 = 600000, },
	},
	.channels = {
		{
			/* values reported in lux */
			.type = IIO_LIGHT,
			.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED) |
					      BIT(IIO_CHAN_INFO_INT_TIME),
			.event_spec = opt300x_event_spec,
			.num_event_specs = ARRAY_SIZE(opt300x_event_spec),
		},
		IIO_CHAN_SOFT_TIMESTAMP(1),
	},
};

static const struct opt300x_chip opt3002_chip = {
	.device_id = -1,
	.scaler_numerator = 6,
	.scaler_denominator = 5,
	.table = {
		{ .val =     4914, .val2 = 0, },
		{ .val =     9828, .val2 = 0, },
		{ .val =    19656, .val2 = 0, },
		{ .val =    39312, .val2 = 0, },
		{ .val =    78624, .val2 = 0, },
		{ .val =   157248, .val2 = 0, },
		{ .val =   314496, .val2 = 0, },
		{ .val =   628992, .val2 = 0, },
		{ .val =  1257984, .val2 = 0, },
		{ .val =  2515968, .val2 = 0, },
		{ .val =  5031936, .val2 = 0, },
		{ .val = 10063872, .val2 = 0, },
	},
	.channels = {
		{
			/* values reported in nW/cm^2 */
			.type = IIO_INTENSITY,
			.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED) |
					      BIT(IIO_CHAN_INFO_INT_TIME),
			.event_spec = opt300x_event_spec,
			.num_event_specs = ARRAY_SIZE(opt300x_event_spec),
		},
		IIO_CHAN_SOFT_TIMESTAMP(1),
	},
};

static int opt300x_find_scale(const struct opt300x *opt, int val,
		int val2, u8 *exponent)
{
	int i;
	int val_tmp = (val * 1000ll + val2 / 1000);

	for (i = 0; i < ARRAY_SIZE(opt->chip->table); i++) {
		const struct opt300x_scale *scale = &opt->chip->table[i];

		int scale_tmp = (scale->val * 1000ll + scale->val2 / 1000);
		/*
		 * Combine the whole and micro parts for comparison purposes.
		 * Use milli precision to avoid 32-bit integer overflows.
		 */
		if (val_tmp <= scale_tmp) {
			*exponent = i;
			return 0;
		}
	}

	return -EINVAL;
}

static void opt300x_to_iio_ret(struct opt300x *opt, u8 exponent,
		u16 mantissa, int *val, int *val2)
{
	u64 res;
	u64 tmp;
	u8 numerator = opt->chip->scaler_numerator;
	u8 denominator = opt->chip->scaler_denominator;
	tmp = 1000000ull;
	do_div(tmp, denominator);
	res = tmp * (mantissa << exponent) * numerator;
	tmp = res;
	do_div(tmp, 1000000);
	*val = tmp;
	*val2 = res - (1000000ull * (*val));
}

static void opt300x_set_mode(struct opt300x *opt, u16 *reg, u16 mode)
{
	*reg &= ~OPT300x_CONFIGURATION_M_MASK;
	*reg |= mode;
	opt->mode = mode;
}


static int opt300x_get_reading(struct opt300x *opt, int *val, int *val2)
{
	int ret;
	u16 mantissa;
	u16 reg;
	u8 exponent;
	u16 value;
	long timeout;

	if (opt->use_irq) {
		/*
		 * Enable the end-of-conversion interrupt mechanism. Note that
		 * doing so will overwrite the low-level limit value however we
		 * will restore this value later on.
		 */
		ret = i2c_smbus_write_word_swapped(opt->client,
					OPT300x_LOW_LIMIT,
					OPT300x_LOW_LIMIT_EOC_ENABLE);
		if (ret < 0) {
			dev_err(opt->dev, "failed to write register %02x\n",
					OPT300x_LOW_LIMIT);
			return ret;
		}

		/* Allow IRQ to access the device despite lock being set */
		opt->ok_to_ignore_lock = true;
	}

	/* Reset data-ready indicator flag */
	opt->result_ready = false;

	/* Configure for single-conversion mode and start a new conversion */
	ret = i2c_smbus_read_word_swapped(opt->client, OPT300x_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT300x_CONFIGURATION);
		goto err;
	}

	reg = ret;
	opt300x_set_mode(opt, &reg, OPT300x_CONFIGURATION_M_SINGLE);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT300x_CONFIGURATION,
			reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT300x_CONFIGURATION);
		goto err;
	}

	if (opt->use_irq) {
		/* Wait for the IRQ to indicate the conversion is complete */
		ret = wait_event_timeout(opt->result_ready_queue,
				opt->result_ready,
				msecs_to_jiffies(OPT300x_RESULT_READY_LONG));
	} else {
		/* Sleep for result ready time */
		timeout = (opt->int_time == OPT300x_INT_TIME_SHORT) ?
			OPT300x_RESULT_READY_SHORT : OPT300x_RESULT_READY_LONG;
		msleep(timeout);

		/* Check result ready flag */
		ret = i2c_smbus_read_word_swapped(opt->client,
						  OPT300x_CONFIGURATION);
		if (ret < 0) {
			dev_err(opt->dev, "failed to read register %02x\n",
				OPT300x_CONFIGURATION);
			goto err;
		}

		if (!(ret & OPT300x_CONFIGURATION_CRF)) {
			ret = -ETIMEDOUT;
			goto err;
		}

		/* Obtain value */
		ret = i2c_smbus_read_word_swapped(opt->client, OPT300x_RESULT);
		if (ret < 0) {
			dev_err(opt->dev, "failed to read register %02x\n",
				OPT300x_RESULT);
			goto err;
		}
		opt->result = ret;
		opt->result_ready = true;
	}

err:
	if (opt->use_irq)
		/* Disallow IRQ to access the device while lock is active */
		opt->ok_to_ignore_lock = false;

	if (ret == 0)
		return -ETIMEDOUT;
	else if (ret < 0)
		return ret;

	if (opt->use_irq) {
		/*
		 * Disable the end-of-conversion interrupt mechanism by
		 * restoring the low-level limit value (clearing
		 * OPT300x_LOW_LIMIT_EOC_ENABLE). Note that selectively clearing
		 * those enable bits would affect the actual limit value due to
		 * bit-overlap and therefore can't be done.
		 */
		value = (opt->low_thresh_exp << 12) | opt->low_thresh_mantissa;
		ret = i2c_smbus_write_word_swapped(opt->client,
						   OPT300x_LOW_LIMIT,
						   value);
		if (ret < 0) {
			dev_err(opt->dev, "failed to write register %02x\n",
					OPT300x_LOW_LIMIT);
			return ret;
		}
	}

	exponent = OPT300x_REG_EXPONENT(opt->result);
	mantissa = OPT300x_REG_MANTISSA(opt->result);

	opt300x_to_iio_ret(opt, exponent, mantissa, val, val2);

	return IIO_VAL_INT_PLUS_MICRO;
}

static int opt300x_get_int_time(struct opt300x *opt, int *val, int *val2)
{
	*val = 0;
	*val2 = opt->int_time;

	return IIO_VAL_INT_PLUS_MICRO;
}

static int opt300x_set_int_time(struct opt300x *opt, int time)
{
	int ret;
	u16 reg;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT300x_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT300x_CONFIGURATION);
		return ret;
	}

	reg = ret;

	switch (time) {
	case OPT300x_INT_TIME_SHORT:
		reg &= ~OPT300x_CONFIGURATION_CT;
		opt->int_time = OPT300x_INT_TIME_SHORT;
		break;
	case OPT300x_INT_TIME_LONG:
		reg |= OPT300x_CONFIGURATION_CT;
		opt->int_time = OPT300x_INT_TIME_LONG;
		break;
	default:
		return -EINVAL;
	}

	return i2c_smbus_write_word_swapped(opt->client, OPT300x_CONFIGURATION,
			reg);
}

static int opt300x_read_raw(struct iio_dev *iio,
		struct iio_chan_spec const *chan, int *val, int *val2,
		long mask)
{
	struct opt300x *opt = iio_priv(iio);
	int ret;

	if (opt->mode == OPT300x_CONFIGURATION_M_CONTINUOUS)
		return -EBUSY;

	if (chan->type != opt->chip->channels[0].type)
		return -EINVAL;

	mutex_lock(&opt->lock);

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		ret = opt300x_get_reading(opt, val, val2);
		break;
	case IIO_CHAN_INFO_INT_TIME:
		ret = opt300x_get_int_time(opt, val, val2);
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&opt->lock);

	return ret;
}

static int opt300x_write_raw(struct iio_dev *iio,
		struct iio_chan_spec const *chan, int val, int val2,
		long mask)
{
	struct opt300x *opt = iio_priv(iio);
	int ret;

	if (opt->mode == OPT300x_CONFIGURATION_M_CONTINUOUS)
		return -EBUSY;

	if (chan->type != opt->chip->channels[0].type)
		return -EINVAL;

	if (mask != IIO_CHAN_INFO_INT_TIME)
		return -EINVAL;

	if (val != 0)
		return -EINVAL;

	mutex_lock(&opt->lock);
	ret = opt300x_set_int_time(opt, val2);
	mutex_unlock(&opt->lock);

	return ret;
}

static int opt300x_read_event_value(struct iio_dev *iio,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir, enum iio_event_info info,
		int *val, int *val2)
{
	struct opt300x *opt = iio_priv(iio);
	int ret = IIO_VAL_INT_PLUS_MICRO;

	mutex_lock(&opt->lock);

	switch (dir) {
	case IIO_EV_DIR_RISING:
		opt300x_to_iio_ret(opt, opt->high_thresh_exp,
				opt->high_thresh_mantissa, val, val2);
		break;
	case IIO_EV_DIR_FALLING:
		opt300x_to_iio_ret(opt, opt->low_thresh_exp,
				opt->low_thresh_mantissa, val, val2);
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&opt->lock);

	return ret;
}

static int opt300x_write_event_value(struct iio_dev *iio,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir, enum iio_event_info info,
		int val, int val2)
{
	struct opt300x *opt = iio_priv(iio);
	int ret;

	u16 mantissa;
	u16 value;
	u16 reg;

	u8 exponent;
	u64 tmp;

	u8 numerator = opt->chip->scaler_numerator;
	u8 denominator = opt->chip->scaler_denominator;


	if (val < 0)
		return -EINVAL;

	mutex_lock(&opt->lock);

	ret = opt300x_find_scale(opt, val, val2, &exponent);
	if (ret < 0) {
		dev_err(opt->dev, "can't find scale for %d.%06u\n", val, val2);
		goto err;
	}

	tmp = ((val * 1000000ull) + val2) * denominator;
	do_div(tmp, numerator * 1000000ull);
	mantissa = tmp >> exponent;

	value = (exponent << 12) | mantissa;

	switch (dir) {
	case IIO_EV_DIR_RISING:
		reg = OPT300x_HIGH_LIMIT;
		opt->high_thresh_mantissa = mantissa;
		opt->high_thresh_exp = exponent;
		break;
	case IIO_EV_DIR_FALLING:
		reg = OPT300x_LOW_LIMIT;
		opt->low_thresh_mantissa = mantissa;
		opt->low_thresh_exp = exponent;
		break;
	default:
		ret = -EINVAL;
		goto err;
	}

	ret = i2c_smbus_write_word_swapped(opt->client, reg, value);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n", reg);
		goto err;
	}

err:
	mutex_unlock(&opt->lock);

	return ret;
}

static int opt300x_read_event_config(struct iio_dev *iio,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir)
{
	struct opt300x *opt = iio_priv(iio);

	return opt->mode == OPT300x_CONFIGURATION_M_CONTINUOUS;
}

static int opt300x_write_event_config(struct iio_dev *iio,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir, int state)
{
	struct opt300x *opt = iio_priv(iio);
	int ret;
	u16 mode;
	u16 reg;

	if (state && opt->mode == OPT300x_CONFIGURATION_M_CONTINUOUS)
		return 0;

	if (!state && opt->mode == OPT300x_CONFIGURATION_M_SHUTDOWN)
		return 0;

	mutex_lock(&opt->lock);

	mode = state ? OPT300x_CONFIGURATION_M_CONTINUOUS
		: OPT300x_CONFIGURATION_M_SHUTDOWN;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT300x_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT300x_CONFIGURATION);
		goto err;
	}

	reg = ret;
	opt300x_set_mode(opt, &reg, mode);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT300x_CONFIGURATION,
			reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT300x_CONFIGURATION);
		goto err;
	}

err:
	mutex_unlock(&opt->lock);

	return ret;
}

static const struct iio_info opt300x_info = {
	.attrs = &opt300x_attribute_group,
	.read_raw = opt300x_read_raw,
	.write_raw = opt300x_write_raw,
	.read_event_value = opt300x_read_event_value,
	.write_event_value = opt300x_write_event_value,
	.read_event_config = opt300x_read_event_config,
	.write_event_config = opt300x_write_event_config,
};

static int opt300x_read_id(struct opt300x *opt)
{
	u16 manufacturer;
	u16 device_id;
	int ret;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT300x_MANUFACTURER_ID);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT300x_MANUFACTURER_ID);
		return ret;
	}

	manufacturer = ret;
	if (manufacturer != MANUFACTURER_ID_TI) {
		dev_err(opt->dev, "Invalid manufacturer ID (%u)\n",
				manufacturer);
		return -ENODEV;
	}
	dev_info(opt->dev, "manufacturer id (%u)\n",manufacturer);

	if (opt->chip->device_id >= 0) {
		ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_DEVICE_ID);
		if (ret < 0) {
			dev_err(opt->dev, "failed to read register %02x\n",
					OPT3001_DEVICE_ID);
			return ret;
		}
		device_id = ret;

		if (device_id != opt->chip->device_id) {
			//dev_err("opt->dev, Invalid Device id %d", device_id);
			return -ENODEV;
		}
	}

	return 0;
}

static int opt300x_configure(struct opt300x *opt)
{
	int ret;
	u16 reg;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT300x_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT300x_CONFIGURATION);
		return ret;
	}

	reg = ret;

	/* Enable automatic full-scale setting mode */
	reg &= ~OPT300x_CONFIGURATION_RN_MASK;
	reg |= OPT300x_CONFIGURATION_RN_AUTO;

	/* Reflect status of the device's integration time setting */
	if (reg & OPT300x_CONFIGURATION_CT)
		opt->int_time = OPT300x_INT_TIME_LONG;
	else
		opt->int_time = OPT300x_INT_TIME_SHORT;

	/* Ensure device is in shutdown initially */
	opt300x_set_mode(opt, &reg, OPT300x_CONFIGURATION_M_SHUTDOWN);

	/* Configure for latched window-style comparison operation */
	reg |= OPT300x_CONFIGURATION_L;
	reg &= ~OPT300x_CONFIGURATION_POL;
	reg &= ~OPT300x_CONFIGURATION_ME;
	reg &= ~OPT300x_CONFIGURATION_FC_MASK;

	ret = i2c_smbus_write_word_swapped(opt->client, OPT300x_CONFIGURATION,
			reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT300x_CONFIGURATION);
		return ret;
	}

	ret = i2c_smbus_read_word_swapped(opt->client, OPT300x_LOW_LIMIT);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT300x_LOW_LIMIT);
		return ret;
	}

	opt->low_thresh_mantissa = OPT300x_REG_MANTISSA(ret);
	opt->low_thresh_exp = OPT300x_REG_EXPONENT(ret);

	ret = i2c_smbus_read_word_swapped(opt->client, OPT300x_HIGH_LIMIT);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT300x_HIGH_LIMIT);
		return ret;
	}

	opt->high_thresh_mantissa = OPT300x_REG_MANTISSA(ret);
	opt->high_thresh_exp = OPT300x_REG_EXPONENT(ret);

	return 0;
}

static irqreturn_t opt300x_irq(int irq, void *_iio)
{
	struct iio_dev *iio = _iio;
	struct opt300x *opt = iio_priv(iio);
	int ret;

	if (!opt->ok_to_ignore_lock)
		mutex_lock(&opt->lock);

	ret = i2c_smbus_read_word_swapped(opt->client, OPT300x_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT300x_CONFIGURATION);
		goto out;
	}

	if ((ret & OPT300x_CONFIGURATION_M_MASK) ==
			OPT300x_CONFIGURATION_M_CONTINUOUS) {
		if (ret & OPT300x_CONFIGURATION_FH)
			iio_push_event(iio,
					IIO_UNMOD_EVENT_CODE(IIO_LIGHT, 0,
							IIO_EV_TYPE_THRESH,
							IIO_EV_DIR_RISING),
					iio_get_time_ns());
		if (ret & OPT300x_CONFIGURATION_FL)
			iio_push_event(iio,
					IIO_UNMOD_EVENT_CODE(IIO_LIGHT, 0,
							IIO_EV_TYPE_THRESH,
							IIO_EV_DIR_FALLING),
					iio_get_time_ns());
	} else if (ret & OPT300x_CONFIGURATION_CRF) {
		ret = i2c_smbus_read_word_swapped(opt->client, OPT300x_RESULT);
		if (ret < 0) {
			dev_err(opt->dev, "failed to read register %02x\n",
					OPT300x_RESULT);
			goto out;
		}
		opt->result = ret;
		opt->result_ready = true;
		wake_up(&opt->result_ready_queue);
	}

out:
	if (!opt->ok_to_ignore_lock)
		mutex_unlock(&opt->lock);

	return IRQ_HANDLED;
}

static int opt300x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct opt300x_chip *c = (struct opt300x_chip *)id->driver_data;
	struct iio_dev *iio;
	struct opt300x *opt;
	int ret;

	int irq = client->irq;

	iio = devm_iio_device_alloc(dev, sizeof(*opt));
	if (!iio)
		return -ENOMEM;

	opt = iio_priv(iio);
	opt->client = client;
	opt->dev = dev;
	opt->chip = c;
	mutex_init(&opt->lock);
	init_waitqueue_head(&opt->result_ready_queue);
	i2c_set_clientdata(client, iio);

	ret = opt300x_read_id(opt);
	if (ret)
		return ret;

	dev_info(dev, "Found %s\n", id->name);

	ret = opt300x_configure(opt);
	if (ret)
		return ret;

	iio->name = client->name;
	iio->channels = opt->chip->channels;
	iio->num_channels = ARRAY_SIZE(opt->chip->channels);
	iio->dev.parent = dev;
	iio->modes = INDIO_DIRECT_MODE;
	iio->info = &opt300x_info;

	ret = devm_iio_device_register(dev, iio);
	if (ret) {
		dev_err(dev, "failed to register IIO device\n");
		return ret;
	}

	/* Make use of INT pin only if valid IRQ no. is given */
	if (irq > 0) {
		ret = request_threaded_irq(irq, NULL, opt300x_irq,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"opt300x", iio);
		if (ret) {
			dev_err(dev, "failed to request IRQ #%d\n", irq);
			return ret;
		}
		opt->use_irq = true;
		dev_info(opt->dev,"enabling interrupt based operation");
	} else {
		dev_info(opt->dev, "enabling interrupt-less operation\n");
	}

	return 0;
}

static int opt300x_remove(struct i2c_client *client)
{
	struct iio_dev *iio = i2c_get_clientdata(client);
	struct opt300x *opt = iio_priv(iio);
	int ret;
	u16 reg;

	if (opt->use_irq)
		free_irq(client->irq, iio);

	ret = i2c_smbus_read_word_swapped(opt->client, OPT300x_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT300x_CONFIGURATION);
		return ret;
	}

	reg = ret;
	opt300x_set_mode(opt, &reg, OPT300x_CONFIGURATION_M_SHUTDOWN);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT300x_CONFIGURATION,
			reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT300x_CONFIGURATION);
		return ret;
	}

	return 0;
}

static const struct i2c_device_id opt300x_id[] = {
	{ "opt3001", (kernel_ulong_t)&opt3001_chip },
	{ "opt3002", (kernel_ulong_t)&opt3002_chip },
	{ } /* Terminating Entry */
};
MODULE_DEVICE_TABLE(i2c, opt300x_id);

static const struct of_device_id opt300x_of_match[] = {
	{ .compatible = "ti,opt3001" },
	{ }
};
MODULE_DEVICE_TABLE(of, opt300x_of_match);

static struct i2c_driver opt300x_driver = {
	.probe = opt300x_probe,
	.remove = opt300x_remove,
	.id_table = opt300x_id,

	.driver = {
		.name = "opt300x",
		.of_match_table = of_match_ptr(opt300x_of_match),
	},
};

module_i2c_driver(opt300x_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Andreas Dannenberg <dannenberg@ti.com>");
MODULE_DESCRIPTION("Texas Instruments OPT300x Light Sensor Driver");
