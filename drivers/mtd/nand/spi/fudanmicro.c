// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_FUDAN		0xA1

#define FM25S01B_STATUS_ECC_MASK	(7 << 4)
#define STATUS_ECC_1_3_BITFLIPS		(1 << 4)
#define STATUS_ECC_4_6_BITFLIPS		(3 << 4)
#define STATUS_ECC_7_8_BITFLIPS		(5 << 4)

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
		SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int fm25s01b_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	region->offset = 64;
	region->length = 64;

	return 0;
}

static int fm25s01b_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	if (section > 3)
		return -ERANGE;

	region->offset = (16 * section) + 4;
	region->length = 12;

	return 0;
}

static const struct mtd_ooblayout_ops fm25s01b_ooblayout = {
	.ecc = fm25s01b_ooblayout_ecc,
	.free = fm25s01b_ooblayout_free,
};

static int fm25s01b_ecc_get_status(struct spinand_device *spinand, u8 status)
{
	switch (status & FM25S01B_STATUS_ECC_MASK) {
	case STATUS_ECC_NO_BITFLIPS:
		return 0;

	case STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	case STATUS_ECC_1_3_BITFLIPS:
		return 3;

	case STATUS_ECC_4_6_BITFLIPS:
		return 6;

	case STATUS_ECC_7_8_BITFLIPS:
		return 8;

	default:
		break;
	}

	return -EINVAL;
}

static const struct spinand_info fudan_spinand_table[] = {
	SPINAND_INFO("FM25S01B",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0xD4),
		     NAND_MEMORG(1, 2048, 128, 64, 1024, 20, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&fm25s01b_ooblayout,
				     fm25s01b_ecc_get_status)),
};

static const struct spinand_manufacturer_ops fudan_spinand_manuf_ops = {
};

const struct spinand_manufacturer fudan_spinand_manufacturer = {
	.id = SPINAND_MFR_FUDAN,
	.name = "Fudan Micro",
	.chips = fudan_spinand_table,
	.nchips = ARRAY_SIZE(fudan_spinand_table),
	.ops = &fudan_spinand_manuf_ops,
};
