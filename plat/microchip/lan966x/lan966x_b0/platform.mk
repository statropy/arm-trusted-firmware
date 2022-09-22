PLAT_VARIANT			:=	lan966x_a0

# Non-volatile counter values
TFW_NVCTR_VAL			?=	2
NTFW_NVCTR_VAL			?=	3

# Build variant that has OTP emulation data in the FW_CONFIG and a noop BL2
LAN966X_OTP_DATA		:=	scripts/otp_data.yaml

include plat/microchip/lan966x/common/common.mk

$(eval $(call add_define,EVB_9662))
$(eval $(call add_define,LAN966X_ASIC))
$(eval $(call add_define,LAN966X_TZ))
