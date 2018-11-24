#ifndef MANGOH_COMMON_H
#define MANGOH_COMMON_H

#include <linux/kernel.h>

/* TODO: There should be a better way to convert from WP GPIO numbers to real GPIO numbers */
#if defined(CONFIG_ARCH_MSM9615) /* For WP75, WP85 */
#include <../arch/arm/mach-msm/board-9615.h>
#elif defined(CONFIG_ARCH_MDM9607) /* For WP76, WP77 */
#include <../arch/arm/mach-msm/include/mach/swimcu.h>
#endif

#if defined(CONFIG_ARCH_MSM9615) /* For WP75, WP85 */
#define PRIMARY_I2C_BUS (0)
#define PRIMARY_SPI_BUS (0)

#define CF3_GPIO42	(80)
#define CF3_GPIO13	(34)
#define CF3_GPIO7	(79)
#define CF3_GPIO8	(29)
#define CF3_GPIO2	(59)
#define CF3_GPIO33	(78)
#define CF3_GPIO36	SWIMCU_GPIO_TO_SYS(2)
/* NOTE: this may be labeled as SPI2_CLK on the mangOH Red schematic */
#define CF3_GPIO38	SWIMCU_GPIO_TO_SYS(4)
#define CF3_GPIO32	(30)

#elif defined(CONFIG_ARCH_MDM9607) /* For WP76, WP77 */

#define PRIMARY_I2C_BUS (4)
#define PRIMARY_SPI_BUS (1)

#define CF3_GPIO42	(79)
#define CF3_GPIO13	(76)
#define CF3_GPIO7	(16)
#define CF3_GPIO8	(58)
#define CF3_GPIO2	(38)
#define CF3_GPIO33	(78)
#define CF3_GPIO36	SWIMCU_GPIO_TO_SYS(0)
/* NOTE: this may be labeled as SPI2_CLK on the mangOH Red schematic */
#define CF3_GPIO38	SWIMCU_GPIO_TO_SYS(2)
#define CF3_GPIO32	(77)

#endif /* CONFIG_ARCH_? */

#endif /* MANGOH_COMMON_H */
