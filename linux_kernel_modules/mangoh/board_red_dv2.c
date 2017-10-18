#define DEBUG
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c/pca954x.h>
//#include <linux/i2c/sx150x.h>
#include <linux/gpio.h>

#include "mangoh.h"
#include "lsm6ds3_platform_data.h"
#include "ltc294x-platform-data.h"
#include "bq24190-platform-data.h"
/*
 *-----------------------------------------------------------------------------
 * Type definitions
 *-----------------------------------------------------------------------------
 */
struct red_dv2_platform_data {
	struct i2c_client* i2c_switch;
	struct i2c_client* gpio_expander;
	struct i2c_client* accelerometer;
	struct i2c_client* pressure;
	struct i2c_client* battery_gauge;
        struct i2c_client* battery_charger;
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

/*static struct sx150x_platform_data red_dv2_expander_platform_data = {
	.gpio_base         = -1,
	.oscio_is_gpo      = false,
	.io_pullup_ena     = 0,
	.io_pulldn_ena     = 0,
	.io_open_drain_ena = 0,
	.io_polarity       = 0,
	.irq_summary       = -1,
	.irq_base          = -1,
};
*/
/*static const struct i2c_board_info red_dv2_expander_devinfo = {
	I2C_BOARD_INFO("sx1509q", 0x3e),
	.platform_data = &red_dv2_expander_platform_data,
	.irq = 0,
};
*/
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

static struct ltc294x_platform_data red_dv2_battery_gauge_platform_data = {
	.chip_id = LTC2942_ID,
	.r_sense = 18,
	.prescaler_exp = 32,
        .name = "LTC2942",
};
static struct i2c_board_info red_dv2_battery_gauge_devinfo = {
	I2C_BOARD_INFO("ltc2942", 0x64),
	.platform_data = &red_dv2_battery_gauge_platform_data,
};

static struct bq24190_platform_data red_dv2_battery_charger_platform_data = {
        .gpio_int = 49,
};



static struct i2c_board_info red_dv2_battery_charger_devinfo = {
	I2C_BOARD_INFO("bq24190", 0x6B),
        .platform_data = &red_dv2_battery_charger_platform_data,
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
#if 0

//static int get_sx150x_gpio_base(struct i2c_client *client)
{
	/*
	 * This is kind of a hack. It depends on the fact that we know
	 * that the clientdata of the gpio expander is a struct
	 * sx150x_chip (type is private to the sx150x driver) and
	 * knowing that the first element of that struct is a struct
	 * gpio_chip.
	 */
	struct gpio_chip *expander = i2c_get_clientdata(client);
	return expander->base;
}
#endif

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

	/* Map GPIO expander */
	#if 0
            dev_dbg(&pdev->dev, "mapping gpio expander\n");
       
	/*
	 * GPIOEXP_INT1 goes to GPIO32 on the CF3 inner ring which maps to
	 * GPIO30 in the WP85.
	 */
	red_dv2_expander_platform_data.irq_summary = gpio_to_irq(30);
	/*
	 * Currently we just hard code an IRQ base that was tested to have a
	 * contiguous set of 16 available IRQs. TODO: Is there a better way to
	 * find a contiguous set of IRQs?
	 */
	red_dv2_expander_platform_data.irq_base = 347;
	adapter = i2c_get_adapter(RED_DV2_I2C_SW_BASE_ADAPTER_ID +
				  RED_DV2_I2C_SW_PORT_GPIO_EXPANDER);
	if (!adapter) {
		dev_err(&pdev->dev, "No I2C bus %d.\n", RED_DV2_I2C_SW_PORT_GPIO_EXPANDER);
		return -ENODEV;
	}
	pdata->gpio_expander = i2c_new_device(adapter, &red_dv2_expander_devinfo);
	if (!pdata->gpio_expander) {
		dev_err(&pdev->dev, "GPIO expander is missing\n");
		return -ENODEV;
	}
        #endif

	/* Map the I2C LSM6DS3 accelerometer */
	dev_dbg(&pdev->dev, "mapping lsm6ds3 accelerometer\n");
	/* Pin 12 of the gpio expander is connected to INT2 of the lsm6ds3 */
	//red_dv2_accelerometer_devinfo.irq =
	//	gpio_to_irq(get_sx150x_gpio_base(pdata->gpio_expander) + 12);
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

	/* Map the I2C ltc2942 battery gauge */
	dev_dbg(&pdev->dev, "mapping ltc2942 battery gauge\n");
	adapter = i2c_get_adapter(RED_DV2_I2C_SW_BASE_ADAPTER_ID +
				  RED_DV2_I2C_SW_PORT_USB_HUB);
	if (!adapter) {
		dev_err(&pdev->dev, "No I2C bus %d.\n", RED_DV2_I2C_SW_BASE_ADAPTER_ID);
		return -ENODEV;
	}
	pdata->battery_gauge = i2c_new_device(adapter, &red_dv2_battery_gauge_devinfo);
	if (!pdata->battery_gauge) {
		dev_err(&pdev->dev, "battery gauge is missing\n");
		return -ENODEV;
	}


        /*Map the I2C BQ24296 driver: for now use the BQ24190 driver code */
        dev_dbg(&pdev->dev, "mapping bq24296 driver\n");
	adapter = i2c_get_adapter(RED_DV2_I2C_SW_BASE_ADAPTER_ID +
				  RED_DV2_I2C_SW_PORT_USB_HUB);
	if(!adapter) {
		dev_err(&pdev->dev, "No I2C bus %d.\n", RED_DV2_I2C_SW_BASE_ADAPTER_ID);
		return -ENODEV;
	}
        pdata->battery_charger = i2c_new_device(adapter, &red_dv2_battery_charger_devinfo);
        if (!pdata->battery_gauge) {
		dev_err(&pdev->dev, "battery charger is missing\n");
		return -ENODEV;
	}	
        
	/*
	 * TODO:
	 * Pressure Sensor: 0x76
	 *    chip is bmp280 by Bosch which has a driver in the mainline kernel
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

static int red_dv2_unmap(struct platform_device* pdev)
{
	struct red_dv2_platform_data* pdata = dev_get_platdata(&pdev->dev);

	i2c_unregister_device(pdata->battery_gauge);
	i2c_put_adapter(pdata->battery_gauge->adapter);

	i2c_unregister_device(pdata->pressure);
	i2c_put_adapter(pdata->pressure->adapter);

	i2c_unregister_device(pdata->accelerometer);
	i2c_put_adapter(pdata->accelerometer->adapter);

	//i2c_unregister_device(pdata->gpio_expander);
	//i2c_put_adapter(pdata->gpio_expander->adapter);
        
        i2c_unregister_device(pdata->battery_charger);
	i2c_put_adapter(pdata->battery_charger->adapter);

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
