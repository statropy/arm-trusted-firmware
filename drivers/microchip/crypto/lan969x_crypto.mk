include drivers/auth/mbedtls/mbedtls_crypto.mk
include drivers/auth/mbedtls/mbedtls_x509.mk

AUTH_SOURCES	:=	drivers/auth/auth_mod.c				\
			drivers/auth/crypto_mod.c			\
			drivers/microchip/crypto/aes.c			\
			drivers/microchip/crypto/lan966x_sha.c		\
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

$(info AUTH_SOURCES = ${AUTH_SOURCES})

BL1_SOURCES	+=	${AUTH_SOURCES}					\
			bl1/tbbr/tbbr_img_desc.c

BL2_SOURCES	+=	${AUTH_SOURCES}

BL2U_SOURCES	+=	${AUTH_SOURCES}
BL2U_SOURCES	+=	${MBEDTLS_SOURCES}
