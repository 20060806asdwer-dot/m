/* 【04 传感器接口】
 * 阅读导航：docs/代码阅读地图.md。
 */
#ifndef SENSORS_H
#define SENSORS_H
#include <stdbool.h>
#include <stdint.h>
bool temperature_read(int16_t *centi);
bool vibration_read(uint16_t *rms_mg);
#endif
