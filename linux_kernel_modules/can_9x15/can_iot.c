#include <linux/module.h> 
#include <linux/kernel.h> 
#include <linux/platform_device.h> 
#include <linux/irq.h>
#include <linux/can/platform/mcp251x.h>
#include <linux/gpio.h>

/* We assume that we are in the linux_kernel_modules dir on the mangOH src tree */
#include "../mangoh/mangoh_common.h"

/*  For later if you need more early startup code for the MCP251x chip driver
static struct platform_driver can_iot_drv = { 
    .probe          = can_iot_probe, 
    .remove         = can_iot_remove, 
    .driver = { 
            .name  = KBUILD_MODNAME, 
            .owner = THIS_MODULE, 
    }, 
}; 
static int can_iot_probe (struct platform_device *pdev){ 
    return 0; 
} 
 
static void can_iot_remove(struct platform_device *pdev){ 
} 
*/

static struct mcp251x_platform_data mcp2515_pdata = {
        .oscillator_frequency   = 16*1000*1000,
};

static struct spi_board_info mcp2515_board_info= {
                .modalias       = "mcp2515",
                .platform_data  = &mcp2515_pdata,
                .max_speed_hz   = 2*1000*1000,
                .mode           = SPI_MODE_0,
                .bus_num        = 0,
                .chip_select    = 0,
};
 
static struct spi_master *spi_master;
static struct spi_device *spi_device;
 
static int __init can_iot_init(void) 
{ 
 

	/* MCP251x needs us to allocate the GPIO irq it will request */
        if ((gpio_request(CF3_GPIO42, "can_irq") == 0) &&
            (gpio_direction_input(CF3_GPIO42) == 0)) {
                gpio_export(CF3_GPIO42, 0);
                irq_set_irq_type(gpio_to_irq(CF3_GPIO42), IRQ_TYPE_EDGE_FALLING);
        } else
                pr_err("can_iot_init: Could not allocate the gpio irq for MCP251x CAN bus interrupt\n");

        mcp2515_board_info.irq = gpio_to_irq(CF3_GPIO42);

        /* Hook into the SPI bus */
        spi_master = spi_busnum_to_master(PRIMARY_SPI_BUS);
        if (!spi_master)
                pr_err("can_iot_init: No SPI Master on Primary SPI Bus\n");

        spi_device = spi_new_device(spi_master, &mcp2515_board_info);
        	pr_info("can_iot_init: mcp2515 (gpio:%d  irq:%d).\n",
                	CF3_GPIO42, mcp2515_board_info.irq);

    	/* platform_driver_register(&can_iot_drv);  */
    	return 0; 
} 
                 
static void __exit can_iot_remove (void) 
{ 
	gpio_unexport(CF3_GPIO42);
	gpio_free(CF3_GPIO42);
	spi_unregister_device(spi_device);
 
    	/* platform_driver_unregister(&can_iot_drv);  */
} 
 
module_init(can_iot_init); 
module_exit(can_iot_remove); 
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sierra Wireless");
MODULE_DESCRIPTION("Platform data for MCP251x chip driver");
