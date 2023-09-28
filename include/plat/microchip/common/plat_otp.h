
/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLAT_OTP_H
#define PLAT_OTP_H

#include <assert.h>
#include <stdint.h>
#include <drivers/microchip/otp.h>

/* OTP_PRG */
#define OTP_PRG_ADDR                             0
#define OTP_PRG_SIZE                             4

/* Fields in OTP_PRG */
#define OTP_OTP_PRG_FEAT_DIS_OFF                 3
#define OTP_OTP_PRG_PARTID_OFF                   2
#define OTP_OTP_PRG_JTAG_UUID_OFF                1
#define OTP_OTP_PRG_SECURE_JTAG_OFF              0

/* FEAT_DIS */
#define FEAT_DIS_ADDR                            4
#define FEAT_DIS_SIZE                            1

/* Fields in FEAT_DIS */
#define OTP_RTE_DISABLE_OFF                      5
#define OTP_SERDES2_DISABLE_OFF                  4
#define OTP_CAN_DISABLE_OFF                      3
#define OTP_CPU_SEC_DBG_DISABLE_OFF              2
#define OTP_CPU_DBG_DISABLE_OFF                  1
#define OTP_CPU_DISABLE_OFF                      0

/* PARTID */
#define PARTID_ADDR                              5
#define PARTID_SIZE                              2

/* TST_TRK */
#define TST_TRK_ADDR                             7
#define TST_TRK_SIZE                             1

/* SERIAL_NUMBER */
#define SERIAL_NUMBER_ADDR                       8
#define SERIAL_NUMBER_SIZE                       8

/* SECURE_JTAG */
#define SECURE_JTAG_ADDR                         16
#define SECURE_JTAG_SIZE                         4

/* Fields in SECURE_JTAG */
#define OTP_JTAG_DISABLE_OFF                     27
#define OTP_JTAG_MODE_OFF                        0
#define OTP_JTAG_MODE_BITS                       2

/* WAFER_TRK */
#define WAFER_TRK_ADDR                           20
#define WAFER_TRK_SIZE                           7

/* JTAG_UUID */
#define JTAG_UUID_ADDR                           32
#define JTAG_UUID_SIZE                           10

/* TRIM */
#define TRIM_ADDR                                48
#define TRIM_SIZE                                8

/* Fields in TRIM */
#define OTP_UVOV_GPIOB_TRIM_OFF                  37
#define OTP_UVOV_GPIOB_TRIM_BITS                 6
#define OTP_UVOV_BOOT_TRIM_OFF                   31
#define OTP_UVOV_BOOT_TRIM_BITS                  6
#define OTP_UVOV_RGMII_TRIM_OFF                  25
#define OTP_UVOV_RGMII_TRIM_BITS                 6
#define OTP_UVOV_GPIOA_TRIM_OFF                  19
#define OTP_UVOV_GPIOA_TRIM_BITS                 6
#define OTP_COM_BIAS_BG_TC_TRIM_OFF              14
#define OTP_COM_BIAS_BG_TC_TRIM_BITS             5
#define OTP_COM_BIAS_BG_MAG_TRIM_OFF             8
#define OTP_COM_BIAS_BG_MAG_TRIM_BITS            6
#define OTP_COM_RBIAS_TC_TRIM_OFF                6
#define OTP_COM_RBIAS_TC_TRIM_BITS               2
#define OTP_COM_RBIAS_MAG_TRIM_OFF               0
#define OTP_COM_RBIAS_MAG_TRIM_BITS              6

/* PROTECT_OTP_WRITE */
#define PROTECT_OTP_WRITE_ADDR                   64
#define PROTECT_OTP_WRITE_SIZE                   4

/* Fields in PROTECT_OTP_WRITE */
#define OTP_OTP_WRITE_PROTECT_OFF                0
#define OTP_OTP_WRITE_PROTECT_BITS               8

/* PROTECT_REGION_ADDR */
#define PROTECT_REGION_ADDR_ADDR                 68
#define PROTECT_REGION_ADDR_SIZE                 32

/* Fields in PROTECT_REGION_ADDR */
#define OTP_PROTECT_REGION0_START_ADDR_OFF       0
#define OTP_PROTECT_REGION0_START_ADDR_BITS      16
#define OTP_PROTECT_REGION0_END_ADDR_OFF         16
#define OTP_PROTECT_REGION0_END_ADDR_BITS        16

/* OTP_PCIE_FLAGS */
#define OTP_PCIE_FLAGS_ADDR                      100
#define OTP_PCIE_FLAGS_SIZE                      4

/* Fields in OTP_PCIE_FLAGS */
#define OTP_OTP_PCIE_FLAGS_MAX_LINK_SPEED_OFF    0
#define OTP_OTP_PCIE_FLAGS_MAX_LINK_SPEED_BITS   3

/* OTP_PCIE_DEV */
#define OTP_PCIE_DEV_ADDR                        104
#define OTP_PCIE_DEV_SIZE                        4

/* Fields in OTP_PCIE_DEV */
#define OTP_OTP_PCIE_DEV_BASE_CLASS_CODE_OFF     24
#define OTP_OTP_PCIE_DEV_BASE_CLASS_CODE_BITS    8
#define OTP_OTP_PCIE_DEV_SUB_CLASS_CODE_OFF      16
#define OTP_OTP_PCIE_DEV_SUB_CLASS_CODE_BITS     8
#define OTP_OTP_PCIE_DEV_PROG_IF_OFF             8
#define OTP_OTP_PCIE_DEV_PROG_IF_BITS            8
#define OTP_OTP_PCIE_DEV_REVISION_ID_OFF         0
#define OTP_OTP_PCIE_DEV_REVISION_ID_BITS        8

/* OTP_PCIE_ID */
#define OTP_PCIE_ID_ADDR                         108
#define OTP_PCIE_ID_SIZE                         8

/* Fields in OTP_PCIE_ID */
#define OTP_OTP_PCIE_DEVICE_ID_OFF               0
#define OTP_OTP_PCIE_DEVICE_ID_BITS              16
#define OTP_OTP_PCIE_VENDOR_ID_OFF               16
#define OTP_OTP_PCIE_VENDOR_ID_BITS              16
#define OTP_OTP_PCIE_SUBSYS_DEVICE_ID_OFF        32
#define OTP_OTP_PCIE_SUBSYS_DEVICE_ID_BITS       16
#define OTP_OTP_PCIE_SUBSYS_VENDOR_ID_OFF        48
#define OTP_OTP_PCIE_SUBSYS_VENDOR_ID_BITS       16

/* OTP_PCIE_BARS */
#define OTP_PCIE_BARS_ADDR                       116
#define OTP_PCIE_BARS_SIZE                       40

/* OTP_TBBR_ROTPK */
#define OTP_TBBR_ROTPK_ADDR                      256
#define OTP_TBBR_ROTPK_SIZE                      32

/* OTP_TBBR_HUK */
#define OTP_TBBR_HUK_ADDR                        288
#define OTP_TBBR_HUK_SIZE                        32

/* OTP_TBBR_EK */
#define OTP_TBBR_EK_ADDR                         320
#define OTP_TBBR_EK_SIZE                         32

/* OTP_TBBR_SSK */
#define OTP_TBBR_SSK_ADDR                        352
#define OTP_TBBR_SSK_SIZE                        32

/* OTP_SJTAG_SSK */
#define OTP_SJTAG_SSK_ADDR                       384
#define OTP_SJTAG_SSK_SIZE                       32

/* OTP_STRAP_DISABLE_MASK */
#define OTP_STRAP_DISABLE_MASK_ADDR              420
#define OTP_STRAP_DISABLE_MASK_SIZE              2

/* OTP_TBBR_NTNVCT */
#define OTP_TBBR_NTNVCT_ADDR                     512
#define OTP_TBBR_NTNVCT_SIZE                     32

/* OTP_TBBR_TNVCT */
#define OTP_TBBR_TNVCT_ADDR                      544
#define OTP_TBBR_TNVCT_SIZE                      32


/* OTP access functions */
#define otp_accessor_group_read(aname, grp_name)			\
static inline int aname(uint8_t *dst, size_t nbytes)			\
{									\
	assert(nbytes >= grp_name##_SIZE);				\
	return otp_read_bytes(grp_name##_ADDR, grp_name##_SIZE, dst);   \
}

#define otp_accessor_read_bool(aname, grp_name, fld_name)		\
static inline bool aname(void)						\
{									\
	uint8_t b;							\
	int addr = grp_name##_ADDR + (OTP_##fld_name##_OFF / 8);	\
	int off = OTP_##fld_name##_OFF % 8;				\
	(void) otp_read_bytes(addr, 1, &b);				\
	return !!(b & (1 << off));					\
}

#define otp_accessor_read_bytes(aname, grp_name, fld_name)		\
static inline int aname(uint8_t *dst, size_t nbytes)			\
{									\
	assert(nbytes >= (OTP_##fld_name##_BITS / 8));			\
	assert((OTP_##fld_name##_OFF % 8) == 0);			\
	return otp_read_bytes(grp_name##_ADDR + 			\
		OTP_##fld_name##_OFF / 8, OTP_##fld_name##_BITS / 8, dst); \
}

#define otp_accessor_read_field(aname, grp_name, fld_name)		\
static inline uint32_t aname(void)					\
{									\
	uint32_t w;							\
	int addr = grp_name##_ADDR + (OTP_##fld_name##_OFF / 8);	\
	int off = OTP_##fld_name##_OFF % 8;				\
	(void) otp_read_uint32(addr, &w);				\
	return (w >> off) & GENMASK(OTP_##fld_name##_BITS - 1, 0);	\
}

otp_accessor_group_read(otp_read_otp_prg, OTP_PRG);
otp_accessor_group_read(otp_read_feat_dis, FEAT_DIS);
otp_accessor_group_read(otp_read_partid, PARTID);
otp_accessor_group_read(otp_read_serial_number, SERIAL_NUMBER);
otp_accessor_read_bool(otp_read_jtag_disable, SECURE_JTAG, JTAG_DISABLE);
otp_accessor_read_field(otp_read_jtag_mode, SECURE_JTAG, JTAG_MODE);
otp_accessor_group_read(otp_read_jtag_uuid, JTAG_UUID);
otp_accessor_group_read(otp_read_trim, TRIM);
otp_accessor_read_field(otp_read_uvov_gpiob_trim, TRIM, UVOV_GPIOB_TRIM);
otp_accessor_read_field(otp_read_uvov_boot_trim, TRIM, UVOV_BOOT_TRIM);
otp_accessor_read_field(otp_read_uvov_rgmii_trim, TRIM, UVOV_RGMII_TRIM);
otp_accessor_read_field(otp_read_uvov_gpioa_trim, TRIM, UVOV_GPIOA_TRIM);
otp_accessor_read_field(otp_read_com_bias_bg_mag_trim, TRIM, COM_BIAS_BG_MAG_TRIM);
otp_accessor_read_field(otp_read_com_rbias_mag_trim, TRIM, COM_RBIAS_MAG_TRIM);
otp_accessor_read_field(otp_read_otp_pcie_flags_max_link_speed, OTP_PCIE_FLAGS, OTP_PCIE_FLAGS_MAX_LINK_SPEED);
otp_accessor_group_read(otp_read_otp_pcie_dev, OTP_PCIE_DEV);
otp_accessor_read_bytes(otp_read_otp_pcie_device_id, OTP_PCIE_ID, OTP_PCIE_DEVICE_ID);
otp_accessor_read_bytes(otp_read_otp_pcie_vendor_id, OTP_PCIE_ID, OTP_PCIE_VENDOR_ID);
otp_accessor_read_bytes(otp_read_otp_pcie_subsys_device_id, OTP_PCIE_ID, OTP_PCIE_SUBSYS_DEVICE_ID);
otp_accessor_read_bytes(otp_read_otp_pcie_subsys_vendor_id, OTP_PCIE_ID, OTP_PCIE_SUBSYS_VENDOR_ID);
otp_accessor_group_read(otp_read_otp_pcie_bars, OTP_PCIE_BARS);
otp_accessor_group_read(otp_read_otp_tbbr_rotpk, OTP_TBBR_ROTPK);
otp_accessor_group_read(otp_read_otp_tbbr_huk, OTP_TBBR_HUK);
otp_accessor_group_read(otp_read_otp_tbbr_ek, OTP_TBBR_EK);
otp_accessor_group_read(otp_read_otp_tbbr_ssk, OTP_TBBR_SSK);
otp_accessor_group_read(otp_read_otp_sjtag_ssk, OTP_SJTAG_SSK);
otp_accessor_group_read(otp_read_otp_strap_disable_mask, OTP_STRAP_DISABLE_MASK);
otp_accessor_group_read(otp_read_otp_tbbr_ntnvct, OTP_TBBR_NTNVCT);
otp_accessor_group_read(otp_read_otp_tbbr_tnvct, OTP_TBBR_TNVCT);

#define OTP_REGION_ADDR(n) (PROTECT_REGION_ADDR_ADDR + (4 * (n)))

#endif	/* PLAT_OTP_H */
