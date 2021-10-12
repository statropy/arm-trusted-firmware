/*
 * Copyright (c) 2016-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h>

#include <firmware_image_package.h>
#include "tbbr_config.h"

/* The images used depends on the platform. */
toc_entry_t toc_entries[] = {
	{
		.id = -1,
		.name = "SCP Firmware Updater Configuration FWU SCP_BL2U",
		.uuid = UUID_TRUSTED_UPDATE_FIRMWARE_SCP_BL2U,
		.cmdline_name = "scp-fwu-cfg"
	},
	{
		.id = BL2U_IMAGE_ID,
		.name = "AP Firmware Updater Configuration BL2U",
		.uuid = UUID_TRUSTED_UPDATE_FIRMWARE_BL2U,
		.cmdline_name = "ap-fwu-cfg"
	},
	{
		.id = -1,
		.name = "Firmware Updater NS_BL2U",
		.uuid = UUID_TRUSTED_UPDATE_FIRMWARE_NS_BL2U,
		.cmdline_name = "fwu"
	},
	{
		.id = FWU_CERT_ID,
		.name = "Non-Trusted Firmware Updater certificate",
		.uuid = UUID_TRUSTED_FWU_CERT,
		.cmdline_name = "fwu-cert"
	},
	{
		.id = BL2_IMAGE_ID,
		.name = "Trusted Boot Firmware BL2",
		.uuid = UUID_TRUSTED_BOOT_FIRMWARE_BL2,
		.cmdline_name = "tb-fw"
	},
	{
		.id = SCP_BL2_IMAGE_ID,
		.name = "SCP Firmware SCP_BL2",
		.uuid = UUID_SCP_FIRMWARE_SCP_BL2,
		.cmdline_name = "scp-fw"
	},
	{
		.id = BL31_IMAGE_ID,
		.name = "EL3 Runtime Firmware BL31",
		.uuid = UUID_EL3_RUNTIME_FIRMWARE_BL31,
		.cmdline_name = "soc-fw"
	},
	{
		.id = BL32_IMAGE_ID,
		.name = "Secure Payload BL32 (Trusted OS)",
		.uuid = UUID_SECURE_PAYLOAD_BL32,
		.cmdline_name = "tos-fw"
	},
	{
		.id = BL32_EXTRA1_IMAGE_ID,
		.name = "Secure Payload BL32 Extra1 (Trusted OS Extra1)",
		.uuid = UUID_SECURE_PAYLOAD_BL32_EXTRA1,
		.cmdline_name = "tos-fw-extra1"
	},
	{
		.id = BL32_EXTRA2_IMAGE_ID,
		.name = "Secure Payload BL32 Extra2 (Trusted OS Extra2)",
		.uuid = UUID_SECURE_PAYLOAD_BL32_EXTRA2,
		.cmdline_name = "tos-fw-extra2"
	},
	{
		.id = BL33_IMAGE_ID,
		.name = "Non-Trusted Firmware BL33",
		.uuid = UUID_NON_TRUSTED_FIRMWARE_BL33,
		.cmdline_name = "nt-fw"
	},
	/* Dynamic Configs */
	{
		.id = FW_CONFIG_ID,
		.name = "FW_CONFIG",
		.uuid = UUID_FW_CONFIG,
		.cmdline_name = "fw-config"
	},
	{
		.id = HW_CONFIG_ID,
		.name = "HW_CONFIG",
		.uuid = UUID_HW_CONFIG,
		.cmdline_name = "hw-config"
	},
	{
		.id = TB_FW_CONFIG_ID,
		.name = "TB_FW_CONFIG",
		.uuid = UUID_TB_FW_CONFIG,
		.cmdline_name = "tb-fw-config"
	},
	{
		.id = SOC_FW_CONFIG_ID,
		.name = "SOC_FW_CONFIG",
		.uuid = UUID_SOC_FW_CONFIG,
		.cmdline_name = "soc-fw-config"
	},
	{
		.id = TOS_FW_CONFIG_ID,
		.name = "TOS_FW_CONFIG",
		.uuid = UUID_TOS_FW_CONFIG,
		.cmdline_name = "tos-fw-config"
	},
	{
		.id = NT_FW_CONFIG_ID,
		.name = "NT_FW_CONFIG",
		.uuid = UUID_NT_FW_CONFIG,
		.cmdline_name = "nt-fw-config"
	},
	/* Key Certificates */
	{
		.id = -1,
		.name = "Root Of Trust key certificate",
		.uuid = UUID_ROT_KEY_CERT,
		.cmdline_name = "rot-cert"
	},
	{
		.id = TRUSTED_KEY_CERT_ID,
		.name = "Trusted key certificate",
		.uuid = UUID_TRUSTED_KEY_CERT,
		.cmdline_name = "trusted-key-cert"
	},
	{
		.id = SCP_FW_KEY_CERT_ID,
		.name = "SCP Firmware key certificate",
		.uuid = UUID_SCP_FW_KEY_CERT,
		.cmdline_name = "scp-fw-key-cert"
	},
	{
		.id = SOC_FW_KEY_CERT_ID,
		.name = "SoC Firmware key certificate",
		.uuid = UUID_SOC_FW_KEY_CERT,
		.cmdline_name = "soc-fw-key-cert"
	},
	{
		.id = TRUSTED_OS_FW_KEY_CERT_ID,
		.name = "Trusted OS Firmware key certificate",
		.uuid = UUID_TRUSTED_OS_FW_KEY_CERT,
		.cmdline_name = "tos-fw-key-cert"
	},
	{
		.id = NON_TRUSTED_FW_KEY_CERT_ID,
		.name = "Non-Trusted Firmware key certificate",
		.uuid = UUID_NON_TRUSTED_FW_KEY_CERT,
		.cmdline_name = "nt-fw-key-cert"
	},

	/* Content certificates */
	{
		.id = TRUSTED_BOOT_FW_CERT_ID,
		.name = "Trusted Boot Firmware BL2 certificate",
		.uuid = UUID_TRUSTED_BOOT_FW_CERT,
		.cmdline_name = "tb-fw-cert"
	},
	{
		.id = SCP_FW_CONTENT_CERT_ID,
		.name = "SCP Firmware content certificate",
		.uuid = UUID_SCP_FW_CONTENT_CERT,
		.cmdline_name = "scp-fw-cert"
	},
	{
		.id = SOC_FW_CONTENT_CERT_ID,
		.name = "SoC Firmware content certificate",
		.uuid = UUID_SOC_FW_CONTENT_CERT,
		.cmdline_name = "soc-fw-cert"
	},
	{
		.id = TRUSTED_OS_FW_CONTENT_CERT_ID,
		.name = "Trusted OS Firmware content certificate",
		.uuid = UUID_TRUSTED_OS_FW_CONTENT_CERT,
		.cmdline_name = "tos-fw-cert"
	},
	{
		.id = NON_TRUSTED_FW_CONTENT_CERT_ID,
		.name = "Non-Trusted Firmware content certificate",
		.uuid = UUID_NON_TRUSTED_FW_CONTENT_CERT,
		.cmdline_name = "nt-fw-cert"
	},
	{
		.id = -1,
		.name = "SiP owned Secure Partition content certificate",
		.uuid = UUID_SIP_SECURE_PARTITION_CONTENT_CERT,
		.cmdline_name = "sip-sp-cert"
	},
	{
		.id = -1,
		.name = "Platform owned Secure Partition content certificate",
		.uuid = UUID_PLAT_SECURE_PARTITION_CONTENT_CERT,
		.cmdline_name = "plat-sp-cert"
	},
	{
		.id = -1,
		.name = NULL,
		.uuid = { {0} },
		.cmdline_name = NULL,
	}
};
