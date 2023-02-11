#ifndef __NDMPART_H__
#define __NDMPART_H__

#if defined(CONFIG_FIRST_IF_MT7915) || \
    defined(CONFIG_SECOND_IF_MT7915)
/* MT7915 AX boards */
#define PART_RF_EEPROM_AX_BOARD
#endif

#if defined(CONFIG_MACH_MT7622)
/* Partitions size hardcoded in MTK uboot, mt7622_evb.h */
#define PART_BL2_SIZE_NOR		0x040000
#define PART_BL2_SIZE_NAND		0x080000

#define PART_ATF_SIZE_NOR		0x020000
#define PART_ATF_SIZE_NAND		0x040000

#define PART_BOOT_SIZE_NOR		0x040000
#define PART_BOOT_SIZE_NAND		0x080000

#define PART_ENV_SIZE_NOR		0x020000
#define PART_ENV_SIZE_NAND		0x080000

#ifdef PART_RF_EEPROM_AX_BOARD
#define PART_E2P_SIZE_NOR		0x080000
#define PART_E2P_SIZE_NAND		0x100000
#else
#define PART_E2P_SIZE_NOR		0x020000
#define PART_E2P_SIZE_NAND		0x040000

#define PART_RSV_SIZE_NOR		(PART_BL2_SIZE_NOR + \
					 PART_ATF_SIZE_NOR + \
					 PART_BOOT_SIZE_NOR)
#define PART_RSV_SIZE_NAND		(PART_BL2_SIZE_NAND + \
					 PART_ATF_SIZE_NAND + \
					 PART_BOOT_SIZE_NAND)
#endif
#elif defined(CONFIG_MACH_MT7981) || \
      defined(CONFIG_MACH_MT7986)
#define PART_HDR_SIZE			0x000800

#define PART_BL2_SIZE_NOR		0x040000
#define PART_BL2_SIZE_NAND		0x080000

#define PART_FIP_SIZE_NOR		0x0a0000
#define PART_FIP_SIZE_NAND		0x200000

#define PART_ENV_SIZE_NOR		0x020000
#define PART_ENV_SIZE_NAND		0x080000

#define PART_E2P_SIZE_NOR		0x100000
#define PART_E2P_SIZE_NAND		0x200000

#define PART_RSV_SIZE_NOR		(PART_BL2_SIZE_NOR + \
					 PART_FIP_SIZE_NOR)
#define PART_RSV_SIZE_NAND		(PART_BL2_SIZE_NAND + \
					 PART_FIP_SIZE_NAND)
#endif

#if defined(CONFIG_MTD_NAND_MT7621)
#define NAND_BB_MODE_SKIP
int get_partition_range(int blk, uint32_t bshift, int *blk_start, int *blk_end);
#endif

bool is_nobbm_partition(uint64_t offs);

bool is_mtd_partition_rootfs(struct mtd_info *mtd);

#endif /* __NDMPART_H__ */
