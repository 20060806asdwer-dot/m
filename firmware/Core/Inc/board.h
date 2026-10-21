#ifndef BOARD_H
#define BOARD_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
extern UART_HandleTypeDef huart1,huart2;
extern I2C_HandleTypeDef hi2c1;
extern ADC_HandleTypeDef hadc1;
extern volatile bool radio_aux_low_seen;
void board_init(void);
void board_fatal(const char *reason);
void board_log(const char *fmt,...);
void board_delay_us(uint32_t us);
void board_watchdog_feed(void);
void board_wait_seconds(uint32_t seconds,bool stop);
uint32_t board_rtc_seconds(void);
uint32_t board_uptime(void);
bool board_measure_store(uint16_t *mv,uint16_t *vdda);
bool board_next_boot(uint32_t *counter);
void board_led(bool on);
#endif
