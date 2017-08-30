#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c/pca954x.h>

#include "mangoh.h"
#include "lsm6ds3_platform_data.h"

/*
 *-----------------------------------------------------------------------------
 * Type definitions
 *-----------------------------------------------------------------------------
 */
struct red_dv2_platform_data {
	struct i2c_client* i2c_switch;
	struct i2c_client* accelerometer;
	struct i2c_client* pressure;
};

struct red_dv2_device
{
	struct platform_device pdev;
	struct red_dv2_platform_data pdata;
};

/*
 *-----------------------------------------------------------------------------
 * Function declarations
 *-----------------------------------------------------------------------------
 */
static int red_dv2_map(struct platform_device *pdev);
static int red_dv2_unmap(struct platform_device* pdev);
static void red_dv2_release_device(struct device* dev);


/*
 *-----------------------------------------------------------------------------
 * Constants
 *-----------------------------------------------------------------------------
 */
#define RED_DV2_I2C_SW_BASE_ADAPTER_ID		(1)
#define RED_DV2_I2C_SW_PORT_IOT0		(0)
#define RED_DV2_I2C_SW_PORT_BATTERY_CHARGER	(1)
#define RED_DV2_I2C_SW_PORT_USB_HUB		(1)
#define RED_DV2_I2C_SW_PORT_GPIO_EXPANDER	(2)
#define RED_DV2_I2C_SW_PORT_EXP			(3)	/* expansion header */

/*
 *-----------------------------------------------------------------------------
 * Variables
 *-----------------------------------------------------------------------------
 */
const struct mangoh_descriptor red_dv2_descriptor = {
	.type = "mangoh red dv2",
	.map = red_dv2_map,
	.unmap = red_dv2_unmap,
};

static struct pca954x_platform_mode red_dv2_pca954x_adap_modes[] = {
	{.adap_id=RED_DV2_I2C_SW_BASE_ADAPTER_ID + 0, .deselect_on_exit=1, .class=0},
	{.adap_id=RED_DV2_I2C_SW_BASE_ADAPTER_ID + 1, .deselect_on_exit=1, .class=0},
	{.adap_id=RED_DV2_I2C_SW_BASE_ADAPTER_ID + 2, .deselect_on_exit=1, .class=0},
	{.adap_id=RED_DV2_I2C_SW_BASE_ADAPTER_ID + 3, .deselect_on_exit=1, .class=0},
};
static struct pca954x_platform_data red_dv2_pca954x_pdata = {
	red_dv2_pca954x_adap_modes,
	ARRAY_SIZE(red_dv2_pca954x_adap_modes),
};
static const struct i2c_board_info red_dv2_pca954x_device_info = {
	I2C_BOARD_INFO("pca9546", 0x71),
	.platform_data = &red_dv2_pca954x_pdata,
};

static struct lsm6ds3_platform_data red_dv2_accelerometer_platform_data = {
	.drdy_int_pin = 1,
};
static struct i2c_board_info red_dv2_accelerometer_devinfo = {
	I2C_BOARD_INFO("lsm6ds3", 0x6A),
	.platform_data = &red_dv2_accelerometer_platform_data,
};

static struct i2c_board_info red_dv2_pressure_devinfo = {
	I2C_BOARD_INFO("bmp280", 0x76),
};

static const char platform_device_name[] = "mangoh red dv2";


/*
 *-----------------------------------------------------------------------------
 * Public function definitions
 *-----------------------------------------------------------------------------
 */
int red_dv2_create_device(struct platform_device** d)
{
	struct red_dv2_device *dv2 = kzalloc(sizeof(struct red_dv2_device), GFP_KERNEL);
	if (!dv2)
		return -ENOMEM;

	dv2->pdev.name = platform_device_name;
	dv2->pdev.id = -1;
	dv2->pdev.dev.platform_data = &dv2->pdata;
	dv2->pdev.dev.release = red_dv2_release_device;

	*d = &dv2->pdev;
	return 0;
}

/*
 *-----------------------------------------------------------------------------
 * Static function definitions
 *-----------------------------------------------------------------------------
 */
static int red_dv2_map(struct platform_device *pdev)
{
	struct red_dv2_platform_data* pdata = dev_get_platdata(&pdev->dev);

	/* Get the main i2c adapter */
	struct i2c_adapter* adapter = i2c_get_adapter(0);
	if (!adapter) {
		dev_err(&pdev->dev, "Failed to get I2C adapter 0.\n");
		return -ENODEV;
	}

	/* Map the I2C switch */
	dev_dbg(&pdev->dev, "mapping i2c switch\n");
	pdata->i2c_switch = i2c_new_device(adapter, &red_dv2_pca954x_device_info);
	if (!pdata->i2c_switch) {
		dev_err(
			&pdev->dev,
			"Failed to register %s\n",
			red_dv2_pca954x_device_info.type);
		return -ENODEV;
	}

	/* Map the I2C LSM6DS3 accelerometer */
	dev_dbg(&pdev->dev, "mapping lsm6ds3 accelerometer\n");
	/* Pin 12 of the gpio expander is connected to INT2 of the lsm6ds3 */
	/* Can't use the irq because the GPIO expander isn't controlled by a kernel driver
	red_dv2_accelerometer_devinfo.irq =
		gpio_to_irq(get_sx150x_gpio_base(pdata->gpio_expander) + 12);
	*/
	adapter = i2c_get_adapter(0);
	if (!adapter) {
		dev_err(&pdev->dev, "No I2C bus %d.\n", RED_DV2_I2C_SW_BASE_ADAPTER_ID);
		return -ENODEV;
	}
	pdata->accelerometer = i2c_new_device(adapter, &red_dv2_accelerometer_devinfo);
	if (!pdata->accelerometer) {
		dev_err(&pdev->dev, "Accelerometer is missing\n");
		return -ENODEV;
	}

	/* Map the I2C BMP280 pressure sensor */
	dev_dbg(&pdev->dev, "mapping bmp280 pressure sensor\n");
	adapter = i2c_get_adapter(0);
	if (!adapter) {
		dev_err(&pdev->dev, "No I2C bus %d.\n", RED_DV2_I2C_SW_BASE_ADAPTER_ID);
		return -ENODEV;
	}
	pdata->pressure = i2c_new_device(adapter, &red_dv2_pressure_devinfo);
	if (!pdata->pressure) {
		dev_err(&pdev->dev, "Pressure sensor is missing\n");
		return -ENODEV;
	}
	/*
	 * TODO:
	 * SX1509 GPIO expander: 0x3E
	 *    There is a driver in the WP85 kernel, but the gpiolib
	 *    infrastructure of the WP85 kernel does not allow the expander
	 *    GPIOs to be used in sysfs due to a hardcoded translation table.
	 * 3503 USB Hub: 0x08
	 *    Looks like there is a driver in the wp85 kernel source at drivers/usb/misc/usb3503.c
	 *    I'm not really sure what benefit is achieved through using this driver.
	 * Battery Gauge: 0x64
	 *    chip is LTC2942 which is made by linear technologies (now Analog
	 *    Devices). There is a kernel driver in the linux-power-supply
	 *    repository in the for-next branch.
	 * Buck & Battery Charger: 0x6B
	 *    chip is BQ24296RGER
	 */
	return 0;
}

static int red_dv2_unmap(struct platform_device* pdev)
{
	struct red_dv2_platform_data* pdata = dev_get_platdata(&pdev->dev);

	i2c_unregister_device(pdata->pressure);
	i2c_put_adapter(pdata->pressure->adapter);

	i2c_unregister_device(pdata->accelerometer);
	i2c_put_adapter(pdata->accelerometer->adapter);

	i2c_unregister_device(pdata->i2c_switch);
	i2c_put_adapter(pdata->i2c_switch->adapter);

	return 0;
}

static void red_dv2_release_device(struct device* dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct red_dv2_device *dv2 = container_of(pdev, struct red_dv2_device, pdev);
	kfree(dv2);
}
