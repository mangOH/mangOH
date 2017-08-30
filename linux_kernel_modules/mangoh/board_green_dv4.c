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
struct green_dv4_platform_data {
	struct i2c_client* i2c_switch;
	struct i2c_client* accelerometer;
};

struct green_dv4_device
{
    struct platform_device pdev;
    struct green_dv4_platform_data pdata;
};

/*
 *-----------------------------------------------------------------------------
 * Function declarations
 *-----------------------------------------------------------------------------
 */
static int green_dv4_map(struct platform_device *pdev);
static int green_dv4_unmap(struct platform_device* pdev);
static void green_dv4_release_device(struct device* dev);


/*
 *-----------------------------------------------------------------------------
 * Constants
 *-----------------------------------------------------------------------------
 */
#define GREEN_DV4_I2C_SW_BASE_ADAPTER_ID	(1)
#define GREEN_DV4_I2C_SW_PORT_IOT0		(0)
#define GREEN_DV4_I2C_SW_PORT_IOT1		(1)
#define GREEN_DV4_I2C_SW_PORT_IOT2		(2)
#define GREEN_DV4_I2C_SW_PORT_USB_HUB		(3)
#define GREEN_DV4_I2C_SW_PORT_GPIO_EXPANDER1	(4)
#define GREEN_DV4_I2C_SW_PORT_GPIO_EXPANDER2	(5)
#define GREEN_DV4_I2C_SW_PORT_GPIO_EXPANDER3    (6)
#define GREEN_DV4_I2C_SW_PORT_BATTERY_CHARGER   (7)

/*
 *-----------------------------------------------------------------------------
 * Variables
 *-----------------------------------------------------------------------------
 */
const struct mangoh_descriptor green_dv4_descriptor = {
	.type = "mangoh green dv4",
	.map = green_dv4_map,
	.unmap = green_dv4_unmap,
};

static struct pca954x_platform_mode green_dv4_pca954x_adap_modes[] = {
	{.adap_id=GREEN_DV4_I2C_SW_BASE_ADAPTER_ID + 0, .deselect_on_exit=1, .class=0},
	{.adap_id=GREEN_DV4_I2C_SW_BASE_ADAPTER_ID + 1, .deselect_on_exit=1, .class=0},
	{.adap_id=GREEN_DV4_I2C_SW_BASE_ADAPTER_ID + 2, .deselect_on_exit=1, .class=0},
	{.adap_id=GREEN_DV4_I2C_SW_BASE_ADAPTER_ID + 3, .deselect_on_exit=1, .class=0},
	{.adap_id=GREEN_DV4_I2C_SW_BASE_ADAPTER_ID + 4, .deselect_on_exit=1, .class=0},
	{.adap_id=GREEN_DV4_I2C_SW_BASE_ADAPTER_ID + 5, .deselect_on_exit=1, .class=0},
	{.adap_id=GREEN_DV4_I2C_SW_BASE_ADAPTER_ID + 6, .deselect_on_exit=1, .class=0},
	{.adap_id=GREEN_DV4_I2C_SW_BASE_ADAPTER_ID + 7, .deselect_on_exit=1, .class=0},
};
static struct pca954x_platform_data green_dv4_pca954x_pdata = {
	green_dv4_pca954x_adap_modes,
	ARRAY_SIZE(green_dv4_pca954x_adap_modes),
};
static const struct i2c_board_info green_dv4_pca954x_device_info = {
	I2C_BOARD_INFO("pca9548", 0x71),
	.platform_data = &green_dv4_pca954x_pdata,
};

static struct lsm6ds3_platform_data green_dv4_accelerometer_platform_data = {
	.drdy_int_pin = 1,
};
static struct i2c_board_info green_dv4_accelerometer_devinfo = {
	I2C_BOARD_INFO("lsm6ds3", 0x6A),
	.platform_data = &green_dv4_accelerometer_platform_data,
};

static const char platform_device_name[] = "mangoh green dv4";


/*
 *-----------------------------------------------------------------------------
 * Public function definitions
 *-----------------------------------------------------------------------------
 */
int green_dv4_create_device(struct platform_device** d)
{
	struct green_dv4_device *dv4 = kzalloc(sizeof(struct green_dv4_device), GFP_KERNEL);
	if (!dv4)
		return -ENOMEM;

	dv4->pdev.name = platform_device_name;
	dv4->pdev.id = -1;
	dv4->pdev.dev.platform_data = &dv4->pdata;
	dv4->pdev.dev.release = green_dv4_release_device;

	*d = &dv4->pdev;
	return 0;
}

/*
 *-----------------------------------------------------------------------------
 * Static function definitions
 *-----------------------------------------------------------------------------
 */
static int green_dv4_map(struct platform_device *pdev)
{
	struct green_dv4_platform_data* pdata = dev_get_platdata(&pdev->dev);

	/* Get the main i2c adapter */
	struct i2c_adapter* adapter = i2c_get_adapter(0);
	if (!adapter) {
		dev_err(&pdev->dev, "Failed to get I2C adapter 0.\n");
		return -ENODEV;
	}

	/* Map the I2C switch */
	dev_dbg(&pdev->dev, "mapping i2c switch\n");
	pdata->i2c_switch = i2c_new_device(adapter, &green_dv4_pca954x_device_info);
	if (!pdata->i2c_switch) {
		dev_err(
			&pdev->dev,
			"Failed to register %s\n",
			green_dv4_pca954x_device_info.type);
		return -ENODEV;
	}

	/* Map the I2C LSM6DS3 accelerometer */
	dev_dbg(&pdev->dev, "mapping lsm6ds3 accelerometer\n");
	/* Pin 10 of gpio expander 2 is connected to INT2 of the lsm6ds3 */
	/* Can't use the irq because the GPIO expander isn't controlled by a kernel driver
	green_dv4_accelerometer_devinfo.irq =
		gpio_to_irq(get_sx150x_gpio_base(pdata->gpio_expander2) + 10);
	*/
	adapter = i2c_get_adapter(0);
	if (!adapter) {
		dev_err(&pdev->dev, "No I2C bus %d.\n", GREEN_DV4_I2C_SW_BASE_ADAPTER_ID);
		return -ENODEV;
	}
	pdata->accelerometer = i2c_new_device(adapter, &green_dv4_accelerometer_devinfo);
	if (!pdata->accelerometer) {
		dev_err(&pdev->dev, "Accelerometer is missing\n");
		return -ENODEV;
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

	return 0;
}

static int green_dv4_unmap(struct platform_device* pdev)
{
	struct green_dv4_platform_data* pdata = dev_get_platdata(&pdev->dev);

	i2c_unregister_device(pdata->accelerometer);
	i2c_put_adapter(pdata->accelerometer->adapter);

	i2c_unregister_device(pdata->i2c_switch);
	i2c_put_adapter(pdata->i2c_switch->adapter);

	return 0;
}

static void green_dv4_release_device(struct device* dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct green_dv4_device *dv4 = container_of(pdev, struct green_dv4_device, pdev);
	kfree(dv4);
}
