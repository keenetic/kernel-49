#define pr_fmt(fmt)				"di: " fmt

#include <linux/fs.h>
#include <linux/log2.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/mtd/mtd.h>
#include <linux/seq_file.h>
#include <linux/byteorder/generic.h>

#ifdef CONFIG_MIPS
#include <prom.h>
#else
#include <linux/of.h>
#endif

#include "ndmpart_di.h"
#include "ndmpart_di_proc.h"

#define NDMPART_DI_U_STATE_MAGIC		(0x55535441)	/* "USTA" */
#define NDMPART_DI_U_STATE_VERSION		(1)

#define NDMPART_DI_WRITE_RETRIES		(3)

#define NDMPART_DI_PROC_ROOT			"dual_image"

#define NDMPART_DI_PROC_KBUF_SIZE		(4)

#define NDMPART_DI_PROC_FP(fp)			\
	((struct ndmpart_di *)NDMPART_DI_PROC_ENTRY_DATA(fp))

struct ndmpart_di_u_state {
	__be32					  magic;
	u8					  version;
	u8					  boot_active;
	u8					  boot_backup;
	u8					  boot_fails;
};

struct ndmpart_di {
	struct mutex				  mutex;
	struct ndmpart_di_u_state		  state;
	struct mtd_info				 *mtd;
	struct proc_dir_entry			 *root;
	u8					 *buf;
	u32					  buf_size;
	u32					  offset;
	u8					  boot_current;
};

static int __ndmpart_di_get(struct seq_file *seq, void *val)
{
	seq_printf(seq, "%hhu\n", *((u8 *)seq->private));
	return 0;
}

static int
__ndmpart_di_set(struct ndmpart_di *di, u8 *val, u8 val_min, u8 val_max,
		 const char __user *buf, size_t size)
{
	int ret;
	unsigned long ul;
	char kbuf[NDMPART_DI_PROC_KBUF_SIZE];

	if (sizeof(kbuf) < size + 1)
		return -EINVAL;

	if (copy_from_user(kbuf, buf, size))
		return -EFAULT;

	kbuf[size] = '\0';

	ret = kstrtoul(kbuf, 10, &ul);
	if (ret) {
		if (ret < 0)
			return ret;

		return -EINVAL;
	}

	if (ul < val_min || ul > val_max)
		return -EINVAL;

	mutex_lock(&di->mutex);
	*val = (u8)ul;
	mutex_unlock(&di->mutex);

	return 0;
}

#define NDMPART_DI_PROC_FUNC(entry, variable, name, val_min, val_max)	 \
static int								 \
__ndmpart_di_proc_open_##entry(struct inode *inode,			 \
			       struct file *fp)				 \
{									 \
	struct ndmpart_di *di = NDMPART_DI_PROC_FP(fp);			 \
									 \
	return single_open(fp, __ndmpart_di_get, &di->variable);	 \
}									 \
									 \
static ssize_t								 \
__ndmpart_di_proc_write_##entry(struct file *fp, const char __user *buf, \
				size_t size, loff_t *off)		 \
{									 \
	struct ndmpart_di *di = NDMPART_DI_PROC_FP(fp);			 \
	const int ret = __ndmpart_di_set(di, &di->variable,		 \
					 val_min, val_max, buf, size);	 \
									 \
	if (ret) {							 \
		pr_crit(name " set failed (%i)\n", ret);		 \
		return ret;						 \
	}								 \
									 \
	return size;							 \
}

NDMPART_DI_PROC_FUNC(boot_active, state.boot_active, "active image", 0, 2)
NDMPART_DI_PROC_FUNC(boot_backup, state.boot_backup, "backup image", 1, 2)
NDMPART_DI_PROC_FUNC(boot_fails, state.boot_fails, "boot fails", 0, 2)

static int
__ndmpart_di_proc_open_boot_current(struct inode *inode, struct file *fp)
{
	struct ndmpart_di *di = NDMPART_DI_PROC_FP(fp);

	return single_open(fp, __ndmpart_di_get, &di->boot_current);
}

static int __ndmpart_di_write(struct mtd_info *mtd, loff_t to, size_t len,
			      size_t *retlen, const u8 *buf)
{
	int ret, retries = NDMPART_DI_WRITE_RETRIES;

	do {
		struct erase_info ei = {
			.mtd  = mtd,
			.addr = to,
			.len  = mtd->erasesize
		};

		ret = mtd_erase(mtd, &ei);
		if (ret) {
			pr_err("MTD erase retry failed at 0x%012llx\n",
			       (unsigned long long)to);
		}

		ret = mtd_write(mtd, to, len, retlen, buf);
		if (ret) {
			pr_err("MTD write retry failed at 0x%012llx\n",
			       (unsigned long long)to);
		}

		if (len != *retlen) {
			pr_err("MTD short write at 0x%012llx\n",
			       (unsigned long long)to);
			ret = -EIO;
		}
	} while (ret && --retries);

	if (ret)
		pr_err("MTD write failed at 0x%012llx\n",
		       (unsigned long long)to);

	return ret;
}

static int __ndmpart_di_commit(struct ndmpart_di *di)
{
	size_t len;
	int ret = mtd_read(di->mtd, di->offset, di->buf_size, &len, di->buf);

	if (ret)
		return ret;

	if (len != di->buf_size)
		return -EIO;

	if (!memcmp(di->buf, &di->state, sizeof(di->state)))
		return 0;

	memcpy(di->buf, &di->state, sizeof(di->state));
	ret = __ndmpart_di_write(di->mtd, di->offset,
				 di->buf_size, &len, di->buf);
	if (ret)
		return ret;

	if (len != di->buf_size)
		return -EIO;

	return 0;
}

static ssize_t
__ndmpart_di_proc_write_commit(struct file *fp, const char __user *buf,
			       size_t size, loff_t *off)
{
	struct ndmpart_di *di = NDMPART_DI_PROC_FP(fp);
	int ret;

	mutex_lock(&di->mutex);
	ret = __ndmpart_di_commit(di);
	mutex_unlock(&di->mutex);

	if (ret) {
		pr_crit("state commit failed (%i)\n", ret);
		return ret;
	}

	return size;
}

static inline bool __ndmpart_di_enabled(void)
{
#if defined(CONFIG_ARCH_VEXPRESS)
	return true; /* QEMU */
#elif defined(CONFIG_OF_FLATTREE)
	u8 di = 0;

	return !of_property_read_u8(of_chosen, "dualimage", &di) && di;
#elif defined(CONFIG_MIPS)
	extern int env_dual_image;

	return env_dual_image > 0;
#else
	return false;
#endif
}

static inline bool __ndmpart_di_is_magic(__be32 magic)
{
	return be32_to_cpu(magic) == NDMPART_DI_U_STATE_MAGIC;
}

#ifdef CONFIG_MTD_NDM_U_STATE_SEARCH
static inline bool
__ndmpart_di_u_state_is_at(struct mtd_info *mtd, u32 offset)
{
	size_t len = 0;
	__be32 magic = 0;

	if (mtd_read(mtd, offset, sizeof(magic), &len, (u8 *)&magic) ||
	    len != sizeof(magic))
		return false;

	return __ndmpart_di_is_magic(magic);
}

static inline u32 __ndmpart_di_u_state_find(struct mtd_info *mtd)
{
	u32 offset;
	const u32 es = mtd->erasesize;
	const u32 start = CONFIG_MTD_NDM_U_STATE_SEARCH_START_BLOCK * es;
	const u32 end = CONFIG_MTD_NDM_U_STATE_SEARCH_END_BLOCK * es;

	if (start < end) {
		for (offset = start; offset <= end; offset += es) {
			if (__ndmpart_di_u_state_is_at(mtd, offset))
				return offset;
		}
	} else {
		for (offset = start; offset >= end; offset -= es) {
			if (__ndmpart_di_u_state_is_at(mtd, offset))
				return offset;
		}
	}

	return NDMPART_DI_NO_OFFSET;
}

#else /* CONFIG_MTD_NDM_U_STATE_SEARCH */

static inline u32 __ndmpart_di_u_state_find(struct mtd_info *mtd)
{
	return NDMPART_DI_NO_OFFSET;
}
#endif /* CONFIG_MTD_NDM_U_STATE_SEARCH */

static inline u32 __ndmpart_di_u_state_offset(struct mtd_info *mtd)
{
	u32 offset = __ndmpart_di_u_state_find(mtd);

	if (offset != NDMPART_DI_NO_OFFSET)
		return offset;

	/* fixed offset */
	offset = mtd->size >> 1;

	/* offset must be aligned */
	if (is_power_of_2(mtd->erasesize))
		offset &= ~(mtd->erasesize - 1);

	return offset;
}

u32 ndmpart_di_u_state_offset(struct mtd_info *mtd)
{
	if (!__ndmpart_di_enabled())
		return NDMPART_DI_NO_OFFSET;

	return __ndmpart_di_u_state_offset(mtd);
}

static void __ndmpart_di_notify_add_stub(struct mtd_info *mtd) {}
static void __ndmpart_di_notify_remove(struct mtd_info *mtd);

static struct ndmpart_di __di;
static struct mtd_notifier __mn = {
	.add	= __ndmpart_di_notify_add_stub,
	.remove	= __ndmpart_di_notify_remove,
};
static DEFINE_MUTEX(__mn_mutex);

int ndmpart_di_init(struct mtd_info *mtd, u32 flash_size_limit)
{
	struct ndmpart_di *di = &__di;
	NDMPART_DI_PROC_DECLARE_RW(boot_active);
	NDMPART_DI_PROC_DECLARE_RW(boot_backup);
	NDMPART_DI_PROC_DECLARE_RO(boot_current);
	NDMPART_DI_PROC_DECLARE_RW(boot_fails);
	NDMPART_DI_PROC_DECLARE_WO(commit);
	u32 offset;
	size_t len;
	int ret;

	offset = ndmpart_di_u_state_offset(mtd);
	if (offset == NDMPART_DI_NO_OFFSET) {
		pr_err("not supported\n");
		return -EOPNOTSUPP;
	}

	if (offset < flash_size_limit) {
		pr_err("invalid flash size limit (0x%08x / 0x%08x)\n",
		       offset, flash_size_limit);
		return -EINVAL;
	}

	if (mtd->type == MTD_NORFLASH)
		di->buf_size = sizeof(di->state);
	else
		di->buf_size = ALIGN(sizeof(di->state), mtd->writesize);

	di->buf = kzalloc(di->buf_size, GFP_KERNEL);
	if (!di->buf) {
		ret = -ENOMEM;
		goto error_kfree;
	}

	ret = mtd_read(mtd, offset, sizeof(di->state),
		       &len, (u8 *)&di->state);
	if (ret != 0 || len != sizeof(di->state)) {
		pr_err("MTD read failed at 0x%012x\n", offset);
		ret = -EIO;
		goto error_kfree;
	}

	if (!__ndmpart_di_is_magic(di->state.magic)) {
		pr_err("unknown magic 0x%08x\n",
		       be32_to_cpu(di->state.magic));
		ret = -EINVAL;
		goto error_kfree;
	}

	if (di->state.version != NDMPART_DI_U_STATE_VERSION) {
		pr_err("unknown version %hhu\n", di->state.version);
		ret = -EINVAL;
		goto error_kfree;
	}

	ret = -ENOMEM;

	/* di must be initialized before /proc files creation */
	di->mtd = mtd;
	di->offset = offset;

	mutex_init(&di->mutex);

	di->root = NDMPART_DI_PROC_CREATE_DIR(NDMPART_DI_PROC_ROOT, NULL);
	if (!di->root)
		goto error_kfree;

	if (!NDMPART_DI_PROC_CREATE_RW(boot_active, di->root, di))
		goto error_boot_active;

	if (!NDMPART_DI_PROC_CREATE_RW(boot_backup, di->root, di))
		goto error_boot_backup;

	if (!NDMPART_DI_PROC_CREATE_RO(boot_current, di->root, di))
		goto error_boot_current;

	if (!NDMPART_DI_PROC_CREATE_RW(boot_fails, di->root, di))
		goto error_boot_fails;

	if (!NDMPART_DI_PROC_CREATE_WO(commit, di->root, di))
		goto error_commit;

	if (di->state.boot_active) {
		/* equal to an active value */
		if (di->state.boot_active == NDMPART_DI_IMAGE_FIRST)
			di->boot_current = NDMPART_DI_IMAGE_FIRST;
		else
			di->boot_current = NDMPART_DI_IMAGE_SECOND;
	} else {
		/* opposite to a backup value */
		if (di->state.boot_backup == NDMPART_DI_IMAGE_FIRST)
			di->boot_current = NDMPART_DI_IMAGE_SECOND;
		else
			di->boot_current = NDMPART_DI_IMAGE_FIRST;
	}

	pr_info("active = %hhu, backup = %hhu, current = %hhu\n",
		di->state.boot_active, di->state.boot_backup,
		di->boot_current);

	register_mtd_user(&__mn);

	return di->boot_current;

error_commit:
	NDMPART_DI_PROC_REMOVE(boot_fails, di->root);

error_boot_fails:
	NDMPART_DI_PROC_REMOVE(boot_current, di->root);

error_boot_current:
	NDMPART_DI_PROC_REMOVE(boot_backup, di->root);

error_boot_backup:
	NDMPART_DI_PROC_REMOVE(boot_active, di->root);

error_boot_active:
	NDMPART_DI_PROC_REMOVE_DIR(NDMPART_DI_PROC_ROOT, NULL);

error_kfree:
	kfree(di->buf);

	memset(&di, 0, sizeof(di));
	return ret;
}

static void __ndmpart_di_fini(void)
{
	struct ndmpart_di *di = &__di;

	NDMPART_DI_PROC_REMOVE(commit, di->root);
	NDMPART_DI_PROC_REMOVE(boot_fails, di->root);
	NDMPART_DI_PROC_REMOVE(boot_current, di->root);
	NDMPART_DI_PROC_REMOVE(boot_backup, di->root);
	NDMPART_DI_PROC_REMOVE(boot_active, di->root);
	NDMPART_DI_PROC_REMOVE_DIR(NDMPART_DI_PROC_ROOT, NULL);

	kfree(di->buf);

	memset(di, 0, sizeof(*di));
}

void ndmpart_di_fini()
{
	mutex_lock(&__mn_mutex);
	__ndmpart_di_fini();
	mutex_unlock(&__mn_mutex);
}

static void __ndmpart_di_notify_remove(struct mtd_info *mtd)
{
	struct ndmpart_di *di = &__di;

	mutex_lock(&__mn_mutex);

	if (di->mtd == mtd)
		__ndmpart_di_fini();

	mutex_unlock(&__mn_mutex);
}

int __init ndmpart_di_module_init(void)
{
	return 0;
}

void __exit ndmpart_di_module_exit(void)
{
	unregister_mtd_user(&__mn);
}

module_init(ndmpart_di_module_init);
module_exit(ndmpart_di_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NDM dual image control module");
