#ifndef _OPEN_WEATHER_MAP_H_
#define _OPEN_WEATHER_MAP_H_

#include <jansson.h>

LE_SHARED json_t *OpenWeatherMapGet(double latitude, double longitude, const char *apiKey);

#endif // _OPEN_WEATHER_MAP_H_
