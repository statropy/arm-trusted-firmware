#
# Copyright (c) 2021, Microchip Technology Inc. and its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Default number of CPUs per cluster on FVP
LAN966x_MAX_CPUS_PER_CLUSTER	:= 1

# Default number of threads per CPU on FVP
LAN966x_MAX_PE_PER_CPU	:= 1

# To compile with highest log level (VERBOSE) set value to 50
LOG_LEVEL := 40

# Pass LAN966x_MAX_CPUS_PER_CLUSTER to the build system.
$(eval $(call add_define,LAN966x_MAX_CPUS_PER_CLUSTER))

# Pass LAN966x_MAX_PE_PER_CPU to the build system.
$(eval $(call add_define,LAN966x_MAX_PE_PER_CPU))

# Include GICv2 driver files
# include drivers/arm/gic/v2/gicv2.mk

# include lib/libfdt/libfdt.mk
include lib/xlat_tables_v2/xlat_tables.mk

# GIC_SOURCES	:=	plat/common/plat_gicv2.c	\
#					plat/arm/common/arm_gicv2.c

PLAT_INCLUDES	:=	-Iplat/microchip/lan966x/include	\
			-Iinclude/drivers/microchip

LAN966X_CONSOLE_SOURCES	+=	\
				drivers/microchip/gpio/vcore_gpio.c		\
				drivers/microchip/flexcom_uart/flexcom_uart.c	\
				drivers/gpio/gpio.c

PLAT_BL_COMMON_SOURCES	+=	\
				lib/cpus/aarch32/cortex_a7.S			\
				${XLAT_TABLES_LIB_SRCS}				\
				${LAN966X_CONSOLE_SOURCES}			\
				plat/common/aarch32/crash_console_helpers.S	\
				plat/microchip/lan966x/${ARCH}/plat_helpers.S	\
				plat/microchip/lan966x/lan966x_common.c

BL1_SOURCES		+=	\
				drivers/delay_timer/delay_timer.c		\
				drivers/delay_timer/generic_delay_timer.c	\
				drivers/microchip/flexcom_uart/flexcom_console.S	\
				drivers/io/io_block.c				\
				drivers/io/io_fip.c				\
				drivers/io/io_memmap.c				\
				drivers/io/io_storage.c				\
				plat/microchip/lan966x/lan966x_io_storage.c	\
				plat/microchip/lan966x/lan966x_bl1_setup.c

#				lib/aarch32/arm32_aeabi_divmod.c		\
#				lib/aarch32/arm32_aeabi_divmod_a32.S		\
#

BL2_SOURCES		+=	common/desc_image_load.c			\
				drivers/delay_timer/delay_timer.c		\
				drivers/delay_timer/generic_delay_timer.c	\
				drivers/io/io_fip.c				\
				drivers/io/io_memmap.c				\
				drivers/io/io_storage.c				\
				plat/microchip/lan966x/lan966x_bl2_setup.c	\
				plat/microchip/lan966x/lan966x_bl2_mem_params_desc.c \
				plat/microchip/lan966x/lan966x_image_load.c	\
				plat/microchip/lan966x/lan966x_io_storage.c

# Enable Activity Monitor Unit extensions by default
ENABLE_AMU			:=	1

# Enable reclaiming of BL31 initialisation code for secondary cores
# stacks for FVP. However, don't enable reclaiming for clang.
ifneq (${RESET_TO_BL31},1)
ifeq ($(findstring clang,$(notdir $(CC))),)
RECLAIM_INIT_CODE	:=	1
endif
endif

# Disable stack protector by default
ENABLE_STACK_PROTECTOR	 	:= 0

ifeq (${ARCH},aarch32)
    NEED_BL32 := no
endif

ifneq (${BL2_AT_EL3}, 0)
    override BL1_SOURCES =
endif
