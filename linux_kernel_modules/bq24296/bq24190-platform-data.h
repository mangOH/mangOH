/*
 * Platform data for the TI bq24190 battery charger driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef BQ24190_PLATFORM_DATA_H_
#define BQ24190_PLATFORM_DATA_H_

struct bq24190_platform_data {
	int	gpio_int;	/* GPIO pin that's connected to INT# */
};

#endif
