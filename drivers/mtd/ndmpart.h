#ifndef __NDMPART_H__
#define __NDMPART_H__

#if defined(CONFIG_MTD_NAND_MT7621)
#define NAND_BB_MODE_SKIP
int get_partition_range(int blk, uint32_t bshift, int *blk_start, int *blk_end);
#endif

bool is_nobbm_partition(uint64_t offs);

bool is_mtd_partition_rootfs(struct mtd_info *mtd);

#endif /* __NDMPART_H__ */
