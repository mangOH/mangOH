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
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/buffer.h>
#include <linux/iio/sysfs.h>
#include <linux/version.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
#include <linux/bitfield.h>
#else
#define __bf_shf(x) (__builtin_ffsll(x) - 1)
#define FIELD_PREP(_mask, _val)						\
	({								\
		((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask);	\
	})
#define FIELD_GET(_mask, _reg)						\
	({								\
		(typeof(_mask))(((_reg) & (_mask)) >> __bf_shf(_mask));	\
	})
#endif

#include "bmi160.h"
#include "bmi160_pdata.h"

#define BMI160_REG_CHIP_ID			0x00
#define BMI160_CHIP_ID_VAL			0xD1

#define BMI160_REG_PMU_STATUS			0x03

/* X axis data low byte address, the rest can be obtained using axis offset */
#define BMI160_REG_DATA_MAGN_XOUT_L		0x04
#define BMI160_REG_DATA_GYRO_XOUT_L		0x0C
#define BMI160_REG_DATA_ACCEL_XOUT_L		0x12
#define BMI160_REG_SENSOR_TIME_0		0x18
#define BMI160_REG_SENSOR_TIME_1		0x19
#define BMI160_REG_SENSOR_TIME_2		0x1A

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
#define BMI160_REG_FIFO_LENGTH_0		0x22
#define BMI160_REG_FIFO_LENGTH_1		0x23
#define BMI160_REG_FIFO_DATA			0x24

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

#define BMI160_REG_FIFO_CONFIG_0		0x46
#define BMI160_FIFO_CONFIG_0_WATER_MARK		GENMASK(7, 0)

#define BMI160_REG_FIFO_CONFIG_1		0x47
#define BMI160_FIFO_CONFIG_1_TIME_EN		BIT(1)
#define BMI160_FIFO_CONFIG_1_TAG_INT2_EN	BIT(2)
#define BMI160_FIFO_CONFIG_1_TAG_INT1_EN	BIT(3)
#define BMI160_FIFO_CONFIG_1_HEADER_EN		BIT(4)
#define BMI160_FIFO_CONFIG_1_MAG_EN		BIT(5)
#define BMI160_FIFO_CONFIG_1_ACC_EN		BIT(6)
#define BMI160_FIFO_CONFIG_1_GYR_EN		BIT(7)

#define BMI160_REG_INT_EN_0			0x50
#define BMI160_INT_EN_0_ANYM_X			BIT(0)
#define BMI160_INT_EN_0_ANYM_Y			BIT(1)
#define BMI160_INT_EN_0_ANYM_Z			BIT(2)
#define BMI160_INT_EN_0_DTAP			BIT(4)
#define BMI160_INT_EN_0_STAP			BIT(5)
#define BMI160_INT_EN_0_ORIENT			BIT(6)
#define BMI160_INT_EN_0_FLAT			BIT(7)

#define BMI160_REG_INT_EN_1			0x51
#define BMI160_INT_EN_1_HIGHG_X			BIT(0)
#define BMI160_INT_EN_1_HIGHG_Y			BIT(1)
#define BMI160_INT_EN_1_HIGHG_Z			BIT(2)
#define BMI160_INT_EN_1_LOWG			BIT(3)
#define BMI160_INT_EN_1_DRDY			BIT(4)
#define BMI160_INT_EN_1_FFULL			BIT(5)
#define BMI160_INT_EN_1_FWM			BIT(6)

#define BMI160_REG_INT_EN_2			0x52
#define BMI160_INT_EN_2_NOMO_X			BIT(0)
#define BMI160_INT_EN_2_NOMO_Y			BIT(1)
#define BMI160_INT_EN_2_NOMO_Z			BIT(2)
#define BMI160_INT_EN_2_STEP_DET		BIT(3)

#define BMI160_REG_INT_OUT_CTRL			0x53
#define BMI160_INT_OUT_CTRL_INT1_TRIG		BIT(0)
#define BMI160_INT_OUT_CTRL_INT1_LVL		BIT(1)
#define BMI160_INT_OUT_CTRL_INT1_OD		BIT(2)
#define BMI160_INT_OUT_CTRL_INT1_OUTPUT_EN	BIT(3)
#define BMI160_INT_OUT_CTRL_INT2_TRIG		BIT(4)
#define BMI160_INT_OUT_CTRL_INT2_LVL		BIT(5)
#define BMI160_INT_OUT_CTRL_INT2_OD		BIT(6)
#define BMI160_INT_OUT_CTRL_INT2_OUTPUT_EN	BIT(7)

/*
 * Relevant to BMI160_INT_OUT_CTRL_INT2_TRIG and BMI160_INT_OUT_CTRL_INT1_TRIG
 */
#define BMI160_INT_TRIGGER_EDGE			0
#define BMI160_INT_TRIGGER_LEVEL		1

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
#define BMI160_INT_MOTION_0_SLO_NO_MOT_DUR	GENMASK(7, 2)

#define BMI160_REG_INT_MOTION_1			0x60
#define BMI160_INT_MOTION_1_ANYM_TH		GENMASK(7, 0)
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

#define BMI160_FIFO_HDR_MODE			GENMASK(7, 6)
#define   BMI160_FIFO_MODE_CONTROL		0x1
#define   BMI160_FIFO_MODE_REGULAR		0x2
#define BMI160_FIFO_HDR_PARM			GENMASK(5, 2)
#define   BMI160_FIFO_PARM_CONTROL_SKIP		0x0
#define   BMI160_FIFO_PARM_CONTROL_SENSORTIME	0x1
#define   BMI160_FIFO_PARM_CONTROL_INPUT_CONFIG	0x2
#define BMI160_FIFO_HDR_EXT			GENMASK(5, 2)

#define BMI160_INPUT_CONFIG_ACC_CONF		BIT(0)
#define BMI160_INPUT_CONFIG_ACC_RANGE		BIT(1)
#define BMI160_INPUT_CONFIG_GYR_CONF		BIT(2)
#define BMI160_INPUT_CONFIG_GYR_RANGE		BIT(3)

#define BMI160_ACCEL_PMU_MIN_USLEEP		3800
#define BMI160_GYRO_PMU_MIN_USLEEP		80000
#define BMI160_SOFTRESET_USLEEP			1000

#define BMI160_FIFO_SIZE			1024
#define BMI160_SENSORTIME_TICK_NS		(39 * NSEC_PER_USEC)

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

struct bmi160_data {
	struct mutex mutex;
	struct regmap *regmap;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0)
	/*
	 * This member is necessary in kernel versions < 3.16 because
	 * regmap_get_device() is not defined.
	 */
	struct device *dev;
#endif
	int int1_irq;
	int int2_irq;
	struct iio_trigger *drdy_trigger;
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
		.scan_index = -1,
		.scan_type = {
			.sign = 's',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
	},
	IIO_CHAN_SOFT_TIMESTAMP(BMI160_SCAN_TIMESTAMP),
};


static int bmi160_buffer_preenable(struct iio_dev *indio_dev)
{
	int ret;
	struct bmi160_data *data = iio_priv(indio_dev);
	struct device *dev = &indio_dev->dev;
	const bool gyroRequired =
		test_bit(BMI160_SCAN_GYRO_X, indio_dev->active_scan_mask) ||
		test_bit(BMI160_SCAN_GYRO_Y, indio_dev->active_scan_mask) ||
		test_bit(BMI160_SCAN_GYRO_Z, indio_dev->active_scan_mask);
	const bool accelRequired =
		test_bit(BMI160_SCAN_ACCEL_X, indio_dev->active_scan_mask) ||
		test_bit(BMI160_SCAN_ACCEL_Y, indio_dev->active_scan_mask) ||
		test_bit(BMI160_SCAN_ACCEL_Z, indio_dev->active_scan_mask);

	mutex_lock(&data->mutex);
	/* Flush the FIFO */
	ret = regmap_write(data->regmap, BMI160_REG_CMD, BMI160_CMD_FIFO_FLUSH);
	if (ret != 0) {
		dev_err(dev,
			"Couldn't write BMI160_REG_CMD with BMI160_CMD_FIFO_FLUSH");
		goto unlock;
	}

	ret = regmap_update_bits(data->regmap, BMI160_REG_FIFO_CONFIG_1,
				 (BMI160_FIFO_CONFIG_1_GYR_EN |
				  BMI160_FIFO_CONFIG_1_ACC_EN |
				  BMI160_FIFO_CONFIG_1_MAG_EN |
				  BMI160_FIFO_CONFIG_1_HEADER_EN |
				  BMI160_FIFO_CONFIG_1_TAG_INT1_EN |
				  BMI160_FIFO_CONFIG_1_TAG_INT2_EN |
				  BMI160_FIFO_CONFIG_1_TIME_EN),
				 ((gyroRequired ? BMI160_FIFO_CONFIG_1_GYR_EN : 0) |
				  (accelRequired ? BMI160_FIFO_CONFIG_1_ACC_EN : 0) |
				  BMI160_FIFO_CONFIG_1_HEADER_EN |
				  BMI160_FIFO_CONFIG_1_TIME_EN));
	if (ret != 0) {
		dev_err(dev, "Couldn't enable FIFO fields");
		goto unlock;
	}

unlock:
	mutex_unlock(&data->mutex);
	return ret;
}

static int bmi160_buffer_postdisable(struct iio_dev *indio_dev)
{
	struct bmi160_data *data = iio_priv(indio_dev);
	struct device *dev = &indio_dev->dev;
	int ret;

	mutex_lock(&data->mutex);

	/* disable all channels for the FIFO */
	ret = regmap_update_bits(data->regmap, BMI160_REG_FIFO_CONFIG_1,
				 (BMI160_FIFO_CONFIG_1_GYR_EN |
				  BMI160_FIFO_CONFIG_1_ACC_EN |
				  BMI160_FIFO_CONFIG_1_MAG_EN |
				  BMI160_FIFO_CONFIG_1_HEADER_EN |
				  BMI160_FIFO_CONFIG_1_TAG_INT1_EN |
				  BMI160_FIFO_CONFIG_1_TAG_INT2_EN |
				  BMI160_FIFO_CONFIG_1_TIME_EN), 0);
	if (ret != 0) {
		dev_err(dev, "Couldn't disable FIFO fields");
		goto unlock;
	}

unlock:
	mutex_unlock(&data->mutex);
	return ret;
}

static bool bmi160_buffer_validate_scan_mask(struct iio_dev *indio_dev,
					     const unsigned long *scan_mask)
{
	struct device *dev = &indio_dev->dev;
	const unsigned long allowed = (BIT(BMI160_SCAN_GYRO_X) |
				       BIT(BMI160_SCAN_GYRO_Y) |
				       BIT(BMI160_SCAN_GYRO_Z) |
				       BIT(BMI160_SCAN_ACCEL_X) |
				       BIT(BMI160_SCAN_ACCEL_Y) |
				       BIT(BMI160_SCAN_ACCEL_Z));
	const unsigned long remaining = ((*scan_mask) & (~allowed));
	if (remaining) {
		dev_warn(dev,
			 "Scan mask (0x%08lX) is invalid. Invalid bits are: 0x%08lX\n",
			 *scan_mask, remaining);
		return false;
	}
	return true;
}

static const struct iio_buffer_setup_ops bmi160_iio_buffer_setup_ops = {
	.preenable	    = &bmi160_buffer_preenable,
	.postenable	    = &iio_triggered_buffer_postenable,
	.predisable	    = &iio_triggered_buffer_predisable,
	.postdisable	    = &bmi160_buffer_postdisable,
	.validate_scan_mask = &bmi160_buffer_validate_scan_mask,
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

	mutex_lock(&data->mutex);
	ret = regmap_bulk_read(data->regmap, reg, &sample, sizeof(sample));
	if (ret < 0)
		goto unlock;
	else
		ret = 0;

	*val = sign_extend32(le16_to_cpu(sample), 15);

unlock:
	mutex_unlock(&data->mutex);
	return ret;
}

static
int bmi160_set_odr(struct bmi160_data *data, enum bmi160_sensor_type t,
		   int odr, int uodr)
{
	int i;
	int ret;

	for (i = 0; i < bmi160_odr_table[t].num; i++)
		if (bmi160_odr_table[t].tbl[i].odr == odr &&
		    bmi160_odr_table[t].tbl[i].uodr == uodr)
			break;

	if (i >= bmi160_odr_table[t].num)
		return -EINVAL;

	mutex_lock(&data->mutex);
	ret = regmap_update_bits(data->regmap,
				 bmi160_regs[t].config,
				 bmi160_regs[t].config_odr_mask,
				 bmi160_odr_table[t].tbl[i].bits);
	mutex_unlock(&data->mutex);
	return ret;
}

static int bmi160_get_odr(struct bmi160_data *data, enum bmi160_sensor_type t,
			  int *odr, int *uodr)
{
	int i, val, ret;

	mutex_lock(&data->mutex);
	ret = regmap_read(data->regmap, bmi160_regs[t].config, &val);
	mutex_unlock(&data->mutex);
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
	struct device *dev = &indio_dev->dev;
	u16 fifo_length;
	int ret;
	u16 buff_offset = 0;
	u16 read_bytes;
	u8 *read_buffer = NULL;
	const bool mask_includes_mag =
		test_bit(BMI160_SCAN_EXT_MAGN_X, indio_dev->active_scan_mask) ||
		test_bit(BMI160_SCAN_EXT_MAGN_Y, indio_dev->active_scan_mask) ||
		test_bit(BMI160_SCAN_EXT_MAGN_Z, indio_dev->active_scan_mask) ||
		test_bit(BMI160_SCAN_RHALL, indio_dev->active_scan_mask);
	const bool mask_includes_gyr =
		test_bit(BMI160_SCAN_GYRO_X, indio_dev->active_scan_mask) ||
		test_bit(BMI160_SCAN_GYRO_Y, indio_dev->active_scan_mask) ||
		test_bit(BMI160_SCAN_GYRO_Z, indio_dev->active_scan_mask);
	const bool mask_includes_acc =
		test_bit(BMI160_SCAN_ACCEL_X, indio_dev->active_scan_mask) ||
		test_bit(BMI160_SCAN_ACCEL_Y, indio_dev->active_scan_mask) ||
		test_bit(BMI160_SCAN_ACCEL_Z, indio_dev->active_scan_mask);
	const u8 data_frame_payload_len = ((mask_includes_mag ? (4 * 2) : 0) +
					   (mask_includes_gyr ? (3 * 2) : 0) +
					   (mask_includes_acc ? (3 * 2) : 0));
	size_t max_data_frames;
	u8 *samples = NULL;
	size_t num_samples = 0;
	u8 length_buf[2];

	mutex_lock(&data->mutex);

	ret = regmap_bulk_read(data->regmap, BMI160_REG_FIFO_LENGTH_0,
			       length_buf, 2);
	if (ret != 0) {
		dev_err(dev, "Failed to read FIFO length");
		goto done;
	}
	fifo_length = (length_buf[0] | ((length_buf[1] & 0x07) << 8));
	dev_dbg(dev, "Processing FIFO when length=%u, scan_bytes=%d\n",
		 fifo_length, indio_dev->scan_bytes);

	/*
	 * Read an extra 2 data frames of data to try to ensure that we get a
	 * timestamp.
	 */
	read_bytes = fifo_length + (2 * (data_frame_payload_len + 1));
	read_buffer = kmalloc(read_bytes, GFP_KERNEL);
	if (!read_buffer) {
		goto done;
	}
	ret = regmap_bulk_read(data->regmap, BMI160_REG_FIFO_DATA, read_buffer,
			       read_bytes);
	if (ret != 0) {
		dev_err(dev, "Failed to read %d bytes from the FIFO",
			read_bytes);
		goto done;
	}

	max_data_frames = read_bytes / (1 + data_frame_payload_len); 
	samples = kmalloc(max_data_frames * indio_dev->scan_bytes, GFP_KERNEL);
	if (!samples) {
		goto done;
	}

	while (buff_offset < read_bytes) {
		const u8 hdr_mode = FIELD_GET(BMI160_FIFO_HDR_MODE,
					      read_buffer[buff_offset]);
		const u8 hdr_parm = FIELD_GET(BMI160_FIFO_HDR_PARM,
					      read_buffer[buff_offset]);
		const u8 hdr_ext = FIELD_GET(BMI160_FIFO_HDR_EXT,
					     read_buffer[buff_offset]);
		/*
		dev_info(dev, "Processing frame: mode=%d, parm=%d, ext=%d\n",
			 hdr_mode, hdr_parm, hdr_ext);
		*/
		buff_offset++;

		if (hdr_mode == BMI160_FIFO_MODE_CONTROL) {
			if (hdr_parm == BMI160_FIFO_PARM_CONTROL_SKIP) {
				dev_warn(dev, "Dropped frames (%d)\n", hdr_ext);
			} else if (hdr_parm == BMI160_FIFO_PARM_CONTROL_SENSORTIME) {
				u32 now_hw;
				s64 now;
				u32 last_sample_hw;
				s64 last_sample;
				int i;
				/* TODO: make dynamic based on ODR. See datasheet table 11 */
				s64 sample_period_ns = 10 * NSEC_PER_MSEC;
				if (buff_offset + 3 > read_bytes) {
					dev_err(dev,
						"Can't process partial sensortime frame\n");
					goto done;
				}
				last_sample_hw = ((read_buffer[buff_offset] << 0) |
						  (read_buffer[buff_offset + 1] << 8) |
						  (read_buffer[buff_offset + 2] << 16));
				buff_offset += 3;

				/*
				 * We are done with read_buffer, so re-use it to
				 * get the current hardware timestamp
				 */
				ret = regmap_bulk_read(data->regmap,
						       BMI160_REG_SENSOR_TIME_0,
						       read_buffer, 3);
				if (ret != 0)
					goto done;
				now_hw = ((read_buffer[0] << 0) |
					  (read_buffer[1] << 8) |
					  (read_buffer[2] << 16));
				now = iio_get_time_ns(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
					indio_dev
#endif
					);
				if (now_hw >= last_sample_hw) {
					last_sample = now - (
						(now_hw - last_sample_hw) *
						BMI160_SENSORTIME_TICK_NS);
				} else {
					last_sample = now - (
						(now_hw +
						 (GENMASK(23, 0) - last_sample_hw)) *
						BMI160_SENSORTIME_TICK_NS);
				}

				for (i = 0; i < num_samples; i++) {
					u8 *sample = &samples[i * indio_dev->scan_bytes];
					s64 t = last_sample - (num_samples - (i - 1)) * sample_period_ns;
					ret = iio_push_to_buffers_with_timestamp(indio_dev, sample, t);
					if (ret != 0) {
						dev_err(dev, "Couldn't push data to buffer");
						goto done;
					}
				}
				break;
			} else if (hdr_parm == BMI160_FIFO_PARM_CONTROL_INPUT_CONFIG) {
				dev_info(dev,
					 "Config changed: ACC_CONF=%ld, ACC_RANGE=%ld, GYR_CONF=%ld, GYR_RANGE=%ld\n",
					 FIELD_GET(BMI160_INPUT_CONFIG_ACC_CONF, read_buffer[buff_offset]),
					 FIELD_GET(BMI160_INPUT_CONFIG_ACC_RANGE, read_buffer[buff_offset]),
					 FIELD_GET(BMI160_INPUT_CONFIG_GYR_CONF, read_buffer[buff_offset]),
					 FIELD_GET(BMI160_INPUT_CONFIG_GYR_CONF, read_buffer[buff_offset]));
				buff_offset += 1;
			} else {
				dev_err(dev,
					"Unexpected FIFO header mode=%d\n, parm=%d",
					hdr_mode, hdr_parm);
				goto done;
			}
		} else if (hdr_mode == BMI160_FIFO_MODE_REGULAR) {
			const bool includes_mag =
				(FIELD_GET(BIT(2), hdr_parm) != 0);
			const bool includes_gyr =
				(FIELD_GET(BIT(1), hdr_parm) != 0);
			const bool includes_acc =
				(FIELD_GET(BIT(0), hdr_parm) != 0);
			u8 *sample = &samples[num_samples * indio_dev->scan_bytes];
			u8 sample_offset = 0;

			if (includes_mag != mask_includes_mag ||
			    includes_gyr != mask_includes_gyr ||
			    includes_acc != mask_includes_acc) {
				dev_err(dev,
					"Data frame header (%d/%d/%d) inconsistent with channel mask (%d/%d/%d)\n",
					includes_mag, includes_gyr,
					includes_acc, mask_includes_mag,
					mask_includes_gyr, mask_includes_acc);
				goto done;
			}

			if (buff_offset + data_frame_payload_len > read_bytes) {
				dev_err(dev,
					"Payload extends past read buffer. This should never happen.\n");
				goto done;
			}

			if (includes_mag) {
				/*
				 * Magnetometer isn't currently supported, so
				 * just skip over the data for now if it is
				 * present, but there should be no reason for it
				 * to be present.
				 */
				dev_warn(dev,
					 "Magnetometer data in FIFO, but magnetometer not yet supported\n");
				buff_offset += (4 * 2);
			}
			if (includes_gyr) {
				if (test_bit(BMI160_SCAN_GYRO_X,
					     indio_dev->active_scan_mask)) {
					sample[sample_offset++] =
						read_buffer[buff_offset + 0];
					sample[sample_offset++] =
						read_buffer[buff_offset + 1];
				}
				if (test_bit(BMI160_SCAN_GYRO_Y,
					     indio_dev->active_scan_mask)) {
					sample[sample_offset++] =
						read_buffer[buff_offset + 2];
					sample[sample_offset++] =
						read_buffer[buff_offset + 3];
				}
				if (test_bit(BMI160_SCAN_GYRO_Z,
					     indio_dev->active_scan_mask)) {
					sample[sample_offset++] =
						read_buffer[buff_offset + 4];
					sample[sample_offset++] =
						read_buffer[buff_offset + 5];
				}
				buff_offset += (3 * 2);
			}
			if (includes_acc) {
				if (test_bit(BMI160_SCAN_ACCEL_X,
					     indio_dev->active_scan_mask)) {
					sample[sample_offset++] =
						read_buffer[buff_offset + 0];
					sample[sample_offset++] =
						read_buffer[buff_offset + 1];
				}
				if (test_bit(BMI160_SCAN_ACCEL_Y,
					     indio_dev->active_scan_mask)) {
					sample[sample_offset++] =
						read_buffer[buff_offset + 2];
					sample[sample_offset++] =
						read_buffer[buff_offset + 3];
				}
				if (test_bit(BMI160_SCAN_ACCEL_Z,
					     indio_dev->active_scan_mask)) {
					sample[sample_offset++] =
						read_buffer[buff_offset + 4];
					sample[sample_offset++] =
						read_buffer[buff_offset + 5];
				}
				buff_offset += (3 * 2);
			}
			num_samples++;
		} else {
			dev_err(dev, "Unexpected FIFO header mode=%d\n",
				hdr_mode);
			goto done;
		}
	}

done:
	kfree(samples);
	kfree(read_buffer);
	iio_trigger_notify_done(indio_dev->trig);
	mutex_unlock(&data->mutex);
	return IRQ_HANDLED;
}

static int bmi160_read_temperature_reg(struct bmi160_data *data, int *val)
{
	int ret;
	__le16 sample;
	const unsigned int reg = BMI160_REG_TEMPERATURE_0;

	mutex_lock(&data->mutex);
	ret = regmap_bulk_read(data->regmap, reg, &sample, sizeof(sample));
	if (ret < 0)
		goto unlock;
	*val = sign_extend32(le16_to_cpu(sample), 15);

unlock:
	mutex_unlock(&data->mutex);
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
			return bmi160_read_temperature_reg(data, val);

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

static irqreturn_t bmi160_sigmot_interrupt_thread(int irq, void *p)
{
	struct bmi160_data *data = p;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	struct device *dev = regmap_get_device(data->regmap);
#else
	struct device *dev = data->dev;
#endif
	dev_info(dev, "bmi160_sigmot_interrupt_thread");

	return IRQ_HANDLED;
}

int bmi160_drdy_trigger_set_state(struct iio_trigger *trig, bool state)
{
	int ret;
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct bmi160_data *data = iio_priv(indio_dev);

	ret = regmap_update_bits(data->regmap, BMI160_REG_INT_EN_1,
				 BMI160_INT_EN_1_DRDY,
				 FIELD_PREP(BMI160_INT_EN_1_DRDY,
					    state ? 1 : 0));
	if (ret != 0) {
		dev_err(&indio_dev->dev,
			"Failed to %s data ready interrupt",
			state ? "enable" : "disable");
		return ret;
	}

	return ret;
}

static const struct iio_trigger_ops bmi160_drdy_trig_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = bmi160_drdy_trigger_set_state,
};

static irqreturn_t bmi160_drdy_irq_handler(int irq, void *private)
{
	iio_trigger_poll_chained(private
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0)
				 , iio_get_time_ns()
#endif
		);
	return IRQ_HANDLED;
}

static int bmi160_register_drdy_trigger(struct bmi160_data *data)
{
	int ret;
	struct iio_dev *indio_dev = iio_priv_to_dev(data);
	struct device *dev = &indio_dev->dev;
	struct iio_trigger *trig;
	if (data->int1_irq <= 0) {
		dev_warn(dev,
			 "Couldn't create data ready trigger because int1 IRQ was not specified");
		return -ENODEV;
	}

	trig = devm_iio_trigger_alloc(
		dev, "%s-dev%d", indio_dev->name, indio_dev->id);
	if (!trig)
		return -ENOMEM;

	data->drdy_trigger = trig;

	ret = devm_request_threaded_irq(dev, data->int1_irq, NULL,
					bmi160_drdy_irq_handler,
					IRQF_TRIGGER_RISING, "bmi160_drdy",
					trig);
	if (ret != 0) {
		dev_err(dev,
			"Couldn't request irq %d for data ready interrupt",
			data->int1_irq);
		goto err_free_trig;
	}

	trig->dev.parent = dev->parent;
	trig->ops = &bmi160_drdy_trig_ops;
	iio_trigger_set_drvdata(trig, indio_dev);

	indio_dev->trig = iio_trigger_get(trig);

	ret = iio_trigger_register(trig);
	if (ret != 0)
	{
		dev_err(dev, "failed to register data ready trigger");
		goto err_free_trig;
	}

	return ret;

err_free_trig:
	devm_iio_trigger_free(dev, trig);
	return ret;
}

/*
 * The bmi160 has two interrupt pins. We use int1 for the data ready
 * interrupt and int2 for the significant interrupt.
 */
static int bmi160_setup_interrupts(struct bmi160_data *data)
{
	int ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	struct device *dev = regmap_get_device(data->regmap);
#else
	struct device *dev = data->dev;
#endif

	/* Configure both interrupt pins of the BMI160 as outputs */
	ret = regmap_write(data->regmap, BMI160_REG_INT_OUT_CTRL,
			   FIELD_PREP(BMI160_INT_OUT_CTRL_INT2_OUTPUT_EN, 1) |
			   FIELD_PREP(BMI160_INT_OUT_CTRL_INT2_OD, 0) |
			   FIELD_PREP(BMI160_INT_OUT_CTRL_INT2_LVL, 1) |
			   FIELD_PREP(BMI160_INT_OUT_CTRL_INT2_TRIG,
				      BMI160_INT_TRIGGER_EDGE) |
			   FIELD_PREP(BMI160_INT_OUT_CTRL_INT1_OUTPUT_EN, 1) |
			   FIELD_PREP(BMI160_INT_OUT_CTRL_INT1_OD, 0) |
			   FIELD_PREP(BMI160_INT_OUT_CTRL_INT1_LVL, 1) |
			   FIELD_PREP(BMI160_INT_OUT_CTRL_INT1_TRIG,
				      BMI160_INT_TRIGGER_EDGE));
	if (ret != 0) {
		dev_err(dev, "Couldn't write BMI160_REG_INT_OUT_CTRL");
		return ret;
	}

	/*
	 * Disable input mode on both INT1 and INT2 and set interrupts to
	 * non-latched mode.
	 */
	regmap_update_bits(data->regmap, BMI160_REG_INT_LATCH,
			   (BMI160_INT_LATCH_INT2_INPUT_EN |
			    BMI160_INT_LATCH_INT1_INPUT_EN |
			    BMI160_INT_LATCH_MODE),
			   (FIELD_PREP(BMI160_INT_LATCH_INT2_INPUT_EN, 0) |
			    FIELD_PREP(BMI160_INT_LATCH_INT1_INPUT_EN, 0) |
			    FIELD_PREP(BMI160_INT_LATCH_MODE,
				       BMI160_INT_LATCH_MODE_NON_LATCHED)));

	/*
	 * Map the data ready interrupt to INT1 and any motion/significant
	 * motion interrupt to INT2.
	 */
	ret = regmap_write(data->regmap, BMI160_REG_INT_MAP_0, 0);
	if (ret != 0) {
		dev_err(dev, "Couldn't write BMI160_REG_INT_MAP_0");
		return ret;
	}
	ret = regmap_write(data->regmap, BMI160_REG_INT_MAP_1,
			   BMI160_INT_MAP_1_INT1_DRDY);
	if (ret != 0) {
		dev_err(dev, "Couldn't write BMI160_REG_INT_MAP_1");
		return ret;
	}
	ret = regmap_write(data->regmap, BMI160_REG_INT_MAP_2,
			   BMI160_INT_MAP_2_INT2_ANYM_SIGMOT);
	if (ret != 0) {
		dev_err(dev, "Couldn't write BMI160_REG_INT_MAP_2");
		return ret;
	}

	/*
	 * Select the significant motion interrupt instead of anymotion and set
	 * "skip" which is the minimum time between two significant motion
	 * events and "proof" which is the duration of motion required to
	 * trigger the significant motion event.
	 */
	ret = regmap_write(
		data->regmap,
		BMI160_REG_INT_MOTION_3,
		(BMI160_INT_MOTION_3_SIG_MOT_SEL |
		 FIELD_PREP(BMI160_INT_MOTION_3_SIG_MOT_SKIP,
			    BMI160_SIG_MOT_SKIP_TIME_1500_MS) |
		 FIELD_PREP(BMI160_INT_MOTION_3_SIG_MOT_PROOF,
			    BMI160_SIG_MOT_PROOF_TIME_2000_MS)));
	if (ret != 0) {
		dev_err(dev, "Couldn't write BMI160_REG_INT_MOTION_3");
		return ret;
	}

	/* Register the interrupt handlers */
	ret = bmi160_register_drdy_trigger(data);
	if (ret != 0) {
		dev_err(dev, "Failed to create data ready trigger");
		return ret;
	}

	if (data->int2_irq > 0) {
		ret = devm_request_threaded_irq(dev, data->int2_irq, NULL,
						bmi160_sigmot_interrupt_thread,
						IRQF_TRIGGER_RISING,
						"bmi160_sigmot", data);
		if (ret != 0) {
			dev_err(dev,
				"Couldn't request irq %d for significant motion interrupt",
				data->int1_irq);
			return ret;
		}
	}

	/* Enable the significant motion interrupt */
	ret = regmap_update_bits(data->regmap, BMI160_REG_INT_EN_0,
				 (BMI160_INT_EN_0_ANYM_Z |
				  BMI160_INT_EN_0_ANYM_Y |
				  BMI160_INT_EN_0_ANYM_X),
				 (FIELD_PREP(BMI160_INT_EN_0_ANYM_Z, 1) |
				  FIELD_PREP(BMI160_INT_EN_0_ANYM_Y, 1) |
				  FIELD_PREP(BMI160_INT_EN_0_ANYM_X, 1)));
	if (ret != 0) {
		dev_err(dev, "Couldn't update BMI160_REG_INT_EN_0");
		return ret;
	}

	return ret;
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

static struct attribute *bmi160_attrs[] = {
	&iio_const_attr_in_accel_sampling_frequency_available.dev_attr.attr,
	&iio_const_attr_in_anglvel_sampling_frequency_available.dev_attr.attr,
	&iio_const_attr_in_accel_scale_available.dev_attr.attr,
	&iio_const_attr_in_anglvel_scale_available.dev_attr.attr,
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
	struct bmi160_platform_data *pdata;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	dev_set_drvdata(dev, indio_dev);
	data->regmap = regmap;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0)
	data->dev = dev;
#endif

	pdata = dev_get_platdata(dev);
	if (pdata)
	{
		data->int1_irq = pdata->int1_irq;
		data->int2_irq = pdata->int2_irq;
	}

	if (!name && ACPI_HANDLE(dev))
		name = bmi160_match_acpi_device(dev);

	ret = bmi160_chip_init(data, use_spi);
	if (ret < 0)
		return ret;

	mutex_init(&data->mutex);

	indio_dev->dev.parent = dev;
	indio_dev->channels = bmi160_channels;
	indio_dev->num_channels = ARRAY_SIZE(bmi160_channels);
	indio_dev->name = name;
	indio_dev->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_TRIGGERED;
	indio_dev->info = &bmi160_info;

	ret = bmi160_setup_interrupts(data);
	if (ret < 0)
		return ret;


	ret = iio_triggered_buffer_setup(indio_dev, NULL,
					 bmi160_trigger_handler,
					 &bmi160_iio_buffer_setup_ops);
	if (ret < 0)
		goto uninit;

	ret = devm_iio_device_register(dev, indio_dev);
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

	iio_triggered_buffer_cleanup(indio_dev);
	iio_trigger_unregister(data->drdy_trigger);
	bmi160_chip_uninit(data);
}
EXPORT_SYMBOL_GPL(bmi160_core_remove);

MODULE_AUTHOR("Daniel Baluta <daniel.baluta@intel.com");
MODULE_DESCRIPTION("Bosch BMI160 driver");
MODULE_LICENSE("GPL v2");
