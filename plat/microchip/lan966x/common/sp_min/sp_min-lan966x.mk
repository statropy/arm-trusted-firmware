#
# Copyright (c) 2016-2018, ARM Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include drivers/arm/gic/v2/gicv2.mk

BL32_SOURCES		+=	plat/microchip/lan966x/common/sp_min/sp_min_plat_setup.c \
				plat/microchip/lan966x/common/lan966x_pm.c	\
				plat/microchip/lan966x/common/lan966x_topology.c

BL32_SOURCES            +=      plat/common/${ARCH}/platform_mp_stack.S		\
                                plat/common/plat_psci_common.c

BL32_SOURCES            +=      ${GICV2_SOURCES}				\
				plat/common/plat_gicv2.c			\
				plat/microchip/lan966x/common/lan966x_gicv2.c

BL32_SOURCES            +=      drivers/microchip/crypto/aes.c			\
				drivers/microchip/otp/otp_tags.c		\
				plat/microchip/lan966x/common/lan966x_fw_bind.c \
				plat/microchip/lan966x/common/lan966x_sip_svc.c	\
				plat/microchip/lan966x/common/lan966x_sjtag.c	\
				plat/microchip/lan966x/common/lan966x_tbbr.c

include lib/libfit/libfit.mk
include lib/libfdt/libfdt.mk

${BUILD_PLAT}/bl32/bl32.elf: ${LIB_DIR}/libfit.a
