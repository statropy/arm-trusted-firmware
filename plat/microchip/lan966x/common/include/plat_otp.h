
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
#define OTP_PRG_LEN                              32

/* Fields in OTP_PRG */
#define OTP_OTP_PRG_FEAT_DIS_OFF                 3
#define OTP_OTP_PRG_PARTID_OFF                   2
#define OTP_OTP_PRG_JTAG_UUID_OFF                1
#define OTP_OTP_PRG_SECURE_JTAG_OFF              0

/* FEAT_DIS */
#define FEAT_DIS_ADDR                            4
#define FEAT_DIS_LEN                             8

/* Fields in FEAT_DIS */
#define OTP_RTE_DISABLE_OFF                      5
#define OTP_SERDES2_DISABLE_OFF                  4
#define OTP_CAN_DISABLE_OFF                      3
#define OTP_CPU_SEC_DBG_DISABLE_OFF              2
#define OTP_CPU_DBG_DISABLE_OFF                  1
#define OTP_CPU_DISABLE_OFF                      0

/* PARTID */
#define PARTID_ADDR                              5
#define PARTID_LEN                               16

/* SERIAL_NUMBER */
#define SERIAL_NUMBER_ADDR                       7
#define SERIAL_NUMBER_LEN                        64

/* TST_TRK */
#define TST_TRK_ADDR                             15
#define TST_TRK_LEN                              8

/* SECURE_JTAG */
#define SECURE_JTAG_ADDR                         16
#define SECURE_JTAG_LEN                          32

/* Fields in SECURE_JTAG */
#define OTP_JTAG_DISABLE_OFF                     27
#define OTP_JTAG_MODE_OFF                        0
#define OTP_JTAG_MODE_LEN                        2

/* WAFER_TRK */
#define WAFER_TRK_ADDR                           20
#define WAFER_TRK_LEN                            56

/* JTAG_UUID */
#define JTAG_UUID_ADDR                           32
#define JTAG_UUID_LEN                            80

/* PROTECT_OTP_WRITE */
#define PROTECT_OTP_WRITE_ADDR                   64
#define PROTECT_OTP_WRITE_LEN                    32

/* Fields in PROTECT_OTP_WRITE */
#define OTP_OTP_WRITE_PROTECT_OFF                0
#define OTP_OTP_WRITE_PROTECT_LEN                8

/* PROTECT_REGION_ADDR */
#define PROTECT_REGION_ADDR_ADDR                 68
#define PROTECT_REGION_ADDR_LEN                  256

/* Fields in PROTECT_REGION_ADDR */
#define OTP_PROTECT_REGION0_START_ADDR_OFF       240
#define OTP_PROTECT_REGION0_START_ADDR_LEN       16
#define OTP_PROTECT_REGION0_END_ADDR_OFF         224
#define OTP_PROTECT_REGION0_END_ADDR_LEN         16
#define OTP_PROTECT_REGION1_START_ADDR_OFF       208
#define OTP_PROTECT_REGION1_START_ADDR_LEN       16
#define OTP_PROTECT_REGION1_END_ADDR_OFF         192
#define OTP_PROTECT_REGION1_END_ADDR_LEN         16
#define OTP_PROTECT_REGION2_START_ADDR_OFF       176
#define OTP_PROTECT_REGION2_START_ADDR_LEN       16
#define OTP_PROTECT_REGION2_END_ADDR_OFF         160
#define OTP_PROTECT_REGION2_END_ADDR_LEN         16
#define OTP_PROTECT_REGION3_START_ADDR_OFF       144
#define OTP_PROTECT_REGION3_START_ADDR_LEN       16
#define OTP_PROTECT_REGION3_END_ADDR_OFF         128
#define OTP_PROTECT_REGION3_END_ADDR_LEN         16
#define OTP_PROTECT_REGION4_START_ADDR_OFF       112
#define OTP_PROTECT_REGION4_START_ADDR_LEN       16
#define OTP_PROTECT_REGION4_END_ADDR_OFF         96
#define OTP_PROTECT_REGION4_END_ADDR_LEN         16
#define OTP_PROTECT_REGION5_START_ADDR_OFF       80
#define OTP_PROTECT_REGION5_START_ADDR_LEN       16
#define OTP_PROTECT_REGION5_END_ADDR_OFF         64
#define OTP_PROTECT_REGION5_END_ADDR_LEN         16
#define OTP_PROTECT_REGION6_START_ADDR_OFF       48
#define OTP_PROTECT_REGION6_START_ADDR_LEN       16
#define OTP_PROTECT_REGION6_END_ADDR_OFF         32
#define OTP_PROTECT_REGION6_END_ADDR_LEN         16
#define OTP_PROTECT_REGION7_START_ADDR_OFF       16
#define OTP_PROTECT_REGION7_START_ADDR_LEN       16
#define OTP_PROTECT_REGION7_END_ADDR_OFF         0
#define OTP_PROTECT_REGION7_END_ADDR_LEN         16

/* DEF_MAC_ADDR_AND_LEN */
#define DEF_MAC_ADDR_AND_LEN_ADDR                100
#define DEF_MAC_ADDR_AND_LEN_LEN                 64

/* TRIM */
#define TRIM_ADDR                                108
#define TRIM_LEN                                 64

/* Fields in TRIM */
#define OTP_UVOV_TRIM0_OFF                       37
#define OTP_UVOV_TRIM0_LEN                       6
#define OTP_UVOV_TRIM1_OFF                       31
#define OTP_UVOV_TRIM1_LEN                       6
#define OTP_UVOV_TRIM2_OFF                       25
#define OTP_UVOV_TRIM2_LEN                       6
#define OTP_UVOV_TRIM3_OFF                       19
#define OTP_UVOV_TRIM3_LEN                       6
#define OTP_COM_BIAS_BG_TC_TRIM_OFF              14
#define OTP_COM_BIAS_BG_TC_TRIM_LEN              5
#define OTP_COM_BIAS_BG_MAG_TRIM_OFF             8
#define OTP_COM_BIAS_BG_MAG_TRIM_LEN             6
#define OTP_COM_RBIAS_TC_TRIM_OFF                6
#define OTP_COM_RBIAS_TC_TRIM_LEN                2
#define OTP_COM_RBIAS_MAG_TRIM_OFF               0
#define OTP_COM_RBIAS_MAG_TRIM_LEN               6

/* OTP_PCIE */
#define OTP_PCIE_ADDR                            116
#define OTP_PCIE_LEN                             104

/* Fields in OTP_PCIE */
#define OTP_OTP_PCIE_DISABLE_REV_3_OFF           97
#define OTP_OTP_PCIE_DISABLE_REV_2_OFF           96
#define OTP_OTP_PCIE_VENDOR_ID_OFF               80
#define OTP_OTP_PCIE_VENDOR_ID_LEN               16
#define OTP_OTP_PCIE_VENDOR_SUB_ID_OFF           64
#define OTP_OTP_PCIE_VENDOR_SUB_ID_LEN           16
#define OTP_OTP_PCIE_CLASS_CODE_OFF              40
#define OTP_OTP_PCIE_CLASS_CODE_LEN              24
#define OTP_OTP_PCIE_REVISION_ID_OFF             32
#define OTP_OTP_PCIE_REVISION_ID_LEN             8
#define OTP_OTP_PCIE_SUBSYSTEM_ID_OFF            16
#define OTP_OTP_PCIE_SUBSYSTEM_ID_LEN            16
#define OTP_OTP_PCIE_SUBSYSTEM_VENDOR_ID_OFF     0
#define OTP_OTP_PCIE_SUBSYSTEM_VENDOR_ID_LEN     16

/* OTP_FLAGS1 */
#define OTP_FLAGS1_ADDR                          129
#define OTP_FLAGS1_LEN                           32

/* Fields in OTP_FLAGS1 */
#define OTP_OTP_FLAGS1_DISABLE_OTP_EMU_OFF       14
#define OTP_OTP_FLAGS1_ENABLE_TBBR_OFF           13
#define OTP_OTP_FLAGS1_ENABLE_FIRMWARE_ENCRYPT_OFF 12
#define OTP_OTP_FLAGS1_FIRMWARE_ENCRYPT_BSSK_OFF 11
#define OTP_OTP_FLAGS1_ENABLE_DDR_ENCRYPT_OFF    10
#define OTP_OTP_FLAGS1_SJTAG_FREEZE_OFF          9
#define OTP_OTP_FLAGS1_QSPI_HEADER_DISABLE_OFF   8
#define OTP_OTP_FLAGS1_EMMC_HEADER_DISABLE_OFF   7
#define OTP_OTP_FLAGS1_COPY_USER_AREA_0_OFF      6
#define OTP_OTP_FLAGS1_COPY_USER_AREA_1_OFF      5
#define OTP_OTP_FLAGS1_COPY_USER_AREA_2_OFF      4
#define OTP_OTP_FLAGS1_COPY_USER_AREA_3_OFF      3
#define OTP_OTP_FLAGS1_ALLOW_CLEAR_TEXT_OFF      2
#define OTP_OTP_FLAGS1_ALLOW_SSK_ENCRYPTED_OFF   1
#define OTP_OTP_FLAGS1_ALLOW_BSSK_ENCRYPTED_OFF  0

/* OTP_STRAP_DISABLE_MASK */
#define OTP_STRAP_DISABLE_MASK_ADDR              133
#define OTP_STRAP_DISABLE_MASK_LEN               16

/* OTP_TBBR */
#define OTP_TBBR_ADDR                            135
#define OTP_TBBR_LEN                             1344

/* Fields in OTP_TBBR */
#define OTP_OTP_TBBR_ROTPK_OFF                   1088
#define OTP_OTP_TBBR_ROTPK_LEN                   256
#define OTP_OTP_TBBR_HUK_OFF                     832
#define OTP_OTP_TBBR_HUK_LEN                     256
#define OTP_OTP_TBBR_EK_OFF                      576
#define OTP_OTP_TBBR_EK_LEN                      256
#define OTP_OTP_TBBR_SSK_OFF                     320
#define OTP_OTP_TBBR_SSK_LEN                     256
#define OTP_OTP_TBBR_TNVCT_OFF                   256
#define OTP_OTP_TBBR_TNVCT_LEN                   64
#define OTP_OTP_TBBR_NTNVCT_OFF                  0
#define OTP_OTP_TBBR_NTNVCT_LEN                  256

/* OTP_USER */
#define OTP_USER_ADDR                            303
#define OTP_USER_LEN                             2816

/* Fields in OTP_USER */
#define OTP_OTP_USER_AREA_0_OFF                  2560
#define OTP_OTP_USER_AREA_0_LEN                  256
#define OTP_OTP_USER_AREA_1_OFF                  2048
#define OTP_OTP_USER_AREA_1_LEN                  512
#define OTP_OTP_USER_AREA_2_OFF                  1024
#define OTP_OTP_USER_AREA_2_LEN                  1024
#define OTP_OTP_USER_AREA_3_OFF                  0
#define OTP_OTP_USER_AREA_3_LEN                  1024


/* OTP access functions */
#define otp_accessor_group_read(aname, grp_name)			\
static inline int aname(uint8_t *dst, size_t nbytes)			\
{									\
	assert((nbytes * 8) >= grp_name##_LEN);				\
	return otp_read_bits(dst, grp_name##_ADDR * 8, grp_name##_LEN); \
}

#define otp_accessor_read_bool(aname, grp_name, fld_name)		\
static inline bool aname(void)						\
{									\
	uint8_t b;							\
	int addr = grp_name##_ADDR + (OTP_##fld_name##_OFF / 8);	\
	int off = OTP_##fld_name##_OFF % 8;				\
	(void) otp_read_bits(&b, addr * 8, 8);				\
	return !!(b & (1 << off));					\
}

#define otp_accessor_read_bytes(aname, grp_name, fld_name)		\
static inline int aname(uint8_t *dst, size_t nbytes)			\
{									\
	assert((nbytes * 8) >= OTP_##fld_name##_LEN);			\
	return otp_read_bits(dst, grp_name##_ADDR * 8 + 		\
		OTP_##fld_name##_OFF, OTP_##fld_name##_LEN);		\
}

#define otp_accessor_read_field(aname, grp_name, fld_name)		\
static inline uint32_t aname(void)					\
{									\
	uint32_t w;							\
	int addr = grp_name##_ADDR + (OTP_##fld_name##_OFF / 8);	\
	int off = OTP_##fld_name##_OFF % 8;				\
	(void) otp_read_uint32(&w, addr * 8);				\
	return (w >> off) & GENMASK(OTP_##fld_name##_LEN - 1, 0);	\
}

otp_accessor_group_read(otp_read_otp_prg, OTP_PRG);
otp_accessor_group_read(otp_read_feat_dis, FEAT_DIS);
otp_accessor_group_read(otp_read_partid, PARTID);
otp_accessor_group_read(otp_read_serial_number, SERIAL_NUMBER);
otp_accessor_read_bool(otp_read_jtag_disable, SECURE_JTAG, JTAG_DISABLE);
otp_accessor_read_field(otp_read_jtag_mode, SECURE_JTAG, JTAG_MODE);
otp_accessor_group_read(otp_read_otp_flags1, OTP_FLAGS1);
otp_accessor_read_bool(otp_read_otp_flags1_disable_otp_emu, OTP_FLAGS1, OTP_FLAGS1_DISABLE_OTP_EMU);
otp_accessor_read_bool(otp_read_otp_flags1_enable_tbbr, OTP_FLAGS1, OTP_FLAGS1_ENABLE_TBBR);
otp_accessor_read_bool(otp_read_otp_flags1_enable_firmware_encrypt, OTP_FLAGS1, OTP_FLAGS1_ENABLE_FIRMWARE_ENCRYPT);
otp_accessor_read_bool(otp_read_otp_flags1_firmware_encrypt_bssk, OTP_FLAGS1, OTP_FLAGS1_FIRMWARE_ENCRYPT_BSSK);
otp_accessor_read_bool(otp_read_otp_flags1_enable_ddr_encrypt, OTP_FLAGS1, OTP_FLAGS1_ENABLE_DDR_ENCRYPT);
otp_accessor_read_bool(otp_read_otp_flags1_sjtag_freeze, OTP_FLAGS1, OTP_FLAGS1_SJTAG_FREEZE);
otp_accessor_read_bool(otp_read_otp_flags1_qspi_header_disable, OTP_FLAGS1, OTP_FLAGS1_QSPI_HEADER_DISABLE);
otp_accessor_read_bool(otp_read_otp_flags1_emmc_header_disable, OTP_FLAGS1, OTP_FLAGS1_EMMC_HEADER_DISABLE);
otp_accessor_read_bool(otp_read_otp_flags1_copy_user_area_0, OTP_FLAGS1, OTP_FLAGS1_COPY_USER_AREA_0);
otp_accessor_read_bool(otp_read_otp_flags1_copy_user_area_1, OTP_FLAGS1, OTP_FLAGS1_COPY_USER_AREA_1);
otp_accessor_read_bool(otp_read_otp_flags1_copy_user_area_2, OTP_FLAGS1, OTP_FLAGS1_COPY_USER_AREA_2);
otp_accessor_read_bool(otp_read_otp_flags1_copy_user_area_3, OTP_FLAGS1, OTP_FLAGS1_COPY_USER_AREA_3);
otp_accessor_read_bool(otp_read_otp_flags1_allow_clear_text, OTP_FLAGS1, OTP_FLAGS1_ALLOW_CLEAR_TEXT);
otp_accessor_read_bool(otp_read_otp_flags1_allow_ssk_encrypted, OTP_FLAGS1, OTP_FLAGS1_ALLOW_SSK_ENCRYPTED);
otp_accessor_read_bool(otp_read_otp_flags1_allow_bssk_encrypted, OTP_FLAGS1, OTP_FLAGS1_ALLOW_BSSK_ENCRYPTED);
otp_accessor_group_read(otp_read_otp_strap_disable_mask, OTP_STRAP_DISABLE_MASK);
otp_accessor_read_bytes(otp_read_otp_tbbr_rotpk, OTP_TBBR, OTP_TBBR_ROTPK);
otp_accessor_read_bytes(otp_read_otp_tbbr_huk, OTP_TBBR, OTP_TBBR_HUK);
otp_accessor_read_bytes(otp_read_otp_tbbr_ek, OTP_TBBR, OTP_TBBR_EK);
otp_accessor_read_bytes(otp_read_otp_tbbr_ssk, OTP_TBBR, OTP_TBBR_SSK);
otp_accessor_read_bytes(otp_read_otp_tbbr_tnvct, OTP_TBBR, OTP_TBBR_TNVCT);
otp_accessor_read_bytes(otp_read_otp_tbbr_ntnvct, OTP_TBBR, OTP_TBBR_NTNVCT);
otp_accessor_read_bytes(otp_read_otp_user_area_0, OTP_USER, OTP_USER_AREA_0);
otp_accessor_read_bytes(otp_read_otp_user_area_1, OTP_USER, OTP_USER_AREA_1);
otp_accessor_read_bytes(otp_read_otp_user_area_2, OTP_USER, OTP_USER_AREA_2);
otp_accessor_read_bytes(otp_read_otp_user_area_3, OTP_USER, OTP_USER_AREA_3);

#endif	/* PLAT_OTP_H */
