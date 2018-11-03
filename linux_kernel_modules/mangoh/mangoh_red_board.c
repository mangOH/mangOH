#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c/pca954x.h>
#include <linux/i2c/sx150x.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/gpio/driver.h>
#include <linux/platform_data/at24.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>

#include "ltc294x-platform-data.h"
#include "bq24190-platform-data.h"
#include "bmi160_pdata.h"

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
	MANGOH_RED_BOARD_REV_UNKNOWN = 0,
	MANGOH_RED_BOARD_REV_DV5,
	MANGOH_RED_BOARD_REV_DV6,

	MANGOH_RED_BOARD_REV_NUM_OF,
};

/*
 *-----------------------------------------------------------------------------
 * Static Function Declarations
 *-----------------------------------------------------------------------------
 */
static void eeprom_setup(struct memory_accessor *mem_acc, void *context);
static void mangoh_red_release(struct device* dev);
static int mangoh_red_probe(struct platform_device* pdev);
static int mangoh_red_remove(struct platform_device* pdev);
static void mangoh_red_led_release(struct device *dev);
#ifdef ENABLE_IOT_SLOT
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
#endif /* ENABLE_IOT_SLOT */

/*
 *-----------------------------------------------------------------------------
 * Variables
 *-----------------------------------------------------------------------------
 */
static char *revision = "unknown";
module_param(revision, charp, S_IRUGO);
MODULE_PARM_DESC(revision, "mangOH Red board revision");

static bool allow_eeprom_write = false;
module_param(allow_eeprom_write, bool, S_IRUGO);
MODULE_PARM_DESC(allow_eeprom_write, "Should the EEPROM be writeable by root");

static char * const board_rev_strings[MANGOH_RED_BOARD_REV_NUM_OF] = {
	[MANGOH_RED_BOARD_REV_UNKNOWN] = "unknown",
	[MANGOH_RED_BOARD_REV_DV5] = "dv5",
	[MANGOH_RED_BOARD_REV_DV6] = "dv6",
};

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
	struct i2c_client* eeprom;
	struct i2c_client* i2c_switch;
	struct i2c_client* accelerometer;
	struct i2c_client* pressure;
	struct i2c_client* battery_gauge;
	struct i2c_client* battery_charger;
	struct i2c_client* gpio_expander;
	bool mux_initialized;
	bool iot_slot_registered;
	bool led_registered;
	int accelerometer_int1_gpio_requested;
	int accelerometer_int2_gpio_requested;
} mangoh_red_driver_data = {
	.mux_initialized = false,
	.iot_slot_registered = false,
	.led_registered = false,
	.accelerometer_int1_gpio_requested = -1,
	.accelerometer_int2_gpio_requested = -1,
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
};

static struct bmi160_platform_data mangoh_red_bmi160_platform_data = {
	/* Values to be initialized during probe() */
	.int1_irq = 0,
	.int2_irq = 0,
};
static struct i2c_board_info mangoh_red_bmi160_devinfo = {
	I2C_BOARD_INFO("bmi160", 0x68),
	.platform_data = &mangoh_red_bmi160_platform_data,
};

static struct i2c_board_info mangoh_red_pressure_devinfo = {
	I2C_BOARD_INFO("bmp280", 0x76),
};

static struct ltc294x_platform_data mangoh_red_battery_gauge_platform_data = {
	.r_sense = 18,
	.prescaler_exp = 32,
	.name = "LTC2942",
};
static struct i2c_board_info mangoh_red_battery_gauge_devinfo = {
	I2C_BOARD_INFO("ltc2942", 0x64),
	.platform_data = &mangoh_red_battery_gauge_platform_data,
};

static struct i2c_board_info mangoh_red_battery_charger_devinfo = {
	I2C_BOARD_INFO("bq24190", 0x6B),
};

#ifdef ENABLE_IOT_SLOT
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
#endif /* ENABLE_IOT_SLOT */

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

/*
 * The EEPROM is marked as read-only to prevent accidental writes. The mangOH
 * Red has the write protect (WP) pin pulled high which has the effect of making
 * the upper 1/4 of the address space of the EEPROM write protected by hardware.
 */
static struct at24_platform_data mangoh_red_eeprom_data = {
	.byte_len = 4096,
	.page_size = 32,
	.flags = AT24_FLAG_ADDR16,
	.setup = eeprom_setup,
};
static struct i2c_board_info mangoh_red_eeprom_info = {
	I2C_BOARD_INFO("at24", 0x51),
	.platform_data = &mangoh_red_eeprom_data,
};

static void mangoh_red_release(struct device* dev)
{
	/* Nothing alloc'd, so nothign to free */
}

static void eeprom_setup(struct memory_accessor *mem_acc, void *context)
{
	const char *dv5_string =
		"mangOH Red DV5.1 PCB Rev 5.0 with SWI MT7697 FW v4.3.0-0 - Manufactured by Talon Communications in Q1 2018";
	const char *dv6_string =
		"mangOH Red DV6.0 PCB Rev 6.0 with SWI MT7697 FW v4.3.0-0 - Manufactured by Talon Communications in Q4 2018";
	enum mangoh_red_board_rev detected_rev = MANGOH_RED_BOARD_REV_UNKNOWN;
	const size_t eeprom_size = mangoh_red_eeprom_data.byte_len;
	u8 data[eeprom_size];
	if (mem_acc->read(mem_acc, data, 0, eeprom_size) != eeprom_size) {
		pr_err("Error reading from board identification EEPROM.\n");
		return;
	}

	if (strcmp(data, dv5_string) == 0)
		detected_rev = MANGOH_RED_BOARD_REV_DV5;
	else if (strcmp(data, dv6_string) == 0)
		detected_rev = MANGOH_RED_BOARD_REV_DV5;

	if (detected_rev != MANGOH_RED_BOARD_REV_UNKNOWN) {
		pr_info("Detected mangOH Red board revision: %s\n",
			board_rev_strings[detected_rev]);
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
		pr_warn("EEPROM contents did not match any known board signature: \"%s\"\"\n",
			eeprom_string);
	}

	if (mangoh_red_pdata.board_rev == MANGOH_RED_BOARD_REV_UNKNOWN)
		mangoh_red_pdata.board_rev = detected_rev;
	else if (mangoh_red_pdata.board_rev != detected_rev)
		pr_warn("WARNING: mismatch between supplied board revision and detected board revision\n");
}

static int mangoh_red_probe(struct platform_device* pdev)
{
	int ret = 0;
#ifdef ENABLE_IOT_SLOT
	int sdio_mux_gpio;
	int pcm_mux_gpio;
#endif /* ENABLE_IOT_SLOT */
	struct gpio_chip *gpio_expander;
	struct i2c_adapter *i2c_adapter_primary, *i2c_adapter_gpio_exp = NULL,
			   *i2c_adapter_batt_charger = NULL;

	dev_info(&pdev->dev, "%s(): probe\n", __func__);

	i2c_adapter_primary = i2c_get_adapter(PRIMARY_I2C_BUS);
	if (!i2c_adapter_primary) {
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

	dev_dbg(&pdev->dev, "mapping eeprom\n");
	if (!allow_eeprom_write)
		mangoh_red_eeprom_data.flags |= AT24_FLAG_READONLY;
	mangoh_red_driver_data.eeprom =
		i2c_new_device(i2c_adapter_primary, &mangoh_red_eeprom_info);
	if (!mangoh_red_driver_data.eeprom) {
		dev_err(&pdev->dev, "Failed to register %s\n",
			mangoh_red_eeprom_info.type);
		ret = -ENODEV;
		goto cleanup;
	}

	/*
	 * If the board revision still isn't known after reading the EEPROM,
	 * then abort.
	 */
	if (mangoh_red_pdata.board_rev == MANGOH_RED_BOARD_REV_UNKNOWN) {
		dev_err(&pdev->dev, "mangOH Red board revision is unknown\n");
		ret = -EINVAL;
		goto done;
	}

	/* Map the I2C switch */
	dev_dbg(&pdev->dev, "mapping i2c switch\n");
	mangoh_red_driver_data.i2c_switch =
		i2c_new_device(i2c_adapter_primary, &mangoh_red_pca954x_device_info);
	if (!mangoh_red_driver_data.i2c_switch) {
		dev_err(&pdev->dev, "Failed to register %s\n",
			mangoh_red_pca954x_device_info.type);
		ret = -ENODEV;
		goto cleanup;
	}

	/* Get other i2c adapters required for probe */
	i2c_adapter_gpio_exp =
		i2c_get_adapter(MANGOH_RED_I2C_BUS_GPIO_EXPANDER);
	i2c_adapter_batt_charger =
		i2c_get_adapter(MANGOH_RED_I2C_BUS_BATTERY_CHARGER);
	if (!i2c_adapter_gpio_exp || !i2c_adapter_batt_charger) {
		dev_err(&pdev->dev,
			"Couldn't get necessary I2C buses downstream of I2C switch\n");
		ret = -ENODEV;
		goto cleanup;
	}

	/* Map the GPIO expander */
	dev_dbg(&pdev->dev, "mapping gpio expander\n");
	if (devm_gpio_request_one(&pdev->dev, CF3_GPIO32, GPIOF_DIR_IN,
				  "gpio expander")) {
		dev_err(&pdev->dev, "Couldn't request GPIO expander interrupt GPIO");
		ret = -ENODEV;
		goto cleanup;
	}
	mangoh_red_gpio_expander_platform_data.irq_summary = gpio_to_irq(CF3_GPIO32);
	/* TODO: Find a better way to assign irq_base */
#if defined(CONFIG_ARCH_MSM9615)
	mangoh_red_gpio_expander_platform_data.irq_base = 500;
#elif defined(CONFIG_ARCH_MDM9607) /* For WP76XX */
	mangoh_red_gpio_expander_platform_data.irq_base = 160;
#endif /* CONFIG_ARCH_? */
	mangoh_red_driver_data.gpio_expander =
		i2c_new_device(i2c_adapter_gpio_exp, &mangoh_red_gpio_expander_devinfo);
	if (!mangoh_red_driver_data.gpio_expander) {
		dev_err(&pdev->dev, "Failed to register %s\n",
			mangoh_red_gpio_expander_devinfo.type);
		ret = -ENODEV;
		goto cleanup;
	}

	gpio_expander = i2c_get_clientdata(
		mangoh_red_driver_data.gpio_expander);

#ifdef ENABLE_IOT_SLOT
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
#endif /* ENABLE_IOT_SLOT */

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
	{
		unsigned gpio;
		int irq;

		gpio = gpio_expander->base + 11;
		if (gpio_request_one(gpio, GPIOF_DIR_IN, "bmi160_int1")) {
			dev_err(&pdev->dev,
				"Couldn't get GPIO expander port11 GPIO");
			ret = -ENODEV;
			goto cleanup;
		}
		mangoh_red_driver_data.accelerometer_int1_gpio_requested = gpio;
		irq = gpio_to_irq(gpio);
		if (irq < 0) {
			dev_err(&pdev->dev,
				"Couldn't lookup GPIO expander port11 IRQ");
			return -ENODEV;
		}
		mangoh_red_bmi160_platform_data.int1_irq = irq;

		gpio = gpio_expander->base + 12;
		if (gpio_request_one(gpio, GPIOF_DIR_IN, "bmi160_int2")) {
			dev_err(&pdev->dev,
				"Couldn't get GPIO expander port12 GPIO");
			ret = -ENODEV;
			goto cleanup;
		}
		mangoh_red_driver_data.accelerometer_int2_gpio_requested = gpio;
		irq = gpio_to_irq(gpio);
		if (irq < 0) {
			dev_err(&pdev->dev,
				"Couldn't lookup GPIO expander port12 IRQ");
			return -ENODEV;
		}
		mangoh_red_bmi160_platform_data.int2_irq = irq;
	}

	mangoh_red_driver_data.accelerometer =
		i2c_new_device(i2c_adapter_primary, &mangoh_red_bmi160_devinfo);
	if (!mangoh_red_driver_data.accelerometer) {
		dev_err(&pdev->dev, "Accelerometer is missing\n");
		return -ENODEV;
	}

	/* Map the I2C BMP280 pressure sensor */
	dev_dbg(&pdev->dev, "mapping bmp280 pressure sensor\n");
	mangoh_red_driver_data.pressure =
		i2c_new_device(i2c_adapter_primary, &mangoh_red_pressure_devinfo);
	if (!mangoh_red_driver_data.pressure) {
		dev_err(&pdev->dev, "Pressure sensor is missing\n");
		return -ENODEV;
	}

	/* Map the I2C BQ24296 driver: for now use the BQ24190 driver code */
	dev_dbg(&pdev->dev, "mapping bq24296 driver\n");
	mangoh_red_driver_data.battery_charger = i2c_new_device(
		i2c_adapter_batt_charger, &mangoh_red_battery_charger_devinfo);
	if (!mangoh_red_driver_data.battery_charger) {
		dev_err(&pdev->dev, "battery charger is missing\n");
		ret = -ENODEV;
		goto cleanup;
	}

	/* Map the I2C ltc2942 battery gauge */
	dev_dbg(&pdev->dev, "mapping ltc2942 battery gauge\n");
	mangoh_red_driver_data.battery_gauge = i2c_new_device(
		i2c_adapter_batt_charger,
		&mangoh_red_battery_gauge_devinfo);
	if (!mangoh_red_driver_data.battery_gauge) {
		dev_err(&pdev->dev, "battery gauge is missing\n");
		ret = -ENODEV;
		goto cleanup;
	}

	/*
	 * TODO:
	 * 3503 USB Hub: 0x08
	 *    Looks like there is a driver in the wp85 kernel source at
	 *    drivers/usb/misc/usb3503.c. I'm not really sure what benefit is
	 *    achieved through using this driver.
	 */

cleanup:
	i2c_put_adapter(i2c_adapter_primary);
	i2c_put_adapter(i2c_adapter_gpio_exp);
	i2c_put_adapter(i2c_adapter_batt_charger);
	if (ret != 0)
		mangoh_red_remove(pdev);
done:
	return ret;
}

static int mangoh_red_remove(struct platform_device* pdev)
{
	struct mangoh_red_driver_data *dd = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "Removing mangoh red platform device\n");

	i2c_unregister_device(dd->battery_gauge);

	i2c_unregister_device(dd->battery_charger);
	i2c_unregister_device(dd->pressure);
	i2c_unregister_device(dd->accelerometer);
	if (dd->accelerometer_int2_gpio_requested >= 0)
		gpio_free(dd->accelerometer_int2_gpio_requested);
	if (dd->accelerometer_int1_gpio_requested >= 0)
		gpio_free(dd->accelerometer_int1_gpio_requested);

	if (dd->led_registered)
		platform_device_unregister(&mangoh_red_led);

#ifdef ENABLE_IOT_SLOT
	if (dd->iot_slot_registered)
		platform_device_unregister(&mangoh_red_iot_slot);

	if (dd->mux_initialized)
		mangoh_red_mux_deinit();
#endif /* ENABLE_IOT_SLOT */

	i2c_unregister_device(dd->gpio_expander);
	i2c_unregister_device(dd->i2c_switch);
	i2c_unregister_device(dd->eeprom);

	return 0;
}

/* Release function is needed to avoid warning when device is deleted */
#ifdef ENABLE_IOT_SLOT
static void mangoh_red_iot_slot_release(struct device *dev) { /* do nothing */ }
#endif /* ENABLE_IOT_SLOT */
static void mangoh_red_led_release(struct device *dev) { /* do nothing */ }

#ifdef ENABLE_IOT_SLOT
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
#endif /* ENABLE_IOT_SLOT */

static int __init mangoh_red_init(void)
{
	int i;
	bool valid_revision_param = false;
	platform_driver_register(&mangoh_red_driver);
	printk(KERN_DEBUG "mangoh: registered platform driver\n");

	for (i = 0; i < MANGOH_RED_BOARD_REV_NUM_OF; i++) {
		if (strcmp(revision, board_rev_strings[i]) == 0) {
			mangoh_red_pdata.board_rev = i;
			valid_revision_param = true;
			break;
		}
	}

	if (!valid_revision_param) {
		printk(KERN_DEBUG "%s: Invalid revision parameter value (%s)\n",
		       __func__, revision);
		return -EINVAL;
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
