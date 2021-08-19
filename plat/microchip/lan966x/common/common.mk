#
# Copyright (c) 2021, Microchip Technology Inc. and its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause
#

ifeq (${ARCH},aarch64)
  $(error Error: AArch64 not supported on ${PLAT})
endif

ARM_CORTEX_A7                   := yes
ARM_ARCH_MAJOR			:= 7

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

include lib/xlat_tables_v2/xlat_tables.mk

ifneq (${TRUSTED_BOARD_BOOT},0)

    $(info Including platform TBBR)
    include plat/microchip/lan966x/common/plat_tbbr.mk

endif

PLAT_INCLUDES	:=	-Iplat/microchip/lan966x/${PLAT}/include	\
			-Iplat/microchip/lan966x/common/include		\
			-Iinclude/drivers/microchip

LAN966X_CONSOLE_SOURCES	+=	\
				drivers/microchip/gpio/vcore_gpio.c		\
				drivers/microchip/qspi/qspi.c			\
				drivers/microchip/flexcom_uart/flexcom_console.S \
				drivers/gpio/gpio.c

PLAT_BL_COMMON_SOURCES	+=	\
				lib/cpus/aarch32/cortex_a7.S			\
				${XLAT_TABLES_LIB_SRCS}				\
				${LAN966X_CONSOLE_SOURCES}			\
				common/desc_image_load.c			\
				drivers/delay_timer/delay_timer.c		\
				drivers/delay_timer/generic_delay_timer.c	\
				drivers/mmc/mmc.c				\
				drivers/microchip/emmc/emmc.c			\
				drivers/microchip/otp/otp.c			\
				drivers/microchip/clock/lan966x_clock.c		\
				drivers/microchip/crypto/lan966x_sha.c		\
				plat/microchip/lan966x/common/lan966x_trng.c	\
				drivers/microchip/tz_matrix/tz_matrix.c		\
				plat/common/${ARCH}/crash_console_helpers.S	\
				plat/microchip/lan966x/common/${ARCH}/plat_helpers.S	\
				plat/microchip/lan966x/common/lan966x_crc32.c		\
				plat/microchip/lan966x/common/lan966x_sjtag.c		\
				plat/microchip/lan966x/common/lan966x_common.c

BL1_SOURCES		+=	drivers/io/io_block.c					\
				drivers/io/io_fip.c					\
				drivers/io/io_memmap.c					\
				drivers/io/io_storage.c					\
				drivers/partition/gpt.c					\
				drivers/partition/partition.c				\
				plat/microchip/lan966x/common/lan966x_mmc.c		\
				plat/microchip/lan966x/common/lan966x_io_storage.c	\
				plat/microchip/lan966x/common/${ARCH}/plat_bootstrap.S	\
				plat/microchip/lan966x/common/lan966x_bootstrap.c	\
				plat/microchip/lan966x/common/lan966x_bl1_bootstrap.c	\
				plat/microchip/lan966x/common/lan966x_bl1_setup.c

BL2_SOURCES		+=	drivers/io/io_block.c				\
				drivers/io/io_fip.c				\
				drivers/io/io_memmap.c				\
				drivers/io/io_storage.c				\
				drivers/partition/gpt.c				\
				drivers/partition/partition.c			\
				plat/microchip/lan966x/common/lan966x_mmc.c		\
				plat/microchip/lan966x/common/lan966x_bl2_setup.c	\
				plat/microchip/lan966x/common/lan966x_bl2_mem_params_desc.c \
				plat/microchip/lan966x/common/lan966x_image_load.c	\
				plat/microchip/lan966x/common/lan966x_ddr.c		\
				plat/microchip/lan966x/common/lan966x_tz.c		\
				plat/microchip/lan966x/common/lan966x_io_storage.c

CONSOLE_BASE			:=	LAN966X_FLEXCOM_0_BASE

# Enable Activity Monitor Unit extensions by default
ENABLE_AMU			:=	1

# We have TRNG
TRNG_SUPPORT			:=	1

# Disable stack protector by default
ENABLE_STACK_PROTECTOR	 	:= strong

# Process flags
$(eval $(call add_define,CONSOLE_BASE))

ifneq (${BL2_AT_EL3}, 0)
    override BL1_SOURCES =
endif

ifneq (${ENABLE_STACK_PROTECTOR},0)
PLAT_BL_COMMON_SOURCES  +=      plat/microchip/lan966x/common/lan966x_stack_protector.c
endif

LAN966X_FW_CONFIG	:=	${BUILD_PLAT}/fw_config.bin

${LAN966X_FW_CONFIG}: bin/otp.bin bin/fw_param.bin
	$(Q)cat $^ > $@

# FW config
$(eval $(call TOOL_ADD_PAYLOAD,${LAN966X_FW_CONFIG},--fw-config,${LAN966X_FW_CONFIG}))
