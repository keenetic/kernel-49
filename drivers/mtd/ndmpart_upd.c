#define pr_fmt(fmt)				  "upd: " fmt

#include "ndmpart_upd.h"

#ifdef MTD_NDM_PARTITION_UPDATE

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/math64.h>
#include <linux/workqueue.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#define NDMPART_UPD_RETRIES			  (3)

#include "ndmpart.h"

#ifdef CONFIG_MTD_NDM_PRELOADER_UPDATE
#include "ndm_preloader.h"
#endif

#ifdef CONFIG_MTD_NDM_ATF_UPDATE
#include "ndm_atf.h"
#endif

#ifdef CONFIG_MTD_NDM_BOOT_UPDATE
#include "ndm_boot.h"
#endif

struct ndmpart_upd {
	const char		*name;
	const u8		*image;
	const unsigned int	*image_len;
};

static const struct ndmpart_upd NDMPART_UPD[] = {
#ifdef CONFIG_MTD_NDM_PRELOADER_UPDATE
	{
		.name		= NDMPART_NAME_PRELOADER,
		.image		= preloader_bin,
		.image_len	= &preloader_bin_len
	},
#endif
#ifdef CONFIG_MTD_NDM_ATF_UPDATE
	{
		.name		= NDMPART_NAME_ATF,
		.image		= atf_bin,
		.image_len	= &atf_bin_len
	},
#endif
#ifdef CONFIG_MTD_NDM_BOOT_UPDATE
	{
		.name		= NDMPART_NAME_U_BOOT,
		.image		= boot_bin,
		.image_len	= &boot_bin_len
	},
#endif
};

static int __ndmpart_upd_read(struct mtd_info *mtd, loff_t from, size_t len,
			      u8 *buf)
{
	size_t n = 0;

	while (n < len) {
		size_t retlen = 0;
		const int ret = mtd_read(mtd, from + (loff_t)n, len - n,
					 &retlen, buf + n);

		if (ret)
			return (ret > 0) ? -EIO : ret;

		n += retlen;
	}

	return 0;
}

static int __ndmpart_upd_write(struct mtd_info *mtd, loff_t to, size_t len,
			       const u8 *out, u8 *verify)
{
	size_t n;
	int ret = 0;
	u32 rem;

	div_u64_rem(to, mtd->erasesize, &rem);

	if (rem != 0) {
		pr_err("unable to write %zu bytes at unaligned 0x%012llx\n",
		       len, (unsigned long long)to);
		return -EINVAL;
	}

	if (len > mtd->erasesize) {
		pr_err("unable to write %zu bytes at 0x%012llx\n",
		       len, (unsigned long long)to);
		return -EINVAL;
	}

	for (n = 0; n < NDMPART_UPD_RETRIES; n++) {
		struct erase_info ei = {
			.mtd  = mtd,
			.addr = to,
			.len  = mtd->erasesize
		};
		size_t retlen = 0;

		ret = mtd_erase(mtd, &ei);
		if (ret) {
			pr_err("erase retry #%zu failed at 0x%012llx\n",
			       n, (unsigned long long)to);
			continue;
		}

		ret = mtd_write(mtd, to, len, &retlen, out);
		if (ret) {
			pr_err("write retry #%zu failed at 0x%012llx\n",
			       n, (unsigned long long)to);
			continue;
		}

		if (len != retlen) {
			pr_err("short write (%zu / %zu) "
			       "on retry #%zu at 0x%012llx\n",
			       retlen, len, n, (unsigned long long)to);
			ret = -EIO;
			continue;
		}

		ret = __ndmpart_upd_read(mtd, to, len, verify);
		if (ret) {
			pr_err("read failed on retry #%zu at 0x%012llx\n",
			       n, (unsigned long long)to);
			continue;
		}

		if (memcmp(out, verify, len)) {
			pr_err("data verification failed "
			       "on retry #%zu at 0x%012llx\n",
			       n, (unsigned long long)to);
			ret = -EIO;
			continue;
		}

		return 0;
	}

	pr_err("MTD write failed at 0x%012llx\n",
	       (unsigned long long)to);

	return ret;
}

static inline int __ndmpart_upd(struct mtd_info *mtd,
				const struct ndmpart_upd *p,
				u64 part_offs, u64 part_size)
{
	int res = 0, ret;
	u32 rem;
	u64 offs, data_size, aligned_size, image_offs = 0;
	u8 *v = NULL, *m = NULL;

#if defined(CONFIG_MACH_MT7981) || \
    defined(CONFIG_MACH_MT7986) || \
    defined(CONFIG_MACH_MT7987) || \
    defined(CONFIG_MACH_MT7988)
#ifdef CONFIG_MTD_NDM_PRELOADER_UPDATE
	if (!strcmp(p->name, NDMPART_NAME_PRELOADER)) {
		/* ROM header for NAND: 2048[4096], for NOR: 1536 */
		if (mtd->type == MTD_NORFLASH)
			image_offs = ROM_HDR_SIZE_NOR;
		else
			image_offs = mtd->writesize;
	}
#endif
#endif

	if (image_offs > mtd->erasesize) {
		pr_err("invalid image offset for \"%s\" partition (%llu)\n",
		       p->name, (unsigned long long)image_offs);
		res = -EINVAL;
		goto out;
	}

	div_u64_rem(part_offs, mtd->erasesize, &rem);

	if (rem != 0) {
		pr_err("\"%s\" partition has invalid offset (%012llx)\n",
		       p->name, (unsigned long long)part_offs);
		res = -EINVAL;
		goto out;
	}

	div_u64_rem(part_size, mtd->erasesize, &rem);

	if (rem != 0) {
		pr_err("\"%s\" partition has wrong size "
		       "alignment (%012llx / %08x)\n",
		       p->name, (unsigned long long )part_size,
		       mtd->erasesize);
		res = -EINVAL;
		goto out;
	}

	data_size = image_offs + *p->image_len;
	aligned_size = ALIGN(data_size, mtd->erasesize);

	if (aligned_size > part_size) {
		pr_err("the image for \"%s\" partition "
		       "is too big (%llu > %llu)\n",
		       p->name, aligned_size, part_size);
		res = -EFBIG;
		goto out;
	}

	v = kmalloc(mtd->erasesize, GFP_KERNEL);
	if (!v) {
		res = -ENOMEM;
		goto out;
	}

	m = kmalloc(part_size, GFP_KERNEL);
	if (!m) {
		res = -ENOMEM;
		goto out;
	}

	/* read partition content limited to a new data size */
	ret = __ndmpart_upd_read(mtd, part_offs, (size_t)data_size, m);
	if (ret) {
		pr_err("failed to read \"%s\" partition content\n", p->name);
		res = -EACCES;
		goto out;
	}

	/* compare actual partition content with the new one */
	if (!memcmp(p->image, m + image_offs, *p->image_len)) {
		pr_info("\"%s\" partition is up to date\n", p->name);
		res = -EEXIST;
		goto out;
	}

	pr_info("updating \"%s\" partition content...\n", p->name);

	/* fill the main buffer with new partition content */
	memcpy(m + image_offs, p->image, *p->image_len);

	/* fill padding */
	if (aligned_size > data_size)
		memset(m + data_size, 0xff, aligned_size - data_size);

	for (offs = 0; offs < aligned_size; offs += mtd->erasesize) {
		ret = __ndmpart_upd_write(mtd, offs + part_offs,
					  mtd->erasesize, m + offs, v);
		if (ret) {
			if (ret < 0)
				res = ret;
			else
				res = -EIO;
			break;
		}
	}

	if (offs != aligned_size)
		pr_crit("\"%s\" partition update failed, "
			"the device may become unbootable\n", p->name);
	else
		pr_info("\"%s\" partition update complete\n", p->name);

out:
	kfree(v);
	kfree(m);

	return res;
}

static void do_restart(struct work_struct *work)
{
	kernel_restart(NULL);
}

static DECLARE_WORK(__ndmpart_upd_restart, do_restart);

void ndmpart_upd(struct mtd_info *mtd, const struct mtd_partition *parts,
		 int parts_nr)
{
	bool need_reboot = false;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(NDMPART_UPD); i++) {
		const struct ndmpart_upd *n = &NDMPART_UPD[i];
		size_t j;

		for (j = 0; j < parts_nr; j++) {
			const struct mtd_partition *p = &parts[j];
			int ret;

			if (strcmp(n->name, p->name))
				continue;

			ret = __ndmpart_upd(mtd, n, p->offset, p->size);
			if (!ret)
				need_reboot = true;
			else if (ret == -EIO) {
				pr_crit("fatal error while updating \"%s\""
					"partition, trying to boot...\n",
					p->name);
				need_reboot = false;
				break;
			}
		}
	}

	if (need_reboot) {
		pr_info("update is done, rebooting...\n");
		queue_work(system_unbound_wq, &__ndmpart_upd_restart);
	}
}

#endif /* MTD_NDM_PARTITION_UPDATE */
