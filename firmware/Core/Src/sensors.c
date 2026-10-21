#include "sensors.h"
#include "board.h"
#include "app_config.h"
#include "features.h"

static void line(bool high){HAL_GPIO_WritePin(TEMP_GPIO,TEMP_PIN,high?GPIO_PIN_SET:GPIO_PIN_RESET);}
static bool level(void){return HAL_GPIO_ReadPin(TEMP_GPIO,TEMP_PIN)==GPIO_PIN_SET;}
static bool ow_reset(void){
    line(false);board_delay_us(480);line(true);
    uint32_t mask=__get_PRIMASK();__disable_irq();board_delay_us(70);bool present=!level();__set_PRIMASK(mask);
    board_delay_us(410);return present && level();
}
static void ow_write_bit(bool bit){
    uint32_t mask=__get_PRIMASK();__disable_irq();line(false);
    board_delay_us(bit?6:60);line(true);board_delay_us(bit?64:10);__set_PRIMASK(mask);
}
static bool ow_read_bit(void){
    uint32_t mask=__get_PRIMASK();__disable_irq();line(false);board_delay_us(3);line(true);
    board_delay_us(10);bool bit=level();board_delay_us(57);__set_PRIMASK(mask);return bit;
}
static void ow_write(uint8_t v){for(unsigned b=0;b<8;b++){ow_write_bit(v&1u);v>>=1;}}
static uint8_t ow_read(void){uint8_t v=0;for(unsigned b=0;b<8;b++)if(ow_read_bit())v|=(uint8_t)(1u<<b);return v;}
bool temperature_read(int16_t *centi){
    /* Single sensor, external 3.3V supply; parasite power is unsupported. */
    if(!ow_reset())return false;
    ow_write(0xcc);ow_write(0x4e);ow_write(75);ow_write(70);ow_write(0x3f);
    if(!ow_reset())return false;
    ow_write(0xcc);ow_write(0x44);
    uint32_t start=HAL_GetTick();
    while(!ow_read_bit()){
        if(HAL_GetTick()-start>TEMP_CONVERT_TIMEOUT_MS)return false;
        board_watchdog_feed();HAL_Delay(1);
    }
    if(!ow_reset())return false;
    ow_write(0xcc);ow_write(0xbe);
    uint8_t scratch[9];for(unsigned i=0;i<9;i++)scratch[i]=ow_read();
    /* Verify configuration too: catches sensors that did not accept 10-bit setup. */
    if((scratch[4]&0x60u)!=0x20u)return false;
    return ds18b20_decode(scratch,centi);
}
static bool reg_read(uint8_t reg,uint8_t *data,uint16_t n){return HAL_I2C_Mem_Read(&hi2c1,ADXL_ADDRESS,reg,I2C_MEMADD_SIZE_8BIT,data,n,30)==HAL_OK;}
static bool reg_write(uint8_t reg,uint8_t value){return HAL_I2C_Mem_Write(&hi2c1,ADXL_ADDRESS,reg,I2C_MEMADD_SIZE_8BIT,&value,1,30)==HAL_OK;}
bool vibration_read(uint16_t *rms_mg){
    uint8_t id;bool ok=false;
    if(!reg_read(0,&id,1)||id!=0xe5)return false;
    if(!reg_write(0x2d,0)||!reg_write(0x31,0x08)||!reg_write(0x2c,0x0a)||!reg_write(0x2d,0x08))goto done;
    vibration_window_t window;vibration_reset(&window);
    uint32_t start=HAL_GetTick();
    /* Discard the first data sample after waking the sensor. */
    bool first=true;
    while(window.count<VIBRATION_SAMPLES){
        if(HAL_GetTick()-start>VIBRATION_TIMEOUT_MS)goto done;
        uint8_t status;
        if(!reg_read(0x30,&status,1))goto done;
        if(status&0x80u){
            uint8_t b[6];if(!reg_read(0x32,b,6))goto done;
            if(first){first=false;continue;}
            int16_t x=(int16_t)((uint16_t)b[0]|((uint16_t)b[1]<<8));
            int16_t y=(int16_t)((uint16_t)b[2]|((uint16_t)b[3]<<8));
            int16_t z=(int16_t)((uint16_t)b[4]|((uint16_t)b[5]<<8));
            if(!vibration_add(&window,x,y,z))goto done;
        }else HAL_Delay(1);
        board_watchdog_feed();
    }
    *rms_mg=vibration_rms_mg(&window);ok=true;
done:
    if(!reg_write(0x2d,0))ok=false;
    return ok;
}
