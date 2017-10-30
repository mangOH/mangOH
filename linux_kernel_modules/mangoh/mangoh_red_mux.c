#include <linux/kernel.h>
#include <linux/gpio.h>
#include "mangoh_red_mux.h"

static struct platform_device *board_device;

static int sdio_mux_gpio;
static struct mutex sdio_mutex;
static enum sdio_selection sdio_current_selection;
static unsigned int sdio_num_requested;

static int pcm_mux_gpio;
static struct mutex pcm_mutex;
static enum pcm_selection pcm_current_selection;
static unsigned int pcm_num_requested;


int mangoh_red_mux_init(struct platform_device *pdev, int sdio_gpio,
			int pcm_gpio)
{
	int ret;
	struct device *device = &pdev->dev;
	board_device = pdev;

	sdio_mux_gpio = sdio_gpio;
	pcm_mux_gpio = pcm_gpio;

	dev_info(device,
		"Initializing mangOH mux with sdio_gpio=%d, pcm_gpio=%d\n",
		sdio_mux_gpio, pcm_mux_gpio);

	ret = devm_gpio_request_one(device, sdio_mux_gpio,
				    GPIOF_OUT_INIT_HIGH,
				    "sdio mux");
	if (ret != 0) {
		dev_err(device, "Couldn't acquire GPIO for SDIO mux\n");
		return ret;
	}

	ret = devm_gpio_request_one(device, pcm_mux_gpio,
				    GPIOF_OUT_INIT_HIGH,
				    "pcm mux");
	if (ret != 0) {
		dev_err(device, "Couldn't acquire GPIO for PCM mux\n");
		/* Release the SDIO mux gpio that was acquired previously */
		devm_gpio_free(device, sdio_mux_gpio);
		return ret;
	}

	mutex_init(&sdio_mutex);
	mutex_init(&pcm_mutex);

	/* Initialize based on hardware defaults */
	sdio_current_selection = SDIO_SELECTION_SD_CARD_SLOT;
	pcm_current_selection = PCM_SELECTION_ONBOARD;

	sdio_num_requested = 0;
	pcm_num_requested = 0;

	return 0;
}

void mangoh_red_mux_deinit(void)
{
	struct device *device = &board_device->dev;
	devm_gpio_free(device, pcm_mux_gpio);
	devm_gpio_free(device, sdio_mux_gpio);
}

int mangoh_red_mux_sdio_select(enum sdio_selection selection)
{
	int ret = 0;
	dev_info(&board_device->dev, "SDIO mux: selecting %s\n",
		 (selection == SDIO_SELECTION_SD_CARD_SLOT ?
		  "SD card slot" : "IoT slot"));
	mutex_lock(&sdio_mutex);
	if (selection == sdio_current_selection)
		sdio_num_requested++;
	else {
		if (sdio_num_requested == 0) {
			ret = gpio_direction_output(
				sdio_mux_gpio,
				selection == SDIO_SELECTION_SD_CARD_SLOT ?
				1 : 0);
			if (ret != 0) {
				dev_err(&board_device->dev,
					"Couldn't set sdio mux\n");
				goto unlock;
			}
			sdio_current_selection = selection;
			sdio_num_requested++;
		} else {
			ret = -EBUSY;
		}
	}

unlock:
	mutex_unlock(&sdio_mutex);

	return ret;
}

int mangoh_red_mux_sdio_release(enum sdio_selection selection)
{
	int ret = 0;
	mutex_lock(&sdio_mutex);

	if (selection != sdio_current_selection) {
		dev_err(&board_device->dev,
			"Trying to release SDIO mux, but the current selection differs from what the client specified\n");
		ret = -EACCES;
		goto unlock;
	}

	if (sdio_num_requested != 0) {
		sdio_num_requested--;
	} else {
		dev_err(&board_device->dev,
			"Couldn't release SDIO since it wasn't requested\n");
		ret = -ENOLCK;
	}
unlock:
	mutex_unlock(&sdio_mutex);

	return ret;
}

int mangoh_red_mux_pcm_select(enum pcm_selection selection)
{
	int ret = 0;
	dev_info(&board_device->dev, "PCM mux: selecting %s\n",
		 (selection == PCM_SELECTION_ONBOARD ? "onboard" : "IoT slot"));
	mutex_lock(&pcm_mutex);
	if (selection == pcm_current_selection)
		pcm_num_requested++;
	else {
		if (pcm_num_requested == 0) {
			ret = gpio_direction_output(
				pcm_mux_gpio,
				selection == PCM_SELECTION_ONBOARD ?
				1 : 0);
			if (ret != 0) {
				dev_err(&board_device->dev,
					"Couldn't set pcm mux\n");
				goto unlock;
			}
			pcm_current_selection = selection;
			pcm_num_requested++;
		} else {
			ret = -EBUSY;
		}
	}

unlock:
	mutex_unlock(&pcm_mutex);

	return ret;
}

int mangoh_red_mux_pcm_release(enum pcm_selection selection)
{
	int ret = 0;
	mutex_lock(&pcm_mutex);

	if (selection != pcm_current_selection) {
		dev_err(
			&board_device->dev,
			"Trying to release PCM mux, but the current selection differs from what the client specified\n");
		ret = -EACCES;
		goto unlock;
	}

	if (pcm_num_requested != 0) {
		pcm_num_requested--;
	} else {
		dev_err(&board_device->dev,
			"Couldn't release PCM since it wasn't requested\n");
		ret = -ENOLCK;
	}
unlock:
	mutex_unlock(&pcm_mutex);

	return ret;
}
