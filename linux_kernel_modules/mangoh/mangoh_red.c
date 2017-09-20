#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c/pca954x.h>
#include <linux/delay.h>

#include "lsm6ds3_platform_data.h"

/*
 *-----------------------------------------------------------------------------
 * Constants
 *-----------------------------------------------------------------------------
 */
#define MANGOH_RED_I2C_SW_BASE_ADAPTER_ID	(1)
#define MANGOH_RED_I2C_SW_PORT_IOT0		(0)
#define MANGOH_RED_I2C_SW_PORT_BATTERY_CHARGER	(1)
#define MANGOH_RED_I2C_SW_PORT_USB_HUB		(1)
#define MANGOH_RED_I2C_SW_PORT_GPIO_EXPANDER	(2)
#define MANGOH_RED_I2C_SW_PORT_EXP		(3)	/* expansion header */

/*
 *-----------------------------------------------------------------------------
 * Types
 *-----------------------------------------------------------------------------
 */
enum mangoh_red_board_rev {
	MANGOH_RED_BOARD_REV_DV2,
	MANGOH_RED_BOARD_REV_DV3,
	MANGOH_RED_BOARD_REV_DV4,
};

/*
 *-----------------------------------------------------------------------------
 * Static Function Declarations
 *-----------------------------------------------------------------------------
 */
static void mangoh_red_release(struct device* dev);
static int mangoh_red_probe(struct platform_device* pdev);
static int mangoh_red_remove(struct platform_device* pdev);

/*
 *-----------------------------------------------------------------------------
 * Variables
 *-----------------------------------------------------------------------------
 */

static char *revision_dv2 = "dv2";
static char *revision_dv3 = "dv3";
static char *revision_dv4 = "dv4";

static char *revision = "dv4";
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
} mangoh_red_driver_data;

static struct platform_device mangoh_red_device = {
	.name = "mangoh red",
	.id = -1,
	.dev = {
		.release = mangoh_red_release,
		.platform_data = &mangoh_red_pdata,
	},
};

static struct pca954x_platform_mode mangoh_red_pca954x_adap_modes[] = {
	{.adap_id=MANGOH_RED_I2C_SW_BASE_ADAPTER_ID + 0, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_RED_I2C_SW_BASE_ADAPTER_ID + 1, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_RED_I2C_SW_BASE_ADAPTER_ID + 2, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_RED_I2C_SW_BASE_ADAPTER_ID + 3, .deselect_on_exit=1, .class=0},
};
static struct pca954x_platform_data mangoh_red_pca954x_pdata = {
	mangoh_red_pca954x_adap_modes,
	ARRAY_SIZE(mangoh_red_pca954x_adap_modes),
};
static const struct i2c_board_info mangoh_red_pca954x_device_info = {
	I2C_BOARD_INFO("pca9546", 0x71),
	.platform_data = &mangoh_red_pca954x_pdata,
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



static void mangoh_red_release(struct device* dev)
{
	/* Nothing alloc'd, so nothign to free */
}

static int mangoh_red_probe(struct platform_device* pdev)
{
	dev_info(&pdev->dev, "In the probe\n");

	/*
	 * This is a workaround of questionable validity for USB issues first
	 * seen on the mangOH Green.
	 */
	msleep(5000);

	/* TODO: seems pointless */
	platform_set_drvdata(pdev, &mangoh_red_driver_data);

	/* Get the main i2c adapter */
	struct i2c_adapter* adapter = i2c_get_adapter(0);
	if (!adapter) {
		dev_err(&pdev->dev, "Failed to get I2C adapter 0.\n");
		return -ENODEV;
	}

	/* Map the I2C switch */
	dev_dbg(&pdev->dev, "mapping i2c switch\n");
	mangoh_red_driver_data.i2c_switch =
		i2c_new_device(adapter, &mangoh_red_pca954x_device_info);
	if (!mangoh_red_driver_data.i2c_switch) {
		dev_err(
			&pdev->dev,
			"Failed to register %s\n",
			mangoh_red_pca954x_device_info.type);
		return -ENODEV;
	}

	/* Map the accelerometer */
	dev_dbg(&pdev->dev, "mapping accelerometer\n");
	/*
	 * Pins 11 and 12 of the gpio expander are connected to bmi160's INT1
	 * and INT2 pins respectively. It does not appear that the bmi160 driver
	 * makes use of these interrupt pins.
	 */
	adapter = i2c_get_adapter(0);
	if (!adapter) {
		dev_err(&pdev->dev, "No I2C bus %d.\n", 0);
		return -ENODEV;
	}
	struct i2c_board_info *accelerometer_board_info =
		mangoh_red_pdata.board_rev == MANGOH_RED_BOARD_REV_DV2 ?
		&mangoh_red_lsm6ds3_devinfo : &mangoh_red_bmi160_devinfo;
	mangoh_red_driver_data.accelerometer =
		i2c_new_device(adapter, accelerometer_board_info);
	if (!mangoh_red_driver_data.accelerometer) {
		dev_err(&pdev->dev, "Accelerometer is missing\n");
		return -ENODEV;
	}

	/* Map the I2C BMP280 pressure sensor */
	dev_dbg(&pdev->dev, "mapping bmp280 pressure sensor\n");
	adapter = i2c_get_adapter(0);
	if (!adapter) {
		dev_err(&pdev->dev, "No I2C bus %d.\n", 0);
		return -ENODEV;
	}
	mangoh_red_driver_data.pressure =
		i2c_new_device(adapter, &mangoh_red_pressure_devinfo);
	if (!mangoh_red_driver_data.pressure) {
		dev_err(&pdev->dev, "Pressure sensor is missing\n");
		return -ENODEV;
	}

	if (mangoh_red_pdata.board_rev != MANGOH_RED_BOARD_REV_DV3) {
		/*
		 * TODO:
		 * SX1509 GPIO expanders: 0x3E
		 *    There is a driver in the WP85 kernel, but the gpiolib
		 *    infrastructure of the WP85 kernel does not allow the
		 *    expander GPIOs to be used in sysfs due to a hardcoded
		 *    translation table.
		 * Battery Gauge: 0x64
		 *    chip is LTC2942 which is made by linear technologies (now
		 *    Analog Devices). There is a kernel driver in the
		 *    linux-power-supply repository in the for-next branch.
		 */
	}

	/*
	 * TODO:
	 * 3503 USB Hub: 0x08
	 *    Looks like there is a driver in the wp85 kernel source at drivers/usb/misc/usb3503.c
	 *    I'm not really sure what benefit is achieved through using this driver.
	 * Buck & Battery Charger: 0x6B
	 *    chip is BQ24296RGER
	 */

	return 0;
}

static int mangoh_red_remove(struct platform_device* pdev)
{
	dev_info(&pdev->dev, "In the remove\n");

	i2c_unregister_device(mangoh_red_driver_data.pressure);
	i2c_put_adapter(mangoh_red_driver_data.pressure->adapter);

	i2c_unregister_device(mangoh_red_driver_data.accelerometer);
	i2c_put_adapter(mangoh_red_driver_data.accelerometer->adapter);

	i2c_unregister_device(mangoh_red_driver_data.i2c_switch);
	i2c_put_adapter(mangoh_red_driver_data.i2c_switch->adapter);
	return 0;
}

static int __init mangoh_red_init(void)
{
	platform_driver_register(&mangoh_red_driver);
	printk(KERN_DEBUG "mangoh: registered platform driver\n");

	if (strcmp(revision, revision_dv2) == 0) {
		mangoh_red_pdata.board_rev = MANGOH_RED_BOARD_REV_DV2;
	} else if (strcmp(revision, revision_dv3) == 0) {
		mangoh_red_pdata.board_rev = MANGOH_RED_BOARD_REV_DV3;
	} else if (strcmp(revision, revision_dv4) == 0) {
		mangoh_red_pdata.board_rev = MANGOH_RED_BOARD_REV_DV4;
	} else {
		pr_err(
			"%s: Unsupported mangOH Red board revision (%s)\n",
			__func__,
			revision);
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
