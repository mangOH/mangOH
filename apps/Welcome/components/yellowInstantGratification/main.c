#include "legato.h"
#include "interfaces.h"

COMPONENT_INIT
{
    // Route button press events to the "vegas mode" trigger and the buzzer enable.
    dhubAdmin_SetSource("/app/vegasMode/triggered/trigger", "/app/button/value");
    dhubAdmin_SetSource("/app/buzzer/enable", "/app/button/value");

    // Set the buzzer to 100% duty cycle, so it stays on as long as the button is held down.
    dhubAdmin_SetNumericOverride("/app/buzzer/percent", 100.0);

    // Route LED states from the vegasMode app to the LED control outputs.
    dhubAdmin_SetSource("/app/leds/mono/enable", "/app/vegasMode/led/0");
    dhubAdmin_SetSource("/app/leds/tri/red/enable", "/app/vegasMode/led/1");
    dhubAdmin_SetSource("/app/leds/tri/green/enable", "/app/vegasMode/led/2");
    dhubAdmin_SetSource("/app/leds/tri/blue/enable", "/app/vegasMode/led/3");

    // Enable continuous (slow) Vegas Mode.
    dhubAdmin_SetBooleanOverride("/app/vegasMode/continuous/enable", true);
}
