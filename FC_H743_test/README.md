# FC_H743_test —— 焊后外设批量自检固件

本工程用于 **STM32H743VIT6 电赛飞控板焊装完成后**，一次性批量检测板载所有外设是否焊接/工作正常，并把结果打印到 **USART1（115200）控制台**，方便快速定位坏点。

> 这是一个**独立测试固件**（CubeMX HAL + CMake，与主飞控 `FC_H743` 分开），不影响主程序。

---

## 1. 用途

焊接/维修后，连接一块 USB-TTL 到 `USART1`（TX→PA9, RX→PA10），上电后固件自动跑一遍外设自检，按条目打印 `OK` / `FAIL`，末尾给出汇总 `PASS n/n`。哪个外设 FAIL 就直接定位到对应焊点/器件。

---

## 2. 这个程序会测哪些外设

| 编号 | 外设 | 判定依据 | 预期 |
|------|------|----------|------|
| 1 | 控制台 USART1 | 能否打印输出 | OK（能看到字符即通） |
| 2 | IMU ICM-42670-P（SPI1） | `WHO_AM_I(0x75)` | `=0x67` |
| 3 | 磁力计 IST8310（I2C1） | `WIA(0x00)` | `=0x10` |
| 4 | 气压计 SPA06-003（I2C2） | `CHIP_ID(0x0D)` | `=0x11` 或 `0x10` |
| 5 | 电源 ADC（ADC1 IN16/IN17） | 采样到非零电压 | `V>0.1V`（并打印 V/I） |
| 6 | SD 卡（SDMMC1 4-bit） | 卡初始化 + 读卡信息 | 有卡则 OK（打印容量） |
| 7 | UART 发送（USART1/3, UART4） | 三个口 TX 都返回 OK | 全 OK（RX 需外部环回） |
| 8 | PWM 输出（TIM1/TIM4） | 启动 PWM 并 1000/1500/2000µs 摆动 | OK（用舵机测试仪/示波器核对） |
| 9 | GPIO 输入/输出（蜂鸣器/LED/卡检测） | 蜂鸣短鸣、LED 亮、读 `TF_CD` 等 | OK（含 TF_CD/MAG_DRDY/IMU_INT 电平） |

> 2026-09-09 代码检查后修复了一批会导致误判/无输出的问题，详见文末「修复记录」。

### 实际输出示例

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

| 功能 | 引脚 |
|------|------|
| 电机 PWM M1..M4 | TIM1 CH1..CH4 = PE9 / PE11 / PE13 / PE14 |
| 舵机/辅助 PWM | TIM4 CH1..CH4 = PD12..PD15 |
| IMU | SPI1：SCK=PA5, MISO=PA6, MOSI=PA7, CS=PC4, INT=PC5 |
| 磁力计 | I2C1：SCL=PB6, SDA=PB7, DRDY=PA2 |
| 气压计 | I2C2：SCL=PB10, SDA=PB11 |
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

- **UART RX 测试需要外部环回**：把某个 UART 的 TX 与 RX 短接（或用两根线互连），才能测接收；本程序默认只测 `TX` 是否正常（`HAL_UART_Transmit` 返回 OK），RX 环回需自行接线，否则 RX 判据留空。
- **SD 测试需插卡**：无卡会显示 `FAIL (no card)`，属于正常。
- **PWM 摆动**：`[08]` 会把 TIM1/TIM4 共八路按 `1000→1500→2000µs` 摆动一圈后停在 1500，便于用舵机测试仪/示波器确认，也可先不接电机。
- **ADC 换算为初值**：电压按 `V_adc × 4`（假设 1:4 分压），电流按 `V_adc × 10`（假设 0.1V/A）。若与实测偏差大，按实际分压/采样电阻改 `FC_H743_test/Core/Src/test.c` 中 `adc_read()` 的系数。
- **电压过低会报 FAIL**：`[05]` 判据为 `V > 0.1V`，若未接电池/电源则显示 LOW。
- **串口中文乱码**：输出的中文为 UTF-8 编码，串口助手请选 UTF-8，否则备注文字会显示乱码（OK/FAIL 不受影响）。

---

## 6. 源码结构

- `Core/Src/test.c` —— 自检主体（各外设逐项测试 + `Test_Run()` 入口）。
- `Core/Inc/test.h` —— `Test_Run()` 声明。
- `Core/Src/main.c` —— `USER CODE 2` 中调用 `Test_Run()`（所有 `MX_*_Init` 之后）。
- `cmake/gcc-arm-none-eabi.cmake` —— 已固定使用 CubeIDE 工具链路径。

> 因为自检代码写在 CubeMX 的 `USER CODE 2` 区并独立成 `test.c`，重新用 CubeMX 生成后不会丢失。

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

**遗留提醒（未改动）：**
- UART4 实际配置为 `100000 8N2 + RX 反相`。本测试固件只测 TX 无影响；但主飞控接真 SBUS 时需要 `8E2`（WordLength 9B + Parity EVEN），届时需同步修改 `FC_H743.ioc` 与生成代码。
- `MX_SDMMC1_SD_Init` 位于 `MX_USART1_UART_Init` 之前（CubeMX 固定排序）。除 SD 外的其他初始化若失败仍会无声死循环；调试时遇到"完全无输出"优先查这一段。
