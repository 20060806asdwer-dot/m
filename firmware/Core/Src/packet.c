#include "packet.h"
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
uint16_t crc16_ccitt(const uint8_t *p,size_t n){
    uint16_t c=0xffffu;
    while(n--){c^=(uint16_t)*p++<<8;for(unsigned i=0;i<8;i++)c=(uint16_t)((c&0x8000u)?((uint32_t)c<<1)^0x1021u:(uint32_t)c<<1);}
    return c;
}
uint8_t crc8_dallas(const uint8_t *p,size_t n){
    uint8_t c=0;
    while(n--){uint8_t v=*p++;for(unsigned i=0;i<8;i++){uint8_t mix=(c^v)&1u;c>>=1;if(mix)c^=0x8cu;v>>=1;}}
    return c;
}
void packet_encode(const telemetry_t *r,uint8_t out[PACKET_SIZE]){
    out[0]=0xa5;out[1]=0x5a;out[2]=1;out[3]=r->node;
    put16(out+4,r->boot);put16(out+6,r->seq);
    for(unsigned i=0;i<4;i++)out[8+i]=(uint8_t)(r->uptime_s>>(8*i));
    put16(out+12,(uint16_t)r->temp_centi);put16(out+14,r->store_mv);put16(out+16,r->vibration_mg);
    out[18]=r->state;out[19]=r->flags;put16(out+20,r->reset_count);put16(out+22,crc16_ccitt(out,22));
}
