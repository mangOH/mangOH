#ifndef FB_WAVESHARE_EINK_H
#define FB_WAVESHARE_EINK_H

struct waveshare_eink_platform_data {
	int rst_gpio;
	int dc_gpio;
	int busy_gpio;
};

#endif /* FB_WAVESHARE_EINK_H */
