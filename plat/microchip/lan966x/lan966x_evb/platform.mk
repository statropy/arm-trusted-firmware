CONSOLE_BASE			:=	LAN966X_FLEXCOM_3_BASE
BL2_AT_EL3			:=	1

include plat/microchip/lan966x/common/common.mk

$(eval $(call add_define,EVB_9662))
$(eval $(call add_define,LAN966X_ASIC))
$(eval $(call add_define,LAN966X_SAMA))
