/* 【01 主流程：上电、采集、决策、发送、休眠】
 * 阅读导航：docs/代码阅读地图.md。
 */
#include "board.h"
#include "app_config.h"
#include "energy_policy.h"
#include "energy_budget.h"
#include "packet.h"
#include "sensors.h"
#include "radio.h"
#include <limits.h>

int main(void){
    /* 步骤1：初始化主板和外设 */
    board_init();
    /* 步骤2：点亮配置只运行LED与串口教学循环 */
    if(APP_STAGE==0){
        uint32_t count=0;
        while(1){board_led(true);board_log("HELLO,%lu\r\n",(unsigned long)count++);HAL_Delay(80);board_led(false);HAL_Delay(920);board_watchdog_feed();}
    }
    /* 步骤3：读取并更新本次启动编号 */
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
    /* 步骤4：建立状态、序号和能量账本 */
    uint16_t sequence=0;
    energy_state_t state=ENERGY_PROTECT;
    energy_budget_t budget={0};
    const bool use_budget=APP_ADAPTIVE && APP_ENFORCE_ENERGY;
    const uint32_t radio_cost=APP_RADIO_ACK?ACK_RADIO_COST_MJ*(RADIO_RETRIES+1u):RADIO_COST_MJ;
    while(1){
        /* 步骤5：开始新一轮，测量储能并判断能量状态 */
        board_watchdog_feed();uint32_t started=board_rtc_seconds();
        telemetry_t t={0};t.node=NODE_ID;t.boot=(uint16_t)boots;t.seq=sequence;t.uptime_s=board_uptime();
        t.reset_count=boots>UINT16_MAX?UINT16_MAX:(uint16_t)boots;
        uint16_t vdda=0;bool voltage_ok=board_measure_store(&t.store_mv,&vdda);
        if(voltage_ok)t.flags|=FLAG_VOLT_VALID;
        budget_begin(&budget,t.store_mv,voltage_ok,t.uptime_s);
        energy_action_t action=energy_update(state,t.store_mv,voltage_ok,APP_ADAPTIVE);
        if(!APP_ENFORCE_ENERGY)action=energy_update(ENERGY_NORMAL,4500,true,false);
        state=action.state;t.state=(uint8_t)state;
        /* 步骤6：预算允许时采温度与振动 */
        if(state!=ENERGY_PROTECT){
            /* 留足发送预算后再采集，避免先测完却没有能量发出结果。 */
            if(!use_budget || budget_allow(&budget,TEMP_COST_MJ+radio_cost)){
                if(temperature_read(&t.temp_centi))t.flags|=FLAG_TEMP_VALID;
                budget_charge(&budget,TEMP_COST_MJ);
            }
            if(action.allow_vibration && (!use_budget || budget_allow(&budget,VIBRATION_COST_MJ+radio_cost))){
                if(vibration_read(&t.vibration_mg))t.flags|=FLAG_VIB_VALID;
                budget_charge(&budget,VIBRATION_COST_MJ);
            }
        }
        /* 步骤7：标记越限告警与启动记录故障 */
        if(((t.flags&FLAG_TEMP_VALID) && t.temp_centi>=TEMP_ALARM_CENTI)||
           ((t.flags&FLAG_VIB_VALID) && t.vibration_mg>=VIBRATION_ALARM_MG)||!persistent)t.flags|=FLAG_ALARM;
        /* A measurement window can exhaust a small store; check again before RF load. */
        /* 步骤8：发射前复查电压与剩余预算 */
        if(APP_ENFORCE_ENERGY){
            voltage_ok=board_measure_store(&t.store_mv,&vdda);
            if(!voltage_ok)t.flags&=(uint8_t)~FLAG_VOLT_VALID;
            else t.flags|=FLAG_VOLT_VALID;
            action=energy_update(state,t.store_mv,voltage_ok,APP_ADAPTIVE);state=action.state;t.state=(uint8_t)state;
            budget_refresh(&budget,t.store_mv,voltage_ok);
        }
        /* 步骤9：编码发送；确认版等待匹配ACK并有限重试 */
        bool tx_ok=false,attempt=false;
        if(APP_STAGE>=2 && action.allow_radio && (!use_budget || budget_allow(&budget,radio_cost))){
            uint8_t frame[PACKET_SIZE];packet_encode(&t,frame);attempt=true;tx_ok=radio_send(frame,sizeof frame);sequence++;
            budget_charge(&budget,APP_RADIO_ACK?ACK_RADIO_COST_MJ*radio_last_attempts():RADIO_COST_MJ);
        }
        /* 步骤10：输出诊断日志；低功耗配置关闭文字日志 */
        board_log("SAMPLE,up=%lu,boot=%u,seq=%u,mV=%u,vdda=%u,temp_centi=%d,vib_mg=%u,state=%u,flags=%02x,tx_attempt=%u,tx_done=%u\r\n",
            (unsigned long)t.uptime_s,t.boot,t.seq,t.store_mv,vdda,t.temp_centi,t.vibration_mg,t.state,t.flags,attempt,tx_ok);
        if(APP_STAGE>=2)board_log("RADIO,result=%u,attempts=%lu,ack=%u,rx_errors=%lu\r\n",radio_last_result(),(unsigned long)radio_last_attempts(),APP_RADIO_ACK,(unsigned long)board_radio_rx_errors());
        if(!APP_LOW_POWER){board_led(true);HAL_Delay(30);board_led(false);}
        /* 步骤11：确定下轮间隔，等待或STOP休眠 */
        uint32_t period=APP_STAGE==1?2u:action.period_s,elapsed=board_rtc_seconds()-started;
        if(use_budget && state!=ENERGY_PROTECT){
            uint32_t next_cost=TEMP_COST_MJ+radio_cost+(action.allow_vibration?VIBRATION_COST_MJ:0u);
            period=budget_period(&budget,period,next_cost);
        }
        if(elapsed<period)board_wait_seconds(period-elapsed,APP_LOW_POWER);
    }
}
