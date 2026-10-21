/* 【08 无线帧字段与有效位定义】
 * 阅读导航：docs/代码阅读地图.md。
 */
#ifndef PACKET_H
#define PACKET_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#define PACKET_SIZE 24u
#define ACK_SIZE 10u
#define FLAG_TEMP_VALID 1u
#define FLAG_VIB_VALID 2u
#define FLAG_VOLT_VALID 4u
#define FLAG_ALARM 8u
typedef struct {
    uint8_t node,state,flags;
    uint16_t boot,seq,store_mv,vibration_mg,reset_count;
    uint32_t uptime_s;
    int16_t temp_centi;
} telemetry_t;
uint16_t crc16_ccitt(const uint8_t *data,size_t n);
uint8_t crc8_dallas(const uint8_t *data,size_t n);
void packet_encode(const telemetry_t *record,uint8_t out[PACKET_SIZE]);
void packet_ack_encode(uint8_t node,uint16_t boot,uint16_t seq,uint8_t out[ACK_SIZE]);
bool packet_ack_matches(const uint8_t data[ACK_SIZE],uint8_t node,uint16_t boot,uint16_t seq);
#endif
