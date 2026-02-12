#include <linux/bug.h>
#include <linux/ctype.h>
#include <linux/magic.h>
#include <linux/atomic.h>
#include <linux/string.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include "../../fs/squashfs/squashfs_fs.h"
#include "check.h"
#include "ndm.h"

#define NDM_MAX(a, b)				  ((a) < (b) ? (b) : (a))

#define NDM_PART_INDEX_1			  ('1')
#define NDM_PART_INDEX_2			  ('2')
#define NDM_PART_INDEX_INVALID			  ('\0')
#define NDM_PART_INDEX_PHOLDER			  "X"

#define NDM_PART_KERNEL_NAME_TMPL		  \
	"Kernel_" NDM_PART_INDEX_PHOLDER
#define NDM_PART_ROOTFS_NAME_TMPL		  \
	"RootFS_" NDM_PART_INDEX_PHOLDER

#define NDM_PART_NAME_MAX			  \
	NDM_MAX(sizeof(NDM_PART_KERNEL_NAME_TMPL),\
		sizeof(NDM_PART_ROOTFS_NAME_TMPL))

#define NDM_PARTNAME_NODE			  "partname"

/* MediaTek u-boot use align SZ_512K for eMMC/SD images */
#define NDM_BLOCK_SIZE				  (0x80000)

#define NDM_SECTOR_SIZE				  (512)
#define NDM_SECTOR_STEP				  \
	((NDM_BLOCK_SIZE) / (NDM_SECTOR_SIZE))

#define NDM_SQUASHFS_MAJOR			  (4)
#define NDM_SQUASHFS_MINOR			  (0)

#define NDM_SQUASHFS_FILE_MAX_SIZE		  (1048576)
#define NDM_SQUASHFS_FILE_MAX_LOG		  (20)

#define NDM_SQUASHFS_MD_SIZE			  (8192)

#define NDM_SQUASHFS_INODE_OFFS(inode)		  \
	((unsigned int)((inode) & 0xffff))

struct ndm_part_name {
	char					  str[NDM_PART_NAME_MAX];
};

static int __ndm_part_limit(const struct parsed_partitions *state)
{
	return state->limit;
}

static struct gendisk *
__ndm_part_disk(const struct parsed_partitions *state)
{
	return state->bdev->bd_disk;
}

static void __ndm_part_print(const struct parsed_partitions *state,
			     int n, char mark)
{
	const struct gendisk *disk = __ndm_part_disk(state);
	typeof(*state->parts) *p = &state->parts[n];

	pr_info(" %s: %02i %c %016llx-%016llx \"%s\"\n",
		disk->disk_name, n, mark,
		((u64)p->from) * NDM_SECTOR_SIZE,
		((u64)(p->from + p->size)) * NDM_SECTOR_SIZE,
		p->info.volname);
}

static bool __ndm_is_boot_media(struct gendisk *disk,
				const struct device_node *rootdisk_np)
{
	const struct device_node *disk_np = rootdisk_np;

	while (disk_np) {
		struct device *part_dev = disk_to_dev(disk);

		while (part_dev) {
			if (dev_of_node(part_dev) == disk_np)
				return true;

			part_dev = part_dev->parent;
		}

		disk_np = disk_np->parent;
	}

	return false;
}

static int __ndm_part_find(const struct parsed_partitions *state,
			   const struct device_node *rootdisk_np,
			   const char *partname)
{
	int i;

	for (i = 0; i < __ndm_part_limit(state); i++) {
		const struct partition_meta_info *info;

		if (!state->parts[i].has_info)
			continue;

		info = &state->parts[i].info;
		if (strcmp(info->volname, partname))
			continue;

		if (__ndm_is_boot_media(__ndm_part_disk(state), rootdisk_np))
			return i;
	}

	return -1;
}

static int
__ndm_part_get_free(const struct parsed_partitions *state, int start)
{
	int i;

	for (i = start + 1; i < __ndm_part_limit(state); i++) {
		if (!state->parts[i].has_info)
			return i;
	}

	return -1;
}

static bool __ndm_probe_rootfs(struct squashfs_super_block *sb,
			       s64 bytes_max)
{
	sb->s_magic = le32_to_cpu(sb->s_magic);
	if (sb->s_magic != SQUASHFS_MAGIC)
		return false;

	sb->s_major = le16_to_cpu(sb->s_major);
	sb->s_minor = le16_to_cpu(sb->s_minor);

	if (sb->s_major != NDM_SQUASHFS_MAJOR ||
	    sb->s_minor != NDM_SQUASHFS_MINOR)
		return false;

	sb->bytes_used = le64_to_cpu(sb->bytes_used);
	if (sb->bytes_used < 0 ||
	    sb->bytes_used > bytes_max)
		return false;

	sb->block_size = le32_to_cpu(sb->block_size);
	if (sb->block_size > NDM_SQUASHFS_FILE_MAX_SIZE)
		return false;

	if (sb->block_size < PAGE_SIZE)
		return false;

        sb->block_log = le16_to_cpu(sb->block_log);
	if (sb->block_log > NDM_SQUASHFS_FILE_MAX_LOG)
		return false;

	if (sb->block_size != (1 << sb->block_log))
		return false;

	sb->root_inode = le64_to_cpu(sb->root_inode);
	if (NDM_SQUASHFS_INODE_OFFS(sb->root_inode) > NDM_SQUASHFS_MD_SIZE)
		return false;

	return true;
}

static int
__ndm_find_rootfs_sector(struct parsed_partitions *state,
			 int f, sector_t *rootfs_from)
{
	typeof(*state->parts) *fpart = &state->parts[f];
	sector_t end = fpart->from + fpart->size;
	s64 bytes_max = fpart->size * NDM_SECTOR_SIZE;
	sector_t n;

	BUILD_BUG_ON((BLOCK_SIZE % NDM_SECTOR_SIZE) != 0);

	*rootfs_from = 0;

	for (n = fpart->from; n < end; n += NDM_SECTOR_STEP) {
		Sector sect;
		bool is_rootfs;
		struct squashfs_super_block *sb;

		BUILD_BUG_ON(sizeof(*sb) > NDM_SECTOR_SIZE);

		sb = read_part_sector(state, n, &sect);
		if (!sb)
			return -EIO;

		is_rootfs = __ndm_probe_rootfs(sb, bytes_max);
		put_dev_sector(sect);

		if (is_rootfs) {
			*rootfs_from = n;
			return 0;
		}

		bytes_max -= NDM_SECTOR_STEP * NDM_SECTOR_SIZE;
	}

	return -ENOENT;
}

static int __ndm_part_add_ro(struct parsed_partitions *state, int f, int n,
			     const char *volname, sector_t from,
			     sector_t size)
{
	typeof(*state->parts) *fpart = &state->parts[f];
	typeof(*state->parts) *part = &state->parts[n];
	struct partition_meta_info *info;
	char *p;
	int vsz;

	memcpy(part, fpart, sizeof(*fpart));

	part->from = from;
	part->size = size;
	part->flags |= ADDPART_FLAG_READONLY;

	info = &part->info;

	memset(info->uuid, 0, sizeof(info->uuid));

	vsz = snprintf(info->volname, sizeof(info->volname), "%s", volname);
	if (vsz < 0 || vsz > sizeof(info->volname))
		return -ENOSPC;

	p = strchr(state->pp_buf, '\n');
	if (p)
		*p = '\0';

	put_partition(state, n, from, size);
	strlcat(state->pp_buf, "\n", PAGE_SIZE);

	__ndm_part_print(state, n, '*');

	return 0;
}

static bool __ndm_part_name_get(struct ndm_part_name *part_name,
				const char *part_tmpl,
				const char *firmware_name)
{
	size_t size = strlen(firmware_name);
	char index;
	char *p;

	/*
	 * Check if a firmware name ends with a single digit "1" or "2".
	 **/
	if (size <= 1)
		return -EINVAL;

	if (isdigit(firmware_name[size - 2]))
		return -EINVAL;

	index = firmware_name[size - 1];

	if (index != NDM_PART_INDEX_1 &&
	    index != NDM_PART_INDEX_2)
		return -EINVAL;

	/*
	 * Check if a template name ends with a placeholder.
	 **/
	size = strlen(part_tmpl);
	if (size <= 1)
		return -EINVAL;

	p = strstr(part_tmpl, NDM_PART_INDEX_PHOLDER);
	if (!p)
		return -EINVAL;

	if (p != part_tmpl + size - 1)
		return -EINVAL;

	if (sizeof(part_name->str) < size + 1)
		return -ENOBUFS;

	memcpy(part_name->str, part_tmpl, size + 1);
	part_name->str[size - 1] = index;

	return 0;
}

static int __ndm_part_split(struct parsed_partitions *state,
			    const char *kernel_name,
			    const char *rootfs_name,
			    int f, int k, int r)
{
	sector_t rootfs_from, kernel_size;
	typeof(*state->parts) *fpart;
	int ret = __ndm_find_rootfs_sector(state, f, &rootfs_from);

	if (ret < 0) {
		pr_err("unable to find the NDM root FS image: %i\n", ret);
		return ret;
	}

	fpart = &state->parts[f];
	kernel_size = rootfs_from - fpart->from;

	ret = __ndm_part_add_ro(state, f, k, kernel_name,
				fpart->from, kernel_size);
	if (ret < 0) {
		pr_err("unable to add the \"%s\" NDM partition: %i\n",
		       kernel_name, ret);
		return ret;
	}

	ret = __ndm_part_add_ro(state, f, r, rootfs_name,
				rootfs_from, fpart->size - kernel_size);
	if (ret < 0) {
		pr_err("unable to add the \"%s\" NDM partition: %i\n",
		       rootfs_name, ret);
		return ret;
	}

	return 0;
}

static int __ndm_update_partname(struct device_node *rootdisk_np,
				 const char *partname)
{
	const int partname_size = (int)(strlen(partname) + 1);
	struct property *prop;

	prop = kzalloc(sizeof(*prop) + partname_size, GFP_KERNEL);
	if (!prop)
		return -ENOMEM;

	prop->value = prop + 1;
	prop->length = partname_size;

	prop->name = kstrdup(NDM_PARTNAME_NODE, GFP_KERNEL);
	if (!prop->name) {
		kfree(prop);
		return -ENOMEM;
	}

	memcpy(prop->value, partname, partname_size);
	of_update_property(rootdisk_np, prop);

	return 0;
}

int ndm_partition(struct parsed_partitions *state)
{
	static atomic_t splitted = ATOMIC_INIT(0);
	struct ndm_part_name kernel_name;
	struct ndm_part_name rootfs_name;
	struct device_node *rootdisk_np;
	const char *partname;
	int k, r, f, ret;
	int i;

	if (atomic_read(&splitted))
		return 1;

	partname = NULL;
	rootdisk_np = of_parse_phandle(of_chosen, "rootdisk", 0);

	if (of_property_read_string(rootdisk_np, NDM_PARTNAME_NODE,
				    &partname)) {
		of_node_put(rootdisk_np);
		pr_err("no NDM boot partition name found\n");
		return -ENOENT;
	}

	f = __ndm_part_find(state, rootdisk_np, partname);
	if (f < 0) {
		/* skip partition split on an unknown media */
		of_node_put(rootdisk_np);
		return 0;
	}

	ret = __ndm_part_name_get(&kernel_name, NDM_PART_KERNEL_NAME_TMPL,
				  partname);
	if (ret != 0) {
		of_node_put(rootdisk_np);
		pr_err("unable to get the %s NDM partition name\n", "kernel");
		return ret;
	}

	ret = __ndm_part_name_get(&rootfs_name, NDM_PART_ROOTFS_NAME_TMPL,
				  partname);
	if (ret != 0) {
		of_node_put(rootdisk_np);
		pr_err("unable to get the %s NDM partition name\n", "root FS");
		return ret;
	}

	of_node_put(rootdisk_np);

	if (!atomic_add_unless(&splitted, 1, 1))
		return 1;

	k = __ndm_part_get_free(state, f);
	if (k < 0) {
		pr_err("no free slot for the %s NDM partition\n", "kernel");
		return -ENOSPC;
	}

	r = __ndm_part_get_free(state, k);
	if (r < 0) {
		pr_err("no free slot for the %s NDM partition\n", "root FS");
		return -ENOSPC;
	}

	for (i = 0; i < __ndm_part_limit(state); i++) {
		if (state->parts[i].has_info)
			__ndm_part_print(state, i, ' ');
	}

	ret = __ndm_part_split(state, kernel_name.str,
			       rootfs_name.str, f, k, r);
	if (ret < 0)
		return ret;

	ret = __ndm_update_partname(rootdisk_np, rootfs_name.str);
	if (ret < 0)
		return ret;

	return 1;
}
