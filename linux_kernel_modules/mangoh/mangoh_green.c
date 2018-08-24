#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c/pca954x.h>
#include <linux/delay.h>

#include "lsm6ds3_platform_data.h"
#include "mangoh_common.h"

/*
 *-----------------------------------------------------------------------------
 * Constants
 *-----------------------------------------------------------------------------
 */
#define MANGOH_GREEN_I2C_SW_BUS_BASE		(PRIMARY_I2C_BUS + 1)
#define MANGOH_GREEN_I2C_BUS_IOT0		(MANGOH_GREEN_I2C_SW_BUS_BASE + 0)
#define MANGOH_GREEN_I2C_BUS_IOT1		(MANGOH_GREEN_I2C_SW_BUS_BASE + 1)
#define MANGOH_GREEN_I2C_BUS_IOT2		(MANGOH_GREEN_I2C_SW_BUS_BASE + 2)
#define MANGOH_GREEN_I2C_BUS_USB_HUB		(MANGOH_GREEN_I2C_SW_BUS_BASE + 3)
#define MANGOH_GREEN_I2C_BUS_GPIO_EXPANDER1	(MANGOH_GREEN_I2C_SW_BUS_BASE + 4)
#define MANGOH_GREEN_I2C_BUS_GPIO_EXPANDER2	(MANGOH_GREEN_I2C_SW_BUS_BASE + 5)
#define MANGOH_GREEN_I2C_BUS_GPIO_EXPANDER3	(MANGOH_GREEN_I2C_SW_BUS_BASE + 6)
#define MANGOH_GREEN_I2C_BUS_BATTERY_CHARGER	(MANGOH_GREEN_I2C_SW_BUS_BASE + 7)

/*
 *-----------------------------------------------------------------------------
 * Types
 *-----------------------------------------------------------------------------
 */
enum mangoh_green_board_rev {
	MANGOH_GREEN_BOARD_REV_DV4,
};

/*
 *-----------------------------------------------------------------------------
 * Static Function Declarations
 *-----------------------------------------------------------------------------
 */
static void mangoh_green_release(struct device* dev);
static int mangoh_green_probe(struct platform_device* pdev);
static int mangoh_green_remove(struct platform_device* pdev);

/*
 *-----------------------------------------------------------------------------
 * Variables
 *-----------------------------------------------------------------------------
 */

static char *revision_dv4 = "dv4";

static char *revision = "dv4";
module_param(revision, charp, S_IRUGO);
MODULE_PARM_DESC(revision, "mangOH Green board revision");

static struct platform_driver mangoh_green_driver = {
	.probe = mangoh_green_probe,
	.remove = mangoh_green_remove,
	.driver  = {
		.name = "mangoh green",
		.owner = THIS_MODULE,
		.bus = &platform_bus_type,
	},
};

static struct mangoh_green_platform_data {
	enum mangoh_green_board_rev board_rev;
} mangoh_green_pdata;

static struct mangoh_green_driver_data {
	struct i2c_client* i2c_switch;
	struct i2c_client* accelerometer;
} mangoh_green_driver_data;

static struct platform_device mangoh_green_device = {
	.name = "mangoh green",
	.id = -1,
	.dev = {
		.release = mangoh_green_release,
		.platform_data = &mangoh_green_pdata,
	},
};

static struct pca954x_platform_mode mangoh_green_pca954x_adap_modes[] = {
	{.adap_id=MANGOH_GREEN_I2C_SW_BUS_BASE + 0, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_GREEN_I2C_SW_BUS_BASE + 1, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_GREEN_I2C_SW_BUS_BASE + 2, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_GREEN_I2C_SW_BUS_BASE + 3, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_GREEN_I2C_SW_BUS_BASE + 4, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_GREEN_I2C_SW_BUS_BASE + 5, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_GREEN_I2C_SW_BUS_BASE + 6, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_GREEN_I2C_SW_BUS_BASE + 7, .deselect_on_exit=1, .class=0},
};
static struct pca954x_platform_data mangoh_green_pca954x_pdata = {
	mangoh_green_pca954x_adap_modes,
	ARRAY_SIZE(mangoh_green_pca954x_adap_modes),
};
static const struct i2c_board_info mangoh_green_pca954x_device_info = {
	I2C_BOARD_INFO("pca9548", 0x71),
	.platform_data = &mangoh_green_pca954x_pdata,
};

static struct lsm6ds3_platform_data mangoh_green_lsm6ds3_platform_data = {
	.drdy_int_pin = 1,
};
static struct i2c_board_info mangoh_green_lsm6ds3_devinfo = {
	I2C_BOARD_INFO("lsm6ds3", 0x6A),
	.platform_data = &mangoh_green_lsm6ds3_platform_data,
};


static void mangoh_green_release(struct device* dev)
{
	/* Nothing alloc'd, so nothign to free */
}

static int mangoh_green_probe(struct platform_device* pdev)
{
	int ret = 0;
	struct i2c_adapter* adapter;
	dev_info(&pdev->dev, "In the probe\n");

	/*
	 * This is a workaround of questionable validity for USB issues first
	 * seen on the mangOH Green.
	 */
	msleep(5000);

	/* TODO: seems pointless */
	platform_set_drvdata(pdev, &mangoh_green_driver_data);

	/* Get the main i2c adapter */
	adapter = i2c_get_adapter(PRIMARY_I2C_BUS);
	if (!adapter) {
		dev_err(&pdev->dev,
			"Failed to get the primary I2C adapter (%d).\n",
			PRIMARY_I2C_BUS);
		ret = -ENODEV;
		goto done;
	}

	/* Map the I2C switch */
	dev_dbg(&pdev->dev, "mapping i2c switch\n");
	mangoh_green_driver_data.i2c_switch =
		i2c_new_device(adapter, &mangoh_green_pca954x_device_info);
	if (!mangoh_green_driver_data.i2c_switch) {
		dev_err(
			&pdev->dev,
			"Failed to register %s\n",
			mangoh_green_pca954x_device_info.type);
		ret = -ENODEV;
		goto cleanup;
	}

	/* Map the accelerometer */
	dev_dbg(&pdev->dev, "mapping bmi160 accelerometer\n");
	mangoh_green_driver_data.accelerometer =
		i2c_new_device(adapter, &mangoh_green_lsm6ds3_devinfo);
	if (!mangoh_green_driver_data.accelerometer) {
		dev_err(&pdev->dev, "Accelerometer is missing\n");
		ret = -ENODEV;
		goto cleanup;
	}

	/*
	 * TODO:
	 * SX1509 GPIO expanders: 0x3E, 0x3F, 0x70
	 *    There is a driver in the WP85 kernel, but the gpiolib
	 *    infrastructure of the WP85 kernel does not allow the expander
	 *    GPIOs to be used in sysfs due to a hardcoded translation table.
	 * 3503 USB Hub: 0x08
	 *    Looks like there is a driver in the wp85 kernel source at drivers/usb/misc/usb3503.c
	 *    I'm not really sure what benefit is achieved through using this driver.
	 * Battery Gauge: 0x55
	 *    chip is BQ27621YZFR-G1A which is supported by this driver which is
	 *    in the mainline linux kernel as
	 *    drivers/power/supply/bq27xxx_battery*. Unfortunately, the power
	 *    supply API has changed, so using this driver would require
	 *    substantial backporting.
	 *    http://www.ti.com/tool/bq27xxxsw-linux
	 * Buck & Battery Charger: 0x6B
	 *    chip is BQ24192IRGER. I haven't been able to find a linux kernel
	 *    driver, but this looks like some relevant code:
	 *    https://chromium.googlesource.com/chromiumos/platform/ec/+/master/driver/charger/bq24192.c
	*/

cleanup:
	i2c_put_adapter(adapter);
	if (ret != 0)
		mangoh_green_remove(pdev);
done:
	return ret;
}

static int mangoh_green_remove(struct platform_device* pdev)
{
	dev_info(&pdev->dev, "In the remove\n");
	i2c_unregister_device(mangoh_green_driver_data.accelerometer);
	i2c_unregister_device(mangoh_green_driver_data.i2c_switch);
	return 0;
}

static int __init mangoh_green_init(void)
{
	platform_driver_register(&mangoh_green_driver);
	printk(KERN_DEBUG "mangoh: registered platform driver\n");

	if (strcmp(revision, revision_dv4) == 0) {
		mangoh_green_pdata.board_rev = MANGOH_GREEN_BOARD_REV_DV4;
	} else {
		pr_err(
			"%s: Unsupported mangOH Red board revision (%s)\n",
			__func__,
			revision);
		return -ENODEV; /* TODO: better value? */
	}

	if (platform_device_register(&mangoh_green_device)) {
		platform_driver_unregister(&mangoh_green_driver);
		return -ENODEV;
	}

	return 0;
}

static void __exit mangoh_green_exit(void)
{
	platform_device_unregister(&mangoh_green_device);
	platform_driver_unregister(&mangoh_green_driver);
}

module_init(mangoh_green_init);
module_exit(mangoh_green_exit);

MODULE_ALIAS(PLATFORM_MODULE_PREFIX "mangoh red");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sierra Wireless");
MODULE_DESCRIPTION("Add devices on mangOH Red hardware board");
MODULE_VERSION("1.0");
