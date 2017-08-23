#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <linux/delay.h>

#include "mangoh.h"
#include "board_green_dv4.h"
#include "board_red_dv2.h"
#include "board_red_dv3.h"

/*
 *-----------------------------------------------------------------------------
 * Type definitions
 *-----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 * Function declarations
 *-----------------------------------------------------------------------------
 */
static int mangoh_probe(struct platform_device* pdev);
static int mangoh_remove(struct platform_device* pdev);


/*
 *-----------------------------------------------------------------------------
 * Variables
 *-----------------------------------------------------------------------------
 */

/* Module parameter definitions */
static char *board = "green dv4";
module_param(board, charp, S_IRUGO);
MODULE_PARM_DESC(revision, "mangOH hardware board model and revision");

static struct platform_device* mangoh_device;

static const struct platform_device_id mangoh_ids[] = {
	{.name="mangoh green dv4", .driver_data=(kernel_ulong_t)&green_dv4_descriptor},
	{.name="mangoh red dv2", .driver_data=(kernel_ulong_t)&red_dv2_descriptor},
	{.name="mangoh red dv3", .driver_data=(kernel_ulong_t)&red_dv3_descriptor},
	{},
};
MODULE_DEVICE_TABLE(platform, mangoh_ids);

static struct platform_driver mangoh_driver = {
	.probe = mangoh_probe,
	.remove = mangoh_remove,
	.driver  = {
		.name = "mangoh",
		.owner = THIS_MODULE,
		.bus = &platform_bus_type,
	},
	.id_table = mangoh_ids,
};


/*
 *-----------------------------------------------------------------------------
 * Function definitions
 *-----------------------------------------------------------------------------
 */
static int __init mangoh_init(void)
{
	if (strcmp(board, "green dv4") == 0) {
		int rc = green_dv4_create_device(&mangoh_device);
		if (rc != 0) {
			pr_err(
				"%s: Couldn't create device for '%s'.\n",
				__func__,
				board);
			return rc;
		}
	} else if (strcmp(board, "red dv2") == 0) {
		int rc = red_dv2_create_device(&mangoh_device);
		if (rc != 0) {
			pr_err(
				"%s: Couldn't create device for '%s'.\n",
				__func__,
				board);
			return rc;
		}
	} else if (strcmp(board, "red dv3") == 0) {
		int rc = red_dv3_create_device(&mangoh_device);
		if (rc != 0) {
			pr_err(
				"%s: Couldn't create device for '%s'.\n",
				__func__,
				board);
			return rc;
		}
	} else {
		pr_err(
			"%s: unsupported board '%s'.\n",
			__func__,
			board);
		return -ENODEV;
	}
	printk(KERN_INFO "mangoh: created device for board %s\n", board);

	platform_driver_register(&mangoh_driver);
	printk(KERN_DEBUG "mangoh: registered platform driver\n");
	if (platform_device_register(mangoh_device)) {
		platform_driver_unregister(&mangoh_driver);
		return -ENODEV;
	}
	printk(KERN_DEBUG "mangoh: registered platform device\n");

	return 0;
}

static void __exit mangoh_exit(void)
{
	if (mangoh_device != NULL)
		platform_device_unregister(mangoh_device);

	platform_driver_unregister(&mangoh_driver);
}

static int mangoh_probe(struct platform_device* pdev)
{
	struct mangoh_descriptor* desc =
		(struct mangoh_descriptor*)
		platform_get_device_id(pdev)->driver_data;
	platform_set_drvdata(pdev, desc);

	/* Work around USB issues at boot time by delaying device installation */
	dev_info(&pdev->dev, "Probing mangOH platform device\n");
	msleep(5000);
	return desc->map(pdev);
}

static int mangoh_remove(struct platform_device* pdev)
{
	struct mangoh_descriptor* desc = platform_get_drvdata(pdev);
	const int res = desc->unmap(pdev);
	dev_info(&pdev->dev, res ? "Removal failed.\n" : "Removed.\n");
	return res;
}

module_init(mangoh_init);
module_exit(mangoh_exit);

MODULE_ALIAS(PLATFORM_MODULE_PREFIX "mangoh");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sierra Wireless");
MODULE_DESCRIPTION("Add devices on mangOH green hardware board");
MODULE_VERSION("1.0");
