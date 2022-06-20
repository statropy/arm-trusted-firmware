/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_SIP_SVC_H
#define LAN966X_SIP_SVC_H

/* SMC function IDs for SiP Service queries */
#define SIP_SVC_UID		0x8200ff01
#define SIP_SVC_VERSION		0x8200ff02
#define SIP_SVC_SJTAG_STATUS	0x8200ff03
#define SIP_SVC_SJTAG_CHALLENGE	0x8200ff04
#define SIP_SVC_SJTAG_UNLOCK	0x8200ff05
#define SIP_SVC_FW_BIND		0x8200ff06

/* SiP Service Calls version numbers */
#define SIP_SVC_VERSION_MAJOR	0
#define SIP_SVC_VERSION_MINOR	1

#endif
