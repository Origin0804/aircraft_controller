# FC_H743 飞控板 —— MCU 引脚与外设分配（引脚级事实表）

> 适用板：`飞控板/ProPrj_飞控_2026-09-09.epro2` 对应的这一版硬件（立创EDA 3.2.186）
> MCU：**STM32H743VIT6**，LQFP-100，480 MHz
> 本文档整理于取用当时的最新导出：**网表 `Netlist_Schematic1_2026-09-15.tel`**（比 .epro2 新 6 天）
>
> ⚠️ 本文描述的是 **rev1 实装状态**（既成事实表）。下一版的引脚重规划见同目录 **`REV2_PLAN.md`**。

---

## 0. 本文档的证据链（先看这节，再决定信到什么程度）

| 级别 | 来源 | 提供什么 | 可信度 |
|---|---|---|---|
| ① 原理图网表 | `飞控板/Netlist_Schematic1_2026-09-15.tel` | **U1 引脚号 → 网络名**、全部元件位号/封装 | 最高（原理图直接导出） |
| ② EDA 工程 | `飞控板/ProPrj_飞控_2026-09-09.epro2` | PCB 焊盘-网络（PAD_NET）、铺铜层、DRC 规则 | 高（用于交叉验证①） |
| ③ 固件 | 本工程 `FC_H743.ioc` + `Core/Src/*.c` | 每个外设的**实际参数**（AF、波特率、分频） | 最高（就是跑在板上的代码） |
| ④ 数据手册 | `飞控板/h743数据手册.pdf`（ST **DS12110 Rev 7**，357 页） | Table 9 引脚定义、Table 10–19 复用功能表 | 权威 |

**已做的核对**：

- **引脚号 ↔ 引脚名**：用 DS12110 Table 9 逐脚比对，可自动解析的 **78 脚全部一致，0 处不符**；其余为跨行/电源类行（VSS/VDD/VCAP/晶振/BOOT0 等），由网表独立佐证。
- **复用功能号（AFx）**：取自 DS12110 Table 10–19，并用固件里已知的 AF 反向校验通过 —— PA5=AF5(SPI1_SCK)、PD12=AF2(TIM4_CH1)、PC8=AF12(SDMMC1_D0)、PB6=AF4(I2C1_SCL)、PA9=AF7(USART1_TX)、PD8=AF7(USART3_TX)、PA11=AF6(UART4_RX)、PE9=AF1(TIM1_CH1) 全部对上。
- **未核对的部分**：① 与 ② 的一致率我按引脚做了对照，**唯一差异是 §6.2 的 LED 悬空问题**；I2C 实际波特率**未上板实测**（§6.4）。

---

## 1. 总览

- 100 脚中 **56 脚**接了网络（含电源/地），**44 脚空置**。
- 外设占用：ADC1、SPI1、I2C1、I2C2、USART1、USART3、UART4、TIM1、TIM4、SDMMC1、SWD + 若干 GPIO/EXTI。
- **没有空闲的"整条 SPI 外设"，但有 3 组完全空闲的 SPI 引脚落点**（见 §5.2）。SPI5 在 LQFP100 上没有引出引脚；SPI6 与 SPI1/SPI3 复用同一组 PB3/PB4/PB5。

---

## 2. 信号引脚表（按引脚号，权威版）

| 脚号 | 引脚 | 板上网络名 | MCU 侧配置（AF / 模式） | 连到哪 |
|---|---|---|---|---|
| 1 | PE2 | `LED_1` | GPIO 推挽输出，初值低 | 3 颗可寻址 RGB 灯级联数据线：LED15→LED1→LED16（LED16 尾部空） |
| 22 | PA0 | `VOLTAGE_ADC` | ADC1_INP16，模拟 | 电压采样节点；CN7.4 外部电压经 R27(1K) 注入 |
| 23 | PA1 | `CURRENT_ADC` | ADC1_INP17，模拟 | 电流采样节点；CN7.3 外部电流经 R23(1K) 注入 |
| 24 | PA2 | `MAG_DRDY` | GPIO 输入，无上下拉 | IST8310(U4) 的 DRDY/INT（EXTI 线 2，固件未启用） |
| 29 | PA5 | `IMU_SCK` | SPI1_SCK，AF5 | ICM-42670-P(U2) pin13 SCLK |
| 30 | PA6 | `IMU_SDO` | SPI1_MISO，AF5 | U2 pin1 SDO/AD0 |
| 31 | PA7 | `IMU_SDI` | SPI1_MOSI，AF5 | U2 pin14 SDI |
| 32 | PC4 | `IMU_CS` | GPIO 推挽输出，**初值高** | U2 pin12 CS（软件片选） |
| 33 | PC5 | `IMU_INT1` | EXTI 线 5，`gpio.c` 配**上升沿**、NVIC 优先级 **0**；`main.c` 启动后 `HAL_NVIC_DisableIRQ(EXTI9_5_IRQn)` | U2 pin4 INT1 |
| 39 | PE9 | `PWM1` | TIM1_CH1，AF1 | H1.1（经 33Ω R7）；另经 220Ω R34 到指示灯 LED4 |
| 41 | PE11 | `PWM2` | TIM1_CH2，AF1 | H1.2（R8）；R35→LED5 |
| 43 | PE13 | `PWM3` | TIM1_CH3，AF1 | H1.3（R9）；R36→LED6 |
| 44 | PE14 | `PWM4` | TIM1_CH4，AF1 | H1.4（R10）；R37→LED7 |
| 46 | PB10 | `BARO_SCL` | I2C2_SCL，AF4，开漏 | U3(SPA06 气压计) pin4 + U4(IST8310) pin1；上拉 R2 4.7K |
| 47 | PB11 | `BARO_SDA` | I2C2_SDA，AF4，开漏 | U3 pin3 + U4 pin16；上拉 R3 4.7K |
| 55 | PD8 | `UART3_TX` | USART3_TX，AF7 | CN5.2（GPS 口） |
| 56 | PD9 | `UART3_RX` | USART3_RX，AF7 | CN5.1（GPS 口） |
| 59 | PD12 | `PWM5` | TIM4_CH1，AF2 | H1.5（经 33Ω R32）；R41→LED11 |
| 60 | PD13 | `PWM6` | TIM4_CH2，AF2 | H1.6（R33）；R42→LED12 |
| 61 | PD14 | `PWM7` | TIM4_CH3，AF2 | H1.7（R11）；R43→LED13 |
| 62 | PD15 | `PWM8` | TIM4_CH4，AF2 | H1.8（R12）；R44→LED14 |
| 64 | PC7 | `BUZZERIO` | GPIO 推挽输出，初值低 | R19(1K) → Q1 基极（低边驱动），BUZZER1=3kHz 蜂鸣器，D1 续流 |
| 65 | PC8 | `SDMMC1_D0` | SDMMC1_D0，AF12 | TF 座 U8 pin7（经 33Ω R29，ESD D7） |
| 66 | PC9 | `SDMMC1_D1` | AF12 | U8 pin8（R30，D5） |
| 67 | PA8 | `TF_CD` | GPIO 输入，**内部上拉** | U8 pin9 卡检测（0=有卡） |
| 68 | PA9 | `UART1_TX` | USART1_TX，AF7 | CH340N(U5) pin7 |
| 69 | PA10 | `UART1_RX` | USART1_RX，AF7 | CH340N(U5) pin6 |
| 70 | PA11 | `UART4_RX_SBUS` | UART4_RX，AF6 | CN1.1（SBUS 输入；H7 USART 支持 `RXINV` 硬件反相，无需外接反相管） |
| 72 | PA13 | `SWDIO` | SWDIO，AF0 | CN8.4 |
| 76 | PA14 | `SWCLK` | SWCLK，AF0 | CN8.1 |
| 78 | PC10 | `SDMMC1_D2` | AF12 | U8 pin1（R25，D9） |
| 79 | PC11 | `SDMMC1_D3` | AF12 | U8 pin2（R26，D8） |
| 80 | PC12 | `SDMMC1_CK` | AF12 | U8 pin5（R31 33Ω，D10） |
| 83 | PD2 | `SDMMC1_CMD` | AF12 | U8 pin3（R28，D6） |
| 92 | PB6 | `I2C1_SCL` | I2C1_SCL，AF4，开漏 | **板上无器件**，只到 CN2/CN3/CN4/CN6 四个对外 I2C 口；上拉 R4 4.7K |
| 93 | PB7 | `I2C1_SDA` | I2C1_SDA，AF4，开漏 | 同上；上拉 R5 4.7K |

### 2.1 电源 / 地 / 时钟 / 复位 / 启动

| 脚号 | 引脚 | 网络 | 说明 |
|---|---|---|---|
| 6 | VBAT | `3.3V` | 直接接 3.3V |
| 11/27/50/75/100 | VDD | `3.3V` | 主 3.3V（U7 LDO，5V→3.3V） |
| 10/26/49/74/99 | VSS | `GND` | |
| 19 | VSSA | `GND` | |
| 21 | VDDA | `VREF+` 网络 | VDDA 与 VREF+ 同网，由 3.3V 经 **L1 磁珠** + C5(100nF)+C6(1µF) 滤波 |
| 20 | VREF+ | `VREF+` | 同上 |
| 48/73 | VCAP | `$1N34`/`$1N35` | 各接 2.2µF（C3/C4）到地 |
| 12/13 | PH0/PH1 | `OSC_IN`/`OSC_OUT` | HSE 25MHz 晶振 X1 + C1/C2 15pF；OSC_OUT 经 R20(0Ω) |
| 14 | NRST | `NRST` | 复位键 SW2 + R6(10K) 上拉 + C22 |
| 94 | BOOT0 | `BOOT0` | R1(10K) 下拉 + SW3 按键上拉至 3.3V |
| 72/76 | PA13/PA14 | `SWDIO`/`SWCLK` | SWD 调试口（CN8），**保留** |

---

## 3. 外设参数（来自固件实际配置，非 CubeMX 默认值）

时钟树：HSE 25 MHz → PLL1(M=5, N=192, P=2) → **SYSCLK 480 MHz**；HCLK 240 MHz；APB1/2/3/4 = 120 MHz（定时器时钟 ×2 = 240 MHz）；PLL1Q = 120 MHz（SPI/SDMMC 内核）；PLL2P = 30 MHz（ADC 内核）。

| 外设 | 引脚 | 关键参数 | 备注 |
|---|---|---|---|
| **ADC1** | PA0/PA1 | 16 bit，扫描 2 通道：rank1=INP16(PA0，电压)、rank2=INP17(PA1，电流)；采样 64.5 周期；非连续；内核 30MHz | 顺序固定：**先电压后电流** |
| **SPI1** | PA5/6/7 | 主机、全双工、**8 bit**、CPOL=0/CPHA=1Edge(Mode0)、软件 NSS、**预分频 8 → SCK ≈ 15 MHz**（内核 120MHz）、MSB first | ICM-42670-P 上限 24MHz，有余量。`DataSize` 必须 8BIT，CubeMX 对 H7 默认 4BIT 是坑 |
| **I2C1** | PB6/PB7 | Timing=`0x307075B1`，内核 D2PCLK1=120MHz | 外部罗盘口（4 个座子） |
| **I2C2** | PB10/PB11 | 同上 | 板载气压计 + 地磁 |
| **USART1** | PA9/PA10 | 115200 8N1 | 接 CH340N + Type-C，即控制台 |
| **USART3** | PD8/PD9 | 57600 8N1 | GPS 口（CN5，带 3.3V_GPS 供电） |
| **UART4** | PA11（RX） | **100000 8E2**（= SBUS 帧格式：100k、偶校验、2 停止位） | 只接了 RX；TX 配在 PA12 但**板上 PA12 无网络**，见 §6.1 |
| **TIM1** | PE9/11/13/14 | PSC=239、ARR=2499 → **400 Hz**，计数 1µs，4 通道初值 1500 (1.5ms) | 高级定时器，必须有 `__HAL_TIM_MOE_ENABLE` 才有输出 |
| **TIM4** | PD12–15 | PSC=239、ARR=49999 → **20 Hz**（周期 50ms），初值 1500 | 与常见 50Hz 舵机帧率不同，见 §6.3 |
| **SDMMC1** | PC8–12、PD2 | 4-bit 宽总线、上升沿采样、ClockDiv=4（内核 PLL1Q 120MHz → 30MHz）；走 HAL（H7 无 SDMMC 的 LL） | 卡识别阶段 HAL 会先降到 ≤400kHz |
| **GPIO 输出** | PC4 / PC7 / PE2 | 推挽，初值分别为 高 / 低 / 低 | IMU 片选、蜂鸣器、WS2812 |
| **GPIO 输入 / EXTI** | PC5 / PA2 / PA8 | PC5=EXTI5(上升沿，优先级0)、PA2=输入无上下拉、PA8=输入内部上拉 | |
| **DMA** | — | **当前没有任何 DMA 配置** | 全工程开 D-Cache，将来加 DMA 需做 cache 维护 |

---

## 4. 板级连接（对外接口）

| 接口 | 型号/位号 | 引脚定义（实测网表） |
|---|---|---|
| **H1** | 2.54mm 3×8 排针（24P） | 1–8 = `PWM1…PWM8`；9–16 = `5V`；17–24 = `GND`（PWM 均经 33Ω 串阻） |
| **CN1** | JST-GH 3P | 1=`UART4_RX_SBUS`，2=`5V`，3=GND |
| **CN2/CN3/CN4/CN6** | JST-GH 4P ×4 | 1=`I2C1_SCL`，2=`I2C1_SDA`，3=GND，4=`5V`（4 个**外部罗盘/扩展 I2C 口**） |
| **CN5** | JST-GH 4P | 1=`UART3_RX`，2=`UART3_TX`，3=`3.3V_GPS`，4=GND（GPS 口；3.3V 经 L6 磁珠供电） |
| **CN7** | JST-GH 6P(+1 固定脚) | 1,2=`POWER_5V`；3→R23(1K)→`CURRENT_ADC`；4→R27(1K)→`VOLTAGE_ADC`；5,6=GND（电源输入 + 电压/电流回采） |
| **CN8** | JST-GH 4P | 1=`SWCLK`，2=`3.3V`，3=GND，4=`SWDIO`（调试口） |
| **USB1** | Type-C 16P | → CH340N(U5) → `USART1`；CC1/CC2 各 5.1K 下拉（R15/R16） |
| **U2** | ICM-42670-P（LGA-14） | 6 轴 IMU，SPI1；pin7 FSYNC 接地，**pin9 INT2 未接** |
| **U3** | SPA06-003（LGA-8） | 气压计，挂 I2C2（**当前板上未焊接**） |
| **U4** | IST8310（LGA-16） | 地磁，挂 I2C2，`MAG_DRDY` 接 PA2 |
| **U8** | TF 卡座（push-push） | SDMMC1 4-bit + 卡检测；D5–D10 为 ESD 二极管，R25–R31 为 33Ω 串阻 |
| **电源** | U7(SOT-89-3) / U6(SOT-23-5) | U7：5V→`3.3V`（主）；U6：→`3.3V_BARO`（**IMU/气压计/地磁独立供电**，低噪声）；Q3 为 5V 开关，D4/D11 输入保护 |
| **指示灯** | LED1/LED15/LED16 + LED2–LED14 | 前者 3 颗可寻址 RGB（PE2 驱动）；后者 13 颗普通 LED，分别由 `PWM1–PWM8`（220Ω）与 `5V`/`3.3V`/`3.3V_BARO`（360–500Ω）供电 —— **但见 §6.2** |
| **按键** | SW2 / SW3 | SW2 = 复位（NRST）；SW3 = BOOT0 拉高 |

---

## 5. 空置引脚（44 个）

### 5.1 完整清单

```
PA3  PA4  PA12 PA15 PB0  PB1  PB2  PB3  PB4  PB5  PB8  PB9  PB12 PB13 PB14 PB15
PC0  PC1  PC2_C PC3_C PC6  PC13 PC14 PC15
PD0  PD1  PD3  PD4  PD5  PD6  PD7  PD10 PD11
PE0  PE1  PE3  PE4  PE5  PE6  PE7  PE8  PE10 PE12 PE15
```

### 5.2 这些空脚能干什么（为下一版预留，AF 号已对过手册）

| 需求 | 可用落点（全为空脚） | 说明 |
|---|---|---|
| **第二路 SPI（双 IMU）** | **PB13=SPI2_SCK / PB14=SPI2_MISO / PB15=SPI2_MOSI（AF5）**，CS 用 PB12 | 引脚 51–54 连续，是布线最顺的一组 |
| 第三路 SPI | PB3=SPI1/3/6_SCK、PB4=MISO、PB5=MOSI | 89–91 连续；占用后失去 SWO 追踪 |
| 第四路 SPI | PE12=SPI4_SCK、PE5=SPI4_MISO、PE6=SPI4_MOSI（AF5） | 三脚分散在 42/4/5 号，布线差 |
| 串口（GPS/数传） | PD0/PD1=UART4_RX/TX（AF8）；PE7/PE8=UART7_RX/TX（AF7）；PD5/PD6=USART2_TX/RX | PD0/PD1、PE7/PE8 都是连续脚 |
| CAN | PB8/PB9 或 PD0/PD1 = FDCAN1_RX/TX（AF9） | PB8/PB9 是首选 |
| ADC 扩展 | PC0=ADC_INP10、PC1=ADC_INP11、PC2_C/PC3_C=ADC3_INP0/INP1、PA3=INP15、PA4=INP18 | `PC2_C/PC3_C` 是 H743 的**直连模拟脚**，不做模拟输入就完全浪费 |
| USB | PA11/PA12 = OTG_FS_DM/DP（AF10）；PB14/PB15 = OTG_HS_DM/DP（AF12） | 注意：**PB14/15 与"SPI2 做第二路 IMU"互斥** |

---

## 6. 已知问题 / 待确认

### 6.1 PA12 空引脚被固件当成 UART4_TX（软硬件不一致）
`Core/Src/usart.c` 里 `UART4` 配的是 `UART_MODE_TX_RX`，TX 落在 **PA12**；但 09-15 网表里 **PA12 没有任何网络**（`.epro2` 的 PAD_NET 同样为空）。也就是说这块板的 SBUS 只能收、不能发。
→ 要么下一版把 PA12 引出到 CN1（SBUS 需要发配置/遥测时有用），要么固件改成 `UART_MODE_RX`。

### 6.2 `LED2`–`LED14` 各有一个电极没有网络（疑似漏连，**新导出仍未修**）
多个独立来源一致：09-15 网表导出、09-09 工程原理图网表、09-09 工程 PCB 焊盘-网络映射、**以及 09-15 新导出的 `.epro2`**，都显示这 13 颗灯只有 **2 脚**接在网络，**1 脚为空**。
对照：板上其它两脚器件（如 R13、D5、C3）两脚都有网络；WS2812 链尾 `LED16.1` 空才是合理的。

13 颗灯的分工（2 脚侧）：

| 灯 | 2 脚经串阻接到 | 类型 |
|---|---|---|
| LED2 / LED3 | `USB_5V`(R13) / `POWER_5V`(R14)，360Ω | 电源指示 |
| LED8 | `5V`(R38)，500Ω | 电源指示 |
| LED9 / LED10 | `3.3V_BARO`(R39) / `3.3V`(R40)，360Ω | 电源指示 |
| LED4–LED7 | `PWM1`–`PWM4`(R34–R37)，220Ω | 通道活动指示 |
| LED11–LED14 | `PWM5`–`PWM8`(R41–R44)，220Ω | 通道活动指示 |

→ 按现有接法推测它们的 1 脚应接 GND，**请在立创EDA 画布上确认**；若确实漏连，这 13 颗指示灯上电不会亮。

**2026-09-15 复查**：新导出的 `ProPrj_飞控_2026-09-15.epro2` 与 09-09 导出在电气上零差异（见 §9），所以这个疑似漏连**没有被修**。

### 6.3 TIM4 的 PWM 帧率是 20 Hz，不是 50 Hz
`TIM4.Prescaler=239`、`TIM4.Period=49999`，定时器时钟 240MHz → 计数 1µs、周期 **50 ms = 20 Hz**。TIM1 是 400 Hz（ARR=2499）正常。
→ 若本意是 50Hz 舵机帧率，ARR 应为 19999。

### 6.4 I2C 实际速率未实测
`Timing = 0x307075B1`（PRESC=1、SCLL=112、SCLH=117），按内核 120MHz 与手册公式算 **SCL ≈ 260 kHz**。这是算出来的值，没上板量过 —— 需要时用逻辑分析仪复核。

### 6.5 IMU_INT1 的 EXTI 配置有历史隐患
`gpio.c` 把 PC5 配成**上升沿中断 + 无上下拉 + NVIC 优先级 0**（高于 SysTick 的 15）；`main.c` 在自检/流式模式下会 `HAL_NVIC_DisableIRQ(EXTI9_5_IRQn)` 兜底。IMU 未焊或 INT 悬空时会疯狂进中断饿死 SysTick（详见 `README.md` §7 第 14 条）。

### 6.6 文档与实物历史不一致（已纠正）
本文件已按实测修正：**地磁 IST8310 在 I2C2（PB10/PB11）**、气压计未焊接。旧文档曾写成"地磁在 I2C1 / 地址 0x0E"，是错的。

---

## 7. 网表名 ↔ 固件宏 对照

| 网表名 | 固件宏（`Core/Inc/main.h`） | 引脚 |
|---|---|---|
| `IMU_CS` | `IMU_CS_Pin` | PC4 |
| `IMU_INT1` | `IMU_INT1_Pin` / `IMU_INT1_EXTI_IRQn` | PC5 |
| `MAG_DRDY` | `MAG_DRDY_Pin` | PA2 |
| `TF_CD` | `TF_CD_Pin` | PA8 |
| `BUZZERIO` | `BUZZER_Pin` | PC7 |
| `LED_1` | `WS2812_DIN_Pin` | PE2 |
| `UART1_*` / `UART3_*` / `UART4_RX_SBUS` | `huart1` / `huart3` / `huart4` | 见 §2 |
| `PWM1…PWM8` | （无宏，直接用 `htim1`/`htim4` 通道） | 见 §2 |

---

## 8. 怎么复核本文档

```sh
# 引脚号→网络（需要 .tel，路径含中文注意加引号）
grep -A2 "'IMU_CS'" 飞控板/Netlist_Schematic1_2026-09-15.tel

# 固件侧引脚宏
grep -n "GPIO_PIN" Core/Inc/main.h Core/Src/gpio.c

# 外设参数
grep -nE "Init\.(BaudRate|Prescaler|Period|Timing|ClockDiv|BaudRatePrescaler)" Core/Src/*.c

# 手册：Table 9 引脚定义 / Table 10–19 复用功能
pdftotext -layout 飞控板/h743数据手册.pdf - | grep -n "PB13"
```

---

## 9. 工程文件本身的坑（重要，导 Gerber/网表前必读）

`ProPrj_飞控_2026-09-15.epro2` 里**同时存在两套完整的设计**（各含原理图 + PCB + 板框）：

| 角色 | 原理图容器 | PCB | 板框 | 最后修改 |
|---|---|---|---|---|
| **现用** | `Schematic1_1` (0f4f34ec) | `PCB1_1` (c9929823) | 标题 "2" (d8595dd6) | **09-14 01:30 / 01:33** |
| 旧版 | `Schematic1` (c4cd6f84) | `PCB1` (33766331) | `Board1` (df71250e) | 08-19 / 09-13 13:46 |

> ⚠️ **命名陷阱：带 `_1` 后缀的才是现用的那套。** 不带后缀的 `Schematic1`/`PCB1`/`Board1` 是 8 月的旧版。
> 导出网表/Gerber/坐标前，先确认当前打开的是 `Schematic1_1` / `PCB1_1`。

**解析工具的注意事项**：`lceda-epro-inspect` 的 `epro_parse.py` 会把**所有文档段合并**，所以它的 `net_names`（139 个）是**超集**——里面混着**没有焊盘/引脚挂载的残留网络定义**（`LEDIO_R/G/B`、`AGND`、`DGND`、`UART2_RX_SBUS`、`3.3V_IMU`、`3.3V_MAG`、`I2C2_SCL` 等，来自早期版本被改名的网络）。
**不要拿它当网络表用**；网络表的权威来源是 `Netlist_Schematic1_2026-09-15.tel`（77 条）或 PCB 焊盘网络（50 个命名网络，两者一致）。

### 9.1 09-15 导出 vs 09-09 导出：全部差异（逐对象比对）

电气层面（网表、器件、位号、布局坐标、铺铜、过孔、规则数值）**零变化**，只有 4 处文档改动：

| 文档 | 改动 |
|---|---|
| 原理图 `Schematic1_1` (09-14 01:30) | 新增 2 条线宽规则：`TRACK/copperThickness1oz`(0.127/0.254/2.54mm)、`copperThickness2oz`(0.203/…) |
| 原理图页 `de893718` (09-14 01:13) | 一只 `1N4148TR` 二极管符号 `isMirror` false→true（仅符号镜像） |
| 现用 PCB `PCB1_1` (09-14 01:33) | 一个元件（Channel ID `$4I7`）旋转 90°→270°（=转 180°）及其属性文字角度 |
| 旧 PCB `PCB1` (09-13 13:46) | 增加 2 条尺寸标注（DIMENSION/LENGTH），另 1 个元素图层 1→2 |

**结论**：这次导出没有改电路，§2 的引脚表和上一轮给出的双 IMU 建议全部继续成立。

### 9.2 DRC/规则检查结果（09-15 导出）

- ⚠️ DRC 模板名是 `JLCPCB Capability(Two Layers Board)`，但铺铜在 **Inner1(GND) / Inner2(3.3V)** → 这是**四层板**。规则数值本身达标（1oz 线宽 min 0.127mm、2oz min 0.203mm，均 ≥ 嘉立创工艺下限），但**下一版建工程时把模板换成四层板能力模板**。
- ✅ 叠层记录：core 1.379mm + prepreg 0.394mm（εr≈3.3，FR4）
- ✅ 关键网络存在：GND / DGND / 3.3V / 5V / POWER_5V / USB_5V
- ✅ 铺铜 18 块 / 4797 mm²；过孔 0（全通孔设计）
- 💡 本检查只核对**规则配置与数据完整性**，布局违规需在立创EDA 里跑 DRC 确认。
