/* 【19 无线测试场景：正确/错误ACK、重试与线路失败】
 * 阅读导航：docs/代码阅读地图.md。
 */
/* 测试：正常ACK、错误ACK、重试成功、线路超时与UART失败。 */
#include "board.h"
#include "app_config.h"
#include "packet.h"
UART_HandleTypeDef huart1;
volatile bool radio_aux_low_seen;
static uint32_t tick,busy_until,ack_at;
static unsigned mode,sends,ack_used;
static uint8_t ack[ACK_SIZE];
static uint16_t mode_pins;
void fake_radio_reset(unsigned selected){mode=selected;sends=0;tick=0;busy_until=0;ack_at=0;ack_used=ACK_SIZE;mode_pins=RADIO_M0_PIN|RADIO_M1_PIN;radio_aux_low_seen=false;}
unsigned fake_radio_sends(void){return sends;}
bool fake_radio_sleeping(void){return mode_pins==(RADIO_M0_PIN|RADIO_M1_PIN);}
void HAL_GPIO_WritePin(int port,uint16_t pin,GPIO_PinState value){(void)port;if(value==GPIO_PIN_SET)mode_pins|=pin;else mode_pins&=(uint16_t)~pin;}
GPIO_PinState HAL_GPIO_ReadPin(int port,uint16_t pin){(void)port;(void)pin;return mode==3 || tick<busy_until?GPIO_PIN_RESET:GPIO_PIN_SET;}
uint32_t HAL_GetTick(void){return tick;}
void HAL_Delay(uint32_t ms){tick+=ms;}
void board_watchdog_feed(void){}
void board_radio_flush_rx(void){ack_used=ACK_SIZE;}
bool board_radio_read_byte(uint8_t *value){
    if(tick<ack_at || ack_used>=ACK_SIZE)return false;
    *value=ack[ack_used++];return true;
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h,uint8_t *data,uint16_t n,uint32_t timeout){
    (void)h;(void)timeout;sends++;
    if(mode==4)return HAL_ERROR;
    if(n!=PACKET_SIZE)return HAL_ERROR;
    if(mode!=5){radio_aux_low_seen=true;busy_until=tick+5;}
    uint16_t boot=(uint16_t)data[4]|((uint16_t)data[5]<<8),seq=(uint16_t)data[6]|((uint16_t)data[7]<<8);
    packet_ack_encode(data[3],boot,seq,ack);
    if(mode==1 || (mode==2 && sends==1))ack[8]^=1;
    if(mode==6)packet_ack_encode(data[3],boot,(uint16_t)(seq+1),ack);
    ack_at=tick+10;ack_used=0;return HAL_OK;
}
