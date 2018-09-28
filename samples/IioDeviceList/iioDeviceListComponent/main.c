#include "legato.h"
#include "interfaces.h"

#include "iio.h"

static void PerformScan(struct iio_context *localCtx)
{
    unsigned int i;
    struct iio_device *device;
    for (i = 0, device = iio_context_get_device(localCtx, i);
         device != NULL;
         ++i, device = iio_context_get_device(localCtx, i))
    {
        LE_INFO("-> IIO Device: '%s'", iio_device_get_name(device));
        unsigned int j;
        const char *attr;
        char attrVal[128];

        LE_INFO("--> IIO Device is trigger: %d", iio_device_is_trigger(device));
        for (j = 0, attr = iio_device_get_attr(device, j);
             attr != NULL;
             ++j, attr = iio_device_get_attr(device, j))
        {
            const ssize_t readLen = iio_device_attr_read(device, attr, attrVal, sizeof(attrVal));
            LE_INFO(
                "--> IIO Device Attribute: '%s=%s'",
                attr,
                readLen < 0 ? "read error!" : attrVal);
        }
        for (j = 0, attr = iio_device_get_buffer_attr(device, j);
             attr != NULL;
             ++j, attr = iio_device_get_buffer_attr(device, j))
        {
            const ssize_t readLen =
                iio_device_buffer_attr_read(device, attr, attrVal, sizeof(attrVal));
            LE_INFO(
                "--> IIO Device Buffer Attribute: '%s=%s'",
                attr,
                readLen < 0 ? "read error!" : attrVal);
        }
        for (j = 0, attr = iio_device_get_debug_attr(device, j);
             attr != NULL;
             ++j, attr = iio_device_get_debug_attr(device, j))
        {
            const ssize_t readLen =
                iio_device_debug_attr_read(device, attr, attrVal, sizeof(attrVal));
            LE_INFO(
                "--> IIO Device Debug Attribute: '%s=%s'",
                attr,
                readLen < 0 ? "read error!" : attrVal);
        }

        struct iio_channel *chan;
        for (j = 0, chan = iio_device_get_channel(device, j);
             chan != NULL;
             ++j, chan = iio_device_get_channel(device, j))
        {
            unsigned int k;
            LE_INFO("--> IIO Channel: %s,%s", iio_channel_get_id(chan), iio_channel_get_name(chan));
            LE_INFO("---> Direction: %s", iio_channel_is_output(chan) ? "output" : "input");
            LE_INFO("---> Channel Type: %d", iio_channel_get_type(chan));
            LE_INFO("---> Enabled: %s", iio_channel_is_enabled(chan) ? "yes" : "no");
            for (k = 0, attr = iio_channel_get_attr(chan, k);
                 attr != NULL;
                 ++k, attr = iio_channel_get_attr(chan, k))
            {
                const ssize_t readLen = iio_channel_attr_read(chan, attr, attrVal, sizeof(attrVal));
                LE_INFO(
                    "---> IIO Channel Attribute: %s=%s",
                    attr,
                    readLen < 0 ? "read error!" : attrVal);
            }
        }
    }
}

COMPONENT_INIT
{
    struct iio_context *localCtx = iio_create_local_context();
    LE_ASSERT(localCtx != NULL);

    LE_FATAL_IF(
        iio_context_set_timeout(localCtx, 5000) != 0, "Failed to set timeout for local context");

    PerformScan(localCtx);

    iio_context_destroy(localCtx);
}
