BL2_AT_EL3			:=	1
LAN966X_OTP_DATA		:=	scripts/otp_data.yaml

include plat/microchip/lan966x/common/common.mk

$(eval $(call add_define,EVB_9662))
$(eval $(call add_define,LAN966X_ASIC))
$(eval $(call add_define,LAN966X_SAMA))
