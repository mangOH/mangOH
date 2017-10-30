#ifndef MANGOH_RED_MUX_H
#define MANGOH_RED_MUX_H

#include <linux/platform_device.h>

enum sdio_selection {
	SDIO_SELECTION_SD_CARD_SLOT,
	SDIO_SELECTION_IOT_SLOT,
};

enum pcm_selection {
	PCM_SELECTION_IOT_SLOT,
	PCM_SELECTION_ONBOARD,
};

int mangoh_red_mux_init(struct platform_device *pdev, int sdio_gpio, int pcm_gpio);
void mangoh_red_mux_deinit(void);
int mangoh_red_mux_sdio_select(enum sdio_selection selection);
int mangoh_red_mux_sdio_release(enum sdio_selection selection);
int mangoh_red_mux_pcm_select(enum pcm_selection selection);
int mangoh_red_mux_pcm_release(enum pcm_selection selection);

#endif /* MANGOH_RED_MUX_H */
