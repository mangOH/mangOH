#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/version.h>
#include "expander_expbrd.h"

/*
 *-----------------------------------------------------------------------------
 * Constants
 *-----------------------------------------------------------------------------
 */
/*
#define RESET_EINK		(1)
#define BUSY_EINK		(5)
#define DC_EINK			(6)
*/


struct expander_expbrd_device {
	struct platform_device *pdev;
	/* atomic_t reset_eink_val, busy_eink_val, dc_eink_val */;
	int gpio_expander_expbrd_base;
};


#define CREATE_SYSFS_DEFN(_var, _offset, _active_low)                          \
static ssize_t _var##_show(struct device *dev,  struct device_attribute *attr, \
                           char *buf)                                          \
{                                                                              \
        struct expander_expbrd_device* exp = dev_get_drvdata(dev);                    \
        return sprintf(buf, "%d\n", atomic_read(&exp->_var##_val));            \
}                                                                              \
static int _var##_store(struct device *dev, struct device_attribute *attr,     \
                        const char *buf, size_t count)                         \
{                                                                              \
        struct expander_expbrd_device* exp = dev_get_drvdata(dev);                    \
        u8 val;                                                                \
        int ret;                                                               \
                                                                               \
        ret = kstrtou8(buf, 10, &val);                                         \
        if (ret || val > 1)                                                    \
                return -EINVAL;                                                \
        atomic_set(&exp->_var##_val, val);                                     \
        gpio_set_value_cansleep(exp->gpio_expander_expbrd_base + _offset,             \
                                _active_low ? !val : val);                     \
                                                                               \
        return count;                                                          \
}                                                                              \
static DEVICE_ATTR_RW(_var)

/* All the Eink gpio's are hooked in the driver from kernel space
CREATE_SYSFS_DEFN(reset_eink, RESET_EINK, false);
CREATE_SYSFS_DEFN(busy_eink, BUSY_EINK, false);
CREATE_SYSFS_DEFN(dc_eink, DC_EINK, false);

static int gpio_initial_status(struct platform_device *pdev,
			       struct device_attribute *attr,
			       int expander_expbrd_gpio_offset, int gpio_output_level,
			       atomic_t *atomic_val, bool is_open_drain)
{
	struct expander_expbrd_device *exp = dev_get_drvdata(&pdev->dev);
 	const int gpio_num = exp->gpio_expander_expbrd_base + expander_expbrd_gpio_offset;
	int ret = devm_gpio_request_one(
		&pdev->dev, gpio_num,
		(GPIOF_DIR_OUT |
		 (gpio_output_level ? GPIOF_INIT_HIGH : GPIOF_INIT_LOW) |
		 (is_open_drain ? GPIOF_OPEN_DRAIN : 0)),
		attr->attr.name);
	if (ret != 0) {
		dev_err(&pdev->dev, "Couldn't request Expander Board GPIO %d\n", gpio_num);
		return ret;
	}

	atomic_set(atomic_val, gpio_output_level);

	ret = device_create_file(&pdev->dev, attr);
	if (ret != 0) {
		dev_err(&pdev->dev, "Couldn't create sysfs file for Expander Board %s\n",
			attr->attr.name);
		return ret;
	}

	return ret;
}

static void gpio_final_status(struct platform_device *pdev,
			      struct device_attribute *attr,
			      int expander_expbrd_gpio_offset, int gpio_output_level)
{
	struct expander_expbrd_device *exp = dev_get_drvdata(&pdev->dev);
	const int gpio_num = exp->gpio_expander_expbrd_base + expander_expbrd_gpio_offset;
	device_remove_file(&pdev->dev, attr);
	gpio_set_value_cansleep(gpio_num, gpio_output_level);
}
*/

static int expander_expbrd_probe(struct platform_device *pdev)
{
	struct expander_expbrd_device* dev;
	int ret = 0;
	struct expander_expbrd_platform_data *pdata = dev_get_platdata(&pdev->dev);

	dev_dbg(&pdev->dev, "%s(): probe\n", __func__);

	if (!pdata) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "Required platform data not provided\n");
		goto done;
	}

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		goto done;
	}

	dev->pdev = pdev;
	dev->gpio_expander_expbrd_base = pdata->gpio_expander_expbrd_base;
	platform_set_drvdata(pdev, dev);

/*
	ret = gpio_initial_status(pdev, &dev_attr_reset_eink, RESET_EINK, 0,
				  &dev->reset_eink_val, false);
	if (ret)
		goto done;
	ret = gpio_initial_status(pdev, &dev_attr_busy_eink, BUSY_EINK, 0,
				  &dev->busy_eink_val, false);
	if (ret)
		goto done;
	ret = gpio_initial_status(pdev, &dev_attr_dc_eink, DC_EINK, 0,
				  &dev->dc_eink_val, false);
	if (ret)
		goto done;
*/

done:
	return ret;
}

static int expander_expbrd_remove(struct platform_device *pdev)
{
	/* remove sysfs files & set final state values for Expander board gpio expander */
/*
	gpio_final_status(pdev, &dev_attr_reset_eink, RESET_EINK, 0);
	gpio_final_status(pdev, &dev_attr_busy_eink, BUSY_EINK, 0);
	gpio_final_status(pdev, &dev_attr_dc_eink, DC_EINK, 0);
*/

	return 0;
}

static const struct platform_device_id yellow_expander_expbrd_ids[] = {
	{"expander_expbrd", (kernel_ulong_t)0},
	{},
};
MODULE_DEVICE_TABLE(platform, yellow_expander_expbrd_ids);

static struct platform_driver expander_expbrd_driver = {
	.probe		= expander_expbrd_probe,
	.remove		= expander_expbrd_remove,
	.driver		= {
		.name	= "expander_expbrd",
		.owner	= THIS_MODULE,
		.bus	= &platform_bus_type,
	},
	.id_table	= yellow_expander_expbrd_ids,
};

static int __init expander_expbrd_init(void)
{
	platform_driver_register(&expander_expbrd_driver);
	return 0;
}

static void __exit expander_expbrd_exit(void)
{
	platform_driver_unregister(&expander_expbrd_driver);
}

module_init(expander_expbrd_init);
module_exit(expander_expbrd_exit);

MODULE_ALIAS("platform:expander_expbrd");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sierra Wireless");
MODULE_DESCRIPTION("EXPANDER driver");
MODULE_VERSION("0.1");
