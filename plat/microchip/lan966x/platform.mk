#
# Copyright (c) 2021, Microchip Technology Inc. and its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Default cluster count for FVP
LAN966x_CLUSTER_COUNT	:= 1

# Default number of CPUs per cluster on FVP
LAN966x_MAX_CPUS_PER_CLUSTER	:= 1

# Default number of threads per CPU on FVP
LAN966x_MAX_PE_PER_CPU	:= 1

# Disable redistributor frame of inactive/fused CPU cores by marking it as read
# only; enable redistributor frames of all CPU cores by default.
LAN966x_GICR_REGION_PROTECTION		:= 0

# Pass LAN966x_CLUSTER_COUNT to the build system.
$(eval $(call add_define,LAN966x_CLUSTER_COUNT))

# Pass LAN966x_MAX_CPUS_PER_CLUSTER to the build system.
$(eval $(call add_define,LAN966x_MAX_CPUS_PER_CLUSTER))

# Pass LAN966x_MAX_PE_PER_CPU to the build system.
$(eval $(call add_define,LAN966x_MAX_PE_PER_CPU))

# Pass LAN966x_GICR_REGION_PROTECTION to the build system.
$(eval $(call add_define,LAN966x_GICR_REGION_PROTECTION))

# Include GICv2 driver files
# include drivers/arm/gic/v2/gicv2.mk

# include lib/libfdt/libfdt.mk
include lib/xlat_tables_v2/xlat_tables.mk

# GIC_SOURCES	:=	plat/common/plat_gicv2.c	\
#					plat/arm/common/arm_gicv2.c

PLAT_INCLUDES	:=	-Iplat/microchip/lan966x/include	\
					-Iinclude/drivers/microchip

# PLAT_BL_COMMON_SOURCES	:=	plat/arm/board/fvp/fvp_common.c

BL1_SOURCES		+=	lib/cpus/aarch32/cortex_a7.S						\
					drivers/delay_timer/delay_timer.c					\
					drivers/delay_timer/generic_delay_timer.c			\
					drivers/microchip/usart/usart.c						\
					plat/microchip/lan966x/${ARCH}/plat_helpers.S		\
					plat/microchip/lan966x/lan966x_bl1_setup.c

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

# Include common arm components
include plat/arm/board/common/board_common.mk
include plat/arm/common/arm_common.mk

