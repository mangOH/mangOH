/**\mainpage
 * Copyright (C) 2017 - 2018 Bosch Sensortec GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of the
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 * The information provided is believed to be accurate and reliable.
 * The copyright holder assumes no responsibility
 * for the consequences of use
 * of such information nor for any infringement of patents or
 * other rights of third parties which may result from its use.
 * No license is granted by implication or otherwise under any patent or
 * patent rights of the copyright holder.
 *
 * @file        bmi088-i2c.c
 * @date        02 June 2018
 * @version     1.0.0
 *
 */
/*! \file bmi088-i2c.c
 \brief Sensor Driver for BMI08x family of sensors */
/****************************************************************************/
/**\name        Header files
 ****************************************************************************/
#include <linux/device.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/i2c.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>

#include "bmi085.h"
#include "bmi088.h"
#include "bmi08x.h"
#include "bmi08x_defs.h"

#define BMI08X_ACCEL_GPIO_NAME                  "bmi088a_pin"
#define BMI08X_GYRO_GPIO_NAME                   "bmi088g_pin"
#define BMI08X_ACCEL_IRQ_NAME                   "bmi088a_event"
#define BMI08X_GYRO_IRQ_NAME                    "bmi088g_event"

#define BMI08X_ACCEL_DEFAULT_THREASHOLD_MG      10 * 2048
#define BMI08X_ACCEL_DEFAULT_DURATION           5
#define BMI08X_GYRO_CLEAR_DELAY                 480

#if defined(CONFIG_ARCH_MDM9607) /* For WP76XX */
/* TODO UPDATE: GPIO38 (ACC_SENSOR_INT1) */
#define BMI08X_INTR_ACCEL_GPIO_PIN              11       
/* GPIO25 (GYRO_SENSOR_INT2) */
#define BMI08X_INTR_GYRO_GPIO_PIN               51      
#endif

#define BMI088_CHANNELS_CONFIG(device_type, si, mod, addr, bitlen)      \
        {                                                               \
                .type = device_type,                                    \
                .modified = 1,                                          \
                .info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),     \
                .scan_index = si,                                       \
                .channel2 = mod,                                        \
                .address = addr,                                        \
                .scan_type = {                                          \
                        .sign = 's',                                    \
                        .realbits = bitlen,                             \
                        .shift = 0,                                     \
                        .storagebits = bitlen,                          \
                },                                                      \
        }

#define BMI088_TEMP_CHANNEL {                                           \
        .type = IIO_TEMP,                                               \
        .info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),             \
        .scan_index = BMI088_SCAN_TEMP,                                 \
        .scan_type = {                                                  \
                .sign = 's',                                            \
                .realbits = 32,                                         \
                .storagebits = 32,                                      \
        },                                                              \
}

enum BMI088_AXIS_SCAN {
        BMI088_SCAN_X,
        BMI088_SCAN_Y,
        BMI088_SCAN_Z,
        BMI088_SCAN_TEMP,
        BMI088_SCAN_TIMESTAMP,
};

struct bmi08x_data {
	struct mutex                    mutex;
        struct bmi08x_sensor_data       sensor_data;
        struct bmi08x_int_cfg           int_cfg;
        u8                              buff[32]; /* 3x 16-bit + 32-bit temperature + 64-bit timestamp */
        struct bmi08x_dev               *bmi08x_dev;
        struct iio_trigger              *trig;
        struct i2c_client               *client;
        struct regmap                   *regmap;
        int                             gpio_pin;
	int                             irq;
};

struct bmi08x_info {
        struct bmi08x_dev               bmi08x_dev;
        struct bmi08x_anymotion_cfg     anymotion_cfg;
        struct bmi08x_data              *accel;
        struct bmi08x_data              *gyro;
};

extern const struct regmap_config bmi088a_regmap_config;
extern const struct regmap_config bmi088g_regmap_config;

static const struct iio_chan_spec bmi088a_channels[] = {
        BMI088_CHANNELS_CONFIG(IIO_ACCEL, BMI088_SCAN_X, IIO_MOD_X, 
                BMI08X_GYRO_X_LSB_REG, 16),
        BMI088_CHANNELS_CONFIG(IIO_ACCEL, BMI088_SCAN_Y, IIO_MOD_Y, 
                BMI08X_GYRO_Y_LSB_REG, 16),
        BMI088_CHANNELS_CONFIG(IIO_ACCEL, BMI088_SCAN_Z, IIO_MOD_Z, 
                BMI08X_GYRO_Z_LSB_REG, 16),
        BMI088_TEMP_CHANNEL,
        IIO_CHAN_SOFT_TIMESTAMP(BMI088_SCAN_TIMESTAMP),
};

static const struct iio_chan_spec bmi088g_channels[] = {
        BMI088_CHANNELS_CONFIG(IIO_ANGL_VEL, BMI088_SCAN_X, IIO_MOD_X, 
                BMI08X_ACCEL_X_LSB_REG, 16),
        BMI088_CHANNELS_CONFIG(IIO_ANGL_VEL, BMI088_SCAN_Y, IIO_MOD_Y, 
                BMI08X_ACCEL_Y_LSB_REG, 16),
        BMI088_CHANNELS_CONFIG(IIO_ANGL_VEL, BMI088_SCAN_Z, IIO_MOD_Z, 
                BMI08X_ACCEL_Z_LSB_REG, 16),
        BMI088_TEMP_CHANNEL,
        IIO_CHAN_SOFT_TIMESTAMP(BMI088_SCAN_TIMESTAMP),
};

static struct bmi08x_info bmi08x_info = {0};

static int match(struct device *dev, void *data)
{
        struct i2c_client *client = to_i2c_client(dev);
        u8* dev_id = (u8*)data;
        return (client->addr == *dev_id);
}

static struct i2c_client *bmi088_get_client(u8 dev_id)
{
        struct device *dev = bus_find_device(&i2c_bus_type, NULL, &dev_id, match);
        if (!dev) 
                return NULL;

        return to_i2c_client(dev);
}

static s8 bmi088_read(u8 dev_id, u8 reg_addr, u8 *reg_data, u16 len)
{
        static struct i2c_client *client_accel = NULL;
        static struct i2c_client *client_gyro = NULL;
        struct i2c_client *client;
        struct iio_dev *indio_dev;
        struct bmi08x_data *data;
        int ret = 0;

        if (dev_id == BMI08X_ACCEL_I2C_ADDR_PRIMARY) {
                if (!client_accel) {
                        client_accel = bmi088_get_client(dev_id);
                        if (IS_ERR(client_accel)) {
                                pr_err("%s(): get client(0x%02x) failed\n", 
                                        __func__, dev_id);
                                ret = PTR_ERR(client_accel);
                                goto exit;
                        }
                }

                client = client_accel;
        }
        else if (dev_id == BMI08X_GYRO_I2C_ADDR_PRIMARY) {
                if (!client_gyro) {
                        client_gyro = bmi088_get_client(dev_id);
                        if (IS_ERR(client_gyro)) {
                                pr_err("%s(): get client(0x%02x) failed\n", 
                                        __func__, dev_id);
                                ret = PTR_ERR(client_gyro);
                                goto exit;
                        }
                }

                client = client_gyro;
        }
        else {
                pr_err("%s(): get client(0x%02x) failed\n", __func__, dev_id);
                ret = -EINVAL;
                goto exit;
        }

        indio_dev = dev_get_drvdata(&client->dev);
        data = iio_priv(indio_dev);

        ret = regmap_bulk_read(data->regmap, reg_addr, reg_data, len);
        if (ret < 0) {
                dev_err(&client->dev, "%s(): regmap_bulk_read() failed(%d)\n", 
                        __func__, ret);
                goto exit;
        }

exit:
        return ret;
}

static s8 bmi088_write(u8 dev_id, u8 reg_addr, u8 *reg_data, u16 len)
{
        static struct i2c_client *client_accel = NULL;
        static struct i2c_client *client_gyro = NULL;
        struct i2c_client *client;
        struct iio_dev *indio_dev;
        struct bmi08x_data *data;
        int ret = 0;

        if (dev_id == BMI08X_ACCEL_I2C_ADDR_PRIMARY) {
                if (!client_accel) {
                        client_accel = bmi088_get_client(dev_id);
                        if (IS_ERR(client_accel)) {
                                ret = PTR_ERR(client_accel);
                                goto exit;
                        }
                }

                client = client_accel;
        }
        else if (dev_id == BMI08X_GYRO_I2C_ADDR_PRIMARY) {
                if (!client_gyro) {
                        client_gyro = bmi088_get_client(dev_id);
                        if (IS_ERR(client_gyro)) {
                                ret = PTR_ERR(client_gyro);
                                goto exit;
                        }
                }

                client = client_gyro;
        }

        indio_dev = dev_get_drvdata(&client->dev);
        data = iio_priv(indio_dev);

        ret = regmap_bulk_write(data->regmap, reg_addr, reg_data, len);
        if (ret < 0) {
                dev_err(&client->dev, "%s(): regmap_bulk_write() failed(%d)\n", 
                        __func__, ret);
                goto exit;
        }

exit:
        return ret;
}

static int bmi088_reset_intr(struct iio_dev *indio_dev)
{
        struct bmi08x_data *data = iio_priv(indio_dev);
        int ret;
        u8 interrupt_status;

        dev_dbg(&indio_dev->dev, "%s(): I2C addr(0x%04x)\n", 
                __func__, data->client->addr);

        if (data->client->addr == BMI08X_ACCEL_I2C_ADDR_PRIMARY) {
                ret = bmi088_read(data->client->addr, BMI08X_ACCEL_INT_STAT_1_REG, 
                        &interrupt_status, sizeof(interrupt_status));
                if (ret < 0) {
                        dev_err(&indio_dev->dev, "%s(): bmi088_read() failed(%d)\n", 
                                __func__, ret);
                        goto exit;
                }
        }
        else if (data->client->addr == BMI08X_GYRO_I2C_ADDR_PRIMARY) {
                msleep_interruptible(1);
                ret = 0;
        }
        else {
                dev_err(&indio_dev->dev, "%s(): invalid I2C addr(0x%04x)\n", 
                        __func__, data->client->addr);
                ret = -EINVAL;
                goto exit;
        }

exit:
        return ret;
}

static int bmi088_set_new_data_intr_state(struct iio_dev *indio_dev, bool state)
{
        struct bmi08x_data *data = iio_priv(indio_dev);
        int ret;
        
        dev_dbg(&indio_dev->dev, "%s(): state(%u) - I2C addr(0x%04x)\n", 
                __func__, state, data->client->addr);

        if (data->client->addr == BMI08X_ACCEL_I2C_ADDR_PRIMARY) {
                /* Interrupt configurations */
                data->int_cfg.accel_int_config_1.int_channel = BMI08X_INT_CHANNEL_2;
                data->int_cfg.accel_int_config_1.int_type = BMI08X_ACCEL_DATA_RDY_INT;
                data->int_cfg.accel_int_config_1.int_pin_cfg.enable_int_pin = state;
                data->int_cfg.accel_int_config_1.int_pin_cfg.lvl = BMI08X_INT_ACTIVE_HIGH;
                data->int_cfg.accel_int_config_1.int_pin_cfg.output_mode = BMI08X_INT_MODE_PUSH_PULL;

	        /* Setting the interrupt configuration */
                ret = bmi08a_set_int_config(&data->int_cfg.accel_int_config_1, data->bmi08x_dev);
                if (ret < 0) {
		        dev_err(&data->client->dev, "%s(): bmi08a_set_int_config() failed(%d)\n", 
                                __func__, ret);
                        ret = -EINVAL;
		        goto exit;
	        }
        }
        else if (data->client->addr == BMI08X_GYRO_I2C_ADDR_PRIMARY) {
	        /* Interrupt configurations */
                data->int_cfg.gyro_int_config_1.int_channel = BMI08X_INT_CHANNEL_3;
                data->int_cfg.gyro_int_config_1.int_type = BMI08X_GYRO_DATA_RDY_INT;
                data->int_cfg.gyro_int_config_1.int_pin_cfg.enable_int_pin = state;
                data->int_cfg.gyro_int_config_1.int_pin_cfg.lvl = BMI08X_INT_ACTIVE_HIGH;
                data->int_cfg.gyro_int_config_1.int_pin_cfg.output_mode = BMI08X_INT_MODE_PUSH_PULL;

                /* Setting the interrupt configuration */
                ret = bmi08g_set_int_config(&data->int_cfg.gyro_int_config_1, data->bmi08x_dev);
	        if (ret < 0) {
		        dev_err(&indio_dev->dev, "%s(): bmi08g_set_int_config() failed(%d)\n", 
                                __func__, ret);
                        ret = -EINVAL;
		        goto exit;
	        }
        }
        else {
                dev_err(&indio_dev->dev, "%s(): invalid I2C addr(0x%04x)\n", 
                        __func__, data->client->addr);
                ret = -EINVAL;
                goto exit;
        }

        ret = bmi088_reset_intr(indio_dev);
        if (ret < 0) {
                dev_err(&indio_dev->dev, "%s(): bmi088_reset_intr() failed(%d)\n", 
                        __func__, ret);
                goto exit;
        }

exit:
        return ret;
}

static ssize_t bmi088_show_accel_x_en(struct device *dev,
                                      struct device_attribute *attr, char *buf)
{
        struct bmi08x_data *data = iio_priv(dev_to_iio_dev(dev));
        ssize_t ret;

        mutex_lock(&data->mutex);
        ret = sprintf(buf, "%u\n", bmi08x_info.anymotion_cfg.x_en);
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_store_accel_x_en(struct device *dev,
		                       struct device_attribute *attr,
		                       const char *buf, size_t len)
{
        struct iio_dev *indio_dev = dev_to_iio_dev(dev);
        struct bmi08x_data *data = iio_priv(indio_dev);
        u8 val;
	int ret;

        ret = kstrtou8(buf, 10, &val);
	if (ret < 0)
		return ret;

        mutex_lock(&data->mutex);

        bmi08x_info.anymotion_cfg.x_en = val ? 1:0;
        dev_dbg(dev, "%s(): accel-x enable(%u)\n", 
                __func__, bmi08x_info.anymotion_cfg.x_en);

        ret = bmi088_configure_anymotion(bmi08x_info.anymotion_cfg, data->bmi08x_dev);
        if (ret < 0) {
		dev_err(dev, "%s(): bmi088_configure_anymotion() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
		goto exit;
	}

        ret = len;

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_show_accel_y_en(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        struct bmi08x_data *data = iio_priv(dev_to_iio_dev(dev));
        ssize_t ret;

        mutex_lock(&data->mutex);
        ret = sprintf(buf, "%u\n", bmi08x_info.anymotion_cfg.y_en);
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_store_accel_y_en(struct device *dev,
		                struct device_attribute *attr,
		                const char *buf, size_t len)
{
        struct iio_dev *indio_dev = dev_to_iio_dev(dev);
        struct bmi08x_data *data = iio_priv(indio_dev);
        u8 val;
	int ret;

        ret = kstrtou8(buf, 10, &val);
	if (ret < 0)
		return ret;

        mutex_lock(&data->mutex);

        bmi08x_info.anymotion_cfg.y_en = val ? 1:0;
        dev_dbg(dev, "%s(): accel-y enable(%u)\n", 
                __func__, bmi08x_info.anymotion_cfg.y_en);

        ret = bmi088_configure_anymotion(bmi08x_info.anymotion_cfg, data->bmi08x_dev);
        if (ret < 0) {
		dev_err(dev, "%s(): bmi088_configure_anymotion() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
		goto exit;
	}

        ret = len;

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_show_accel_z_en(struct device *dev,
                                      struct device_attribute *attr, char *buf)
{
        struct bmi08x_data *data = iio_priv(dev_to_iio_dev(dev));
        ssize_t ret;

        mutex_lock(&data->mutex);
        ret = sprintf(buf, "%u\n", bmi08x_info.anymotion_cfg.z_en);
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_store_accel_z_en(struct device *dev,
		                       struct device_attribute *attr,
		                       const char *buf, size_t len)
{
        struct iio_dev *indio_dev = dev_to_iio_dev(dev);
        struct bmi08x_data *data = iio_priv(indio_dev);
        u8 val;
	int ret;

        ret = kstrtou8(buf, 10, &val);
	if (ret < 0)
		return ret;

        mutex_lock(&data->mutex);

        bmi08x_info.anymotion_cfg.z_en = val ? 1:0;
        dev_dbg(dev, "%s(): accel-z enable(%u)\n", 
                __func__, bmi08x_info.anymotion_cfg.z_en);

        ret = bmi088_configure_anymotion(bmi08x_info.anymotion_cfg, data->bmi08x_dev);
        if (ret < 0) {
		dev_err(dev, "%s(): bmi088_configure_anymotion() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
		goto exit;
	}

        ret = len;

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_show_accel_threshold(struct device *dev,
                                           struct device_attribute *attr, char *buf)
{
        struct bmi08x_data *data = iio_priv(dev_to_iio_dev(dev));
        ssize_t ret;

        mutex_lock(&data->mutex);
        ret = sprintf(buf, "%u\n", bmi08x_info.anymotion_cfg.threshold / 2048);
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_store_accel_threshold(struct device *dev,
		                            struct device_attribute *attr,
		                            const char *buf, size_t len)
{
        struct iio_dev *indio_dev = dev_to_iio_dev(dev);
        struct bmi08x_data *data = iio_priv(indio_dev);
        u16 val;
	int ret;

        ret = kstrtou16(buf, 10, &val);
	if (ret < 0)
		return ret;

        mutex_lock(&data->mutex);

        /* 
         * 11 bit threshold of anymotion detection 
         * (threshold = X mg * 2,048 (5.11 format))
         */
        bmi08x_info.anymotion_cfg.threshold = val * 2048;
        dev_dbg(dev, "%s(): accel threshold(%u)\n", 
                __func__, bmi08x_info.anymotion_cfg.threshold / 2048);

        ret = bmi088_configure_anymotion(bmi08x_info.anymotion_cfg, data->bmi08x_dev);
        if (ret < 0) {
		dev_err(dev, "%s(): bmi088_configure_anymotion() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
		goto exit;
	}

        ret = len;

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_show_accel_nomotion_sel(struct device *dev,
                                              struct device_attribute *attr, char *buf)
{
        struct bmi08x_data *data = iio_priv(dev_to_iio_dev(dev));
        ssize_t ret;

        mutex_lock(&data->mutex);
        ret = sprintf(buf, "%u\n", bmi08x_info.anymotion_cfg.nomotion_sel);
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_store_accel_nomotion_sel(struct device *dev,
		                               struct device_attribute *attr,
		                               const char *buf, size_t len)
{
        struct iio_dev *indio_dev = dev_to_iio_dev(dev);
        struct bmi08x_data *data = iio_priv(indio_dev);
        u8 val;
	int ret;

        ret = kstrtou8(buf, 10, &val);
	if (ret < 0)
		return ret;

        mutex_lock(&data->mutex);

        /* 1 bit select between any- and nomotion behavior */
        bmi08x_info.anymotion_cfg.nomotion_sel = val ? 1:0;
        dev_dbg(dev, "%s(): accel nomotion sel(%u)\n", 
                __func__, bmi08x_info.anymotion_cfg.nomotion_sel);

        ret = bmi088_configure_anymotion(bmi08x_info.anymotion_cfg, data->bmi08x_dev);
        if (ret < 0) {
		dev_err(dev, "%s(): bmi088_configure_anymotion() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
		goto exit;
	}

        ret = len;

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_show_accel_duration(struct device *dev,
                                          struct device_attribute *attr, char *buf)
{
        struct bmi08x_data *data = iio_priv(dev_to_iio_dev(dev));
        ssize_t ret;

        mutex_lock(&data->mutex);
        ret = sprintf(buf, "%u\n", bmi08x_info.anymotion_cfg.duration);
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_store_accel_duration(struct device *dev,
		                           struct device_attribute *attr,
		                           const char *buf, size_t len)
{
        struct iio_dev *indio_dev = dev_to_iio_dev(dev);
        struct bmi08x_data *data = iio_priv(indio_dev);
        u16 val;
	int ret;

        ret = kstrtou16(buf, 10, &val);
	if (ret < 0)
		return ret;

        mutex_lock(&data->mutex);

        /* 
         * 13 bit set the duration for any- and nomotion 
         * (time = duration * 20ms (@50Hz)) 
         */
        bmi08x_info.anymotion_cfg.duration = val;
        dev_dbg(dev, "%s(): accel duration(%u)\n", 
                __func__, bmi08x_info.anymotion_cfg.duration);

        ret = bmi088_configure_anymotion(bmi08x_info.anymotion_cfg, data->bmi08x_dev);
        if (ret < 0) {
		dev_err(dev, "%s(): bmi088_configure_anymotion() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
		goto exit;
	}

        ret = len;

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_show_accel_range(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
        struct bmi08x_data *data = iio_priv(dev_to_iio_dev(dev));
        ssize_t ret;

        mutex_lock(&data->mutex);

        ret = bmi08a_get_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08a_get_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        ret = sprintf(buf, "%u\n", data->bmi08x_dev->accel_cfg.range);

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_store_accel_range(struct device *dev,
		                        struct device_attribute *attr,
		                        const char *buf, size_t len)
{
        struct iio_dev *indio_dev = dev_to_iio_dev(dev);
        struct bmi08x_data *data = iio_priv(indio_dev);
	int ret;
        u8 val;

        ret = kstrtou8(buf, 10, &val);
	if (ret < 0)
		return ret;

        mutex_lock(&data->mutex);

        ret = bmi08a_get_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08a_get_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        data->bmi08x_dev->accel_cfg.range = val;

        ret = bmi08a_set_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08a_set_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        ret = len;

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_show_accel_odr(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
        struct bmi08x_data *data = iio_priv(dev_to_iio_dev(dev));
        ssize_t ret;

        mutex_lock(&data->mutex);

        ret = bmi08a_get_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08a_get_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        ret = sprintf(buf, "%u\n", data->bmi08x_dev->accel_cfg.odr);

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_store_accel_odr(struct device *dev,
		                      struct device_attribute *attr,
		                      const char *buf, size_t len)
{
        struct iio_dev *indio_dev = dev_to_iio_dev(dev);
        struct bmi08x_data *data = iio_priv(indio_dev);
	int ret;
        u8 val;

        ret = kstrtou8(buf, 10, &val);
	if (ret < 0)
		return ret;

        mutex_lock(&data->mutex);

        ret = bmi08a_get_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08a_get_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        data->bmi08x_dev->accel_cfg.odr = val;

        ret = bmi08a_set_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08a_set_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        ret = len;

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_show_accel_bwp(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
        struct bmi08x_data *data = iio_priv(dev_to_iio_dev(dev));
        ssize_t ret;

        mutex_lock(&data->mutex);

        ret = bmi08a_get_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08a_get_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        ret = sprintf(buf, "%u\n", data->bmi08x_dev->accel_cfg.bw);

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_store_accel_bwp(struct device *dev,
		                      struct device_attribute *attr,
		                      const char *buf, size_t len)
{
        struct iio_dev *indio_dev = dev_to_iio_dev(dev);
        struct bmi08x_data *data = iio_priv(indio_dev);
	int ret;
        u8 val;

        ret = kstrtou8(buf, 10, &val);
	if (ret < 0)
		return ret;
        
        mutex_lock(&data->mutex);

        ret = bmi08a_get_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08a_get_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        data->bmi08x_dev->accel_cfg.bw = val;

        ret = bmi08a_set_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08a_set_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        ret = len;

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_show_gyro_range(struct device *dev,
                                      struct device_attribute *attr, char *buf)
{
        struct bmi08x_data *data = iio_priv(dev_to_iio_dev(dev));
        ssize_t ret;

        mutex_lock(&data->mutex);

        ret = bmi08g_get_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08g_get_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        ret = sprintf(buf, "%u\n", data->bmi08x_dev->gyro_cfg.range);

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_store_gyro_range(struct device *dev,
		                       struct device_attribute *attr,
		                       const char *buf, size_t len)
{
        struct iio_dev *indio_dev = dev_to_iio_dev(dev);
        struct bmi08x_data *data = iio_priv(indio_dev);
	int ret;
        u8 val;

        ret = kstrtou8(buf, 10, &val);
	if (ret < 0)
		return ret;

        mutex_lock(&data->mutex);

        ret = bmi08g_get_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08g_get_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        data->bmi08x_dev->gyro_cfg.range = val;

        ret = bmi08g_set_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08g_set_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        ret = len;

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_show_gyro_odr(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
        struct bmi08x_data *data = iio_priv(dev_to_iio_dev(dev));
        ssize_t ret;

        mutex_lock(&data->mutex);

        ret = bmi08g_get_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08g_get_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        ret = sprintf(buf, "%u\n", data->bmi08x_dev->gyro_cfg.odr);

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static ssize_t bmi088_store_gyro_odr(struct device *dev,
		                        struct device_attribute *attr,
		                        const char *buf, size_t len)
{
        struct iio_dev *indio_dev = dev_to_iio_dev(dev);
        struct bmi08x_data *data = iio_priv(indio_dev);
	int ret;
        u8 val;

        ret = kstrtou8(buf, 10, &val);
	if (ret < 0)
		return ret;

        mutex_lock(&data->mutex);

        ret = bmi08g_get_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08g_get_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        data->bmi08x_dev->gyro_cfg.odr = val;

        ret = bmi08g_set_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
                dev_err(dev, "%s(): bmi08g_set_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
                goto exit;
        }

        ret = len;

exit:
        mutex_unlock(&data->mutex);
        return ret;
}

static IIO_DEVICE_ATTR(in_accel_x_en,
        S_IRUSR | S_IWUSR, bmi088_show_accel_x_en, bmi088_store_accel_x_en, 0);
static IIO_DEVICE_ATTR(in_accel_y_en,
        S_IRUSR | S_IWUSR, bmi088_show_accel_y_en, bmi088_store_accel_y_en, 0);
static IIO_DEVICE_ATTR(in_accel_z_en,
        S_IRUSR | S_IWUSR, bmi088_show_accel_z_en, bmi088_store_accel_z_en, 0);
static IIO_DEVICE_ATTR(in_accel_threshold,
        S_IRUSR | S_IWUSR, bmi088_show_accel_threshold, bmi088_store_accel_threshold, 0);
static IIO_DEVICE_ATTR(in_accel_nomotion_sel,
        S_IRUSR | S_IWUSR, bmi088_show_accel_nomotion_sel, bmi088_store_accel_nomotion_sel, 0);
static IIO_DEVICE_ATTR(in_accel_duration,
        S_IRUSR | S_IWUSR, bmi088_show_accel_duration, bmi088_store_accel_duration, 0);
static IIO_DEVICE_ATTR(in_accel_range,
        S_IRUSR | S_IWUSR, bmi088_show_accel_range, bmi088_store_accel_range, 0);
static IIO_DEVICE_ATTR(in_accel_odr,
        S_IRUSR | S_IWUSR, bmi088_show_accel_odr, bmi088_store_accel_odr, 0);
static IIO_DEVICE_ATTR(in_accel_bwp,
        S_IRUSR | S_IWUSR, bmi088_show_accel_bwp, bmi088_store_accel_bwp, 0);
static IIO_DEVICE_ATTR(in_gyro_range,
        S_IRUSR | S_IWUSR, bmi088_show_gyro_range, bmi088_store_gyro_range, 0);
static IIO_DEVICE_ATTR(in_gyro_odr,
        S_IRUSR | S_IWUSR, bmi088_show_gyro_odr, bmi088_store_gyro_odr, 0);

static struct attribute *bmi088a_attributes[] = {
        &iio_dev_attr_in_accel_x_en.dev_attr.attr,
        &iio_dev_attr_in_accel_y_en.dev_attr.attr,
        &iio_dev_attr_in_accel_z_en.dev_attr.attr,
        &iio_dev_attr_in_accel_threshold.dev_attr.attr,
        &iio_dev_attr_in_accel_nomotion_sel.dev_attr.attr,
        &iio_dev_attr_in_accel_duration.dev_attr.attr,
        &iio_dev_attr_in_accel_range.dev_attr.attr,
        &iio_dev_attr_in_accel_odr.dev_attr.attr,
        &iio_dev_attr_in_accel_bwp.dev_attr.attr,
        &iio_dev_attr_in_gyro_range.dev_attr.attr,
        &iio_dev_attr_in_gyro_odr.dev_attr.attr,
        NULL,
};

static const struct attribute_group bmi088a_attrs_group = {
        .attrs = bmi088a_attributes,
};

static void bmi088_delay_milli_sec(u32 period)
{
        /*
         * Return control or wait,
         * for a period amount of milliseconds
         */
        msleep_interruptible(period);
}

static int bmi088_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct bmi08x_data *data = iio_priv(indio_dev);
        s32 temperature;
        int ret;
        u8 range;

	mutex_lock(&data->mutex);

        switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
        {
                if (iio_buffer_enabled(indio_dev)) {
                        dev_warn(&indio_dev->dev, "%s(): IIO buffer enabled\n", __func__);
                        ret = -EBUSY;
                        goto exit;
                }

                switch (chan->type) {
                case IIO_TEMP:
                        ret = bmi08a_get_sensor_temperature(data->bmi08x_dev, &temperature);
                        if (ret < 0) {
                                dev_err(&indio_dev->dev, "%s(): bmi08a_get_sensor_temperature() failed(%d)\n", 
                                        __func__, ret);
                                goto exit;
                        }

                        *val = temperature;
                        *val2 = 1000;
                        ret = IIO_VAL_FRACTIONAL;
                        break;

                case IIO_ACCEL:
                        ret = bmi088_read(data->client->addr, BMI08X_ACCEL_RANGE_REG, 
                                &range, sizeof(range));
                        if (ret < 0) {
                                dev_err(&indio_dev->dev, "%s(): bmi088_read() failed(%d)\n", 
                                        __func__, ret);
                                goto exit;
                        }

                        ret = bmi08a_get_data(&data->sensor_data, data->bmi08x_dev);
                        if (ret < 0) {
                                dev_err(&indio_dev->dev, "%s(): bmi08a_get_data() failed(%d)\n", 
                                        __func__, ret);
                                goto exit;
                        }

                        data->sensor_data.x = (data->sensor_data.x * 1000) / 32768 * 2^(range + 1);
                        data->sensor_data.y = (data->sensor_data.y * 1000) / 32768 * 2^(range + 1);
                        data->sensor_data.z = (data->sensor_data.z * 1000) / 32768 * 2^(range + 1);

                        switch (chan->scan_index) {
                                ret = bmi088_read(data->client->addr, BMI08X_GYRO_RANGE_REG, 
                                        &range, sizeof(range));
                                if (ret < 0) {
                                        dev_err(&indio_dev->dev, "%s(): bmi088_read() failed(%d)\n", 
                                                __func__, ret);
                                        goto exit;
                                }

                                ret = bmi08g_get_data(&data->sensor_data, data->bmi08x_dev);
                                if (ret < 0) {
                                        dev_err(&indio_dev->dev, "%s(): bmi08g_get_data() failed(%d)\n", 
                                                __func__, ret);
                                        goto exit;
                                }

                                data->sensor_data.x = (data->sensor_data.x / 32767) * (2000 / 2^range);
                                data->sensor_data.y = (data->sensor_data.y / 32767) * (2000 / 2^range);
                                data->sensor_data.z = (data->sensor_data.z / 32767) * (2000 / 2^range);

                                dev_dbg(&indio_dev->dev, "%s(): (x, y, z) degrees/sec: (%d, %d, %d)\n", 
                                        __func__, data->sensor_data.x, data->sensor_data.y, data->sensor_data.z);

                        case BMI088_SCAN_X:
                                *val = data->sensor_data.x;
                                ret = IIO_VAL_INT;
                                break;

                        case BMI088_SCAN_Y:
                                *val = data->sensor_data.y;
                                ret = IIO_VAL_INT;
                                break;

                        case BMI088_SCAN_Z:
                                *val = data->sensor_data.z;
                                ret = IIO_VAL_INT;
                                break;

                        default:
                                dev_err(&indio_dev->dev, "%s(): unsupported axis(%d)\n", 
                                        __func__, chan->scan_index);
			        ret = -EINVAL;
			        goto exit;
                        }

                        break;

                case IIO_ANGL_VEL:
                        switch (chan->scan_index) {
                        case BMI088_SCAN_X:
                                *val = data->sensor_data.x;
                                ret = IIO_VAL_INT;
                                break;

                        case BMI088_SCAN_Y:
                                *val = data->sensor_data.y;
                                ret = IIO_VAL_INT;
                                break;

                        case BMI088_SCAN_Z:
                                *val = data->sensor_data.z;
                                ret = IIO_VAL_INT;
                                break;

                        default:
                                dev_err(&indio_dev->dev, "%s(): unsupported axis(%d)\n", 
                                        __func__, chan->scan_index);
			        ret = -EINVAL;
			        goto exit;
                        }

                        break;

		default:
                        dev_err(&indio_dev->dev, "%s(): unsupported channel type(%d)\n", 
                                __func__, chan->type);
			ret = -EINVAL;
			goto exit;
                }

                break;
        }
	default:
                dev_err(&indio_dev->dev, "%s(): unsupported mask(%ld)\n", 
                        __func__, mask);
		ret = -EINVAL;
		goto exit;
        }

exit:
        mutex_unlock(&data->mutex);
	return ret;
}

static int bmi088_write_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int val, int val2, long mask)
{
        dev_err(&indio_dev->dev, "%s(): unsupported\n", __func__);
	return -EINVAL;
}

static irqreturn_t bmi088_trigger_handler(int irq, void *p)
{
        struct iio_poll_func *pf = p;
        struct iio_dev *indio_dev = pf->indio_dev;
        struct bmi08x_data *data = iio_priv(indio_dev);
        int64_t time_ns = iio_get_time_ns();
        int ret;
        u8 range;

        mutex_lock(&data->mutex);

        dev_dbg(&indio_dev->dev, "%s(): I2C addr(0x%04x)\n", 
                __func__, data->client->addr);

        if (data->client->addr == BMI08X_ACCEL_I2C_ADDR_PRIMARY) {
                s32 sensor_temperature;
                u32 sensor_time;
                u64 time;

                ret = bmi088_read(data->client->addr, BMI08X_ACCEL_RANGE_REG, 
                        &range, sizeof(range));
                if (ret < 0) {
                        dev_err(&indio_dev->dev, "%s(): bmi088_read() failed(%d)\n", 
                                __func__, ret);
                        goto err;
                }

                ret = bmi08a_get_data(&data->sensor_data, data->bmi08x_dev);
                if (ret < 0) {
                        dev_err(&indio_dev->dev, "%s(): bmi08a_get_data() failed(%d)\n", 
                                __func__, ret);
                        goto err;
                }

                ret = bmi08a_get_sensor_temperature(data->bmi08x_dev, &sensor_temperature);
                if (ret < 0) {
                        dev_err(&indio_dev->dev, "%s(): bmi08a_get_sensor_temperature() failed(%d)\n", 
                                __func__, ret);
                        goto err;
                }

                ret = bmi08a_get_sensor_time(data->bmi08x_dev, &sensor_time);
                if (ret < 0) {
                        dev_err(&indio_dev->dev, "%s(): bmi08a_get_sensor_time() failed(%d)\n", 
                                __func__, ret);
                        goto err;
                }

                ((s16 *)data->buff)[0] = (data->sensor_data.x * 1000) / 32768 * 2^(range + 1);
                ((s16 *)data->buff)[1] = (data->sensor_data.y * 1000) / 32768 * 2^(range + 1);
                ((s16 *)data->buff)[2] = (data->sensor_data.z * 1000) / 32768 * 2^(range + 1);
                memcpy(&(((s16 *)data->buff)[3]), &sensor_temperature, sizeof(sensor_temperature));
                time = sensor_time;
                memcpy(&(((s16 *)data->buff)[5]), &time, sizeof(time));

                dev_dbg(&indio_dev->dev, "%s(): t(%u) %d.%u degC (x, y, z) cm/s^2: (%d, %d, %d)\n", 
                        __func__, sensor_time, sensor_temperature / 1000, 
                        (sensor_temperature > 0) ? (sensor_temperature % 1000) : (-sensor_temperature % 1000),
                        data->sensor_data.x, data->sensor_data.y, data->sensor_data.z);
        }
        else if (data->client->addr == BMI08X_GYRO_I2C_ADDR_PRIMARY) {
                ret = bmi088_read(data->client->addr, BMI08X_GYRO_RANGE_REG, 
                        &range, sizeof(range));
                if (ret < 0) {
                        dev_err(&indio_dev->dev, "%s(): bmi088_read() failed(%d)\n", 
                                __func__, ret);
                        goto err;
                }

                ret = bmi08g_get_data(&data->sensor_data, data->bmi08x_dev);
                if (ret < 0) {
                        dev_err(&indio_dev->dev, "%s(): bmi08g_get_data() failed(%d)\n", 
                                __func__, ret);
                        goto err;
                }

                ((s16 *)data->buff)[0] = (data->sensor_data.x / 32767) * (2000 / 2^range);
                ((s16 *)data->buff)[1] = (data->sensor_data.y / 32767) * (2000 / 2^range);
                ((s16 *)data->buff)[2] = (data->sensor_data.z / 32767) * (2000 / 2^range);

                dev_dbg(&indio_dev->dev, "%s(): (x, y, z) degrees/sec: (%d, %d, %d)\n", 
                        __func__, data->sensor_data.x, data->sensor_data.y, data->sensor_data.z);
        }
        else {
                dev_err(&indio_dev->dev, "%s(): unsupported I2C addr(0x%04x)\n", 
                        __func__, data->client->addr);
                goto err;
        }

err:
        mutex_unlock(&data->mutex);
        iio_push_to_buffers_with_timestamp(indio_dev, &data->buff, time_ns);
        
        iio_trigger_notify_done(indio_dev->trig);
        return IRQ_HANDLED;
}

static int bmi088_config_accel(struct i2c_client *client)
{
        struct iio_dev *indio_dev = i2c_get_clientdata(client);
        struct bmi08x_data *data = iio_priv(indio_dev);
        int ret;

        /* Assign the desired configurations */
        data->bmi08x_dev->accel_cfg.bw = BMI08X_ACCEL_BW_NORMAL;
        data->bmi08x_dev->accel_cfg.odr = BMI08X_ACCEL_ODR_100_HZ;
        data->bmi08x_dev->accel_cfg.range = BMI088_ACCEL_RANGE_6G;
        data->bmi08x_dev->accel_cfg.power = BMI08X_ACCEL_PM_ACTIVE;

        ret = bmi08a_set_power_mode(data->bmi08x_dev);
        /* 
         * Wait for 10ms to switch between the power modes - 
         * delay taken care inside the function
         */
        if (ret < 0) {
		dev_err(&client->dev, "%s(): bmi08a_set_power_mode() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
		goto exit;
	}

        ret = bmi08a_set_meas_conf(data->bmi08x_dev);
        if (ret < 0) {
		dev_err(&client->dev, "%s(): bmi08a_set_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

static int bmi088_config_gyro(struct i2c_client *client)
{
        struct iio_dev *indio_dev = i2c_get_clientdata(client);
        struct bmi08x_data *data = iio_priv(indio_dev);
        int ret;

        data->bmi08x_dev->gyro_cfg.power = BMI08X_GYRO_PM_NORMAL;

        ret = bmi08g_set_power_mode(data->bmi08x_dev);
        /* 
         * Wait for 30ms to switch between the power modes - 
         * delay taken care inside the function
         */
	if (ret < 0) {
		dev_err(&client->dev, "%s(): bmi08g_set_power_mode() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
		goto exit;
	}

        /* Assign the desired configurations */
        data->bmi08x_dev->gyro_cfg.odr = BMI08X_GYRO_BW_23_ODR_200_HZ;
        data->bmi08x_dev->gyro_cfg.range = BMI08X_GYRO_RANGE_1000_DPS;
        data->bmi08x_dev->gyro_cfg.bw = BMI08X_GYRO_BW_23_ODR_200_HZ;

        ret = bmi08g_set_meas_conf(data->bmi08x_dev);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): bmi08g_set_meas_conf() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

static int bmi088_data_rdy_trigger_set_state(struct iio_trigger *trig,
                bool state)
{
        struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
        return bmi088_set_new_data_intr_state(indio_dev, state);
}

static int bmi088_trig_try_reen(struct iio_trigger *trig)
{
        struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
        return bmi088_reset_intr(indio_dev);
}

static const struct iio_info bmi088a_info = {
	.driver_module = THIS_MODULE,
        .attrs = &bmi088a_attrs_group,
	.read_raw = &bmi088_read_raw,
	.write_raw = &bmi088_write_raw,
};

static const struct iio_info bmi088g_info = {
	.driver_module = THIS_MODULE,
        .read_raw = &bmi088_read_raw,
	.write_raw = &bmi088_write_raw,
};

static const struct iio_trigger_ops bmi088_trigger_ops = {
        .set_trigger_state = bmi088_data_rdy_trigger_set_state,
        .try_reenable = bmi088_trig_try_reen,
        .owner = THIS_MODULE,
};

static int bmi088_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int ret;
	struct iio_dev *indio_dev;
        struct bmi08x_data *data;

        dev_info(&client->dev, "%s(): probe addr(0x%04x) chip ID(0x%02lx)\n", 
                __func__, client->addr, id->driver_data);
        if ((id->driver_data != BMI08X_ACCEL_CHIP_ID) && 
            (id->driver_data != BMI08X_GYRO_CHIP_ID)) {
                dev_err(&client->dev, "%s(): invalid chip ID(0x%lx)\n", 
                        __func__, id->driver_data);
                ret = -EINVAL;
		goto exit;
        }

        indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (IS_ERR(indio_dev)) {
                dev_err(&client->dev, "%s(): devm_iio_device_alloc() failed(%d)\n", 
                        __func__, ret);
		ret = PTR_ERR(indio_dev);
                goto exit;
        }

	data = iio_priv(indio_dev);
        i2c_set_clientdata(client, indio_dev);
	mutex_init(&data->mutex);
        data->client = client;
        data->bmi08x_dev = &bmi08x_info.bmi08x_dev;

        data->regmap = devm_regmap_init_i2c(client, 
                (id->driver_data == BMI08X_ACCEL_CHIP_ID) ? 
                        &bmi088a_regmap_config:&bmi088g_regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(&client->dev, "%s(): devm_regmap_init_i2c() failed(%d)\n", 
                        __func__, ret);
		ret = PTR_ERR(data->regmap);
                goto exit;
	}

	indio_dev->dev.parent = &client->dev;
	indio_dev->name = id->name;
	indio_dev->channels = (id->driver_data == BMI08X_ACCEL_CHIP_ID) ? 
                bmi088a_channels:bmi088g_channels;
	indio_dev->info = (id->driver_data == BMI08X_ACCEL_CHIP_ID) ?
                &bmi088a_info : &bmi088g_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->num_channels = (id->driver_data == BMI08X_ACCEL_CHIP_ID) ? 
                ARRAY_SIZE(bmi088a_channels):ARRAY_SIZE(bmi088g_channels);

        dev_set_drvdata(&client->dev, indio_dev);

        data->trig = iio_trigger_alloc("%s-dev%d", indio_dev->name, indio_dev->id);
        if (IS_ERR(data->trig)) {
                dev_err(&client->dev, "%s(): iio_trigger_alloc() failed\n", __func__);
                ret = -ENOMEM;
                goto exit;
        }

        if (id->driver_data == BMI08X_ACCEL_CHIP_ID) {
                bmi08x_info.accel = data;

                /* Reset the accelerometer */
	        ret = bmi08a_soft_reset(data->bmi08x_dev);
                /* Wait for 1 ms - delay taken care inside the function */
                if (ret < 0) {
		        dev_err(&client->dev, "%s(): bmi08a_soft_reset() failed(%d)\n", 
                                __func__, ret);
                        ret = -EINVAL;
		        goto err_trigger_free;
	        }

                ret = bmi08a_init(data->bmi08x_dev);
                if (ret < 0) {
		        dev_err(&client->dev, "%s(): bmi08a_init() failed(%d)\n", 
                                __func__, ret);
                        ret = -EINVAL;
		        goto err_trigger_free;
	        }

                ret = bmi088_config_accel(client);
                if (ret < 0) {
		        dev_err(&client->dev, "%s(): bmi088_config_accel() failed(%d)\n", 
                                __func__, ret);
                        ret = -EINVAL;
		        goto err_trigger_free;
	        }    
        }
        else if (id->driver_data == BMI08X_GYRO_CHIP_ID) {
                bmi08x_info.gyro = data;

                /* Reset the gyro */
	        ret = bmi08g_soft_reset(data->bmi08x_dev);
                /* Wait for 1 ms - delay taken care inside the function */
                if (ret < 0) {
		        dev_err(&client->dev, "%s(): bmi08g_soft_reset() failed(%d)\n", 
                                __func__, ret);
                        ret = -EINVAL;
		        goto err_trigger_free;
	        }

                ret = bmi08g_init(data->bmi08x_dev);
                if (ret < 0) {
		        dev_err(&client->dev, "%s(): bmi08g_init() failed(%d)\n", 
                                __func__, ret);
                        ret = -EINVAL;
		        goto err_trigger_free;
	        }
                
                ret = bmi088_config_gyro(client);
                if (ret < 0) {
		        dev_err(&client->dev, "%s(): bmi088_config_gyro() failed(%d)\n", 
                                __func__, ret);
                        ret = -EINVAL;
		        goto err_trigger_free;
	        }
        }
        else {
                dev_err(&client->dev, "%s(): unsupported chip ID(0x%02lx)",
		        __func__, id->driver_data);
                ret = -EINVAL;
		goto err_trigger_free;
	}

        /* Setting the interrupt configuration */
        ret = bmi088_set_new_data_intr_state(indio_dev, false);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): bmi088_set_new_data_intr_state() failed(%d)\n", 
                        __func__, ret);
                ret = -EINVAL;
		goto err_trigger_free;
	}

        /* Configure controller port pin for the interrupt and assign ISR */
	data->gpio_pin = (id->driver_data == BMI08X_ACCEL_CHIP_ID) ?
                BMI08X_INTR_ACCEL_GPIO_PIN : BMI08X_INTR_GYRO_GPIO_PIN;
	ret = gpio_request(data->gpio_pin, 
                (id->driver_data == BMI08X_ACCEL_CHIP_ID) ? 
                        BMI08X_ACCEL_GPIO_NAME : BMI08X_GYRO_GPIO_NAME);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): gpio_request() failed(%d)",
			__func__, ret);
		goto err_trigger_free;
	}

	gpio_direction_input(data->gpio_pin);
	data->irq = gpio_to_irq(data->gpio_pin);

	dev_info(&client->dev, "%s(): request irq GPIO(%d)\n", 
                __func__, data->gpio_pin);
	ret = devm_request_irq(&client->dev, data->irq, 
                iio_trigger_generic_data_rdy_poll, IRQF_TRIGGER_RISING, 
                (id->driver_data == BMI08X_ACCEL_CHIP_ID) ? 
                        BMI08X_ACCEL_IRQ_NAME : BMI08X_GYRO_IRQ_NAME, data->trig);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): devm_request_irq() failed(%d)",
		        __func__, ret);
		goto err_trigger_free;
	}

        data->trig->dev.parent = &client->dev;
        data->trig->ops = &bmi088_trigger_ops;
        iio_trigger_set_drvdata(data->trig, indio_dev);
        indio_dev->trig = iio_trigger_get(data->trig);

        ret = iio_trigger_register(data->trig);
        if (ret) {
                dev_err(&client->dev, "%s(): iio_trigger_register() failed(%d)\n", 
                        __func__, ret);
                goto failed_gpio_req;
        }

        ret = iio_triggered_buffer_setup(indio_dev, NULL,
                bmi088_trigger_handler, NULL);
        if (ret < 0) {
                dev_err(&client->dev, "%s(): iio_triggered_buffer_setup() failed(%d)\n", 
                        __func__, ret);
                goto err_trigger_unregister;
        }

	ret = iio_device_register(indio_dev);
        if (ret) {
		dev_err(&client->dev, "%s(): iio_device_register() failed(%d)\n", 
                        __func__, ret);
		goto err_buffer_cleanup;
	}

        return 0;

err_buffer_cleanup:
        iio_triggered_buffer_cleanup(indio_dev);

err_trigger_unregister:
        if (data->trig)
                iio_trigger_unregister(data->trig);

failed_gpio_req:
	if (data->gpio_pin > 0) 
                gpio_free(data->gpio_pin);

err_trigger_free:
        iio_trigger_free(data->trig);
exit:
	return ret;
}

static int bmi088_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = dev_get_drvdata(&client->dev);
        struct bmi08x_data *data = iio_priv(indio_dev);
        int ret;

        dev_info(&client->dev, "%s(): remove('%s') addr(0x%04x)\n", 
                __func__, indio_dev->name, client->addr);

        if (client->addr == BMI08X_ACCEL_I2C_ADDR_PRIMARY) {
                data->int_cfg.accel_int_config_1.int_pin_cfg.enable_int_pin = BMI08X_DISABLE;
                ret = bmi08a_set_int_config(&data->int_cfg.accel_int_config_1, data->bmi08x_dev);
                if (ret < 0) {
		        dev_err(&client->dev, "%s(): bmi08a_set_int_config() failed(%d)\n", 
                                __func__, ret);
                        ret = -EINVAL;
                        goto exit;
	        }

                data->bmi08x_dev->accel_cfg.power = BMI08X_ACCEL_PM_SUSPEND;
                ret = bmi08a_set_power_mode(data->bmi08x_dev);
                /* 
                 * Wait for 10ms to switch between the power modes - 
                 * delay taken care inside the function
                 */
                if (ret < 0) {
		        dev_err(&client->dev, "%s(): bmi08a_set_power_mode() failed(%d)\n", 
                                __func__, ret);
                        ret = -EINVAL;
                        goto exit;
                }
        }
        else {
                data->int_cfg.gyro_int_config_1.int_pin_cfg.enable_int_pin = BMI08X_DISABLE;
                ret = bmi08g_set_int_config(&data->int_cfg.gyro_int_config_1, data->bmi08x_dev);
                if (ret < 0) {
		        dev_err(&client->dev, "%s(): bmi08g_set_int_config() failed(%d)\n", 
                                __func__, ret);
                        ret = -EINVAL;
                        goto exit;
	        }

                data->bmi08x_dev->gyro_cfg.power = BMI08X_GYRO_PM_SUSPEND;
                ret = bmi08g_set_power_mode(data->bmi08x_dev);
                /* 
                 * Wait for 30ms to switch between the power modes - 
                 * delay taken care inside the function
                 */
	        if (ret < 0) {
		        dev_err(&client->dev, "%s(): bmi08g_set_power_mode() failed(%d)\n", 
                                __func__, ret);
                        ret = -EINVAL;
                        goto exit;
	        }
        }

	free_irq(data->irq, data);
	gpio_free(data->gpio_pin);

	iio_device_unregister(indio_dev);
        iio_triggered_buffer_cleanup(indio_dev);
        if (data->trig) {
                iio_trigger_unregister(data->trig);
                iio_trigger_free(data->trig);
        }

        iio_device_free(indio_dev);

exit:
	return ret;
}

static const struct acpi_device_id bmi088_acpi_i2c_match[] = {
	{"BMI088a", BMI08X_ACCEL_CHIP_ID },
        {"BMI088g", BMI08X_GYRO_CHIP_ID },
	{ },
};
MODULE_DEVICE_TABLE(acpi, bmi088_acpi_i2c_match);

#ifdef CONFIG_OF
static const struct of_device_id bmi088_of_i2c_match[] = {
	{ .compatible = "bosch,bmi088a", .data = (void *)BMI08X_ACCEL_CHIP_ID },
        { .compatible = "bosch,bmi088g", .data = (void *)BMI08X_GYRO_CHIP_ID },
	{ },
};
MODULE_DEVICE_TABLE(of, bmi088_of_i2c_match);
#else
#define bmi088_of_i2c_match NULL
#endif

static const struct i2c_device_id bmi088_id[] = {
	{"bmi088a", BMI08X_ACCEL_CHIP_ID },
        {"bmi088g", BMI08X_GYRO_CHIP_ID },
	{ },
};
MODULE_DEVICE_TABLE(i2c, bmi088_id);

static struct i2c_driver bmi088_driver = {
	.driver = {
		.name = "bmi088",
                .acpi_match_table = ACPI_PTR(bmi088_acpi_i2c_match),
		.of_match_table = of_match_ptr(bmi088_of_i2c_match),
	},
	.probe		= bmi088_probe,
	.remove		= bmi088_remove,
	.id_table	= bmi088_id,
};

static int __init bmi088_module_init(void)
{
        bmi08x_info.bmi08x_dev.accel_id = BMI08X_ACCEL_I2C_ADDR_PRIMARY;
        bmi08x_info.bmi08x_dev.gyro_id  = BMI08X_GYRO_I2C_ADDR_PRIMARY;
        bmi08x_info.bmi08x_dev.intf = BMI08X_I2C_INTF;
        bmi08x_info.bmi08x_dev.read = bmi088_read;
        bmi08x_info.bmi08x_dev.write = bmi088_write; 
        bmi08x_info.bmi08x_dev.delay_ms = bmi088_delay_milli_sec;

        return i2c_add_driver(&bmi088_driver);
}
module_init(bmi088_module_init);

static void __exit bmi088_module_exit(void)
{
        i2c_del_driver(&bmi088_driver);
}
module_exit(bmi088_module_exit);


MODULE_AUTHOR("David Clark <dclark@sierrawireless.com>");
MODULE_DESCRIPTION("Driver for Bosch Sensortec BMI088 acceleration/gyro sensor");
MODULE_LICENSE("GPL v2");
