/*
 * BMI160 - Bosch IMU (accel, gyro plus external magnetometer)
 *
 * Copyright (c) 2016, Intel Corporation.
 *
 * This file is subject to the terms and conditions of version 2 of
 * the GNU General Public License.  See the file COPYING in the main
 * directory of this archive for more details.
 *
 * IIO core driver for BMI160, with support for I2C/SPI busses
 *
 * TODO: magnetometer, interrupts, hardware FIFO
 */
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/acpi.h>
#include <linux/delay.h>

#include <linux/iio/iio.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/buffer.h>
#include <linux/iio/sysfs.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
#include <linux/bitfield.h>
#else
#define __bf_shf(x) (__builtin_ffsll(x) - 1)
#define FIELD_PREP(_mask, _val)						\
	({								\
		((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask);	\
	})
#endif

#include "bmi160.h"

#define BMI160_REG_CHIP_ID			0x00
#define BMI160_CHIP_ID_VAL			0xD1

#define BMI160_REG_PMU_STATUS			0x03

/* X axis data low byte address, the rest can be obtained using axis offset */
#define BMI160_REG_DATA_MAGN_XOUT_L		0x04
#define BMI160_REG_DATA_GYRO_XOUT_L		0x0C
#define BMI160_REG_DATA_ACCEL_XOUT_L		0x12

#define BMI160_REG_STATUS			0x1B
#define BMI160_STATUS_GYR_SELF_TEST_OK		BIT(1)
#define BMI160_STATUS_MAG_MAN_OP		BIT(2)
#define BMI160_STATUS_FOC_RDY			BIT(3)
#define BMI160_STATUS_NVM_RDY			BIT(4)
#define BMI160_STATUS_DRDY_MAG			BIT(5)
#define BMI160_STATUS_DRDY_GYR			BIT(6)
#define BMI160_STATUS_DRDY_ACC			BIT(7)

#define BMI160_REG_INT_STATUS_0			0x1C
#define BMI160_INT_STATUS_0_STEP		BIT(0)
#define BMI160_INT_STATUS_0_SIGMOT		BIT(1)
#define BMI160_INT_STATUS_0_ANYM		BIT(2)
#define BMI160_INT_STATUS_0_PMU_TRIGGER		BIT(3)
#define BMI160_INT_STATUS_0_DTAP		BIT(4)
#define BMI160_INT_STATUS_0_STAP		BIT(5)
#define BMI160_INT_STATUS_0_ORIENT		BIT(6)
#define BMI160_INT_STATUS_0_FLAT		BIT(7)

#define BMI160_REG_INT_STATUS_1			0x1D
#define BMI160_INT_STATUS_1_HIGHG		BIT(2)
#define BMI160_INT_STATUS_1_LOWG		BIT(3)
#define BMI160_INT_STATUS_1_DRDY		BIT(4)
#define BMI160_INT_STATUS_1_FFULL		BIT(5)
#define BMI160_INT_STATUS_1_FWM			BIT(6)
#define BMI160_INT_STATUS_1_NOMO		BIT(7)

#define BMI160_REG_INT_STATUS_2			0x1E
#define BMI160_INT_STATUS_2_ANYM_FIRST_X	BIT(0)
#define BMI160_INT_STATUS_2_ANYM_FIRST_Y	BIT(1)
#define BMI160_INT_STATUS_2_ANYM_FIRST_Z	BIT(2)
#define BMI160_INT_STATUS_2_ANYM_SIGN		BIT(3)
#define BMI160_INT_STATUS_2_TAP_FIRST_X		BIT(4)
#define BMI160_INT_STATUS_2_TAP_FIRST_Y		BIT(5)
#define BMI160_INT_STATUS_2_TAP_FIRST_Z		BIT(6)
#define BMI160_INT_STATUS_2_TAP_SIGN		BIT(7)

#define BMI160_REG_INT_STATUS_3			0x1F
#define BMI160_INT_STATUS_3_HIGHG_FIRST_X	BIT(0)
#define BMI160_INT_STATUS_3_HIGHG_FIRST_Y	BIT(1)
#define BMI160_INT_STATUS_3_HIGHG_FIRST_Z	BIT(2)
#define BMI160_INT_STATUS_3_HIGHG_SIGN		BIT(3)
/*
 * TODO: It's weird that the datasheet reports a 2-bit field for x & y and a
 * 1-bit field for z
 */
#define BMI160_INT_STATUS_3_ORIENT_XY		GENMASK(5, 4)
#define BMI160_INT_STATUS_3_ORIENT_Z		BIT(6)
#define BMI160_INT_STATUS_3_FLAT		BIT(7)

#define BMI160_REG_TEMPERATURE_0		0x20
#define BMI160_REG_TEMPERATURE_1		0x21

#define BMI160_REG_ACCEL_CONFIG			0x40
#define BMI160_ACCEL_CONFIG_ODR_MASK		GENMASK(3, 0)
#define BMI160_ACCEL_CONFIG_BWP_MASK		GENMASK(6, 4)

#define BMI160_REG_ACCEL_RANGE			0x41
#define BMI160_ACCEL_RANGE_2G			0x03
#define BMI160_ACCEL_RANGE_4G			0x05
#define BMI160_ACCEL_RANGE_8G			0x08
#define BMI160_ACCEL_RANGE_16G			0x0C

#define BMI160_REG_GYRO_CONFIG			0x42
#define BMI160_GYRO_CONFIG_ODR_MASK		GENMASK(3, 0)
#define BMI160_GYRO_CONFIG_BWP_MASK		GENMASK(5, 4)

#define BMI160_REG_GYRO_RANGE			0x43
#define BMI160_GYRO_RANGE_2000DPS		0x00
#define BMI160_GYRO_RANGE_1000DPS		0x01
#define BMI160_GYRO_RANGE_500DPS		0x02
#define BMI160_GYRO_RANGE_250DPS		0x03
#define BMI160_GYRO_RANGE_125DPS		0x04

#define BMI160_REG_INT_EN_0			0x50
#define BMI160_INT_EN_0_ANYM_X			BIT(0)
#define BMI160_INT_EN_0_ANYM_Y			BIT(1)
#define BMI160_INT_EN_0_ANYM_Z			BIT(2)
#define BMI160_INT_EN_0_DTAP			BIT(4)
#define BMI160_INT_EN_0_STAP			BIT(5)
#define BMI160_INT_EN_0_ORIENT			BIT(6)
#define BMI160_INT_EN_0_FLAT			BIT(7)

#define BMI160_REG_INT_EN_1			0x51
#define BMI160_INT_EN_1_HIGHX			BIT(0)
#define BMI160_INT_EN_1_HIGHY			BIT(1)
#define BMI160_INT_EN_1_HIGHZ			BIT(2)
#define BMI160_INT_EN_1_LOW			BIT(3)
#define BMI160_INT_EN_1_DRDY			BIT(4)
#define BMI160_INT_EN_1_FFULL			BIT(5)
#define BMI160_INT_EN_1_FWM			BIT(6)

#define BMI160_REG_INT_EN_2			0x52
#define BMI160_INT_EN_2_NOMOX			BIT(0)
#define BMI160_INT_EN_2_NOMOY			BIT(1)
#define BMI160_INT_EN_2_NOMOZ			BIT(2)
#define BMI160_INT_EN_2_STEP_DET		BIT(3)

#define BMI160_REG_INT_OUT_CTRL			0x53
#define BMI160_INT_OUT_CTRL_INT1_EDGE		BIT(0)
#define BMI160_INT_OUT_CTRL_INT1_LVL		BIT(1)
#define BMI160_INT_OUT_CTRL_INT1_OD		BIT(2)
#define BMI160_INT_OUT_CTRL_INT1_OUTPUT_EN	BIT(3)
#define BMI160_INT_OUT_CTRL_INT2_EDGE		BIT(4)
#define BMI160_INT_OUT_CTRL_INT2_LVL		BIT(5)
#define BMI160_INT_OUT_CTRL_INT2_OD		BIT(6)
#define BMI160_INT_OUT_CTRL_INT2_OUTPUT_EN	BIT(7)

#define BMI160_REG_INT_LATCH			0x54
#define BMI160_INT_LATCH_MODE			GENMASK(3, 0)
#define BMI160_INT_LATCH_MODE_NON_LATCHED	0
#define BMI160_INT_LATCH_MODE_TMP_312500_NS	1
#define BMI160_INT_LATCH_MODE_TMP_625_US	2
#define BMI160_INT_LATCH_MODE_TMP_1250_US	3
#define BMI160_INT_LATCH_MODE_TMP_2500_US	4
#define BMI160_INT_LATCH_MODE_TMP_5_MS		5
#define BMI160_INT_LATCH_MODE_TMP_10_MS		6
#define BMI160_INT_LATCH_MODE_TMP_20_MS		7
#define BMI160_INT_LATCH_MODE_TMP_40_MS		8
#define BMI160_INT_LATCH_MODE_TMP_80_MS		9
#define BMI160_INT_LATCH_MODE_TMP_160_MS	10
#define BMI160_INT_LATCH_MODE_TMP_320_MS	11
#define BMI160_INT_LATCH_MODE_TMP_640_MS	12
#define BMI160_INT_LATCH_MODE_TMP_1280_MS	13
#define BMI160_INT_LATCH_MODE_TMP_2560_MS	14
#define BMI160_INT_LATCH_MODE_LATCHED		15
#define BMI160_INT_LATCH_INT1_INPUT_EN		BIT(4)
#define BMI160_INT_LATCH_INT2_INPUT_EN		BIT(5)

#define BMI160_REG_INT_MAP_0			0x55
#define BMI160_INT_MAP_0_INT1_LOWG_STEP_DET	BIT(0)
#define BMI160_INT_MAP_0_INT1_HIGHG		BIT(1)
#define BMI160_INT_MAP_0_INT1_ANYM_SIGMOT	BIT(2)
#define BMI160_INT_MAP_0_INT1_NOMO		BIT(3)
#define BMI160_INT_MAP_0_INT1_DTAP		BIT(4)
#define BMI160_INT_MAP_0_INT1_STAP		BIT(5)
#define BMI160_INT_MAP_0_INT1_ORIENTATION	BIT(6)
#define BMI160_INT_MAP_0_INT1_FLAT		BIT(7)

#define BMI160_REG_INT_MAP_1			0x56
#define BMI160_INT_MAP_1_INT2_PMU_TRIGGER	BIT(0)
#define BMI160_INT_MAP_1_INT2_FFULL		BIT(1)
#define BMI160_INT_MAP_1_INT2_FWM		BIT(2)
#define BMI160_INT_MAP_1_INT2_DRDY		BIT(3)
#define BMI160_INT_MAP_1_INT1_PMU_TRIGGER	BIT(4)
#define BMI160_INT_MAP_1_INT1_FFULL		BIT(5)
#define BMI160_INT_MAP_1_INT1_FWM		BIT(6)
#define BMI160_INT_MAP_1_INT1_DRDY		BIT(7)

#define BMI160_REG_INT_MAP_2			0x57
#define BMI160_INT_MAP_2_INT2_LOWG_STEP_DET	BIT(0)
#define BMI160_INT_MAP_2_INT2_HIGHG		BIT(1)
#define BMI160_INT_MAP_2_INT2_ANYM_SIGMOT	BIT(2)
#define BMI160_INT_MAP_2_INT2_NOMO		BIT(3)
#define BMI160_INT_MAP_2_INT2_DTAP		BIT(4)
#define BMI160_INT_MAP_2_INT2_STAP		BIT(5)
#define BMI160_INT_MAP_2_INT2_ORIENTATION	BIT(6)
#define BMI160_INT_MAP_2_INT2_FLAT		BIT(7)

#define BMI160_REG_INT_DATA_0			0x58
#define BMI160_INT_DATA_0_TAP_SRC		BIT(3)
#define BMI160_INT_DATA_0_LOW_HIGH_SRC		BIT(7)

#define BMI160_REG_INT_DATA_1			0x59
#define BMI160_INT_DATA_1_MOTION_SRC		BIT(7)

#define BMI160_REG_INT_MOTION_0			0x5F
#define BMI160_INT_MOTION_0_ANYM_DUR		GENMASK(1, 0)
#define BMI160_INT_MOTION_0_ANYM_DUR_DEFAULT		2
#define BMI160_INT_MOTION_0_SLO_NO_MOT_DUR	GENMASK(7, 2)

#define BMI160_REG_INT_MOTION_1			0x60
#define BMI160_INT_MOTION_1_ANYM_TH		GENMASK(7, 0)
#define BMI160_INT_MOTION_1_ANYM_TH_DEFAULT		128
/*
 * All 8-bits represent an acceleration slope which must be maintained for
 * int_anym_dur + 1 samples in order to trigger the interrupt. It seems that
 * each unit in this register represents:
 * (n * (MAX_ACCEL - * MIN_ACCEL)) / (256 * 4).
 * There is a special case for n==0 which is basically computed as though n==1/2
 */

#define BMI160_REG_INT_MOTION_2			0x61
#define BMI160_INT_MOTION_2_SLO_NO_MOT_TH	GENMASK(7, 0)

#define BMI160_REG_INT_MOTION_3			0x62
#define BMI160_INT_MOTION_3_NO_MOT_SEL		BIT(0)
#define BMI160_INT_MOTION_3_SIG_MOT_SEL		BIT(1)
#define BMI160_INT_MOTION_3_SIG_MOT_SEL_ANY	0
#define BMI160_INT_MOTION_3_SIG_MOT_SEL_SIG	1
#define BMI160_INT_MOTION_3_SIG_MOT_SKIP	GENMASK(3, 2)
#define BMI160_SIG_MOT_SKIP_TIME_1500_MS	0
#define BMI160_SIG_MOT_SKIP_TIME_3000_MS	1
#define BMI160_SIG_MOT_SKIP_TIME_6000_MS	2
#define BMI160_SIG_MOT_SKIP_TIME_12000_MS	3
#define BMI160_INT_MOTION_3_SIG_MOT_PROOF	GENMASK(5, 4)
#define BMI160_SIG_MOT_PROOF_TIME_250_MS	0
#define BMI160_SIG_MOT_PROOF_TIME_500_MS	1
#define BMI160_SIG_MOT_PROOF_TIME_1000_MS	2
#define BMI160_SIG_MOT_PROOF_TIME_2000_MS	3

#define BMI160_REG_CMD				0x7E
#define BMI160_CMD_START_FOC			0x03
#define BMI160_CMD_ACCEL_PM_SUSPEND		0x10
#define BMI160_CMD_ACCEL_PM_NORMAL		0x11
#define BMI160_CMD_ACCEL_PM_LOW_POWER		0x12
#define BMI160_CMD_GYRO_PM_SUSPEND		0x14
#define BMI160_CMD_GYRO_PM_NORMAL		0x15
#define BMI160_CMD_GYRO_PM_FAST_STARTUP		0x17
#define BMI160_CMD_MAG_PM_SUSPEND		0x19
#define BMI160_CMD_MAG_PM_NORMAL		0x1A
#define BMI160_CMD_MAG_PM_LOW_POWER		0x1B
#define BMI160_CMD_PROG_NVM			0xA0
#define BMI160_CMD_FIFO_FLUSH			0xB0
#define BMI160_CMD_INT_RESET			0xB1
#define BMI160_CMD_STEP_CNT_CLR			0xB2
#define BMI160_CMD_SOFTRESET			0xB6

#define BMI160_REG_DUMMY			0x7F

#define BMI160_ACCEL_PMU_MIN_USLEEP		3800
#define BMI160_GYRO_PMU_MIN_USLEEP		80000
#define BMI160_SOFTRESET_USLEEP			1000

#define BMI160_CHANNEL(_type, _axis, _index) {			\
	.type = _type,						\
	.modified = 1,						\
	.channel2 = IIO_MOD_##_axis,				\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |  \
		BIT(IIO_CHAN_INFO_SAMP_FREQ),			\
	.scan_index = _index,					\
	.scan_type = {						\
		.sign = 's',					\
		.realbits = 16,					\
		.storagebits = 16,				\
		.endianness = IIO_LE,				\
	},							\
}

/* scan indexes follow DATA register order */
enum bmi160_scan_axis {
	BMI160_SCAN_EXT_MAGN_X = 0,
	BMI160_SCAN_EXT_MAGN_Y,
	BMI160_SCAN_EXT_MAGN_Z,
	BMI160_SCAN_RHALL,
	BMI160_SCAN_GYRO_X,
	BMI160_SCAN_GYRO_Y,
	BMI160_SCAN_GYRO_Z,
	BMI160_SCAN_ACCEL_X,
	BMI160_SCAN_ACCEL_Y,
	BMI160_SCAN_ACCEL_Z,
	BMI160_SCAN_TIMESTAMP,
	BMI160_SCAN_TEMPERATURE,
};

enum bmi160_sensor_type {
	BMI160_ACCEL	= 0,
	BMI160_GYRO,
	BMI160_EXT_MAGN,
	BMI160_NUM_SENSORS /* must be last */
};

enum bmi160_pmu_state {
	BMI160_PMU_STATE_SUSPEND = 0,
	BMI160_PMU_STATE_NORMAL,
	BMI160_PMU_STATE_LOW_POWER,
	BMI160_PMU_STATE_FAST_STARTUP,
	BMI160_PMU_STATE_COUNT /* Special last element */
};

enum bmi160_interrupt_type {
	SIGMOT = 0,
	ANYMOT
};

static char *BMI160_INTERRUPT_TYPE_STR[] = {
	"significant_motion", "any_motion"
};

struct bmi160_data {
	struct regmap *regmap;
	int interrupt_type;
	u8 anymot_th;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0)
	/*
	 * This member is necessary in kernel versions < 3.16 because
	 * regmap_get_device() is not defined.
	 */
	struct device *dev;
#endif
};

const struct regmap_config bmi160_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};
EXPORT_SYMBOL(bmi160_regmap_config);

struct bmi160_regs {
	u8 data; /* LSB byte register for X-axis */
	u8 config;
	u8 config_odr_mask;
	u8 config_bwp_mask;
	u8 range;
	enum bmi160_pmu_state pmu_cmds[BMI160_PMU_STATE_COUNT];
};

static struct bmi160_regs bmi160_regs[] = {
	[BMI160_ACCEL] = {
		.data	= BMI160_REG_DATA_ACCEL_XOUT_L,
		.config	= BMI160_REG_ACCEL_CONFIG,
		.config_odr_mask = BMI160_ACCEL_CONFIG_ODR_MASK,
		.config_bwp_mask = BMI160_ACCEL_CONFIG_BWP_MASK,
		.range	= BMI160_REG_ACCEL_RANGE,
		.pmu_cmds = {
			[BMI160_PMU_STATE_SUSPEND] =
				BMI160_CMD_ACCEL_PM_SUSPEND,
			[BMI160_PMU_STATE_NORMAL] = BMI160_CMD_ACCEL_PM_NORMAL,
			[BMI160_PMU_STATE_LOW_POWER] =
				BMI160_CMD_ACCEL_PM_LOW_POWER,
		},
	},
	[BMI160_GYRO] = {
		.data	= BMI160_REG_DATA_GYRO_XOUT_L,
		.config	= BMI160_REG_GYRO_CONFIG,
		.config_odr_mask = BMI160_GYRO_CONFIG_ODR_MASK,
		.config_bwp_mask = BMI160_GYRO_CONFIG_BWP_MASK,
		.range	= BMI160_REG_GYRO_RANGE ,
		.pmu_cmds = {
			[BMI160_PMU_STATE_SUSPEND] =
				BMI160_CMD_GYRO_PM_SUSPEND,
			[BMI160_PMU_STATE_NORMAL] = BMI160_CMD_GYRO_PM_NORMAL,
			[BMI160_PMU_STATE_FAST_STARTUP] =
				BMI160_CMD_GYRO_PM_FAST_STARTUP,
		},
	},
};

static unsigned long bmi160_pmu_time[] = {
	[BMI160_ACCEL] = BMI160_ACCEL_PMU_MIN_USLEEP,
	[BMI160_GYRO] = BMI160_GYRO_PMU_MIN_USLEEP,
};

struct bmi160_scale {
	u8 bits;
	int uscale;
};

struct bmi160_odr {
	u8 bits;
	int odr;
	int uodr;
};

static const struct bmi160_scale bmi160_accel_scale[] = {
	{ BMI160_ACCEL_RANGE_2G, 598},
	{ BMI160_ACCEL_RANGE_4G, 1197},
	{ BMI160_ACCEL_RANGE_8G, 2394},
	{ BMI160_ACCEL_RANGE_16G, 4788},
};

static const struct bmi160_scale bmi160_gyro_scale[] = {
	{ BMI160_GYRO_RANGE_2000DPS, 1065},
	{ BMI160_GYRO_RANGE_1000DPS, 532},
	{ BMI160_GYRO_RANGE_500DPS, 266},
	{ BMI160_GYRO_RANGE_250DPS, 133},
	{ BMI160_GYRO_RANGE_125DPS, 66},
};

struct bmi160_scale_item {
	const struct bmi160_scale *tbl;
	int num;
};

static const struct  bmi160_scale_item bmi160_scale_table[] = {
	[BMI160_ACCEL] = {
		.tbl	= bmi160_accel_scale,
		.num	= ARRAY_SIZE(bmi160_accel_scale),
	},
	[BMI160_GYRO] = {
		.tbl	= bmi160_gyro_scale,
		.num	= ARRAY_SIZE(bmi160_gyro_scale),
	},
};

static const struct bmi160_odr bmi160_accel_odr[] = {
	{0x01, 0, 781250},
	{0x02, 1, 562500},
	{0x03, 3, 125000},
	{0x04, 6, 250000},
	{0x05, 12, 500000},
	{0x06, 25, 0},
	{0x07, 50, 0},
	{0x08, 100, 0},
	{0x09, 200, 0},
	{0x0A, 400, 0},
	{0x0B, 800, 0},
	{0x0C, 1600, 0},
};

static const struct bmi160_odr bmi160_gyro_odr[] = {
	{0x06, 25, 0},
	{0x07, 50, 0},
	{0x08, 100, 0},
	{0x09, 200, 0},
	{0x0A, 400, 0},
	{0x0B, 800, 0},
	{0x0C, 1600, 0},
	{0x0D, 3200, 0},
};

struct bmi160_odr_item {
	const struct bmi160_odr *tbl;
	int num;
};

static const struct  bmi160_odr_item bmi160_odr_table[] = {
	[BMI160_ACCEL] = {
		.tbl	= bmi160_accel_odr,
		.num	= ARRAY_SIZE(bmi160_accel_odr),
	},
	[BMI160_GYRO] = {
		.tbl	= bmi160_gyro_odr,
		.num	= ARRAY_SIZE(bmi160_gyro_odr),
	},
};

static const struct iio_chan_spec bmi160_channels[] = {
	BMI160_CHANNEL(IIO_ACCEL, X, BMI160_SCAN_ACCEL_X),
	BMI160_CHANNEL(IIO_ACCEL, Y, BMI160_SCAN_ACCEL_Y),
	BMI160_CHANNEL(IIO_ACCEL, Z, BMI160_SCAN_ACCEL_Z),
	BMI160_CHANNEL(IIO_ANGL_VEL, X, BMI160_SCAN_GYRO_X),
	BMI160_CHANNEL(IIO_ANGL_VEL, Y, BMI160_SCAN_GYRO_Y),
	BMI160_CHANNEL(IIO_ANGL_VEL, Z, BMI160_SCAN_GYRO_Z),
	IIO_CHAN_SOFT_TIMESTAMP(BMI160_SCAN_TIMESTAMP),
	/*
	 * The temperature sensor is somewhat linked to the gyro. If the gyro is
	 * in normal mode, the temperature is updated every 10ms. When the gyro
	 * is in suspended or fast power up mode, then the temperature is
	 * updated every 1.28s.
	 */
	{
		.type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
		                      BIT(IIO_CHAN_INFO_SCALE) |
		                      BIT(IIO_CHAN_INFO_OFFSET),
		.scan_index = BMI160_SCAN_TEMPERATURE,
		.scan_type = {
			.sign = 's',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
	},
};

static enum bmi160_sensor_type bmi160_to_sensor(enum iio_chan_type iio_type)
{
	switch (iio_type) {
	case IIO_ACCEL:
		return BMI160_ACCEL;
	case IIO_ANGL_VEL:
		return BMI160_GYRO;
	default:
		return -EINVAL;
	}
}

static
int bmi160_set_mode(struct bmi160_data *data, enum bmi160_sensor_type t,
		    enum bmi160_pmu_state mode)
{
	int ret;
	u8 cmd;

	if (mode < 0 || mode >= BMI160_PMU_STATE_COUNT)
		return -EINVAL;

	cmd = bmi160_regs[t].pmu_cmds[mode];
	if (cmd == 0)
		return -EINVAL;

	ret = regmap_write(data->regmap, BMI160_REG_CMD, cmd);
	if (ret < 0)
		return ret;

	usleep_range(bmi160_pmu_time[t], bmi160_pmu_time[t] + 1000);

	return 0;
}

static
int bmi160_set_scale(struct bmi160_data *data, enum bmi160_sensor_type t,
		     int uscale)
{
	int i;

	for (i = 0; i < bmi160_scale_table[t].num; i++)
		if (bmi160_scale_table[t].tbl[i].uscale == uscale)
			break;

	if (i == bmi160_scale_table[t].num)
		return -EINVAL;

	return regmap_write(data->regmap, bmi160_regs[t].range,
			    bmi160_scale_table[t].tbl[i].bits);
}

static
int bmi160_get_scale(struct bmi160_data *data, enum bmi160_sensor_type t,
		     int *uscale)
{
	int i, ret, val;

	ret = regmap_read(data->regmap, bmi160_regs[t].range, &val);
	if (ret < 0)
		return ret;

	for (i = 0; i < bmi160_scale_table[t].num; i++)
		if (bmi160_scale_table[t].tbl[i].bits == val) {
			*uscale = bmi160_scale_table[t].tbl[i].uscale;
			return 0;
		}

	return -EINVAL;
}

static int bmi160_get_data(struct bmi160_data *data, int chan_type,
			   int axis, int *val)
{
	u8 reg;
	int ret;
	__le16 sample;
	enum bmi160_sensor_type t = bmi160_to_sensor(chan_type);

	reg = bmi160_regs[t].data + (axis - IIO_MOD_X) * sizeof(sample);

	ret = regmap_bulk_read(data->regmap, reg, &sample, sizeof(sample));
	if (ret < 0)
		return ret;

	*val = sign_extend32(le16_to_cpu(sample), 15);

	return 0;
}

static
int bmi160_set_odr(struct bmi160_data *data, enum bmi160_sensor_type t,
		   int odr, int uodr)
{
	int i;

	for (i = 0; i < bmi160_odr_table[t].num; i++)
		if (bmi160_odr_table[t].tbl[i].odr == odr &&
		    bmi160_odr_table[t].tbl[i].uodr == uodr)
			break;

	if (i >= bmi160_odr_table[t].num)
		return -EINVAL;

	return regmap_update_bits(data->regmap,
				  bmi160_regs[t].config,
				  bmi160_regs[t].config_odr_mask,
				  bmi160_odr_table[t].tbl[i].bits);
}

static int bmi160_get_odr(struct bmi160_data *data, enum bmi160_sensor_type t,
			  int *odr, int *uodr)
{
	int i, val, ret;

	ret = regmap_read(data->regmap, bmi160_regs[t].config, &val);
	if (ret < 0)
		return ret;

	val &= bmi160_regs[t].config_odr_mask;

	for (i = 0; i < bmi160_odr_table[t].num; i++)
		if (val == bmi160_odr_table[t].tbl[i].bits)
			break;

	if (i >= bmi160_odr_table[t].num)
		return -EINVAL;

	*odr = bmi160_odr_table[t].tbl[i].odr;
	*uodr = bmi160_odr_table[t].tbl[i].uodr;

	return 0;
}

static irqreturn_t bmi160_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct bmi160_data *data = iio_priv(indio_dev);
	__le16 buf[16];
	/* 3 sens x 3 axis x __le16 + 3 x __le16 pad + 4 x __le16 tstamp */
	int i, ret, j = 0, base = BMI160_REG_DATA_MAGN_XOUT_L;
	__le16 sample;

	for_each_set_bit(i, indio_dev->active_scan_mask,
			 indio_dev->masklength) {
		ret = regmap_bulk_read(data->regmap, base + i * sizeof(sample),
				       &sample, sizeof(sample));
		if (ret < 0)
			goto done;
		buf[j++] = sample;
	}

	iio_push_to_buffers_with_timestamp(indio_dev, buf, iio_get_time_ns(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
						   indio_dev
#endif
						   ));
done:
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static int bmi160_read_temperature_reg(struct regmap *regmap, int *val)
{
	int ret;
	__le16 sample;
	const unsigned int reg = BMI160_REG_TEMPERATURE_0;
	ret = regmap_bulk_read(regmap, reg, &sample, sizeof(sample));
	if (ret < 0)
		return ret;
	*val = sign_extend32(le16_to_cpu(sample), 15);
	return IIO_VAL_INT;
}

static int bmi160_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	int ret;
	struct bmi160_data *data = iio_priv(indio_dev);

	switch (chan->type) {
	case IIO_ACCEL:
	case IIO_ANGL_VEL:
		switch (mask) {
		case IIO_CHAN_INFO_RAW:
			ret = bmi160_get_data(data, chan->type, chan->channel2, val);
			if (ret < 0)
				return ret;
			return IIO_VAL_INT;

		case IIO_CHAN_INFO_SCALE:
			*val = 0;
			ret = bmi160_get_scale(data,
					       bmi160_to_sensor(chan->type), val2);
			return ret < 0 ? ret : IIO_VAL_INT_PLUS_MICRO;

		case IIO_CHAN_INFO_SAMP_FREQ:
			ret = bmi160_get_odr(data, bmi160_to_sensor(chan->type),
					     val, val2);
			return ret < 0 ? ret : IIO_VAL_INT_PLUS_MICRO;

		default:
			return -EINVAL;
		}
		break;

	case IIO_TEMP:
		switch (mask) {
		case IIO_CHAN_INFO_RAW:
			return bmi160_read_temperature_reg(data->regmap, val);

		case IIO_CHAN_INFO_SCALE:
			/* 1000x multiplier to convert to milli-degrees celcius */
			*val = 1 * 1000;
			/*
			 * datasheet says 1/(2**9) degrees celcius per unit in
			 * the register
			 */
			*val2 = (1 << 9);
			return IIO_VAL_FRACTIONAL;

		case IIO_CHAN_INFO_OFFSET:
			/*
			 * 0x0000 in the register means 23 degrees celcius and
			 * (1 << 9) is how many register values make up one
			 * degree.
			 */
			*val = 23 * (1 << 9);
			return IIO_VAL_INT;

		default:
			return -EINVAL;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int bmi160_write_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int val, int val2, long mask)
{
	struct bmi160_data *data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		return bmi160_set_scale(data,
					bmi160_to_sensor(chan->type), val2);
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		return bmi160_set_odr(data, bmi160_to_sensor(chan->type),
				      val, val2);
	default:
		return -EINVAL;
	}

	return 0;
}

static int bmi160_setup_int(struct bmi160_data *data)
{
	int ret = 0;

	/* Route any motion/significant motion interrupt to int1 and int2 */
	ret = regmap_write(
		data->regmap,
		BMI160_REG_INT_MAP_0,
		BMI160_INT_MAP_0_INT1_ANYM_SIGMOT);
	if (ret < 0)
		return ret;

	ret = regmap_write(
		data->regmap,
		BMI160_REG_INT_MAP_1,
		0);
	if (ret < 0)
		return ret;

	ret = regmap_write(
		data->regmap,
		BMI160_REG_INT_MAP_2,
		BMI160_INT_MAP_2_INT2_ANYM_SIGMOT);
	if (ret < 0)
		return ret;

	if (data->interrupt_type == SIGMOT) {
		/*
		 * Select the significant motion interrupt instead of anymotion and set
		 * "skip" which is the minimum time between two anymotion events and
		 * "proof" which is the duration of motion required to trigger the
		 * significant motion event.
		 */
		ret = regmap_write(
			data->regmap,
			BMI160_REG_INT_MOTION_3,
			(FIELD_PREP(BMI160_INT_MOTION_3_SIG_MOT_SEL,
				BMI160_INT_MOTION_3_SIG_MOT_SEL_SIG) |
			FIELD_PREP(BMI160_INT_MOTION_3_SIG_MOT_SKIP,
				BMI160_SIG_MOT_SKIP_TIME_1500_MS) |
			FIELD_PREP(BMI160_INT_MOTION_3_SIG_MOT_PROOF,
				BMI160_SIG_MOT_PROOF_TIME_2000_MS)));
		if (ret < 0)
			return ret;
	} else {
		/*
		 * Select the any motion interrupt instead of significant motion.
		 */
		ret = regmap_write(
			data->regmap,
			BMI160_REG_INT_MOTION_3,
			FIELD_PREP(BMI160_INT_MOTION_3_SIG_MOT_SEL,
				BMI160_INT_MOTION_3_SIG_MOT_SEL_ANY));
		if (ret < 0)
			return ret;

		/*
		 * Select the number of points above threshold needed before triggering the
		 * interruption
		 */
		ret = regmap_write(
			data->regmap,
			BMI160_REG_INT_MOTION_0,
			FIELD_PREP(BMI160_INT_MOTION_0_ANYM_DUR,
				BMI160_INT_MOTION_0_ANYM_DUR_DEFAULT));
		if (ret < 0)
			return ret;

		/*
		 * Select the value of the threshold
		 */
		ret = regmap_write(
			data->regmap,
			BMI160_REG_INT_MOTION_1,
			FIELD_PREP(BMI160_INT_MOTION_1_ANYM_TH,
				data->anymot_th));
		if (ret < 0)
			return ret;
	}

	/*
	 * Enable a long latch period to easily catch the signal while polling.
	 */
	ret = regmap_write(
		data->regmap,
		BMI160_REG_INT_LATCH,
		(FIELD_PREP(BMI160_INT_LATCH_MODE,
			    BMI160_INT_LATCH_MODE_TMP_2560_MS)));
	if (ret < 0)
		return ret;

	/* Enable edge interrupt on INT1 and INT2 pin */
	ret = regmap_write(
		data->regmap,
		BMI160_REG_INT_OUT_CTRL,
		(BMI160_INT_OUT_CTRL_INT1_EDGE |
		 BMI160_INT_OUT_CTRL_INT1_LVL |
		 BMI160_INT_OUT_CTRL_INT1_OUTPUT_EN |
		 BMI160_INT_OUT_CTRL_INT2_EDGE |
		 BMI160_INT_OUT_CTRL_INT2_LVL |
		 BMI160_INT_OUT_CTRL_INT2_OUTPUT_EN));
	if (ret < 0)
		return ret;

	/* Enable anymotion/sigmotion interruptions on the 3 axis */
	ret = regmap_write(
		data->regmap,
		BMI160_REG_INT_EN_0,
		(BMI160_INT_EN_0_ANYM_X|
		 BMI160_INT_EN_0_ANYM_Y|
		 BMI160_INT_EN_0_ANYM_Z));

	return ret;
}

/*
 * Function called each time the user displays content of the in_accel_int file
 */
static ssize_t show_in_accel_int(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	// Get driver data
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi160_data *data = iio_priv(indio_dev);

	// Display selected interrupt number
	return scnprintf(buf,
					PAGE_SIZE,
					"%s\n",
					BMI160_INTERRUPT_TYPE_STR[data->interrupt_type]);
}

/*
 * Function called each time the user writes in the in_accel_int file
 */
static ssize_t store_in_accel_int(struct device *dev,
								struct device_attribute *attr,
								const char *buf,
								size_t count)
{
	// Get driver data
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi160_data *data = iio_priv(indio_dev);
	int ret, interrupt_type;
	char new[count];

	// Remove \n added in buffer
	strcpy(new, buf);
	new[strcspn(new, "\n")] = 0;

	// Compute corresponding interrupt type
	if (strcmp(new, BMI160_INTERRUPT_TYPE_STR[SIGMOT]) == 0)
		interrupt_type = SIGMOT;
	else if (strcmp(new, BMI160_INTERRUPT_TYPE_STR[ANYMOT]) == 0)
		interrupt_type = ANYMOT;
	else
		return -EINVAL;

	// Update interrupt configuration if necessary
	if (interrupt_type != data->interrupt_type){
		data->interrupt_type = interrupt_type;
		ret = bmi160_setup_int(data);
		if (ret < 0)
			return ret;
	}
	return count;
}

/*
 * Function called each time the user displays content of the in_accel_int_anymot_th file.
 */
static ssize_t show_in_accel_int_anymot_th(struct device *dev,
										struct device_attribute *attr,
										char *buf)
{
	// Get driver data
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi160_data *data = iio_priv(indio_dev);

	// Display selected threshold value
	return scnprintf(buf, PAGE_SIZE, "%d\n", data->anymot_th);
}

/*
 * Function called each time the user writes in the in_accel_int_anymot_th file.
 * If the user try to write in in_accel_int_anymot_th when the Significant
 * motion interrupt is selected, the function does nothing and doesn't
 * update driver data.
 */
static ssize_t store_in_accel_int_anymot_th(struct device *dev,
											struct device_attribute *attr,
											const char *buf,
											size_t count)
{
	// Get driver data
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi160_data *data = iio_priv(indio_dev);
	int ret;
	u8 new;

	// Convert str user input into int value
	ret = kstrtou8(buf, 10, &new);
	if (ret < 0)
		return ret;

	// Select the value of the threshold
	ret = regmap_write(
		data->regmap,
		BMI160_REG_INT_MOTION_1,
		new);
	if (ret < 0)
		return ret;

	// Update threshold value
	data->anymot_th = new;

	return count;
}

static
IIO_CONST_ATTR(in_accel_sampling_frequency_available,
	       "0.78125 1.5625 3.125 6.25 12.5 25 50 100 200 400 800 1600");
static
IIO_CONST_ATTR(in_anglvel_sampling_frequency_available,
	       "25 50 100 200 400 800 1600 3200");
static
IIO_CONST_ATTR(in_accel_scale_available,
	       "0.000598 0.001197 0.002394 0.004788");
static
IIO_CONST_ATTR(in_anglvel_scale_available,
	       "0.001065 0.000532 0.000266 0.000133 0.000066");
static IIO_CONST_ATTR(in_accel_int_available, "significant_motion any_motion");
static IIO_CONST_ATTR(in_accel_int_anymot_th_min, "0");
static IIO_CONST_ATTR(in_accel_int_anymot_th_max, "255");
static IIO_DEVICE_ATTR(in_accel_int, (S_IRUGO | S_IWUSR), show_in_accel_int,
						store_in_accel_int, 0);
static IIO_DEVICE_ATTR(in_accel_int_anymot_th, (S_IRUGO | S_IWUSR), show_in_accel_int_anymot_th,
						store_in_accel_int_anymot_th, 0);

static struct attribute *bmi160_attrs[] = {
	&iio_const_attr_in_accel_sampling_frequency_available.dev_attr.attr,
	&iio_const_attr_in_anglvel_sampling_frequency_available.dev_attr.attr,
	&iio_const_attr_in_accel_scale_available.dev_attr.attr,
	&iio_const_attr_in_anglvel_scale_available.dev_attr.attr,
	&iio_const_attr_in_accel_int_available.dev_attr.attr,
	&iio_const_attr_in_accel_int_anymot_th_min.dev_attr.attr,
	&iio_const_attr_in_accel_int_anymot_th_max.dev_attr.attr,
	&iio_dev_attr_in_accel_int.dev_attr.attr,
	&iio_dev_attr_in_accel_int_anymot_th.dev_attr.attr,
	NULL,
};

static const struct attribute_group bmi160_attrs_group = {
	.attrs = bmi160_attrs,
};

static const struct iio_info bmi160_info = {
	.read_raw = bmi160_read_raw,
	.write_raw = bmi160_write_raw,
	.attrs = &bmi160_attrs_group,
};

static const char *bmi160_match_acpi_device(struct device *dev)
{
	const struct acpi_device_id *id;

	id = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!id)
		return NULL;

	return dev_name(dev);
}

static int bmi160_chip_init(struct bmi160_data *data, bool use_spi)
{
	int ret;
	unsigned int val;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	struct device *dev = regmap_get_device(data->regmap);
#else
	struct device *dev = data->dev;
#endif

	ret = regmap_write(data->regmap, BMI160_REG_CMD, BMI160_CMD_SOFTRESET);
	if (ret < 0)
		return ret;

	usleep_range(BMI160_SOFTRESET_USLEEP, BMI160_SOFTRESET_USLEEP + 1);

	/*
	 * CS rising edge is needed before starting SPI, so do a dummy read
	 * See Section 3.2.1, page 86 of the datasheet
	 */
	if (use_spi) {
		ret = regmap_read(data->regmap, BMI160_REG_DUMMY, &val);
		if (ret < 0)
			return ret;
	}

	ret = regmap_read(data->regmap, BMI160_REG_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(dev, "Error reading chip id\n");
		return ret;
	}
	if (val != BMI160_CHIP_ID_VAL) {
		dev_err(dev, "Wrong chip id, got %x expected %x\n",
			val, BMI160_CHIP_ID_VAL);
		return -ENODEV;
	}

	ret = bmi160_set_mode(data, BMI160_ACCEL, BMI160_PMU_STATE_NORMAL);
	if (ret < 0)
		return ret;

	ret = bmi160_set_mode(data, BMI160_GYRO, BMI160_PMU_STATE_NORMAL);
	if (ret < 0)
		return ret;

	ret = bmi160_setup_int(data);
	if (ret < 0)
		return ret;

	return 0;
}

static void bmi160_chip_uninit(struct bmi160_data *data)
{
	/*
	 * Putting the gyro in suspend and the accelerometer in lower power mode
	 * still allows the significant motion interrupt to fire while linux is
	 * powered down.
	 */
	bmi160_set_mode(data, BMI160_GYRO, BMI160_PMU_STATE_SUSPEND);
	bmi160_set_mode(data, BMI160_ACCEL, BMI160_PMU_STATE_LOW_POWER);
}

int bmi160_core_probe(struct device *dev, struct regmap *regmap,
		      const char *name, bool use_spi)
{
	struct iio_dev *indio_dev;
	struct bmi160_data *data;
	int ret;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	dev_set_drvdata(dev, indio_dev);
	data->regmap = regmap;
	data->interrupt_type = SIGMOT;
	data->anymot_th = BMI160_INT_MOTION_1_ANYM_TH_DEFAULT;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0)
	data->dev = dev;
#endif
	ret = bmi160_chip_init(data, use_spi);
	if (ret < 0)
		return ret;

	if (!name && ACPI_HANDLE(dev))
		name = bmi160_match_acpi_device(dev);

	indio_dev->dev.parent = dev;
	indio_dev->channels = bmi160_channels;
	indio_dev->num_channels = ARRAY_SIZE(bmi160_channels);
	indio_dev->name = name;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &bmi160_info;

	ret = iio_triggered_buffer_setup(indio_dev, NULL,
					 bmi160_trigger_handler, NULL);
	if (ret < 0)
		goto uninit;

	ret = iio_device_register(indio_dev);
	if (ret < 0)
		goto buffer_cleanup;

	return 0;
buffer_cleanup:
	iio_triggered_buffer_cleanup(indio_dev);
uninit:
	bmi160_chip_uninit(data);
	return ret;
}
EXPORT_SYMBOL_GPL(bmi160_core_probe);

void bmi160_core_remove(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi160_data *data = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);
	bmi160_chip_uninit(data);
}
EXPORT_SYMBOL_GPL(bmi160_core_remove);

MODULE_AUTHOR("Daniel Baluta <daniel.baluta@intel.com");
MODULE_DESCRIPTION("Bosch BMI160 driver");
MODULE_LICENSE("GPL v2");
