#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c/pca954x.h>
#include <linux/i2c/sx150x.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/gpio/driver.h>

#include "lsm6ds3_platform_data.h"
#include "ltc294x-platform-data.h"
#include "bq24190-platform-data.h"

#include "mangoh_red_mux.h"
#include "mangoh_common.h"
#include "iot-slot.h"
#include "led.h"

/*
 *-----------------------------------------------------------------------------
 * Constants
 *-----------------------------------------------------------------------------
 */
#define MANGOH_RED_I2C_SW_BUS_BASE		(PRIMARY_I2C_BUS + 1)
#define MANGOH_RED_I2C_BUS_IOT0			(MANGOH_RED_I2C_SW_BUS_BASE + 0)
#define MANGOH_RED_I2C_BUS_BATTERY_CHARGER	(MANGOH_RED_I2C_SW_BUS_BASE + 1)
#define MANGOH_RED_I2C_BUS_USB_HUB		(MANGOH_RED_I2C_SW_BUS_BASE + 1)
#define MANGOH_RED_I2C_BUS_GPIO_EXPANDER	(MANGOH_RED_I2C_SW_BUS_BASE + 2)
#define MANGOH_RED_I2C_BUS_EXP			(MANGOH_RED_I2C_SW_BUS_BASE + 3)


/*
 *-----------------------------------------------------------------------------
 * Types
 *-----------------------------------------------------------------------------
 */
enum mangoh_red_board_rev {
	MANGOH_RED_BOARD_REV_DV2,
	MANGOH_RED_BOARD_REV_DV3,
	MANGOH_RED_BOARD_REV_DV5,
};

/*
 *-----------------------------------------------------------------------------
 * Static Function Declarations
 *-----------------------------------------------------------------------------
 */
static void mangoh_red_release(struct device* dev);
static int mangoh_red_probe(struct platform_device* pdev);
static int mangoh_red_remove(struct platform_device* pdev);
static void mangoh_red_led_release(struct device *dev);
static void mangoh_red_iot_slot_release(struct device *dev);
static int mangoh_red_iot_slot_request_i2c(struct i2c_adapter **adapter);
static int mangoh_red_iot_slot_release_i2c(struct i2c_adapter **adapter);
static int mangoh_red_iot_slot_request_spi(struct spi_master **spi_master,
                                           int *cs);
static int mangoh_red_iot_slot_release_spi(void);
static int mangoh_red_iot_slot_request_sdio(void);
static int mangoh_red_iot_slot_release_sdio(void);
static int mangoh_red_iot_slot_request_pcm(void);
static int mangoh_red_iot_slot_release_pcm(void);

/*
 *-----------------------------------------------------------------------------
 * Variables
 *-----------------------------------------------------------------------------
 */

static char *revision_dv2 = "dv2";
static char *revision_dv3 = "dv3";
static char *revision_dv5 = "dv5";

static char *revision = "dv5";
module_param(revision, charp, S_IRUGO);
MODULE_PARM_DESC(revision, "mangOH Red board revision");

static struct platform_driver mangoh_red_driver = {
	.probe = mangoh_red_probe,
	.remove = mangoh_red_remove,
	.driver  = {
		.name = "mangoh red",
		.owner = THIS_MODULE,
		.bus = &platform_bus_type,
	},
};

static struct mangoh_red_platform_data {
	enum mangoh_red_board_rev board_rev;
} mangoh_red_pdata;

static struct mangoh_red_driver_data {
	struct i2c_client* i2c_switch;
	struct i2c_client* accelerometer;
	struct i2c_client* pressure;
	struct i2c_client* battery_gauge;
	struct i2c_client* battery_charger;
	struct i2c_client* gpio_expander;
	bool mux_initialized;
	bool iot_slot_registered;
	bool led_registered;
} mangoh_red_driver_data = {
	.mux_initialized = false,
	.iot_slot_registered = false,
	.led_registered = false,
};

static struct platform_device mangoh_red_device = {
	.name = "mangoh red",
	.id = -1,
	.dev = {
		.release = mangoh_red_release,
		.platform_data = &mangoh_red_pdata,
	},
};

static struct pca954x_platform_mode mangoh_red_pca954x_adap_modes[] = {
	{.adap_id=MANGOH_RED_I2C_SW_BUS_BASE + 0, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_RED_I2C_SW_BUS_BASE + 1, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_RED_I2C_SW_BUS_BASE + 2, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_RED_I2C_SW_BUS_BASE + 3, .deselect_on_exit=1, .class=0},
};
static struct pca954x_platform_data mangoh_red_pca954x_pdata = {
	mangoh_red_pca954x_adap_modes,
	ARRAY_SIZE(mangoh_red_pca954x_adap_modes),
};
static const struct i2c_board_info mangoh_red_pca954x_device_info = {
	I2C_BOARD_INFO("pca9546", 0x71),
	.platform_data = &mangoh_red_pca954x_pdata,
};

static struct sx150x_platform_data mangoh_red_gpio_expander_platform_data = {
	.gpio_base	    = -1,
	.oscio_is_gpo	    = false,
	.io_pullup_ena	    = 0,
	.io_pulldn_ena	    = 0,
	.io_open_drain_ena  = 0,
	.io_polarity	    = 0,
	.irq_summary	    = -1,
	.irq_base	    = -1,
	.reset_during_probe = true,

};
static const struct i2c_board_info mangoh_red_gpio_expander_devinfo = {
	I2C_BOARD_INFO("sx1509q", 0x3e),
	.platform_data = &mangoh_red_gpio_expander_platform_data,
	.irq = 0,
};

static struct i2c_board_info mangoh_red_bmi160_devinfo = {
	I2C_BOARD_INFO("bmi160", 0x68),
};

static struct lsm6ds3_platform_data mangoh_red_lsm6ds3_platform_data = {
	.drdy_int_pin = 1,
};
static struct i2c_board_info mangoh_red_lsm6ds3_devinfo = {
	I2C_BOARD_INFO("lsm6ds3", 0x6A),
	.platform_data = &mangoh_red_lsm6ds3_platform_data,
};

static struct i2c_board_info mangoh_red_pressure_devinfo = {
	I2C_BOARD_INFO("bmp280", 0x76),
};

#if 0
static struct ltc294x_platform_data mangoh_red_battery_gauge_platform_data = {
	.r_sense = 18,
	.prescaler_exp = 32,
	.name = "LTC2942",
};
static struct i2c_board_info mangoh_red_battery_gauge_devinfo = {
	I2C_BOARD_INFO("ltc2942", 0x64),
	.platform_data = &mangoh_red_battery_gauge_platform_data,
};
#endif

static struct i2c_board_info mangoh_red_battery_charger_devinfo = {
	I2C_BOARD_INFO("bq24190", 0x6B),
};

static struct iot_slot_platform_data mangoh_red_iot_slot_pdata = {
	.gpio		  = {CF3_GPIO42, CF3_GPIO13, CF3_GPIO7, CF3_GPIO8},
	.reset_gpio	  = CF3_GPIO2,
	.card_detect_gpio = CF3_GPIO33,
	.request_i2c	  = mangoh_red_iot_slot_request_i2c,
	.release_i2c	  = mangoh_red_iot_slot_release_i2c,
	.request_spi      = mangoh_red_iot_slot_request_spi,
	.release_spi      = mangoh_red_iot_slot_release_spi,
	.request_sdio	  = mangoh_red_iot_slot_request_sdio,
	.release_sdio	  = mangoh_red_iot_slot_release_sdio,
	.request_pcm	  = mangoh_red_iot_slot_request_pcm,
	.release_pcm	  = mangoh_red_iot_slot_release_pcm,
};

static struct platform_device mangoh_red_iot_slot = {
	.name = "iot-slot",
	.id = 0, /* Means IoT slot 0 */
	.dev = {
		.platform_data = &mangoh_red_iot_slot_pdata,
		.release = mangoh_red_iot_slot_release,
	},
};

static struct led_platform_data mangoh_red_led_pdata = {
	.gpio = -1,
};

static struct platform_device mangoh_red_led = {
	.name = "led",
	.dev = {
		.platform_data = &mangoh_red_led_pdata,
		.release = mangoh_red_led_release,
	},
};

static void mangoh_red_release(struct device* dev)
{
	/* Nothing alloc'd, so nothign to free */
}

static int mangoh_red_probe(struct platform_device* pdev)
{
	int ret = 0;
	int sdio_mux_gpio;
	int pcm_mux_gpio;
	struct gpio_chip *gpio_expander;
	struct i2c_board_info *accelerometer_board_info;
	struct i2c_adapter *other_adapter = NULL;
	struct i2c_adapter *main_adapter;

	dev_info(&pdev->dev, "%s(): probe\n", __func__);

	main_adapter = i2c_get_adapter(PRIMARY_I2C_BUS);
	if (!main_adapter) {
		dev_err(&pdev->dev, "Failed to get primary I2C adapter (%d).\n",
			PRIMARY_I2C_BUS);
		ret = -ENODEV;
		goto done;
	}

	/*
	 * This is a workaround of questionable validity for USB issues first
	 * seen on the mangOH Green.
	 */
	msleep(5000);

	platform_set_drvdata(pdev, &mangoh_red_driver_data);

	/* Map the I2C switch */
	dev_dbg(&pdev->dev, "mapping i2c switch\n");
	mangoh_red_driver_data.i2c_switch =
		i2c_new_device(main_adapter, &mangoh_red_pca954x_device_info);
	if (!mangoh_red_driver_data.i2c_switch) {
		dev_err(&pdev->dev, "Failed to register %s\n",
			mangoh_red_pca954x_device_info.type);
		ret = -ENODEV;
		goto cleanup;
	}

	/* Map the GPIO expander */
	dev_dbg(&pdev->dev, "mapping gpio expander\n");
	other_adapter = i2c_get_adapter(MANGOH_RED_I2C_BUS_GPIO_EXPANDER);
	if (!other_adapter) {
		dev_err(&pdev->dev,
			"Couldn't get I2C bus %d to add the GPIO expander.\n",
			MANGOH_RED_I2C_BUS_GPIO_EXPANDER);
		ret = -ENODEV;
		goto cleanup;
	}
	mangoh_red_driver_data.gpio_expander =
		i2c_new_device(other_adapter, &mangoh_red_gpio_expander_devinfo);
	i2c_put_adapter(other_adapter);
	if (!mangoh_red_driver_data.gpio_expander) {
		dev_err(&pdev->dev, "Failed to register %s\n",
			mangoh_red_gpio_expander_devinfo.type);
		ret = -ENODEV;
		goto cleanup;
	}

	gpio_expander = i2c_get_clientdata(
		mangoh_red_driver_data.gpio_expander);
	sdio_mux_gpio = gpio_expander->base + 9;
	pcm_mux_gpio = gpio_expander->base + 13;
	ret = mangoh_red_mux_init(pdev, sdio_mux_gpio, pcm_mux_gpio);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to initialize mangOH Red mux\n");
		goto cleanup;

	}
	mangoh_red_driver_data.mux_initialized = true;

	/* Map the IoT slot */
	ret = platform_device_register(&mangoh_red_iot_slot);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to register IoT slot device\n");
		goto cleanup;
	}
	mangoh_red_driver_data.iot_slot_registered = true;

	/* Map the LED */
	mangoh_red_led_pdata.gpio = gpio_expander->base + 8;
	ret = platform_device_register(&mangoh_red_led);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to register LED device\n");
		goto cleanup;
	}
	mangoh_red_driver_data.led_registered = true;

	/* Map the accelerometer */
	dev_dbg(&pdev->dev, "mapping accelerometer\n");
	/*
	 * Pins 11 and 12 of the gpio expander are connected to bmi160's INT1
	 * and INT2 pins respectively. It does not appear that the bmi160 driver
	 * makes use of these interrupt pins.
	 */
	accelerometer_board_info =
		mangoh_red_pdata.board_rev == MANGOH_RED_BOARD_REV_DV2 ?
		&mangoh_red_lsm6ds3_devinfo : &mangoh_red_bmi160_devinfo;
	mangoh_red_driver_data.accelerometer =
		i2c_new_device(main_adapter, accelerometer_board_info);
	if (!mangoh_red_driver_data.accelerometer) {
		dev_err(&pdev->dev, "Accelerometer is missing\n");
		return -ENODEV;
	}

	/* Map the I2C BMP280 pressure sensor */
	dev_dbg(&pdev->dev, "mapping bmp280 pressure sensor\n");
	mangoh_red_driver_data.pressure =
		i2c_new_device(main_adapter, &mangoh_red_pressure_devinfo);
	if (!mangoh_red_driver_data.pressure) {
		dev_err(&pdev->dev, "Pressure sensor is missing\n");
		return -ENODEV;
	}

	/* Map the I2C BQ24296 driver: for now use the BQ24190 driver code */
	dev_dbg(&pdev->dev, "mapping bq24296 driver\n");
	other_adapter = i2c_get_adapter(MANGOH_RED_I2C_BUS_BATTERY_CHARGER);
	if(!other_adapter) {
		dev_err(&pdev->dev, "No I2C bus %d.\n",
			MANGOH_RED_I2C_BUS_BATTERY_CHARGER);
		ret = -ENODEV;
		goto cleanup;
	}
	mangoh_red_driver_data.battery_charger = i2c_new_device(
		other_adapter, &mangoh_red_battery_charger_devinfo);
	i2c_put_adapter(other_adapter);
	if (!mangoh_red_driver_data.battery_charger) {
		dev_err(&pdev->dev, "battery charger is missing\n");
		ret = -ENODEV;
		goto cleanup;
	}

#if 0
	if (mangoh_red_pdata.board_rev != MANGOH_RED_BOARD_REV_DV3) {
		/* Map the I2C ltc2942 battery gauge */
		dev_dbg(&pdev->dev, "mapping ltc2942 battery gauge\n");
		other_adapter =
			i2c_get_adapter(MANGOH_RED_I2C_BUS_BATTERY_CHARGER);
		if (!other_adapter) {
			dev_err(&pdev->dev, "No I2C bus %d.\n",
			        MANGOH_RED_I2C_BUS_BATTERY_CHARGER);
			ret = -ENODEV;
			goto cleanup;
		}
		mangoh_red_driver_data.battery_gauge = i2c_new_device(
			other_adapter, &mangoh_red_battery_gauge_devinfo);
		i2c_put_adapter(other_adapter);
		if (!mangoh_red_driver_data.battery_gauge) {
			dev_err(&pdev->dev, "battery gauge is missing\n");
			ret = -ENODEV;
			goto cleanup;
		}
	}
#endif
	/*
	 * TODO:
	 * 3503 USB Hub: 0x08
	 *    Looks like there is a driver in the wp85 kernel source at
	 *    drivers/usb/misc/usb3503.c. I'm not really sure what benefit is
	 *    achieved through using this driver.
	 */

cleanup:
	i2c_put_adapter(main_adapter);
	if (ret != 0)
		mangoh_red_remove(pdev);
done:
	return ret;
}

static void try_unregister_i2c_device(struct i2c_client *client)
{
	if (client != NULL) {
		i2c_unregister_device(client);
		i2c_put_adapter(client->adapter);
	}
}

static int mangoh_red_remove(struct platform_device* pdev)
{
	struct mangoh_red_driver_data *dd = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "Removing mangoh red platform device\n");

#if 0
	if (mangoh_red_pdata.board_rev != MANGOH_RED_BOARD_REV_DV3)
		try_unregister_i2c_device(dd->battery_gauge);
#endif

	try_unregister_i2c_device(dd->battery_charger);
	try_unregister_i2c_device(dd->pressure);
	try_unregister_i2c_device(dd->accelerometer);

	if (dd->led_registered)
		platform_device_unregister(&mangoh_red_led);

	if (dd->iot_slot_registered)
		platform_device_unregister(&mangoh_red_iot_slot);

	if (dd->mux_initialized)
		mangoh_red_mux_deinit();

	try_unregister_i2c_device(dd->gpio_expander);
	try_unregister_i2c_device(dd->i2c_switch);

	return 0;
}

/* Release function is needed to avoid warning when device is deleted */
static void mangoh_red_iot_slot_release(struct device *dev) { /* do nothing */ }
static void mangoh_red_led_release(struct device *dev) { /* do nothing */ }

static int mangoh_red_iot_slot_request_i2c(struct i2c_adapter **adapter)
{
	*adapter = i2c_get_adapter(MANGOH_RED_I2C_BUS_IOT0);
	return *adapter != NULL ? 0 : -EINVAL;
}

static int mangoh_red_iot_slot_release_i2c(struct i2c_adapter **adapter)
{
	i2c_put_adapter(*adapter);
	*adapter = NULL;
	return 0;
}

static int mangoh_red_iot_slot_request_spi(struct spi_master **spi_master,
                                           int *cs)
{
	*spi_master = spi_busnum_to_master(PRIMARY_SPI_BUS);
	*cs = 0;
	if (!*spi_master) {
		return -ENODEV;
	}

	return 0;
}

static int mangoh_red_iot_slot_release_spi(void)
{
	return 0; /* Nothing to do */
}

static int mangoh_red_iot_slot_request_sdio(void)
{
	return mangoh_red_mux_sdio_select(SDIO_SELECTION_IOT_SLOT);
}

static int mangoh_red_iot_slot_release_sdio(void)
{
	return mangoh_red_mux_sdio_release(SDIO_SELECTION_IOT_SLOT);
}

static int mangoh_red_iot_slot_request_pcm(void)
{
	return mangoh_red_mux_pcm_select(PCM_SELECTION_IOT_SLOT);
}

static int mangoh_red_iot_slot_release_pcm(void)
{
	return mangoh_red_mux_pcm_release(PCM_SELECTION_IOT_SLOT);
}

static int __init mangoh_red_init(void)
{
	platform_driver_register(&mangoh_red_driver);
	printk(KERN_DEBUG "mangoh: registered platform driver\n");

	if (strcmp(revision, revision_dv2) == 0) {
		mangoh_red_pdata.board_rev = MANGOH_RED_BOARD_REV_DV2;
	} else if (strcmp(revision, revision_dv3) == 0) {
		mangoh_red_pdata.board_rev = MANGOH_RED_BOARD_REV_DV3;
	} else if (strcmp(revision, revision_dv5) == 0) {
		mangoh_red_pdata.board_rev = MANGOH_RED_BOARD_REV_DV5;
	} else {
		pr_err("%s: Unsupported mangOH Red board revision (%s)\n",
			__func__, revision);
		return -ENODEV; /* TODO: better value? */
	}

	if (platform_device_register(&mangoh_red_device)) {
		platform_driver_unregister(&mangoh_red_driver);
		return -ENODEV;
	}

	return 0;
}

static void __exit mangoh_red_exit(void)
{
	platform_device_unregister(&mangoh_red_device);
	platform_driver_unregister(&mangoh_red_driver);
}

module_init(mangoh_red_init);
module_exit(mangoh_red_exit);

MODULE_ALIAS(PLATFORM_MODULE_PREFIX "mangoh red");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sierra Wireless");
MODULE_DESCRIPTION("Add devices on mangOH Red hardware board");
MODULE_VERSION("1.0");
