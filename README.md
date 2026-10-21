# RailNode：STM32F103C8T6 工程使用说明

已按你的选择固定为**一开始推荐的 STM32F103C8T6 Blue Pill 蓝色最小系统板**，板载 LED 接 PC13，64 KiB Flash、20 KiB RAM。开发入口为 VS Code。该版没有 NUCLEO 或 L476 适配。

本工程已包含应用代码、ST 官方 HAL、ARM 启动文件、链接脚本、构建与烧录脚本、VS Code 任务、上位机、CSV 分析工具与 DeepSeek 只读助手。**直接打开现有工程，不需要先生成或拼接代码。** 它是手工维护的 HAL/CMake 工程，没有 `.ioc`，不要用 CubeMX 直接覆盖。CubeMX 可在另一个目录学习引脚配置。

软件已编译，纯算法已测试；传感器时序、真实无线、STOP 唤醒、供电与功耗尚待实物验证。本代码不包含尚未测定的磁芯绕组、电源原理图或量产 PCB。

## 1 买什么，先拿哪些东西

| 物品 | 数量 | 要点 |
|---|---:|---|
| STM32F103C8T6 Blue Pill 最小系统板 | 1，另1块可备用 | 芯片型号 C8T6，PC13 LED，能接 SWD；不按未标称的128K空间设计 |
| ST-Link 调试器、杜邦线 | 1套 | 买明确支持 STM32 SWD 的型号；区分目标供电与 VTref |
| 3.3V逻辑 USB-TTL | 2只便于联调 | 一只看调试日志，一只接无线接收端；也可一只轮流用 |
| DS18B20、4.7kΩ | 各1 | 三线外供电，核对封装脚序 |
| ADXL345 模块 | 1 | I2C、实际芯片供电合适，CS拉高、SDO拉低 |
| E22-400T22D、对应天线 | 各2 | 两只同型号同版本；本驱动使用透明 UART，不是 SPI 的 M 系列 |
| 100kΩ电阻、100nF电容 | 2只、若干 | VSTORE分压及去耦；建议分压电阻1%或更好 |
| 独立限流3.3V电源、万用表 | 借用 | 电源需覆盖无线瞬时电流，先按至少300mA能力联调 |

磁芯、绕组、电源及储能材料按主方案执行。先验证实际带载能力，再冻结这些器件；不能只凭软件定义认为已经实现自供电。

## 2 固定接线

以下按板上引脚名称接线；芯片裸引脚号和不同传感器封装的脚序要查实物资料。所有低压数字模块共地。

| 功能 | STM32端 | 外设端/接法 |
|---|---|---|
| 板供电 | 3V3、GND | 受控3.3V，首次限流；只保留一条供电路径 |
| SWD | PA13、PA14、GND | SWDIO、SWCLK、GND；VTref接目标3.3V参考；有NRST可接复位 |
| 启动 | BOOT0 | 正常运行保持0；首次不改BOOT1 |
| 调试日志 USART2 | PA2 | 接USB-TTL RX；GND共地；9600、8N1 |
| 调试串口输入 | PA3 | 可不接；程序当前不读取命令 |
| DS18B20 | PA1 | DQ并4.7kΩ上拉到3.3V；VDD=3.3V，GND共地 |
| ADXL345 I2C1 | PB6、PB7 | SCL、SDA；外部上拉到3.3V；CS=3.3V，SDO=GND，地址0x53 |
| 储能测量 | PA0 | VSTORE→100kΩ→PA0→100kΩ→GND；PA0对地100nF |
| E22 UART1 | PA9、PA10 | 接 E22 RXD、TXD，交叉接线 |
| E22模式 | PB0、PB1 | 接 M0、M1，不能悬空 |
| E22 AUX | PB10 | 接 AUX，用于就绪和发送忙检测 |
| LED | PC13 | 默认板载低电平亮；若实物不同需改 app_config.h |

PA0只接已限压的低压 VSTORE 分压，当前量程上限5.5V；软件阈值不是输入保护。外供电检查 ADC 时，可用独立可调低压模拟 VSTORE，两边共地，不把线圈或整流高压接到 PA0。无线电源单独验证后再并入节点稳压轨，不能用 MCU GPIO 供电，USB-TTL 的3.3V输出也未必带得动无线。

接收端不需要第二个 MCU：E22 TXD→接收 USB-TTL RX，GND共地；需要配置时再接 USB-TTL TX→E22 RXD。E22 M0=M1=0为正常接收，独立合适电源供电，天线先接好。

## 3 电脑准备与第一次编译

1. 准备 VS Code、ST官方扩展 `stmicroelectronics.stm32-vscode-extension`、标准 Python（包含pip与Tk）。
2. 在 ST 的 Bundle Manager 准备 `gnu-tools-for-stm32`、`cmake`、`ninja`；调试烧录另需 `gnu-gdb-for-stm32`、`stlink-gdbserver`、`programmer`。本工程已经带 HAL，无需先下载 CubeF1。
3. 在 VS Code 打开本文件所在 **RailNode 文件夹**，终端当前目录也应为它。
4. 按顺序执行：

```powershell
python tools/project.py setup
python tools/project.py build bringup
python desktop/selftest.py
python -m unittest discover -s tests -p "test_desktop.py"
python desktop/monitor.py --simulate
```

`setup`检测已安装工具并生成这台电脑的 `.vscode/launch.json`，不会烧录、改系统PATH或安装软件。工具位置默认查PATH与ST Bundle目录；若使用自己的工具，可设置 `RAILNODE_GNU_TOOLS_FOR_STM32`、`RAILNODE_CMAKE`、`RAILNODE_NINJA` 等环境变量为对应可执行文件完整路径。`python`不可用而`py`可用时，将命令中的python换为py；VS Code任务里的command也一并换掉。

通过标准：构建产生 `build/bringup/railnode.hex`，Python显示PASS，界面注明演示数据。模拟数据不计入比赛测量。

## 4 按这五份固件推进

| 顺序/名称 | 实际行为 | 如何验收 |
|---|---|---|
| ① bringup | 只检查LED与日志，约每秒打印HELLO递增计数 | 连续看到30行、PC13闪烁 |
| ② sensors | 约每2秒读取温度、振动、储能电压；不发射 | 温度随触碰变化；振动随晃动变化；拔线后有效标志变0 |
| ③ wireless | 外部供电联调，约15秒采集发送；故意关闭能量门控 | 两只E22配对后，上位机收到真实24字节帧 |
| ④ adaptive | 根据储能电压调整周期；STOP休眠；关闭日志；看门狗 | 连续供电/唤醒测试，再测不同电压下周期与整机功耗 |
| ⑤ fixed | 与④相同保护与休眠，正常运行固定15秒 | 与④在相同取能输入及环境下对比 |

每一步只替换命令最后的配置名：

```powershell
python tools/project.py build bringup
python tools/project.py flash bringup
```

**只有 `flash` 会连接硬件并下载程序。** 此次交付没有执行烧录。也可用 VS Code 的 Ctrl+Shift+B 选择 Build，再在任务中选择 Flash；先运行setup后可按F5选择对应配置调试。低功耗测量时停止调试并断开调试器、USB与其他供电回路。

已经编译好的文件在 `releases/配置名/`：HEX可由CubeProgrammer直接下载；BIN加载地址 `0x08000000`；预编译ELF保留函数符号但已移除体积较大的源码调试信息。要逐行调试先重新build，F5使用build目录的完整ELF。修改代码后应重新build，脚本flash使用build目录里的新文件。下载仅擦写覆盖的程序页，最后两页留给启动记录；全片擦除会清掉记录。

第一次接好板供电和SWD后先烧bringup。日志接PA2，用 `python -m serial.tools.miniterm COM5 9600` 查看，COM5替换实际值，Ctrl+]退出后才能让其他程序用这个端口。

## 5 传感器怎么检查

烧sensors，先只接DS18B20。日志 `temp_centi=2500` 表示25.00℃；`flags`的第0位为1才有效。10位分辨率为0.25℃，打印两位小数不代表精度0.01℃。程序每轮发起新转换、检查超时和CRC，未读到不会伪造正常值。单总线只支持一只外供电DS18B20。

再接ADXL345：CS拉高、SDO拉低，PB6/PB7检查上拉。日志第1位为1才是有效振动值。程序按100Hz收集64点，去掉三轴直流均值，报告合成RMS（mg），不是频谱或机械故障诊断；±2g模式超量程会削顶。此处100Hz仅是传感器输出设置，需实测采样节拍，不能据此承诺高频振动能力。

最后接分压。用万用表同时记录 VSTORE 与板上3.3V；日志 `mV`对应储能，`vdda`对应内部基准估算的供电。先在3.6/4.0/4.5V几个点核对；校准 `app_config.h` 的 `ADC_VREFINT_MV` 及实际分压比，再重编译。F103使用内部基准标称值，没有假装成每片出厂精确校准。

`flags`以十六进制显示：01温度有效、02振动有效、04电压有效、08节点告警；组合07表示前三项有效。告警包括温度≥50℃、RMS≥300mg或启动计数持久化失败。缺项时先看有效位，不把数值0当成正常。

## 6 无线配对与上位机

两只E22先使用厂家匹配的配置工具设为：9600、8N1、透明传输、相同信道/网络ID/空速/密钥，关闭附加RSSI、固定地址头及中继等额外帧功能。先选相同且在实验条件允许的参数，记录实际配置；程序不盲写厂家配置寄存器。仅用产品页对应手册，不混用E220、E22-M系列或其他固件的模式表。

本驱动按 E22-400T22D 的 M1/M0=00正常、11休眠编写；到货核对手册版本。每次唤醒等AUX高稳定至少2ms，发送24字节后观察忙信号再等空闲。`aux_done=1`只表示本地模块完成AUX忙流程，不能据此声称远端收到或数据已可靠送达。超时会返回失败并尝试休眠。

```powershell
python -m pip install -r desktop/requirements.txt
python -m serial.tools.list_ports
python desktop/monitor.py --port COM6 --out-dir logs
```

COM6为**接收端E22**的USB-TTL口，不是采集板的文本日志口。界面显示温度、储能、振动、状态、超时和包统计；真实与演示CSV分别标记LIVE/SIMULATED，串口坏了不会改用模拟值。

24字节V1协议在 `desktop/protocol.py` 与 `firmware/Core/Inc/packet.h` 一致：A55A帧头、版本、节点、boot、seq、uptime、温度、储能、振动、状态、有效/告警位、reset_count、CRC16。上位机能在杂音、分片与坏帧后重新同步。

## 7 自供电与对比实验

先通过wireless外供电，再上adaptive；低功耗版不打印串口、不开工作LED。板载电源灯、板载稳压器、调试器和串口倒灌仍可能吞掉能量，须测整机实际电流；软件STOP不等于整板低功耗。ADC分压本身也耗电。

| 状态 | 阈值与滞回 | 行为 |
|---|---|---|
| 保护0 | 电压无效或低于3.65V；保护恢复需≥3.80V | 不采温振、不发射，约5秒检查电压 |
| 低能量1 | 恢复后低于3.95V；正常降到低于3.85V也进入 | 温度+电压，约60秒一包，不采振动 |
| 正常2 | 升到≥3.95V；维持到低于3.85V | 温度+振动+电压，约15秒一包 |

fixed版也保留3.65/3.80V保护，恢复运行后固定15秒并采振动，比较的是调度差异。阈值是初始工程参数，要按真实稳压压差与掉压修改。采集后、发射前再次检查储能。保护只是减少负载，不能替代硬件欠压锁定和充电保护。

采样周期从本轮起点计算，RTC使用未校准40kHz LSI，实际时间会有偏差。使用秒表测10分钟与日志uptime之比，按 `新RTC_LSI_HZ = 原值 × 读数秒数 / 实际秒数` 估算修正，改后重新下载；更高精度再做实际LSE硬件适配。

启动次数用Flash最后两页的双页记录维护，每条数值及反码，支持中途写失败与页轮换；只在估算VDDA≥3.0V时写入。boot和seq在线协议只有16位，长期部署需升级，当前实验不要运行到计数回卷；持久化失败可能重复会话，告警位和启动日志会提示。看门狗用于低功耗版，不等于已完成全部掉电恢复验证。

## 8 实测CSV怎么分析

```powershell
python desktop/analyze.py telemetry logs/LIVE_实际文件名.csv
python desktop/analyze.py power 实测功耗.csv
```

功耗CSV表头必须为 `t_s,v_v,i_ma`：秒、测量位置电压V、测量电流mA，时间严格递增，至少两行。脚本对功率点做梯形积分，输出能量J、平均mW及采样峰值；仪器采样太慢会漏掉无线尖峰，不能用平均电流的手工猜值充当实测。分别保存fixed和adaptive的完整周期数据、取能输入、仪器档位和接线位置，再比较。

## 9 我和DeepSeek如何配合

我负责实现、解释、排错及整合；你们提供实际接线和观测。DeepSeek做独立只读检查，它的建议先复核再改代码。硬件未验证的问题要标成待测。

在RailNode目录执行下列命令仅预览，不联网：

```powershell
python desktop/deepseek_review.py --question "检查低电压滞回与安全恢复条件，给出最小反例" --file firmware/Core/Src/energy_policy.c --file firmware/Core/Inc/app_config.h
```

准备调用时用同一命令加 `--send --out logs/review_01.md`。没有环境变量密钥时脚本在本地隐藏输入，不保存密钥；不要发到聊天或提交源码。默认模型deepseek-flash，也可用--model指定账号可用模型。脚本不自动执行API返回的代码、不会改硬件或自动重试计费。此次没有使用你的API、没有产生API调用费用。

## 10 报错先查这里

| 现象 | 先做什么 |
|---|---|
| 找不到编译器/CMake | 查看Bundle Manager相应组件是否下载完成；运行setup，保留完整报错 |
| ST-Link无法连接 | 测3.3V，查GND/SWDIO/SWCLK、BOOT0=0；需要复位连接时接NRST |
| HELLO无字或乱码 | 设备管理器确认COM，PA2接RX，9600/8N1，释放其他占用软件 |
| 温度无效 | 查器件脚序、三线供电与上拉，线别过长；不把DS18S20当DS18B20 |
| 振动无效 | CS拉高、SDO拉低、PB6/PB7不反接；确认0x53与模块实际供电 |
| AUX超时、无线无包 | 查天线与电源掉压、AUX接PB10、M0/M1、两只完整参数；分别用厂家工具验证 |
| adaptive没输出 | 日志已关闭；VSTORE不足会不发射；先用wireless排查，确认不是看错串口 |
| 一发射就复位 | 示波器看3.3V最低值，查电源限流、稳压峰值与电容ESR；不要先靠延长延时掩盖掉压 |

两人每天只需保留：当前固件配置/版本、接线照片、原始串口/CSV、期望与实际结果。下一次排错可以直接定位具体一步。

代码来源与数据手册：

- [ST HAL F1](https://github.com/STMicroelectronics/stm32f1xx-hal-driver)、[ST CMSIS F1](https://github.com/STMicroelectronics/cmsis-device-f1)、[ARM CMSIS](https://github.com/ARM-software/CMSIS_5)：固定提交与SHA256见依赖清单。
- [DS18B20数据手册](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf)：温度转换、CRC与分辨率低位处理依据。
- [ADXL345官方快速指南](https://www.analog.com/en/resources/app-notes/an-1077.html)：初始化、数据读法与3.9mg/LSB依据。
- [E22-400T22D官方产品及下载](https://www.ebyte.com/product/1582.html)：模块、配对工具与版本手册。
- [DeepSeek官方接口文档](https://api-docs.deepseek.com/guides/search_fetch)：接口与模型名，实测调用尚未进行。

结构演示：[离线3D模型](docs/3D动态结构模型.html)、[OpenSCAD草模](docs/结构草模.scad)。下载HTML后用浏览器打开；为布局概念，不是可生产尺寸。
