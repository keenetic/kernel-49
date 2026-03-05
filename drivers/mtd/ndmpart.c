/*
 * Copyright (C) 2016 Alexander Papenko <ap@ndmsystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * NDM flash partition table.
 *
 * Based on ar7part by Eugene Konev <ejka@openwrt.org>
 * Based on ar7 map by Felix Fietkau <nbd@openwrt.org>
 *
 * 2016-2019, Eugene Yudin <e.yudin@ndmsystems.com>
 *  - Dual Image support
 *  - "U-State" search
 */

#define pr_fmt(fmt)				  KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/magic.h>
#include <linux/sizes.h>

#ifdef CONFIG_NMBM_MTD
#include <linux/mtd/nmbm/nmbm-mtd.h>
#endif /* CONFIG_NMBM_MTD */

#ifdef CONFIG_AIROHA_BMT
#include <linux/mtd/airoha-bmt.h>
#endif /* CONFIG_AIROHA_BMT */

#include "ndmpart.h"
#include "ndmpart_di.h"
#include "ndmpart_upd.h"

#ifndef SQUASHFS_MAGIC
#define SQUASHFS_MAGIC			0x73717368
#endif

#define NDMS_MAGIC			0x736D646E
#define CONFIG_MAGIC			cpu_to_be32(0x2e6e646d)
#define CONFIG_MAGIC_V1			cpu_to_be32(0x1f8b0801)

#define KERNEL_MAGIC			be32_to_cpu(0x27051956)
#define ROOTFS_MAGIC			SQUASHFS_MAGIC

#define PART_SIZE_UNKNOWN		(~0)

#define PART_STORAGE_LEGACY		"Storage_Legacy"

#if !defined(CONFIG_MTD_NDM_OS_ON_MMC)
#define PLACE_OS_ON_MTD
#endif

#if defined(CONFIG_MACH_MT7622)
#define HAS_PART_ROM_HDR
#define HAS_PART_ATF
#endif

#if defined(CONFIG_MACH_MT7622) || \
    defined(CONFIG_MACH_MT7981) || \
    defined(CONFIG_MACH_MT7986) || \
    defined(CONFIG_MACH_MT7987) || \
    defined(CONFIG_MACH_MT7988)
#define HAS_PART_PRELOADER
#endif

static const char **__ndmpart_rootfs_name;

enum part {
	/* Image 1 */
#ifdef HAS_PART_ROM_HDR
	PART_ROM_HDR,
#endif
#ifdef HAS_PART_PRELOADER
	PART_PRELOADER,		/* BL2 */
#endif
#ifdef HAS_PART_ATF
	PART_ATF,		/* ARM Trusted Firmware (BL31) */
#endif
	PART_U_BOOT,		/* U-Boot or FIP */
	PART_U_CONFIG,
	PART_RF_EEPROM,
#ifdef PLACE_OS_ON_MTD
	PART_KERNEL_1,
	PART_ROOTFS_1,
	PART_FIRMWARE_1,
	PART_CONFIG_1,
	PART_STORAGE,		/* optional */
	PART_DUMP,		/* optional */
	PART_STORAGE_A,		/* optional */
#else
	/* PART_CONFIG_1 will be removed */
	PART_CONFIG_1,
	PART_DUMP,		/* optional */
#endif
	/* Image 2 */
	PART_U_STATE,
	PART_RESERVE,
	PART_U_CONFIG_RES,
	PART_RF_EEPROM_RES,
#ifdef PLACE_OS_ON_MTD
	PART_KERNEL_2,		/* optional */
	PART_ROOTFS_2,		/* optional */
	PART_FIRMWARE_2,	/* optional */
	PART_CONFIG_2,		/* optional */
	PART_STORAGE_B,		/* optional */
#else
	/* PART_CONFIG_2 will be removed */
	PART_CONFIG_2,		/* optional */
#endif
	/* Pseudo */
	PART_FULL,
	PART_MAX
};

struct part_dsc {
	const char	*name;
	uint32_t	 offset;
	uint32_t	 size;
	bool		 skip;
	bool		 read_only;
};

static struct part_dsc parts[PART_MAX] = {
	/* Image 1 */
#ifdef HAS_PART_ROM_HDR
	[PART_ROM_HDR] = {
		.name		= "ROM-Header",
		.read_only	= true
	},
#endif /* HAS_PART_ROM_HDR */
#ifdef HAS_PART_PRELOADER
	[PART_PRELOADER] = {
		.name		= NDMPART_NAME_PRELOADER,
		.read_only	= true
	},
#endif /* HAS_PART_PRELOADER */
#ifdef HAS_PART_ATF
	[PART_ATF] = {
		.name		= NDMPART_NAME_ATF,
		.read_only	= true
	},
#endif /* HAS_PART_ATF */
	[PART_U_BOOT] = {
		.name		= NDMPART_NAME_U_BOOT,
		.read_only	= true
	},
	[PART_U_CONFIG] = {
		.name		= "U-Config"
	},
	[PART_RF_EEPROM] = {
		.name		= "RF-EEPROM"
	},
#ifdef PLACE_OS_ON_MTD
	[PART_KERNEL_1] = {
		.name		= "Kernel",
		.skip		= true,
		.read_only	= true
	},
	[PART_ROOTFS_1] = {
		.name		= "RootFS",
		.skip		= true,
		.read_only	= true
	},
	[PART_FIRMWARE_1] = {
		.name		= "Firmware"
	},
	[PART_CONFIG_1] = {
		.name		= "Config"
	},
	[PART_STORAGE] = {
		.name		= "Storage",
		.skip		= true
	},
	[PART_DUMP] = {
		.name		= "Dump",
		.skip		= true
	},
	[PART_STORAGE_A] = {
		.name		= "Storage_A",
		.skip		= true
	},
#else
	/* PART_CONFIG_1 will be removed */
	[PART_CONFIG_1] = {
		.name		= "Config"
	},
	[PART_DUMP] = {
		.name		= "Dump",
		.skip		= true
	},
#endif
	/* Image 2 */
	[PART_U_STATE] = {
		.name		= "U-State",
		.skip		= true
	},
	[PART_RESERVE] = {
		.name		= "Reserve",
		.skip		= true
	},
	[PART_U_CONFIG_RES] = {
		.name		= "D-State",
		.skip		= true
	},
	[PART_RF_EEPROM_RES] = {
		.name		= "RF-EEPROM_res",
		.skip		= true
	},
#ifdef PLACE_OS_ON_MTD
	[PART_KERNEL_2] = {
		.name		= "Kernel_2",
		.skip		= true,
		.read_only	= true
	},
	[PART_ROOTFS_2] = {
		.name		= "RootFS_2",
		.skip		= true,
		.read_only	= true
	},
	[PART_FIRMWARE_2] = {
		.name		= "Firmware_2",
		.skip		= true
	},
	[PART_CONFIG_2] = {
		.name		= "Config_2",
		.skip		= true
	},
	[PART_STORAGE_B] = {
		.name		= "Storage_B",
		.skip		= true
	},
#else
	/* PART_CONFIG_2 will be removed */
	[PART_CONFIG_2] = {
		.name		= "Config_2",
		.skip		= true
	},
#endif
	/* Pseudo */
	[PART_FULL] = {
		.name		= "Full",
		.size		= MTDPART_SIZ_FULL,
		.read_only	= true
	}
};

static inline uint32_t parts_offset_end(enum part part)
{
	return parts[part].offset + parts[part].size;
}

static uint32_t parts_size_default_get(enum part part,
				       struct mtd_info *master)
{
	uint32_t size = PART_SIZE_UNKNOWN;
#if !defined(CONFIG_ARCH_VEXPRESS)
	bool is_nor = master->type == MTD_NORFLASH;
#endif

#if defined(CONFIG_ARCH_VEXPRESS)
	/* QEMU */
	switch (part) {
	case PART_U_BOOT:
		size = PART_BOOT_SIZE;
		break;
	case PART_U_CONFIG:
		size = PART_ENV_SIZE;
		break;
	case PART_RF_EEPROM:
		size = PART_E2P_SIZE;
		break;
	case PART_CONFIG_1:
		size = master->erasesize * 4;
		break;
	case PART_U_STATE:
		size = PART_UST_SIZE;
		break;
	case PART_RESERVE:
		size = PART_RSV_SIZE;
		break;
	default:
		break;
	}
#elif defined(CONFIG_MACH_MT7622)
	switch (part) {
	case PART_ROM_HDR:
		size = master->erasesize;
		break;
	case PART_PRELOADER:
		size = is_nor ? PART_BL2_SIZE_NOR : PART_BL2_SIZE_NAND;
		/* subtract ROM header */
		size -= master->erasesize;
		break;
	case PART_ATF:
		size = is_nor ? PART_ATF_SIZE_NOR : PART_ATF_SIZE_NAND;
		break;
	case PART_U_BOOT:
		size = is_nor ? PART_BOOT_SIZE_NOR : PART_BOOT_SIZE_NAND;
		break;
	case PART_U_CONFIG:
		size = is_nor ? PART_ENV_SIZE_NOR : PART_ENV_SIZE_NAND;
		break;
	case PART_RF_EEPROM:
		size = is_nor ? PART_E2P_SIZE_NOR : PART_E2P_SIZE_NAND;
		break;
	case PART_CONFIG_1:
		size = master->erasesize * 4;
		break;
	case PART_U_STATE:
		size = master->erasesize;
		break;
	case PART_RESERVE:
		size = is_nor ? PART_RSV_SIZE_NOR : PART_RSV_SIZE_NAND;
		/* subtract U_STATE */
		size -= master->erasesize;
		break;
	default:
		break;
	}
#elif defined(CONFIG_MACH_MT7981) || \
      defined(CONFIG_MACH_MT7986) || \
      defined(CONFIG_MACH_MT7987) || \
      defined(CONFIG_MACH_MT7988) || \
      defined(CONFIG_MACH_AN7552) || \
      defined(CONFIG_MACH_AN7581) || \
      defined(CONFIG_MACH_AN7583)
	switch (part) {
#ifdef HAS_PART_PRELOADER
	case PART_PRELOADER:
		size = is_nor ? PART_BL2_SIZE_NOR : PART_BL2_SIZE_NAND;
		break;
#endif /* HAS_PART_PRELOADER */
	case PART_U_BOOT:
		size = is_nor ? PART_FIP_SIZE_NOR : PART_FIP_SIZE_NAND;
		break;
	case PART_U_CONFIG:
		size = is_nor ? PART_ENV_SIZE_NOR : PART_ENV_SIZE_NAND;
		break;
	case PART_RF_EEPROM:
		size = is_nor ? PART_E2P_SIZE_NOR : PART_E2P_SIZE_NAND;
		break;
	case PART_CONFIG_1:
		size = master->erasesize * 4;
		break;
	case PART_U_STATE:
		size = is_nor ? PART_UST_SIZE_NOR : PART_UST_SIZE_NAND;
		break;
	case PART_RESERVE:
		size = is_nor ? PART_RSV_SIZE_NOR : PART_RSV_SIZE_NAND;
		break;
	default:
		break;
	}
#else
	/* MIPS boards */
	switch (part) {
	case PART_U_BOOT:
	case PART_U_STATE:
		if (!is_nor)
#ifdef NAND_BB_MODE_SKIP
			size = master->erasesize << 2;
#else
			size = master->erasesize;
#endif
		else
			size = 3 * master->erasesize;
		break;
	case PART_U_CONFIG:
	case PART_RF_EEPROM:
	case PART_CONFIG_1:
		size = master->erasesize;
#ifdef NAND_BB_MODE_SKIP
		if (!is_nor)
			size = master->erasesize << 2;
#endif
		break;
	case PART_RESERVE:
		size = 0;
		break;
	default:
		break;
	}

#ifdef PART_RF_EEPROM_AX_BOARD
	if (part == PART_RF_EEPROM) {
		if (!is_nor)
#ifdef NAND_BB_MODE_SKIP
			size = master->erasesize << 2;	/* 512K */
#else
			size = master->erasesize << 1;	/* 256K */
#endif
		else
			size = 4 * master->erasesize;	/* 256K */
	}
#endif /* PART_RF_EEPROM_AX_BOARD */
#endif

	return size;
}

#ifdef PLACE_OS_ON_MTD
static u32 part_rootfs_offset(struct mtd_info *master, u32 begin, u32 size)
{
	/* skip first kernel blocks to speedup */
	u32 off = begin + SZ_1M;

	for (; off < begin + size; off += master->erasesize) {
		__le32 magic;
		size_t len;

		if (mtd_read(master, off, sizeof(magic), &len, (u8 *)&magic))
			continue;

		if (le32_to_cpu(magic) == ROOTFS_MAGIC ||
		    le32_to_cpu(magic) == NDMS_MAGIC)
			return off;
	}

	return 0;
}
#endif

static int create_mtd_partitions(struct mtd_info *m,
				 const struct mtd_partition **pparts,
				 struct mtd_part_parser_data *data)
{
	int i, j, parts_nr = 0;
	u8 di_current = NDMPART_DI_IMAGE_FIRST;
	uint32_t u_state_offset = NDMPART_DI_NO_OFFSET;
	uint32_t offs_uboot = 0;
	uint32_t flash_size_lim = 0;
	struct mtd_partition *mp = NULL;
#ifdef PLACE_OS_ON_MTD
	uint32_t off;
	bool use_storage = CONFIG_MTD_NDM_STORAGE_SIZE ? true : false;
#endif
	bool use_dump = CONFIG_MTD_NDM_DUMP_SIZE ? true : false;

#ifdef CONFIG_NMBM_MTD
	if (!is_nmbm_mtd(m))
		return 0;
#endif /* CONFIG_NMBM_MTD */

#ifdef CONFIG_AIROHA_BMT
	airoha_bmt_attach(m);
#endif /* CONFIG_AIROHA_BMT */

	u_state_offset = ndmpart_di_u_state_offset(m);

#ifdef PLACE_OS_ON_MTD
	flash_size_lim = CONFIG_MTD_NDM_FLASH_SIZE_LIMIT;
#endif
	if (!flash_size_lim || flash_size_lim > m->size) {
		if (u_state_offset != NDMPART_DI_NO_OFFSET)
			flash_size_lim = u_state_offset;
		else
			flash_size_lim = m->size;
	}

	/* Fill known fields */
#ifdef HAS_PART_ROM_HDR
	parts[PART_ROM_HDR].size = parts_size_default_get(PART_ROM_HDR, m);
#endif
#ifdef HAS_PART_PRELOADER
	parts[PART_PRELOADER].size = parts_size_default_get(PART_PRELOADER, m);
#endif
#ifdef HAS_PART_ATF
	parts[PART_ATF].size = parts_size_default_get(PART_ATF, m);
#endif

#if defined(CONFIG_MACH_MT7622)
	parts[PART_PRELOADER].offset = parts_offset_end(PART_ROM_HDR);
	parts[PART_ATF].offset = parts_offset_end(PART_PRELOADER);

	offs_uboot = parts_offset_end(PART_ATF);
#endif

#if defined(CONFIG_MACH_MT7981) || \
    defined(CONFIG_MACH_MT7986) || \
    defined(CONFIG_MACH_MT7987) || \
    defined(CONFIG_MACH_MT7988)
	offs_uboot = parts_offset_end(PART_PRELOADER);
#endif

	/* early fill partition info for NAND */
	parts[PART_U_BOOT].offset = offs_uboot;
	parts[PART_U_BOOT].size = parts_size_default_get(PART_U_BOOT, m);

	/* early fill dump partition (end of image) */
	if (use_dump) {
		parts[PART_DUMP].skip = false;
		parts[PART_DUMP].offset = flash_size_lim -
					  CONFIG_MTD_NDM_DUMP_SIZE;
		parts[PART_DUMP].size = CONFIG_MTD_NDM_DUMP_SIZE;
	}

	if (u_state_offset != NDMPART_DI_NO_OFFSET) {
		const int ret = ndmpart_di_init(m, flash_size_lim);

		if (ret < 0)
			return ret;

		/* early fill partition info for NAND */
		parts[PART_U_STATE].offset = u_state_offset;
		parts[PART_U_STATE].size = parts_size_default_get(PART_U_STATE, m);
		parts[PART_U_STATE].skip = false;

		di_current = (u8)ret;
	}

	/* Fill known fields */
	parts[PART_U_BOOT].offset = offs_uboot;
	parts[PART_U_BOOT].size = parts_size_default_get(PART_U_BOOT, m);

	parts[PART_U_CONFIG].offset = parts_offset_end(PART_U_BOOT);
	parts[PART_U_CONFIG].size = parts_size_default_get(PART_U_CONFIG, m);

	parts[PART_RF_EEPROM].offset = parts_offset_end(PART_U_CONFIG);
	parts[PART_RF_EEPROM].size = parts_size_default_get(PART_RF_EEPROM, m);

#ifdef PLACE_OS_ON_MTD
	parts[PART_KERNEL_1].offset = parts_offset_end(PART_RF_EEPROM);
	parts[PART_FIRMWARE_1].offset = parts[PART_KERNEL_1].offset;

	if (CONFIG_MTD_NDM_CONFIG_SIZE)
		parts[PART_CONFIG_1].size = CONFIG_MTD_NDM_CONFIG_SIZE;
	else
		parts[PART_CONFIG_1].size = parts_size_default_get(PART_CONFIG_1, m);

	if (use_storage) {
		parts[PART_STORAGE].skip = false;
		parts[PART_STORAGE].size = CONFIG_MTD_NDM_STORAGE_SIZE;
	}

	/* Calculate & fill unknown fields */
	if (use_dump && !use_storage) {
		parts[PART_CONFIG_1].offset = parts[PART_DUMP].offset -
					      parts[PART_CONFIG_1].size;
	} else if (!use_dump && use_storage) {
		parts[PART_STORAGE].offset = flash_size_lim -
					     parts[PART_STORAGE].size;
		parts[PART_CONFIG_1].offset = parts[PART_STORAGE].offset -
					      parts[PART_CONFIG_1].size;
	} else if (use_dump && use_storage) {
		parts[PART_STORAGE].offset = parts[PART_DUMP].offset -
					     parts[PART_STORAGE].size;
		parts[PART_CONFIG_1].offset = parts[PART_STORAGE].offset -
					      parts[PART_CONFIG_1].size;
	} else {
		parts[PART_CONFIG_1].offset = flash_size_lim -
					      parts[PART_CONFIG_1].size;
	}

#ifdef CONFIG_MTD_NDM_EXTENDED_STORAGE
	if (m->size > flash_size_lim) {
		parts[PART_STORAGE_A].offset = flash_size_lim;
		parts[PART_STORAGE_A].size = m->size - flash_size_lim;
		parts[PART_STORAGE_A].skip = false;
	}
#endif

	parts[PART_FIRMWARE_1].size = parts[PART_CONFIG_1].offset -
				      parts[PART_FIRMWARE_1].offset;

	if (di_current == NDMPART_DI_IMAGE_FIRST) {
		uint32_t s_beg, s_size;

		s_beg = parts[PART_FIRMWARE_1].offset;
		s_size = parts[PART_FIRMWARE_1].size;

		/* early fill partition info for NAND */
		parts[PART_ROOTFS_1].offset = s_beg;
		parts[PART_ROOTFS_1].size = s_size;
		parts[PART_ROOTFS_1].skip = false;

		off = part_rootfs_offset(m, s_beg, s_size);
		if (off) {
			/* recalc KERNEL & ROOTFS partitions */
			parts[PART_ROOTFS_1].offset = off;
			parts[PART_ROOTFS_1].size = parts[PART_CONFIG_1].offset -
						    parts[PART_ROOTFS_1].offset;

			parts[PART_KERNEL_1].skip = false;
			parts[PART_KERNEL_1].size = off - parts[PART_KERNEL_1].offset;

			__ndmpart_rootfs_name = &parts[PART_ROOTFS_1].name;
		} else {
			/* hide ROOTFS partition (error path) */
			parts[PART_ROOTFS_1].skip = true;
		}
	}
#else
	/* PART_CONFIG_1 will be removed */
	parts[PART_CONFIG_1].size = parts_size_default_get(PART_CONFIG_1, m);

	if (use_dump)
		parts[PART_CONFIG_1].offset = parts[PART_DUMP].offset -
					      parts[PART_CONFIG_1].size;
	else
		parts[PART_CONFIG_1].offset = flash_size_lim -
					      parts[PART_CONFIG_1].size;
#endif

	if (u_state_offset != NDMPART_DI_NO_OFFSET) {
		/* U_STATE parts already calculated at this place */
		parts[PART_RESERVE].offset = parts_offset_end(PART_U_STATE);
		parts[PART_RESERVE].size = parts_size_default_get(PART_RESERVE, m);

		parts[PART_U_CONFIG_RES].skip = false;
		parts[PART_U_CONFIG_RES].offset = parts_offset_end(PART_RESERVE);
		parts[PART_U_CONFIG_RES].size = parts[PART_U_CONFIG].size;

		parts[PART_RF_EEPROM_RES].skip = false;
		parts[PART_RF_EEPROM_RES].offset = parts_offset_end(PART_U_CONFIG_RES);
		parts[PART_RF_EEPROM_RES].size = parts[PART_RF_EEPROM].size;

		parts[PART_CONFIG_1].name = "Config_1";
		parts[PART_CONFIG_2].skip = false;
		parts[PART_CONFIG_2].offset = u_state_offset + parts[PART_CONFIG_1].offset;
		parts[PART_CONFIG_2].size = parts[PART_CONFIG_1].size;

#ifdef PLACE_OS_ON_MTD
		parts[PART_FIRMWARE_1].name = "Firmware_1";
		parts[PART_KERNEL_1].name = "Kernel_1";
		parts[PART_ROOTFS_1].name = "RootFS_1";

		parts[PART_FIRMWARE_2].skip = false;
		parts[PART_FIRMWARE_2].offset = parts_offset_end(PART_RF_EEPROM_RES);
		parts[PART_FIRMWARE_2].size = parts[PART_FIRMWARE_1].size;

		/* check PART_STORAGE_A fit to half size */
		if (!parts[PART_STORAGE_A].skip &&
		    u_state_offset < (parts[PART_STORAGE_A].offset + m->erasesize))
			parts[PART_STORAGE_A].skip = true;

		if (!parts[PART_STORAGE_A].skip) {
			parts[PART_STORAGE_A].size =
				u_state_offset - parts[PART_STORAGE_A].offset;
			parts[PART_STORAGE_B].offset =
				parts[PART_CONFIG_2].offset +
				parts[PART_CONFIG_2].size;

			if (m->size > parts[PART_STORAGE_B].offset) {
				parts[PART_STORAGE_B].size = m->size -
					parts[PART_STORAGE_B].offset;
				parts[PART_STORAGE_B].skip = false;
			}
		}

		if (di_current == NDMPART_DI_IMAGE_SECOND) {
			uint32_t s_beg, s_size;

			s_beg = parts[PART_FIRMWARE_2].offset;
			s_size = parts[PART_FIRMWARE_2].size;

			/* early fill partition info for NAND */
			parts[PART_ROOTFS_2].offset = s_beg;
			parts[PART_ROOTFS_2].size = s_size;
			parts[PART_ROOTFS_2].skip = false;

			off = part_rootfs_offset(m, s_beg, s_size);
			if (off) {
				/* recalc KERNEL & ROOTFS partitions */
				parts[PART_ROOTFS_2].offset = off;
				parts[PART_ROOTFS_2].size = parts_offset_end(PART_FIRMWARE_2) -
							    off;

				parts[PART_KERNEL_2].skip = false;
				parts[PART_KERNEL_2].offset = parts[PART_FIRMWARE_2].offset;
				parts[PART_KERNEL_2].size = off - parts[PART_KERNEL_2].offset;

				__ndmpart_rootfs_name = &parts[PART_ROOTFS_2].name;
			} else {
				/* hide ROOTFS partition (error path) */
				parts[PART_ROOTFS_2].skip = true;
			}
		}
#endif
	}

#ifdef PLACE_OS_ON_MTD
	if (!parts[PART_STORAGE_A].skip)
		parts[PART_STORAGE].name = PART_STORAGE_LEGACY;
#endif

	for (i = 0; i < PART_MAX; i++) {
		if (!parts[i].skip)
			parts_nr++;
	}

	mp = kzalloc(parts_nr * sizeof(*mp), GFP_KERNEL);
	if (!mp) {
		ndmpart_di_fini();
		return -ENOMEM;
	}

	for (i = j = 0; i < PART_MAX; i++) {
		const struct part_dsc *d = &parts[i];
		struct mtd_partition *p;

		if (d->skip)
			continue;

		p = &mp[j];
		p->name = (char *)d->name;
		p->offset = d->offset;
		p->size = d->size;

		if (d->read_only)
			p->mask_flags = MTD_WRITEABLE;

		j++;
	}

	ndmpart_upd(m, mp, parts_nr);

	*pparts = mp;
	return parts_nr;
}

#ifdef NAND_BB_MODE_SKIP
int get_partition_range(int blk, uint32_t bshift,
			int *blk_start, int *blk_end)
{
	int i, blk_end_last;
	uint64_t offs, size;

	for (i = 0; i < PART_MAX; i++) {
		if (parts[i].skip)
			continue;

		offs = parts[i].offset;
		size = parts[i].size;

		/* skip early not filled partitions */
		if (size == 0)
			continue;

		/* skip partition with full flash */
		if (size == MTDPART_SIZ_FULL)
			continue;

		*blk_start = (int)(offs >> bshift);
		blk_end_last = *blk_start + (int)(size >> bshift);

		if (blk_end_last > *blk_start)
			*blk_end = blk_end_last - 1;
		else
			*blk_end = blk_end_last;

		if (blk >= *blk_start && blk <= *blk_end) {
			/* pick-up merged partition */
			switch (i) {
			case PART_KERNEL_1:
			case PART_ROOTFS_1:
				offs = parts[PART_FIRMWARE_1].offset;
				size = parts[PART_FIRMWARE_1].size;
				*blk_start = (int)(offs >> bshift);
				*blk_end = *blk_start + (int)(size >> bshift) - 1;
				break;

			case PART_KERNEL_2:
			case PART_ROOTFS_2:
				offs = parts[PART_FIRMWARE_2].offset;
				size = parts[PART_FIRMWARE_2].size;
				*blk_start = (int)(offs >> bshift);
				*blk_end = *blk_start + (int)(size >> bshift) - 1;
				break;
			}

			return 0;
		}
	}

	return -ENOENT;
}

bool is_nobbm_partition(uint64_t offs)
{
#ifdef PLACE_OS_ON_MTD
	size_t i;
	static const enum part NOBBM_PARTS[] = {
		PART_STORAGE,
		PART_STORAGE_A,
		PART_STORAGE_B
	};

	/* disable bad block management for given partitions */
	for (i = 0; i < ARRAY_SIZE(NOBBM_PARTS); i++) {
		const struct part_dsc *d = &parts[NOBBM_PARTS[i]];

		if (!d->skip &&
		    offs >= d->offset && offs < (d->offset + d->size))
			return true;
	}
#endif

	return false;
}
#endif

bool is_mtd_partition_rootfs(struct mtd_info *mtd)
{
	if (!__ndmpart_rootfs_name)
		return false;

	return !strcmp(mtd->name, *__ndmpart_rootfs_name);
}

static struct mtd_part_parser ndm_parser = {
	.parse_fn	= create_mtd_partitions,
	.name		= "ndmpart"
};

static int __init ndm_parser_init(void)
{
	pr_info("registering the NDM partition parser...\n");

	return register_mtd_parser(&ndm_parser);
}

static void __exit ndm_parser_exit(void)
{
	deregister_mtd_parser(&ndm_parser);
}

module_init(ndm_parser_init);
module_exit(ndm_parser_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alexander Papenko <ap@ndmsystems.com>, "
		"Eugene Yudin <e.yudin@ndmsystems.com>");
MODULE_DESCRIPTION("MTD partitioning for NDM devices");
