#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/concat.h>

#define MTD_PART_STORAGE_A		"Storage_A"
#define MTD_PART_STORAGE_B		"Storage_B"

#define MTD_UBI_MAP			"ndmubipart"

static struct mtd_info *concat_mtd[2];
static struct mtd_info *merged_mtd;

static const char *upart_probe_types[] = {
	MTD_UBI_MAP,
	NULL
};

enum upart {
	UPART_UBI_FULL,
	UPART_MAX
};

struct upart_dsc {
	const char *name;
	uint32_t offset;
	uint32_t size;
};

static struct upart_dsc uparts[UPART_MAX] = {
	[UPART_UBI_FULL] = {
		name: "Storage",
		offset: 0,
		size: MTDPART_SIZ_FULL
	}
};

static void cleanup_concat_mtd(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(concat_mtd); i++) {
		if (concat_mtd[i]) {
			put_mtd_device(concat_mtd[i]);
			concat_mtd[i] = NULL;
		}
	}
}

static int create_ubi_mtd_partitions(struct mtd_info *m,
				     const struct mtd_partition **pparts,
				     struct mtd_part_parser_data *data)
{
	struct mtd_partition *ndm_ubi_parts;
	size_t i;

	ndm_ubi_parts = kzalloc(sizeof(*ndm_ubi_parts) * UPART_MAX, GFP_KERNEL);
	if (ndm_ubi_parts == NULL)
		return -ENOMEM;

	for (i = 0; i < UPART_MAX; i++) {
		ndm_ubi_parts[i].name = (char *)uparts[i].name;
		ndm_ubi_parts[i].offset = uparts[i].offset;
		ndm_ubi_parts[i].size = uparts[i].size;
	}

	*pparts = ndm_ubi_parts;

	return UPART_MAX;
}

static struct mtd_part_parser ndm_ubi_parser = {
	.owner = THIS_MODULE,
	.parse_fn = create_ubi_mtd_partitions,
	.name = MTD_UBI_MAP,
};

static int __init init_ndm_storage(void)
{
	struct mtd_info *mtd = NULL;
	int err = 0;

	memset(concat_mtd, 0, sizeof(concat_mtd));

	pr_info("Searching for suitable storage partitions...\n");

	mtd = get_mtd_device_nm(MTD_PART_STORAGE_A);
	if (IS_ERR(mtd)) {
		pr_info("No storage partitions found\n");

		return err;
	}

	pr_info("Found 1st storage partition of size %llu bytes\n", mtd->size);

	concat_mtd[0] = mtd;

	mtd = get_mtd_device_nm(MTD_PART_STORAGE_B);
	if (!IS_ERR(mtd)) {
		pr_info("Found 2nd storage partition of size %llu bytes\n",
			mtd->size);
		concat_mtd[1] = mtd;
	}

	pr_info("Registering UBI data partitions parser\n");

	register_mtd_parser(&ndm_ubi_parser);

	/* Combine the two partitions into a single MTD device & register it: */
	merged_mtd = mtd_concat_create(concat_mtd,
		concat_mtd[1] == NULL ? 1 : 2,
		"NDM combined UBI partition");

	if (merged_mtd)
		err = mtd_device_parse_register(
			merged_mtd, upart_probe_types, NULL, NULL, 0);

	if (err) {
		cleanup_concat_mtd();
		pr_err("Merging storage partitions failed\n");
	} else
		pr_info("Merging storage partitions OK\n");

	return err;
}

static void __exit exit_ndm_storage(void)
{
	if (merged_mtd) {
		mtd_device_unregister(merged_mtd);
		mtd_concat_destroy(merged_mtd);
		merged_mtd = NULL;
	}

	cleanup_concat_mtd();
}

module_init(init_ndm_storage);
module_exit(exit_ndm_storage);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NDM");
MODULE_DESCRIPTION("MTD merged Storage map driver for NDMS");
