#ifndef IOT_SLOT_H
#define IOT_SLOT_H

struct spi_master;
struct i2c_adapter;

/*
 * TODO:
 * Need to decide whether an absent function pointer means that no action is
 * needed to acquire the interface or that the interface is not supported on the
 * IoT slot. I think not supported makes more sense unless we separate out
 * muxing from getting. Eg. have a require_x, get_x, release_x functions.
 */
struct iot_slot_platform_data {
	int gpio[4];
	int reset_gpio;
	int card_detect_gpio;

	int (*request_i2c)(struct i2c_adapter **adapter);
	int (*release_i2c)(struct i2c_adapter **adapter);

	int (*request_spi)(struct spi_master **spi_master, int *cs);
	int (*release_spi)(void);

	/* TODO: what output param(s) for uart? */
	int (*request_uart)(void);
	int (*release_uart)(void);

	/* TODO: how are adc's managed in the kernel? */
	int (*request_adc)(void);
	int (*release_adc)(void);

	/* TODO: output params? */
	int (*request_sdio)(void);
	int (*release_sdio)(void);

	/* TODO: output params? */
	int (*request_pcm)(void);
	int (*release_pcm)(void);
};

#endif /* IOT_SLOT_H */
