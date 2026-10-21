#ifndef ENERGY_POLICY_H
#define ENERGY_POLICY_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { ENERGY_PROTECT=0, ENERGY_LOW=1, ENERGY_NORMAL=2 } energy_state_t;
typedef struct {
    energy_state_t state;
    uint32_t period_s;
    bool allow_radio;
    bool allow_vibration;
} energy_action_t;
energy_action_t energy_update(energy_state_t previous,uint16_t mv,bool valid,bool adaptive);
#endif
