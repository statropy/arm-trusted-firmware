// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries.
 *
 */

#include <ddr_config.h>

const struct ddr_config lan969x_evb_ddr4_ddr_config = {
	.info = {
		.name = "lan969x_evb_ddr4 2023-07-11-15:48:31 168381bc2e53",
		.speed = 1866,
		.size = 0x38000000,
		.bus_width = 16,
	},
	.main = {
		.crcparctl1 = 0x00001091,
		.dbictl = 0x00000001,
		.dfimisc = 0x00000040,
		.dfitmg0 = 0x038d820c,
		.dfitmg1 = 0x00040201,
		.dfiupd0 = 0x40400003,
		.dfiupd1 = 0x004000ff,
		.ecccfg0 = 0x003f7f44,
		.init0 = 0x000201c8,
		.init1 = 0x00b60000,
		.init3 = 0x0a200501,
		.init4 = 0x10080200,
		.init5 = 0x00110000,
		.init6 = 0x00000401,
		.init7 = 0x00000419,
		.mstr = 0x81040010,
		.pccfg = 0x00000000,
		.pwrctl = 0x00000000,
		.rfshctl0 = 0x00210020,
		.rfshctl3 = 0x00000000,
	},

	.timing = {
		.dramtmg0 = 0x140f1010,
		.dramtmg1 = 0x00050417,
		.dramtmg12 = 0x1a000010,
		.dramtmg2 = 0x0709050f,
		.dramtmg3 = 0x0000400c,
		.dramtmg4 = 0x07030307,
		.dramtmg5 = 0x05050303,
		.dramtmg8 = 0x04030a06,
		.dramtmg9 = 0x0003030d,
		.odtcfg = 0x0700060c,
		.rfshtmg = 0x003800a4,
	},

	.mapping = {
		.addrmap0 = 0x0000001f,
		.addrmap1 = 0x003f0505,
		.addrmap2 = 0x01010100,
		.addrmap3 = 0x13131303,
		.addrmap4 = 0x00001f1f,
		.addrmap5 = 0x04040404,
		.addrmap6 = 0x04040404,
		.addrmap7 = 0x00000f0f,
		.addrmap8 = 0x00003f01,
	},

	.phy = {
		.dcr = 0x0000040c,
		.dsgcr = 0x0064401b,
		.dtcr0 = 0x8000b0cf,
		.dtcr1 = 0x00010a37,
		.dxccr = 0x00c01884,
		.pgcr2 = 0x00000caa,
		.schcr1 = 0x00000000,
		.zq0pr = 0x0007bb00,
		.zq1pr = 0x0007bb00,
		.zq2pr = 0x00000000,
		.zqcr = 0x00058e00,
	},

	.phy_timing = {
		.dtpr0 = 0x06200c08,
		.dtpr1 = 0x281d0018,
		.dtpr2 = 0x00050151,
		.dtpr3 = 0x02550101,
		.dtpr4 = 0x01470806,
		.dtpr5 = 0x002e0d07,
		.ptr0 = 0x3a81d390,
		.ptr1 = 0x5b44906e,
		.ptr2 = 0x00083def,
		.ptr3 = 0x15172000,
		.ptr4 = 0x1002d800,
	},

};
