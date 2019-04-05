#include "legato.h"
#include "interfaces.h"

#include "outputActuator.h"

static const struct
{
    const char *sysfsPath;
    const char *dhubPath;
} OutputActuators[] =
{
    {
        .sysfsPath="/sys/devices/platform/expander.0/tri_led_red",
        .dhubPath="tri_led_red",
    },
    {
        .sysfsPath="/sys/devices/platform/expander.0/tri_led_grn",
        .dhubPath="tri_led_green",
    },
    {
        .sysfsPath="/sys/devices/platform/expander.0/tri_led_blu",
        .dhubPath="tri_led_blue",
    },
    {
        .sysfsPath="/sys/devices/platform/expander.0/generic_led",
        .dhubPath="generic_led",
    },
};

COMPONENT_INIT
{
    for (int i = 0; i < NUM_ARRAY_MEMBERS(OutputActuators); i++)
    {
        le_result_t res = RegisterOutputActuator(
            OutputActuators[i].dhubPath, OutputActuators[i].sysfsPath);
        LE_FATAL_IF(
            res != LE_OK,
            "Output actuator registration failed for %s at %s",
            OutputActuators[i].sysfsPath,
            OutputActuators[i].dhubPath);
    }
}
