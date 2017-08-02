#define DEBUG
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c/pca954x.h>
#include <linux/i2c/sx150x.h>
#include <linux/gpio.h>

#include "mangoh.h"
#include "lsm6ds3_platform_data.h"

/*
 *-----------------------------------------------------------------------------
 * Type definitions
 *-----------------------------------------------------------------------------
 */
struct red_dv3_platform_data {
	struct i2c_client* i2c_switch;
	struct i2c_client* gpio_expander;
	struct i2c_client* accelerometer;
	struct i2c_client* pressure;
};

struct red_dv3_device
{
	struct platform_device pdev;
	struct red_dv3_platform_data pdata;
};

/*
 *-----------------------------------------------------------------------------
 * Function declarations
 *-----------------------------------------------------------------------------
 */
static int red_dv3_map(struct platform_device *pdev);
static int red_dv3_unmap(struct platform_device* pdev);
static void red_dv3_release_device(struct device* dev);


/*
 *-----------------------------------------------------------------------------
 * Constants
 *-----------------------------------------------------------------------------
 */
#define RED_DV3_I2C_SW_BASE_ADAPTER_ID		(1)
#define RED_DV3_I2C_SW_PORT_IOT0		(0)
#define RED_DV3_I2C_SW_PORT_USB_HUB		(1)
#define RED_DV3_I2C_SW_PORT_GPIO_EXPANDER	(2)
#define RED_DV3_I2C_SW_PORT_EXP			(3)	/* expansion header */

/*
 *-----------------------------------------------------------------------------
 * Variables
 *-----------------------------------------------------------------------------
 */
const struct mangoh_descriptor red_dv3_descriptor = {
	.type = "mangoh red dv3",
	.map = red_dv3_map,
	.unmap = red_dv3_unmap,
};

static struct pca954x_platform_mode red_dv3_pca954x_adap_modes[] = {
	{.adap_id=RED_DV3_I2C_SW_BASE_ADAPTER_ID + 0, .deselect_on_exit=1, .class=0},
	{.adap_id=RED_DV3_I2C_SW_BASE_ADAPTER_ID + 1, .deselect_on_exit=1, .class=0},
	{.adap_id=RED_DV3_I2C_SW_BASE_ADAPTER_ID + 2, .deselect_on_exit=1, .class=0},
	{.adap_id=RED_DV3_I2C_SW_BASE_ADAPTER_ID + 3, .deselect_on_exit=1, .class=0},
};
static struct pca954x_platform_data red_dv3_pca954x_pdata = {
	red_dv3_pca954x_adap_modes,
	ARRAY_SIZE(red_dv3_pca954x_adap_modes),
};
static const struct i2c_board_info red_dv3_pca954x_device_info = {
	I2C_BOARD_INFO("pca9546", 0x71),
	.platform_data = &red_dv3_pca954x_pdata,
};

static struct sx150x_platform_data red_dv3_expander_platform_data = {
	.gpio_base         = -1,
	.oscio_is_gpo      = false,
	.io_pullup_ena     = 0,
	.io_pulldn_ena     = 0,
	.io_open_drain_ena = 0,
	.io_polarity       = 0,
	.irq_summary       = -1,
	.irq_base          = -1,
};
static const struct i2c_board_info red_dv3_expander_devinfo = {
	I2C_BOARD_INFO("sx1509q", 0x3e),
	.platform_data = &red_dv3_expander_platform_data,
	.irq = 0,
};

static struct i2c_board_info red_dv3_accelerometer_devinfo = {
	I2C_BOARD_INFO("bmi160", 0x68),
};

static struct i2c_board_info red_dv3_pressure_devinfo = {
	I2C_BOARD_INFO("bmp280", 0x76),
};

static const char platform_device_name[] = "mangoh red dv3";


/*
 *-----------------------------------------------------------------------------
 * Public function definitions
 *-----------------------------------------------------------------------------
 */
int red_dv3_create_device(struct platform_device** d)
{
	struct red_dv3_device *dv3 = kzalloc(sizeof(struct red_dv3_device), GFP_KERNEL);
	if (!dv3)
		return -ENOMEM;

	dv3->pdev.name = platform_device_name;
	dv3->pdev.id = -1;
	dv3->pdev.dev.platform_data = &dv3->pdata;
	dv3->pdev.dev.release = red_dv3_release_device;

	*d = &dv3->pdev;
	return 0;
}

/*
 *-----------------------------------------------------------------------------
 * Static function definitions
 *-----------------------------------------------------------------------------
 */
static int red_dv3_map(struct platform_device *pdev)
{
	struct red_dv3_platform_data* pdata = dev_get_platdata(&pdev->dev);

	/* Get the main i2c adapter */
	struct i2c_adapter* adapter = i2c_get_adapter(0);
	if (!adapter) {
		dev_err(&pdev->dev, "Failed to get I2C adapter 0.\n");
		return -ENODEV;
	}

	/* Map the I2C switch */
	dev_dbg(&pdev->dev, "mapping i2c switch\n");
	pdata->i2c_switch = i2c_new_device(adapter, &red_dv3_pca954x_device_info);
	if (!pdata->i2c_switch) {
		dev_err(
			&pdev->dev,
			"Failed to register %s\n",
			red_dv3_pca954x_device_info.type);
		return -ENODEV;
	}

#if 0
	/* Map GPIO expander */
	dev_dbg(&pdev->dev, "mapping gpio expander\n");
	/*
	 * GPIOEXP_INT1 goes to GPIO32 on the CF3 inner ring which maps to
	 * GPIO30 in the WP85.
	 */
	red_dv3_expander_platform_data.irq_summary = gpio_to_irq(30);
	/*
	 * Currently we just hard code an IRQ base that was tested to have a
	 * contiguous set of 16 available IRQs. TODO: Is there a better way to
	 * find a contiguous set of IRQs?
	 */
	red_dv3_expander_platform_data.irq_base = 347;
	adapter = i2c_get_adapter(RED_DV3_I2C_SW_BASE_ADAPTER_ID +
				  RED_DV3_I2C_SW_PORT_GPIO_EXPANDER);
	if (!adapter) {
		dev_err(&pdev->dev, "No I2C bus %d.\n",
			RED_DV3_I2C_SW_BASE_ADAPTER_ID +
			RED_DV3_I2C_SW_PORT_GPIO_EXPANDER);
		return -ENODEV;
	}
	pdata->gpio_expander = i2c_new_device(adapter, &red_dv3_expander_devinfo);
	if (!pdata->gpio_expander) {
		dev_err(&pdev->dev, "GPIO expander is missing\n");
		return -ENODEV;
	}
#endif // GPIO expander not properly supported

	/* Map the I2C BMI160 accelerometer */
	dev_dbg(&pdev->dev, "mapping bmi160 accelerometer\n");
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
	pdata->accelerometer = i2c_new_device(adapter, &red_dv3_accelerometer_devinfo);
	if (!pdata->accelerometer) {
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
	pdata->pressure = i2c_new_device(adapter, &red_dv3_pressure_devinfo);
	if (!pdata->pressure) {
		dev_err(&pdev->dev, "Pressure sensor is missing\n");
		return -ENODEV;
	}
	/*
	 * TODO:
	 * 3503 USB Hub: 0x08
	 *    Looks like there is a driver in the wp85 kernel source at drivers/usb/misc/usb3503.c
	 *    I'm not really sure what benefit is achieved through using this driver.
	 * Battery Gauge: 0x64
	 *    chip is LTC29421 which is made by linear technologies (now Analog
	 *    Devices). I haven't found a linux kernel driver.
	 * Buck & Battery Charger: 0x6B
	 *    chip is BQ24296RGER
	 */
	return 0;
}

static int red_dv3_unmap(struct platform_device* pdev)
{
	struct red_dv3_platform_data* pdata = dev_get_platdata(&pdev->dev);

	i2c_unregister_device(pdata->pressure);
	i2c_put_adapter(pdata->pressure->adapter);

	i2c_unregister_device(pdata->accelerometer);
	i2c_put_adapter(pdata->accelerometer->adapter);

#if 0
	i2c_unregister_device(pdata->gpio_expander);
	i2c_put_adapter(pdata->gpio_expander->adapter);
#endif // GPIO expander not properly supported

	i2c_unregister_device(pdata->i2c_switch);
	i2c_put_adapter(pdata->i2c_switch->adapter);

	return 0;
}

static void red_dv3_release_device(struct device* dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct red_dv3_device *dv3 = container_of(pdev, struct red_dv3_device, pdev);
	kfree(dv3);
}
