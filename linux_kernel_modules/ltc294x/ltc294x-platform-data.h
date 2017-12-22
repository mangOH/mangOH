#ifndef LTC294X_PLATFORM_DATA_H
#define LTC294X_PLATFORM_DATA_H

struct ltc294x_platform_data
{
	const char *name;
	int r_sense;
	u32 prescaler_exp;
};

#endif /* LTC294X_PLATFORM_DATA_H */

