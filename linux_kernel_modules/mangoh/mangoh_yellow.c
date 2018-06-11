#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio/driver.h>

#include "mangoh_common.h"

/*
 *-----------------------------------------------------------------------------
 * Constants
 *-----------------------------------------------------------------------------
 */
#define MANGOH_YELLOW_I2C_SW_BUS_BASE		(PRIMARY_I2C_BUS + 1)
#define MANGOH_YELLOW_I2C_BUS_IOT0	        (MANGOH_YELLOW_I2C_SW_BUS_BASE + 0)
#define MANGOH_YELLOW_I2C_BUS_BATTERY_CHARGER	(MANGOH_YELLOW_I2C_SW_BUS_BASE + 1)
#define MANGOH_YELLOW_I2C_BUS_USB_HUB		(MANGOH_YELLOW_I2C_SW_BUS_BASE + 1)
#define MANGOH_YELLOW_I2C_BUS_GPIO_EXPANDER	(MANGOH_YELLOW_I2C_SW_BUS_BASE + 2)
#define MANGOH_YELLOW_I2C_BUS_EXP		(MANGOH_YELLOW_I2C_SW_BUS_BASE + 3)

/*
 *-----------------------------------------------------------------------------
 * Types
 *-----------------------------------------------------------------------------
 */
enum mangoh_yellow_board_rev {
	MANGOH_YELLOW_BOARD_REV_DV1,
};

/*
 *-----------------------------------------------------------------------------
 * Static Function Declarations
 *-----------------------------------------------------------------------------
 */
static void mangoh_yellow_release(struct device* dev);
static int mangoh_yellow_probe(struct platform_device* pdev);
static int mangoh_yellow_remove(struct platform_device* pdev);

/*
 *-----------------------------------------------------------------------------
 * Variables
 *-----------------------------------------------------------------------------
 */

static char *revision = "dv1";
module_param(revision, charp, S_IRUGO);
MODULE_PARM_DESC(revision, "mangOH Yellow board revision");

static struct platform_driver mangoh_yellow_driver = {
	.probe = mangoh_yellow_probe,
	.remove = mangoh_yellow_remove,
	.driver  = {
		.name = "mangoh yellow",
		.owner = THIS_MODULE,
		.bus = &platform_bus_type,
	},
};

static struct mangoh_yellow_platform_data {
	enum mangoh_yellow_board_rev board_rev;
} mangoh_yellow_pdata;

static struct mangoh_yellow_driver_data {
        struct i2c_client* bme680;
        struct i2c_client* bmi088a;
        struct i2c_client* bmi088g;
} mangoh_yellow_driver_data = {
};

static struct platform_device mangoh_yellow_device = {
	.name = "mangoh yellow",
	.id = -1,
	.dev = {
		.release = mangoh_yellow_release,
		.platform_data = &mangoh_yellow_pdata,
	},
};

static struct i2c_board_info mangoh_yellow_bme680_devinfo = {
	I2C_BOARD_INFO("bme680", 0x76),
};

static struct i2c_board_info mangoh_yellow_bmi088a_devinfo = {
	I2C_BOARD_INFO("bmi088a", 0x18),
};

static struct i2c_board_info mangoh_yellow_bmi088g_devinfo = {
	I2C_BOARD_INFO("bmi088g", 0x68),
};

static void mangoh_yellow_release(struct device* dev)
{
	/* Nothing alloc'd, so nothign to free */
}

static int mangoh_yellow_probe(struct platform_device* pdev)
{
	int ret = 0;
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

	platform_set_drvdata(pdev, &mangoh_yellow_driver_data);

        /* Map the I2C BME680 humidity/gas/temp/pressure sensor */
	dev_dbg(&pdev->dev, "mapping bme680 gas/temperature/pressure sensor\n");    
	mangoh_yellow_driver_data.bme680 =
		i2c_new_device(main_adapter, &mangoh_yellow_bme680_devinfo);
        i2c_put_adapter(main_adapter);
	if (!mangoh_yellow_driver_data.bme680) {
		dev_err(&pdev->dev, "Gas/Humidity/Temp sensor is missing\n");
		ret = -ENODEV;
                goto cleanup;
	}      

        /* Map the I2C BMI088 gyro/accel sensor */
	dev_dbg(&pdev->dev, "mapping bmi088 gyro/accel sensor\n"); 
        mangoh_yellow_driver_data.bmi088a =
		i2c_new_device(main_adapter, &mangoh_yellow_bmi088a_devinfo);
        i2c_put_adapter(main_adapter);
	if (!mangoh_yellow_driver_data.bmi088a) {
		dev_err(&pdev->dev, "bmi088 accel sensor is missing\n");
		ret = -ENODEV;
                goto cleanup;
	}   

        mangoh_yellow_driver_data.bmi088g =
		i2c_new_device(main_adapter, &mangoh_yellow_bmi088g_devinfo);
        i2c_put_adapter(main_adapter);
	if (!mangoh_yellow_driver_data.bmi088g) {
		dev_err(&pdev->dev, "bmi088 gyro sensor is missing\n");
		ret = -ENODEV;
                goto cleanup;
	}

cleanup:
	i2c_put_adapter(main_adapter);
	if (ret != 0)
		mangoh_yellow_remove(pdev);
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

static int mangoh_yellow_remove(struct platform_device* pdev)
{
	struct mangoh_yellow_driver_data *dd = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "Removing mangoh red platform device\n");

        try_unregister_i2c_device(dd->bmi088a);
        try_unregister_i2c_device(dd->bmi088g);
        try_unregister_i2c_device(dd->bme680);

	return 0;
}

static int __init mangoh_yellow_init(void)
{
	platform_driver_register(&mangoh_yellow_driver);
	printk(KERN_DEBUG "mangoh: registered platform driver\n");

	mangoh_yellow_pdata.board_rev = MANGOH_YELLOW_BOARD_REV_DV1;

	if (platform_device_register(&mangoh_yellow_device)) {
		platform_driver_unregister(&mangoh_yellow_driver);
		return -ENODEV;
	}

	return 0;
}

static void __exit mangoh_yellow_exit(void)
{
	platform_device_unregister(&mangoh_yellow_device);
	platform_driver_unregister(&mangoh_yellow_driver);
}

module_init(mangoh_yellow_init);
module_exit(mangoh_yellow_exit);

MODULE_ALIAS(PLATFORM_MODULE_PREFIX "mangoh yellow");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sierra Wireless");
MODULE_DESCRIPTION("Add devices on mangOH Yellow hardware board");
MODULE_VERSION("1.0");
