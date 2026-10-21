/* 【09 无线驱动：唤醒、发送、确认、有限重试、休眠】
 * 阅读导航：docs/代码阅读地图.md。
 */
#include "radio.h"
#include "board.h"
#include "app_config.h"
#include "packet.h"

static radio_result_t last_result;
static uint32_t last_attempts;
radio_result_t radio_last_result(void){return last_result;}
uint32_t radio_last_attempts(void){return last_attempts;}

/* A 休眠：设置M0/M1模式脚 */
void radio_sleep(void){
    HAL_GPIO_WritePin(RADIO_M0_GPIO,RADIO_M0_PIN,GPIO_PIN_SET);
    HAL_GPIO_WritePin(RADIO_M1_GPIO,RADIO_M1_PIN,GPIO_PIN_SET);
}
/* B AUX握手：检查就绪、忙和稳定空闲 */
static bool aux_ready(uint32_t timeout,bool require_busy){
    uint32_t start=HAL_GetTick(),high_since=0;bool timing=false;
    while(HAL_GetTick()-start<timeout){
        board_watchdog_feed();
        bool high=HAL_GPIO_ReadPin(RADIO_AUX_GPIO,RADIO_AUX_PIN)==GPIO_PIN_SET;
        if(!high){radio_aux_low_seen=true;timing=false;}
        else if(!require_busy || radio_aux_low_seen){
            if(!timing){high_since=HAL_GetTick();timing=true;}
            if(HAL_GetTick()-high_since>=2)return true;
        }
        HAL_Delay(1);
    }
    return false;
}
/* C 接收确认：滑动窗口查找匹配的10字节ACK */
static bool wait_ack(const uint8_t *frame){
    uint8_t buffer[ACK_SIZE];unsigned used=0;uint32_t start=HAL_GetTick();
    uint16_t boot=(uint16_t)frame[4]|((uint16_t)frame[5]<<8),seq=(uint16_t)frame[6]|((uint16_t)frame[7]<<8);
    while(HAL_GetTick()-start<RADIO_ACK_TIMEOUT_MS){
        uint8_t byte;
        if(board_radio_read_byte(&byte)){
            buffer[used++]=byte;
            if(used==ACK_SIZE){
                if(packet_ack_matches(buffer,frame[3],boot,seq))return true;
                for(unsigned i=1;i<ACK_SIZE;i++)buffer[i-1]=buffer[i];
                used--;
            }
        }else HAL_Delay(1);
        board_watchdog_feed();
    }
    return false;
}
/* D 完整发送流程：超时、有限重试、错误原因与最终休眠 */
bool radio_send(const uint8_t *data,size_t n){
    bool ok=false;last_result=RADIO_BAD_ARGUMENT;last_attempts=0;
    if(!data || n==0 || n>64 || (APP_RADIO_ACK && n!=PACKET_SIZE))return false;
    /* E22-400T22D: M1/M0=00 normal, 11 deep sleep; configure the pair separately. */
    HAL_GPIO_WritePin(RADIO_M0_GPIO,RADIO_M0_PIN,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RADIO_M1_GPIO,RADIO_M1_PIN,GPIO_PIN_RESET);
    if(!aux_ready(RADIO_READY_TIMEOUT_MS,false)){last_result=RADIO_READY_TIMEOUT;goto done;}
    for(unsigned retry=0;retry<=(APP_RADIO_ACK?RADIO_RETRIES:0u);retry++){
        board_radio_flush_rx();radio_aux_low_seen=false;last_attempts++;
        if(HAL_UART_Transmit(&huart1,(uint8_t*)data,(uint16_t)n,500)!=HAL_OK){last_result=RADIO_UART_ERROR;break;}
        if(!aux_ready(RADIO_TX_TIMEOUT_MS,true)){last_result=RADIO_AUX_TIMEOUT;break;}
        if(!APP_RADIO_ACK || wait_ack(data)){last_result=RADIO_OK;ok=true;break;}
        last_result=RADIO_ACK_TIMEOUT;
    }
done:
    radio_sleep();return ok;
}
