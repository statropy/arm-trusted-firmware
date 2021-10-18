PLAT_VARIANT			:=	lan966x_evb

# Non-volatile counter values
TFW_NVCTR_VAL			:=	2
NTFW_NVCTR_VAL			:=	3

include plat/microchip/lan966x/common/common.mk

$(eval $(call add_define,EVB_9662))
$(eval $(call add_define,LAN966X_ASIC))
$(eval $(call add_define,LAN966X_TZ))
