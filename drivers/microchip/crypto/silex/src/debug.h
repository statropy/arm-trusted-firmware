/** Debug printing functions
 *
 * @file
 */
/*
 * Copyright (c) 2018-2020 Silex Insight sa
 * Copyright (c) 2018-2020 Beerten Engineering scs
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef DEBUG_HEADER_FILE
#define DEBUG_HEADER_FILE

#define SX_NOPRINT(fmt, ...)  do {} while (0)

#ifdef DBG_STDERR

#include <stdio.h>
#define SX_PRINT(...)  fprintf(stderr, __VA_ARGS__)

#else
#define SX_PRINT  SX_NOPRINT

#endif

#ifndef DBG_LEVEL
#define DBG_LEVEL 0
#endif

#if DBG_LEVEL > 0
#define SX_FATALPRINT SX_PRINT
#else
#define SX_FATALPRINT SX_NOPRINT
#endif

#if DBG_LEVEL > 1
#define SX_ERRPRINT SX_PRINT
#else
#define SX_ERRPRINT SX_NOPRINT
#endif

#if DBG_LEVEL > 2
#define SX_WARNPRINT SX_PRINT
#else
#define SX_WARNPRINT SX_NOPRINT
#endif


#if DBG_LEVEL > 5
#define SX_DBGPRINT SX_PRINT
#else
#define SX_DBGPRINT SX_NOPRINT
#endif

#endif
