/**
 * @file
 *
 * Bosch Temperature, Humidity, Pressure and Air Quality monitor.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */
#include <iio.h>
#include <locale.h>
#include "legato.h"

#define BME680_DEVICE_NAME                      "bme680"
#define BMI088_ACCEL_DEVICE_NAME                "bmi088a" 
#define BMI088_GYRO_DEVICE_NAME                 "bmi088g"   

#define BME680_GAS_RESISTANCE                   "resistance"
#define BME680_HUMIDITY                         "humidityrelative"
#define BME680_TEMP                             "temp"
#define BME680_PRESSURE                         "pressure"

#define BMI088_ACCEL_X                          "accel_x"
#define BMI088_ACCEL_Y                          "accel_y"
#define BMI088_ACCEL_Z                          "accel_z"

#define BMI088_GYRO_X                           "anglvel_x"
#define BMI088_GYRO_Y                           "anglvel_y"
#define BMI088_GYRO_Z                           "anglvel_z"
#define BMI088_GYRO_TEMP                        "anglvel_temp"

#define BMI088_TEMP                             "temp"
#define BMI088_TIMESTAMP                        "timestamp"

#define BMI088_ACCEL_X_ATTRIBUTE_ENABLE         "accel_x_en"
#define BMI088_ACCEL_Y_ATTRIBUTE_ENABLE         "accel_y_en"
#define BMI088_ACCEL_Z_ATTRIBUTE_ENABLE         "accel_z_en"
#define BMI088_ACCEL_ATTRIBUTE_DURATION         "accel_duration"
#define BMI088_ACCEL_ATTRIBUTE_THREASHOLD       "accel_threshold"
#define BMI088_ACCEL_ATTRIBUTE_NO_MOTION        "accel_nomotion_sel"

#define BMI088_ACCEL_DEFAULT_DURATION           100
#define BMI088_ACCEL_DEFAULT_THREASHOLD         100

typedef struct _bmi088_sensor_data_t {
    int16_t     x;
    int16_t     y;
    int16_t     z;
    int32_t     temp;
    uint32_t    time;
} __attribute__((packed)) bmi088_sensor_data_t;

static struct iio_context *ctx = NULL;
static struct iio_buffer *accelBuffer = NULL;
static struct iio_buffer *gyroBuffer = NULL;

static le_timer_Ref_t SensorTimer;
static le_fdMonitor_Ref_t accelFdMonitor;
static le_fdMonitor_Ref_t gyroFdMonitor;

// Wait time between each round of sensor readings.
static const int DelayBetweenReadings = 5;

static bool channel_has_attr(struct iio_channel *chn, const char *attr)
{
	unsigned int i, nb = iio_channel_get_attrs_count(chn);
	for (i = 0; i < nb; i++)
		if (!strcmp(attr, iio_channel_get_attr(chn, i)))
			return true;
	return false;
}

static bool is_valid_channel(struct iio_channel *chn)
{
        return !iio_channel_is_output(chn) &&
                (channel_has_attr(chn, "raw") ||
                 channel_has_attr(chn, "input"));
}

static double get_channel_value(struct iio_channel *chn)
{
	char *old_locale;
	char buf[1024];
	double val;

	old_locale = strdup(setlocale(LC_NUMERIC, NULL));
	setlocale(LC_NUMERIC, "C");

	if (channel_has_attr(chn, "input")) {
		iio_channel_attr_read(chn, "input", buf, sizeof(buf));
		val = strtod(buf, NULL);
	} else {
		iio_channel_attr_read(chn, "raw", buf, sizeof(buf));
		val = strtod(buf, NULL);

		if (channel_has_attr(chn, "offset")) {
			iio_channel_attr_read(chn, "offset", buf, sizeof(buf));
			val += strtod(buf, NULL);
		}

		if (channel_has_attr(chn, "scale")) {
			iio_channel_attr_read(chn, "scale", buf, sizeof(buf));
			val *= strtod(buf, NULL);
		}
	}

	setlocale(LC_NUMERIC, old_locale);
	free(old_locale);

	return val;
}

static void SensorTimerHandler(le_timer_Ref_t timer)
{
    struct iio_context *ctx = le_timer_GetContextPtr(timer); 	
    struct iio_device *dev = iio_context_find_device(ctx, BME680_DEVICE_NAME);
    LE_FATAL_IF(dev == NULL, "Failed to get BME680 device");

    struct iio_channel *gas_resistance_chn = iio_device_find_channel(dev, BME680_GAS_RESISTANCE, false);
    LE_FATAL_IF(gas_resistance_chn == NULL, "Failed to get gas resistance channel");
    LE_INFO("Gas resistance(%.2f ohms)", get_channel_value(gas_resistance_chn));

    struct iio_channel *humidity_chn = iio_device_find_channel(dev, BME680_HUMIDITY, false);
    LE_FATAL_IF(humidity_chn == NULL, "Failed to get humidity channel");
    LE_INFO("Humidity(%.2f %%rH)", get_channel_value(humidity_chn));

    struct iio_channel *temp_chn = iio_device_find_channel(dev, BME680_TEMP, false);
    LE_FATAL_IF(temp_chn == NULL, "Failed to get temperature channel");
    LE_INFO("Temp(%.2f degC)", get_channel_value(temp_chn));

    struct iio_channel *pressure_chn = iio_device_find_channel(dev, BME680_PRESSURE, false);
    LE_FATAL_IF(pressure_chn == NULL, "Failed to get pressure channel");
    LE_INFO("Pressure(%.2f Pa)", get_channel_value(pressure_chn));
}

ssize_t accel_sample_cb(const struct iio_channel *chn, void *src, size_t bytes, void *d)
{
    /* Use "src" to read or write a sample for this channel */
    bmi088_sensor_data_t data = {0};
    iio_channel_convert(chn, &data, (bmi088_sensor_data_t*)src);
    LE_INFO("Bytes Rx(%u)", bytes);

    LE_INFO("Accel (x,y,z)(%d, %d, %d) temp(%d) time(%u)", data.x, data.y, data.z, data.temp, data.time);
    return bytes;
}

static void AccelHandler(int fd, short events)
{
    if (events & POLLIN)
    {
        ssize_t samples = iio_buffer_refill(accelBuffer);
        LE_INFO("samples(%u)", samples);
        ssize_t ret = iio_buffer_foreach_sample(accelBuffer, accel_sample_cb, NULL);
        LE_INFO("iio_buffer_foreach_sample() returns(%u)", ret);
    }
 
    if ((events & POLLERR) || (events & POLLHUP))
    {
        LE_WARN("Error or hangup");
    }
}

ssize_t gyro_sample_cb(const struct iio_channel *chn, void *src, size_t bytes, void *d)
{
    /* Use "src" to read or write a sample for this channel */
    bmi088_sensor_data_t data = {0};
    iio_channel_convert(chn, &data, (bmi088_sensor_data_t*)src);
    LE_INFO("Bytes Rx(%u)", bytes);

    LE_INFO("Gyro (x,y,z)(%d, %d, %d) temp(%d) time(%u)", data.x, data.y, data.z, data.temp, data.time);
    return bytes;
}

static void GyroHandler(int fd, short events)
{
    if (events & POLLIN)
    {
        ssize_t samples = iio_buffer_refill(gyroBuffer);
        LE_INFO("samples(%u)", samples);
        ssize_t ret = iio_buffer_foreach_sample(gyroBuffer, gyro_sample_cb, NULL);
        LE_INFO("iio_buffer_foreach_sample() returns(%u)", ret);
    }
 
    if ((events & POLLERR) || (events & POLLHUP))
    {
        LE_WARN("Error or hangup");
    }
}

static void AccelConfig(struct iio_context *ctx)
{
    int ret;

    struct iio_device *dev = iio_context_find_device(ctx, BMI088_ACCEL_DEVICE_NAME);
    LE_FATAL_IF(dev == NULL, "Failed to get BMI088 acceleration device");

    struct iio_channel *accel_x_chn = iio_device_find_channel(dev, BMI088_ACCEL_X, false);
    LE_FATAL_IF(accel_x_chn == NULL, "Failed to get accel-x channel");
    iio_channel_enable(accel_x_chn);
    LE_FATAL_IF(!iio_channel_is_enabled(accel_x_chn), "Failed to enable accel-x channel");

    struct iio_channel *accel_y_chn = iio_device_find_channel(dev, BMI088_ACCEL_Y, false);
    LE_FATAL_IF(accel_y_chn == NULL, "Failed to get accel-y channel");
    iio_channel_enable(accel_y_chn);
    LE_FATAL_IF(!iio_channel_is_enabled(accel_y_chn), "Failed to enable accel-y channel");

    struct iio_channel *accel_z_chn = iio_device_find_channel(dev, BMI088_ACCEL_Z, false);
    LE_FATAL_IF(accel_z_chn == NULL, "Failed to get accel-z channel");
    iio_channel_enable(accel_z_chn);
    LE_FATAL_IF(!iio_channel_is_enabled(accel_z_chn), "Failed to enable accel-z channel");

    ret = iio_device_attr_write_bool(dev, BMI088_ACCEL_X_ATTRIBUTE_ENABLE, true);
    if (ret < 0)
    {
        LE_ERROR("iio_device_attr_write_bool() failed(%d/%d)", ret, errno);
        goto exit;
    }

    ret = iio_device_attr_write_bool(dev, BMI088_ACCEL_Y_ATTRIBUTE_ENABLE, true);
    if (ret < 0)
    {
        LE_ERROR("iio_device_attr_write_bool() failed(%d/%d)", ret, errno);
        goto exit;
    }

    ret = iio_device_attr_write_bool(dev, BMI088_ACCEL_Z_ATTRIBUTE_ENABLE, true);
    if (ret < 0)
    {
        LE_ERROR("iio_device_attr_write_bool() failed(%d/%d)", ret, errno);
        goto exit;
    }

    ret = iio_device_attr_write_longlong(dev, BMI088_ACCEL_ATTRIBUTE_DURATION, BMI088_ACCEL_DEFAULT_DURATION);
    if (ret < 0)
    {
        LE_ERROR("iio_device_attr_write_longlong() failed(%d/%d)", ret, errno);
        goto exit;
    }

    ret = iio_device_attr_write_longlong(dev, BMI088_ACCEL_ATTRIBUTE_THREASHOLD, BMI088_ACCEL_DEFAULT_THREASHOLD);
    if (ret < 0)
    {
        LE_ERROR("iio_device_attr_write_longlong() failed(%d/%d)", ret, errno);
        goto exit;
    }

    ret = iio_device_attr_write_longlong(dev, BMI088_ACCEL_ATTRIBUTE_NO_MOTION, 0);
    if (ret < 0)
    {
        LE_ERROR("iio_device_attr_write_longlong() failed(%d/%d)", ret, errno);
        goto exit;
    }

    LE_INFO("Device '%s' sample size(%u)", iio_device_get_name(dev), iio_device_get_sample_size(dev));

    accelBuffer = iio_device_create_buffer(dev, 1, false);
    if (accelBuffer == NULL)
    {
        LE_ERROR("iio_device_create_buffer() failed(%d)", errno);
        goto exit;
    }

    int accelFd = iio_buffer_get_poll_fd(accelBuffer);
    if (accelFd < 0)
    {
        LE_ERROR("iio_buffer_get_poll_fd() failed(%d)", errno);
        goto exit;
    }

    ret = iio_buffer_set_blocking_mode(accelBuffer, true);
    if (ret < 0)
    {
        LE_ERROR("iio_buffer_set_blocking_mode() failed(%d/%d)", ret, errno);
        goto exit;
    }

    accelFdMonitor = le_fdMonitor_Create("Accel", accelFd, AccelHandler, POLLIN);
    LE_FATAL_IF(accelFdMonitor == NULL, "Failed to create accel monitor");

exit:
    return;
}

static void GyroConfig(struct iio_context *ctx)
{
    int ret;

    struct iio_device *dev = iio_context_find_device(ctx, BMI088_GYRO_DEVICE_NAME);
    LE_FATAL_IF(dev == NULL, "Failed to get BMI088 gyro device");

    struct iio_channel *gyro_x_chn = iio_device_find_channel(dev, BMI088_GYRO_X, false);
    LE_FATAL_IF(gyro_x_chn == NULL, "Failed to get gyro-x channel");
    iio_channel_enable(gyro_x_chn);
    LE_FATAL_IF(!iio_channel_is_enabled(gyro_x_chn), "Failed to enable gyro-x channel");

    struct iio_channel *gyro_y_chn = iio_device_find_channel(dev, BMI088_GYRO_Y, false);
    LE_FATAL_IF(gyro_y_chn == NULL, "Failed to get gyro-y channel");
    iio_channel_enable(gyro_y_chn);
    LE_FATAL_IF(!iio_channel_is_enabled(gyro_y_chn), "Failed to enable gyro-y channel");

    struct iio_channel *gyro_z_chn = iio_device_find_channel(dev, BMI088_GYRO_Z, false);
    LE_FATAL_IF(gyro_z_chn == NULL, "Failed to get gyro-z channel");
    iio_channel_enable(gyro_z_chn);
    LE_FATAL_IF(!iio_channel_is_enabled(gyro_z_chn), "Failed to enable gyro-z channel");

    LE_INFO("Device '%s' sample size(%u)", iio_device_get_name(dev), iio_device_get_sample_size(dev));

    gyroBuffer = iio_device_create_buffer(dev, 1, false);
    if (gyroBuffer == NULL)
    {
        LE_ERROR("iio_device_create_buffer() failed(%d)", errno);
        goto exit;
    }

    int gyroFd = iio_buffer_get_poll_fd(gyroBuffer);
    if (gyroFd < 0)
    {
        LE_ERROR("iio_buffer_get_poll_fd() failed(%d)", errno);
        goto exit;
    }

    ret = iio_buffer_set_blocking_mode(gyroBuffer, true);
    if (ret < 0)
    {
        LE_ERROR("iio_buffer_set_blocking_mode() failed(%d/%d)", ret, errno);
        goto exit;
    }

    gyroFdMonitor = le_fdMonitor_Create("Gyro", gyroFd, GyroHandler, POLLIN);
    LE_FATAL_IF(gyroFdMonitor == NULL, "Failed to create gyro monitor");

exit:
    return;
}

static void ShutdownHandler(int sigNum)
{
    if (accelBuffer) iio_buffer_destroy(accelBuffer);
    if (gyroBuffer) iio_buffer_destroy(gyroBuffer);
    if (ctx) { iio_context_destroy(ctx); }
}

static void display_iio(struct iio_context *ctx)
{
    unsigned int i, j, k;

    unsigned int nb_devs = iio_context_get_devices_count(ctx);
    LE_INFO("# devices(%u)", nb_devs);
    for (i = 0; i < nb_devs; i++) {
        struct iio_device *dev = iio_context_get_device(ctx, i);
        unsigned int nb_channels = iio_device_get_channels_count(dev);
        LE_INFO("Dev%u: '%s' is trigger(%d) sample size(%u)", 
                i + 1, iio_device_get_name(dev), iio_device_is_trigger(dev), iio_device_get_sample_size(dev));

        unsigned int nb_attrs = iio_device_get_attrs_count(dev);
        LE_INFO("# attributes(%u)", nb_attrs);
        for (j = 0; j < nb_attrs; j++) {
            const char* attr = (const char*)iio_device_get_attr(dev, j);
            LE_INFO("Device attr%u: '%s'", j + 1, attr);
        }

        LE_INFO("# channels(%u)", nb_channels);
        for (j = 0; j < nb_channels; j++) {
            const char *name;
            const char *id;
            struct iio_channel *chn = iio_device_get_channel(dev, j);

            if (!is_valid_channel(chn))
                continue;

            name = iio_channel_get_name(chn);
            id = iio_channel_get_id(chn);
            if (!name)
                name = id;

            LE_INFO("Ch%u: '%s' id('%s') type(%u) modifier(%u) is scan element(%d) output(%d)", 
                j + 1, name, id, iio_channel_get_type(chn), iio_channel_get_modifier(chn), 
                iio_channel_is_scan_element(chn), iio_channel_is_output(chn));

            unsigned int nb_chn_attrs = iio_channel_get_attrs_count(chn);
            LE_INFO("# channel attributes(%u)", nb_chn_attrs);
            for (k = 0; k < nb_chn_attrs; k++) {
                const char* chn_attr = iio_channel_get_attr(chn, k);
                LE_INFO("Channel attr%u: '%s' file('%s')", k + 1, chn_attr, iio_channel_attr_get_filename(chn, chn_attr));
            }
        }

        unsigned int nb_buff_attrs = iio_device_get_buffer_attrs_count(dev);
        LE_INFO("# buffer attributes(%u)", nb_buff_attrs);
        for (j = 0; j < nb_buff_attrs; j++) {
            const char* buff_attr = iio_device_get_buffer_attr(dev, j);
            LE_INFO("Buffer attr%u: '%s'", j + 1, buff_attr);
        }
    }
}

COMPONENT_INIT
{  
    ctx = iio_create_local_context();

    le_sig_Block(SIGTERM);
    le_sig_SetEventHandler(SIGTERM, ShutdownHandler);

    display_iio(ctx);

    AccelConfig(ctx);
    GyroConfig(ctx);

    SensorTimer = le_timer_Create("Bosch Sensor");
    LE_FATAL_IF(SensorTimer == NULL, "Failed to create sensor timer");

    LE_ASSERT_OK(le_timer_SetMsInterval(SensorTimer, DelayBetweenReadings * 1000));
    LE_ASSERT_OK(le_timer_SetRepeat(SensorTimer, 0));
    LE_ASSERT_OK(le_timer_SetHandler(SensorTimer, SensorTimerHandler));
    LE_ASSERT_OK(le_timer_SetContextPtr(SensorTimer, ctx));
    LE_ASSERT_OK(le_timer_Start(SensorTimer)); 	
}
