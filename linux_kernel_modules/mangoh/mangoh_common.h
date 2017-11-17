#ifndef MANGOH_COMMON_H
#define MANGOH_COMMON_H

#include <linux/kernel.h>

/* TODO: There should be a better way to convert from WP GPIO numbers to real GPIO numbers */
#if defined(CONFIG_ARCH_MSM9615) /* For WPX5XX */
#define PRIMARY_I2C_BUS (0)
#define PRIMARY_SPI_BUS (0)

#define CF3_GPIO42	(80)
#define CF3_GPIO13	(34)
#define CF3_GPIO7	(79)
#define CF3_GPIO8	(29)
#define CF3_GPIO2	(59)
#define CF3_GPIO33	(78)
#elif defined(CONFIG_ARCH_MDM9607) /* For WP76XX */
#define PRIMARY_I2C_BUS (4)
#define PRIMARY_SPI_BUS (1)

#define CF3_GPIO42	(79)
#define CF3_GPIO13	(76)
#define CF3_GPIO7	(16)
#define CF3_GPIO8	(58)
#define CF3_GPIO2	(38)
#define CF3_GPIO33	(78)
#endif

#endif /* MANGOH_COMMON_H */
