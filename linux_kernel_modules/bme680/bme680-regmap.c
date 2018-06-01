#include <linux/device.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include "bme680_defs.h"

static bool bme680_is_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case BME680_CONF_HEAT_CTRL_ADDR:
	case BME680_CONF_ODR_RUN_GAS_NBC_ADDR:
	case BME680_CONF_OS_H_ADDR:
	case BME680_CONF_T_P_MODE_ADDR:
        case BME680_SOFT_RESET_ADDR:
        case BME680_GAS_WAIT0_ADDR:
		return true;
	default:
                if (((reg >= BME680_CONF_ODR_FILT_ADDR) && 
                     (reg <= BME680_CONF_ODR_FILT_ADDR + BME680_FILTER_SIZE_127)) ||
                    ((reg >= BME680_ADDR_SENS_CONF_START) && 
                     (reg <= BME680_ADDR_SENS_CONF_START + BME680_FILTER_SIZE_7)))
                        return true;

                dev_err(dev, "%s(): Error register(0x%04x) not writable\n", __func__, reg);
		return false;
	};
}

static bool bme680_is_volatile_reg(struct device *dev, unsigned int reg)
{
	if (((reg >= BME680_ADDR_GAS_CONF_START) && (reg <= BME680_ADDR_GAS_CONF_START + 9)) ||
            ((reg >= BME680_ADDR_SENS_CONF_START) && (reg <= BME680_ADDR_SENS_CONF_START + 9)))
		return true;
	
	return false;
}

const struct regmap_config bme680_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = BME680_COEFF_ADDR2 + BME680_COEFF_ADDR2_LEN,
	.cache_type = REGCACHE_NONE,

	.writeable_reg = bme680_is_writeable_reg,
	.volatile_reg = bme680_is_volatile_reg,
};
EXPORT_SYMBOL(bme680_regmap_config);
