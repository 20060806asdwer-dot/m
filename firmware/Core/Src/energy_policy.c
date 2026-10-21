#include "energy_policy.h"
#include "app_config.h"
energy_action_t energy_update(energy_state_t previous,uint16_t mv,bool valid,bool adaptive)
{
    energy_action_t a={ENERGY_PROTECT,PROTECT_POLL_S,false,false};
    if(!valid || mv<STORE_PROTECT_MV ||
       (previous!=ENERGY_PROTECT && previous!=ENERGY_LOW && previous!=ENERGY_NORMAL))return a;
    if(previous==ENERGY_PROTECT && mv<STORE_RECOVER_MV)return a;
    if(!adaptive || mv>=STORE_NORMAL_ENTER_MV ||
       (previous==ENERGY_NORMAL && mv>=STORE_NORMAL_EXIT_MV)) {
        a.state=ENERGY_NORMAL;a.period_s=FIXED_PERIOD_S;a.allow_vibration=true;
    } else {a.state=ENERGY_LOW;a.period_s=LOW_PERIOD_S;}
    a.allow_radio=true;
    return a;
}
