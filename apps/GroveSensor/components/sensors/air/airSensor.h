#ifndef AIR_SENSOR_H
#define AIR_SENSOR_H

LE_SHARED le_result_t air_ReadStandardPM1_0(uint16_t *value);
LE_SHARED le_result_t air_ReadStandardPM2_5(uint16_t *value);
LE_SHARED le_result_t air_ReadStandardPM10(uint16_t *value);
LE_SHARED le_result_t air_ReadAtmosphericEnvironmentPM1_0(uint16_t *value);
LE_SHARED le_result_t air_ReadAtmosphericEnvironmentPM2_5(uint16_t *value);
LE_SHARED le_result_t air_ReadAtmosphericEnvironmentPM10(uint16_t *value);

#endif // AIR_SENSOR_H
