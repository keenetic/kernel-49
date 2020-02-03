#include <crypto/hash.h>
#include <linux/mtd/mtd.h>
#include <linux/sizes.h>
#include <linux/sched.h>

#define MTD_PART_CONFIG_1		"Config_1"
#define MTD_PART_CONFIG			"Config"
#define MTD_PART_FULL			"Full"

#define MTD_SIZE_LIMIT_NAND		SZ_2M
#define MTD_SIZE_LIMIT_NOR		SZ_512K

#define HASH_TRANSFORM			"sha256"
#define HASH_DIGEST_SIZE		(256 / 8)

/*
 * Magic value to make kernel happy,
 * Should be more than 128 (drivers/char/random.c, 723)
 * but less than hash digest size (256 for sha256)
 */
#define RNG_ENTROPY_BITS		249

/* drivers/char/random.c */
int random_add_entropy(void *p, size_t size, size_t ent_count);
void crng_wait_ready_external(void);

static int ndm_rng_read_device_eb(struct mtd_info *mtd, loff_t addr,
				  const size_t size, u8 *buf)
{
	size_t read;
	int err;

	err = mtd_read(mtd, addr, size, &read, buf);
	/* Ignore corrected ECC errors */
	if (mtd_is_bitflip(err))
		err = 0;
	if (!err && read != size)
		err = -EIO;
	if (err)
		pr_err_ratelimited("error: read failed at %#llx\n", addr);

	return err;
}

static int ndm_rng_read_device(struct mtd_info *mtd, struct shash_desc *shash)
{
	u8 *iobuf;
	uint64_t tmp;
	int i, nbr, ebcnt, rlimit;
	int err = 0;

	tmp = mtd->size;
	do_div(tmp, mtd->erasesize);
	ebcnt = (int)tmp;

	rlimit = MTD_SIZE_LIMIT_NAND;
	if (mtd->type == MTD_NORFLASH)
		rlimit = MTD_SIZE_LIMIT_NOR;

	iobuf = kmalloc(mtd->erasesize, GFP_KERNEL);
	if (!iobuf)
		return -ENOMEM;

	nbr = 0;
	for (i = 0; i < ebcnt; i++) {
		loff_t addr = (loff_t)i * mtd->erasesize;
		int ret;

		if (mtd_can_have_bb(mtd) && mtd_block_isbad(mtd, addr))
			continue;

		ret = ndm_rng_read_device_eb(mtd, addr, mtd->erasesize, iobuf);
		if (ret) {
			if (!err)
				err = ret;
			goto read_out;
		}

		ret = crypto_shash_update(shash, iobuf, mtd->erasesize);
		if (ret) {
			if (!err)
				err = ret;
			goto read_out;
		}

		nbr++;
		if ((nbr * mtd->erasesize) >= rlimit)
			goto read_out;

		cond_resched();
	}

read_out:

	kfree(iobuf);

	if (err)
		pr_info("error %d occurred\n", err);
	return err;
}

void ndm_rng_seed(void)
{
	struct mtd_info *config_mtd = NULL;
	struct mtd_info *full_mtd = NULL;
	struct crypto_shash *tfm = NULL;
	const char *config_str = MTD_PART_CONFIG_1;
	int err = 0;

	pr_info("RNG reseed started\n");

	config_mtd = get_mtd_device_nm(config_str);
	if (IS_ERR(config_mtd)) {
		config_str = MTD_PART_CONFIG;
		config_mtd = get_mtd_device_nm(config_str);
	}

	if (IS_ERR(config_mtd)) {
		err = PTR_ERR(config_mtd);
		pr_err("cannot get MTD device %s: %d\n", config_str, err);
		config_mtd = NULL;
		goto out;
	}

	full_mtd = get_mtd_device_nm(MTD_PART_FULL);
	if (IS_ERR(full_mtd)) {
		err = PTR_ERR(full_mtd);
		pr_err("cannot get MTD device %s: %d\n", MTD_PART_FULL, err);
		full_mtd = NULL;
		goto out;
	}

	tfm = crypto_alloc_shash(HASH_TRANSFORM, 0, CRYPTO_ALG_ASYNC);
	if (!tfm) {
		pr_err("failed to load transform for %s\n", HASH_TRANSFORM);
		goto out;
	}

	do {
		SHASH_DESC_ON_STACK(shash, tfm);
		u8 digest[HASH_DIGEST_SIZE];

		shash->tfm = tfm;
		shash->flags = 0;

		if (ndm_rng_read_device(config_mtd, shash) ||
		    ndm_rng_read_device(full_mtd, shash)) {
			pr_err("unable to read MTD device\n");
			goto out;
		}

		if (crypto_shash_final(shash, digest)) {
			pr_err("unable to form out digest\n");
			goto out;
		}

		if (random_add_entropy(digest, HASH_DIGEST_SIZE,
				       RNG_ENTROPY_BITS) < 0) {
			pr_err("unable to credit entropy pool\n");
			goto out;
		}
	} while (0);

	cond_resched();

	pr_info("RNG reseeded\n");

	crng_wait_ready_external();

out:
	if (tfm != NULL)
		crypto_free_shash(tfm);

	if (config_mtd != NULL)
		put_mtd_device(config_mtd);

	if (full_mtd != NULL)
		put_mtd_device(full_mtd);
}
