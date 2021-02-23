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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/bootmem.h>
#include <linux/magic.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#ifdef CONFIG_MTD_NDM_BOOT_UPDATE
#include <linux/crc32.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include "ndm_boot.h"
#endif

#ifdef CONFIG_MTD_NDM_DUAL_IMAGE
#include <asm/uaccess.h>
#ifdef CONFIG_MIPS
#include <prom.h>
#else
#include <linux/libfdt.h>
#include <linux/of_fdt.h>
#endif
#endif /* CONFIG_MTD_NDM_DUAL_IMAGE */

#include "ndmpart.h"

#define DI_U_STATE_MAGIC		0x55535441	/* "USTA" */
#define DI_U_STATE_VERSION		1

#define DI_BOOT_ACTIVE			"boot_active"
#define DI_BOOT_BACKUP			"boot_backup"
#define DI_BOOT_FAILS			"boot_fails"

#define DI_IMAGE_FIRST			1
#define DI_IMAGE_SECOND			2

#ifndef SQUASHFS_MAGIC
#define SQUASHFS_MAGIC			0x73717368
#endif

#define NDMS_MAGIC			0x736D646E
#define CONFIG_MAGIC			cpu_to_be32(0x2e6e646d)
#define CONFIG_MAGIC_V1			cpu_to_be32(0x1f8b0801)

#define KERNEL_MAGIC			be32_to_cpu(0x27051956)
#define ROOTFS_MAGIC			SQUASHFS_MAGIC
#define PART_OPTIONAL_NUM		2		/* storage and dump */

#define MIN_FLASH_SIZE_FOR_STORAGE	0x800000	/* 8 MB */
#define MTD_MAX_RETRIES			3

#define PART_SIZE_UNKNOWN		(~0)

#if defined(CONFIG_FIRST_IF_MT7915) || \
    defined(CONFIG_SECOND_IF_MT7915)
/* MT7915 AX boards */
#define PART_RF_EEPROM_AX_BOARD
#endif

#define PART_STORAGE_LEGACY		"Storage_Legacy"

enum part {
	/* Image 1 */
#if defined(CONFIG_MACH_MT7622)
	PART_PRELOADER,
	PART_ATF,		/* ARM Trusted Firmware */
#endif
	PART_U_BOOT,
	PART_U_CONFIG,
	PART_RF_EEPROM,
	PART_KERNEL_1,
	PART_ROOTFS_1,
	PART_FIRMWARE_1,
	PART_CONFIG_1,
	PART_STORAGE,		/* optional */
	PART_DUMP,		/* optional */
	PART_STORAGE_1,		/* optional */
	/* Image 2 */
	PART_U_STATE,
	PART_RESERVE,
	PART_U_CONFIG_RES,
	PART_RF_EEPROM_RES,
	PART_KERNEL_2,		/* optional */
	PART_ROOTFS_2,		/* optional */
	PART_FIRMWARE_2,	/* optional */
	PART_CONFIG_2,		/* optional */
	PART_STORAGE_2,		/* optional */
	/* Pseudo */
	PART_FULL,
	PART_MAX
};

struct di_u_state {
	uint32_t magic;
	uint8_t version;
	uint8_t boot_active;
	uint8_t boot_backup;
	uint8_t boot_fails;
};

struct part_dsc {
	const char *name;
	uint32_t offset;
	uint32_t size;
	bool skip;
	bool read_only;
};

#ifdef CONFIG_MTD_NDM_DUAL_IMAGE
static bool di_is_enabled(void);
static bool u_state_is_pending(void);

static int u_state_init(struct mtd_info *master, uint32_t off, uint32_t size);
static int u_state_get(const char *name, int *val);
static int u_state_set(const char *name, int val);
static int u_state_commit(void);
#endif

#ifdef CONFIG_MTD_NDM_DUAL_IMAGE
static int ndmpart_image_cur = 1;
static bool ndmpart_di_is_enabled;
#endif

static struct part_dsc parts[PART_MAX] = {
	/* Image 1 */
#if defined(CONFIG_MACH_MT7622)
	[PART_PRELOADER] = {
		name: "Preloader",
		read_only: true
	},
	[PART_ATF] = {
		name: "ATF",
		read_only: true
	},
#endif
	[PART_U_BOOT] = {
		name: "U-Boot"
	},
	[PART_U_CONFIG] = {
		name: "U-Config"
	},
	[PART_RF_EEPROM] = {
		name: "RF-EEPROM"
	},
	[PART_KERNEL_1] = {
		name: "Kernel",
		skip: true,
		read_only: true
	},
	[PART_ROOTFS_1] = {
		name: "RootFS",
		skip: true,
		read_only: true
	},
	[PART_FIRMWARE_1] = {
		name: "Firmware"
	},
	[PART_CONFIG_1] = {
		name: "Config"
	},
	[PART_STORAGE] = {
		name: "Storage",
		skip: true
	},
	[PART_DUMP] = {
		name: "Dump",
		skip: true
	},
	[PART_STORAGE_1] = {
		name: "Storage_1",
		skip: true
	},
	/* Image 2 */
	[PART_U_STATE] = {
		name: "U-State",
		skip: true
	},
	[PART_RESERVE] = {
		name: "Reserve",
		skip: true
	},
	[PART_U_CONFIG_RES] = {
		name: "U-Config_res",
		skip: true
	},
	[PART_RF_EEPROM_RES] = {
		name: "RF-EEPROM_res",
		skip: true
	},
	[PART_KERNEL_2] = {
		name: "Kernel_2",
		skip: true,
		read_only: true
	},
	[PART_ROOTFS_2] = {
		name: "RootFS_2",
		skip: true,
		read_only: true
	},
	[PART_FIRMWARE_2] = {
		name: "Firmware_2",
		skip: true
	},
	[PART_CONFIG_2] = {
		name: "Config_2",
		skip: true
	},
	[PART_STORAGE_2] = {
		name: "Storage_2",
		skip: true
	},
	/* Pseudo */
	[PART_FULL] = {
		name: "Full",
		size: MTDPART_SIZ_FULL,
		read_only: true
	}
};

static inline uint32_t parts_offset_end(enum part part)
{
	return parts[part].offset + parts[part].size;
}

static unsigned parts_num(void)
{
	int i;
	unsigned num = 0;

	for (i = 0; i < PART_MAX; i++) {
		if (parts[i].skip)
			continue;
		num++;
	}

	return num;
}

static uint32_t parts_size_default_get(enum part part, struct mtd_info *master)
{
	uint32_t size = PART_SIZE_UNKNOWN;

#if defined(CONFIG_MACH_MT7622)

#ifdef PART_RF_EEPROM_AX_BOARD
/* AX boards */
#define PART_RF_EEPROM_SIZE_NOR		0x080000
#define PART_RF_EEPROM_SIZE_NAND	0x100000
#else
/* AC boards */
#define PART_RF_EEPROM_SIZE_NOR		0x020000
#define PART_RF_EEPROM_SIZE_NAND	0x040000
#endif

	/*
	 * Partitions size hardcoded in MTK uboot, see "mt7622_evb.h".
	 * We now support NOR and SLC NAND layouts.
	 * Todo: eMMC & SD layouts (with GPT) support.
	 * Todo: MLC NAND layout support.
	 */
	switch (part) {
	case PART_PRELOADER:
		size = (master->type == MTD_NORFLASH) ? 0x40000 : 0x80000;
		break;
	case PART_ATF:
		size = (master->type == MTD_NORFLASH) ? 0x20000 : 0x40000;
		break;
	case PART_U_BOOT:
		size = (master->type == MTD_NORFLASH) ? 0x40000 : 0x80000;
		break;
	case PART_U_CONFIG:
		size = (master->type == MTD_NORFLASH) ? 0x20000 : 0x80000;
		break;
	case PART_RF_EEPROM:
		if (master->type == MTD_NORFLASH)
			size = PART_RF_EEPROM_SIZE_NOR;
		else
			size = PART_RF_EEPROM_SIZE_NAND;
		break;
	case PART_CONFIG_1:
		size = master->erasesize * 4;
		break;
	case PART_U_STATE:
		size = master->erasesize;
		break;
	case PART_RESERVE:
		size = master->erasesize * 9;
		break;
	default:
		break;
	}
#else
	switch (part) {
	case PART_U_BOOT:
	case PART_U_STATE:
		if (master->type == MTD_NANDFLASH)
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
#ifdef NAND_BB_MODE_SKIP
		if (master->type == MTD_NANDFLASH)
			size = master->erasesize << 2;
		else
#endif
		size = master->erasesize;
		break;
	case PART_RESERVE:
		size = 0;
		break;
	default:
		break;
	}

#ifdef PART_RF_EEPROM_AX_BOARD
	if (part == PART_RF_EEPROM) {
		if (master->type == MTD_NANDFLASH)
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

#if defined(CONFIG_MTD_NDM_BOOT_UPDATE) || \
    defined(CONFIG_MTD_NDM_DUAL_IMAGE)
static int mtd_write_retry(struct mtd_info *mtd, loff_t to, size_t len,
			   size_t *retlen, const u_char *buf)
{
	int ret, retries = MTD_MAX_RETRIES;

	do {
		ret = mtd_write(mtd, to, len, retlen, buf);
		if (ret) {
			printk("%s: write%s failed at 0x%012llx\n", __func__,
				"", (unsigned long long) to);
		}

		if (len != *retlen) {
			printk("%s: short write at 0x%012llx\n", __func__,
				(unsigned long long) to);
			ret = -EIO;
		}
	} while (ret && --retries);

	if (ret) {
		printk("%s: write%s failed at 0x%012llx\n", __func__,
			" completely", (unsigned long long) to);
	}

	return ret;
}

static int mtd_erase_retry(struct mtd_info *mtd, struct erase_info *instr)
{
	int ret, retries = MTD_MAX_RETRIES;

	do {
		ret = mtd_erase(mtd, instr);
		if (ret) {
			printk("%s: erase%s failed at 0x%012llx\n", __func__,
				"", (unsigned long long) instr->addr);
		}
	} while (ret && --retries);

	if (ret) {
		printk("%s: erase%s failed at 0x%012llx\n", __func__,
			" completely", (unsigned long long) instr->addr);
	}

	return ret;
}
#endif

#ifdef CONFIG_MTD_NDM_BOOT_UPDATE
static void do_restart(struct work_struct *work)
{
	kernel_restart(NULL);
}

static DECLARE_WORK(restart_work, do_restart);

static int ndm_flash_boot(struct mtd_info *master,
			  uint32_t p_off, uint32_t p_size)
{
	bool update_need;
	int res = -1, ret, retries;
	size_t len;
	uint32_t off, size, es, ws;
	uint32_t dst_crc = 0, src_crc = 0;
	unsigned char *m, *v;

	es = master->erasesize;

	/* Check bootloader size */
	if (boot_bin_len > p_size) {
		printk(KERN_ERR "too big bootloader\n");
		goto out;
	}

	/* Read current bootloader */
	m = kmalloc(p_size, GFP_KERNEL);
	if (m == NULL)
		goto out;

	/* Alloc verify buffer */
	v = kmalloc(es, GFP_KERNEL);
	if (v == NULL)
		goto out_kfree;

	ret = mtd_read(master, p_off, p_size, &len, m);
	if (ret || len != p_size) {
		printk(KERN_ERR "read failed");
		goto out_kfree;
	}

	/* Detect and compare version */
	len = strlen(NDM_BOOT_VERSION);
	size = p_size;
	update_need = true;

	for (off = 0; off < size; off++) {
		if (size - off < len)
			break;
		else if (!memcmp(NDM_BOOT_VERSION, m + off, len)) {
			update_need = false;
			break;
		}
	}

	if (!update_need) {
		printk(KERN_INFO "Bootloader is up to date\n");
		res = 0;
		goto out_kfree;
	}

	printk(KERN_INFO "Updating bootloader...\n");

	/* Flash bootloader */
	memcpy(m, boot_bin, boot_bin_len);
	size = ALIGN(boot_bin_len, master->writesize);

	/* fill padding */
	if (size > boot_bin_len)
		memset(m + boot_bin_len, 0xff, size - boot_bin_len);

	/* erase & write -> verify */
	for (off = 0; off < size; off += es) {
		struct erase_info ei = {
			.mtd = master,
			.addr = off + p_off,
			.len = es
		};

		/* write size can be < erase size */
		ws = min(es, size - off);

		src_crc = crc32(0, m + off, ws);
		dst_crc = src_crc + 1;

		retries = MTD_MAX_RETRIES;
		do {
			ret = mtd_erase_retry(master, &ei);
			if (ret)
				goto out_write_fail;

			ret = mtd_write_retry(master, ei.addr, ws, &len,
					      m + off);
			if (ret)
				goto out_write_fail;

			memset(v, 0xff, ws);
			ret = mtd_read(master, ei.addr, ws, &len, v);
			if (ret == 0 && len == ws)
				dst_crc = crc32(0, v, ws);

		} while (src_crc != dst_crc && --retries);
	}

	if (src_crc == dst_crc) {
		printk("Bootloader update complete, scheduling reboot...\n");
		queue_work(system_unbound_wq, &restart_work);
		res = 0;

		goto out_kfree;
	}

out_write_fail:
	if (src_crc != dst_crc)
		printk(KERN_ERR "Bootloader update FAILED!"
			" Device may be bricked!\n");
out_kfree:
	kfree(v);
	kfree(m);
out:
	return res;
}
#endif /* CONFIG_MTD_NDM_BOOT_UPDATE */

#ifdef CONFIG_MTD_NDM_DUAL_IMAGE
static inline uint32_t part_u_state_offset_(struct mtd_info *master)
{
	uint32_t off = master->size >> 1;

	/* offset must be aligned */
	if (is_power_of_2(master->erasesize))
		off &= ~(master->erasesize - 1);

	return off;
}

#ifdef CONFIG_MTD_NDM_U_STATE_SEARCH
static bool is_u_state(struct mtd_info *master, uint32_t off)
{
	size_t len;
	uint32_t magic = 0;

	if (mtd_read(master, off, sizeof(magic), &len, (uint8_t *)&magic))
		return false;

	return ntohl(magic) == DI_U_STATE_MAGIC;
}

static uint32_t part_u_state_offset(struct mtd_info *master)
{
	uint32_t es, off, start, end;

	es = master->erasesize;
	start = CONFIG_MTD_NDM_U_STATE_SEARCH_START_BLOCK * es;
	end = CONFIG_MTD_NDM_U_STATE_SEARCH_END_BLOCK * es;

	if (start < end) {
		for (off = start; off <= end; off += es) {
			if (is_u_state(master, off))
				return off;
		}
	} else {
		for (off = start; off >= end; off -= es) {
			if (is_u_state(master, off))
				return off;
		}
	}

	return part_u_state_offset_(master);
}
#else /* CONFIG_MTD_NDM_U_STATE_SEARCH */
static uint32_t part_u_state_offset(struct mtd_info *master)
{
	return part_u_state_offset_(master);
}
#endif /* CONFIG_MTD_NDM_U_STATE_SEARCH */
#endif /* CONFIG_MTD_NDM_DUAL_IMAGE */

static uint32_t part_rootfs_offset(struct mtd_info *master,
				   uint32_t begin, uint32_t size)
{
	size_t len;
	uint32_t off, magic, kernel_min_size;

	/* kernel min size ~1MB */
	if (master->type == MTD_NORFLASH)
		kernel_min_size = master->erasesize * 10;	/* 64K * 10 */
	else
		kernel_min_size = master->erasesize * 5;	/* 128K * 5 */

	/* Skip kernel first blocks to speedup */
	for (off = begin + kernel_min_size;
	     off < begin + size;
	     off += master->erasesize) {
		if (mtd_read(master, off, sizeof(magic), &len,
			     (uint8_t *)&magic))
			continue;

		if (le32_to_cpu(magic) == ROOTFS_MAGIC ||
		    le32_to_cpu(magic) == NDMS_MAGIC)
			return off;
	}

	return 0;
}

static inline int di_image_num_pair_get(int n)
{
	return (n == 1) ? 2 : 1;
}

static int create_mtd_partitions(struct mtd_info *m,
				 const struct mtd_partition **pparts,
				 struct mtd_part_parser_data *data)
{
	bool use_dump, use_storage;
	int i, j;
	uint32_t offs_uboot = 0;
#ifdef CONFIG_MTD_NDM_DUAL_IMAGE
	int boot_active = 0, boot_backup = 0;
	uint32_t off_si = 0;
#endif
	uint32_t off, flash_size_lim;
	struct mtd_partition *ndm_parts;
	unsigned ndm_parts_num;

	use_dump = use_storage = false;
	if (CONFIG_MTD_NDM_DUMP_SIZE)
		use_dump = true;
	if (CONFIG_MTD_NDM_STORAGE_SIZE)
		use_storage = true;

	flash_size_lim = CONFIG_MTD_NDM_FLASH_SIZE_LIMIT;
	if (!flash_size_lim || flash_size_lim > m->size)
		flash_size_lim = m->size;

#if defined(CONFIG_MACH_MT7622)
	/* Fill known fields */
	parts[PART_PRELOADER].size = parts_size_default_get(PART_PRELOADER, m);

	parts[PART_ATF].offset = parts_offset_end(PART_PRELOADER);
	parts[PART_ATF].size = parts_size_default_get(PART_ATF, m);

	offs_uboot = parts_offset_end(PART_ATF);
#endif

#ifdef CONFIG_MTD_NDM_BOOT_UPDATE
	/* early fill partition info for NAND */
	parts[PART_U_BOOT].offset = offs_uboot;
	parts[PART_U_BOOT].size = parts_size_default_get(PART_U_BOOT, m);

	ndm_flash_boot(m, offs_uboot, (uint32_t)parts[PART_U_BOOT].size);
#endif

#ifdef CONFIG_MTD_NDM_DUAL_IMAGE
	if (ndmpart_di_is_enabled) {
		int ret;

		off_si = part_u_state_offset(m);
		if (off_si < flash_size_lim) {
			printk(KERN_ERR "di: invalid flash size limit (0x%x)\n",
				off_si);
			return -EINVAL;
		}

		/* early fill partition info for NAND */
		parts[PART_U_STATE].offset = off_si;
		parts[PART_U_STATE].size = parts_size_default_get(PART_U_STATE, m);
		parts[PART_U_STATE].skip = false;

		ret = u_state_init(m, off_si, (uint32_t)parts[PART_U_STATE].size);
		if (ret < 0)
			return ret;

		u_state_get(DI_BOOT_ACTIVE, &boot_active);
		u_state_get(DI_BOOT_BACKUP, &boot_backup);

		if (boot_active)
			ndmpart_image_cur = boot_active;
		else
			ndmpart_image_cur = di_image_num_pair_get(boot_backup);

		printk(KERN_INFO "di: active = %d, backup = %d, current = %d\n",
				 boot_active, boot_backup, ndmpart_image_cur);
	}
#endif

	/* Fill known fields */
	parts[PART_U_BOOT].offset = offs_uboot;
	parts[PART_U_BOOT].size = parts_size_default_get(PART_U_BOOT, m);

	parts[PART_U_CONFIG].offset = parts_offset_end(PART_U_BOOT);
	parts[PART_U_CONFIG].size = parts_size_default_get(PART_U_CONFIG, m);

	parts[PART_RF_EEPROM].offset = parts_offset_end(PART_U_CONFIG);
	parts[PART_RF_EEPROM].size = parts_size_default_get(PART_RF_EEPROM, m);

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

	if (use_dump) {
		parts[PART_DUMP].skip = false;
		parts[PART_DUMP].offset = flash_size_lim -
					  CONFIG_MTD_NDM_DUMP_SIZE;
		parts[PART_DUMP].size = CONFIG_MTD_NDM_DUMP_SIZE;
	}

	parts[PART_STORAGE_1].skip = true;

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

		off = parts[PART_DUMP].offset + parts[PART_DUMP].size;
		if (m->size >= (off + m->erasesize)) {
			parts[PART_STORAGE_1].offset = off;
			parts[PART_STORAGE_1].size = m->size - off;
			parts[PART_STORAGE_1].skip = false;
		}
	} else {
		parts[PART_CONFIG_1].offset = flash_size_lim -
					      parts[PART_CONFIG_1].size;
	}

	parts[PART_FIRMWARE_1].size = parts[PART_CONFIG_1].offset -
				      parts[PART_FIRMWARE_1].offset;

#ifdef CONFIG_MTD_NDM_DUAL_IMAGE
	if (ndmpart_image_cur == DI_IMAGE_FIRST)
#endif
	{
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
		} else {
			/* hide ROOTFS partition (error path) */
			parts[PART_ROOTFS_1].skip = true;
		}
	}

	parts[PART_STORAGE_2].skip = true;

#ifdef CONFIG_MTD_NDM_DUAL_IMAGE
	if (ndmpart_di_is_enabled) {
		parts[PART_FIRMWARE_1].name = "Firmware_1";
		parts[PART_KERNEL_1].name = "Kernel_1";
		parts[PART_ROOTFS_1].name = "RootFS_1";
		parts[PART_CONFIG_1].name = "Config_1";

		/* U_STATE parts already calculated at this place */
		parts[PART_RESERVE].offset = parts_offset_end(PART_U_STATE);
		parts[PART_RESERVE].size = parts_size_default_get(PART_RESERVE, m);

		parts[PART_U_CONFIG_RES].skip = false;
		parts[PART_U_CONFIG_RES].offset = parts_offset_end(PART_RESERVE);
		parts[PART_U_CONFIG_RES].size = parts[PART_U_CONFIG].size;

		parts[PART_RF_EEPROM_RES].skip = false;
		parts[PART_RF_EEPROM_RES].offset = parts_offset_end(PART_U_CONFIG_RES);
		parts[PART_RF_EEPROM_RES].size = parts[PART_RF_EEPROM].size;

		parts[PART_FIRMWARE_2].skip = false;
		parts[PART_FIRMWARE_2].offset = parts_offset_end(PART_RF_EEPROM_RES);
		parts[PART_FIRMWARE_2].size = parts[PART_FIRMWARE_1].size;

		parts[PART_CONFIG_2].skip = false;
		parts[PART_CONFIG_2].offset = off_si + parts[PART_CONFIG_1].offset;
		parts[PART_CONFIG_2].size = parts[PART_CONFIG_1].size;

		/* check PART_STORAGE_1 fit to half size */
		if (!parts[PART_STORAGE_1].skip &&
		    off_si < (parts[PART_STORAGE_1].offset + m->erasesize))
			parts[PART_STORAGE_1].skip = true;

		if (!parts[PART_STORAGE_1].skip) {
			parts[PART_STORAGE_1].size =
				off_si - parts[PART_STORAGE_1].offset;
			parts[PART_STORAGE_2].offset =
				parts[PART_CONFIG_2].offset +
				parts[PART_CONFIG_2].size;

			if (m->size > parts[PART_STORAGE_2].offset) {
				parts[PART_STORAGE_2].size = m->size -
					parts[PART_STORAGE_2].offset;
				parts[PART_STORAGE_2].skip = false;
			}
		}

		if (ndmpart_image_cur == DI_IMAGE_SECOND) {
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
			} else {
				/* hide ROOTFS partition (error path) */
				parts[PART_ROOTFS_2].skip = true;
			}
		}
	}
#endif

	if (!parts[PART_STORAGE_1].skip)
		parts[PART_STORAGE].name = PART_STORAGE_LEGACY;

	/* Post actions */
	ndm_parts_num = parts_num();

	ndm_parts = kzalloc(sizeof(*ndm_parts) * ndm_parts_num, GFP_KERNEL);
	if (ndm_parts == NULL)
		return -ENOMEM;

	for (i = j = 0; i < PART_MAX; i++) {
		if (parts[i].skip)
			continue;

		ndm_parts[j].name = (char *)parts[i].name;
		ndm_parts[j].offset = parts[i].offset;
		ndm_parts[j].size = parts[i].size;

		if (parts[i].read_only)
			ndm_parts[j].mask_flags = MTD_WRITEABLE;

		j++;
	}

	*pparts = ndm_parts;

	return ndm_parts_num;
}

#ifdef NAND_BB_MODE_SKIP
int get_partition_range(int blk, uint32_t bshift, int *blk_start, int *blk_end)
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
#endif

bool is_nobbm_partition(uint64_t offs)
{
	int i;
	const int nobbm_parts[] = {
		PART_STORAGE,
		PART_STORAGE_1,
		PART_STORAGE_2
	};

	/* disable bad block management for given partitions */
	for (i = 0; i < ARRAY_SIZE(nobbm_parts); i++) {
		const int part_idx = nobbm_parts[i];

		if (!parts[part_idx].skip &&
		    offs >= parts[part_idx].offset &&
		    offs < (parts[part_idx].offset + parts[part_idx].size))
			return true;
	}

	return false;
}

bool is_mtd_partition_rootfs(struct mtd_info *mtd)
{
#ifdef CONFIG_MTD_NDM_DUAL_IMAGE
	if (ndmpart_image_cur == DI_IMAGE_SECOND)
		return strcmp(mtd->name, parts[PART_ROOTFS_2].name) == 0;
#endif
	return strcmp(mtd->name, parts[PART_ROOTFS_1].name) == 0;
}

#ifdef CONFIG_MTD_NDM_DUAL_IMAGE
static int show_boot(struct seq_file *s, void *v)
{
	int ret, val;
	char *name = s->private;

	ret = u_state_get(name, &val);
	if (ret < 0) {
		printk(KERN_WARNING "unknown name \"%s\"\n", name);
		return ret;
	}

	seq_printf(s, "%d\n", val);

	return 0;
}

static int show_boot_current(struct seq_file *s, void *v)
{
	seq_printf(s, "%d\n", ndmpart_image_cur);

	return 0;
}

static int boot_active_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_boot, DI_BOOT_ACTIVE);
}

static int boot_backup_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_boot, DI_BOOT_BACKUP);
}

static int boot_current_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_boot_current, NULL);
}

static int boot_fails_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_boot, DI_BOOT_FAILS);
}

static ssize_t commit_proc_write(struct file *file, const char __user *buffer,
				 size_t count, loff_t *pos)
{
	int ret;

	ret = u_state_commit();
	if (ret)
		printk(KERN_WARNING "%s: commit failed\n", __func__);

	return count;
}

static ssize_t boot_proc_write(const char __user *buffer, size_t count,
			       const char *name, bool (*is_valid)(char c))
{
	char c;

	if (is_valid == NULL)
		return -EINVAL;

	if (count > 0) {
		if (get_user(c, buffer))
			return -EFAULT;

		if (is_valid(c))
			u_state_set(name, c - '0');
	}

	return count;
}

static inline bool boot_active_is_valid(char c)
{
	return c >= '0' && c <= '2';
}

static ssize_t boot_active_proc_write(struct file *file, const char __user *buffer,
				      size_t count, loff_t *pos)
{
	return boot_proc_write(buffer, count, DI_BOOT_ACTIVE,
			       boot_active_is_valid);
}

static inline bool boot_backup_is_valid(char c)
{
	return c >= '1' && c <= '2';
}

static ssize_t boot_backup_proc_write(struct file *file, const char __user *buffer,
				      size_t count, loff_t *pos)
{
	return boot_proc_write(buffer, count, DI_BOOT_BACKUP,
			       boot_backup_is_valid);
}

static inline bool boot_fails_is_valid(char c)
{
	/* XXX: Force to '0'? */
	return c >= '0' && c <= '2';
}

static ssize_t boot_fails_proc_write(struct file *file, const char __user *buffer,
				     size_t count, loff_t *pos)
{
	return boot_proc_write(buffer, count, DI_BOOT_FAILS,
			       boot_fails_is_valid);
}

static const struct file_operations fops_boot_active = {
	.open = boot_active_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = boot_active_proc_write
};

static const struct file_operations fops_boot_backup = {
	.open = boot_backup_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = boot_backup_proc_write
};

static const struct file_operations fops_boot_current = {
	.open = boot_current_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static const struct file_operations fops_boot_fails = {
	.open = boot_fails_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = boot_fails_proc_write
};

static const struct file_operations fops_commit = {
	.write = commit_proc_write,
	.llseek = noop_llseek
};
#endif

static struct mtd_part_parser ndm_parser = {
	.parse_fn = create_mtd_partitions,
	.name = "ndmpart",
};

static int __init ndm_parser_init(void)
{
#ifdef CONFIG_MTD_NDM_DUAL_IMAGE
	struct proc_dir_entry *proc_dir, *entry;

	ndmpart_di_is_enabled = di_is_enabled();
	if (ndmpart_di_is_enabled) {
		proc_dir = proc_mkdir("dual_image", NULL);
		BUG_ON(proc_dir == NULL);

		entry = proc_create(DI_BOOT_ACTIVE, S_IRUGO | S_IWUSR, proc_dir,
			&fops_boot_active);
		BUG_ON(entry == NULL);

		entry = proc_create(DI_BOOT_BACKUP, S_IRUGO | S_IWUSR, proc_dir,
			&fops_boot_backup);
		BUG_ON(entry == NULL);

		entry = proc_create("boot_current", S_IRUGO | S_IWUSR, proc_dir,
			&fops_boot_current);
		BUG_ON(entry == NULL);

		entry = proc_create(DI_BOOT_FAILS, S_IRUGO | S_IWUSR, proc_dir,
			&fops_boot_fails);
		BUG_ON(entry == NULL);

		entry = proc_create("commit", S_IWUSR, proc_dir, &fops_commit);
		BUG_ON(entry == NULL);
	}
#endif
	printk(KERN_INFO "Registering NDM partitions parser\n");

	return register_mtd_parser(&ndm_parser);
}

static void __exit ndm_parser_exit(void)
{
	deregister_mtd_parser(&ndm_parser);
}

#ifdef CONFIG_MTD_NDM_DUAL_IMAGE
static struct di_u_state u_state;
static struct mtd_info *u_state_master;
static uint32_t u_state_offset, u_state_size;

static int u_state_init(struct mtd_info *m, uint32_t off, uint32_t size)
{
	int ret;
	size_t len;

	u_state_master = m;
	u_state_offset = off;
	u_state_size = size;

	ret = mtd_read(m, off, sizeof(u_state), &len, (void *)&u_state);
	if (ret != 0 || len != sizeof(u_state)) {
		printk("%s: read failed at 0x%012x\n", __func__, off);
		return -EIO;
	}

	if (ntohl(u_state.magic) != DI_U_STATE_MAGIC) {
		printk("%s: unknown magic %08x\n", __func__,
			ntohl(u_state.magic));
		return -EINVAL;
	}

	if (u_state.version != DI_U_STATE_VERSION) {
		printk("%s: unknown version %d\n", __func__,
			(int)u_state.version);
		return -EINVAL;
	}

	return 0;
}

static int u_state_get(const char *name, int *val)
{
	if (name == NULL || val == NULL)
		return -EINVAL;

	if (!strcmp(name, DI_BOOT_ACTIVE)) {
		*val = u_state.boot_active;
	} else if (!strcmp(name, DI_BOOT_BACKUP)) {
		*val = u_state.boot_backup;
	} else if (!strcmp(name, DI_BOOT_FAILS)) {
		*val = u_state.boot_fails;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int u_state_set(const char *name, int val)
{
	if (name == NULL)
		return -EINVAL;

	if (!strcmp(name, DI_BOOT_ACTIVE)) {
		u_state.boot_active = val;
	} else if (!strcmp(name, DI_BOOT_BACKUP)) {
		u_state.boot_backup = val;
	} else if (!strcmp(name, DI_BOOT_FAILS)) {
		u_state.boot_fails = val;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int u_state_commit(void)
{
	int res = -1, ret;
	size_t len;
	struct mtd_info *master = u_state_master;
	uint32_t off = u_state_offset;
	void *m;

	struct erase_info ei = {
		.mtd = master,
		.addr = off,
		.len = master->erasesize
	};

	if (!u_state_is_pending()) {
		res = 0;
		goto out;
	}

	m = kzalloc(master->erasesize, GFP_KERNEL);
	if (m == NULL) {
		res = -ENOMEM;
		goto out;
	}

	memcpy(m, &u_state, sizeof(u_state));

	ret = mtd_erase_retry(master, &ei);
	if (ret) {
		res = ret;
		goto out_kfree;
	}

	ret = mtd_write_retry(master, ei.addr, ei.len, &len, m);
	if (ret) {
		res = ret;
		goto out_kfree;
	}

	res = 0;

out_kfree:
	kfree(m);
out:
	return res;
}

#ifdef CONFIG_MIPS
static bool di_is_enabled(void)
{
	char *s;

	s = prom_getenv("dual_image");
	if (s && !strcmp(s, "0"))
		return false;

	return true;
}
#else
static bool di_is_enabled(void)
{
	int off;
	const unsigned char *val;
	void *fdt = initial_boot_params;

	if ((off = fdt_path_offset(fdt, "/chosen")) >= 0 &&
	    (val = fdt_getprop(fdt, off, "dualimage", NULL)) &&
	    *val)
		return true;

	return false;
}
#endif /* CONFIG_MIPS */

static bool u_state_is_pending(void)
{
	int ret;
	size_t len;
	struct di_u_state u;

	ret = mtd_read(u_state_master, u_state_offset, sizeof(u), &len, (void *)&u);
	if (!ret && len == sizeof(u) &&
	    !memcmp(&u, &u_state, sizeof(u))) {
		return false;
	}

	return true;
}
#endif

module_init(ndm_parser_init);
module_exit(ndm_parser_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alexander Papenko <ap@ndmsystems.com>, "
		"Eugene Yudin <e.yudin@ndmsystems.com>");
MODULE_DESCRIPTION("MTD partitioning for NDM devices");
