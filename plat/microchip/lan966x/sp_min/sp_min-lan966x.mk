#
# Copyright (c) 2016-2018, ARM Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include drivers/arm/gic/v2/gicv2.mk

BL32_SOURCES		+=	plat/microchip/lan966x/sp_min/sp_min_plat_setup.c \
				plat/microchip/lan966x/lan966x_pm.c		\
				plat/microchip/lan966x/lan966x_trng.c		\
				plat/microchip/lan966x/lan966x_topology.c

BL32_SOURCES            +=      plat/common/${ARCH}/platform_mp_stack.S		\
                                plat/common/plat_psci_common.c

BL32_SOURCES            +=      ${GICV2_SOURCES}				\
				plat/common/plat_gicv2.c			\
				plat/microchip/lan966x/lan966x_gicv2.c
