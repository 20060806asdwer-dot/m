/* 【09 无线结果与发送接口】
 * 阅读导航：docs/代码阅读地图.md。
 */
#ifndef RADIO_H
#define RADIO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum {RADIO_OK=0,RADIO_READY_TIMEOUT,RADIO_UART_ERROR,RADIO_AUX_TIMEOUT,RADIO_ACK_TIMEOUT,RADIO_BAD_ARGUMENT} radio_result_t;
bool radio_send(const uint8_t *data,size_t n);
void radio_sleep(void);
radio_result_t radio_last_result(void);
uint32_t radio_last_attempts(void);
#endif
