#ifndef __NDMPART_UPD_H__
#define __NDMPART_UPD_H__

struct mtd_info;
struct mtd_partition;

#if defined(CONFIG_MTD_NDM_PRELOADER_UPDATE) || \
    defined(CONFIG_MTD_NDM_ATF_UPDATE) || \
    defined(CONFIG_MTD_NDM_BOOT_UPDATE)

#define MTD_NDM_PARTITION_UPDATE

void ndmpart_upd(struct mtd_info *mtd, const struct mtd_partition *parts,
		 int parts_nr);

#else

static inline void
ndmpart_upd(struct mtd_info *mtd, const struct mtd_partition *parts,
	    int parts_nr) {}

#endif /* *_UPDATE */

#endif /* __NDMPART_UPD_H__ */
