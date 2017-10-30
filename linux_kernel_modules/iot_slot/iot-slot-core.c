#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>

#include "iot-slot-eeprom.h"
#include "iot-slot.h"

#ifndef CONFIG_IOT_SLOT_NUM_SUPPORTED
#define CONFIG_IOT_SLOT_NUM_SUPPORTED 3
#endif

#ifndef CONFIG_IOT_SLOT_MAX_INTERFACES_PER_CARD
#define CONFIG_IOT_SLOT_MAX_INTERFACES_PER_CARD 10
#endif /* CONFIG_IOT_SLOT_MAX_INTERFACES_PER_CARD */

enum iot_slot_interface {
	IOT_SLOT_INTERFACE_GPIO,
	IOT_SLOT_INTERFACE_I2C,
	IOT_SLOT_INTERFACE_SPI,
	IOT_SLOT_INTERFACE_USB,
	IOT_SLOT_INTERFACE_SDIO,
	IOT_SLOT_INTERFACE_ADC,
	IOT_SLOT_INTERFACE_PCM,
	IOT_SLOT_INTERFACE_CLK,
	IOT_SLOT_INTERFACE_UART,
	IOT_SLOT_INTERFACE_PLATFORM,
};

struct iot_slot_interface_data {
	enum iot_slot_interface type;
	union {
		struct {
			struct i2c_client* client;
		} i2c;
		struct {
			struct spi_device* device;
		} spi;
	} data;
};

struct iot_slot {
	struct platform_device *pdev;

	bool card_present;

	struct i2c_adapter *i2c_adapter;

	unsigned int num_interfaces;
	struct iot_slot_interface_data interfaces[
		CONFIG_IOT_SLOT_MAX_INTERFACES_PER_CARD];
};


static int iot_slot_add_gpio(struct iot_slot *slot,
			     struct list_head *gpio_item);
static int iot_slot_add_i2c(struct iot_slot *slot, struct list_head *i2c_item);
static int iot_slot_add_spi(struct iot_slot *slot, struct list_head *spi_item);
static int iot_slot_add_sdio(struct iot_slot *slot,
			     struct list_head *sdio_item);
static int iot_slot_add_adc(struct iot_slot *slot, struct list_head *adc_item);
static int iot_slot_add_pcm(struct iot_slot *slot, struct list_head *pcm_item);
static int iot_slot_add_uart(struct iot_slot *slot,
			     struct list_head *uart_item);
static void iot_slot_release_resources(struct iot_slot *slot);


static struct iot_slot *slots[CONFIG_IOT_SLOT_NUM_SUPPORTED];
static struct mutex management_mutex;


static int iot_slot_enumerate(struct iot_slot *slot)
{
	int ret = 0;
	struct list_head *item;
	struct device* device;
	struct iot_slot_platform_data *pdata;
	int slot_index;
	struct i2c_client *eeprom;

	device = &slot->pdev->dev;
	pdata = dev_get_platdata(device);
	slot_index = slot->pdev->id;

	slot->card_present =
		gpio_get_value_cansleep(pdata->card_detect_gpio) == 0;
	if (slot->card_present) {
		dev_info(device, "Detected IoT card on slot %d\n", slot_index);
	} else {
		dev_dbg(device, "No IoT card detected on slot %d\n", slot_index);
		goto done;
	}

	/* Set card detect to output high to activate the eeprom */
	ret = gpio_direction_output(pdata->card_detect_gpio, 1);
	if (ret != 0) {
		dev_err(device, "Couldn't set card detect to output\n");
	}

	eeprom = eeprom_load(slot->i2c_adapter);
	if (eeprom == NULL)
	{
		dev_warn(device,
			 "Card detected on IoT slot %d, but a valid eeprom was not detected\n",
			 slot_index);
		goto restore_card_detect;
	}

	/* Take the IoT card out of reset */
	gpio_set_value_cansleep(pdata->reset_gpio, 1);

	/*
	 * Give the IoT card 10ms to get ready before we start accessing it.
	 */
	msleep(10);

	list_for_each(item, eeprom_if_list(eeprom)) {
		enum EepromInterface interface_type;
		if (slot->num_interfaces >=
		    CONFIG_IOT_SLOT_MAX_INTERFACES_PER_CARD) {
			dev_info(
				device,
				"Card contains more than the maximum supported number of interfaces (%d)\n",
				CONFIG_IOT_SLOT_MAX_INTERFACES_PER_CARD);
			ret = -ERANGE;
			goto enumeration_fail;
		}
		interface_type = eeprom_if_type(item);
		switch (interface_type) {
		case EEPROM_IF_GPIO:
			dev_info(device,
				 "Found GPIO interface specification\n");
			ret = iot_slot_add_gpio(slot, item);
			if (ret != 0) {
				goto enumeration_fail;
			}
			break;
		case EEPROM_IF_I2C:
			dev_info(device,
				 "Found I2C interface specification\n");
			ret = iot_slot_add_i2c(slot, item);
			if (ret != 0) {
				goto enumeration_fail;
			}
			break;
		case EEPROM_IF_SPI:
			dev_info(device,
				 "Found SPI interface specification\n");
			ret = iot_slot_add_spi(slot, item);
			if (ret != 0) {
				goto enumeration_fail;
			}
			break;
		case EEPROM_IF_USB:
			dev_info(device, "Found USB interface specification\n");
			slot->interfaces[slot->num_interfaces].type =
				IOT_SLOT_INTERFACE_CLK;
			slot->num_interfaces++;
			break;
		case EEPROM_IF_SDIO:
			dev_info(device,
				 "Found SDIO interface specification\n");
			ret = iot_slot_add_sdio(slot, item);
			if (ret != 0) {
				goto enumeration_fail;
			}
			break;
		case EEPROM_IF_ADC:
			dev_info(device, "Found ADC interface specification\n");
			ret = iot_slot_add_adc(slot, item);
			if (ret != 0) {
				goto enumeration_fail;
			}
			break;
		case EEPROM_IF_PCM:
			dev_info(device, "Found PCM interface specification\n");
			ret = iot_slot_add_pcm(slot, item);
			if (ret != 0) {
				goto enumeration_fail;
			}
			break;
		case EEPROM_IF_CLK:
			dev_info(device, "Found PPS interface specification\n");
			slot->interfaces[slot->num_interfaces].type =
				IOT_SLOT_INTERFACE_CLK;
			slot->num_interfaces++;
			break;
		case EEPROM_IF_UART:
			dev_info(device,
				 "Found UART interface specification\n");
			ret = iot_slot_add_uart(slot, item);
			if (ret != 0) {
				goto enumeration_fail;
			}
			break;
		case EEPROM_IF_PLAT:
			/* TODO: platorm type isn't properly supported yet */
			dev_info(device,
				 "Found platofrm interface specification\n");
			slot->interfaces[slot->num_interfaces].type =
				IOT_SLOT_INTERFACE_PLATFORM;
			slot->num_interfaces++;
			break;
		default:
			break;
		}
	}
	goto unload_eeprom;

enumeration_fail:
	iot_slot_release_resources(slot);
unload_eeprom:
	/* Free EEPROM and restore GPIO direction */
	eeprom_unload(eeprom);
restore_card_detect:
	gpio_direction_input(pdata->card_detect_gpio);
done:
	return ret;
}

static int iot_slot_add_gpio(struct iot_slot *slot,
			     struct list_head *gpio_item)
{
	struct platform_device *pdev = slot->pdev;
	struct device *device = &pdev->dev;
	int i;
	struct iot_slot_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int slot_index = pdev->id;
	int ret = 0;
	char gpio_label[32];

	for (i = 0; i < 4; i++) {
		uint8_t cfg = eeprom_if_gpio_cfg(gpio_item, i);
		ret = snprintf(gpio_label, ARRAY_SIZE(gpio_label),
			       "IoT slot %d gpio %d", slot_index, i);
		BUG_ON(ret >= ARRAY_SIZE(gpio_label));
		ret = devm_gpio_request_one(device,
					    pdata->gpio[i],
					    GPIOF_IN, gpio_label);
		if (ret != 0) {
			int j;
			dev_err(device,
				"Couldn't acquire gpio %d on IoT slot %d\n", i,
				slot_index);
			for (j = 0; j < i; j++) {
				devm_gpio_free(device, pdata->gpio[j]);
				return -EACCES;
			}
		}

		switch (cfg)
		{
		case EEPROM_GPIO_CFG_OUTPUT_LOW:
			gpio_direction_output(pdata->gpio[i], 0);
			break;
		case EEPROM_GPIO_CFG_OUTPUT_HIGH:
			gpio_direction_output(pdata->gpio[i], 1);
			break;
		case EEPROM_GPIO_CFG_INPUT_PULL_UP:
			gpio_direction_input(pdata->gpio[i]);
			gpio_pull_up(pdata->gpio[i]);
			break;
		case EEPROM_GPIO_CFG_INPUT_PULL_DOWN:
			gpio_direction_input(pdata->gpio[i]);
			gpio_pull_down(pdata->gpio[i]);
			break;
		case EEPROM_GPIO_CFG_INPUT_FLOATING:
			gpio_direction_input(pdata->gpio[i]);
			break;
		default:
			/* reserved, ignore */
			break;
		}
	}

	slot->interfaces[slot->num_interfaces].type = IOT_SLOT_INTERFACE_GPIO;
	slot->num_interfaces++;

	return 0;
}

static int iot_slot_add_i2c(struct iot_slot *slot, struct list_head *i2c_item)
{
	struct platform_device *pdev = slot->pdev;
	struct iot_slot_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct i2c_board_info board = {};
	int irq_gpio;

	/*
	 * NOTE: There's no need to request the i2c_adapter because that's
	 * already handled earlier when the eeprom is read.
	 */

	strncpy(board.type, eeprom_if_i2c_modalias(i2c_item),
		sizeof(board.type));
	irq_gpio = eeprom_if_i2c_irq_gpio(i2c_item);
	if (irq_gpio != IRQ_GPIO_UNUSED)
	{
		board.irq = gpio_to_irq(pdata->gpio[irq_gpio]);
	}
	board.addr = eeprom_if_i2c_address(i2c_item);
	slot->interfaces[slot->num_interfaces].type = IOT_SLOT_INTERFACE_I2C;
	slot->interfaces[slot->num_interfaces].data.i2c.client =
		i2c_new_device(slot->i2c_adapter, &board);
	if (slot->interfaces[slot->num_interfaces].data.i2c.client == NULL) {
		return -EINVAL;
	}
	slot->num_interfaces++;

	return 0;
}

static int iot_slot_add_spi(struct iot_slot *slot, struct list_head *spi_item)
{
	int ret = 0;

	struct platform_device *pdev = slot->pdev;
	struct device *device = &pdev->dev;
	struct iot_slot_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct spi_master *spi_master;
	int chip_select;
	int irq_gpio;
	struct spi_board_info board = {
		.max_speed_hz = 2*1000*1000,
		.mode = SPI_MODE_0,
		.platform_data = NULL,
	};
	struct spi_device *spi_device;

	ret = pdata->request_spi(&spi_master, &chip_select);
	if (ret != 0) {
		dev_err(device, "Couldn't get SPI master\n");
		return -ENODEV;
	}

	board.chip_select = chip_select;

	/* Assign IRQ number */
	irq_gpio = eeprom_if_spi_irq_gpio(spi_item);
	if (irq_gpio != IRQ_GPIO_UNUSED)
	{
		board.irq = gpio_to_irq(pdata->gpio[irq_gpio]);
	}
	strncpy(board.modalias, eeprom_if_spi_modalias(spi_item),
		sizeof(board.modalias));

	spi_device = spi_new_device(spi_master, &board);
	if (spi_device == NULL) {
		dev_err(device, "Couldn't add new SPI device\n");
		pdata->release_spi();
		return -ENODEV;
	}

	slot->interfaces[slot->num_interfaces].type = IOT_SLOT_INTERFACE_SPI;
	slot->interfaces[slot->num_interfaces].data.spi.device = spi_device;
	slot->num_interfaces++;

	return 0;
}


static int iot_slot_add_sdio(struct iot_slot *slot, struct list_head *sdio_item)
{
	int ret = 0;
	struct platform_device *pdev = slot->pdev;
	struct device *device = &pdev->dev;
	struct iot_slot_platform_data *pdata = dev_get_platdata(&pdev->dev);
	if (pdata->request_sdio) {
		ret = pdata->request_sdio();
		if (ret != 0)
		{
			dev_err(device, "Couldn't request the SDIO interface\n");
			ret = -ENODEV;
			goto done;
		}
	}
	slot->interfaces[slot->num_interfaces].type = IOT_SLOT_INTERFACE_SDIO;
	slot->num_interfaces++;
done:
	return ret;
}

static int iot_slot_add_adc(struct iot_slot *slot, struct list_head *adc_item)
{
	int ret = 0;
	struct platform_device *pdev = slot->pdev;
	struct device *device = &pdev->dev;
	struct iot_slot_platform_data *pdata = dev_get_platdata(&pdev->dev);
	if (pdata->request_adc) {
		ret = pdata->request_adc();
		if (ret != 0)
		{
			dev_err(device, "Couldn't request the ADC\n");
			ret = -ENODEV;
			goto done;
		}
	}
	slot->interfaces[slot->num_interfaces].type = IOT_SLOT_INTERFACE_ADC;
	slot->num_interfaces++;
done:
	return ret;
}

static int iot_slot_add_pcm(struct iot_slot *slot, struct list_head *pcm_item)
{
	int ret = 0;
	struct platform_device *pdev = slot->pdev;
	struct device *device = &pdev->dev;
	struct iot_slot_platform_data *pdata = dev_get_platdata(&pdev->dev);
	if (pdata->request_pcm) {
		ret = pdata->request_pcm();
		if (ret != 0)
		{
			dev_err(device, "Couldn't request the PCM interface\n");
			ret = -ENODEV;
			goto done;
		}
	}
	slot->interfaces[slot->num_interfaces].type = IOT_SLOT_INTERFACE_PCM;
	slot->num_interfaces++;
done:
	return ret;
}

static int iot_slot_add_uart(struct iot_slot *slot, struct list_head *uart_item)
{
	int ret = 0;
	struct platform_device *pdev = slot->pdev;
	struct device *device = &pdev->dev;
	struct iot_slot_platform_data *pdata = dev_get_platdata(&pdev->dev);
	if (pdata->request_uart) {
		/*
		 * TODO: Should the request_uart be providing some sort of uart handle
		 * that we can pass to a kernel module?
		 */
		ret = pdata->request_uart();
		if (ret != 0)
		{
			dev_err(device, "Couldn't request the UART interface\n");
			ret = -ENODEV;
			goto done;
		}
	}
	slot->interfaces[slot->num_interfaces].type =
		IOT_SLOT_INTERFACE_UART;
	slot->num_interfaces++;

done:
	return ret;
}

static void iot_slot_release_resources(struct iot_slot *slot)
{
	struct platform_device *pdev = slot->pdev;
	struct device *device = &pdev->dev;
	struct iot_slot_platform_data *pdata = dev_get_platdata(device);
	int i;

	for (i = 0; i < slot->num_interfaces; i++) {
		int gpio;
		dev_info(device, "On interface %d of type %d\n", i, slot->interfaces[i].type);
		switch (slot->interfaces[i].type) {
		case IOT_SLOT_INTERFACE_GPIO:
			for (gpio = 0; gpio < ARRAY_SIZE(pdata->gpio); gpio++) {
				devm_gpio_free(device, pdata->gpio[gpio]);
			}
			break;

		case IOT_SLOT_INTERFACE_I2C:
			i2c_unregister_device(
				slot->interfaces[i].data.i2c.client);
			/*
			 * NOTE: I2C is released lower down because it is
			 * acquired to read the eeprom
			 */
			break;

		case IOT_SLOT_INTERFACE_SPI:
			spi_unregister_device(
				slot->interfaces[i].data.spi.device);
			if (pdata->release_spi() != 0)
				dev_err(device, "Couldn't release SPI\n");
			break;

		case IOT_SLOT_INTERFACE_USB:
			/* No cleanup required */
			break;

		case IOT_SLOT_INTERFACE_SDIO:
			if (pdata->release_sdio() != 0)
				dev_err(device, "Couldn't release SDIO\n");
			break;

		case IOT_SLOT_INTERFACE_ADC:
			if (pdata->release_adc() != 0)
				dev_err(device, "Couldn't release ADC\n");
			break;

		case IOT_SLOT_INTERFACE_PCM:
			if (pdata->release_pcm() != 0)
				dev_err(device, "Couldn't release PCM\n");
			break;

		case IOT_SLOT_INTERFACE_CLK:
			/* No cleanup required */
			break;

		case IOT_SLOT_INTERFACE_UART:
			if (pdata->release_uart && pdata->release_uart() != 0)
				dev_err(device, "Couldn't release UART\n");
			break;

		case IOT_SLOT_INTERFACE_PLATFORM:
			/* TODO: ??? */
			break;

		default:
			dev_err(device,
				"Found interface type %d, which is not supported\n",
				slot->interfaces[i].type);
			break;
		}
	}

	if (slot->i2c_adapter) {
		int ret = pdata->release_i2c(&slot->i2c_adapter);
		if (ret != 0) {
			dev_err(device, "Failed to release i2c adapter");
		}
		slot->i2c_adapter = NULL;
	}

	devm_gpio_free(device, pdata->card_detect_gpio);
	devm_gpio_free(device, pdata->reset_gpio);

	slot->pdev = NULL;
	slot->card_present = false;
}

static int iot_slot_request_essential_resources(struct iot_slot *slot)
{
	int ret = 0;
	struct device* device;
	struct iot_slot_platform_data *pdata;
	int slot_index;
	char gpio_label[32];

	device = &slot->pdev->dev;
	pdata = dev_get_platdata(device);
	slot_index = slot->pdev->id;

	ret = snprintf(gpio_label, ARRAY_SIZE(gpio_label), "IoT slot %d reset",
		      slot_index);
	BUG_ON(ret >= ARRAY_SIZE(gpio_label));
	ret = devm_gpio_request_one(device, pdata->reset_gpio,
				    (GPIOF_OUT_INIT_HIGH | GPIOF_ACTIVE_LOW),
				    gpio_label);
	if (ret != 0) {
		dev_err(device, "Couldn't acquire reset gpio on IoT slot %d\n",
			slot_index);
		goto cleanup;
	}

	ret = snprintf(gpio_label, ARRAY_SIZE(gpio_label),
		      "IoT slot %d card detect", slot_index);
	BUG_ON(ret >= ARRAY_SIZE(gpio_label));
	ret = devm_gpio_request_one(device,
				    pdata->card_detect_gpio,
				    GPIOF_IN, gpio_label);
	if (ret != 0) {
		dev_err(device,
			 "Couldn't acquire card detect gpio on IoT slot %d\n",
			 slot_index);
		goto cleanup;
	}

	ret = pdata->request_i2c(&slot->i2c_adapter);
	if (ret != 0) {
		// Couldn't get i2c adapter
		goto cleanup;
	}

	return ret;

cleanup:
	iot_slot_release_resources(slot);
	return ret;
}


static int iot_slot_probe(struct platform_device *pdev)
{
	int ret = 0;
	int new_slot_index = pdev->id;
	struct device *device = &pdev->dev;
	struct iot_slot *slot;

	if (new_slot_index < 0 ||
	    new_slot_index >= CONFIG_IOT_SLOT_NUM_SUPPORTED) {
		dev_err(device, "IoT slot has invalid index (%d)\n",
			new_slot_index);
		ret = -EINVAL;
		goto done;
	}

	mutex_lock(&management_mutex);
	if (slots[new_slot_index] != NULL) {
		dev_err(device,
			"An IoT slot device has already been created for slot %d\n",
			new_slot_index);
		ret = -EINVAL;
		goto unlock;
	}

	slot = devm_kzalloc(&pdev->dev, sizeof(*slot), GFP_KERNEL);
	if (!slot) {
		ret = -ENOMEM;
		goto unlock;
	}
	slots[new_slot_index] = slot;
	slot->pdev = pdev;
	slot->num_interfaces = 0;

	platform_set_drvdata(pdev, slot);

	ret = iot_slot_request_essential_resources(slot);
	if (ret != 0) {
		dev_warn(device,
			 "Couldn't acquire resources required to perform IoT card enumeration\n");
		goto cleanup;
	}

	ret = iot_slot_enumerate(slot);
	if (ret != 0)
		dev_warn(device,
			 "IoT card enumeration failed for slot %d\n",
			 new_slot_index);

cleanup:
	if (ret != 0) {
		kfree(slot);
		slots[new_slot_index] = NULL;
	}
unlock:
	mutex_unlock(&management_mutex);
done:
	return ret;
}

static int iot_slot_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct iot_slot *slot = platform_get_drvdata(pdev);
	const int slot_index = pdev->id;
	if (slot_index < 0 || slot_index >= CONFIG_IOT_SLOT_NUM_SUPPORTED) {
		dev_err(&pdev->dev, "IoT slot has invalid index (%d)\n",
			slot_index);
		ret = -EINVAL;
		goto done;
	}

	if (slots[slot_index]->pdev == NULL) {
		dev_err(&pdev->dev,
			"Tried to remove IoT slot %d, but it does not exist\n",
			slot_index);
		ret = -EINVAL;
		goto done;
	}

	if (slots[slot_index] != slot || slots[slot_index]->pdev != pdev) {
		dev_err(&pdev->dev,
			"The platform device which claims to be registered for IoT slot %d does not appear to be the actual device registered\n",
			slot_index);
		ret = -EINVAL;
		goto done;
	}

	iot_slot_release_resources(slot);
	kfree(slot);
	slots[slot_index] = NULL;

done:
	return ret;
}


static const struct platform_device_id iot_slot_ids[] = {
	{"iot-slot", (kernel_ulong_t)0},
	{},
};
MODULE_DEVICE_TABLE(platform, iot_slot_ids);

static struct platform_driver iot_slot_driver = {
	.probe		= iot_slot_probe,
	.remove		= iot_slot_remove,
	.driver		= {
		.name	= "iot-slot",
		.owner	= THIS_MODULE,
		.bus	= &platform_bus_type,
	},
	.id_table	= iot_slot_ids,
};

static int __init iot_slot_init(void)
{
	mutex_init(&management_mutex);
	platform_driver_register(&iot_slot_driver);
	return 0;
}

static void __exit iot_slot_exit(void)
{
	platform_driver_unregister(&iot_slot_driver);
}

module_init(iot_slot_init);
module_exit(iot_slot_exit);

MODULE_ALIAS("platform:iot-slot");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sierra Wireless");
MODULE_DESCRIPTION("IoT slot management for IoT cards");
MODULE_VERSION("0.2");
