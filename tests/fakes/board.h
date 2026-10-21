/* 【19 测试替身接口：不是实际板卡驱动】
 * 阅读导航：docs/代码阅读地图.md。
 */
/* 测试替身：只为无线驱动提供可控时钟、GPIO与UART，不代表真实外设。 */
#ifndef TEST_BOARD_H
#define TEST_BOARD_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {int unused;} UART_HandleTypeDef;
typedef enum {HAL_OK=0,HAL_ERROR=1} HAL_StatusTypeDef;
typedef enum {GPIO_PIN_RESET=0,GPIO_PIN_SET=1} GPIO_PinState;
#define GPIOA 1
#define GPIOB 2
#define GPIOC 3
#define GPIO_PIN_0 1u
#define GPIO_PIN_1 2u
#define GPIO_PIN_10 1024u
#define GPIO_PIN_13 8192u
extern UART_HandleTypeDef huart1;
extern volatile bool radio_aux_low_seen;
void HAL_GPIO_WritePin(int port,uint16_t pin,GPIO_PinState value);
GPIO_PinState HAL_GPIO_ReadPin(int port,uint16_t pin);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h,uint8_t *data,uint16_t n,uint32_t timeout);
void board_watchdog_feed(void);
bool board_radio_read_byte(uint8_t *value);
void board_radio_flush_rx(void);
void fake_radio_reset(unsigned mode);
unsigned fake_radio_sends(void);
bool fake_radio_sleeping(void);
#endif
