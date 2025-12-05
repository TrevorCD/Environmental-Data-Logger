#
#   Copyright 2025 Trevor Calderwood
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
#-------------------------------------------------------------------------------
#
#  Makefile: Compiles FreeRTOS and flashes the binary to an STM32F446RE.
#
#  File paths in this Makefile assume the same structure as the FreeRTOS github
#  repository. FreeRTOS/FreeRTOS/Source contains the FreeRTOS-Kernel repository.
#  The STM32CubeF4 github repository is also required.
#
#  FreeRTOS:           https://github.com/FreeRTOS/FreeRTOS
#  FreeRTOS-Kernel:    https://github.com/FreeRTOS/FreeRTOS-Kernel
#  STM32CubeF4:        https://github.com/STMicroelectronics/STM32CubeF4
#
#
#  Software Dependencies:
#    - arm-none-eabi-gcc
#    - st-link


#-------------------------------[ Build Config ]--------------------------------

CC := arm-none-eabi-gcc
AS := arm-none-eabi-as
CP := arm-none-eabi-objcopy

# General compilation flags
CFLAGS += -O2 -g -std=c99

# General Include directories
CFLAGS += -I. -Iinclude

SRCS += src/main.c src/stubs.c src/itm.c src/syscalls.c

# Linker flags
LDFLAGS += -T $(FLASH_SCRIPT) -nostartfiles -Wl,--gc-sections -specs=nano.specs

# ASM files not included in OBJS. Prevents 'make clean' from deleting .s files.
OBJS = $(SRCS:.c=.o)


#==================================#
#     STM32F446RE Compilation      #
#==================================#

# Preprocessor definition for exact board
BOARD := -DSTM32F446xx

# Vendor supplied script for flashing to MCU
FLASH_SCRIPT := STM32F446RETX_FLASH.ld

# Microcontroller flags
CFLAGS += -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += $(BOARD)

# Path to STM32CubeF4 Drivers. Should have BSP/ and CMSIS/ subdirs
STM_DRIVERS := ./STM32CubeF4/Drivers

# STM32 board specific and HAL driver source files
SRCS += src/system_stm32f4xx.c \
        $(STM_DRIVERS)/BSP/STM32F4xx-Nucleo/stm32f4xx_nucleo.c \
        $(STM_DRIVERS)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
        $(STM_DRIVERS)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
        $(STM_DRIVERS)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
        $(STM_DRIVERS)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
        $(STM_DRIVERS)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c \
        $(STM_DRIVERS)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c \
        $(STM_DRIVERS)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c \
        $(STM_DRIVERS)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c \
        $(STM_DRIVERS)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c

ASM_SRCS += src/startup_stm32f446xx.s

# STM32 header include directories
CFLAGS += -I$(STM_DRIVERS)/CMSIS/Include \
          -I$(STM_DRIVERS)/CMSIS/Device/ST/STM32F4xx/Include \
          -I$(STM_DRIVERS)/BSP/STM32F4xx-Nucleo \
          -I$(STM_DRIVERS)/STM32F4xx_HAL_Driver/Inc


#==================================#
#   FreeRTOS Kernel Compilation    #
#==================================#

# Path to FreeRTOS root dir. Should have Demo/ and Source/ subdirs
FREERTOS_ROOT := ./FreeRTOS/FreeRTOS

# Path to FreeRTOS Kernel source. Should have portable/ and include/ subdirs
KERNEL_DIR := $(FREERTOS_ROOT)/Source

# FreeRTOS kernel source files
SRCS += $(KERNEL_DIR)/tasks.c \
        $(KERNEL_DIR)/queue.c \
        $(KERNEL_DIR)/list.c \
        $(KERNEL_DIR)/timers.c \
        $(KERNEL_DIR)/event_groups.c \
        $(KERNEL_DIR)/portable/MemMang/heap_4.c \
        $(KERNEL_DIR)/portable/GCC/ARM_CM4F/port.c

# FreeRTOS kernel header include directories
CFLAGS += -I$(KERNEL_DIR)/include \
          -I$(KERNEL_DIR)/portable/GCC/ARM_CM4F


#==================================#
#       BME680 Device Driver       #
#==================================#

SRCS += src/drivers/bme680.c
CFLAGS += -I./include/drivers

#----------------------------------[ Targets ]----------------------------------

all: main.elf

main.elf: $(OBJS) $(ASM_SRCS)
	$(CC) $(CFLAGS) $(OBJS) $(ASM_SRCS) $(LDFLAGS) -o $@

flash: main.elf
	$(CP) -O binary main.elf main.bin
	st-flash write main.bin 0x8000000

clean:
	rm -f $(OBJS) main.elf main.bin

ocd:
	rm -f ocd.log.old
	touch ocd.log
	mv ocd.log ocd.log.old
	touch ocd.log
	openocd -f openocd.cfg

gdb:
	arm-none-eabi-gdb main.elf
