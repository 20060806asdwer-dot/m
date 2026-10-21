#ifndef SENSORS_H
#define SENSORS_H
#include <stdbool.h>
#include <stdint.h>
bool temperature_read(int16_t *centi);
bool vibration_read(uint16_t *rms_mg);
#endif
