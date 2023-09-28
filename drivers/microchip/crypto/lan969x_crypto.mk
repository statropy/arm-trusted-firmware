#
# Copyright (c) 2022, Microchip Technology Inc. and its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause
#

SILEX_DIR	:= drivers/microchip/crypto/silex

INCLUDES	+= -I${SILEX_DIR}/include

include drivers/auth/mbedtls/mbedtls_x509.mk

AUTH_SOURCES            :=      drivers/auth/auth_mod.c                 \
				drivers/auth/crypto_mod.c		\
				drivers/microchip/crypto/aes.c		\
				drivers/auth/img_parser_mod.c		\
				drivers/auth/tbbr/tbbr_cot_common.c

AUTH_SOURCES	+= drivers/microchip/crypto/lan969x_crypto.c 		\
			drivers/microchip/crypto/silex_crypto.c

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
			bl1/tbbr/tbbr_img_desc.c

BL2_SOURCES	+=	${AUTH_SOURCES}

BL2U_SOURCES	+=	${AUTH_SOURCES}

LIBSILEX_SRCS 	:=	$(addprefix ${SILEX_DIR}/src/,	\
				iomem.c			\
				ed25519.c		\
				ed448.c			\
				montgomery.c)

LIBSILEX_SRCS 	+=	$(addprefix ${SILEX_DIR}/target/baremetal_ba414e/,	\
				baremetal_ba414e.c	\
				baremetal_wait.c)

LIBSILEX_SRCS 	+=	$(addprefix ${SILEX_DIR}/target/hw/ba414/,	\
				pkhardware_ba414e.c	\
				pkhardware_ba414e_run.c	\
				ba414_status.c		\
				ec_curves.c		\
				cmddefs_ecc.c		\
				cmddefs_modmath.c	\
				cmddefs_sm9.c		\
				cmddefs_ecjpake.c	\
				cmddefs_srp.c)

LIBSILEX_SRCS   +=      $(addprefix ${SILEX_DIR}/interface/sxbuf/, sxbufops.c)

SILEX_INC	:=	-I${SILEX_DIR}/target/baremetal_ba414e/

$(eval $(call MAKE_LIB,silex, ${SILEX_INC}))
