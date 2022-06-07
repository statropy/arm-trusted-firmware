BL2_AT_EL3			:=	1
LAN966X_OTP_DATA		:=	scripts/otp_data.yaml

include plat/microchip/lan966x/common/common.mk

$(eval $(call add_define,EVB_9662))
$(eval $(call add_define,LAN966X_ASIC))
$(eval $(call add_define,LAN966X_ASIC_A0))
$(eval $(call add_define,LAN966X_SAMA))

# Enable/Disable the eMMC built-in-system-test
LAN966X_EMMC_TEST	:=	0

# Include the eMMC sources for the built-in-system-test
ifeq (${LAN966X_EMMC_TEST},1)
$(eval $(call add_define,LAN966X_EMMC_TESTS))
BL2_SOURCES	+= plat/microchip/lan966x/common/lan966x_emmc_tests.c
endif
