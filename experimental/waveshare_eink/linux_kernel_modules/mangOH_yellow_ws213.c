/* This files provides configuration data  & SPI setup for the
 * Waveshare 213 for the mangOH Yellow.
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio/driver.h>

#if defined(CONFIG_ARCH_MDM9607) /* For WP76XX, WP77XX */
#include <../arch/arm/mach-msm/include/mach/swimcu.h>
#endif

#include "fb_waveshare_eink.h"


/* Legato 19.07 stop/start causes SPI bus to be renumerated
 * off of the SPI port of the CP2130 - could be an issue
 * on the USB that feeds the CP2130 or even the USB Hub.
 * The Expansion connector SPI is of the 2nd SPI bus
 * not the primary and starts enumeration at 32766.
 */
#define SPI_MAX_SEARCH		32
#define SPI_START_BUS		32766
#define SPI_MIN_BUS		(32766 - SPI_MAX_SEARCH)
#define SPI_BUS_CS1		1
/*
 * The minimum clock cycle duration supported by the waveshare 2.13 Inch display
 * is 250ns which translates to 4MHz bus speed
 */
#define SPI_BUS_SPEED	4000000

const char eink_device_name[] = "waveshare_213";
static struct spi_device *eink_device;

/* On the Yellow the busy gpio is off the IO expander unlike the Red */
#if defined(CONFIG_ARCH_MDM9607)
#define CF3_GPIO24		(11)
#define GPIO_RESET_OFFSET	1
#define GPIO_BUSY_OFFSET	5
#define GPIO_DC_OFFSET		6
#endif

static struct waveshare_eink_platform_data ws213_pdata = {
	.rst_gpio	= -1,
	.dc_gpio	= -1,
	.busy_gpio	= -1,
};

static int gpiochip_match_name(struct gpio_chip *chip, void *data)
{
	const char *name = data;

	return !strcmp(chip->label, name);
}

static int __init add_ws213fb_device_to_bus(void)
{
	struct spi_master *spi_master = NULL;
	struct gpio_chip *expander_gpiop;
	int busnum;
	int status = 0;

        /* Bus not assigned: find SPI master with lowest bus number */
        for (busnum = SPI_START_BUS;
		busnum > SPI_MIN_BUS && NULL == spi_master; busnum--) {
                spi_master = spi_busnum_to_master(busnum);
        }

        if (!spi_master) {
		printk(KERN_ALERT
		"mangOH_yellow_ws213: spi_busnum_to_master(%d) search returned NULL\n",
		       busnum);
		return -1;
	}

	eink_device = spi_alloc_device(spi_master);
	if (!eink_device) {
		put_device(&spi_master->dev);
		printk(KERN_ALERT "spi_alloc_device() failed\n");
		status = -1;
		goto put_master;
	}

	eink_device->chip_select = SPI_BUS_CS1;
	eink_device->max_speed_hz = SPI_BUS_SPEED;
	eink_device->mode = SPI_MODE_3;
	eink_device->bits_per_word = 8;
	eink_device->irq = -1;

	/* If we find the expander board gpio expander we are ok */
	expander_gpiop = gpiochip_find((void *) "sx1508q", gpiochip_match_name);
	if (expander_gpiop == NULL) {
		printk(KERN_ALERT
		"mangOH_yellow_ws213: Finding sx1508q gpiochip for the DC/RESET gpios failed\n");
		status = -1;
		goto put_master;
	}
	ws213_pdata.rst_gpio = expander_gpiop->base + GPIO_RESET_OFFSET;
	ws213_pdata.dc_gpio = expander_gpiop->base + GPIO_DC_OFFSET;
	ws213_pdata.busy_gpio = expander_gpiop->base + GPIO_BUSY_OFFSET;

	eink_device->dev.platform_data = &ws213_pdata;
	eink_device->controller_state = NULL;
	eink_device->controller_data = NULL;
	strlcpy(eink_device->modalias, eink_device_name, SPI_NAME_SIZE);

	status = spi_add_device(eink_device);
	if (status < 0) {
		spi_dev_put(eink_device);
		printk(KERN_ALERT
		"mangOH_yellow_ws213: spi_add_device() failed: %d\n", status);
	}

put_master:
	put_device(&spi_master->dev);

	return status;
}
module_init(add_ws213fb_device_to_bus);

static void __exit exit_ws213fb_device(void)
{
	spi_unregister_device(eink_device);
}
module_exit(exit_ws213fb_device);

MODULE_AUTHOR("Sierra Wireless");
MODULE_DESCRIPTION("Bind device driver to SPI ws213 e-ink display");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
