#include "board.h"
#include "app_config.h"
#include "energy_policy.h"
#include "packet.h"
#include "sensors.h"
#include "radio.h"
#include <limits.h>

int main(void){
    board_init();
    if(APP_STAGE==0){
        uint32_t count=0;
        while(1){board_led(true);board_log("HELLO,%lu\r\n",(unsigned long)count++);HAL_Delay(80);board_led(false);HAL_Delay(920);board_watchdog_feed();}
    }
    uint32_t boots=0;
    bool persistent=board_next_boot(&boots);
    if(!persistent){
        /* Backup domain survives reset, not full power loss: mark the session degraded. */
        boots=(uint32_t)HAL_RTCEx_BKUPRead(NULL,RTC_BKP_DR1)+1u;
        HAL_RTCEx_BKUPWrite(NULL,RTC_BKP_DR1,(uint16_t)boots);
    }
    board_log("BOOT,%lu,persistent=%u\r\n",(unsigned long)boots,persistent);
    if(!APP_ENFORCE_ENERGY)board_log("BENCH_EXTERNAL_POWER,energy_gating_disabled\r\n");
    if(APP_STAGE>=2)board_log("RADIO,paired_E22_400T22D_9600_8N1,configure_pair_before_use\r\n");
    uint16_t sequence=0;
    energy_state_t state=ENERGY_PROTECT;
    while(1){
        board_watchdog_feed();uint32_t started=board_rtc_seconds();
        telemetry_t t={0};t.node=NODE_ID;t.boot=(uint16_t)boots;t.seq=sequence;t.uptime_s=board_uptime();
        t.reset_count=boots>UINT16_MAX?UINT16_MAX:(uint16_t)boots;
        uint16_t vdda=0;bool voltage_ok=board_measure_store(&t.store_mv,&vdda);
        if(voltage_ok)t.flags|=FLAG_VOLT_VALID;
        energy_action_t action=energy_update(state,t.store_mv,voltage_ok,APP_ADAPTIVE);
        if(!APP_ENFORCE_ENERGY)action=energy_update(ENERGY_NORMAL,4500,true,false);
        state=action.state;t.state=(uint8_t)state;
        if(state!=ENERGY_PROTECT){
            if(temperature_read(&t.temp_centi))t.flags|=FLAG_TEMP_VALID;
            if(action.allow_vibration && vibration_read(&t.vibration_mg))t.flags|=FLAG_VIB_VALID;
        }
        if(((t.flags&FLAG_TEMP_VALID) && t.temp_centi>=TEMP_ALARM_CENTI)||
           ((t.flags&FLAG_VIB_VALID) && t.vibration_mg>=VIBRATION_ALARM_MG)||!persistent)t.flags|=FLAG_ALARM;
        /* A measurement window can exhaust a small store; check again before RF load. */
        if(APP_ENFORCE_ENERGY){
            voltage_ok=board_measure_store(&t.store_mv,&vdda);
            if(!voltage_ok)t.flags&=(uint8_t)~FLAG_VOLT_VALID;
            else t.flags|=FLAG_VOLT_VALID;
            action=energy_update(state,t.store_mv,voltage_ok,APP_ADAPTIVE);state=action.state;t.state=(uint8_t)state;
        }
        bool tx_ok=false,attempt=false;
        if(APP_STAGE>=2 && action.allow_radio){
            uint8_t frame[PACKET_SIZE];packet_encode(&t,frame);attempt=true;tx_ok=radio_send(frame,sizeof frame);sequence++;
        }
        board_log("SAMPLE,up=%lu,boot=%u,seq=%u,mV=%u,vdda=%u,temp_centi=%d,vib_mg=%u,state=%u,flags=%02x,tx_attempt=%u,aux_done=%u\r\n",
            (unsigned long)t.uptime_s,t.boot,t.seq,t.store_mv,vdda,t.temp_centi,t.vibration_mg,t.state,t.flags,attempt,tx_ok);
        if(!APP_LOW_POWER){board_led(true);HAL_Delay(30);board_led(false);}
        uint32_t period=APP_STAGE==1?2u:action.period_s,elapsed=board_rtc_seconds()-started;
        if(elapsed<period)board_wait_seconds(period-elapsed,APP_LOW_POWER);
    }
}
