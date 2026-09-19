# FC_H743_test —— 焊后外设批量自检固件

本工程用于 **STM32H743VIT6 电赛飞控板焊装完成后**，一次性批量检测板载所有外设是否焊接/工作正常，并把结果打印到 **USART1（115200）控制台**，方便快速定位坏点。

> 这是一个**独立测试固件**（CubeMX HAL + CMake，与主飞控 `FC_H743` 分开），不影响主程序。

---

## 0. 先读这节：三个踩过的坑

### 0.1 `SysTick_Handler` 缺失 → 整板静默死机【最严重】

`Core/Src/stm32h7xx_it.c` 里**必须**有 `SysTick_Handler`（内容就是调 `HAL_IncTick()`）。

**缺了会怎样**：链接器把 SysTick 向量解析到 startup 文件里的弱符号 `Default_Handler`（`Infinite_Loop: b Infinite_Loop` 死循环）。`HAL_Init()` 启动 SysTick 后约 **1ms**，内核就永久卡死 —— `SystemClock_Config()`、所有外设初始化、一切串口输出**都永远不会发生**。表现是"上电后串口一声不吭"，极易误判成硬件坏。

**它真的发生过**：2026-09-13 实测该文件里没有这个函数，意味着此前所有基于本固件的"实机结论"**从未成立过**。

**怎么确认**：halt 后若 `pc` 落在 `Default_Handler` 附近、且 `xPSR` 低 8 位为 `0x0f`（异常号 15 = SysTick），即是此症。

**相关**：`HardFault_Handler` / `MemManage` / `BusFault` / `UsageFault` 目前也都是这个弱死循环 —— 任何硬件访问出错都会静默卡死而不报错，排查时建议先把它们改成"打印故障类型和出错地址再挂起"。

### 0.2 CH340 串口：不拉 DTR 就一个字节都读不到

板上 Type-C 走的是 CH340N（`/dev/cu.usbserial-*`）。**打开端口必须拉起 DTR/RTS**，否则 CH340 不转发数据：`cat` / `perl` 直接 `open()` 会一直读到 0 字节，而设备节点存在、`stty` 也不报错 —— 非常容易误判成"固件没输出"。

- 用 `screen -L -dmS <名> /dev/cu.usbserial-* 115200`（会自动拉 DTR，日志落在当前目录 `screenlog.0`）
- 或用 Python `termios` + `fcntl.ioctl(fd, TIOCMBIS, TIOCM_DTR|TIOCM_RTS)`
- **串口是独占的**：自己的后台读取进程和串口助手会互相抢，抓之前先确认端口没被占用

### 0.3 IST8310 地磁：配置对了也不出数，必须先软复位

见 §5 的说明。**只把 `CNTL1` 设成连续测量是不够的** —— 芯片可能停在"配置正确但测量引擎没跑"的状态（`CNTL1` 读回来明明是 `0x0B`，数据寄存器却恒为 0、`STAT1` 的 DRDY 永不置位）。**必须先写 `CNTL2` bit0 做一次软复位**，或做一次 standby→测量的模式切换。

---

## 0.9 本文档引脚表已实测校正（2026-09-14）

**原有引脚表与实物不符**，排查时照表走真实地浪费了一整晚。冲突时以本节为准：

| 项目 | 原文写的 | **实测（以此为准）** |
|---|---|---|
| 磁力计 IST8310 | I2C1（PB6/PB7），地址 `0x0E` | **I2C2（PB10=SCL / PB11=SDA），7 位地址 `0x0C`** |
| 气压计 SPA06 | I2C2 | 板上**未焊接** |
| IMU ICM-42670-P | SPI1：PA5/6/7 + CS=PC4 | **版图核对一致，这部分原文是对的** |

> **教训：排查任何外设前，先实测确认它挂在哪条总线、哪个地址，不要直接照本表。**
>
> 实用做法：I2C 用**全地址扫描**（`0x08~0x77`）确定器件在哪条总线、哪个地址；SPI 则要靠"复位值已知的寄存器"验证读数是否可信（见 §5）。

---

## 1. 用途

焊接/维修后，连接一块 USB-TTL 到 `USART1`（TX→PA9, RX→PA10），上电后固件自动跑一遍外设自检，按条目打印 `OK` / `FAIL`，末尾给出汇总 `PASS n/n`。哪个外设 FAIL 就直接定位到对应焊点/器件。

---

## 2. 这个程序会测哪些外设

| 编号 | 外设 | 判定依据 | 预期 |
|------|------|----------|------|
| 1 | 控制台 USART1 | 能否打印输出 | OK（能看到字符即通） |
| 2 | IMU ICM-42670-P（SPI1） | `WHO_AM_I(0x75)` | `=0x67` |
| 3 | 磁力计 IST8310（**I2C2，地址 0x0C**） | `WIA(0x00)` | `=0x10` |
| 4 | 气压计 SPA06-003（I2C2） | `CHIP_ID(0x0D)` | `=0x11` 或 `0x10` |
| 5 | 电源 ADC（ADC1 IN16/IN17） | 采样到非零电压 | `V>0.1V`（并打印 V/I） |
| 6 | SD 卡（SDMMC1 4-bit） | 卡初始化 + 读卡信息 | 有卡则 OK（打印容量） |
| 7 | UART 发送（USART1/3, UART4） | 三个口 TX 都返回 OK | 全 OK（RX 需外部环回） |
| 8 | PWM 输出（TIM1/TIM4） | 启动 PWM 并 1000/1500/2000µs 摆动 | OK（用舵机测试仪/示波器核对） |
| 9 | GPIO 输入/输出（蜂鸣器/LED/卡检测） | 蜂鸣短鸣、LED 亮、读 `TF_CD` 等 | OK（含 TF_CD/MAG_DRDY/IMU_INT 电平） |

> 2026-09-09 代码检查后修复了一批会导致误判/无输出的问题，详见文末「修复记录」。

### 期望输出示例

> **注意**：下面是**设计目标**，不是实测结果。截至 2026-09-14，由于 `SysTick_Handler` 缺失（§0.1），**这份自检固件在此前从未在实机上完整跑通过**，本示例中的数字均为示意。实测情况见 §7 第三轮。实际跑下来会是：`[02]` IMU FAIL（芯片无响应）、`[04]` 气压计 FAIL（未焊接）、`[06]` SD 视有无卡而定。

```
=== FC_H743 PERIPHERAL SELF-TEST ===
[01] Console USART1 .......... OK  ->115200
[02] IMU ICM42670 SPI1 ....... OK
        WHO_AM_I=0x67
[03] Mag IST8310 I2C1 ........ OK
        WIA=0x10
[04] Baro SPA06 I2C2 ......... OK
        CHIP_ID=0x11
[05] ADC V/I ................. OK
        V=12.60V  I=0.30A
[06] SDMMC SD card ........... OK
        Capacity=30532MB Block=512
[07] UART TX (1/3/4) ......... OK  (RX 需外部环回)
[08] PWM TIM1/TIM4 ........... OK  started
[09] GPIO IO (beep/LED/card) . OK
        TF_CD=1 (0=有卡)  IMU_INT=0

=== RESULT: 9/9 PASS, 0 FAIL ===
```

---

## 3. 如何构建与烧录

### 构建
推荐用 **STM32CubeIDE 自带工具链**（本机 PATH 上的 `homebrew arm-none-eabi-gcc 15.2` 缺 newlib 头文件，无法编译）。`cmake/gcc-arm-none-eabi.cmake` 已固定指向 CubeIDE 14.3 自带 gcc，升级 CubeIDE 后需同步 `TOOLCHAIN_ROOT`。

```bash
cd FC_H743_test
cmake --preset Debug
cmake --build build/Debug -j4
# 产物：build/Debug/FC_H743.elf
```

### 烧录
将生成的 `FC_H743.elf` 用 **STM32CubeProgrammer / OpenOCD**（SWD：SWDIO/SWCLK）烧录，或用 CubeIDE 直接下载。

### 运行
- 用 USB-TTL 接 `USART1`：TX=PA9（板端 RX）、RX=PA10（板端 TX），**注意交叉连接**，共地。
- 串口助手设 **115200, 8N1**。
- 上电/复位，即可看到自检输出。

---

## 4. 硬件引脚参考（本板）

> ⚠️ **本表部分内容已实测校正，先看 §0.9。** 尤其磁力计一行：实际在 **I2C2 / 地址 0x0C**。

| 功能 | 引脚 |
|------|------|
| 电机 PWM M1..M4 | TIM1 CH1..CH4 = PE9 / PE11 / PE13 / PE14 |
| 舵机/辅助 PWM | TIM4 CH1..CH4 = PD12..PD15 |
| IMU | SPI1：SCK=PA5, MISO=PA6, MOSI=PA7, CS=PC4, INT=PC5 |
| 磁力计 IST8310 | **I2C2：SCL=PB10, SDA=PB11**，地址 **0x0C**（原表写的 I2C1/PB6/PB7 是错的） |
| 气压计 SPA06 | 原表写 I2C2 —— 但板上**未焊接**，I2C1（PB6/PB7）实测无任何器件 |
| 电源采样 | ADC1 IN16=PA0(电压), IN17=PA1(电流) |
| SD 卡 | SDMMC1 4-bit：PC8/9/10/11(D0-D3) + PD2(CMD) + PC12(CK) |
| 控制台 | USART1：PA9(TX), PA10(RX) |
| 数传    | USART3：PD8(TX), PD9(RX) |
| SBUS 接收 | UART4：PA11(RX), PA12(TX)（RX 反相；当前实际配置 100000 8N2） |
| 蜂鸣器 | PC7 |
| WS2812 LED | PE2 |
| SD 卡检测 TF_CD | PA8 |
| USB 烧录 | U5=CH340N + Type-C |

> **注意：** 本期为布局与串口烧录方便，**调换过几个 UART 外设**，且**数传现借用 GPS 的 UART**，故 **GPS 暂不接入**。实际以本板最终焊接与 `FC_H743.ioc` 为准。

---

## 5. 注意事项 / 判据说明

- **地磁 IST8310 必须先软复位才会出数**（实测 2026-09-14）：芯片可能停在"配置正确但测量引擎没跑"的状态 —— 读 `CNTL1` 明明已是 `0x0B`（连续测量），数据寄存器却恒为 0、`STAT1` 的 DRDY 永不置位。**必须先写 `CNTL2`(0x0B) bit0 做一次软复位**，或做一次 standby(`CNTL1=0x00`)→测量的模式切换，它才开始出数。光设 `CNTL1` 不够。

  已验证可用的配方：I2C2 + 地址 `0x0C`；寄存器 `0x00`=WIA(`0x10`)、`0x02`=STAT1、`0x03~0x08`=X/Y/Z（**小端 int16**）、`0x09`=STAT2、`0x0A`=CNTL1、`0x0B`=CNTL2、`0x41`=AVGCNTL、`0x42`=PDCNTL；换算 **0.3 µT/LSB**（实测矢量模长 ≈31 µT，落在地磁 25~65 µT 的合理区间）。配置值：`CNTL1=0x0B`、`CNTL2=0x08`、`AVGCNTL=0x24`、`PDCNTL=0xC0`。

- **读外设数据时，别把"HAL 返回非 OK"当小事，也别把"稳定的值"当真实**（实测教训）：SPI 传输超时（`HAL_TIMEOUT`）时接收缓冲区**根本没被写入**，里面是**栈残留垃圾**。典型特征是：**不同的寄存器读出同一个值**（曾出现 `0x00`/`0x01`/`0x75` 三个寄存器都读出 `0x48`）、**多行转储逐行完全重复**。这曾被误判成"芯片应答了"。

  **可靠的判据是：读几个手册给出复位值的寄存器**（ICM-42670-P 的 `0x00` MCLK_RDY 应为 `0x01`、`0x01` DEVICE_CONFIG 应为 `0x04`、`0x75` WHO_AM_I 固定 `0x67`）——**不同的输入必须给出不同的输出**，才算通信可信。

- **ADC 自检的 `OK` 可能是假通过**（实测 2026-09-14）：曾出现 `V=6.60V I=16.50A`，反推原始值两个通道**都正好是 `0x8000`（满量程中点）** —— 两个独立输入恰好停在正中间是不可能的，说明 ADC 返回了固定值而非真实采样。判据 `V>0.1V` 挡不住这种情况，**这一项要通过时最好同时看数值是否合理**。

- **UART RX 测试需要外部环回**：把某个 UART 的 TX 与 RX 短接（或用两根线互连），才能测接收；本程序默认只测 `TX` 是否正常（`HAL_UART_Transmit` 返回 OK），RX 环回需自行接线，否则 RX 判据留空。
- **SD 测试判读**：无卡显示 `FAIL (no card)`，属正常；若显示 `FAIL (card-in but init fail)` 且 `TF_CD=0`，说明卡在位但通信失败——优先查 CMD/D0-D3/CK 焊点与上拉，其次换卡。SD 初始化内置 3 次重试，兼容上电偏慢的卡。
- **PWM 摆动**：`[08]` 会把 TIM1/TIM4 共八路按 `1000→1500→2000µs` 摆动一圈后停在 1500，便于用舵机测试仪/示波器确认，也可先不接电机。
- **ADC 换算为初值**：电压按 `V_adc × 4`（假设 1:4 分压），电流按 `V_adc × 10`（假设 0.1V/A）。若与实测偏差大，按实际分压/采样电阻改 `FC_H743_test/Core/Src/test.c` 中 `adc_read()` 的系数。
- **电压过低会报 FAIL**：`[05]` 判据为 `V > 0.1V`，若未接电池/电源则显示 LOW。
- **串口中文乱码**：输出的中文为 UTF-8 编码，串口助手请选 UTF-8，否则备注文字会显示乱码（OK/FAIL 不受影响）。

---

## 6. 源码结构

- `Core/Src/test.c` —— 自检主体（各外设逐项测试 + `Test_Run()` 入口）。
- `Core/Inc/test.h` —— `Test_Run()` 声明。
- `Core/Src/imu_test.c` / `Core/Inc/imu_test.h` —— ICM-42670-P 数据流与排查工具（MISO/INT 线体检、bit-bang 对照、模式 0/3 对比）。**尚未跑通**，见 §7 第三轮。
- `Core/Src/main.c` —— 顶部 `TEST_MODE` 宏切换测试模式；`USER CODE 4` 里放各模式的诊断函数。
- `cmake/gcc-arm-none-eabi.cmake` —— 已固定使用 CubeIDE 工具链路径。

### `TEST_MODE` 模式一览（`main.c` 顶部改这一个宏）

| 模式 | 值 | 用途 |
|---|---|---|
| `TEST_MODE_FULL_SELFTEST` | 0 | 完整 9 项外设自检（`Test_Run()`） |
| `TEST_MODE_SERIAL_ONLY` | 1 | 只初始化 USART1，主循环打心跳（验证串口链路） |
| `TEST_MODE_IMU_STREAM` | 2 | 读 ICM-42670-P 并流式打印 |
| `TEST_MODE_I2C_SCAN` | 3 | I2C 总线诊断：空闲电平 + 全地址扫描（**定位器件挂在哪条总线/哪个地址**，就是靠它发现地磁在 I2C2/0x0C） |
| `TEST_MODE_MAG_STREAM` | 4 | 读 IST8310 地磁并流式打印（含软复位与配置流程） |

每个模式只初始化自己需要的外设 —— 无关外设的初始化失败既浪费时间，也可能在 USART1 就绪前把流程带进 `Error_Handler`。

> ### ⚠️ 用 CubeMX 重新生成代码后必看
>
> 实测（2026-09-09）重新生成会丢失的东西比想象多，**重新生成后请按此清单核对**：
>
> 1. **`test.c` 会从构建里消失**：CubeMX 会重写 `cmake/stm32cubemx/CMakeLists.txt`，而它不认识 `test.c` → `undefined reference to Test_Run`。现在 `test.c` 已登记在**根 `CMakeLists.txt`**（CubeMX 不覆盖该文件），正常情况下直接构建即可；若再遇到该报错，检查根 CMakeLists 的 user sources。
> 2. **`sdmmc.c` 的"无卡不进 Error_Handler"补丁会被还原**：即使还原也有第二道保险——`main.c` 的 `Error_Handler()`（USER CODE 区，再生成不覆盖）已改为打印 `!INIT-ERR!` 后**返回**而非挂死，主流程继续、`test_sd()` 照常给出 FAIL。建议仍按 `Core/Src/sdmmc.c` 内注释重新应用补丁，避免每次上电多打印一行 `!INIT-ERR!`。
> 3. **SPI1 DataSize 可能回退 4BIT**：`.ioc` 已记录 `SPI1.DataSize=SPI_DATASIZE_8BIT`，正常会保留；即使回退，`test.c` 的 `ensure_spi8()` 也会在运行时纠正。
> 4. `main.c` 的 `Test_Run()` 调用、TIM/TF_CD 上拉等若异常，对照 `.ioc`（SPI DataSize / TIM1 CH4 脉宽 / PA8 上拉均已记录在案）。

---

## 7. 修复记录（2026-09-09 全量代码检查）

| # | 文件 | 问题 | 影响 | 修复 |
|---|------|------|------|------|
| 1 | `Core/Src/sdmmc.c` | 未插卡时 `HAL_SD_Init` 失败 → `Error_Handler()` 死循环 | **最严重**：该初始化在 USART1 之前执行，无卡上电整板串口无任何输出，与"无卡显示 FAIL"的设计意图矛盾 | 不再进 `Error_Handler`，由 `test_sd()` 判定并打印 `FAIL (no card)`。⚠️ CubeMX 重新生成会还原此修改，需重打补丁 |
| 2 | `Core/Src/spi.c` | `DataSize = SPI_DATASIZE_4BIT`（CubeMX 对 H7 SPI 的默认值是个坑） | SPI 帧宽 4 位，ICM-42670 寄存器读写完全错位，`[02]` 永远 FAIL | 改为 `SPI_DATASIZE_8BIT`，并在 `FC_H743.ioc` 中显式记录 |
| 3 | `Core/Src/test.c` | I2C 地址未按 HAL 要求左移 1 位（IST8310 传 `0x0E`、SPA06 传 `0x76`） | `[03]`/`[04]` 即使器件完好也永远 FAIL | 改为 `0x0E<<1`、`0x76<<1` |
| 4 | `Core/Src/test.c` | `__HAL_TIM_SET_COMPARE(&htim1, ch, ...)` 的 `ch` 用了 1..4，而 `TIM_CHANNEL_x` 实为 0/4/8/12 | 所有写比较值都落到 CCR4：CH1-3 不摆动，CH4 停在 2000µs 与文档不符 | 按 `TIM_CHANNEL_1..4` 宏遍历，且 TIM1/TIM4 各四路全部参与摆动、结束停在 1500µs |
| 5 | `Core/Src/tim.c` | TIM1 CH4 初始脉宽为 0（`.ioc` 中 CH4 脉宽键名遗留错误） | M4 上电无波形，与 M1-M3=1500µs 不一致 | 脉宽改 1500，并修正 `.ioc` 键名 `Pulse-PWM Generation4 CH4` |
| 6 | `cmake/gcc-arm-none-eabi.cmake` | newlib-nano 未加 `-u _printf_float` | `%.2f`/`%.0f` 打印空白：电压/电流/SD 容量都没有数字（实机才会发现） | 链接参数增加 `-u _printf_float`（FLASH 63K→73K，可忽略） |

### 第二轮（同日，针对"TF 卡 bug"及 CubeMX 再生成事故）

| # | 文件 | 问题 | 影响 | 修复 |
|---|------|------|------|------|
| 7 | `cmake/stm32cubemx/CMakeLists.txt`（CubeMX 重写） | 中途有人重新生成过代码，`test.c` 被移出源列表 | 工程直接无法链接：`undefined reference to Test_Run` | `test.c` 改登记在**根 `CMakeLists.txt`**（CubeMX 不覆盖），一次修复永久生效 |
| 8 | `Core/Src/test.c`（`test_sd`） | 无卡与"卡在位但通信失败"都打印 `FAIL (no card)`，无法指导排查；HAL 失败路径 `ErrorCode` 累积不清理 | 现场会把好卡当坏卡换、或漏查焊点 | 重试 3 次 + 每次清 `ErrorCode`；结合 `TF_CD` 区分 `no card` / `card-in but init fail` 并打印错误码 |
| 9 | `Core/Src/gpio.c` | TF_CD（PA8）输入无上拉，卡座无外部上拉时无卡读数悬空随机 | [8] 的无卡/焊点不良判据不可靠 | TF_CD 启用内部上拉（`.ioc` 已记录 `GPIO_PuPd`），无卡稳定读 1、有卡读 0 |
| 10 | `Core/Src/main.c`（`Error_Handler`） | 原版 `__disable_irq()+while(1)` 静默挂死，且 SD 初始化失败必经此处 | TF 卡问题的根源；再生成后 [1] 号补丁被还原就复发 | USER CODE 区改为：UART 就绪则打印 `!INIT-ERR!` 后**返回**，绝不静默挂死（再生成不覆盖，属永久保险） |
| 11 | `Core/Src/test.c`（`ensure_spi8`） | 再生成后 `spi.c` DataSize 回退 4BIT（实测发生了） | IMU 测试失效 | `Test_Run()` 入口运行时强制 8 位帧（`test.c` 不在再生成范围，永久保险） |

> 第 7-11 项均因同一根因：**CubeMX 再生成会还原一切它认识的文件**。第 7、10、11 项已做成再生成也夺不走的修复；第 1、2、5 项补丁仍需在再生成后重新应用（`.ioc` 已记录配置，正常再生成会保留其效果）。

**遗留提醒（未改动）：**
- UART4 实际配置为 `100000 8N2 + RX 反相`。本测试固件只测 TX 无影响；但主飞控接真 SBUS 时需要 `8E2`（WordLength 9B + Parity EVEN），届时需同步修改 `FC_H743.ioc` 与生成代码。
- `MX_SDMMC1_SD_Init` 位于 `MX_USART1_UART_Init` 之前（CubeMX 固定排序）。即使 `Error_Handler` 不再挂死，其他初始化失败也只能靠 `!INIT-ERR!` 提示定位；遇到该提示时按上表顺序排查。

### 第三轮（2026-09-13/14，实机联调）

这一轮才第一次真正在实机上跑起来，暴露出前两轮从代码检查里看不出来的问题。

| # | 文件 | 问题 | 影响 | 修复 |
|---|------|------|------|------|
| 12 | `Core/Src/stm32h7xx_it.c` | **`SysTick_Handler` 缺失**，落到 startup 的弱符号 `Default_Handler`（死循环） | **最严重**：`HAL_Init()` 后约 1ms 内核永久卡死，`SystemClock_Config()`、所有外设初始化、串口输出**全部不会发生**。此前所有"实机结论"因此从未成立 | 补上 `SysTick_Handler`（调 `HAL_IncTick()`）。详见 §0.1 |
| 13 | `Core/Src/imu_test.c`、`main.c` | 把 SPI 传输失败（`HAL_TIMEOUT`）时的**栈残留垃圾**当成芯片应答 | 三个不同寄存器都读出同一个值（`0x48`）、转储逐行重复，却被判成"ID 不符"，白绕一整轮 | 传输非 `HAL_OK` 时明确报"数据无效"，不再返回缓冲区内容；并增加"读复位值已知的寄存器"作为通信可信度判据 |
| 14 | `Core/Src/main.c` | IMU_INT1(PC5) 的 EXTI 被配成"上升沿中断 + 无上下拉"、**优先级 0（高于 SysTick 的 15）** | IMU 未焊/INT 悬空时疯狂触发中断**饿死 SysTick**，`HAL_Delay` 永久卡住、自检中途静止 —— 症状与 #12 一模一样，极难分辨 | 轮询式测试模式下一律 `HAL_NVIC_DisableIRQ(EXTI9_5_IRQn)` |
| 15 | `README.md` | **引脚表与实物不符**：地磁写成 I2C1/`0x0E` | 整晚在一条**没有任何器件**的总线上排查"上拉缺失/走线断裂" | 见 §0.9 实测校正表 |

**本轮实测确认（供后续参考）**：

- **ICM-42670-P 寄存器表以 `FC_H743/Core/Src/fc/fc_imu.c` 为准，并经手册 DS-000451 逐条核对无误** —— `WHO_AM_I=0x75`(固定 `0x67`)、`PWR_MGMT0=0x1F`、`GYRO_CONFIG0=0x20`、`ACCEL_CONFIG0=0x21`、`TEMP_DATA1=0x09`、`ACCEL_DATA_X1=0x0B`、`GYRO_DATA_X1=0x11`。SPI 协议（首字节 bit7=1 读 + 7 位地址、MSB first、上升沿锁存、最大 24MHz）与固件实现完全一致。
- **`[02]` IMU 截至 2026-09-14 仍无响应**：`MISO(PA6)` 体检显示 CS 高低都不被驱动（线悬空），SPI 传输超时，所有读数均为栈残留。该芯片曾**三个角短路**（已修复）并在此状态下反复上电工作。**待办**：上电量 IMU 的 VDD 焊盘对 GND；若供电正常仍无响应，则判定芯片损坏、需换件。
- **`[03]` 地磁已跑通**（配方见 §5），`[04]` 气压计因**未焊接**必然 FAIL，属正常。
