/* 【03 主板服务：时钟、引脚、串口、ADC、休眠与中断】
 * 阅读导航：docs/代码阅读地图.md。
 */
#include "board.h"
#include "app_config.h"
#include "features.h"
#include "boot_journal.h"
#include <stdarg.h>
#include <stdio.h>

UART_HandleTypeDef huart1,huart2;
I2C_HandleTypeDef hi2c1;
ADC_HandleTypeDef hadc1;
static RTC_HandleTypeDef hrtc;
static IWDG_HandleTypeDef hiwdg;
static uint32_t boot_rtc;
static bool log_ready,watchdog_ready;
static volatile bool rtc_alarm_fired;
volatile bool radio_aux_low_seen;
static uint8_t radio_rx_byte;
static volatile uint8_t radio_rx[64],radio_rx_head,radio_rx_tail;
static volatile uint32_t radio_rx_errors;

/* A 无线接收缓冲区：中断收字节、主循环取字节 */
bool board_radio_read_byte(uint8_t *value){
    if(radio_rx_head==radio_rx_tail)return false;
    *value=radio_rx[radio_rx_tail];radio_rx_tail=(uint8_t)((radio_rx_tail+1u)%64u);return true;
}
void board_radio_flush_rx(void){uint32_t mask=__get_PRIMASK();__disable_irq();radio_rx_tail=radio_rx_head;__set_PRIMASK(mask);}
uint32_t board_radio_rx_errors(void){return radio_rx_errors;}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h){
    if(h->Instance!=USART1)return;
    uint8_t next=(uint8_t)((radio_rx_head+1u)%64u);
    if(next==radio_rx_tail)radio_rx_errors++;
    else{radio_rx[radio_rx_head]=radio_rx_byte;radio_rx_head=next;}
    HAL_UART_Receive_IT(h,&radio_rx_byte,1);
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h){
    if(h->Instance==USART1){radio_rx_errors++;HAL_UART_Receive_IT(h,&radio_rx_byte,1);}
}

/* B LED与调试日志 */
void board_led(bool on){HAL_GPIO_WritePin(LED_GPIO,LED_PIN,on?GPIO_PIN_RESET:GPIO_PIN_SET);}
void board_log(const char *fmt,...){
#if APP_LOG_ENABLE
    if(!log_ready)return;
    char buffer[220];va_list args;va_start(args,fmt);int n=vsnprintf(buffer,sizeof buffer,fmt,args);va_end(args);
    if(n>0){uint16_t size=(uint16_t)(n<(int)sizeof buffer?n:(int)sizeof buffer-1);HAL_UART_Transmit(&huart2,(uint8_t*)buffer,size,400);}
#else
    (void)fmt;
#endif
}
/* C 看门狗和致命错误处理 */
void board_watchdog_feed(void){if(watchdog_ready)HAL_IWDG_Refresh(&hiwdg);}
void board_fatal(const char *why){
    board_log("FATAL,%s\r\n",why);
    while(1){board_led(true);HAL_Delay(80);board_led(false);HAL_Delay(920);}
}
static void check(HAL_StatusTypeDef result,const char *name){if(result!=HAL_OK)board_fatal(name);}
/* D 8MHz HSI时钟与微秒延时 */
static void clocks(void){
    RCC_OscInitTypeDef o={0};o.OscillatorType=RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
    o.HSIState=RCC_HSI_ON;o.HSICalibrationValue=RCC_HSICALIBRATION_DEFAULT;o.LSIState=RCC_LSI_ON;o.PLL.PLLState=RCC_PLL_OFF;
    check(HAL_RCC_OscConfig(&o),"HSI_LSI");
    RCC_ClkInitTypeDef c={0};c.ClockType=RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource=RCC_SYSCLKSOURCE_HSI;c.AHBCLKDivider=RCC_SYSCLK_DIV1;c.APB1CLKDivider=RCC_HCLK_DIV1;c.APB2CLKDivider=RCC_HCLK_DIV1;
    check(HAL_RCC_ClockConfig(&c,FLASH_LATENCY_0),"clock");
    __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV2);
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;DWT->CYCCNT=0;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
}
void board_delay_us(uint32_t us){uint32_t start=DWT->CYCCNT,ticks=us*(SystemCoreClock/1000000u);while((uint32_t)(DWT->CYCCNT-start)<ticks){}}
/* E 引脚初始状态与SWD保留 */
static void gpio(void){
    __HAL_RCC_AFIO_CLK_ENABLE();__HAL_RCC_GPIOA_CLK_ENABLE();__HAL_RCC_GPIOB_CLK_ENABLE();__HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG(); /* Keep PA13/PA14 SWD available. */
    GPIO_InitTypeDef g={0};
    /* Floating unused pins become analog; required pins are configured below. */
    g.Mode=GPIO_MODE_ANALOG;g.Pin=GPIO_PIN_All & ~(GPIO_PIN_13|GPIO_PIN_14);HAL_GPIO_Init(GPIOA,&g);
    g.Pin=GPIO_PIN_All;HAL_GPIO_Init(GPIOB,&g);HAL_GPIO_Init(GPIOC,&g);
    board_led(false);g.Pin=LED_PIN;g.Mode=GPIO_MODE_OUTPUT_PP;g.Speed=GPIO_SPEED_FREQ_LOW;HAL_GPIO_Init(LED_GPIO,&g);
    HAL_GPIO_WritePin(TEMP_GPIO,TEMP_PIN,GPIO_PIN_SET);g.Pin=TEMP_PIN;g.Mode=GPIO_MODE_OUTPUT_OD;HAL_GPIO_Init(TEMP_GPIO,&g);
    HAL_GPIO_WritePin(GPIOB,RADIO_M0_PIN|RADIO_M1_PIN,GPIO_PIN_SET);
    g.Pin=RADIO_M0_PIN|RADIO_M1_PIN;g.Mode=GPIO_MODE_OUTPUT_PP;HAL_GPIO_Init(GPIOB,&g);
    g.Pin=RADIO_AUX_PIN;g.Mode=GPIO_MODE_IT_RISING_FALLING;g.Pull=GPIO_NOPULL;HAL_GPIO_Init(RADIO_AUX_GPIO,&g);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn,5,0);HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}
/* F 两路串口初始化：USART1无线、USART2调试 */
static void uart_init(UART_HandleTypeDef *h,USART_TypeDef *instance,uint32_t baud){
    h->Instance=instance;h->Init.BaudRate=baud;h->Init.WordLength=UART_WORDLENGTH_8B;h->Init.StopBits=UART_STOPBITS_1;
    h->Init.Parity=UART_PARITY_NONE;h->Init.Mode=UART_MODE_TX_RX;h->Init.HwFlowCtl=UART_HWCONTROL_NONE;h->Init.OverSampling=UART_OVERSAMPLING_16;
    check(HAL_UART_Init(h),"UART");
}
void HAL_UART_MspInit(UART_HandleTypeDef *h){
    GPIO_InitTypeDef g={0};g.Mode=GPIO_MODE_AF_PP;g.Speed=GPIO_SPEED_FREQ_LOW;
    if(h->Instance==USART1){__HAL_RCC_USART1_CLK_ENABLE();HAL_NVIC_SetPriority(USART1_IRQn,6,0);HAL_NVIC_EnableIRQ(USART1_IRQn);g.Pin=GPIO_PIN_9;HAL_GPIO_Init(GPIOA,&g);g.Pin=GPIO_PIN_10;}
    else{__HAL_RCC_USART2_CLK_ENABLE();g.Pin=GPIO_PIN_2;HAL_GPIO_Init(GPIOA,&g);g.Pin=GPIO_PIN_3;}
    g.Mode=GPIO_MODE_INPUT;g.Pull=GPIO_NOPULL;HAL_GPIO_Init(GPIOA,&g);
}
/* G I2C初始化与SDA锁死恢复 */
void HAL_I2C_MspInit(I2C_HandleTypeDef *h){(void)h;__HAL_RCC_I2C1_CLK_ENABLE();GPIO_InitTypeDef g={0};g.Pin=GPIO_PIN_6|GPIO_PIN_7;g.Mode=GPIO_MODE_AF_OD;g.Speed=GPIO_SPEED_FREQ_LOW;HAL_GPIO_Init(GPIOB,&g);}
static void i2c_init(void){
    hi2c1.Instance=I2C1;hi2c1.Init.ClockSpeed=100000;hi2c1.Init.DutyCycle=I2C_DUTYCYCLE_2;
    hi2c1.Init.AddressingMode=I2C_ADDRESSINGMODE_7BIT;hi2c1.Init.DualAddressMode=I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode=I2C_GENERALCALL_DISABLE;hi2c1.Init.NoStretchMode=I2C_NOSTRETCH_DISABLE;
    check(HAL_I2C_Init(&hi2c1),"I2C");
}
bool board_i2c_recover(void){
    /* 从设备中途掉电可能锁住SDA：释放线路、最多送9个时钟，再产生STOP。 */
    HAL_I2C_DeInit(&hi2c1);
    GPIO_InitTypeDef g={0};g.Pin=GPIO_PIN_6|GPIO_PIN_7;g.Mode=GPIO_MODE_OUTPUT_OD;g.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(GPIOB,g.Pin,GPIO_PIN_SET);HAL_GPIO_Init(GPIOB,&g);
    for(unsigned pulse=0;pulse<9 && HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_7)==GPIO_PIN_RESET;pulse++){
        HAL_GPIO_WritePin(GPIOB,GPIO_PIN_6,GPIO_PIN_RESET);board_delay_us(5);
        HAL_GPIO_WritePin(GPIOB,GPIO_PIN_6,GPIO_PIN_SET);board_delay_us(5);
    }
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_6|GPIO_PIN_7,GPIO_PIN_RESET);board_delay_us(5);
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_6,GPIO_PIN_SET);board_delay_us(5);
    bool scl=HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_6)==GPIO_PIN_SET;
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_7,GPIO_PIN_SET);board_delay_us(5);
    bool idle=scl && HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_7)==GPIO_PIN_SET;
    /* 重新走初始化回调，把GPIO从普通开漏恢复为I2C复用开漏。 */
    hi2c1.State=HAL_I2C_STATE_RESET;
    bool initialized=HAL_I2C_Init(&hi2c1)==HAL_OK;
    return initialized && idle;
}
/* H ADC校准与储能电压测量 */
void HAL_ADC_MspInit(ADC_HandleTypeDef *h){(void)h;__HAL_RCC_ADC1_CLK_ENABLE();GPIO_InitTypeDef g={0};g.Pin=GPIO_PIN_0;g.Mode=GPIO_MODE_ANALOG;HAL_GPIO_Init(GPIOA,&g);}
static void adc_init(void){
    hadc1.Instance=ADC1;hadc1.Init.ScanConvMode=ADC_SCAN_DISABLE;hadc1.Init.ContinuousConvMode=DISABLE;
    hadc1.Init.DiscontinuousConvMode=DISABLE;hadc1.Init.ExternalTrigConv=ADC_SOFTWARE_START;hadc1.Init.DataAlign=ADC_DATAALIGN_RIGHT;hadc1.Init.NbrOfConversion=1;
    check(HAL_ADC_Init(&hadc1),"ADC");check(HAL_ADCEx_Calibration_Start(&hadc1),"ADC_calibration");
}
static bool adc_read(uint32_t channel,uint16_t *value){
    ADC_ChannelConfTypeDef c={0};c.Channel=channel;c.Rank=ADC_REGULAR_RANK_1;c.SamplingTime=ADC_SAMPLETIME_239CYCLES_5;
    if(HAL_ADC_ConfigChannel(&hadc1,&c)!=HAL_OK)return false;
    if(channel==ADC_CHANNEL_VREFINT)HAL_Delay(1);
    if(HAL_ADC_Start(&hadc1)!=HAL_OK)return false;
    bool ok=HAL_ADC_PollForConversion(&hadc1,10)==HAL_OK;
    if(ok)*value=(uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);return ok;
}
bool board_measure_store(uint16_t *mv,uint16_t *vdda){
    uint32_t s=0,r=0;uint16_t value;
    bool ok=true;
    for(unsigned i=0;i<8;i++){
        if(!adc_read(ADC_CHANNEL_0,&value)){ok=false;break;}s+=value;
        if(!adc_read(ADC_CHANNEL_VREFINT,&value)){ok=false;break;}r+=value;
    }
    CLEAR_BIT(ADC1->CR2,ADC_CR2_TSVREFE);
    if(!ok)return false;
    return store_convert((uint16_t)(s/8),(uint16_t)(r/8),mv,vdda);
}
/* I RTC时间、定时唤醒与STOP休眠 */
uint32_t board_rtc_seconds(void){uint32_t high,low;do{high=RTC->CNTH;low=RTC->CNTL;}while(high!=RTC->CNTH);return (high<<16)|low;}
uint32_t board_uptime(void){return board_rtc_seconds()-boot_rtc;}
static bool rtc_ready(uint32_t flag){uint32_t t=HAL_GetTick();while(!(RTC->CRL&flag)){if(HAL_GetTick()-t>1000)return false;}return true;}
static void rtc_init(void){
    __HAL_RCC_PWR_CLK_ENABLE();__HAL_RCC_BKP_CLK_ENABLE();HAL_PWR_EnableBkUpAccess();
    RCC_PeriphCLKInitTypeDef p={0};p.PeriphClockSelection=RCC_PERIPHCLK_RTC;p.RTCClockSelection=RCC_RTCCLKSOURCE_LSI;
    check(HAL_RCCEx_PeriphCLKConfig(&p),"RTC_clock");__HAL_RCC_RTC_ENABLE();
    hrtc.Instance=RTC;hrtc.Init.AsynchPrediv=RTC_LSI_HZ-1;hrtc.Init.OutPut=RTC_OUTPUTSOURCE_NONE;
    check(HAL_RTC_Init(&hrtc),"RTC_init");
    boot_rtc=board_rtc_seconds();
    __HAL_RTC_ALARM_EXTI_ENABLE_IT();__HAL_RTC_ALARM_EXTI_ENABLE_RISING_EDGE();
    HAL_NVIC_SetPriority(RTC_Alarm_IRQn,4,0);HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);
}
static bool rtc_alarm(uint32_t at){
    if(!rtc_ready(RTC_CRL_RTOFF))return false;
    RTC->CRL|=RTC_CRL_CNF;RTC->ALRH=(at>>16)&0xffffu;RTC->ALRL=at&0xffffu;RTC->CRL&=~RTC_CRL_CNF;
    if(!rtc_ready(RTC_CRL_RTOFF))return false;
    rtc_alarm_fired=false;RTC->CRL&=~RTC_CRL_ALRF;__HAL_RTC_ALARM_EXTI_CLEAR_FLAG();RTC->CRH|=RTC_CRH_ALRIE;
    return true;
}
void board_wait_seconds(uint32_t seconds,bool stop){
    uint32_t start=board_rtc_seconds();
    while(1){
        uint32_t elapsed=board_rtc_seconds()-start;
        if(elapsed>=seconds)break;
        uint32_t left=seconds-elapsed,chunk=left>5?5:left;
        board_watchdog_feed();
        if(stop){
            uint32_t until=board_rtc_seconds()+chunk;
            if(!rtc_alarm(until))board_fatal("RTC_alarm");
            HAL_SuspendTick();
            /* Mask IRQs across the condition/WFI boundary so a pending alarm cannot be lost. */
            while(!rtc_alarm_fired && (int32_t)(board_rtc_seconds()-until)<0){
                __disable_irq();
                if(!rtc_alarm_fired && (int32_t)(board_rtc_seconds()-until)<0)HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON,PWR_STOPENTRY_WFI);
                __enable_irq();
            }
            HAL_ResumeTick();
            /* STOP wakes on HSI; this project already runs on 8 MHz HSI. */
            SystemCoreClockUpdate();RTC->CRH&=~RTC_CRH_ALRIE;
        }else HAL_Delay(chunk*1000u);
    }
    board_watchdog_feed();
}
/* J 启动计数的Flash擦写接口 */
static bool flash_erase(const uint32_t *page){FLASH_EraseInitTypeDef e={0};uint32_t error=0;e.TypeErase=FLASH_TYPEERASE_PAGES;e.PageAddress=(uint32_t)page;e.NbPages=1;return HAL_FLASHEx_Erase(&e,&error)==HAL_OK;}
static bool flash_program(const uint32_t *address,uint32_t value){return HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,(uint32_t)address,value)==HAL_OK;}
bool board_next_boot(uint32_t *counter){
    uint16_t store,vdda;if(!board_measure_store(&store,&vdda)||vdda<3000)return false;
    boot_storage_t s={{(const uint32_t*)0x0800f800u,(const uint32_t*)0x0800fc00u},flash_erase,flash_program};
    if(HAL_FLASH_Unlock()!=HAL_OK)return false;
    bool ok=boot_journal_next(&s,counter);HAL_FLASH_Lock();return ok;
}
/* K 上电总初始化入口：由main首先调用 */
void board_init(void){
    HAL_Init();clocks();gpio();uart_init(&huart2,USART2,DEBUG_UART_BAUD);log_ready=true;
    board_log("\r\nRAILNODE,HSI_8MHz,stage=%d,lowpower=%d,enforce_energy=%d,adaptive=%d\r\n",APP_STAGE,APP_LOW_POWER,APP_ENFORCE_ENERGY,APP_ADAPTIVE);
    adc_init();rtc_init();
    if(APP_STAGE>0)i2c_init();
    if(APP_STAGE>=2){uart_init(&huart1,USART1,RADIO_UART_BAUD);check(HAL_UART_Receive_IT(&huart1,&radio_rx_byte,1),"UART_RX_IT");}
#if APP_WATCHDOG
    hiwdg.Instance=IWDG;hiwdg.Init.Prescaler=IWDG_PRESCALER_256;hiwdg.Init.Reload=4095;
    check(HAL_IWDG_Init(&hiwdg),"IWDG");watchdog_ready=true;
#endif
    board_log("RESET_FLAGS,%08lx\r\n",(unsigned long)RCC->CSR);
    __HAL_RCC_CLEAR_RESET_FLAGS();
}
/* L 中断处理入口：系统节拍、UART、AUX和RTC */
void SysTick_Handler(void){HAL_IncTick();}
void USART1_IRQHandler(void){HAL_UART_IRQHandler(&huart1);}
void EXTI15_10_IRQHandler(void){HAL_GPIO_EXTI_IRQHandler(RADIO_AUX_PIN);}
void HAL_GPIO_EXTI_Callback(uint16_t pin){if(pin==RADIO_AUX_PIN && HAL_GPIO_ReadPin(RADIO_AUX_GPIO,RADIO_AUX_PIN)==GPIO_PIN_RESET)radio_aux_low_seen=true;}
void RTC_Alarm_IRQHandler(void){
    if(RTC->CRL&RTC_CRL_ALRF){RTC->CRL&=~RTC_CRL_ALRF;rtc_alarm_fired=true;}
    __HAL_RTC_ALARM_EXTI_CLEAR_FLAG();
}
void HardFault_Handler(void){NVIC_SystemReset();}
void MemManage_Handler(void){NVIC_SystemReset();}
void BusFault_Handler(void){NVIC_SystemReset();}
void UsageFault_Handler(void){NVIC_SystemReset();}
