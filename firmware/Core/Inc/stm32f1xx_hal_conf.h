#ifndef STM32F1xx_HAL_CONF_H
#define STM32F1xx_HAL_CONF_H
#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RTC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HSE_VALUE 8000000u
#define HSE_STARTUP_TIMEOUT 100u
#define HSI_VALUE 8000000u
#define LSI_VALUE 40000u
#define LSE_VALUE 32768u
#define LSE_STARTUP_TIMEOUT 5000u
#define VDD_VALUE 3300u
#define TICK_INT_PRIORITY 15u
#define USE_RTOS 0u
#define PREFETCH_ENABLE 1u
#define USE_HAL_ADC_REGISTER_CALLBACKS 0u
#define USE_HAL_I2C_REGISTER_CALLBACKS 0u
#define USE_HAL_UART_REGISTER_CALLBACKS 0u
#define USE_HAL_RTC_REGISTER_CALLBACKS 0u
#include "stm32f1xx_hal_rcc.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_dma.h"
#include "stm32f1xx_hal_cortex.h"
#include "stm32f1xx_hal_adc.h"
#include "stm32f1xx_hal_i2c.h"
#include "stm32f1xx_hal_uart.h"
#include "stm32f1xx_hal_pwr.h"
#include "stm32f1xx_hal_rtc.h"
#include "stm32f1xx_hal_flash.h"
#include "stm32f1xx_hal_iwdg.h"
#define assert_param(expr) ((void)0u)
#endif
