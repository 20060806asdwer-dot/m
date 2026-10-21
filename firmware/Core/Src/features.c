/* 【05 数据计算：振动RMS、温度解码与电压换算】
 * 阅读导航：docs/代码阅读地图.md。
 */
#include "features.h"
#include "app_config.h"
#include "packet.h"
#include <limits.h>
/* A 建立三轴采样窗口并累加 */
void vibration_reset(vibration_window_t *w){for(unsigned a=0;a<3;a++){w->sum[a]=0;w->sum_sq[a]=0;}w->count=0;}
bool vibration_add(vibration_window_t *w,int16_t x,int16_t y,int16_t z){
    if(w->count>=4096u)return false;
    const int32_t v[3]={x,y,z};for(unsigned a=0;a<3;a++){w->sum[a]+=v[a];w->sum_sq[a]+=(uint64_t)((int64_t)v[a]*v[a]);}w->count++;return true;
}
/* B 无浮点平方根 */
static uint32_t isqrt64(uint64_t n){uint64_t r=0,bit=(uint64_t)1<<62;while(bit>n)bit>>=2;while(bit){if(n>=r+bit){n-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}return (uint32_t)r;}
/* C 去掉重力/直流均值后计算三轴合成RMS */
uint16_t vibration_rms_mg(const vibration_window_t *w){
    if(w->count<2)return 0;
    uint64_t variance=0,n=w->count;
    for(unsigned a=0;a<3;a++)variance+=w->sum_sq[a]*n-(uint64_t)(w->sum[a]*w->sum[a]);
    /* Full-resolution ADXL345, nominal 3.9mg/LSB; scale before square root. */
    uint64_t divisor=n*n;
    uint64_t scaled=((variance/divisor)*1521u+(variance%divisor)*1521u/divisor)/100u;
    uint32_t rms=isqrt64(scaled);return rms>UINT16_MAX?UINT16_MAX:(uint16_t)rms;
}
/* D 温度CRC、分辨率低位处理和量程校验 */
bool ds18b20_decode(const uint8_t p[9],int16_t *centi){
    uint8_t any=0,all=0xff;for(unsigned i=0;i<9;i++){any|=p[i];all&=p[i];}
    if(any==0 || all==0xff || crc8_dallas(p,8)!=p[8])return false;
    uint16_t bits=(uint16_t)p[0]|((uint16_t)p[1]<<8);
    unsigned resolution=(p[4]>>5)&3u;
    bits&=(uint16_t)~((1u<<(3u-resolution))-1u);
    int32_t raw=(int16_t)bits;
    int32_t value=raw*100/16;if(value < -5500 || value>12500)return false;
    *centi=(int16_t)value;return true;
}
/* E 用内部基准估算VDDA，再计算分压前储能电压 */
bool store_convert(uint16_t raw,uint16_t ref,uint16_t *mv,uint16_t *vdda){
    if(raw>4095 || ref==0 || ref>4095)return false;
    uint32_t supply=(ADC_VREFINT_MV*4095u+ref/2u)/ref;
    uint32_t value=(uint32_t)(((uint64_t)raw*supply*STORE_DIVIDER_NUM+4095u*STORE_DIVIDER_DEN/2u)/(4095u*STORE_DIVIDER_DEN));
    if(supply<2200 || supply>3600 || value>STORE_VALID_MAX_MV)return false;
    *mv=(uint16_t)value;*vdda=(uint16_t)supply;return true;
}
