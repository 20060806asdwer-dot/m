/* 【07 能量账本：任务准入与15至300秒调度】
 * 阅读导航：docs/代码阅读地图.md。
 */
/* 模块：能量账本。功率来自电压变化与任务耗能估算，不是电流传感器实测。 */
#include "energy_budget.h"
#include "app_config.h"
#include <limits.h>

/* A 电容能量：由容量和电压计算毫焦耳 */
uint32_t capacitor_energy_mj(uint16_t mv){
    uint64_t energy=(uint64_t)STORE_CAPACITANCE_MF*mv*mv/2000000u;
    return energy>UINT32_MAX?UINT32_MAX:(uint32_t)energy;
}
/* B 可用能量：扣除保护电压以下不可用的部分 */
void budget_refresh(energy_budget_t *b,uint16_t mv,bool valid){
    uint32_t energy=capacitor_energy_mj(mv),floor=capacitor_energy_mj(STORE_PROTECT_MV);
    b->available_mj=valid && mv<=STORE_VALID_MAX_MV && energy>floor?energy-floor:0;
}
/* C 输入功率估算：上一轮能量变化加估算任务消耗，做平滑 */
void budget_begin(energy_budget_t *b,uint16_t mv,bool valid,uint32_t now_s){
    valid=valid && mv<=STORE_VALID_MAX_MV;
    uint32_t energy=capacitor_energy_mj(mv),dt=now_s-b->previous_s;
    if(valid && b->previous_valid && dt && dt<=3600u){
        int64_t input=((int64_t)energy-b->previous_energy_mj+b->cycle_cost_mj)*1000/(int64_t)dt+SLEEP_POWER_UW;
        if(input<0)input=0;
        if(input>1000000)input=1000000;
        b->harvest_uw=b->estimate_valid?(uint32_t)(((uint64_t)b->harvest_uw*3u+(uint32_t)input)/4u):(uint32_t)input;
        b->estimate_valid=true;
    }else b->estimate_valid=false;
    b->previous_valid=valid;b->previous_energy_mj=energy;b->previous_s=now_s;b->cycle_cost_mj=0;
    budget_refresh(b,mv,valid);
}
/* D 任务准入：给任务和安全余量留够能量 */
bool budget_allow(const energy_budget_t *b,uint32_t cost_mj){
    return b->available_mj>=ENERGY_RESERVE_MJ && cost_mj<=b->available_mj-ENERGY_RESERVE_MJ;
}
/* E 任务记账：记录本轮估算消耗 */
void budget_charge(energy_budget_t *b,uint32_t cost_mj){
    b->available_mj=cost_mj>b->available_mj?0:b->available_mj-cost_mj;
    b->cycle_cost_mj=cost_mj>UINT32_MAX-b->cycle_cost_mj?UINT32_MAX:b->cycle_cost_mj+cost_mj;
}
/* F 调整间隔：输入不足时延长周期，上限300秒 */
uint32_t budget_period(const energy_budget_t *b,uint32_t base_s,uint32_t next_cost_mj){
    if(base_s>MAX_ADAPTIVE_PERIOD_S)base_s=MAX_ADAPTIVE_PERIOD_S;
    if(!b->estimate_valid)return base_s;
    if(b->harvest_uw<=SLEEP_POWER_UW)return MAX_ADAPTIVE_PERIOD_S;
    uint32_t net=b->harvest_uw-SLEEP_POWER_UW;
    uint64_t needed=((uint64_t)next_cost_mj*1000u+net-1u)/net;
    if(needed>MAX_ADAPTIVE_PERIOD_S)return MAX_ADAPTIVE_PERIOD_S;
    return needed<base_s?base_s:(uint32_t)needed;
}
