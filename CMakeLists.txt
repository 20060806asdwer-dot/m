# 【21 构建清单：选配置、收集源码、生成固件】
cmake_minimum_required(VERSION 3.22)
project(RailNode C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# A 六个构建配置及对应功能开关
set(PROFILE bringup CACHE STRING "bringup/sensors/wireless/wireless_ack/adaptive/fixed")
set(STAGE 0)
set(LOW_POWER 0)
set(ENFORCE 0)
set(ADAPTIVE 1)
set(LOGGING 1)
set(RADIO_ACK 0)
if(PROFILE STREQUAL "sensors")
  set(STAGE 1)
elseif(PROFILE STREQUAL "wireless")
  set(STAGE 2)
elseif(PROFILE STREQUAL "wireless_ack")
  set(STAGE 2)
  set(RADIO_ACK 1)
elseif(PROFILE STREQUAL "adaptive" OR PROFILE STREQUAL "fixed")
  set(STAGE 2)
  set(LOW_POWER 1)
  set(ENFORCE 1)
  set(LOGGING 0)
  if(PROFILE STREQUAL "fixed")
    set(ADAPTIVE 0)
  endif()
elseif(NOT PROFILE STREQUAL "bringup")
  message(FATAL_ERROR "Unknown PROFILE=${PROFILE}")
endif()
# B 第三方驱动与应用文件路径
set(HAL firmware/Drivers/STM32F1xx_HAL_Driver)
set(DEVICE firmware/Drivers/CMSIS/Device/ST/STM32F1xx)
set(APP firmware/Core)
# C 参与编译的应用和启动文件
add_executable(railnode
  ${APP}/Src/main.c ${APP}/Src/board.c ${APP}/Src/sensors.c ${APP}/Src/radio.c ${APP}/Src/syscalls.c
  ${APP}/Src/features.c ${APP}/Src/packet.c ${APP}/Src/energy_policy.c ${APP}/Src/energy_budget.c ${APP}/Src/boot_journal.c
  ${DEVICE}/Source/Templates/system_stm32f1xx.c
  ${DEVICE}/Source/Templates/gcc/startup_stm32f103xb.s)
foreach(module hal hal_cortex hal_rcc hal_rcc_ex hal_gpio hal_gpio_ex hal_dma hal_adc hal_adc_ex hal_i2c hal_uart hal_pwr hal_rtc hal_rtc_ex hal_flash hal_flash_ex hal_iwdg)
  target_sources(railnode PRIVATE ${HAL}/Src/stm32f1xx_${module}.c)
endforeach()
# D 头文件、宏与编译选项
target_include_directories(railnode PRIVATE ${APP}/Inc ${HAL}/Inc ${HAL}/Inc/Legacy ${DEVICE}/Include firmware/Drivers/CMSIS/Core/Include)
target_compile_definitions(railnode PRIVATE USE_HAL_DRIVER STM32F103xB
  APP_STAGE=${STAGE} APP_LOW_POWER=${LOW_POWER} APP_ENFORCE_ENERGY=${ENFORCE}
  APP_ADAPTIVE=${ADAPTIVE} APP_LOG_ENABLE=${LOGGING} APP_RADIO_ACK=${RADIO_ACK})
target_compile_options(railnode PRIVATE -mcpu=cortex-m3 -mthumb -Os -g3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
set_target_properties(railnode PROPERTIES SUFFIX .elf)
# E 链接脚本和内存布局
target_link_options(railnode PRIVATE -mcpu=cortex-m3 -mthumb --specs=nano.specs --specs=nosys.specs
  -T${CMAKE_CURRENT_SOURCE_DIR}/firmware/STM32F103C8Tx_FLASH.ld
  -Wl,--gc-sections,-Map=${CMAKE_CURRENT_BINARY_DIR}/railnode.map -Wl,--print-memory-usage)
target_link_libraries(railnode PRIVATE m)
# F 编译后生成HEX和BIN
add_custom_command(TARGET railnode POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:railnode> ${CMAKE_CURRENT_BINARY_DIR}/railnode.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:railnode> ${CMAKE_CURRENT_BINARY_DIR}/railnode.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:railnode>)
