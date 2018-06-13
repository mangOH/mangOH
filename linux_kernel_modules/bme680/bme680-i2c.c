#include <linux/device.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/version.h>
#include <linux/delay.h>

#include "bme680.h"
#include "bme680_defs.h"

extern const struct regmap_config bme680_regmap_config;

struct bme680_data {
	struct mutex                    lock;
	struct bme680_dev               bme680_dev;
        struct bme680_field_data        field_data;
        struct regmap                   *regmap;
};

static const struct iio_chan_spec bme680_channels[] = {
	{
		.type = IIO_PRESSURE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
        },
	{
		.type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
	},
        {
		.type = IIO_HUMIDITYRELATIVE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
        },
	{
		.type = IIO_RESISTANCE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
	},
};

static void bme680_delay_ms(u32 period)
{
        /*
         * Return control or wait,
         * for a period amount of milliseconds
         */
        msleep_interruptible(period);
}

static int bme680_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct bme680_data *data = iio_priv(indio_dev);
        int ret;

	mutex_lock(&data->lock);

        switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
        {
                uint16_t meas_period;
                u8 set_required_settings;

                /* Select the power mode */
                /* Must be set before writing the sensor configuration */
                data->bme680_dev.power_mode = BME680_FORCED_MODE; 

                /* Set the required sensor settings needed */
                set_required_settings = BME680_OST_SEL | BME680_OSP_SEL | BME680_OSH_SEL | BME680_FILTER_SEL 
                        | BME680_GAS_SENSOR_SEL;

                /* Set the desired sensor configuration */
                ret = bme680_set_sensor_settings(set_required_settings, &data->bme680_dev);
                if (ret) {
		        dev_err(&indio_dev->dev, "%s(): bme680_set_sensor_settings() failed(%d)\n", __func__, ret);
		        goto exit;
	        }

                /* Set the power mode */
                ret = bme680_set_sensor_mode(&data->bme680_dev);
                if (ret) {
		        dev_err(&indio_dev->dev, "%s(): bme680_set_sensor_settings() failed(%d)\n", __func__, ret);
		        goto exit;
	        }

                bme680_get_profile_dur(&meas_period, &data->bme680_dev);

                /* Delay till the measurement is ready */
                bme680_delay_ms(meas_period * 2);

                ret = bme680_get_sensor_data(&data->field_data, &data->bme680_dev);
                if (ret) {
                        if (ret != BME680_W_NO_NEW_DATA) {
		                dev_err(&indio_dev->dev, "%s(): bme680_get_sensor_data() failed(%d)\n", 
                                        __func__, ret);
                        }
                        else {
                                dev_warn(&indio_dev->dev, "%s(): no new data\n", __func__);
                                ret = BME680_OK;
                        }

		        goto exit;
	        }

                switch (chan->type) {
		case IIO_RESISTANCE:
                        if (data->field_data.status & BME680_GASM_VALID_MSK) {
                                dev_dbg(&indio_dev->dev, "%s(): gas resistance(%d ohms)\n", 
                                        __func__, data->field_data.gas_resistance);
                                *val = data->field_data.gas_resistance;
                                ret = IIO_VAL_INT;
                        }
                        else {
                                dev_warn(&indio_dev->dev, "%s(): unstable heating setup\n", 
                                        __func__);
                                ret = -EAGAIN;
                                goto exit;
                        }

			break;
		case IIO_PRESSURE:
                        dev_dbg(&indio_dev->dev, "%s(): pressure(%d.%u hPa)\n", 
                                __func__, data->field_data.pressure / 100, 
                                data->field_data.pressure % 100);
                        *val = data->field_data.pressure;
                        ret = IIO_VAL_INT;
                        break;
                case IIO_HUMIDITYRELATIVE:
                        dev_dbg(&indio_dev->dev, "%s(): humidity(%d.%u %%rH)\n", 
                                __func__, data->field_data.humidity / 1000,
                                data->field_data.humidity % 1000);
			*val = data->field_data.humidity;
                        *val2 = 1000;
                        ret = IIO_VAL_FRACTIONAL;
                        break;
		case IIO_TEMP:
                        dev_dbg(&indio_dev->dev, "%s(): temperature(%d.%u degC)\n", 
                                __func__, data->field_data.temperature / 100,
                                data->field_data.temperature % 100);
			*val = data->field_data.temperature;
                        *val2 = 100;
                        ret = IIO_VAL_FRACTIONAL;
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
                dev_err(&indio_dev->dev, "%s(): unsupported mask(%ld)\n", __func__, mask);
		ret = -EINVAL;
		goto exit;
	}

exit:
        mutex_unlock(&data->lock);
	return ret;
}

static int bme680_write_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int val, int val2, long mask)
{
        dev_err(&indio_dev->dev, "%s(): unsupported\n", __func__);
	return -EINVAL;
}

static const struct iio_info bme680_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &bme680_read_raw,
	.write_raw = &bme680_write_raw,
};

static int match(struct device *dev, void *data)
{
        struct i2c_client *client = to_i2c_client(dev);
        return (client->addr == BME680_I2C_ADDR_PRIMARY);
}

static struct i2c_client *bme680_i2c_get_client(void)
{
        struct device *dev = bus_find_device(&i2c_bus_type, NULL, NULL, match);
        if (!dev) 
                return NULL;

        return to_i2c_client(dev);
}

static s8 bme680_i2c_read(u8 dev_id, u8 reg_addr, u8 *reg_data, u16 len)
{
        static struct i2c_client *client = NULL;
        struct iio_dev *indio_dev;
        struct bme680_data *data;
        int ret = 0;

        if (!client) {
                client = bme680_i2c_get_client();
                if (IS_ERR(client)) {
                        ret = PTR_ERR(client);
                        goto exit;
                }
        }

        indio_dev = dev_get_drvdata(&client->dev);
        data = iio_priv(indio_dev);

        ret = regmap_bulk_read(data->regmap, reg_addr, reg_data, len);
        if (ret < 0) {
                dev_err(&client->dev, "%s(): regmap_bulk_read() failed(%d)\n", __func__, ret);
                goto exit;
        }

exit:
        return ret;
}

static s8 bme680_i2c_write(u8 dev_id, u8 reg_addr, u8 *reg_data, u16 len)
{
        static struct i2c_client *client = NULL;
        struct iio_dev *indio_dev;
        struct bme680_data *data;
        int ret = 0;

        if (!client) {
                client = bme680_i2c_get_client();
                if (IS_ERR(client)) {
                        ret = PTR_ERR(client);
                        goto exit;
                }
        }

        indio_dev = dev_get_drvdata(&client->dev);
        data = iio_priv(indio_dev);

        ret = regmap_bulk_write(data->regmap, reg_addr, reg_data, len);
        if (ret < 0) {
                dev_err(&client->dev, "%s(): regmap_bulk_write() failed(%d)\n", __func__, ret);
                goto exit;
        }

exit:
        return ret;
}

static int bme680_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int ret;
	struct iio_dev *indio_dev;
        struct bme680_data *data;

        dev_info(&client->dev, "%s(): probe addr(0x%04x) chip ID(0x%lx)\n", 
                __func__, client->addr, id->driver_data);
        if (id->driver_data != BME680_CHIP_ID) {
                dev_err(&client->dev, "%s(): invalid chip ID(0x%lx)\n", __func__, id->driver_data);
                ret = -EINVAL;
		goto exit;
        }

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (IS_ERR(indio_dev)) {
                dev_err(&client->dev, "%s(): devm_iio_device_alloc() failed(%d)\n", __func__, ret);
		ret = PTR_ERR(indio_dev);
                goto exit;
        }

	data = iio_priv(indio_dev);
	mutex_init(&data->lock);

        data->regmap = devm_regmap_init_i2c(client, &bme680_regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(&client->dev, "%s(): devm_regmap_init_i2c() failed(%d)\n", __func__, ret);
		ret = PTR_ERR(data->regmap);
                goto exit;
	}

	indio_dev->dev.parent = &client->dev;
	indio_dev->name = id->name;
	indio_dev->channels = bme680_channels;
	indio_dev->info = &bme680_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->num_channels = 4;

        dev_set_drvdata(&client->dev, indio_dev);

	ret = iio_device_register(indio_dev);
        if (ret) {
		dev_err(&client->dev, "%s(): iio_device_register() failed(%d)\n", __func__, ret);
		goto exit;
	}

        data->bme680_dev.dev_id = BME680_I2C_ADDR_PRIMARY;
        data->bme680_dev.intf = BME680_I2C_INTF;
        data->bme680_dev.read = bme680_i2c_read;
        data->bme680_dev.write = bme680_i2c_write;
        data->bme680_dev.delay_ms = bme680_delay_ms;

        ret = bme680_init(&data->bme680_dev);
        if (ret) {
		dev_err(&client->dev, "%s(): bme680_init() failed(%d)\n", __func__, ret);
		goto exit;
	}

        /* Set the temperature, pressure and humidity settings */
        data->bme680_dev.tph_sett.os_hum = BME680_OS_2X;
        data->bme680_dev.tph_sett.os_pres = BME680_OS_4X;
        data->bme680_dev.tph_sett.os_temp = BME680_OS_8X;
        data->bme680_dev.tph_sett.filter = BME680_FILTER_SIZE_3;

        /* Set the remaining gas sensor settings and link the heating profile */
        data->bme680_dev.gas_sett.run_gas = BME680_ENABLE_GAS_MEAS;
        /* Create a ramp heat waveform in 3 steps */
        data->bme680_dev.gas_sett.heatr_temp = 320; /* degree Celsius */
        data->bme680_dev.gas_sett.heatr_dur = 150; /* milliseconds */

        /* Select the power mode */
        /* Must be set before writing the sensor configuration */
        data->bme680_dev.power_mode = BME680_FORCED_MODE;

exit:
	return ret;
}

static int bme680_i2c_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = dev_get_drvdata(&client->dev);

        dev_info(&client->dev, "%s(): remove\n", __func__);
	iio_device_unregister(indio_dev);
        iio_device_free(indio_dev);
	return 0;
}

static const struct acpi_device_id bme680_acpi_i2c_match[] = {
	{"BME680", BME680_CHIP_ID },
	{ },
};
MODULE_DEVICE_TABLE(acpi, bme680_acpi_i2c_match);

#ifdef CONFIG_OF
static const struct of_device_id bme680_of_i2c_match[] = {
	{ .compatible = "bosch,bme680", .data = (void *)BME680_CHIP_ID },
	{ },
};
MODULE_DEVICE_TABLE(of, bme680_of_i2c_match);
#else
#define bme680_of_i2c_match NULL
#endif

static const struct i2c_device_id bme680_i2c_id[] = {
	{"bme680", BME680_CHIP_ID },
	{ },
};
MODULE_DEVICE_TABLE(i2c, bme680_i2c_id);

static struct i2c_driver bme680_i2c_driver = {
	.driver = {
		.name = "bme680",
		.acpi_match_table = ACPI_PTR(bme680_acpi_i2c_match),
		.of_match_table = of_match_ptr(bme680_of_i2c_match),
	},
	.probe		= bme680_i2c_probe,
	.remove		= bme680_i2c_remove,
	.id_table	= bme680_i2c_id,
};
module_i2c_driver(bme680_i2c_driver);

MODULE_AUTHOR("David Clark <dclark@sierrawireless.com>");
MODULE_DESCRIPTION("Driver for Bosch Sensortec BME680 gas, humidity, pressure and temperature sensor");
MODULE_LICENSE("GPL v2");
