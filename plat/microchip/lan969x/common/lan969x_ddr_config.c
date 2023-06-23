// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries.
 *
 */

#include <ddr_config.h>

const struct ddr_config lan969x_ddr_config = {
	.info = {
		.name = "lan969x 2023-06-23-12:52:17 8093bf2eaa60-dirty",
		.speed = 1600,
		.size = 0x38000000,
		.bus_width = 16,
	},
	.main = {
		.crcparctl1 = 0x00001091,
		.dbictl = 0x00000001,
		.dfimisc = 0x00000040,
		.dfitmg0 = 0x038c820d,
		.dfitmg1 = 0x00040201,
		.dfiupd0 = 0x40400003,
		.dfiupd1 = 0x004000ff,
		.ecccfg0 = 0x003f7f44,
		.init0 = 0x00020186,
		.init1 = 0x009c0000,
		.init3 = 0x06140501,
		.init4 = 0x10100000,
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
		.dramtmg0 = 0x120e0d0e,
		.dramtmg1 = 0x00050314,
		.dramtmg12 = 0x1a000010,
		.dramtmg2 = 0x0808040f,
		.dramtmg3 = 0x0000400c,
		.dramtmg4 = 0x06030306,
		.dramtmg5 = 0x04040303,
		.dramtmg8 = 0x04030a05,
		.dramtmg9 = 0x0003030d,
		.odtcfg = 0x07000604,
		.rfshtmg = 0x0030008c,
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
		.pgcr2 = 0x00000aa0,
		.schcr1 = 0x00000000,
		.zq0pr = 0x0007bb00,
		.zq1pr = 0x0007bb00,
		.zq2pr = 0x00000000,
		.zqcr = 0x00058e00,
	},

	.phy_timing = {
		.dtpr0 = 0x061c0a06,
		.dtpr1 = 0x281c0018,
		.dtpr2 = 0x00040120,
		.dtpr3 = 0x02550101,
		.dtpr4 = 0x01180805,
		.dtpr5 = 0x00280c06,
		.ptr0 = 0x32019010,
		.ptr1 = 0x4e200e10,
		.ptr2 = 0x00083def,
		.ptr3 = 0x12061800,
		.ptr4 = 0x10027000,
	},

};
