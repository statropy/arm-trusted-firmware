#
# Copyright (c) 2021, Microchip Technology Inc. and its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include plat/microchip/lan969x/common/common.mk

# This is used in lan969x code
$(eval $(call add_define,LAN969X_ASIC))
# This is used in common drivers
$(eval $(call add_define,LAN966X_ASIC))
$(eval $(call add_define,LAN969X_PCIE))
