#ifndef ENVIRONMENT_SENSOR_H
#define ENVIRONMENT_SENSOR_H

LE_SHARED le_result_t mangOH_ReadPressureSensor(double *reading);
LE_SHARED le_result_t mangOH_ReadTemperatureSensor(double *reading);
LE_SHARED le_result_t mangOH_ReadGasSensor(double *reading);
LE_SHARED le_result_t mangOH_ReadHumiditySensor(double *reading);

#endif // ENVIRONMENT_SENSOR_H
