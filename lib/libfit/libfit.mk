#
# Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
#
# SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
#

LIBFIT_SRCS	:=	$(addprefix lib/libfit/,	\
			fit.c)				\

INCLUDES	+=	-Iinclude/lib/libfit

$(eval $(call MAKE_LIB,fit))
