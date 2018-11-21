#ifndef MANGOH_COMMON_H
#define MANGOH_COMMON_H

#include <linux/kernel.h>

/* TODO: There should be a better way to convert from WP GPIO numbers to real GPIO numbers */
#if defined(CONFIG_ARCH_MSM9615) /* For WPX5XX */
//#include "/opt/swi/y22-ext-SWI9X15Y_07.13.05.00/sysroots/armv7a-neon-poky-linux-gnueabi/usr/src/kernel/arch/arm/mach-msm/board-9615.h"

#include <../arch/arm/mach-msm/board-9615.h>
#elif defined(CONFIG_ARCH_MDM9607) /* For WP76XX */
#include <../arch/arm/mach-msm/include/mach/swimcu.h>
#endif

#if defined(CONFIG_ARCH_MSM9615)
#define PRIMARY_I2C_BUS (0)
#define PRIMARY_SPI_BUS (0)

#define CF3_GPIO42	(80)
#define CF3_GPIO13	(34)
#define CF3_GPIO7	(79)
#define CF3_GPIO8	(29)
#define CF3_GPIO2	(59)
#define CF3_GPIO33	(78)
#define CF3_GPIO36     SWIMCU_GPIO_TO_SYS(2)

#elif defined(CONFIG_ARCH_MDM9607) /* For WP76XX */

#define PRIMARY_I2C_BUS (4)
#define PRIMARY_SPI_BUS (1)

#define CF3_GPIO42	(79)
#define CF3_GPIO13	(76)
#define CF3_GPIO7	(16)
#define CF3_GPIO8	(58)
#define CF3_GPIO2	(38)
#define CF3_GPIO33	(78)
#define CF3_GPIO36     SWIMCU_GPIO_TO_SYS(0)
#endif

#endif /* MANGOH_COMMON_H */
