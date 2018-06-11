#include <linux/device.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include "bmi08x_defs.h"

static bool bmi088a_is_writeable_reg(struct device *dev, unsigned int reg)
{
        if ((reg >= BMI08X_ACCEL_FEATURE_CFG_REG) &&
            (reg <= BMI08X_ACCEL_FEATURE_CFG_REG + BMI08X_ACCEL_ANYMOTION_LEN * 2))
                return true;

	switch (reg) {
        case BMI08X_ACCEL_SOFTRESET_REG:
        case BMI08X_ACCEL_PWR_CTRL_REG:
        case BMI08X_ACCEL_PWR_CONF_REG:
        case BMI08X_ACCEL_SELF_TEST_REG:
        case BMI08X_ACCEL_INT1_INT2_MAP_DATA_REG:
        case BMI08X_ACCEL_INT1_IO_CONF_REG:
        case BMI08X_ACCEL_INT2_IO_CONF_REG:
        case BMI08X_ACCEL_RANGE_REG:
        case BMI08X_ACCEL_CONF_REG:
                return true;

	default:
                dev_err(dev, "%s(): Error register(0x%04x) not writable\n", __func__, reg);
		return false;
	};
}

static bool bmi088a_is_volatile_reg(struct device *dev, unsigned int reg)
{
        switch (reg) {
        case BMI08X_ACCEL_PWR_CTRL_REG:
        case BMI08X_ACCEL_PWR_CONF_REG:
        case BMI08X_ACCEL_SELF_TEST_REG:
        case BMI08X_ACCEL_INT1_INT2_MAP_DATA_REG:
        case BMI08X_ACCEL_INT1_IO_CONF_REG:
        case BMI08X_ACCEL_INT2_IO_CONF_REG:
        case BMI08X_ACCEL_RANGE_REG:
        case BMI08X_ACCEL_CONF_REG:
        case BMI08X_TEMP_MSB_REG:
        case BMI08X_TEMP_LSB_REG:
        case BMI08X_ACCEL_SENSORTIME_2_REG:
        case BMI08X_ACCEL_SENSORTIME_1_REG:
        case BMI08X_ACCEL_SENSORTIME_0_REG:
        case BMI08X_ACCEL_Z_MSB_REG:
        case BMI08X_ACCEL_Z_LSB_REG:
        case BMI08X_ACCEL_Y_MSB_REG:
        case BMI08X_ACCEL_Y_LSB_REG:
        case BMI08X_ACCEL_X_MSB_REG:
        case BMI08X_ACCEL_X_LSB_REG:
        case BMI08X_ACCEL_STATUS_REG:
        case BMI08X_ACCEL_ERR_REG:
        case BMI08X_ACCEL_CHIP_ID_REG:
                return true;

        default:
                break;
        }

	return false;
}

const struct regmap_config bmi088a_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = BMI08X_ACCEL_SOFTRESET_REG,
	.cache_type = REGCACHE_NONE,

	.writeable_reg = bmi088a_is_writeable_reg,
	.volatile_reg = bmi088a_is_volatile_reg,
};
EXPORT_SYMBOL(bmi088a_regmap_config);

static bool bmi088g_is_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
        case BMI08X_GYRO_SOFTRESET_REG:
        case BMI08X_GYRO_SELF_TEST_REG:
        case BMI08X_GYRO_INT3_INT4_IO_MAP_REG:
        case BMI08X_GYRO_INT3_INT4_IO_CONF_REG:
        case BMI08X_GYRO_INT_CTRL_REG:
        case BMI08X_GYRO_LPM1_REG:
        case BMI08X_GYRO_BANDWIDTH_REG:
        case BMI08X_GYRO_RANGE_REG:
                return true;

	default:
                dev_err(dev, "%s(): Error register(0x%04x) not writable\n", __func__, reg);
		return false;
	};
}

static bool bmi088g_is_volatile_reg(struct device *dev, unsigned int reg)
{
        switch (reg) {
        case BMI08X_GYRO_SELF_TEST_REG:
        case BMI08X_GYRO_INT3_INT4_IO_MAP_REG:
        case BMI08X_GYRO_INT3_INT4_IO_CONF_REG:
        case BMI08X_GYRO_INT_CTRL_REG:
        case BMI08X_GYRO_LPM1_REG:
        case BMI08X_GYRO_BANDWIDTH_REG:
        case BMI08X_GYRO_RANGE_REG:
        case BMI08X_GYRO_INT_STAT_1_REG:
        case BMI08X_GYRO_Z_MSB_REG:
        case BMI08X_GYRO_Z_LSB_REG:
        case BMI08X_GYRO_Y_MSB_REG:
        case BMI08X_GYRO_Y_LSB_REG:
        case BMI08X_GYRO_X_MSB_REG:
        case BMI08X_GYRO_X_LSB_REG:
        case BMI08X_GYRO_CHIP_ID_REG:
                return true;

        default:
                break;
        }

	return false;
}

const struct regmap_config bmi088g_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = BMI08X_GYRO_SELF_TEST_REG,
	.cache_type = REGCACHE_NONE,

	.writeable_reg = bmi088g_is_writeable_reg,
	.volatile_reg = bmi088g_is_volatile_reg,
};
EXPORT_SYMBOL(bmi088g_regmap_config);
