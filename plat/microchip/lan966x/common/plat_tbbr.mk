#
# Copyright (c) 2021, Microchip Technology Inc. and its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause
#

LAN966X_HW_CRYPTO	:=	yes

LAN966X_CRYPTO_TEST	:=	no

# Include common TBB sources
AUTH_SOURCES	:=	drivers/auth/auth_mod.c				\
			drivers/auth/crypto_mod.c			\
			drivers/microchip/crypto/aes.c			\
			drivers/microchip/crypto/pkcl.c			\
			drivers/auth/img_parser_mod.c

ifeq (${LAN966X_HW_CRYPTO},yes)
AUTH_SOURCES	+= plat/microchip/lan966x/common/lan966x_crypto.c
else
CRYPTO_LIB_MK := drivers/auth/mbedtls/mbedtls_crypto.mk
$(info Including ${CRYPTO_LIB_MK})
include ${CRYPTO_LIB_MK}
endif

# Include the selected chain of trust sources.
ifeq (${LAN966X_CRYPTO_TEST},yes)
$(eval $(call add_define,LAN966X_AES_TESTS))
BL2_SOURCES	+= plat/microchip/lan966x/common/lan966x_crypto_tests.c
endif

# Include the selected chain of trust sources.
ifeq (${COT},tbbr)
    BL1_SOURCES	+=	drivers/auth/tbbr/tbbr_cot_common.c		\
			drivers/auth/tbbr/tbbr_cot_bl1.c

    BL2_SOURCES	+=	drivers/auth/tbbr/tbbr_cot_common.c		\
			drivers/auth/tbbr/tbbr_cot_bl2.c
else
    $(error Unknown chain of trust ${COT})
endif

BL1_SOURCES	+=	${AUTH_SOURCES}					\
			plat/microchip/lan966x/common/lan966x_tbbr.c	\
			bl1/tbbr/tbbr_img_desc.c

BL2_SOURCES	+=	${AUTH_SOURCES}					\
			plat/microchip/lan966x/common/lan966x_tbbr.c

BL2U_SOURCES	+=	${AUTH_SOURCES}					\
			plat/microchip/lan966x/common/lan966x_tbbr.c

IMG_PARSER_LIB_MK := drivers/auth/mbedtls/mbedtls_x509.mk

$(info Including ${IMG_PARSER_LIB_MK})
include ${IMG_PARSER_LIB_MK}

ifeq (${MEASURED_BOOT},1)
    MEASURED_BOOT_MK := drivers/measured_boot/measured_boot.mk
    $(info Including ${MEASURED_BOOT_MK})
    include ${MEASURED_BOOT_MK}
endif
