# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概览

电赛飞行器（多旋翼）的 **STM32H743VIT6（LQFP100）飞控固件**。当前处于硬件驱动初始化 + 主循环骨架阶段，尚未接入应用逻辑（IMU 数据就绪中断处理是 `TODO`）。

- 工程目录命名含 `_LL` 的后缀表示"基于 **LL 库**（辅以最小 HAL）"，是全项目唯一含实际源码的工程。
- 引脚分配与硬件参照 `hw_ref/h743vitx_pins.json`（H743VITx 的引脚号 ↔ 信号表，用于规划复用功能）。

## 目录结构与职责

- `FC_H743/` — 只含 **CubeMX 工程配置与日志**，无源码：`.ioc` 工程文件、`FC_H743_backup_20260908.ioc` 备份、`generate.txt`（无头调用 CubeMX 生成代码的批处理命令）、`cli_*.log`（那批命令的终端输出）。`.ioc` 是 HAL 工程模式，属硬件配置的事实来源/存档。
- `FC_H743_LL/` — **活跃固件工程**（LL 模式）。含手写的 `Core/` 源码、`CMakeLists.txt`、`Startup/`、链接脚本 `STM32H743VITX_FLASH.ld`，以及已配置好的 `build/`（CMake + Ninja）。其 `.ioc`（LL 工程模式）用于与源码保持外设配置一致。
- `hw_ref/h743vitx_pins.json` — 板级引脚参考。

注意：本目录不是 git 仓库，无版本历史可依。

## 构建

裸机固件，无测试框架、无 lint。构建产物在 `FC_H743_LL/build/`（`.elf/.bin/.hex/.map`）。

工具链：`arm-none-eabi-gcc`（Cortex-M7, 硬浮点 `fpv5-d16`），本机取自 STM32CubeIDE 14.3 自带工具，路径含版本号（见 `build/CMakeCache.txt`）。固件包 `STM32Cube_FW_H7` 默认路径写在 `CMakeLists.txt` 的 `FW_ROOT`（本机 `V1.13.0`）。两条路径都随本机环境而变，换机/升级 CubeIDE 后需重新配置：

```sh
cd FC_H743_LL
# 首次 / 环境变化后重新配置（编译器路径与 FW_ROOT 按本机实际替换）
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER="$HOME/.../tools/bin/arm-none-eabi-gcc" \
  -DCMAKE_ASM_COMPILER="$HOME/.../tools/bin/arm-none-eabi-gcc" \
  -DFW_ROOT="/Users/origin/STM32Cube/Repository/STM32Cube_FW_H7_V1.13.0"

cmake --build build        # 增量编译（等价于在 build/ 里跑 ninja）
```

## 固件架构要点

- **`LL` 为主、HAL 仅用于 SDMMC**：`CMakeLists.txt` 同时定义 `USE_FULL_LL_DRIVER` 与 `USE_HAL_DRIVER`，但 HAL 只链接了 `hal`/`hal_cortex`/`hal_rcc(_ex)`/`hal_sd`。SDMMC1 走 HAL 是因为 H7 官方没有 SDMMC 的 LL 封装（见 `main.c` 注释）。
- **代码是手写/定制维护的**（`main.c` 头注释标 `@author Origin's assistant (Hanako)`），且**没有 CubeMX 的 `USER CODE BEGIN/END` 保护段**——从 `.ioc` 重新生成代码会覆盖手写内容。改外设初始化请直接编辑 `main.c`，并保持与 `.ioc` 一致；不要盲目用 CubeMX 重新生成。
- **头文件顺序约束**（`main.h` 注释说明）：`stm32h7xx_hal.h` 必须最先 include（因定义了 `USE_HAL_DRIVER`，`stm32h7xx.h` 会自动引入 HAL），随后才是各 `stm32h7xx_ll_*.h`。
- **主时钟 480 MHz**：HSE 25 MHz → PLL1（M=5,N=192,P=2，VCO 960），PLL2P=240 MHz 供 ADC；AHB=480，APB1/2/4=240 MHz，Flash 4WS，VOS1。入口 `main()` 先开 I/D-Cache 再配时钟。
- **中断**（`Core/Src/stm32h7xx_it.c`）：`EXTI9_5_IRQHandler` 处理 PC5 的 IMU_INT1（下降沿，线 5），处理逻辑留 `TODO`；fault handler 均为死循环。`SysTick_Handler` 只调 `HAL_IncTick()`（喂 HAL tick 供 SDMMC 初始化超时；LL tick 是硬件直读，无需 ISR）。
- PWM 用 **LL 寄存器级**直接改占空比即可（`LL_TIM_OC_SetCompareValue`），无需走 HAL。TIM1 是高级定时器，注意必须已调用 `LL_TIM_EnableAllOutputs` 才有输出。
- 全工程开了 D-Cache（`SCB_EnableDCache()`），今后引入 DMA（SDMMC/ADC DMA）时缓冲区需做 cache 维护或用非缓存内存。

## 外设分配总览

规范来源：`FC_H743_LL/Core/Inc/main.h` 头部注释、`Core/Src/main.c` 的 `MX_*_Init`。改接线前先查这两处与 `hw_ref/`。

| 外设 | 引脚 / 通道 | 用途（波特率/参数） |
|---|---|---|
| ADC1 | PA0=IN16, PA1=IN17 | 电压/电流采样，序列长度 2 |
| SPI1 | PA5/6/7 + PC4 CS | IMU（软件片选），8bit 主模式 |
| I2C1 | PB6/PB7 | 磁力计等，400 kHz |
| I2C2 | PB10/PB11 | 气压计等，400 kHz |
| USART1 | PA9/PA10 | 数传/调试，115200 |
| UART4 | PA12 TX / PA11 RX | GPS，57600 |
| USART3 | PD8/PD9 | 数传2，57600 |
| TIM1 CH1–CH4 | PE9/11/13/14 | PWM 输出组（PSC=600, ARR=1999 ≈ 400 Hz，初始 50%） |
| TIM4 CH1–CH4 | PD12–15 | PWM 输出组（同上） |
| SDMMC1 | PC8–12 + PD2 | TF 卡 4bit（PA8 = 卡检测），走 HAL |
| GPIO 输出 | PC4=IMU_CS(高)、PC7=BUZZER(低)、PE2=WS2812 | — |
| GPIO 输入 / EXTI | PC5=IMU_INT1(EXTI5 降沿)、PA2=MAG_DRDY、PA8=TF_CD | — |
| HSE | PH0/PH1 | 25 MHz 晶振 |
| SWD | PA13/PA14 | 调试 |

TIM1/TIM4 两条 4 通道 PWM 组在代码里尚未定义具体通道 → 电机/舵机的对应关系（飞控里通常是电调输出），需要时在应用层按飞控定义区分。

## 内存布局（链接脚本）

`STM32H743VITX_FLASH.ld`（从 NUCLEO-H743ZI 模板改）：
- ROM = 0x08000000, 2048K（Flash）
- RAM = 0x20000000, 128K（AXI SRAM）——栈顶 `_estack` 在 RAM 末尾
- ITCMRAM = 0x00000000, 64K（当前未用）
- 链接参数 `-Wl,--gc-sections` + `-ffunction-sections`/`-fdata-sections`，`-specs=nano.specs`
