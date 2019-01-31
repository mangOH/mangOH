#include <linux/module.h>
#include <linux/spi/spi.h>
#include "fb_waveshare_eink.h"

#define SPI_BUS		32766
#define SPI_BUS_CS1	1
#define SPI_BUS_SPEED	12000000

const char eink_device_name[] = "waveshare_213";
static struct spi_device *eink_device;

#if defined(CONFIG_ARCH_MSM9615)
#define CF3_GPIO22	(49)
#define CF3_GPIO23	(54)
#define CF3_GPIO24	(61)
#elif defined(CONFIG_ARCH_MDM9607)
#define CF3_GPIO22	 (9)
#define CF3_GPIO23	(10)
#define CF3_GPIO24	(11)
#endif

static struct waveshare_eink_platform_data ws213_pdata = {
	.rst_gpio	= CF3_GPIO23,
	.dc_gpio	= CF3_GPIO22,
	.busy_gpio	= CF3_GPIO24,
};

static int __init add_ws213fb_device_to_bus(void)
{
	struct spi_master *spi_master;
	int status = 0;

	spi_master = spi_busnum_to_master(SPI_BUS);
	if (!spi_master) {
		printk(KERN_ALERT "spi_busnum_to_master(%d) returned NULL\n",
		       SPI_BUS);
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
	eink_device->dev.platform_data = &ws213_pdata;
	eink_device->controller_state = NULL;
	eink_device->controller_data = NULL;
	strlcpy(eink_device->modalias, eink_device_name, SPI_NAME_SIZE);

	status = spi_add_device(eink_device);
	if (status < 0) {
		spi_dev_put(eink_device);
		printk(KERN_ALERT "spi_add_device() failed: %d\n", status);
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

MODULE_AUTHOR("Neil Greatorex");
MODULE_DESCRIPTION("Bind device driver to SPI ws213 e-ink display");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
