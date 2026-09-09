# ============================================================================
# FC_H743_LL - STM32H743VIT6 飞行控制器（LL 库工程）
# 目标：arm-none-eabi-gcc, Cortex-M7, 软硬浮点 fpv5-d16
# 依赖：STM32Cube_FW_H7 固件包（默认指向本机 Repository，可改用任意路径）
# ============================================================================
cmake_minimum_required(VERSION 3.22)
project(FC_H743_LL C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard")

# ----------------------------------------------------------------------------
# 固件包路径（修改此处即可切换 STM32Cube_FW_H7 版本）
# ----------------------------------------------------------------------------
set(FW_ROOT "/Users/origin/STM32Cube/Repository/STM32Cube_FW_H7_V1.13.0" CACHE PATH "STM32Cube FW_H7 root")
set(CMSIS_DEV  ${FW_ROOT}/Drivers/CMSIS/Device/ST/STM32H7xx)
set(CMSIS_CORE ${FW_ROOT}/Drivers/CMSIS/Include)
set(HAL_DRIVER ${FW_ROOT}/Drivers/STM32H7xx_HAL_Driver)

# ----------------------------------------------------------------------------
# 宏定义：STM32H743xx / LL 全驱动 / HAL（SDMMC 混合）
# ----------------------------------------------------------------------------
add_compile_definitions(
  STM32H743xx
  USE_FULL_LL_DRIVER
  USE_HAL_DRIVER
)

# ----------------------------------------------------------------------------
# 源文件
# ----------------------------------------------------------------------------
# 应用层
set(APP_SOURCES
  Core/Src/main.c
  Core/Src/stm32h7xx_it.c
  Core/Src/syscalls.c
  Core/Src/sysmem.c
)

# LL 驱动（仅本项目用到的模块）
set(LL_SOURCES
  ${HAL_DRIVER}/Src/stm32h7xx_ll_adc.c
  ${HAL_DRIVER}/Src/stm32h7xx_ll_exti.c
  ${HAL_DRIVER}/Src/stm32h7xx_ll_gpio.c
  ${HAL_DRIVER}/Src/stm32h7xx_ll_i2c.c
  ${HAL_DRIVER}/Src/stm32h7xx_ll_pwr.c
  ${HAL_DRIVER}/Src/stm32h7xx_ll_rcc.c
  ${HAL_DRIVER}/Src/stm32h7xx_ll_sdmmc.c
  ${HAL_DRIVER}/Src/stm32h7xx_ll_spi.c
  ${HAL_DRIVER}/Src/stm32h7xx_ll_tim.c
  ${HAL_DRIVER}/Src/stm32h7xx_ll_usart.c
  ${HAL_DRIVER}/Src/stm32h7xx_ll_utils.c
)

# HAL 驱动（基础 + CORTEX + SDMMC。H7 无 SDMMC LL 封装，按官方混合模式）
set(HAL_SOURCES
  ${HAL_DRIVER}/Src/stm32h7xx_hal.c
  ${HAL_DRIVER}/Src/stm32h7xx_hal_cortex.c
  ${HAL_DRIVER}/Src/stm32h7xx_hal_rcc.c
  ${HAL_DRIVER}/Src/stm32h7xx_hal_rcc_ex.c
  ${HAL_DRIVER}/Src/stm32h7xx_hal_sd.c
)

# CMSIS
set(CMSIS_SOURCES
  ${CMSIS_DEV}/Source/Templates/system_stm32h7xx.c
)

# ----------------------------------------------------------------------------
# 目标
# ----------------------------------------------------------------------------
add_executable(${PROJECT_NAME}.elf
  ${APP_SOURCES}
  ${LL_SOURCES}
  ${HAL_SOURCES}
  ${CMSIS_SOURCES}
  Startup/startup_stm32h743xx.s
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
  Core/Inc
  ${CMSIS_DEV}/Include
  ${CMSIS_CORE}
  ${HAL_DRIVER}/Inc
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
  -mcpu=cortex-m7
  -mfpu=fpv5-d16
  -mfloat-abi=hard
  -Wall
  -Werror=implicit-function-declaration
  -fdata-sections
  -ffunction-sections
  -g3
  -O1
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
  -T${CMAKE_CURRENT_SOURCE_DIR}/STM32H743VITX_FLASH.ld
  -static
  -Wl,--gc-sections
  -specs=nano.specs
  -Wl,-Map=${PROJECT_NAME}.map
)
