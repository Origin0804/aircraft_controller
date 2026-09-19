# 飞控板 Rev2 —— MCU 引脚分配方案

> **范围**：本文只做 **MCU（STM32H743VIT6，LQFP100）引脚重新分配**，不涉及电源树、连接器、布局的大改。
> **证据来源**：
> - 引脚号 ↔ 引脚名：ST **DS12110 Rev 7** *Figure 5. LQFP100 pinout*（顶视图）+ *Table 9*（逐脚核对）
> - 复用功能（AF）：DS12110 **Table 10–19**，并用 rev1 固件里已知的 8 个 AF 反向校验通过
> - rev1 实际接线：`飞控板/Netlist_Schematic1_2026-09-15.tel` + `.epro2` 的 PCB 焊盘网络
> - 新器件：`飞控板/QMI8658A DATASHEET.pdf`（QST Doc# 13-52-25 Rev A）、`飞控板/LED手册.pdf`（WS2812B-2020-V6）
> - rev1 引脚事实表：同目录 `PINMAP.md`
>
> 凡是**我算出来的、没上板量过的**，都标「未实测」。

---

## 0. 本次确认的决策

| # | 决策 | 对引脚分配的影响 |
|---|---|---|
| 1 | **IMU#2 = QMI8658A**（LGA-14，与 U2 同封装库） | 挂 SPI2：PB13/PB14/PB15 + CS=PB12 + INT1=PD10 + INT2=PD11 |
| 2 | **不加 CAN** | PB8/PB9 保持空闲 |
| 3 | **只有 LED1 需要驱动**，频率未定，倾向 GPIO 直驱 | PE2 保留；其余 13 颗指示灯不在本次范围（见 §2.3 的 GPIO 可行性结论） |
| 4 | **电源部分先不动** | 引脚层面无影响 |
| 5 | **USB 改原生 OTG_FS** | **PA11/PA12 让给 USB**，SBUS 必须从 UART4(PA11/PA12) 迁走 |
| 6 | **UART4 = SBUS 接收机** | UART4 迁到 **PD0(RX) / PD1(TX)**，外设不变 |
| 7 | 只重规划 MCU 引脚 | 电源/连接器/PCB 规范等维持 rev1（但 §5 的两个必改项要带上） |

**净结果**：信号引脚 **49 个**（rev1 是 36 个），空闲 **31 个**。

---

## 1. Rev2 引脚分配总表

| 脚号 | 引脚 | Rev2 网络名 | AF / 模式 | 接什么 | 相对 rev1 |
|---|---|---|---|---|---|
| 1 | PE2 | `LED_DIN` | GPIO 推挽输出（备选 SPI4_SCK AF5） | LED1（WS2812B-2020-V6）数据输入 | 改名（原 `LED_1`） |
| 22 | PA0 | `VOLTAGE_ADC` | ADC1_INP16 | 电压采样 | 不变 |
| 23 | PA1 | `CURRENT_ADC` | ADC1_INP17 | 电流采样 | 不变 |
| 24 | PA2 | `MAG_DRDY` | GPIO 输入 → EXTI2 | IST8310 DRDY | 不变 |
| 29 | PA5 | `IMU1_SCK` | SPI1_SCK (AF5) | U2 ICM-42670-P pin13 | 改名（原 `IMU_SCK`） |
| 30 | PA6 | `IMU1_MISO` | SPI1_MISO (AF5) | U2 pin1 SDO | 改名（原 `IMU_SDO`） |
| 31 | PA7 | `IMU1_MOSI` | SPI1_MOSI (AF5) | U2 pin14 SDI | 改名（原 `IMU_SDI`） |
| 32 | PC4 | `IMU1_CS` | GPIO 输出，初值高 | U2 pin12 CS | 改名（原 `IMU_CS`） |
| 33 | PC5 | `IMU1_INT1` | GPIO → EXTI5 | U2 pin4 INT1 | 不变 |
| 39 | PE9 | `PWM1` | TIM1_CH1 (AF1) | H1.1（33Ω） | 不变 |
| 41 | PE11 | `PWM2` | TIM1_CH2 (AF1) | H1.2（33Ω） | 不变 |
| 43 | PE13 | `PWM3` | TIM1_CH3 (AF1) | H1.3（33Ω） | 不变 |
| 44 | PE14 | `PWM4` | TIM1_CH4 (AF1) | H1.4（33Ω） | 不变 |
| 46 | PB10 | `BARO_SCL` | I2C2_SCL (AF4) | U3 SPA06 + U4 IST8310 | 不变 |
| 47 | PB11 | `BARO_SDA` | I2C2_SDA (AF4) | 同上 | 不变 |
| 51 | PB12 | `IMU2_CS` | GPIO 输出，初值高 | **U9 QMI8658A pin12 CS** | **新增** |
| 52 | PB13 | `IMU2_SCK` | SPI2_SCK (AF5) | U9 pin13 SPC | **新增** |
| 53 | PB14 | `IMU2_MISO` | SPI2_MISO (AF5) | U9 pin1 SDO | **新增** |
| 54 | PB15 | `IMU2_MOSI` | SPI2_MOSI (AF5) | U9 pin14 SDI | **新增** |
| 55 | PD8 | `UART3_TX` | USART3_TX (AF7) | CN5 GPS | 不变 |
| 56 | PD9 | `UART3_RX` | USART3_RX (AF7) | CN5 GPS | 不变 |
| 57 | PD10 | `IMU2_INT1` | GPIO → EXTI10 | U9 pin4 INT1 | **新增** |
| 58 | PD11 | `IMU2_INT2` | GPIO → EXTI11 | U9 pin9 INT2/DRDY | **新增** |
| 59 | PD12 | `PWM5` | TIM4_CH1 (AF2) | H1.5（33Ω） | 不变 |
| 60 | PD13 | `PWM6` | TIM4_CH2 (AF2) | H1.6（33Ω） | 不变 |
| 61 | PD14 | `PWM7` | TIM4_CH3 (AF2) | H1.7（33Ω） | 不变 |
| 62 | PD15 | `PWM8` | TIM4_CH4 (AF2) | H1.8（33Ω） | 不变 |
| 63 | PC6 | `IMU1_INT2` | GPIO → EXTI6 | U2 **pin9**（rev1 悬空）→ 接出 | **新增** |
| 64 | PC7 | `BUZZERIO` | GPIO 输出，初值低 | Q1 基极（低边驱动蜂鸣器） | 不变 |
| 65 | PC8 | `SDMMC1_D0` | SDMMC1 (AF12) | U8 TF 座 | 不变 |
| 66 | PC9 | `SDMMC1_D1` | SDMMC1 (AF12) | U8 | 不变 |
| 67 | PA8 | `TF_CD` | GPIO 输入，内部上拉 | U8 卡检测 | 不变 |
| 68 | PA9 | `UART1_TX` | USART1_TX (AF7) | 调试串口（CN8 合并） | 不变 |
| 69 | PA10 | `UART1_RX` | USART1_RX (AF7) | 调试串口 | 不变 |
| 70 | PA11 | `USB_DM` | **OTG_FS_DM (AF10)** | Type-C D− | **改**（原 `UART4_RX_SBUS`） |
| 71 | PA12 | `USB_DP` | **OTG_FS_DP (AF10)** | Type-C D+ | **改**（rev1 空脚） |
| 72 | PA13 | `SWDIO` | SWDIO (AF0) | CN8 | 不变 |
| 76 | PA14 | `SWCLK` | SWCLK (AF0) | CN8 | 不变 |
| 78 | PC10 | `SDMMC1_D2` | SDMMC1 (AF12) | U8 | 不变 |
| 79 | PC11 | `SDMMC1_D3` | SDMMC1 (AF12) | U8 | 不变 |
| 80 | PC12 | `SDMMC1_CK` | SDMMC1 (AF12) | U8 | 不变 |
| 81 | PD0 | `SBUS_RX` | **UART4_RX (AF8) + RXINV** | 接收机 SBUS | **新增**（从 PA11 迁来） |
| 82 | PD1 | `SBUS_TX` | **UART4_TX (AF8)** | 接收机（可选回传） | **新增**（从 PA12 迁来） |
| 83 | PD2 | `SDMMC1_CMD` | SDMMC1 (AF12) | U8 | 不变 |
| 86 | PD5 | `UART2_TX` | USART2_TX (AF7) | 备用串口口（可选） | 新增 |
| 87 | PD6 | `UART2_RX` | USART2_RX (AF7) | 备用串口口（可选） | 新增 |
| 89 | PB3 | `SWO` | JTDO/TRACESWO (AF0) | 调试追踪，**不放他用** | 明确保留 |
| 92 | PB6 | `I2C1_SCL` | I2C1_SCL (AF4) | 外部罗盘口 | 不变 |
| 93 | PB7 | `I2C1_SDA` | I2C1_SDA (AF4) | 外部罗盘口 | 不变 |

### 1.1 电源 / 时钟 / 复位（与 rev1 相同）

| 脚号 | 引脚 | 网络 |
|---|---|---|
| 6 | VBAT | `3.3V` |
| 11/27/50/75/100 | VDD | `3.3V` |
| 10/26/49/74/99 | VSS | `GND` |
| 19 / 21 / 20 | VSSA / VDDA / VREF+ | VDDA 与 VREF+ 同网，经磁珠 + 100nF + 1µF |
| 48 / 73 | VCAP | 各 2.2µF 到地 |
| 12 / 13 | PH0 / PH1 | HSE 25 MHz 晶振 |
| 14 | NRST | 复位键 + 10K 上拉 + 100nF |
| 94 | BOOT0 | 10K 下拉 + 按键 |

---

## 2. 三处关键改动

### 2.1 USB 改原生 OTG_FS ⇒ SBUS 从 PA11/PA12 迁到 PD0/PD1

**引脚性质（已核）**：`PA11 = OTG_FS_DM (AF10)`、`PA12 = OTG_FS_DP (AF10)`，两只都是 `FT_u`（5V 容忍）—— 原生 USB 在这两个脚上是标准用法。

**SBUS 必须搬家**：rev1 的 SBUS 用 `PA11 = UART4_RX (AF6)`。PA11 让给 USB 后，UART4 换到 **PD0 = UART4_RX (AF8) / PD1 = UART4_TX (AF8)**（引脚 81/82，PD 口连续两只）。

**RXINV 硬件反相仍然有效**：`UART4_CR2` 的 **bit16 = RXINV** 确实存在（[ST SVD 字段表](https://stm32-rs.github.io/stm32-rs/stm32h/UART4_0x40004C00_CR2_0x0004.html)：UART4_CR2 含 RXINV/TXINV/DATAINV），CMSIS 头文件里也有 `USART_CR2_RXINV`，固件 rev1 已经设了 `UART_ADVFEATURE_RXINV_ENABLE`。换引脚不换外设 ⇒ **SBUS 处理逻辑一行不用改，只改 MspInit 里的 AF 配置**。

**VBUS 检测的取舍（要拍板）**：PA9 同时是 `USART1_TX (AF7)` 和 `OTG_FS_VBUS`（DS12110 Table 9 的 PA9 行明确列出附加功能 `SPI2_SCK/I2S2_CK, OTG_FS_VBUS`）。而 USART1 在 PB14/PB15 上的备用落点已经被 SPI2 占掉，所以：

> **建议：PA9/PA10 保留为 USART1 调试串口，固件关闭 OTG 的 VBUS 检测（自供电模式）。**
> 代价：不能靠 VBUS 自动判断插拔，改用软连接（`OTG_DCTL.SDIS`）重新枚举。这也是常见飞控的做法。
> 如果你更想要 VBUS 检测，那就得放弃硬件调试串口 —— 我不推荐。

**外围改动**：
- 删除 **U5 CH340N** 及其周边（rev1 的 USB 走 CH340N → USART1）。
- 保留 Type-C 与 CC 上下拉（R15/R16 5.1K，设备模式）。
- D+/D− 串 **22Ω ±5%**（PHY 侧），并加 **USB ESD 保护**（如 USBLC6-2SC6）。
- 调试串口挪到 CN8：把 CN8 从 4P 扩成 6P = `SWCLK / SWDIO / GND / 3.3V / UART1_TX / UART1_RX`，一根线同时拿 SWD 和串口。

### 2.2 IMU#2 = QMI8658A 挂 SPI2

| 项 | 设置 | 依据 |
|---|---|---|
| 总线 | SPI2：PB13(SCK) / PB14(MISO) / PB15(MOSI)，CS=PB12 | AF5，引脚 51–54 连续 |
| 模式 | **SPI Mode 0**（CPOL=0、CPHA=1Edge）、MSB first、8bit | QMI8658A 支持 Mode 0 或 Mode 3，数据在上升沿锁存 —— 与 rev1 固件现有参数**完全一致** |
| 速率 | **预分频 16 → 7.5 MHz**（内核 120MHz） | QMI8658A `fSPC` **最大 15 MHz**（Table 42）。跑满 15MHz 余量为零，建议降一档 |
| 中断 | INT1=PD10（EXTI10）、INT2/DRDY=PD11（EXTI11），同属 `EXTI15_10_IRQn` | 与 IMU#1 的 `EXTI9_5_IRQn` **不同向量**，互不牵连 |
| IMU#1 | SPI1 保持预分频 8 → 15 MHz（ICM-42670-P 上限 24MHz，有余量） | rev1 参数不变 |

**注意 SPI2 与 USB 不冲突**：PB14/PB15 也是 OTG_HS_DM/DP，但本板用的是 OTG_FS（PA11/PA12），无冲突。

### 2.3 LED1（WS2812B-2020-V6）用 GPIO 直驱 —— 可行，但有条件

**器件事实（`LED手册.pdf`）**：单线归零码（NZR），**800 kbps**，每像素 24 bit（顺序 **G7..G0 → R7..R0 → B7..B0**），数据高电平编码：

| 符号 | 含义 | 允许范围 |
|---|---|---|
| T0H | 0 码高电平 | **220–380 ns** |
| T1H | 1 码高电平 | **580 ns – 1 µs** |
| T0L / T1L | 低电平 | 580 ns – 1 µs |
| T0H+T0L、T1H+T1L | 位周期 | **≥ 1.25 µs** |
| RES | 帧间复位低电平 | **≥ 280 µs** |

其它：3.3V 供电可用，每通道 12 mA，静态电流 < 1 µA，端口扫描 2 kHz，内置信号整形可级联。

**结论：1 颗灯用 GPIO 直驱是可行的**，但要满足两个条件：

1. **一个像素 = 24 bit × 1.25 µs ≈ 30 µs 的严格时序**。480 MHz 下 1 周期 = 2.083 ns：T0H ≈ 144 周期（300 ns）、T1H ≈ 384 周期（800 ns）。用 DWT 周期计数或空循环延时即可，**但这 30 µs 内必须关中断**（本板有 EXTI 中断，被抢占会把位拉长到超出容限）。
2. 30 µs 关中断对 400 Hz 控制环（2500 µs 周期）约占 1.2%，**可接受**；RES 的 280 µs 不必关中断。

**如果以后要驱动更长的链**（>3 颗）或不想关中断，硬件上有现成退路：
- **SPI4 + DMA**：PE2 = `SPI4_SCK (AF5)`，SPI4 内核 120 MHz，取 **3.75 MHz（预分频 32）**，用 **5 个 SPI 位编码 1 个 NZR 位** —— 位周期 1.333 µs（≥1.25 ✓）、T0H = 267 ns（在 220–380 ✓）、T1H = 800 ns（在 580ns–1µs ✓）。占 SPI4 但不用额外引脚（PE2 本来就是它的 SCK）。
- **TIM + DMA**：任一空闲定时器通道按 1.25 µs 周期、25%/62.5% 占空比输出，DMA 送 24 个比较值。
- 三个方案里 **GPIO 直驱最省外设，SPI4+DMA 最不占 CPU**；本文按 GPIO 出图，PE2 不锁定 AF。

---

## 3. QMI8658A 硬件适配

### 3.1 逐脚对照：现有 footprint 能不能直接焊 QMI8658A

U2 现在的封装库名就是 `LGA-14_L3.0-W2.5-P0.50-TL_QMI8658A`（BOM 里写着），我按 QMI8658A 手册 Table 2/3 与 rev1 网表逐脚对了一遍：

| QMI 脚 | QMI8658A 名称 | 4-wire SPI 功能 | rev1 该脚网络 | 判定 |
|---|---|---|---|---|
| 1 | SDO/SA0 | SDO | `IMU_SDO` → PA6 | ✅ 同功能 |
| 2 | SDx（保留） | 可接 VDDIO/GND/NC | `GND` | ✅ 允许 |
| 3 | SCx（保留） | 可接 VDDIO/GND/NC | `GND` | ✅ 允许 |
| 4 | INT1 | 可编程中断 1 | `IMU_INT1` → PC5 | ✅ |
| 5 | **VDDIO** | IO 电源 | `3.3V_BARO` | ✅ |
| 6 | GND | — | `GND` | ✅ |
| 7 | GND | — | `GND` | ✅ |
| 8 | **VDD** | 主电源 | `3.3V_BARO` | ✅ |
| 9 | INT2/DRDY | 可编程中断 2 / 数据就绪 | **悬空** | ⚠️ rev2 接出到 PD11 |
| 10 | **RESV** | **须接 VDDIO/高电平，或悬空（靠内部 200K 上拉）** | **`GND`** | ❌ **必须改网络** |
| 11 | RESV-NC | 可接 VDDIO/高 或 GND/低 | `GND` | ✅ 允许 |
| 12 | CS | 片选（低=SPI 模式） | `IMU_CS` → PC4 | ✅ |
| 13 | SCL/SPC | SPI 时钟 | `IMU_SCK` → PA5 | ✅ |
| 14 | SDA/SDI | SPI 数据入 | `IMU_SDI` → PA7 | ✅ |

**结论：封装/land pattern 完全兼容，只有 U9（QMI8658A）的 pin10 必须从 `GND` 改到 `3.3V_BARO`。**

> 手册原文（§1.5）：*"In case of the necessity to connect it to High or Low level, the RESV (Pin 10) should be firmly connected to VDDIO, providing a stable High level, to **disable the output of Pin 11**."*
> 现在 pin10=GND、pin11=GND：pin11 被接死在 GND，而 pin10 又没给出"稳定高"，等于让 pin11 的输出使能处于未定义状态 —— **存在 pin11 输出与 GND 打架的风险**，必须改。

**两颗件的引脚定义逐条核对（ICM 侧取自 DS-000451 Table 9）**：

| 脚 | ICM-42670-P | QMI8658A | 是否一致 |
|---|---|---|---|
| 1 | AP_SDO / AP_AD0 | SDO / SA0 | ✅ |
| 2 / 3 | RESV（NC / GND / VDDIO 均可） | SDx / SCx（VDDIO / GND / NC 均可） | ✅ 允许范围相同 |
| 4 | INT1 | INT1 | ✅ |
| 5 | **VDDIO** | **VDDIO** | ✅ 相同 |
| 6 / 7 | GND / FSYNC(不用时接 GND) | GND / GND | ✅ |
| 8 | **VDD** | **VDD** | ✅ 相同 |
| 9 | INT2 | INT2 / DRDY | ✅ |
| 10 / 11 | RESV（NC / GND / VDDIO 均可） | **RESV 必须 VDDIO/高或悬空**；RESV-NC 任意 | ❌ **pin10 要求不同** |
| 12 / 13 / 14 | AP_CS / AP_SCL / AP_SDA | CS / SCL / SDA | ✅ |

也就是说：**两颗件的 pin5/pin8 定义完全相同**（都是 pin5=VDDIO、pin8=VDD），真正的差异只有 **pin10**：ICM 允许接 GND，QMI8658A 不允许。

### 3.2 驱动差异（写固件时要知道）

| 项 | ICM-42670-P（IMU#1） | QMI8658A（IMU#2） |
|---|---|---|
| `WHO_AM_I` | 寄存器 `0x75`，期望 **0x67** | 寄存器 `0x00`，期望 **0x05** |
| 寄存器映射 | Bank 0：`PWR_MGMT0 0x1F`、`GYRO_CONFIG0 0x20`、`ACCEL_CONFIG0 0x21`、数据 0x0B/0x11 | 完全不同（已在 QMI 手册 §4.1 核到）：`CTRL1 0x02`（接口/电源）、`CTRL2 0x03`（加速度 ODR/量程）、`CTRL3 0x04`（陀螺 ODR/量程）、`CTRL7 0x08`（sEN/aEN/gEN 使能）、`CTRL8 0x09`、**`CTRL9 0x0A` 命令协议**、温度 `0x33/0x34`、数据 `0x35…0x40` |
| SPI 上限 | 24 MHz | **15 MHz** |
| FIFO | 有 | 1536 字节（更大） |
| SPI 模式 | Mode 0/3 | Mode 0/3（两种都支持，自动识别） |
| 结论 | — | **必须新写 `qmi8658a.c/h`**，不能复用 `icm42670` 的寄存器表 |

### 3.3 异构双 IMU 的后果（要认账）

IMU#1 = ICM-42670-P、IMU#2 = QMI8658A，这是**异构冗余**：

- ✅ **能做**：交叉合理性校验（冻结/饱和/零偏漂移/数据中断/WHO_AM_I 丢失），这是实际最常见的失效模式，价值很大。
- ❌ **不能做**：直接把两路数据取平均或做 2-of-2 表决 —— 两颗的噪声密度、标度因数、零偏温漂、安装矩阵都不同，平均会把精度拉向差的那一颗。
- 建议策略：**固定一颗为主（建议 ICM-42670-P），另一颗只做门限校验**；主 IMU 失效时切换并降级姿态估计。若你更看重"可以直接平均"，那两颗都用同一型号更合适（QMI8658A 的 footprint 本来就是现成的，U2 换料即可，代价是 U2 的 pin10 也要一起改）。

### 3.4 两颗的性能对比（都是手册典型值 @25°C、VDD=1.8V）

来源：ICM-42670-P **DS-000451** Table 1/Table 2（手册第 10/11 页）；QMI8658A **13-52-25 Rev A** Table 7/8/12/14/17。

| 指标 | ICM-42670-P（IMU#1） | QMI8658A（IMU#2） | 谁好 |
|---|---|---|---|
| **陀螺噪声密度** | **0.007 °/s/√Hz = 7 mdps/√Hz** @10Hz | **13 mdps/√Hz**（高分辨率模式） | **ICM，约 1.9×** |
| **加速度噪声密度** | **100 µg/√Hz** @10Hz | **150 µg/√Hz** | **ICM，1.5×** |
| 陀螺零偏（初始） | ±1 °/s | ±10 dps | **ICM，10×** |
| 陀螺零偏温漂 | ±0.015 °/s/°C | X/Y ±0.1、Z ±0.05 dps/°C | **ICM，3–7×** |
| 陀螺灵敏度初始容差 | ±1 % | ±3 % | **ICM，3×** |
| 陀螺非线性 | ±0.1 % | ±0.2 % | ICM，2× |
| 加速度零偏（初始） | ±25 mg | ±100 mg | **ICM，4×** |
| 加速度零偏温漂 | ±0.15 mg/°C | ±1 mg/°C | **ICM，6.7×** |
| 加速度灵敏度容差 | ±1 % | ±6 % | **ICM，6×** |
| 加速度非线性 | ±0.1 % | ±0.75 % | **ICM，7.5×** |
| 交叉轴 | 陀螺 ±2 % / 加速度 ±1 % | 陀螺 ±2 % / 加速度 ±1 % | 相同 |
| RMS 噪声 | 陀螺 0.07 °/s-rms、加速度 1.0 mg-rms（@100Hz BW） | 手册未给（可由噪声密度 × √BW 估算） | — |
| 陀螺量程 | ±250/500/1000/2000 dps | **±16…±2048 dps（8 档）** | QMI 档位多 |
| 陀螺 ODR | 12.5–1600 Hz | 28–7174 Hz（**>1 kHz 噪声显著上升，手册标 RSV**） | 各有取舍 |
| 加速度 ODR | 1.5625–1600 Hz | 3–8000 Hz | QMI |
| 启动时间 | **陀螺 30 ms**、加速度 10 ms | 系统 15 ms，但**陀螺 turn-on 150 ms + 3/ODR** | **ICM 明显快** |
| 6 轴电流 | **0.55 mA** | 约 0.75–1.03 mA（112Hz 约 0.75 mA） | **ICM，约 1.5×** |
| SPI 上限 | **24 MHz** | 15 MHz | ICM |
| FIFO | 1 KB / 2.25 KB 可配，支持 **20-bit** 数据包 | 1536 B | 相当 |
| 封装 / 温度 | LGA-14 2.5×3.0×0.76；-40~+85°C | LGA-14 2.5×3.0×0.86；-40~+85°C | 相同 |
| 其它 | APEX 运动算法、自检 | 计步/敲击/运动检测、**I3C 12.5 MHz** | — |

**自洽性交叉校验**（说明这两组数不是抄错的）：ICM 表 1 里 `RMS 噪声 = 噪声密度 × √BW`：7 mdps/√Hz × √100 Hz = 70 mdps = 0.07 °/s ✓；100 µg/√Hz × √100 = 1.0 mg ✓。两条都正好对上。

**结论**：**ICM-42670-P 在几乎每一项"决定姿态估计质量"的指标上都更好**——噪声低 1.5–2×，零偏及其温漂好 3–10×，标度因数容差与非线性好 3–7×，陀螺启动快 5×，功耗低 1.5×，SPI 快 1.6×。
QMI8658A 的强项是**便宜、好买、量程/ODR 档位更多、FIFO 更大、支持 I3C**。
→ 所以 §3.3 的建议更明确了：**ICM-42670-P 做主 IMU，QMI8658A 只做交叉校验**。QMI 当"备份主 IMU"时要留意它 ±10 dps 的初始零偏与 ±0.1 dps/°C 的温漂，切换后需要重新做零偏估计。

> 以上全是**手册典型值**，不是实测。板上 VDD 是 3.3V 而手册数据标在 1.8V，实际噪声会略有差别；真要定论得上转台/静态方差实测。

---

## 4. 空闲引脚（31 个）与预留

```
PA3(25)  PA4(28)  PA15(77)
PB0(34)  PB1(35)  PB2(36)  PB4(90)  PB5(91)  PB8(95)  PB9(96)
PC0(15)  PC1(16)  PC2_C(17)  PC3_C(18)  PC13(7)  PC14(8)  PC15(9)
PD3(84)  PD4(85)  PD7(88)
PE0(97)  PE1(98)  PE3(2)  PE4(3)  PE5(4)  PE6(5)  PE7(37)  PE8(38)  PE10(40)  PE12(42)  PE15(45)
```

| 预留方向 | 落点 | 说明 |
|---|---|---|
| 备用 PWM（4 通道） | PB0/PB1/PB4/PB5 = TIM3_CH3/CH4/CH1/CH2 (AF2) | 需要第 9–12 路输出时用 |
| 补充同频 PWM（3 路） | PE8/PE10/PE12 = TIM1_CH1N/CH2N/CH3N (AF1) | 与 PWM1–4 同一定时器、同频率 |
| 第 3 路串口 | PE7/PE8 = UART7_RX/TX (AF7)；PE0/PE1 = UART8_RX/TX (AF8) | 都不占连接器，需要时引测试点 |
| 模拟输入扩展 | PC0/PC1 = ADC_INP10/11；**PC2_C/PC3_C = ADC3_INP0/INP1（H743 直连模拟脚）** | 直连脚不做模拟就是纯浪费 |
| 第二组 CAN | PB8/PB9（AF9 = FDCAN1）或 PD0/PD1（AF9=FDCAN1、PD3/PD4） | 本次决定不做，引脚留着 |
| 注意 | PA15/PB4 需先关 JTAG 才能当 GPIO；PB3 已明确留给 SWO | |

---

## 5. 顺带必须处理的两个硬件问题

| # | 问题 | 证据 | 处理 |
|---|---|---|---|
| **H1** | **D1 续流二极管极性反接** 🔴 | 符号 `1N4148TR`：**pin1 = C（阴极）**、pin2 = A；网表 `D1 {'1':'$4N4','2':'5V'}`，而 `$4N4 = BUZZER1.2 + D1.1 + Q1.3`（Q1 集电极开关节点）⇒ 阴极在开关节点、阳极在 5V，**Q1 一导通 D1 就正向导通，5V 轨近乎短路** | 对调 D1 的 pin1/pin2 网络（或整器件翻转 180°）。这条与 rev2 无关，rev1 也要改 |
| **H2** | **U9(QMI8658A) pin10 不能接 GND** 🟠 | QMI8658A 手册 Table 3 / §1.5 | 见 §3.1，改接 `3.3V_BARO` |

> 电源树与输入通路的问题（载流、压差、保险丝）按你的决定**本次不动**，记录在同目录 `飞控板/电源树检查_2026-09-15.md`。

---

## 6. 固件变更清单（引脚相关）

| # | 变更 | 说明 |
|---|---|---|
| F1 | `main.h` 引脚宏 | 新增 `IMU2_CS/SCK/MISO/MOSI/INT1/INT2`、`IMU1_INT2`、`USB_DM/DP`；SBUS 由 PA11/PA12 改 PD0/PD1 |
| F2 | `usart.c`（MspInit） | UART4 的 GPIO 从 PA11/PA12 改 **PD0/PD1 = AF8**；其余参数（100000 8E2、`RXINV`）不动 |
| F3 | 新增 SPI2 | 8bit、Mode0、软件 NSS、**预分频 16（7.5 MHz）**；SPI1 保持预分频 8 |
| F4 | 新增 `qmi8658a.c/h` | 独立驱动：`WHO_AM_I(0x00)=0x05`、CTRL 寄存器组、CTRL9 命令、1536B FIFO |
| F5 | `icm42670.c/h` | 由 `imu_test.c` 拆出，SPI 句柄 + CS 参数化，两个 IMU 实例化 |
| F6 | 中断 | 新增 `EXTI9_5`（PC5/PC6）、`EXTI15_10`（PD10/PD11）；**优先级一律 ≤5、开启内部下拉、ISR 内先读状态再清标志**（rev1 是"上升沿+无上下拉+优先级 0"，IMU 未焊时会饿死 SysTick） |
| F7 | USB | 新增 USB Device（CDC）初始化；**关闭 VBUS 检测**，用软连接控制枚举 |
| F8 | TIM4 | `Period` 49999 → 19999（50 Hz 舵机）；或与 TIM1 统一 400 Hz 电机 |
| F9 | LED | PE2 GPIO 输出 WS2812B 时序（T0H≈300ns、T1H≈800ns、位周期 1.25µs、RES≥280µs），发送期间关中断 |
| F10 | I²C | 复核目标 400 kHz（rev1 的 `Timing=0x307075B1` 按公式算是 ≈260 kHz，**未实测**） |
| F11 | 自检 | 覆盖两颗 IMU（分别读 WHO_AM_I：0x67 / 0x05）、气压、地磁、TF 卡、SBUS 帧 |

---

## 7. 怎么复核本文件

```sh
# 引脚号 ↔ 引脚名（DS12110 Rev 7 Figure 5 / Table 9）
pdftotext -layout 飞控板/h743数据手册.pdf - | grep -n "LQFP100 pinout"

# 复用功能（AF）
pdftotext -layout 飞控板/h743数据手册.pdf - | grep -n "PA11\|PB13\|PD0"

# QMI8658A 引脚定义与 SPI 上限
pdftotext -layout "飞控板/QMI8658A DATASHEET.pdf" - | grep -n "Table 3\|fSPC\|WHO_AM_I"

# WS2812B 时序
pdftotext -layout 飞控板/LED手册.pdf - | grep -n "T0H\|T1H\|RES"

# rev1 实际接线
python3 -c "import json;n=json.load(open('/tmp/nets.json'));print(n['U2'])"
```
