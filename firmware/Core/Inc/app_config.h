/* 【02 参数表：引脚、阈值、周期与能耗估算】
 * 阅读导航：docs/代码阅读地图.md。
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* A 配置开关：由构建预设决定测试阶段与工作方式 */
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
#ifndef APP_RADIO_ACK
#define APP_RADIO_ACK 0
#endif

/* B 能量预算与无线确认：估算值须经实物校准 */
/* 能量预算初始估算，必须按实物电容和完整任务波形校准。 */
#define STORE_CAPACITANCE_MF        1000u
#define ENERGY_RESERVE_MJ           100u
#define TEMP_COST_MJ                5u
#define VIBRATION_COST_MJ           10u
#define RADIO_COST_MJ               150u
#define ACK_RADIO_COST_MJ           400u
#define SLEEP_POWER_UW              200u
#define MAX_ADAPTIVE_PERIOD_S       300u
#define RADIO_ACK_TIMEOUT_MS        3000u
#define RADIO_RETRIES               1u

/* C 节点身份、串口速率与传感器参数 */
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
/* D 时钟和ADC标称值：需要校准 */
#define RTC_LSI_HZ                 40000u /* Calibrate, or use a verified LSE design. */
#define ADC_VREFINT_MV             1200u /* F103 nominal; calibrate with a voltmeter. */
#define STORE_DIVIDER_NUM          2u   /* 100k upper / 100k lower. */
#define STORE_DIVIDER_DEN          1u
#define STORE_VALID_MAX_MV         5500u
/* E 电压滞回阈值与基础周期 */
#define STORE_PROTECT_MV           3650u
#define STORE_RECOVER_MV           3800u
#define STORE_NORMAL_ENTER_MV      3950u
#define STORE_NORMAL_EXIT_MV       3850u
#define FIXED_PERIOD_S             15u
#define LOW_PERIOD_S               60u
#define PROTECT_POLL_S             5u

/* F 接线定义：修改前先核对README接线表 */
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

/* G 参数一致性：把常见的阈值顺序/除零配置错误拦在编译阶段。 */
_Static_assert(STORE_CAPACITANCE_MF>0,"capacitance must be positive");
_Static_assert(STORE_DIVIDER_DEN>0 && STORE_DIVIDER_NUM>=STORE_DIVIDER_DEN,"invalid ADC divider");
_Static_assert(STORE_PROTECT_MV<STORE_RECOVER_MV && STORE_RECOVER_MV<=STORE_NORMAL_EXIT_MV && STORE_NORMAL_EXIT_MV<STORE_NORMAL_ENTER_MV,"invalid voltage hysteresis");
_Static_assert(STORE_NORMAL_ENTER_MV<=STORE_VALID_MAX_MV,"voltage threshold outside measurement range");
_Static_assert(FIXED_PERIOD_S>0 && LOW_PERIOD_S>=FIXED_PERIOD_S && MAX_ADAPTIVE_PERIOD_S>=LOW_PERIOD_S,"invalid sampling periods");
_Static_assert(RADIO_RETRIES<=3,"too many energy-consuming retries");

#endif
