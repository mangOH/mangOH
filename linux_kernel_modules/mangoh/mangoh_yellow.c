#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/gpio/driver.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c/pca954x.h>
#include <linux/i2c/sx150x.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_data/at24.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

#include "bq25601-platform-data.h"
#include "bq27426-platform-data.h"
#include "bq27xxx_battery.h"
#include "expander.h"
#include "mangoh_common.h"

/*
 *-----------------------------------------------------------------------------
 * Constants
 *-----------------------------------------------------------------------------
 */
#define MANGOH_YELLOW_I2C_SW_BUS_BASE	(PRIMARY_I2C_BUS + 1)
#define MANGOH_YELLOW_I2C_BUS_PORT0	(MANGOH_YELLOW_I2C_SW_BUS_BASE + 0)
#define MANGOH_YELLOW_I2C_BUS_PORT1 	(MANGOH_YELLOW_I2C_SW_BUS_BASE + 1)
#define MANGOH_YELLOW_I2C_BUS_PORT2	(MANGOH_YELLOW_I2C_SW_BUS_BASE + 2)
#define MANGOH_YELLOW_I2C_BUS_PORT3	(MANGOH_YELLOW_I2C_SW_BUS_BASE + 3)

/*
 *-----------------------------------------------------------------------------
 * Types
 *-----------------------------------------------------------------------------
 */
enum board_rev {
	BOARD_REV_UNKNOWN = 0,
	BOARD_REV_DV2,
	BOARD_REV_DV3,
	BOARD_REV_NUM_OF, /* keep as last definition */
};

/*
 *-----------------------------------------------------------------------------
 * Static Function Declarations
 *-----------------------------------------------------------------------------
 */
static void mangoh_yellow_release(struct device* dev);
static int mangoh_yellow_probe(struct platform_device* pdev);
static int mangoh_yellow_remove(struct platform_device* pdev);
static void mangoh_yellow_expander_release(struct device *dev);
static void eeprom_setup(struct memory_accessor *mem_acc, void *context);

/*
 *-----------------------------------------------------------------------------
 * Variables
 *-----------------------------------------------------------------------------
 */

static char * const board_rev_strings[BOARD_REV_NUM_OF] = {
	[BOARD_REV_UNKNOWN] = "unknown",
	[BOARD_REV_DV2] = "dv2",
	[BOARD_REV_DV3] = "dv3",
};

static char *force_revision = "";
module_param(force_revision, charp, S_IRUGO);
MODULE_PARM_DESC(force_revision, "Skip board revision detection and assume a specific revision");

static bool allow_eeprom_write = false;
module_param(allow_eeprom_write, bool, S_IRUGO);
MODULE_PARM_DESC(allow_eeprom_write, "Should the EEPROM be writeable by root");

static struct platform_driver mangoh_yellow_driver = {
	.probe = mangoh_yellow_probe,
	.remove = mangoh_yellow_remove,
	.driver  = {
		.name = "mangoh yellow",
		.owner = THIS_MODULE,
		.bus = &platform_bus_type,
	},
};


static struct mangoh_yellow_driver_data {
	struct i2c_client* eeprom;
	struct i2c_client* i2c_switch;
	struct i2c_client* imu;
	struct i2c_client* rtc;
	struct i2c_client* light;
	struct i2c_client* magnetometer;
	struct i2c_client* battery_gauge;
	struct i2c_client* battery_charger;
	struct i2c_client* gpio_expander;
	bool expander_registered;
	enum board_rev board_rev;
} mangoh_yellow_driver_data = {
	.expander_registered = false,
	.board_rev = BOARD_REV_UNKNOWN,
};

static struct platform_device mangoh_yellow_device = {
	.name = "mangoh yellow",
	.id = -1,
	.dev = {
		.release = mangoh_yellow_release,
	},
};

static struct pca954x_platform_mode mangoh_yellow_pca954x_adap_modes[] = {
	{.adap_id=MANGOH_YELLOW_I2C_BUS_PORT0, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_YELLOW_I2C_BUS_PORT1, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_YELLOW_I2C_BUS_PORT2, .deselect_on_exit=1, .class=0},
	{.adap_id=MANGOH_YELLOW_I2C_BUS_PORT3, .deselect_on_exit=1, .class=0},
};
static struct pca954x_platform_data mangoh_yellow_pca954x_platform_data = {
	mangoh_yellow_pca954x_adap_modes,
	ARRAY_SIZE(mangoh_yellow_pca954x_adap_modes),
};
static const struct i2c_board_info mangoh_yellow_pca954x_device_info = {
	I2C_BOARD_INFO("pca9546",0x71),
	.platform_data = &mangoh_yellow_pca954x_platform_data,
};

static struct sx150x_platform_data mangoh_yellow_gpio_expander_platform_data = {
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
static const struct i2c_board_info mangoh_yellow_gpio_expander_devinfo = {
	I2C_BOARD_INFO("sx1509q",0x3e),
	.platform_data = &mangoh_yellow_gpio_expander_platform_data,
	.irq = 0,
};
static struct i2c_board_info mangoh_yellow_imu_devinfo = {
	I2C_BOARD_INFO("bmi160", 0x68),
};
static struct i2c_board_info mangoh_yellow_rtc_devinfo = {
	I2C_BOARD_INFO("pcf85063", 0x51),
};
static struct i2c_board_info mangoh_yellow_battery_charger_devinfo = {
	I2C_BOARD_INFO("bq25601", 0x6b),
};
static struct bq27426_platform_data mangoh_yellow_battery_gauge_platform_data = {
	.energy_full_design_uwh = 4800000,
	.charge_full_design_uah = 1200000,
	.voltage_min_design_uv =  3300000,
};
static struct i2c_board_info mangoh_yellow_battery_gauge_devinfo = {
	I2C_BOARD_INFO("bq27426", 0x55),
	.platform_data = &mangoh_yellow_battery_gauge_platform_data,
};
static struct i2c_board_info mangoh_yellow_magnetometer_devinfo = {
	I2C_BOARD_INFO("bmm150_magn", 0x10),
};


static struct i2c_board_info mangoh_yellow_light_devinfo = {
	I2C_BOARD_INFO("opt3002", 0x44),
	.irq = 0,
};
static struct expander_platform_data mangoh_yellow_expander_platform_data = {
	.gpio_expander_base = -1,
};
static struct platform_device mangoh_yellow_expander = {
	.name = "expander",
	.dev = {
		.platform_data = &mangoh_yellow_expander_platform_data,
		.release = mangoh_yellow_expander_release,
	},
};

/*
 * The EEPROM is marked as read-only to prevent accidental writes. The mangOH
 * Yellow has the write protect (WP) pin pulled high which has the effect of
 * making the upper 1/4 of the address space of the EEPROM write protected by
 * hardware.
 */
static struct at24_platform_data mangoh_yellow_eeprom_data = {
	.byte_len = 4096,
	.page_size = 32,
	.flags = (AT24_FLAG_ADDR16),
	.setup = eeprom_setup,
};
static struct i2c_board_info mangoh_yellow_eeprom_info = {
	I2C_BOARD_INFO("at24", 0x50),
	.platform_data = &mangoh_yellow_eeprom_data,
};

static void eeprom_setup(struct memory_accessor *mem_acc, void *context)
{
	const size_t eeprom_size = mangoh_yellow_eeprom_data.byte_len;
	const char *dv3_string = "mangOH Yellow DV3";
	u8 *data;

	if (mangoh_yellow_driver_data.board_rev != BOARD_REV_UNKNOWN) {
		/*
		 * The board revision must have been forced via the kernel
		 * module parameter, so don't bother reading out the eeprom
		 * data.
		 */
		return;
	}

	data = kmalloc(eeprom_size, GFP_KERNEL);
	if (data == NULL) {
		pr_err("Couldn't allocate memory for EEPROM data");
		return;
	}

	if (mem_acc->read(mem_acc, data, 0, eeprom_size) != eeprom_size) {
		pr_err("Error reading from board identification EEPROM.\n");
		goto done;
	}

	if (strcmp(data, dv3_string) == 0)
		mangoh_yellow_driver_data.board_rev = BOARD_REV_DV3;

	if (mangoh_yellow_driver_data.board_rev != BOARD_REV_UNKNOWN) {
		pr_info("Detected mangOH Yellow board revision: %s\n",
			board_rev_strings[mangoh_yellow_driver_data.board_rev]);
	} else {
		const char *eeprom_string = "<not printable>";
		int i;
		for (i = 0; i < eeprom_size; i++) {
			if (data[i] == '\0') {
				eeprom_string = (char *)data;
				break;
			}
			if (!isprint(data[i]))
				break;
		}
		pr_warn("EEPROM contents did not match any known board signature: \"%s\"\n",
			eeprom_string);
	}

done:
	kfree(data);
}

static void mangoh_yellow_release(struct device* dev)
{
	/* Nothing alloc'd, so nothign to free */
}

static int mangoh_yellow_probe(struct platform_device* pdev)
{
	int ret = 0;
	struct i2c_adapter *i2c_adapter_primary, *i2c_adapter_port1 = NULL,
		*i2c_adapter_port2 = NULL, *i2c_adapter_port3 = NULL;
	struct gpio_chip *gpio_expander;

	dev_info(&pdev->dev, "%s(): probe\n", __func__);

	i2c_adapter_primary = i2c_get_adapter(PRIMARY_I2C_BUS);
	if (!i2c_adapter_primary) {
		dev_err(&pdev->dev, "Failed to get primary I2C adapter (%d).\n",
			PRIMARY_I2C_BUS);
		ret = -ENODEV;
		goto done;
	}

	/*
	 * This is a workaround that needs to be tested more for issue first
	 * seen on mangOH Green
	 */
	msleep(5000);

	platform_set_drvdata(pdev, &mangoh_yellow_driver_data);

	/* map the EEPROM */
	dev_dbg(&pdev->dev, "mapping eeprom\n");
	if (!allow_eeprom_write)
		mangoh_yellow_eeprom_data.flags |= AT24_FLAG_READONLY;
	mangoh_yellow_driver_data.eeprom =
		i2c_new_device(i2c_adapter_primary, &mangoh_yellow_eeprom_info);
	if (!mangoh_yellow_driver_data.eeprom) {
		if (mangoh_yellow_driver_data.board_rev == BOARD_REV_UNKNOWN) {
			/*
			 * Assume board is a DV2 since the EEPROM was missing on
			 * that revision
			 */
			mangoh_yellow_driver_data.board_rev = BOARD_REV_DV2;
		} else if (mangoh_yellow_driver_data.board_rev !=
			   BOARD_REV_DV2) {
			/*
			 * If the revision was forced to something other than
			 * DV2, the EEPROM is expected to be present.
			 */
			dev_err(&pdev->dev, "Failed to register %s\n",
				mangoh_yellow_eeprom_info.type);
			ret = -ENODEV;
			goto cleanup;
		}
	}

	/*
	 * If the board revision still isn't known after reading the EEPROM,
	 * then abort.
	 */
	if (mangoh_yellow_driver_data.board_rev == BOARD_REV_UNKNOWN) {
		dev_err(&pdev->dev, "mangOH Yellow board revision is unknown\n");
		ret = -EINVAL;
		goto done;
	}

	/* Map the I2C switch */
	dev_dbg(&pdev->dev, "mapping i2c switch\n");
	mangoh_yellow_driver_data.i2c_switch =
		i2c_new_device(i2c_adapter_primary,
			       &mangoh_yellow_pca954x_device_info);
	if (!mangoh_yellow_driver_data.i2c_switch) {
		dev_err(&pdev->dev, "Failed to register %s\n",
			mangoh_yellow_pca954x_device_info.type);
		ret = -ENODEV;
		goto cleanup;
	}

	i2c_adapter_port1 = i2c_get_adapter(MANGOH_YELLOW_I2C_BUS_PORT1);
	i2c_adapter_port2 = i2c_get_adapter(MANGOH_YELLOW_I2C_BUS_PORT2);
	i2c_adapter_port3 = i2c_get_adapter(MANGOH_YELLOW_I2C_BUS_PORT3);
	if (!i2c_adapter_port1 || !i2c_adapter_port2 || !i2c_adapter_port3) {
		dev_err(&pdev->dev,
			"Couldn't get necessary I2C buses downstream of I2C switch\n");
		ret = -ENODEV;
		goto cleanup;
	}

	/* Map the GPIO expander */
	dev_dbg(&pdev->dev, "mapping the gpio expander\n");
	mangoh_yellow_driver_data.gpio_expander =
		i2c_new_device(i2c_adapter_port3,
			       &mangoh_yellow_gpio_expander_devinfo);
	if (!mangoh_yellow_driver_data.gpio_expander) {
		dev_err(&pdev->dev, "Failed to register %s\n",
			mangoh_yellow_gpio_expander_devinfo.type);
		ret = -ENODEV;
		goto cleanup;
	}

	gpio_expander = i2c_get_clientdata(
		mangoh_yellow_driver_data.gpio_expander);


	/* Map the Expander gpios as hardcoded functions */
	mangoh_yellow_expander_platform_data.gpio_expander_base =
		gpio_expander->base;
	ret = platform_device_register(&mangoh_yellow_expander);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"Failed to register expander gpios for hardcoding\n");
		goto cleanup;
	}
	mangoh_yellow_driver_data.expander_registered = true;

	/* Map the I2C BMI160 IMU for gyro/accel/temp sensor */
	dev_dbg(&pdev->dev, "mapping bmi160 gyro/accel/temp sensor\n");
	mangoh_yellow_driver_data.imu =
		i2c_new_device(i2c_adapter_port1, &mangoh_yellow_imu_devinfo);
	if (!mangoh_yellow_driver_data.imu) {
		dev_err(&pdev->dev,
			"BMI160 IMU accel/gyro/temp sensor is missing %s\n",
			mangoh_yellow_imu_devinfo.type);
		ret = -ENODEV;
		goto cleanup;
	}

	/*
	 * On the mangOH Yellow DV3, the magnetometer is downstream of the
	 * bmi160.
	 */
	if (mangoh_yellow_driver_data.board_rev == BOARD_REV_DV2)
	{
		/* Map the I2C BMM150 magnetometer */
		dev_dbg(&pdev->dev, "mapping bmm150 magnetometer sensor\n");
		mangoh_yellow_driver_data.magnetometer =
			i2c_new_device(i2c_adapter_port1,
				&mangoh_yellow_magnetometer_devinfo);
		if (!mangoh_yellow_driver_data.magnetometer) {
			dev_err(&pdev->dev, "BMM150 sensor is missing %s\n",
				mangoh_yellow_magnetometer_devinfo.type);
			ret = -ENODEV;
			goto cleanup;
		}
	}

	/* Map the I2C RTC */
	dev_dbg(&pdev->dev, "mapping RTC\n");
	mangoh_yellow_driver_data.rtc =
		i2c_new_device(i2c_adapter_port3, &mangoh_yellow_rtc_devinfo);
	if (!mangoh_yellow_driver_data.rtc) {
		dev_err(&pdev->dev, "RTC is missing %s\n",
			mangoh_yellow_rtc_devinfo.type);
		ret = -ENODEV;
		goto cleanup;
	}

	/* Map the I2C Battery Charger */
	dev_dbg(&pdev->dev, "mapping battery charger\n");
	mangoh_yellow_driver_data.battery_charger =
		i2c_new_device(i2c_adapter_port2,
			       &mangoh_yellow_battery_charger_devinfo);
	if (!mangoh_yellow_driver_data.battery_charger) {
		dev_err(&pdev->dev, "Battery Charger is missing %s\n",
			mangoh_yellow_battery_charger_devinfo.type);
		ret = -ENODEV;
		goto cleanup;
	}

	 /* Map the I2C Battery Gauge */
	dev_dbg(&pdev->dev, "mapping battery gauge\n");
	mangoh_yellow_driver_data.battery_gauge =
		i2c_new_device(i2c_adapter_port2,
			       &mangoh_yellow_battery_gauge_devinfo);
	if (!mangoh_yellow_driver_data.battery_gauge) {
		dev_err(&pdev->dev, "Battery Gauge is missing %s\n",
			mangoh_yellow_battery_gauge_devinfo.type);
		ret = -ENODEV;
		goto cleanup;
	}

 	/* Map the I2C Light Sensor */
	dev_dbg(&pdev->dev, "mapping Light Sensor\n");
	if (devm_gpio_request_one(&pdev->dev, CF3_GPIO36, GPIOF_DIR_IN,
				  "CF3 opt300x gpio interrupt")) {
		dev_err(&pdev->dev, "Couldn't request CF3 gpio36");
		ret = -ENODEV;
		goto cleanup;
	}
	mangoh_yellow_light_devinfo.irq = gpio_to_irq(CF3_GPIO36);
	mangoh_yellow_driver_data.light =
		i2c_new_device(i2c_adapter_port2, &mangoh_yellow_light_devinfo);
	if (!mangoh_yellow_driver_data.light) {
		dev_err(&pdev->dev, "Light Sensor is missing %s\n",
			mangoh_yellow_light_devinfo.type);
		ret = -ENODEV;
		goto cleanup;
	}

cleanup:
	i2c_put_adapter(i2c_adapter_port3);
	i2c_put_adapter(i2c_adapter_port2);
	i2c_put_adapter(i2c_adapter_port1);
	i2c_put_adapter(i2c_adapter_primary);
	if (ret != 0)
		mangoh_yellow_remove(pdev);
done:
	return ret;
}

static void safe_i2c_unregister_device(struct i2c_client *client)
{
	if (client)
		i2c_unregister_device(client);
}

static int mangoh_yellow_remove(struct platform_device* pdev)
{
	struct mangoh_yellow_driver_data *dd = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "Removing mangoh yellow platform device\n");

	safe_i2c_unregister_device(dd->imu);
	safe_i2c_unregister_device(dd->magnetometer);
	safe_i2c_unregister_device(dd->light);
	safe_i2c_unregister_device(dd->battery_charger);
	safe_i2c_unregister_device(dd->battery_gauge);
	safe_i2c_unregister_device(dd->rtc);
	if (dd->expander_registered)
		platform_device_unregister(&mangoh_yellow_expander);
	safe_i2c_unregister_device(dd->gpio_expander);
	safe_i2c_unregister_device(dd->i2c_switch);
	safe_i2c_unregister_device(dd->eeprom);
	return 0;
}


static void mangoh_yellow_expander_release(struct device *dev)
{
	/* do nothing */
}

static int __init mangoh_yellow_init(void)
{
	platform_driver_register(&mangoh_yellow_driver);
	printk(KERN_DEBUG "mangoh: registered platform driver\n");

	if (strcmp(force_revision, board_rev_strings[BOARD_REV_DV2]) == 0) {
		mangoh_yellow_driver_data.board_rev = BOARD_REV_DV2;
	} else if (strcmp(force_revision, board_rev_strings[BOARD_REV_DV3]) == 0) {
		mangoh_yellow_driver_data.board_rev = BOARD_REV_DV3;
	} else if (strcmp(force_revision, "") != 0) {
		pr_err("%s: Can't force unsupported board revision (%s)\n",
		       __func__, force_revision);
		return -ENODEV;
	}

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
