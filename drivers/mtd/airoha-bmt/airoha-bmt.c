#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

/*
 * Airoha BMT algorithm
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/bits.h>
#include <linux/mtd/mtd.h>
#include <linux/of.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)
#define NEED_ERASE_CALLBACK
#endif

#define MAX_BMT_SIZE			(250)
#define MAX_RAW_BAD_BLOCK_SIZE		(250)

#define POOL_GOOD_BLOCK_PERCENT(_b)	((_b) * 8 / 100)
#define MAX_BMT_PERCENT(_b)		((_b) * 1 / 10)

struct bmt_table_header {
	char signature[3];
	u8 version;
	u8 bad_count;	// this field is useless
	u8 size;
	u8 checksum;
	u8 reserved[13];
};

struct bmt_entry {
	u16 from;
	u16 to;
};

struct bmt_table {
	struct bmt_table_header header;
	struct bmt_entry table[MAX_BMT_SIZE];
};

struct bbt_table_header {
	char signature[4];
	u32 checksum;
	u8 version;
	u8 size;
	u8 reserved[2];
};

struct bbt_table {
	struct bbt_table_header header;
	u16 table[MAX_RAW_BAD_BLOCK_SIZE];
};

#define RESULT_FAIL_READ	1
#define RESULT_FAIL_WRITE	2

struct bmt_desc {
	struct mtd_info *mtd;
	u8 *data_buf;

	int (*_read_oob) (struct mtd_info *mtd, loff_t from,
			  struct mtd_oob_ops *ops);
	int (*_write_oob) (struct mtd_info *mtd, loff_t to,
			   struct mtd_oob_ops *ops);
	int (*_panic_write) (struct mtd_info *mtd, loff_t to, size_t len,
			     size_t *retlen, const u_char *buf);
	int (*_erase) (struct mtd_info *mtd, struct erase_info *instr);
	int (*_block_isbad) (struct mtd_info *mtd, loff_t ofs);
	int (*_block_markbad) (struct mtd_info *mtd, loff_t ofs);

	u32 pg_size;
	u32 blk_size;
	u16 pg_shift;
	u16 blk_shift;

	const __be32 *remap_range;
	int remap_range_len;
};

static struct bmt_desc bmtd;

static struct bbt_table bbt;
static struct bmt_table bmt;

static int pbbt[0xffff];
static int bmt_index = 0xffff;
static int bbt_index = 0xffff;
static u32 total_blks, system_blks, bmt_blks;

static inline u32 blk_pg(u16 block)
{
	return (u32)block << (bmtd.blk_shift - bmtd.pg_shift);
}

static int
bbt_nand_read(u32 page, void *dat, int dat_len,
	      unsigned char *fdm, int fdm_len)
{
	struct mtd_oob_ops ops = {
		.mode = MTD_OPS_PLACE_OOB,
		.ooboffs = 0,
		.oobbuf = fdm,
		.ooblen = fdm_len,
		.datbuf = dat,
		.len = dat_len,
	};
	int ret;

	ret = bmtd._read_oob(bmtd.mtd, page << bmtd.pg_shift, &ops);
	if (ret < 0)
		return ret;

	if (ret)
		pr_warn("%s: %d bitflips\n", __func__, ret);

	return 0;
}

static int
bbt_nand_copy(u16 dest_blk, u16 src_blk, loff_t max_offset)
{
	int pages = bmtd.blk_size >> bmtd.pg_shift;
	loff_t src = (loff_t)src_blk << bmtd.blk_shift;
	loff_t dest = (loff_t)dest_blk << bmtd.blk_shift;
	loff_t offset = 0;
	uint8_t oob[256];
	int i, ret;

	for (i = 0; i < pages; i++) {
		struct mtd_oob_ops rd_ops = {
			.mode = MTD_OPS_PLACE_OOB,
			.oobbuf = oob,
			.ooblen = min_t(int, bmtd.mtd->oobsize / pages, sizeof(oob)),
			.datbuf = bmtd.data_buf,
			.len = bmtd.pg_size,
		};
		struct mtd_oob_ops wr_ops = {
			.mode = MTD_OPS_PLACE_OOB,
			.oobbuf = oob,
			.datbuf = bmtd.data_buf,
			.len = bmtd.pg_size,
		};

		if (offset >= max_offset)
			break;

		ret = bmtd._read_oob(bmtd.mtd, src + offset, &rd_ops);
		if (ret < 0 && !mtd_is_bitflip(ret))
			return RESULT_FAIL_READ;

		if (!rd_ops.retlen)
			break;

		ret = bmtd._write_oob(bmtd.mtd, dest + offset, &wr_ops);
		if (ret < 0)
			return RESULT_FAIL_WRITE;

		wr_ops.ooblen = rd_ops.oobretlen;
		offset += rd_ops.retlen;
	}

	return 0;
}

static bool is_bad_raw(int block)
{
	u8 fdm[4];
	int ret;

	ret = bbt_nand_read(blk_pg(block), bmtd.data_buf, bmtd.pg_size,
			    fdm, sizeof(fdm));
	if (ret || fdm[0] != 0xff)
		return true;

	return false;
}

static bool is_bad(int block)
{
	u8 fdm[4];
	int ret;

	ret = bbt_nand_read(blk_pg(block), bmtd.data_buf, bmtd.pg_size,
			    fdm, sizeof(fdm));

	if (ret || fdm[0] != 0xff || fdm[1] != 0xff)
		return true;

	return false;
}

static bool is_mapped(int block)
{
	u8 fdm[4];
	int ret;
	u16 mapped_block;

	ret = bbt_nand_read(blk_pg(block), bmtd.data_buf, bmtd.pg_size,
			    fdm, sizeof(fdm));
	mapped_block = (fdm[2] << 8) | fdm[3];

	if (mapped_block == 0xffff)
		return false;

	return true;
}

static void mark_bad(int block)
{
	u8 fdm[4] = {0xff, 0xff, 0xff, 0xff};
	struct mtd_oob_ops ops = {
		.mode = MTD_OPS_PLACE_OOB,
		.ooboffs = 0,
		.ooblen = 4,
		.oobbuf = fdm,
		.datbuf = NULL,
		.len = 0,
	};
	int retlen;

	fdm[1] = 0x00; // we never create a bbt table entry ourself

	retlen = bmtd._write_oob(bmtd.mtd, block << bmtd.blk_shift , &ops) ;
	if (retlen < 0)
		pr_err("marking bad block failed\n");
}

// mark_bad_raw: only used for testing
static void mark_bad_raw(int block)
{
	u8 fdm[4] = {0xff, 0xff, 0xff, 0xff};
	struct mtd_oob_ops ops = {
		.mode = MTD_OPS_PLACE_OOB,
		.ooboffs = 0,
		.ooblen = 4,
		.oobbuf = fdm,
		.datbuf = NULL,
		.len = 0,
	};
	int retlen;

	pr_warn("marking bad: %d\n", block);

	fdm[0] = 0x00;

	retlen = bmtd._write_oob(bmtd.mtd, block << bmtd.blk_shift, &ops);
	if (retlen < 0)
		pr_err("marking bad block failed\n");
}

static void make_mapping(u16 from , u16 to)
{
	u8 fdm[4] = {0xff, 0xff, 0xff, 0xff};
	struct mtd_oob_ops ops = {
		.mode = MTD_OPS_PLACE_OOB,
		.ooboffs = 0,
		.ooblen = 4,
		.oobbuf = fdm,
		.datbuf = NULL,
		.len = 0,
	};
	int retlen;

	memcpy(fdm + 2, &to, sizeof(to)); // this has to be exactly like this

	retlen = bmtd._write_oob(bmtd.mtd, from << bmtd.blk_shift, &ops);
	if (retlen < 0)
		pr_err("marking bad block failed\n");
}

static u16 bbt_checksum(void)
{
	u8 *data = (u8 *) &bbt;
	int i;
	u16 checksum = 0;

	checksum += bbt.header.version;
	checksum += bbt.header.size;
	data += sizeof(struct bbt_table_header);

	for (i = 0; i < sizeof(bbt.table); i++)
		checksum += data[i];

	return checksum;
}

static bool parse_bbt(void)
{
	u8 fdm[4];
	int ret, i = system_blks;

	for (; i < total_blks; i++) {
		ret = bbt_nand_read(blk_pg(i), &bbt, sizeof(bbt), fdm, sizeof(fdm));
		if (ret == 0 && fdm[0] == 0xff && fdm[1] == 0xff &&
		    strncmp(bbt.header.signature , "RAWB", 4) == 0 &&
		    bbt.header.checksum == bbt_checksum()) {
			bbt_index = i;
			return true;
		}
	}

	return false;
}

static u8 bmt_checksum(void)
{
	u8 *data = (u8 *) &bmt;
	int i;
	u8 checksum = 0;

	checksum += bmt.header.version;
	checksum += bmt.header.size;

	data += sizeof(struct bmt_table_header);

	for (i = 0; i < bmt_blks * sizeof(struct bmt_entry); i++)
		checksum += data[i];

	return checksum;
}

static bool bmt_sanity_check(void)
{
	int i ;

	for (i = 0; i < bmt.header.size; i++) {
		if (bmt.table[i].to <= system_blks)
			return false;
	}

	return true;
}

static bool parse_bmt(void)
{
	u8 fdm[4];
	int ret, i = total_blks - 1;

	for (; i > system_blks; i--) {
		ret = bbt_nand_read(blk_pg(i), &bmt, sizeof(bmt), fdm, sizeof(fdm));
		if (ret == 0 && fdm[0] == 0xff && fdm[1] == 0xff &&
		    strncmp(bmt.header.signature , "BMT", 3) == 0 &&
		    bmt.header.checksum == bmt_checksum() && bmt_sanity_check()) {
			bmt_index = i;
			return true;
		}
	}

	return false;
}

// returns the index of a new empty block in bmt region , -1 in other cases
static int find_available_block(bool start_from_end)
{
	int i = system_blks, d = 1;
	int count = 0;

	if (start_from_end) {
		i = total_blks - 1;
		d = -1;
	}

	for (; count < (total_blks - system_blks); count++, i += d) {
		if (bmt_index == i || bbt_index == i  || is_bad(i) || is_mapped(i))
			continue;

		return i;
	}

	return -1;
}

static int bbt_nand_erase(u16 block)
{
	struct mtd_info *mtd = bmtd.mtd;
	struct erase_info instr = {
#ifdef NEED_ERASE_CALLBACK
		.mtd = mtd,
#endif
		.addr = (loff_t)block << bmtd.blk_shift,
		.len = bmtd.blk_size,
	};

	return bmtd._erase(mtd, &instr);
}

static void update_bmt_bbt(void)
{
	struct mtd_oob_ops ops, ops1;
	int retlen = 0;

	bbt.header.checksum = bbt_checksum();
	bmt.header.checksum = bmt_checksum();

	if (bbt_index == 0xffff)
		bbt_index = find_available_block(false);

	if (bmt_index == 0xffff)
		bmt_index = find_available_block(true);

	bbt_nand_erase(bmt_index);
	bbt_nand_erase(bbt_index);

	pr_info("putting back in bbt_index: %d, bmt_index: %d\n",
		bbt_index, bmt_index);

	ops = (struct mtd_oob_ops) {
		.mode = MTD_OPS_PLACE_OOB,
		.ooboffs = 0,
		.ooblen = 0,
		.oobbuf = NULL,
		.len = sizeof(bmt),
		.datbuf = (u8 *)&bmt,
	};

retry_bmt:
	retlen = bmtd._write_oob(bmtd.mtd, bmt_index << bmtd.blk_shift, &ops);
	if (retlen) {
		mark_bad(bmt_index);
		if (bmt_index > system_blks) {
			bmt_index--;
			goto retry_bmt;
		}
		return;
	}

	ops1 = (struct mtd_oob_ops) {
		.mode = MTD_OPS_PLACE_OOB,
		.ooboffs = 0,
		.ooblen = 0,
		.oobbuf = NULL,
		.len = sizeof(bbt),
		.datbuf = (u8 *)&bbt,
	};

retry_bbt:
	retlen = bmtd._write_oob(bmtd.mtd, bbt_index << bmtd.blk_shift, &ops1);
	if (retlen) {
		mark_bad(bbt_index);
		if (bbt_index < total_blks) {
			bbt_index++;
			goto retry_bbt;
		}
		return;
	}
}

static bool is_in_bmt(int block)
{
	int i;

	for (i = 0; i < bmt.header.size; i++) {
		if (bmt.table[i].from == block)
			return true;
	}

	return false;
}

static bool reconstruct_from_oob(void)
{
	int i;

	memset(&bmt, 0x00, sizeof(bmt));
	memcpy(&bmt.header.signature, "BMT", 3);

	bmt.header.version = 1;
	bmt.header.size = 0;

	for (i = total_blks - 1; i >= system_blks; i--) {
		u8 fdm[4];
		int ret;
		unsigned short mapped_block;

		if (is_bad(i))
			continue;

		ret = bbt_nand_read(blk_pg(i), bmtd.data_buf, bmtd.pg_size,
				    fdm, sizeof(fdm));
		if (ret < 0)
			mark_bad(i);

		memcpy(&mapped_block, fdm + 2, 2); // need to be this way
		if (mapped_block >= system_blks)
			continue;

		pr_warn("block %X was mapped to: %X\n", mapped_block, i);

		bmt.table[bmt.header.size++] = (struct bmt_entry)
					 { .from = mapped_block, .to = i };

		if (bmt.header.size > MAX_RAW_BAD_BLOCK_SIZE) {
			pr_err("bad block cound exceeded the maximum amount\n");
			return false;
		}
	}

	memset(&bbt, 0x00, sizeof(bbt));
	memcpy(&bbt. header.signature , "RAWB", 4);

	bbt.header.version  = 1;
	bbt.header.size = 0;

	for (i = 0 ; i < system_blks; i++) {
		if (is_bad_raw(i) && !is_in_bmt(i))
			bbt.table[bbt.header.size++] = (u16)i;

		if (bbt.header.size > MAX_RAW_BAD_BLOCK_SIZE) {
			pr_err("bad block cound exceeded the maximum amount\n");
			return false;
		}
	}

	bmt.header.checksum = bmt_checksum();
	bbt.header.checksum = bbt_checksum();

	update_bmt_bbt();

	pr_info("bbt and bmt reconstructed successfully\n");

	return true;
}

static bool remap_block(u16 block, u16 mapped_block, int copy_len)
{
	int retry_count = 10;
	int i, ret;
	u16 new_block;
	bool mapped_already_in_bmt;

retry:
	new_block = find_available_block(false);
	if (new_block < 0)
		return false;

	bbt_nand_erase(new_block);

	if (copy_len) {
		ret = bbt_nand_copy(new_block, mapped_block, copy_len);
		if (retry_count-- > 0 && ret) {
			if (ret == RESULT_FAIL_WRITE)
				mark_bad(new_block);
			goto retry;
		}
	}

	mapped_already_in_bmt = false;

	for (i = 0; i < bmt.header.size; i++) {
		if (bmt.table[i].from == block) {
			bmt.table[i].to = new_block;
			mapped_already_in_bmt = true;
			break;
		}
	}

	if (!mapped_already_in_bmt)
		bmt.table[bmt.header.size++] = (struct bmt_entry)
				{ .from = mapped_block, .to = new_block };

	if (mapped_block != block)
		mark_bad(mapped_block);

	if (mapped_block <= system_blks)
		mark_bad_raw(mapped_block);

	mark_bad_raw(block);
	make_mapping(new_block, mapped_block);

	update_bmt_bbt();

	return false;
}

static int bmt_init(struct mtd_info *mtd)
{
	u32 need_valid_block_num;
	int last_blk;
	int valid_blks = 0;
	int i = 0, j = 0, k = 0;

	total_blks = mtd->size >> bmtd.blk_shift;
	last_blk = total_blks - 1;

	need_valid_block_num = POOL_GOOD_BLOCK_PERCENT(total_blks);
	if (need_valid_block_num > MAX_BMT_SIZE)
		need_valid_block_num = MAX_BMT_SIZE;

	for (; last_blk > 0; last_blk--) {
		if (is_bad_raw(last_blk))
			continue;

		valid_blks++;
		if (valid_blks == need_valid_block_num)
			break;
	}

	if ((total_blks - last_blk) > MAX_BMT_PERCENT(total_blks)) {
		pr_err("cant find bmt area inside resevearea\n");
		return -1;
	}

	bmt_blks = total_blks - last_blk;
	if (bmt_blks > MAX_BMT_SIZE)
		bmt_blks = MAX_BMT_SIZE;

	pr_info("bmt pool size: %u\n", bmt_blks);

	system_blks = total_blks - bmt_blks;

	mtd->size = (total_blks - MAX_BMT_PERCENT(total_blks)) * mtd->erasesize;

	if (!(parse_bbt() && parse_bmt())) {
		if (!reconstruct_from_oob())
			return -1;
	} else
		pr_info("bmt/bbt found\n");

	/* generate pbbt this is like a map, for the evaluation of bbt table */
	for (i = 0; i < system_blks; i++) {
		if (j != bbt.header.size && bbt.table[j] == i) {
			j++;
			continue;
		}
		pbbt[k++] = i;
	}

	while (k != system_blks) {
		pbbt[k] = pbbt[k-1] + 1;
		k++;
	}

	return 0;
}

static int get_mapping_block(int block)
{
	int i, bbt_mapped_block;

	if (block > system_blks)
		return block;

	bbt_mapped_block = pbbt[block];

	for (i = 0; i < bmt.header.size; i++) {
		if (bmt.table[i].from == bbt_mapped_block)
			return bmt.table[i].to;
	}

	return bbt_mapped_block;
}

/* -------- Bad Blocks Management -------- */
static bool mapping_block_in_range(int block, int *start, int *end)
{
	const __be32 *cur = bmtd.remap_range;
	u32 addr = block << bmtd.blk_shift;
	int i;

	if (!cur || !bmtd.remap_range_len) {
		*start = 0;
		*end = total_blks;
		return true;
	}

	for (i = 0; i < bmtd.remap_range_len; i++, cur += 2) {
		if (addr < be32_to_cpu(cur[0]) || addr >= be32_to_cpu(cur[1]))
			continue;

		*start = be32_to_cpu(cur[0]);
		*end = be32_to_cpu(cur[1]);
		return true;
	}

	return false;
}

static bool
mtk_bmt_remap_block(u32 block, u32 mapped_block, int copy_len)
{
	int start, end;

	if (!mapping_block_in_range(block, &start, &end))
		return false;

	return remap_block(block, mapped_block, copy_len);
}

static int
airoha_bmt_read_oob(struct mtd_info *mtd, loff_t from,
		    struct mtd_oob_ops *ops)
{
	struct mtd_oob_ops cur_ops = *ops;
	int retry_count = 0;
	loff_t cur_from;
	int ret = 0;
	int max_bitflips = 0;

	ops->retlen = 0;
	ops->oobretlen = 0;

	while (ops->retlen < ops->len || ops->oobretlen < ops->ooblen) {
		u32 offset = from & (bmtd.blk_size - 1);
		u32 block = from >> bmtd.blk_shift;
		int cur_block;
		int cur_ret;

		cur_block = get_mapping_block(block);
		if (cur_block < 0)
			return -EIO;

		cur_from = ((loff_t)cur_block << bmtd.blk_shift) + offset;

		cur_ops.oobretlen = 0;
		cur_ops.retlen = 0;
		cur_ops.len = min_t(u32, mtd->erasesize - offset,
					 ops->len - ops->retlen);
		cur_ret = bmtd._read_oob(mtd, cur_from, &cur_ops);

		if (cur_ret < 0)
			ret = cur_ret;
		else
			max_bitflips = max_t(int, max_bitflips, cur_ret);

		if (cur_ret < 0 && !mtd_is_bitflip(cur_ret)) {
			if (mtk_bmt_remap_block(block, cur_block, mtd->erasesize) &&
				retry_count++ < 10)
				continue;

			goto out;
		}

		if (mtd->bitflip_threshold && cur_ret >= mtd->bitflip_threshold)
			mtk_bmt_remap_block(block, cur_block, mtd->erasesize);

		ops->retlen += cur_ops.retlen;
		ops->oobretlen += cur_ops.oobretlen;

		cur_ops.ooboffs = 0;
		cur_ops.datbuf += cur_ops.retlen;
		cur_ops.oobbuf += cur_ops.oobretlen;
		cur_ops.ooblen -= cur_ops.oobretlen;

		if (!cur_ops.len)
			cur_ops.len = mtd->erasesize - offset;

		from += cur_ops.len;
		retry_count = 0;
	}

out:
	if (ret < 0)
		return ret;

	return max_bitflips;
}

static int
airoha_bmt_write_oob(struct mtd_info *mtd, loff_t to,
		     struct mtd_oob_ops *ops)
{
	struct mtd_oob_ops cur_ops = *ops;
	int retry_count = 0;
	loff_t cur_to;
	int ret;

	ops->retlen = 0;
	ops->oobretlen = 0;

	while (ops->retlen < ops->len || ops->oobretlen < ops->ooblen) {
		u32 offset = to & (bmtd.blk_size - 1);
		u32 block = to >> bmtd.blk_shift;
		int cur_block;

		cur_block = get_mapping_block(block);
		if (cur_block < 0)
			return -EIO;

		cur_to = ((loff_t)cur_block << bmtd.blk_shift) + offset;

		cur_ops.oobretlen = 0;
		cur_ops.retlen = 0;
		cur_ops.len = min_t(u32, bmtd.blk_size - offset,
					 ops->len - ops->retlen);
		ret = bmtd._write_oob(mtd, cur_to, &cur_ops);
		if (ret < 0) {
			if (mtk_bmt_remap_block(block, cur_block, offset) &&
			    retry_count++ < 10)
				continue;

			return ret;
		}

		ops->retlen += cur_ops.retlen;
		ops->oobretlen += cur_ops.oobretlen;

		cur_ops.ooboffs = 0;
		cur_ops.datbuf += cur_ops.retlen;
		cur_ops.oobbuf += cur_ops.oobretlen;
		cur_ops.ooblen -= cur_ops.oobretlen;

		if (!cur_ops.len)
			cur_ops.len = mtd->erasesize - offset;

		to += cur_ops.len;
		retry_count = 0;
	}

	return 0;
}

static int
airoha_bmt_panic_write(struct mtd_info *mtd, loff_t to, size_t len,
			size_t *retlen, const u_char *buf)
{
	loff_t cur_to;
	size_t cur_len, cur_retlen;
	int ret;

	*retlen = 0;

	while (*retlen < len) {
		u32 offset = to & (bmtd.blk_size - 1);
		u32 block = to >> bmtd.blk_shift;
		int cur_block;

		cur_block = get_mapping_block(block);
		if (cur_block < 0)
			return -EIO;

		cur_to = ((loff_t)cur_block << bmtd.blk_shift) + offset;
		cur_len = min_t(u32, bmtd.blk_size - offset, len - *retlen);
		cur_retlen = 0;

		ret = bmtd._panic_write(mtd, cur_to, cur_len, &cur_retlen,
					buf + *retlen);
		if (ret < 0)
			return ret;

		*retlen += cur_retlen;

		if (!cur_len)
			cur_len = mtd->erasesize - offset;

		to += cur_len;
	}

	return 0;
}

static int
airoha_bmt_mtd_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct erase_info mapped_instr = {
#ifdef NEED_ERASE_CALLBACK
		.mtd = mtd,
#endif
		.len = bmtd.blk_size,
	};
	int retry_count = 0;
	u64 start_addr, end_addr;
	int ret = 0;
	int block;
	u32 orig_block;

#ifdef NEED_ERASE_CALLBACK
	instr->state = MTD_ERASING;
#endif

	start_addr = instr->addr & (~mtd->erasesize_mask);
	end_addr = instr->addr + instr->len;

	while (start_addr < end_addr) {
		orig_block = start_addr >> bmtd.blk_shift;
		block = get_mapping_block(orig_block);
		if (block < 0) {
			ret = -EIO;
			break;
		}

		mapped_instr.addr = (loff_t)block << bmtd.blk_shift;
		ret = bmtd._erase(mtd, &mapped_instr);
		if (ret) {
			if (mtk_bmt_remap_block(orig_block, block, 0) &&
			    retry_count++ < 10)
				continue;

			break;
		}

		start_addr += mtd->erasesize;
		retry_count = 0;
	}

#ifdef NEED_ERASE_CALLBACK
	instr->state = (ret == 0) ? MTD_ERASE_DONE : MTD_ERASE_FAILED;
#endif

	if (ret == 0) {
#ifdef NEED_ERASE_CALLBACK
		mtd_erase_callback(instr);
#endif
	} else
		instr->fail_addr = start_addr;

	return ret;
}

static int airoha_bmt_block_isbad(struct mtd_info *mtd, loff_t ofs)
{
	int retry_count = 0;
	u32 orig_block = ofs >> bmtd.blk_shift;
	u16 block;
	int ret;

retry:
	block = get_mapping_block(orig_block);
	ret = bmtd._block_isbad(mtd, (loff_t)block << bmtd.blk_shift);
	if (ret) {
		if (mtk_bmt_remap_block(orig_block, block, bmtd.blk_size) &&
		    retry_count++ < 10)
			goto retry;
	}

	return ret;
}

static int airoha_bmt_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	u32 orig_block = ofs >> bmtd.blk_shift;
	int block;

	block = get_mapping_block(orig_block);
	if (block < 0)
		return -EIO;

	mtk_bmt_remap_block(orig_block, block, bmtd.blk_size);

	return bmtd._block_markbad(mtd, (loff_t)block << bmtd.blk_shift);
}

void airoha_bmt_detach(struct mtd_info *mtd)
{
	if (bmtd.mtd != mtd)
		return;

	kfree(bmtd.data_buf);

	/* do not restore original mtd size to prevent vanish BMT */

	mtd->_read_oob = bmtd._read_oob;
	mtd->_write_oob = bmtd._write_oob;
	mtd->_panic_write = bmtd._panic_write;
	mtd->_erase = bmtd._erase;
	mtd->_block_isbad = bmtd._block_isbad;
	mtd->_block_markbad = bmtd._block_markbad;

	memset(&bmtd, 0, sizeof(bmtd));
}

int airoha_bmt_attach(struct mtd_info *mtd)
{
	struct device_node *np;
	int ret;

	if (bmtd.mtd)
		return -ENOSPC;

	np = mtd_get_of_node(mtd);
	if (!np)
		return 0;

	if (!of_property_read_bool(np, "airoha,bmt")) {
		of_node_put(np);
		return 0;
	}

	bmtd.remap_range = of_get_property(np, "airoha,bmt-remap-range",
					   &bmtd.remap_range_len);
	bmtd.remap_range_len /= 8;

	of_node_put(np);

	bmtd.data_buf = kmalloc(mtd->writesize + mtd->oobsize, GFP_KERNEL);
	if (!bmtd.data_buf)
		return -ENOMEM;

	memset(bmtd.data_buf, 0xff, mtd->writesize + mtd->oobsize);

	bmtd.blk_size = mtd->erasesize;
	bmtd.blk_shift = ffs(bmtd.blk_size) - 1;
	bmtd.pg_size = mtd->writesize;
	bmtd.pg_shift = ffs(bmtd.pg_size) - 1;

	bmtd.mtd = mtd;
	bmtd._read_oob = mtd->_read_oob;
	bmtd._write_oob = mtd->_write_oob;
	bmtd._erase = mtd->_erase;
	bmtd._block_isbad = mtd->_block_isbad;
	bmtd._block_markbad = mtd->_block_markbad;
	bmtd._panic_write = mtd->_panic_write;

	mtd->_read_oob = airoha_bmt_read_oob;
	mtd->_write_oob = airoha_bmt_write_oob;
	mtd->_erase = airoha_bmt_mtd_erase;
	mtd->_block_isbad = airoha_bmt_block_isbad;
	mtd->_block_markbad = airoha_bmt_block_markbad;

	if (mtd->_panic_write)
		mtd->_panic_write = airoha_bmt_panic_write;

	ret = bmt_init(mtd);
	if (ret) {
		airoha_bmt_detach(mtd);
		return ret;
	}

	return 0;
}
