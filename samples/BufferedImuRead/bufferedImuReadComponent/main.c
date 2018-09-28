#include "legato.h"
#include "interfaces.h"

#include "iio.h"

static ssize_t ImuSampleCallback(
    const struct iio_channel *chan, void *src, size_t bytes, void *context)
{
    const struct iio_data_format *fmt = iio_channel_get_data_format(chan);

    unsigned int repeat = fmt->repeat;
    LE_ASSERT(fmt->is_signed);
    if (strcmp("timestamp", iio_channel_get_id(chan)) == 0)
    {
        LE_ASSERT(bytes == 8);
        for (int i = 0; i < repeat; i++)
        {
            int64_t *ts = src;
            LE_INFO("%s = %" PRId64, iio_channel_get_id(chan), *ts);
        }
    }
    else
    {
        LE_ASSERT(bytes == 2);
        for (int i = 0; i < repeat; i++)
        {
            uint16_t *rawVal = src;
            int16_t val = fmt->is_be ? be16toh(*rawVal) : le16toh(*rawVal);
            val >>= fmt->shift;
            const double scale = fmt->with_scale ? fmt->scale : 1.0;
            const double scaledVal = val * scale;

            LE_INFO("%s = %f", iio_channel_get_id(chan), scaledVal);
        }
    }

    return bytes * repeat;
}

void ReadImu(struct iio_context *localCtx)
{
    struct iio_device *bmi160 = iio_context_find_device(localCtx, "bmi160");
    LE_FATAL_IF(bmi160 == NULL, "Couldn't find bmi160 device");

    const bool isOutput = false;
    struct iio_channel *gyroXChan = iio_device_find_channel(bmi160, "anglvel_x", isOutput);
    struct iio_channel *gyroYChan = iio_device_find_channel(bmi160, "anglvel_y", isOutput);
    struct iio_channel *gyroZChan = iio_device_find_channel(bmi160, "anglvel_z", isOutput);
    struct iio_channel *accelXChan = iio_device_find_channel(bmi160, "accel_x", isOutput);
    struct iio_channel *accelYChan = iio_device_find_channel(bmi160, "accel_y", isOutput);
    struct iio_channel *accelZChan = iio_device_find_channel(bmi160, "accel_z", isOutput);
    struct iio_channel *timestampChan = iio_device_find_channel(bmi160, "timestamp", isOutput);
    LE_FATAL_IF(
        gyroXChan == NULL || gyroYChan == NULL || gyroZChan == NULL ||
        accelXChan == NULL || accelYChan == NULL || accelZChan == NULL || timestampChan == NULL,
        "Couldn't find channels");
    LE_INFO(
        "Got channels (%s, %s, %s, %s, %s, %s, %s)",
        iio_channel_get_id(gyroXChan),
        iio_channel_get_id(gyroYChan),
        iio_channel_get_id(gyroZChan),
        iio_channel_get_id(accelXChan),
        iio_channel_get_id(accelYChan),
        iio_channel_get_id(accelZChan),
        iio_channel_get_id(timestampChan));

    iio_channel_enable(gyroXChan);
    iio_channel_enable(gyroYChan);
    iio_channel_enable(gyroZChan);
    iio_channel_enable(accelXChan);
    iio_channel_enable(accelYChan);
    iio_channel_enable(accelZChan);
    iio_channel_enable(timestampChan);

    const size_t samplesCount = 20;
    const bool cyclic = false;
    struct iio_buffer *sampleBuffer = iio_device_create_buffer(bmi160, samplesCount, cyclic);
    LE_FATAL_IF(sampleBuffer == NULL, "Couldn't create sample buffer");

    for (int i = 0; i < 2; i++)
    {
        const ssize_t bytesRead = iio_buffer_refill(sampleBuffer);
        LE_FATAL_IF(bytesRead < 0, "Failed to refill buffer: %m");

        const ssize_t bytesProcessed =
            iio_buffer_foreach_sample(sampleBuffer, ImuSampleCallback, NULL);
        LE_INFO("Processed %d bytes in iio_buffer_foreach_sample", bytesProcessed);
    }

    iio_buffer_destroy(sampleBuffer);
    LE_INFO("Buffer destroyed");
}

COMPONENT_INIT
{
    struct iio_context *localCtx = iio_create_local_context();
    LE_ASSERT(localCtx != NULL);

    LE_FATAL_IF(
        iio_context_set_timeout(localCtx, 5000) != 0, "Failed to set timeout for local context");

    ReadImu(localCtx);

    iio_context_destroy(localCtx);
}
