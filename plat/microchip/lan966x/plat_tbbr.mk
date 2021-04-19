#
# Copyright (c) 2021, Microchip Technology Inc. and its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Include common TBB sources
AUTH_SOURCES	:=	drivers/auth/auth_mod.c				\
			drivers/auth/crypto_mod.c			\
			drivers/auth/img_parser_mod.c

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
			plat/microchip/lan966x/lan966x_tbbr.c		\
			bl1/tbbr/tbbr_img_desc.c			\
			plat/common/tbbr/plat_tbbr.c

BL2_SOURCES	+=	${AUTH_SOURCES}					\
			plat/microchip/lan966x/lan966x_tbbr.c		\
			plat/common/tbbr/plat_tbbr.c

CRYPTO_LIB_MK := drivers/auth/mbedtls/mbedtls_crypto.mk
IMG_PARSER_LIB_MK := drivers/auth/mbedtls/mbedtls_x509.mk

$(info Including ${CRYPTO_LIB_MK})
include ${CRYPTO_LIB_MK}

$(info Including ${IMG_PARSER_LIB_MK})
include ${IMG_PARSER_LIB_MK}
