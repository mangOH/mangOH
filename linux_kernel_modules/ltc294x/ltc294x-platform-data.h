#ifndef LTC294X_PLATFORM_DATA_H
#define LTC294X_PLATFORM_DATA_H

// TODO: duplicated between .c and .h - that's bad
enum ltc294x_id {
	LTC2941_ID,
	LTC2942_ID,
	LTC2943_ID,
	LTC2944_ID,
};

struct ltc294x_platform_data
{
	enum ltc294x_id chip_id;
	int r_sense;
	u32 prescaler_exp;
	const char *name;
};

#endif /* LTC294X_PLATFORM_DATA_H */

