/* 【05 数据计算接口】
 * 阅读导航：docs/代码阅读地图.md。
 */
#ifndef FEATURES_H
#define FEATURES_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {int64_t sum[3];uint64_t sum_sq[3];uint32_t count;} vibration_window_t;
void vibration_reset(vibration_window_t *w);
bool vibration_add(vibration_window_t *w,int16_t x,int16_t y,int16_t z);
uint16_t vibration_rms_mg(const vibration_window_t *w);
bool ds18b20_decode(const uint8_t scratchpad[9],int16_t *centi);
bool store_convert(uint16_t raw,uint16_t vref_raw,uint16_t *store_mv,uint16_t *vdda_mv);
#endif
