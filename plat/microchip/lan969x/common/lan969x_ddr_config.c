// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries.
 *
 */

#include <ddr_config.h>

const struct ddr_config lan969x_evb_ddr4_ddr_config = {
	.info = {
		.name = "lan969x_evb_ddr4 2023-07-19-14:24:18 193b2e888363",
		.speed = 2400,
		.size = 0x38000000,
		.bus_width = 16,
	},
	.main = {
		.crcparctl1 = 0x00001091,
		.dbictl = 0x00000001,
		.dfimisc = 0x00000040,
		.dfitmg0 = 0x0391820f,
		.dfitmg1 = 0x00040201,
		.dfiupd0 = 0x40400003,
		.dfiupd1 = 0x004000ff,
		.ecccfg0 = 0x003f7f44,
		.init0 = 0x00020248,
		.init1 = 0x00e80000,
		.init3 = 0x0c340101,
		.init4 = 0x10180200,
		.init5 = 0x00110000,
		.init6 = 0x00000402,
		.init7 = 0x00000c19,
		.mstr = 0x81040010,
		.pccfg = 0x00000000,
		.pwrctl = 0x00000000,
		.rfshctl0 = 0x00210020,
		.rfshctl3 = 0x00000000,
	},

	.timing = {
		.dramtmg0 = 0x17131413,
		.dramtmg1 = 0x0007051b,
		.dramtmg12 = 0x1a000010,
		.dramtmg2 = 0x090b0512,
		.dramtmg3 = 0x0000400c,
		.dramtmg4 = 0x08040409,
		.dramtmg5 = 0x07070404,
		.dramtmg8 = 0x05040c07,
		.dramtmg9 = 0x0003040d,
		.odtcfg = 0x07000610,
		.rfshtmg = 0x004900d3,
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
		.pgcr2 = 0x000010ba,
		.schcr1 = 0x00000000,
		.zq0pr = 0x00079900,
		.zq1pr = 0x10077900,
		.zq2pr = 0x00000000,
		.zqcr = 0x00058f00,
	},

	.phy_timing = {
		.dtpr0 = 0x0827100a,
		.dtpr1 = 0x28250018,
		.dtpr2 = 0x000701b1,
		.dtpr3 = 0x03000101,
		.dtpr4 = 0x01a50808,
		.dtpr5 = 0x00361009,
		.ptr0 = 0x4ae25710,
		.ptr1 = 0x74f4950e,
		.ptr2 = 0x00083def,
		.ptr3 = 0x1b192000,
		.ptr4 = 0x1003a000,
	},

};
