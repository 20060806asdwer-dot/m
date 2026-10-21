#include "radio.h"
#include "board.h"
#include "app_config.h"

void radio_sleep(void){
    HAL_GPIO_WritePin(RADIO_M0_GPIO,RADIO_M0_PIN,GPIO_PIN_SET);
    HAL_GPIO_WritePin(RADIO_M1_GPIO,RADIO_M1_PIN,GPIO_PIN_SET);
}
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
bool radio_send(const uint8_t *data,size_t n){
    bool ok=false;
    if(!data || n==0 || n>64)return false;
    /* E22-400T22D: M1/M0=00 normal, 11 deep sleep; configure the pair separately. */
    HAL_GPIO_WritePin(RADIO_M0_GPIO,RADIO_M0_PIN,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RADIO_M1_GPIO,RADIO_M1_PIN,GPIO_PIN_RESET);
    if(!aux_ready(RADIO_READY_TIMEOUT_MS,false))goto done;
    radio_aux_low_seen=false;
    if(HAL_UART_Transmit(&huart1,(uint8_t*)data,(uint16_t)n,500)!=HAL_OK)goto done;
    ok=aux_ready(RADIO_TX_TIMEOUT_MS,true);
done:
    radio_sleep();return ok;
}
