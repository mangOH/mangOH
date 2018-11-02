#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/version.h>
#include "expander.h"

/*
 *-----------------------------------------------------------------------------
 * Constants
 *-----------------------------------------------------------------------------
 */
#define GENERIC_LED		(4)
#define PCM_SEL			(5)
#define SDIO_SEL		(6)
#define TRI_LED_BLU		(7)
#define TRI_LED_GRN		(15)
#define TRI_LED_RED		(10)
#define BUZZER			(0)


struct expander_device {
	struct platform_device *pdev;
	atomic_t generic_led_val, pcm_sel_val, sdio_sel_val, buzzer_val,
	         tri_led_blu_val, tri_led_red_val, tri_led_grn_val;
	int gpio_expander_base;
};


#define CREATE_SYSFS_DEFN(_var, _offset)                                       \
static ssize_t _var##_show(struct device *dev, struct device_attribute *attr,  \
			   char *buf)                                          \
{                                                                              \
	struct expander_device* exp = dev_get_drvdata(dev);                    \
	return sprintf(buf, "%d\n", atomic_read(&exp->_var##_val));            \
}                                                                              \
static int _var##_store(struct device *dev, struct device_attribute *attr,     \
			const char *buf, size_t count)                         \
{                                                                              \
	struct expander_device* exp = dev_get_drvdata(dev);                    \
	u8 val;                                                                \
	int ret;							       \
									       \
	ret = kstrtou8(buf, 10, &val);					       \
	if (ret || val > 1)						       \
		return -EINVAL;						       \
									       \
	gpio_set_value_cansleep(exp->gpio_expander_base + _offset, val);       \
	atomic_set(&exp->_var##_val, val);				       \
									       \
	return count;							       \
}									       \
static DEVICE_ATTR_RW(_var)

CREATE_SYSFS_DEFN(generic_led, GENERIC_LED);
CREATE_SYSFS_DEFN(pcm_sel, PCM_SEL);
CREATE_SYSFS_DEFN(sdio_sel, SDIO_SEL);
CREATE_SYSFS_DEFN(tri_led_blu, TRI_LED_BLU);
CREATE_SYSFS_DEFN(tri_led_red, TRI_LED_RED);
CREATE_SYSFS_DEFN(tri_led_grn, TRI_LED_GRN);
CREATE_SYSFS_DEFN(buzzer, BUZZER);


static int gpio_initial_status(struct platform_device *pdev,
			       struct device_attribute *attr,
			       int function_number, int function_val,
			       atomic_t *atomic_val)
{
	int ret = 0;
	struct expander_device* exp = dev_get_drvdata(&pdev->dev);

	const int gpio_num = exp->gpio_expander_base + function_number;
	ret = devm_gpio_request_one(&pdev->dev, gpio_num,
				    (GPIOF_DIR_OUT |
				     (function_val ? GPIOF_INIT_HIGH :
				                     GPIOF_INIT_LOW)),
				    attr->attr.name);
	if (ret != 0) {
		dev_err(&pdev->dev, "Couldn't request GPIO %d\n", gpio_num);
		return ret;
	}

	atomic_set(atomic_val, function_val);

	ret = device_create_file(&pdev->dev, attr);
	if (ret != 0) {
		dev_err(&pdev->dev, "Couldn't create sysfs file for %s\n",
			attr->attr.name);
		return ret;
	}

	return ret;
}

static void gpio_final_status(struct platform_device *pdev,
			      struct device_attribute *attr,
			      int function_number, int function_val)
{

	struct expander_device* exp = dev_get_drvdata(&pdev->dev);

	device_remove_file(&pdev->dev, attr);

	gpio_set_value_cansleep(exp->gpio_expander_base + function_number,
				function_val);
}

static int expander_probe(struct platform_device *pdev)
{
	struct expander_device* dev;
	int ret = 0;
	struct expander_platform_data *pdata = dev_get_platdata(&pdev->dev);

	dev_info(&pdev->dev, "%s(): probe\n", __func__);

	if (!pdata) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "Required platform data not provided\n");
		goto  done;
	}

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		goto done;
	}

	dev->pdev = pdev;
 	dev->gpio_expander_base = pdata->gpio_expander_base;
	platform_set_drvdata(pdev, dev);

	ret = gpio_initial_status(pdev, &dev_attr_generic_led, GENERIC_LED, 0,
				  &dev->generic_led_val);
	if (ret != 0)
		goto done;

	ret = gpio_initial_status(pdev, &dev_attr_pcm_sel, PCM_SEL, 0,
				  &dev->pcm_sel_val);
	if (ret != 0)
		goto done;

	ret = gpio_initial_status(pdev, &dev_attr_sdio_sel, SDIO_SEL, 0,
				  &dev->sdio_sel_val);
	if (ret != 0)
		goto done;

	ret = gpio_initial_status(pdev, &dev_attr_tri_led_blu, TRI_LED_BLU, 0,
				  &dev->tri_led_blu_val);
	if (ret != 0)
		goto done;

	ret = gpio_initial_status(pdev, &dev_attr_tri_led_red, TRI_LED_RED, 0,
				  &dev->tri_led_red_val);
	if (ret != 0)
		goto done;

	ret = gpio_initial_status(pdev, &dev_attr_tri_led_grn, TRI_LED_GRN, 0,
				 &dev->tri_led_grn_val);
	if (ret != 0)
		goto done;

	ret = gpio_initial_status(pdev, &dev_attr_buzzer, BUZZER, 0,
				  &dev->buzzer_val);
	if (ret != 0)
		goto done;



done:
	return ret;
}

static int expander_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s(): remove\n", __func__);

	/* remove sysfs files & set final state values for gpio expander */
	gpio_final_status(pdev, &dev_attr_generic_led, GENERIC_LED, 0);
        gpio_final_status(pdev, &dev_attr_pcm_sel, PCM_SEL, 0);
	gpio_final_status(pdev, &dev_attr_sdio_sel, SDIO_SEL, 0);
	gpio_final_status(pdev, &dev_attr_tri_led_blu, TRI_LED_BLU, 0);
	gpio_final_status(pdev, &dev_attr_tri_led_red, TRI_LED_RED, 0);
	gpio_final_status(pdev, &dev_attr_tri_led_grn, TRI_LED_GRN, 0);
	gpio_final_status(pdev, &dev_attr_buzzer, BUZZER, 0);

	return 0;
}

static const struct platform_device_id yellow_expander_ids[] = {
	{"expander", (kernel_ulong_t)0},
	{},
};
MODULE_DEVICE_TABLE(platform, yellow_expander_ids);

static struct platform_driver expander_driver = {
	.probe		= expander_probe,
	.remove		= expander_remove,
	.driver		= {
		.name	= "expander",
		.owner	= THIS_MODULE,
		.bus	= &platform_bus_type,
	},
	.id_table	= yellow_expander_ids,
};

static int __init expander_init(void)
{
	platform_driver_register(&expander_driver);
	return 0;
}

static void __exit expander_exit(void)
{
	platform_driver_unregister(&expander_driver);
}

module_init(expander_init);
module_exit(expander_exit);

MODULE_ALIAS("platform:expander");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sierra Wireless");
MODULE_DESCRIPTION("Expander driver");
MODULE_VERSION("0.1");
