// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_UNIM			0xB0

#define UNIM_8_STATUS_ECC_MASK			(7 << 4)
#define UNIM_8_STATUS_ECC_UNCOR_ERROR		(2 << 4)
#define UNIM_8_STATUS_ECC_1_3_BITFLIPS		(1 << 4)
#define UNIM_8_STATUS_ECC_4_6_BITFLIPS		(3 << 4)
#define UNIM_8_STATUS_ECC_7_8_BITFLIPS		(5 << 4)

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

static SPINAND_OP_VARIANTS(read_cache_variants_x2,
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants_x1,
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

/* Program load random data (opcode 0x84) is broken on UNIM */
static SPINAND_OP_VARIANTS(update_cache_variants_x1,
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static int unim_64_ooblayout_ecc(struct mtd_info *mtd, int section,
				 struct mtd_oob_region *region)
{
	return -ERANGE;
}

static int unim_64_ooblayout_free(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	/* Reserve 2 bytes for the BBM. */
	region->offset = 2;
	region->length = 62;

	return 0;
}

static const struct mtd_ooblayout_ops unim_64_ooblayout = {
	.ecc = unim_64_ooblayout_ecc,
	.free = unim_64_ooblayout_free,
};

static int unim_8_ecc_get_status(struct spinand_device *spinand, u8 status)
{
	switch (status & UNIM_8_STATUS_ECC_MASK) {
	case STATUS_ECC_NO_BITFLIPS:
		return 0;

	case UNIM_8_STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	case UNIM_8_STATUS_ECC_1_3_BITFLIPS:
		return 3;

	case UNIM_8_STATUS_ECC_4_6_BITFLIPS:
		return 6;

	case UNIM_8_STATUS_ECC_7_8_BITFLIPS:
		return 8;

	default:
		break;
	}

	return -EINVAL;
}

static const struct spinand_info unim_spinand_table[] = {
	SPINAND_INFO("UM19A0HISW",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x14),
		     NAND_MEMORG(1, 2048, 64, 64, 1024, 20, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants_x2,
					      &write_cache_variants_x1,
					      &update_cache_variants_x1),
		     SPINAND_HAS_QE_BIT | SPINAND_PP_BROKEN_LOAD_RANDOM |
		     SPINAND_SKIP_X4_MODES,
		     SPINAND_ECCINFO(&unim_64_ooblayout,
				     unim_8_ecc_get_status)),
};

static const struct spinand_manufacturer_ops unim_spinand_manuf_ops = {
};

const struct spinand_manufacturer unim_spinand_manufacturer = {
	.id = SPINAND_MFR_UNIM,
	.name = "UNIM",
	.chips = unim_spinand_table,
	.nchips = ARRAY_SIZE(unim_spinand_table),
	.ops = &unim_spinand_manuf_ops,
};
