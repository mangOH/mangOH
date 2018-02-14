#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/version.h>
#include "led.h"

struct led_device {
	struct platform_device *pdev;
	atomic_t val;
	int gpio;
};

static ssize_t led_show(struct device *dev, struct device_attribute *attr,
                        char *buf)
{
	struct led_device* led = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", atomic_read(&led->val));
}

static int led_store(struct device *dev, struct device_attribute *attr,
                     const char *buf, size_t count)
{
	struct led_device* led = dev_get_drvdata(dev);
	unsigned int val;
	int ret;

	ret = kstrtoint(buf, 10, &val);
	if (ret || val > 1)
		return -EINVAL;

	gpio_set_value_cansleep(led->gpio, val);
	atomic_set(&led->val, val);
	return count;
}

static DEVICE_ATTR_RW(led);

static int led_probe(struct platform_device *pdev)
{
	struct led_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct led_device* dev;
	int ret = 0;

	dev_info(&pdev->dev, "%s(): probe\n", __func__);

	dev = kzalloc(sizeof(struct led_device), GFP_KERNEL);
	if (!dev)
		goto err_out;

	dev->pdev = pdev;
	atomic_set(&dev->val, 0);

	pdata = dev_get_platdata(&pdev->dev);
	dev->gpio = pdata->gpio;

	ret = gpio_request(dev->gpio, dev_name(&pdev->dev));
	if (ret) {
		dev_err(&pdev->dev, "failed to request LED gpio\n");
		goto err_out;
	}

	ret = gpio_direction_output(dev->gpio, atomic_read(&dev->val));
	if (ret) {
		dev_err(&pdev->dev, "failed to set LED gpio as output\n");
		goto free_gpio;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_led);
	if (ret) {
		dev_err(&pdev->dev, "failed to create led sysfs entry\n");
		goto free_gpio;
	}

	platform_set_drvdata(pdev, dev);
	return 0;

free_gpio:
	gpio_direction_input(dev->gpio);
	gpio_free(dev->gpio);

err_out:
	if (dev)
		kfree(dev);

	return ret;
}

static int led_remove(struct platform_device *pdev)
{
	struct led_device *dev = platform_get_drvdata(pdev);
	int ret;

	dev_info(&pdev->dev, "%s(): remove\n", __func__);

	/* remove sysfs files */
	device_remove_file(&pdev->dev, &dev_attr_led);
	ret = gpio_direction_input(dev->gpio);
	if (ret)
		dev_err(&pdev->dev, "failed to set LED gpio as input\n");

	gpio_free(dev->gpio);
	kfree(dev);
	return 0;
}

static const struct platform_device_id led_ids[] = {
	{"led", (kernel_ulong_t)0},
	{},
};
MODULE_DEVICE_TABLE(platform, led_ids);

static struct platform_driver led_driver = {
	.probe		= led_probe,
	.remove		= led_remove,
	.driver		= {
		.name	= "led",
		.owner	= THIS_MODULE,
		.bus	= &platform_bus_type,
	},
	.id_table	= led_ids,
};

static int __init led_init(void)
{
	platform_driver_register(&led_driver);
	return 0;
}

static void __exit led_exit(void)
{
	platform_driver_unregister(&led_driver);
}

module_init(led_init);
module_exit(led_exit);

MODULE_ALIAS("platform:led");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sierra Wireless");
MODULE_DESCRIPTION("LED driver");
MODULE_VERSION("0.1");
