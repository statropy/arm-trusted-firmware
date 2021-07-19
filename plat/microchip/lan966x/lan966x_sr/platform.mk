CONSOLE_BASE			:=	LAN966X_FLEXCOM_0_BASE

include plat/microchip/lan966x/common/common.mk

$(eval $(call add_define,LAN966X_TZ))
