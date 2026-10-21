"""Draw module-level electronic schematics as vector PDF/SVG from one scene model."""
from pathlib import Path
import csv,json,html,math
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.colors import HexColor
from pypdf import PdfReader

ROOT=Path(__file__).resolve().parents[1]
PROJECT=ROOT if (ROOT/'firmware/Core/Inc/app_config.h').exists() else ROOT/'outputs/RailNode'
OUT=PROJECT/'hardware'
OUT.mkdir(parents=True,exist_ok=True)
W,H=1190,842
INK='#123b70';WIRE='#197447';TEXT='#243448';MUTED='#5b6878';WARN='#9a5715';GRID='#ccd5df'
pdfmetrics.registerFont(TTFont('CN','C:/Windows/Fonts/msyh.ttc',subfontIndex=0))
pdfmetrics.registerFont(TTFont('CNB','C:/Windows/Fonts/msyhbd.ttc',subfontIndex=0))
scenes=[];connections=[];parts={}

class Sheet:
    def __init__(self,title,num,subtitle):
        self.title=title;self.num=num;self.ops=[]
        self.rect(20,20,W-40,H-40,color=GRID)
        self.text(40,48,title,19,bold=True)
        self.text(40,72,subtitle,10,color=MUTED)
        self.line(40,87,W-40,87,color=GRID,width=.8)
        self.text(40,789,'RailNode / STM32F103C8T6 Blue Pill / R0.1 / 2026-10-21',9,color=MUTED)
        self.text(W-40,789,f'{num:02d} / 04   A3 横向',9,anchor='end',color=MUTED)
        self.text(40,811,'原理图草案：逻辑引脚连接已与代码核对；未做EDA/ERC、PCB布线、实物与功耗验证。',9,color=WARN)
        scenes.append(self)
    def line(self,x1,y1,x2,y2,color=WIRE,width=1.2,dash=False):self.ops.append(('line',x1,y1,x2,y2,color,width,dash))
    def rect(self,x,y,w,h,color=INK,fill=None,dash=False):self.ops.append(('rect',x,y,w,h,color,fill,dash))
    def circle(self,x,y,r=2,color=WIRE,fill=True):self.ops.append(('circle',x,y,r,color,fill))
    def text(self,x,y,value,size=10,color=TEXT,anchor='start',bold=False):
        width=pdfmetrics.stringWidth(str(value),'CNB' if bold else 'CN',size)
        left=x if anchor=='start' else x-width if anchor=='end' else x-width/2
        assert left>=18 and left+width<=W-18,(self.num,value,left,width)
        self.ops.append(('text',x,y,str(value),size,color,anchor,bold))
    def node(self,x,y,net,anchor='start'):
        self.text(x,y-5,net,9,color=WIRE,anchor=anchor)
    def gnd(self,x,y,net='GND_NODE'):
        self.line(x,y,x,y+7);self.line(x-10,y+7,x+10,y+7);self.line(x-6,y+11,x+6,y+11);self.line(x-2,y+15,x+2,y+15)
        self.text(x,y+30,net,8,color=WIRE,anchor='middle')
    def resistor(self,x,y,ref,value,topnet,bottomnet):
        # The endpoints are y and y+70; IEC rectangle is 32 high.
        self.line(x,y,x,y+18);self.rect(x-7,y+18,14,32);self.line(x,y+50,x,y+70)
        self.text(x+14,y+30,ref,9,color=INK);self.text(x+14,y+45,value,9)
        add(ref,value,{'1':topnet,'2':bottomnet},self.num,'电阻；按图连接')
    def cap(self,x,y,ref,value,topnet,bottomnet='GND_NODE',polar=False):
        self.line(x,y,x,y+23);self.line(x-12,y+23,x+12,y+23,color=INK,width=1.6)
        self.line(x-12,y+30,x+12,y+30,color=INK,width=1.6);self.line(x,y+30,x,y+58)
        self.text(x+17,y+24,ref,9,color=INK);self.text(x+17,y+40,value,9)
        if polar:self.text(x-24,y+22,'+',11,color=INK)
        add(ref,value,{'1':topnet,'2':bottomnet},self.num,'有极性，正极接高端' if polar else '非极性去耦/滤波')
    def module(self,x,y,w,h,ref,value):
        self.rect(x,y,w,h,fill='#fbfcff');self.text(x+w/2,y+23,ref,11,anchor='middle',bold=True,color=INK)
        self.text(x+w/2,y+43,value,10,anchor='middle',color=INK)
    def pin(self,x,y,w,h,side,at,label,net,ref,pinname=None,nc=False):
        if side=='left':
            px,py=x-35,y+at;self.line(px,py,x,py);self.text(x+7,py+3,label,9,color=INK);self.node(px-6,py,net,'end') if net else None
        elif side=='right':
            px,py=x+w+35,y+at;self.line(x+w,py,px,py);self.text(x+w-7,py+3,label,9,anchor='end',color=INK);self.node(px+6,py,net) if net else None
        elif side=='top':
            px,py=x+at,y-30;self.line(px,py,px,y);self.text(px+6,y-7,label,9,color=INK);self.node(px,py-3,net,'middle') if net else None
        else:
            px,py=x+at,y+h+30;self.line(px,y+h,px,py);self.text(px+6,y+h-8,label,9,color=INK);self.node(px,py+21,net,'middle') if net else None
        if nc:
            self.line(px-4,py-4,px+4,py+4,color=INK);self.line(px-4,py+4,px+4,py-4,color=INK)
        if net:connect(ref,pinname or label,net,self.num)
        return px,py
    def notes(self,items,y=641):
        self.line(40,y-16,W-40,y-16,color=GRID,width=.8)
        for i,line in enumerate(items):self.text(44,y+20*i,line,10,color=TEXT if i else WARN,bold=(i==0))
    def labelbox(self,x,y,w,h,title,lines,color=GRID):
        self.rect(x,y,w,h,color=color);self.text(x+12,y+23,title,11,bold=True)
        for i,t in enumerate(lines):self.text(x+12,y+45+i*18,t,9,color=MUTED)

def connect(ref,pin,net,page):connections.append({'ref':ref,'pin':pin,'net':net,'sheet':page})
def add(ref,value,pins,page,note=''):
    assert ref not in parts,ref
    parts[ref]={'ref':ref,'value':value,'sheet':page,'note':note}
    for pin,net in pins.items():
        if net:connect(ref,pin,net,page)

# 01 - Actual module header connections. No bare-chip PCB footprint is assumed.
s=Sheet('01  主板、供电与调试接口',1,'使用成品Blue Pill最小系统板；同名网络跨页直接相连。方框内是模块，脚名按实物丝印核对。')
x,y,w,h=450,140,270,450
s.module(x,y,w,h,'U1','STM32F103C8T6 / Blue Pill')
left=[('PA0 / ADC1_IN0','VSTORE_ADC'),('PA1 / 1-Wire','TEMP_DQ'),('PB6 / I2C1_SCL','I2C_SCL'),('PB7 / I2C1_SDA','I2C_SDA'),('PA9 / USART1_TX','RADIO_RX'),('PA10 / USART1_RX','RADIO_TX'),('PB0 / GPIO','RADIO_M0'),('PB1 / GPIO','RADIO_M1'),('PB10 / EXTI','RADIO_AUX')]
for i,(label,net) in enumerate(left):s.pin(x,y,w,h,'left',90+i*36,label,net,'U1',label.split(' /')[0])
right=[('3V3','3V3_NODE'),('GND','GND_NODE'),('PA13 / SWDIO','SWDIO'),('PA14 / SWCLK','SWCLK'),('NRST','NRST'),('PA2 / USART2_TX','DBG_TX'),('PA3 / USART2_RX','DBG_RX')]
for i,(label,net) in enumerate(right):s.pin(x,y,w,h,'right',90+i*42,label,net,'U1',label.split(' /')[0])
add('U1','STM32F103C8T6 Blue Pill',{},1,'按排针丝印接线，不作为裸芯片封装；BOOT0=0，PC13板载LED')
s.text(585,565,'BOOT0=0；PC13 LED沿用板载',9,anchor='middle')
for xx,yy,ref,val in [(105,405,'C5','100nF / 10V'),(255,405,'C6','10uF / 10V')]:
    s.node(xx,yy-8,'3V3_NODE','middle');s.cap(xx,yy,ref,val,'3V3_NODE');s.gnd(xx,yy+58)
s.labelbox(55,145,300,155,'主板已含的部分',[
 '板载稳压、去耦、复位、启动跳帽与LED。',
 '现代码使用内部8MHz HSI与LSI，',
 '不依赖外部8MHz / 32.768kHz晶振。',
 '各家蓝板电路不同，收到后核对实物。'])
s.labelbox(835,123,300,175,'J1  ST-Link / SWD逻辑接口',[
 'SWDIO  → SWDIO / PA13', 'SWCLK  → SWCLK / PA14', 'GND       → GND_NODE',
 'NRST     → NRST（可选）', 'VTref     → 3V3_NODE（仅电平参考）'])
add('J1','SWD逻辑接口',{'SWDIO':'SWDIO','SWCLK':'SWCLK','GND':'GND_NODE','NRST':'NRST','VTref':'3V3_NODE'},1,'调试器若标3.3V的是供电输出，则不并接到已供电目标')
s.labelbox(835,340,300,150,'J2  USB-TTL调试接口',[
 '适配器RXD ← DBG_TX / PA2', '适配器TXD → DBG_RX / PA3（可选）', '适配器GND ↔ GND_NODE',
 '适配器VCC / 5V / 3V3供电脚不接', '9600 / 8N1；先只接RXD与GND'])
add('J2','3.3V逻辑 USB-TTL调试口',{'RXD':'DBG_TX','TXD':'DBG_RX','GND':'GND_NODE'},1,'VCC悬空，避免电脑向节点倒灌')
s.notes(['接线规则：信号名以接收器件为准，RADIO_RX是无线模块的RXD，因此接MCU的PA9/TX。',
 '本页为成品主板的接口原理图，未把板内裸芯片、晶振和稳压器重复画成可生产电路。',
 '只使用一条明确的节点供电路径；SWD的VTref不是额外电源，按实际调试器说明连接。',
 '自供电验收需拔掉节点USB、ST-Link与调试串口；接收端电脑可以保持独立供电。',
 'C5/C6靠近主板供电排针；高峰值无线去耦位于第03页。所有元件值仍需实测复核。'])

# 02 - Real passive component-level sensor and ADC circuits.
s=Sheet('02  温度、振动与储能电压采集',2,'绿色同名标签表示同一网络；黑色圆点为电气连接点。传感器电源与数字上拉均为节点3.3V。')
for xx,title in [(45,'A  DS18B20：三线外供电'),(420,'B  ADXL345：I2C模式'),(820,'C  储能电压：二分之一分压')]:s.text(xx,120,title,12,bold=True)
s.line(390,145,390,600,color=GRID,width=.7);s.line(790,145,790,600,color=GRID,width=.7)
x,y,w,h=160,320,170,120;s.module(x,y,w,h,'U2','DS18B20 / TO-92')
s.pin(x,y,w,h,'top',85,'3 VDD','3V3_NODE','U2','3')
s.pin(x,y,w,h,'left',80,'2 DQ','','U2','2');connect('U2','2','TEMP_DQ',2)
s.pin(x,y,w,h,'bottom',85,'1 GND','GND_NODE','U2','1')
add('U2','DS18B20+ / TO-92',{},2,'1=GND，2=DQ，3=VDD；其他封装/探头线色需核对')
s.node(85,204,'3V3_NODE','middle');s.resistor(85,215,'R1','4.7k','3V3_NODE','TEMP_DQ')
s.line(85,285,85,400);s.line(85,400,125,400);s.circle(85,400);s.node(85,425,'TEMP_DQ','middle')
s.node(275,157,'3V3_NODE','middle');s.cap(275,170,'C1','100nF / 10V','3V3_NODE');s.gnd(275,228)
s.text(55,535,'R1是DQ上拉，不串在DQ信号线上。',9)
s.text(55,555,'TO-92数字为引脚号，不代表探头线色。',9)
s.text(55,575,'不支持寄生供电；每条总线先接一只。',9)
x,y,w,h=520,330,180,220;s.module(x,y,w,h,'U3','ADXL345 / 3.3V模块')
for at,label,net in [(68,'SCL','I2C_SCL'),(108,'SDA','I2C_SDA'),(148,'CS','3V3_NODE'),(188,'SDO / ALT','GND_NODE')]:s.pin(x,y,w,h,'left',at,label,net,'U3',label.split(' /')[0])
s.pin(x,y,w,h,'top',90,'VCC / 3V3','3V3_NODE','U3','VCC')
s.pin(x,y,w,h,'bottom',90,'GND','GND_NODE','U3','GND')
s.pin(x,y,w,h,'right',90,'INT1','', 'U3',nc=True)
s.pin(x,y,w,h,'right',130,'INT2','', 'U3',nc=True)
add('U3','ADXL345 3.3V兼容I2C模块',{},2,'CS高，SDO低，7位地址0x53；内部供电/I/O电压必须确认')
for xx,ref,net in [(445,'R2','I2C_SCL'),(625,'R3','I2C_SDA')]:
    s.node(xx,175,'3V3_NODE','middle');s.resistor(xx,190,ref,'4.7k (DNP*)','3V3_NODE',net);s.node(xx,282,net,'middle')
s.text(425,298,'* 模块已有合适上拉时R2/R3不装。',8,color=WARN)
s.node(735,200,'3V3_NODE','end');s.cap(715,220,'C2','100nF','3V3_NODE');s.gnd(715,278)
# ADC divider: closed JP1 when node is powered and low-voltage measurement is intended.
s.node(910,165,'VSTORE','middle');s.line(910,170,910,190)
s.circle(910,194,3,fill=False);s.circle(910,214,3,fill=False);s.line(910,194,910,214,color=INK,width=2)
s.text(932,199,'JP1',10,color=INK);s.text(932,216,'测量跳线',9)
add('JP1','1x2 ADC测量跳线（调试时闭合）',{'1':'VSTORE','2':'VSTORE_DIV_IN'},2,'节点无电时断开外部VSTORE，避免PA0倒灌')
s.line(910,217,910,240);s.resistor(910,240,'R6','100k / 1%','VSTORE_DIV_IN','VSTORE_ADC')
s.line(910,310,910,340);s.circle(910,340);s.line(910,340,1070,340);s.node(1080,340,'VSTORE_ADC','end')
s.resistor(910,340,'R7','100k / 1%','VSTORE_ADC','GND_NODE');s.gnd(910,410)
s.line(1020,340,1020,370);s.circle(1020,340);s.cap(1020,370,'C3','100nF / 10V','VSTORE_ADC');s.gnd(1020,428)
s.text(825,510,'VSTORE_ADC → U1.PA0 / ADC1_IN0',9)
s.text(825,535,'VSTORE=ADC÷4095×VDDA×2',9)
s.text(825,557,'VSTORE暂定0至5.5V；不是过压保护。',9,color=WARN)
s.text(825,579,'4.0V时分压支路约20uA / 80uW。',9)
s.notes(['ADXL345模块必须确认内部供电和I/O确实适合3.3V；不能把模块的VIN与芯片VDD_IO混为一谈。',
 '若裸用ADXL345芯片，需要另按厂家电源去耦、封装与布局设计；本图为模块接口。INT1/INT2本版轮询读取，不接。',
 'ADC输入必须在允许范围内；不接线圈或未限压整流输出。先让节点供电，再接低压VSTORE，断电顺序反过来。',
 'JP1用于断开独立VSTORE测量源，避免节点断电时从PA0倒灌；未来自动欠压断电方案还要增加采样隔离设计。',
 'R6/R7各100k使分压倍率为2；更换阻值后同步修改app_config.h并用万用表校准。'])

# 03 - Node radio and independently powered PC receiver.
s=Sheet('03  LoRa采集端与电脑接收端',3,'U4/U5均为E22-400T22D串口模块；本图按逻辑脚名连接，不臆定不同模块版本的物理排针顺序。')
s.text(50,123,'A  节点端：MCU控制模式和接收ACK',12,bold=True)
s.text(660,123,'B  电脑端：独立供电，正常接收模式',12,bold=True)
s.line(610,145,610,600,color=GRID,width=.7)
x,y,w,h=200,225,200,245;s.module(x,y,w,h,'U4','E22-400T22D')
for at,label,net in [(75,'RXD','RADIO_RX'),(110,'TXD','RADIO_TX'),(150,'M0','RADIO_M0'),(185,'M1','RADIO_M1')]:s.pin(x,y,w,h,'left',at,label,net,'U4',label)
s.pin(x,y,w,h,'right',110,'AUX','RADIO_AUX','U4','AUX')
s.pin(x,y,w,h,'top',100,'VCC','3V3_NODE','U4','VCC')
s.pin(x,y,w,h,'bottom',100,'GND','GND_NODE','U4','GND')
add('U4','E22-400T22D + 匹配天线',{},3,'节点端，M1/M0=00正常、11休眠；核对实际版本手册')
for xx,ref,net in [(90,'R4','RADIO_M0'),(485,'R5','RADIO_M1')]:
    s.node(xx,166,'3V3_NODE','middle');s.resistor(xx,170,ref,'100k','3V3_NODE',net);s.node(xx,256,net,'middle')
for xx,ref,value,polar in [(80,'C7','100nF / 10V',False),(400,'C8','220uF / 10V',True)]:
    s.node(xx,516,'3V3_NODE','middle');s.cap(xx,523,ref,value,'3V3_NODE',polar=polar);s.gnd(xx,581)
s.text(440,391,'AUX → PB10',9);s.text(440,412,'RXD ← PA9/TX',9);s.text(440,433,'TXD → PA10/RX',9)
x,y,w,h=745,205,180,245;s.module(x,y,w,h,'U5','E22-400T22D')
s.pin(x,y,w,h,'top',90,'VCC','3V3_RX','U5','VCC')
s.pin(x,y,w,h,'bottom',90,'GND','GND_RX','U5','GND')
s.pin(x,y,w,h,'left',80,'M0','GND_RX','U5','M0');s.pin(x,y,w,h,'left',115,'M1','GND_RX','U5','M1')
s.pin(x,y,w,h,'right',100,'TXD','RX_TO_PC','U5','TXD')
s.pin(x,y,w,h,'right',155,'RXD','PC_TO_RX','U5','RXD')
s.pin(x,y,w,h,'left',185,'AUX','', 'U5',nc=True)
add('U5','E22-400T22D + 匹配天线',{},3,'接收端独立3.3V，模式脚接地，配置参数与U4匹配')
s.labelbox(975,410,160,171,'J5  电脑USB-TTL',[
 'RXD ← RX_TO_PC','TXD → PC_TO_RX','GND ↔ GND_RX','VCC供电脚不接',
 'ACK需要TXD这根线','电平必须3.3V'])
add('J5','电脑接收USB-TTL',{'RXD':'RX_TO_PC','TXD':'PC_TO_RX','GND':'GND_RX'},3,'真实串口--ack时必须接TXD')
s.node(665,506,'3V3_RX','middle');s.cap(665,520,'C9','100nF / 10V','3V3_RX','GND_RX');s.gnd(665,578,'GND_RX')
s.node(755,506,'3V3_RX','middle');s.cap(755,520,'C10','220uF / 10V','3V3_RX','GND_RX',polar=True);s.gnd(755,578,'GND_RX')
s.labelbox(970,151,165,105,'J4  接收端电源',[
 '3.3V稳压 / 独立限流', '正极 → 3V3_RX','负极 → GND_RX'])
add('J4','接收端独立3.3V电源口',{'1':'3V3_RX','2':'GND_RX'},3,'外部电源覆盖射频峰值；TTL供电能力不可凭标签假定')
s.notes(['节点与接收端之间仅通过无线通信；GND_NODE与GND_RX不是要求用导线跨接的同一网络。',
 'R4/R5在上电、MCU尚未配置时将模式脚拉高；固件运行后由PB0/PB1控制。实际模式表按对应版本手册。',
 'C8/C10为无线近端储能去耦的调试起点，不代替超级电容；容量、ESR、稳压器稳定性和发射最低电压需实测。',
 'wireless_ack配置配合monitor.py --ack；普通wireless/fixed/adaptive不等待远端确认。双方参数须匹配。',
 '电脑端TXD用于回传ACK；只接RXD能够收数但无法确认。先接好匹配天线，再上电测试。'],y=664)

# 04 - Verified LDO pinout, manual power selection, and explicitly unresolved front end.
s=Sheet('04  3.3V稳压、手动电源选择与取电接口',4,'上半页为LP5907固定3.3V参考连接；下半页仅列待定取电前端，尚不能用于完整自供电或生产打板。')
x,y,w,h=385,200,225,190;s.module(x,y,w,h,'U6','LP5907MFX-3.3/NOPB')
s.pin(x,y,w,h,'left',75,'1 IN','VSTORE','U6','1')
s.pin(x,y,w,h,'left',125,'3 EN','VSTORE','U6','3')
s.pin(x,y,w,h,'right',75,'5 OUT','REG3V3','U6','5')
s.pin(x,y,w,h,'bottom',110,'2 GND','GND_NODE','U6','2')
s.pin(x,y,w,h,'right',125,'4 N/C','', 'U6',nc=True)
add('U6','LP5907MFX-3.3/NOPB / SOT-23-5',{},4,'仅按DBV/MFX封装：1IN 2GND 3EN 4NC 5OUT，其他封装不可套用')
# Continuous VIN/EN joining wire; the OUT net connects only through manual switch.
s.line(285,275,350,275);s.line(285,275,285,325);s.line(285,325,350,325);s.circle(285,275)
s.node(130,170,'VSTORE','middle');s.cap(130,190,'C11','2.2uF / 10V','VSTORE');s.gnd(130,248)
s.node(660,370,'REG3V3','middle');s.cap(660,385,'C12','2.2uF / 10V','REG3V3');s.gnd(660,443)
s.labelbox(55,330,255,145,'J3  低压储能/调试输入',[
 '正极 → VSTORE；负极 → GND_NODE',
 '初期用限流直流源模拟，名义4.0V。',
 '不接线圈、未保护整流或市电。',
 'LP5907推荐输入上限5.5V，',
 '前端设计必须留过冲与误差裕量。'])
add('J3','受控低压VSTORE输入口',{'1':'VSTORE','2':'GND_NODE'},4,'初期限流直流验证；最终取电输出须经保护与储能管理')
# A break-before-make SPDT supply selector: two sources are never directly tied.
s.text(886,162,'SW1  单刀双掷 / 先断后合',12,anchor='middle',bold=True)
s.node(813,231,'EXT3V3','end');s.line(815,235,875,235);s.circle(875,235,3,fill=False)
s.node(813,311,'REG3V3','end');s.line(815,315,875,315);s.circle(875,315,3,fill=False)
s.circle(950,275,3,fill=False);s.line(950,275,880,239,color=INK,width=1.6);s.line(953,275,1085,275);s.node(1090,275,'3V3_NODE','end')
s.text(925,350,'只选一路，禁止同时并接',10,anchor='middle',color=WARN)
add('SW1','SPDT先断后合电源选择开关',{'1':'EXT3V3','2':'3V3_NODE','3':'REG3V3'},4,'手动在外部3.3V与LDO输出之间选择，非自动电源路径')
s.labelbox(805,385,330,91,'J6  外部3.3V调试电源',[
 '正极 → EXT3V3；负极 → GND_NODE',
 '输出设3.3V并限流；SW1选择EXT位置。'])
add('J6','节点外部3.3V电源口',{'1':'EXT3V3','2':'GND_NODE'},4,'独立电源覆盖MCU/传感器/无线峰值；不等于TTL供电脚')
s.text(45,514,'以下为取电前端设计边界：虚线框内未定型，不属于可照图焊接的电路。',11,color=WARN,bold=True)
pending=[(45,205,'磁芯与次级绕组',['材料/窗口/气隙/匝数','50Hz带载功率曲线']),
         (285,245,'永久保护、整流与泄放',['TVS最大钳位/持续耗散','二极管额定/损耗/负载']),
         (565,245,'降压与限流充电',['LMR36503具体后缀待核','电感/UVLO/反馈/启动']),
         (845,290,'超级电容与欠压路径',['容量/ESR/漏电/充电限压','防倒灌/硬件欠压/ADC隔离'])]
for xx,ww,title,lines in pending:
    s.rect(xx,538,ww,83,color=WARN,dash=True);s.text(xx+10,560,title,11,color=WARN,bold=True)
    for i,t in enumerate(lines):s.text(xx+10,581+i*17,t,9,color=MUTED)
s.notes(['LP5907的有效输入/输出电容各需至少1uF；图中2.2uF仍须按直流偏压、温度和容差核查有效容量。',
 'LP5907额定250mA不代表本板已经满足无线峰值；先测掉压、稳压稳定性和温升，必要时另设计无线支路。',
 '本版没有完成取能、整流限压、超级电容充电和硬件欠压电路；固件保护与预算不能替代这些电路。',
 '原作品的TVS标称电压不能直接当成最大钳位电压；LMR36503工作/瞬态限值要按完整后缀和厂家手册核算。',
 '下一版需先确定磁芯带载曲线、前端峰值、充电目标与实际负载，再画完整电源元件级原理图。'],y=664)

def render_pdf(path):
    c=canvas.Canvas(str(path),pagesize=(W,H));c.setTitle('RailNode 原理图草案 R0.1：模块搭建版');c.setAuthor('RailNode Project')
    for s in scenes:
        for op in s.ops:
            kind,*a=op
            if kind=='line':
                x1,y1,x2,y2,color,width,dash=a;c.setStrokeColor(HexColor(color));c.setLineWidth(width);c.setDash([5,4] if dash else []);c.line(x1,H-y1,x2,H-y2)
            elif kind=='rect':
                x,y,w,h,color,fill,dash=a;c.setStrokeColor(HexColor(color));c.setLineWidth(.9);c.setDash([5,4] if dash else [])
                if fill:c.setFillColor(HexColor(fill))
                c.rect(x,H-y-h,w,h,stroke=1,fill=bool(fill))
            elif kind=='circle':
                x,y,r,color,fill=a;c.setStrokeColor(HexColor(color));c.setFillColor(HexColor(color));c.setDash([]);c.circle(x,H-y,r,stroke=1,fill=fill)
            else:
                x,y,t,size,color,anchor,bold=a;c.setFont('CNB' if bold else 'CN',size);c.setFillColor(HexColor(color))
                {'start':c.drawString,'end':c.drawRightString,'middle':c.drawCentredString}[anchor](x,H-y,t)
        c.showPage()
    c.save()

def svg(s):
    out=[f'<svg xmlns="http://www.w3.org/2000/svg" width="420mm" height="297mm" viewBox="0 0 {W} {H}">',f'<title>{html.escape(s.title)}</title>','<rect width="100%" height="100%" fill="white"/>']
    for kind,*a in s.ops:
        if kind=='line':
            x1,y1,x2,y2,color,width,dash=a;out.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" stroke-width="{width}"'+(' stroke-dasharray="5 4"' if dash else '')+'/>')
        elif kind=='rect':
            x,y,w,h,color,fill,dash=a;out.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" stroke="{color}" fill="{fill or "none"}" stroke-width="0.9"'+(' stroke-dasharray="5 4"' if dash else '')+'/>')
        elif kind=='circle':
            x,y,r,color,fill=a;out.append(f'<circle cx="{x}" cy="{y}" r="{r}" stroke="{color}" fill="{color if fill else "white"}"/>')
        else:
            x,y,t,size,color,anchor,bold=a;out.append(f'<text x="{x}" y="{y}" fill="{color}" font-size="{size}" text-anchor="{anchor}" font-family="Microsoft YaHei, Noto Sans CJK SC, sans-serif" font-weight="{700 if bold else 400}">{html.escape(t)}</text>')
    return '\n'.join(out+['</svg>'])

def validate():
    seen={}
    for c in connections:
        key=(c['ref'],c['pin'])
        if key in seen:assert seen[key]==c['net'],(key,seen[key],c['net'])
        seen[key]=c['net']
    expected={'PA0':'VSTORE_ADC','PA1':'TEMP_DQ','PB6':'I2C_SCL','PB7':'I2C_SDA','PA9':'RADIO_RX','PA10':'RADIO_TX','PB0':'RADIO_M0','PB1':'RADIO_M1','PB10':'RADIO_AUX','PA2':'DBG_TX','PA3':'DBG_RX','PA13':'SWDIO','PA14':'SWCLK'}
    for pin,net in expected.items():assert seen['U1',pin]==net
    assert seen['U4','RXD']==seen['U1','PA9'] and seen['U4','TXD']==seen['U1','PA10']
    assert seen['U5','TXD']==seen['J5','RXD'] and seen['U5','RXD']==seen['J5','TXD']
    assert seen['U3','CS']=='3V3_NODE' and seen['U3','SDO']=='GND_NODE'
    assert seen['U6','1']==seen['U6','3']=='VSTORE' and seen['U6','5']=='REG3V3'
    assert seen['U4','GND']!=seen['U5','GND']
    assert seen['SW1','1']!=seen['SW1','3']
    # The graph must agree with the currently shipped ADC divider and all MCU port/pin macros.
    config=(PROJECT/'firmware/Core/Inc/app_config.h').read_text(encoding='utf-8')
    assert '#define STORE_DIVIDER_NUM          2u' in config and '#define STORE_DIVIDER_DEN          1u' in config
    for text in ('#define TEMP_PIN                   GPIO_PIN_1','#define RADIO_M0_PIN               GPIO_PIN_0','#define RADIO_M1_PIN               GPIO_PIN_1','#define RADIO_AUX_PIN              GPIO_PIN_10'):assert text in config
    return seen

if __name__=='__main__':
    validate()
    pdf=OUT/'RailNode_Schematic_R01.pdf';render_pdf(pdf)
    for s,name in zip(scenes,['01_Controller','02_Sensors_ADC','03_LoRa','04_Power_Interface']):(OUT/(name+'.svg')).write_text(svg(s),encoding='utf-8')
    # Deduplicate interface pins that also appear as explicit components.
    unique=list({(c['ref'],c['pin']):c for c in connections}.values())
    with (OUT/'Connections.csv').open('w',newline='',encoding='utf-8-sig') as f:
        writer=csv.DictWriter(f,fieldnames=['sheet','ref','pin','net']);writer.writeheader();writer.writerows(unique)
    with (OUT/'BOM.csv').open('w',newline='',encoding='utf-8-sig') as f:
        writer=csv.DictWriter(f,fieldnames=['ref','value','sheet','note']);writer.writeheader();writer.writerows(parts.values())
    (OUT/'schematic_scene.json').write_text(json.dumps([{'title':s.title,'ops':s.ops} for s in scenes],ensure_ascii=False,indent=2),encoding='utf-8')
    d=PdfReader(pdf);assert len(d.pages)==4
    for i,p in enumerate(d.pages):assert 'RailNode' in p.extract_text(),i
    print('PASS: four vector schematic sheets,',len(parts),'parts,',len(unique),'pin/net connections; PDF text verified')
