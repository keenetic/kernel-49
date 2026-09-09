#ifndef __NDMPART_DI_H__
#define __NDMPART_DI_H__

#include <linux/types.h>
#include <linux/errno.h>

#define NDMPART_DI_NO_OFFSET			  (0)

#define NDMPART_DI_IMAGE_FIRST			  (1)
#define NDMPART_DI_IMAGE_SECOND			  (2)

#ifdef CONFIG_MTD_NDM_DUAL_IMAGE

u32 ndmpart_di_u_state_offset(struct mtd_info *mtd);
int ndmpart_di_init(struct mtd_info *mtd, u32 offset, u32 flash_size_limit);
void ndmpart_di_fini(void);

#else /* CONFIG_MTD_NDM_DUAL_IMAGE */

static inline u32 ndmpart_di_u_state_offset(struct mtd_info *mtd)
{
	return NDMPART_DI_NO_OFFSET;
}

static inline int ndmpart_di_init(struct mtd_info *mtd, u32 offset,
				  u32 flash_size_limit)
{
	return -EOPNOTSUP;
}

static inline void ndmpart_di_fini(struct ndmpart_di **di) {}

#endif /* CONFIG_MTD_NDM_DUAL_IMAGE */

#endif /* __NDMPART_DI_H__ */
