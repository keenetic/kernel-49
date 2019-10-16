#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/mtd/mtd.h>

static DEFINE_MUTEX(ra_mtd_mutex);

/*
 * Flash API: ra_mtd_read, ra_mtd_write
 * Arguments:
 *   - num: specific the mtd number
 *   - to/from: the offset to read from or written to
 *   - len: length
 *   - buf: data to be read/written
 * Returns:
 *   - return -errno if failed
 *   - return the number of bytes read/written if successed
 */
int ra_mtd_write_nm(char *name, loff_t to, size_t len, const u_char *buf)
{
	int ret, res;
	loff_t ofs, ofs_align, bad_shift;
	size_t cnt, i_len, r_len, w_len;
	struct mtd_info *mtd;
	struct erase_info ei;
	u_char *bak, *ptr;

	mtd = get_mtd_device_nm(name);
	if (IS_ERR(mtd))
		return PTR_ERR(mtd);

	ret = 0;

	if ((to + len) > mtd->size) {
		ret = -E2BIG;
		printk(KERN_ERR "%s: out of mtd size (%lld)!\n",
			 __func__, mtd->size);
		goto out;
	}

	bak = kzalloc(mtd->erasesize, GFP_KERNEL);
	if (!bak) {
		ret = -ENOMEM;
		goto out;
	}

	ptr = (u_char *)buf;
	ofs = to;
	cnt = len;

	bad_shift = 0;

	mutex_lock(&ra_mtd_mutex);

	while (cnt > 0) {
		ofs_align = ofs & ~(mtd->erasesize - 1);	/* aligned to erase boundary */
		i_len = mtd->erasesize - (ofs - ofs_align);
		if (cnt < i_len)
			i_len = cnt;

		ei.mtd = mtd;
		ei.callback = NULL;
		ei.addr = ofs_align + bad_shift;
		ei.len = mtd->erasesize;
		ei.priv = 0;

		/* check bad block */
		if (mtd->type == MTD_NANDFLASH || mtd->type == MTD_MLCNANDFLASH) {
			if ((ei.addr + mtd->erasesize) > mtd->size) {
				ret = -E2BIG;
				goto free_out;
			}
			res = mtd_block_isbad(mtd, ei.addr);
			if (res < 0) {
				ret = -EIO;
				goto free_out;
			}
			if (res > 0) {
				bad_shift += mtd->erasesize;
				continue;
			}
		}

		/* backup */
		r_len = 0;
		res = mtd_read(mtd, ei.addr, mtd->erasesize, &r_len, bak);
		if (res || mtd->erasesize != r_len) {
			ret = -EIO;
			printk(KERN_ERR "%s: read from 0x%llx failed!\n",
				 __func__, ei.addr);
			goto free_out;
		}

		/* erase */
		res = mtd_erase(mtd, &ei);
		if (res) {
			ret = -EIO;
			printk(KERN_ERR "%s: erase on 0x%llx failed!\n",
				 __func__, ei.addr);
			goto free_out;
		}

		/* write */
		w_len = 0;
		memcpy(bak + (ofs - ofs_align), ptr, i_len);
		res = mtd_write(mtd, ei.addr, mtd->erasesize, &w_len, bak);
		if (res || mtd->erasesize != w_len) {
			ret = -EIO;
			printk(KERN_ERR "%s: write to 0x%llx failed!\n",
				 __func__, ei.addr);
			goto free_out;
		}

		ptr += i_len;
		ofs += i_len;
		cnt -= i_len;

		/* add delay after write */
		udelay(10);
	}

free_out:
	mutex_unlock(&ra_mtd_mutex);
	kfree(bak);
out:
	put_mtd_device(mtd);
	return ret;
}
EXPORT_SYMBOL(ra_mtd_write_nm);

int ra_mtd_read_nm(char *name, loff_t from, size_t len, u_char *buf)
{
	int ret;
	size_t r_len = 0;
	struct mtd_info *mtd;

	mtd = get_mtd_device_nm(name);
	if (IS_ERR(mtd))
		return PTR_ERR(mtd);

	mutex_lock(&ra_mtd_mutex);
	ret = mtd_read(mtd, from, len, &r_len, buf);
	mutex_unlock(&ra_mtd_mutex);

	if (ret) {
		printk(KERN_ERR "%s: read from 0x%llx failed!\n", __func__, from);
		goto out;
	}

	if (r_len != len)
		printk("%s: read len (%zu) is not equal to requested len (%zu)\n",
			__func__, r_len, len);
out:
	put_mtd_device(mtd);
	return ret;
}
EXPORT_SYMBOL(ra_mtd_read_nm);

