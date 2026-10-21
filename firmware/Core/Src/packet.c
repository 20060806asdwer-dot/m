/* 【08 无线数据格式：24字节遥测与10字节确认】
 * 阅读导航：docs/代码阅读地图.md。
 */
#include "packet.h"
/* A 小端序写入：多字节数字拆成低字节在前 */
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
/* B 无线帧CRC16校验 */
uint16_t crc16_ccitt(const uint8_t *p,size_t n){
    uint16_t c=0xffffu;
    while(n--){c^=(uint16_t)*p++<<8;for(unsigned i=0;i<8;i++)c=(uint16_t)((c&0x8000u)?((uint32_t)c<<1)^0x1021u:(uint32_t)c<<1);}
    return c;
}
/* C DS18B20的CRC8校验 */
uint8_t crc8_dallas(const uint8_t *p,size_t n){
    uint8_t c=0;
    while(n--){uint8_t v=*p++;for(unsigned i=0;i<8;i++){uint8_t mix=(c^v)&1u;c>>=1;if(mix)c^=0x8cu;v>>=1;}}
    return c;
}
/* D 遥测编码：温度、电压、振动、状态与有效位 */
void packet_encode(const telemetry_t *r,uint8_t out[PACKET_SIZE]){
    out[0]=0xa5;out[1]=0x5a;out[2]=1;out[3]=r->node;
    put16(out+4,r->boot);put16(out+6,r->seq);
    for(unsigned i=0;i<4;i++)out[8+i]=(uint8_t)(r->uptime_s>>(8*i));
    put16(out+12,(uint16_t)r->temp_centi);put16(out+14,r->store_mv);put16(out+16,r->vibration_mg);
    out[18]=r->state;out[19]=r->flags;put16(out+20,r->reset_count);put16(out+22,crc16_ccitt(out,22));
}
/* E 确认帧编码：接收端回传节点、启动号和序号 */
void packet_ack_encode(uint8_t node,uint16_t boot,uint16_t seq,uint8_t out[ACK_SIZE]){
    out[0]=0xb5;out[1]=0x5b;out[2]=1;out[3]=node;
    put16(out+4,boot);put16(out+6,seq);put16(out+8,crc16_ccitt(out,8));
}
/* F 确认匹配：拒绝坏CRC、其他节点或过期序号 */
bool packet_ack_matches(const uint8_t data[ACK_SIZE],uint8_t node,uint16_t boot,uint16_t seq){
    if(data[0]!=0xb5 || data[1]!=0x5b || data[2]!=1 || data[3]!=node)return false;
    if(((uint16_t)data[4]|((uint16_t)data[5]<<8))!=boot || ((uint16_t)data[6]|((uint16_t)data[7]<<8))!=seq)return false;
    return crc16_ccitt(data,8)==((uint16_t)data[8]|((uint16_t)data[9]<<8));
}
