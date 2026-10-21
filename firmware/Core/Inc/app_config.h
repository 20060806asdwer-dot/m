#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Selected by CMake preset. Stage 0=LED/UART, 1=sensors, 2=wireless/system. */
#ifndef APP_STAGE
#define APP_STAGE 0
#endif
#ifndef APP_LOW_POWER
#define APP_LOW_POWER 0
#endif
#ifndef APP_ADAPTIVE
#define APP_ADAPTIVE 1
#endif
#ifndef APP_ENFORCE_ENERGY
#define APP_ENFORCE_ENERGY 0
#endif
#ifndef APP_LOG_ENABLE
#define APP_LOG_ENABLE 1
#endif
#ifndef APP_WATCHDOG
#define APP_WATCHDOG APP_LOW_POWER
#endif

#define NODE_ID                    1u
#define DEBUG_UART_BAUD            9600u
#define RADIO_UART_BAUD            9600u
#define RADIO_READY_TIMEOUT_MS     3000u
#define RADIO_TX_TIMEOUT_MS        5000u
#define TEMP_CONVERT_TIMEOUT_MS    250u /* 10-bit conversion: maximum 187.5 ms. */
#define VIBRATION_SAMPLES          64u
#define VIBRATION_TIMEOUT_MS       1200u
#define TEMP_ALARM_CENTI           5000
#define VIBRATION_ALARM_MG         300u
#define RTC_LSI_HZ                 40000u /* Calibrate, or use a verified LSE design. */
#define ADC_VREFINT_MV             1200u /* F103 nominal; calibrate with a voltmeter. */
#define STORE_DIVIDER_NUM          2u   /* 100k upper / 100k lower. */
#define STORE_DIVIDER_DEN          1u
#define STORE_VALID_MAX_MV         5500u
#define STORE_PROTECT_MV           3650u
#define STORE_RECOVER_MV           3800u
#define STORE_NORMAL_ENTER_MV      3950u
#define STORE_NORMAL_EXIT_MV       3850u
#define FIXED_PERIOD_S             15u
#define LOW_PERIOD_S               60u
#define PROTECT_POLL_S             5u

#define TEMP_GPIO                  GPIOA
#define TEMP_PIN                   GPIO_PIN_1
#define LED_GPIO                   GPIOC
#define LED_PIN                    GPIO_PIN_13
#define RADIO_M0_GPIO              GPIOB
#define RADIO_M0_PIN               GPIO_PIN_0
#define RADIO_M1_GPIO              GPIOB
#define RADIO_M1_PIN               GPIO_PIN_1
#define RADIO_AUX_GPIO             GPIOB
#define RADIO_AUX_PIN              GPIO_PIN_10
#define ADXL_ADDRESS               (0x53u << 1)

#endif
