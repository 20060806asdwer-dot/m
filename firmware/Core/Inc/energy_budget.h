/* 【07 能量账本接口】
 * 阅读导航：docs/代码阅读地图.md。
 */
/* 模块：能量账本——估算可用能量、输入功率和下一轮间隔。 */
#ifndef ENERGY_BUDGET_H
#define ENERGY_BUDGET_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint32_t available_mj,cycle_cost_mj,previous_energy_mj,previous_s;
    uint32_t harvest_uw;
    bool previous_valid,estimate_valid;
} energy_budget_t;
uint32_t capacitor_energy_mj(uint16_t mv);
void budget_begin(energy_budget_t *b,uint16_t mv,bool valid,uint32_t now_s);
void budget_refresh(energy_budget_t *b,uint16_t mv,bool valid);
bool budget_allow(const energy_budget_t *b,uint32_t cost_mj);
void budget_charge(energy_budget_t *b,uint32_t cost_mj);
uint32_t budget_period(const energy_budget_t *b,uint32_t base_s,uint32_t next_cost_mj);
#endif
