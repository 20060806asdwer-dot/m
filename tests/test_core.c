/* 【19 ARM核心与无线驱动测试：使用假的时间和外设】
 * 阅读导航：docs/代码阅读地图.md。
 */
#include "energy_policy.h"
#include "packet.h"
#include "features.h"
#include "boot_journal.h"
#include "energy_budget.h"
#include "radio.h"
#include "app_config.h"
#include "board.h"
#include <stdint.h>

volatile uint32_t test_checks,test_failed_line;
uint8_t test_frame[PACKET_SIZE];
uint8_t test_ack[ACK_SIZE];
static uint32_t pages[2][BOOT_PAGE_WORDS];
static int writes_left;
#define CHECK(expr) do {test_checks++;if(!(expr)){test_failed_line=__LINE__;goto failed;}}while(0)
__attribute__((noinline,noreturn)) void tests_finished(void){while(1)__asm volatile("nop");}
static bool erase_page(const uint32_t *p){for(unsigned i=0;i<BOOT_PAGE_WORDS;i++)((uint32_t*)p)[i]=UINT32_MAX;return true;}
static bool program_word(const uint32_t *p,uint32_t value){
    if(writes_left==0)return false;
    if(writes_left>0)writes_left--;
    if((*p&value)!=value)return false;
    *(uint32_t*)p=value;return true;
}
void test_main(void){
    CHECK(crc16_ccitt((const uint8_t*)"123456789",9)==0x29b1);
    telemetry_t record={.node=1,.boot=7,.seq=65535,.uptime_s=123456,.temp_centi=-1234,
      .store_mv=3900,.vibration_mg=27,.state=2,.flags=7,.reset_count=3};
    packet_encode(&record,test_frame);
    CHECK(test_frame[0]==0xa5 && test_frame[1]==0x5a && test_frame[12]==0x2e && test_frame[13]==0xfb);
    packet_ack_encode(record.node,record.boot,record.seq,test_ack);
    CHECK(packet_ack_matches(test_ack,1,7,65535));
    CHECK(!packet_ack_matches(test_ack,2,7,65535));
    CHECK(!packet_ack_matches(test_ack,1,8,65535));
    CHECK(!packet_ack_matches(test_ack,1,7,0));
    test_ack[9]^=1;CHECK(!packet_ack_matches(test_ack,1,7,65535));test_ack[9]^=1;
    fake_radio_reset(0);CHECK(radio_send(test_frame,PACKET_SIZE));CHECK(radio_last_result()==RADIO_OK && fake_radio_sends()==1 && fake_radio_sleeping());
    fake_radio_reset(1);CHECK(!radio_send(test_frame,PACKET_SIZE));CHECK(radio_last_result()==RADIO_ACK_TIMEOUT && fake_radio_sends()==2 && fake_radio_sleeping());
    fake_radio_reset(2);CHECK(radio_send(test_frame,PACKET_SIZE));CHECK(radio_last_attempts()==2 && fake_radio_sleeping());
    fake_radio_reset(3);CHECK(!radio_send(test_frame,PACKET_SIZE));CHECK(radio_last_result()==RADIO_READY_TIMEOUT && fake_radio_sends()==0 && fake_radio_sleeping());
    fake_radio_reset(4);CHECK(!radio_send(test_frame,PACKET_SIZE));CHECK(radio_last_result()==RADIO_UART_ERROR && fake_radio_sleeping());
    fake_radio_reset(5);CHECK(!radio_send(test_frame,PACKET_SIZE));CHECK(radio_last_result()==RADIO_AUX_TIMEOUT && fake_radio_sleeping());
    fake_radio_reset(6);CHECK(!radio_send(test_frame,PACKET_SIZE));CHECK(radio_last_result()==RADIO_ACK_TIMEOUT && fake_radio_sleeping());
    CHECK(!radio_send(0,PACKET_SIZE) && radio_last_result()==RADIO_BAD_ARGUMENT);
    static energy_budget_t budget;
    CHECK(capacitor_energy_mj(4000)==8000);
    budget_begin(&budget,4000,true,0);CHECK(!budget.estimate_valid && budget.available_mj==1339);
    CHECK(budget_allow(&budget,1239) && !budget_allow(&budget,1240));
    budget_charge(&budget,160);budget_begin(&budget,4000,true,15);
    CHECK(budget.estimate_valid && budget.harvest_uw==10866);
    CHECK(budget_period(&budget,15,165)==16);
    budget.harvest_uw=SLEEP_POWER_UW;CHECK(budget_period(&budget,15,165)==MAX_ADAPTIVE_PERIOD_S);
    budget_refresh(&budget,3600,true);CHECK(!budget_allow(&budget,1));
    budget_refresh(&budget,4000,false);CHECK(budget.available_mj==0);
    budget_begin(&budget,4000,false,30);CHECK(!budget.estimate_valid);
    budget_begin(&budget,4000,true,45);CHECK(!budget.estimate_valid);
    CHECK(energy_update(ENERGY_NORMAL,3649,true,true).state==ENERGY_PROTECT);
    CHECK(!energy_update(ENERGY_NORMAL,4000,false,true).allow_radio);
    CHECK(energy_update(ENERGY_PROTECT,3799,true,true).state==ENERGY_PROTECT);
    CHECK(energy_update(ENERGY_PROTECT,3800,true,true).state==ENERGY_LOW);
    CHECK(!energy_update(ENERGY_LOW,3900,true,true).allow_vibration);
    CHECK(energy_update(ENERGY_LOW,3949,true,true).period_s==60);
    CHECK(energy_update(ENERGY_LOW,3950,true,true).state==ENERGY_NORMAL);
    CHECK(energy_update(ENERGY_NORMAL,3850,true,true).state==ENERGY_NORMAL);
    CHECK(energy_update(ENERGY_NORMAL,3849,true,true).state==ENERGY_LOW);
    CHECK(energy_update(ENERGY_PROTECT,4000,true,false).period_s==15);
    CHECK(energy_update(ENERGY_NORMAL,3649,true,false).state==ENERGY_PROTECT);
    CHECK(energy_update((energy_state_t)99,4000,true,true).state==ENERGY_PROTECT);
    vibration_window_t window;vibration_reset(&window);
    for(unsigned i=0;i<64;i++)CHECK(vibration_add(&window,3,-7,256));
    CHECK(vibration_rms_mg(&window)==0);
    vibration_reset(&window);
    for(unsigned i=0;i<64;i++)CHECK(vibration_add(&window,i%2?100:-100,0,256));
    CHECK(vibration_rms_mg(&window)==390);
    vibration_reset(&window);
    CHECK(vibration_add(&window,0,0,0));CHECK(vibration_add(&window,1,0,0));
    CHECK(vibration_rms_mg(&window)==1); /* Fractional count variance must survive scaling. */
    vibration_reset(&window);
    for(unsigned i=0;i<4096;i++)CHECK(vibration_add(&window,i%2?32767:-32768,-32768,32767));
    CHECK(!vibration_add(&window,1,2,3));CHECK(vibration_rms_mg(&window)==65535);
    uint8_t scratch[9]={0x5e,0xff,75,70,0x3f,0xff,0x0c,0x10,0};int16_t temp;
    scratch[8]=crc8_dallas(scratch,8);CHECK(ds18b20_decode(scratch,&temp)&&temp==-1025);
    scratch[4]=0x7f;scratch[8]=crc8_dallas(scratch,8);
    CHECK(ds18b20_decode(scratch,&temp)&&temp==-1012);
    scratch[0]^=1;CHECK(!ds18b20_decode(scratch,&temp));
    for(unsigned i=0;i<9;i++)scratch[i]=0;
    CHECK(!ds18b20_decode(scratch,&temp));
    for(unsigned i=0;i<9;i++)scratch[i]=255;
    CHECK(!ds18b20_decode(scratch,&temp));
    uint16_t store,vdda;
    CHECK(store_convert(2420,1489,&store,&vdda)&&vdda>=3299&&vdda<=3301&&store>=3899&&store<=3902);
    CHECK(store_convert(0,1489,&store,&vdda)&&store==0);
    CHECK(!store_convert(2420,0,&store,&vdda));
    CHECK(!store_convert(4095,1489,&store,&vdda));
    CHECK(!store_convert(2420,500,&store,&vdda));
    boot_storage_t storage={{pages[0],pages[1]},erase_page,program_word};uint32_t boot=0;
    erase_page(pages[0]);erase_page(pages[1]);writes_left=-1;
    CHECK(boot_journal_next(&storage,&boot)&&boot==1);
    writes_left=1;CHECK(!boot_journal_next(&storage,&boot)); /* Torn inverse word. */
    writes_left=-1;CHECK(boot_journal_next(&storage,&boot)&&boot==2);
    for(unsigned i=3;i<=400;i++)CHECK(boot_journal_next(&storage,&boot)&&boot==i);
    erase_page(pages[0]);erase_page(pages[1]);
    pages[0][0]=UINT32_MAX-1;pages[0][1]=1;
    CHECK(boot_journal_next(&storage,&boot)&&boot==1);
    CHECK(boot_journal_next(&storage,&boot)&&boot==2);
failed:
    tests_finished();
}
