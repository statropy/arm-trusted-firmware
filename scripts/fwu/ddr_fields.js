/*
 * Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

const ddr_fields = new Map([
    ["ecc_mode", new Map([
	[0, "ECC disabled"],
	[4, "ECC SEC/DED"],
    ])],
    ["data_bus_width", new Map([
	[0, "Full DQ width"],
	[1, "Half DQ width"],
	[2, "Quarter DQ width"],
    ])],
]);
