#
# Copyright (c) 2021, Microchip Technology Inc. and its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Use the GICv3 driver on the FVP by default
FVP_USE_GIC_DRIVER	:= FVP_GICV3

# Default cluster count for FVP
FVP_CLUSTER_COUNT	:= 1

# Default number of CPUs per cluster on FVP
FVP_MAX_CPUS_PER_CLUSTER	:= 1

# Default number of threads per CPU on FVP
FVP_MAX_PE_PER_CPU	:= 1

# Disable redistributor frame of inactive/fused CPU cores by marking it as read
# only; enable redistributor frames of all CPU cores by default.
FVP_GICR_REGION_PROTECTION		:= 0

# The FVP platform depends on this macro to build with correct GIC driver.
$(eval $(call add_define,FVP_USE_GIC_DRIVER))

# Pass FVP_CLUSTER_COUNT to the build system.
$(eval $(call add_define,FVP_CLUSTER_COUNT))

# Pass FVP_MAX_CPUS_PER_CLUSTER to the build system.
$(eval $(call add_define,FVP_MAX_CPUS_PER_CLUSTER))

# Pass FVP_MAX_PE_PER_CPU to the build system.
$(eval $(call add_define,FVP_MAX_PE_PER_CPU))

# Pass FVP_GICR_REGION_PROTECTION to the build system.
$(eval $(call add_define,FVP_GICR_REGION_PROTECTION))

# Sanity check the cluster count and if FVP_CLUSTER_COUNT <= 2,
# choose the CCI driver , else the CCN driver
ifeq ($(FVP_CLUSTER_COUNT), 0)
$(error "Incorrect cluster count specified for FVP port")
else ifeq ($(FVP_CLUSTER_COUNT),$(filter $(FVP_CLUSTER_COUNT),1 2))
FVP_INTERCONNECT_DRIVER := FVP_CCI
else
FVP_INTERCONNECT_DRIVER := FVP_CCN
endif

$(eval $(call add_define,FVP_INTERCONNECT_DRIVER))

# Choose the GIC sources depending upon the how the FVP will be invoked
ifeq (${FVP_USE_GIC_DRIVER}, FVP_GICV3)

# The GIC model (GIC-600 or GIC-500) will be detected at runtime
GICV3_SUPPORT_GIC600		:=	1
GICV3_OVERRIDE_DISTIF_PWR_OPS	:=	1

# Include GICv3 driver files
include drivers/arm/gic/v3/gicv3.mk

FVP_GIC_SOURCES		:=	${GICV3_SOURCES}			\
				plat/common/plat_gicv3.c		\
				plat/arm/common/arm_gicv3.c

	ifeq ($(filter 1,${BL2_AT_EL3} ${RESET_TO_BL31} ${RESET_TO_SP_MIN}),)
		FVP_GIC_SOURCES += plat/arm/board/fvp/fvp_gicv3.c
	endif

else ifeq (${FVP_USE_GIC_DRIVER}, FVP_GICV2)

# No GICv4 extension
GIC_ENABLE_V4_EXTN	:=	0
$(eval $(call add_define,GIC_ENABLE_V4_EXTN))

# Include GICv2 driver files
include drivers/arm/gic/v2/gicv2.mk

FVP_GIC_SOURCES		:=	${GICV2_SOURCES}			\
				plat/common/plat_gicv2.c		\
				plat/arm/common/arm_gicv2.c

FVP_DT_PREFIX		:=	fvp-base-gicv2-psci
else
$(error "Incorrect GIC driver chosen on FVP port")
endif

ifeq (${FVP_INTERCONNECT_DRIVER}, FVP_CCI)
FVP_INTERCONNECT_SOURCES	:= 	drivers/arm/cci/cci.c
else ifeq (${FVP_INTERCONNECT_DRIVER}, FVP_CCN)
FVP_INTERCONNECT_SOURCES	:= 	drivers/arm/ccn/ccn.c		\
					plat/arm/common/arm_ccn.c
else
$(error "Incorrect CCN driver chosen on FVP port")
endif

FVP_SECURITY_SOURCES	:=	drivers/arm/tzc/tzc400.c		\
				plat/arm/board/fvp/fvp_security.c	\
				plat/arm/common/arm_tzc400.c


PLAT_INCLUDES		:=	-Iplat/arm/board/fvp/include


PLAT_BL_COMMON_SOURCES	:=	plat/arm/board/fvp/fvp_common.c

FVP_CPU_LIBS		:=	lib/cpus/${ARCH}/aem_generic.S

FVP_CPU_LIBS		+=	lib/cpus/aarch32/cortex_a32.S

BL1_SOURCES		+=	drivers/arm/smmu/smmu_v3.c			\
				drivers/arm/sp805/sp805.c			\
				drivers/delay_timer/delay_timer.c		\
				drivers/io/io_semihosting.c			\
				lib/semihosting/semihosting.c			\
				lib/semihosting/${ARCH}/semihosting_call.S	\
				plat/arm/board/fvp/${ARCH}/fvp_helpers.S	\
				plat/arm/board/fvp/fvp_bl1_setup.c		\
				plat/arm/board/fvp/fvp_err.c			\
				plat/arm/board/fvp/fvp_io_storage.c		\
				${FVP_CPU_LIBS}					\
				${FVP_INTERCONNECT_SOURCES}

ifeq (${USE_SP804_TIMER},1)
BL1_SOURCES		+=	drivers/arm/sp804/sp804_delay_timer.c
else
BL1_SOURCES		+=	drivers/delay_timer/generic_delay_timer.c
endif

# Enable Activity Monitor Unit extensions by default
ENABLE_AMU			:=	1

# Enable dynamic mitigation support by default
DYNAMIC_WORKAROUND_CVE_2018_3639	:=	1

# Enable reclaiming of BL31 initialisation code for secondary cores
# stacks for FVP. However, don't enable reclaiming for clang.
ifneq (${RESET_TO_BL31},1)
ifeq ($(findstring clang,$(notdir $(CC))),)
RECLAIM_INIT_CODE	:=	1
endif
endif

ifneq (${ENABLE_STACK_PROTECTOR},0)
PLAT_BL_COMMON_SOURCES	+=	plat/arm/board/fvp/fvp_stack_protector.c
endif

ifneq (${BL2_AT_EL3}, 0)
    override BL1_SOURCES =
endif

include plat/arm/board/common/board_common.mk
include plat/arm/common/arm_common.mk

ifeq (${TRUSTED_BOARD_BOOT}, 1)
BL1_SOURCES		+=	plat/arm/board/fvp/fvp_trusted_boot.c

# FVP being a development platform, enable capability to disable Authentication
# dynamically if TRUSTED_BOARD_BOOT is set.
DYN_DISABLE_AUTH	:=	1
endif
