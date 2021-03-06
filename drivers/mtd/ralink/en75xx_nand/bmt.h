#ifndef _EN75XX_BMT_H
#define _EN75XX_BMT_H

#include "spi_nand_flash.h"

//#define RABMT_DEBUG

#define MAX_RAW_BAD_BLOCK_SIZE		(250)
#define MAX_BMT_SIZE			(250)

#define BBT_SIGNATURE_OFFSET		(0)
#define BBT_VERSION			1

#define BAD_BLOCK_RAW			(0)
#define BMT_BADBLOCK_GENERATE_LATER	(1)

#define BMT_VERSION			(1)	// initial version

#define MAIN_SIGNATURE_OFFSET		(0)
#define OOB_INDEX_OFFSET		(2)
#define OOB_INDEX_SIZE			(2)

typedef struct _bmt_entry_
{
	u16 bad_index;			// bad block index
	u16 mapped_index;		// mapping block index in the replace pool
} bmt_entry;

typedef enum
{
	UPDATE_ERASE_FAIL,
	UPDATE_WRITE_FAIL,
	UPDATE_UNMAPPED_BLOCK,
	UPDATE_REASON_COUNT,
} update_reason_t;

typedef struct {
	bmt_entry table[MAX_BMT_SIZE];
	u8 version;
	u8 mapped_count;		// mapped block count in pool
	u8 bad_count;			// bad block count in pool. Not used in V1
} bmt_struct;

typedef struct {
	u16 badblock_table[MAX_RAW_BAD_BLOCK_SIZE];	//store bad block raw
	u8 version;
	u8 badblock_count;
	u8 reserved[2];
} init_bbt_struct;

/***************************************************************
*                                                              *
* Interface BMT need to use                                    *
*                                                              *
***************************************************************/
extern int en7512_nand_exec_read_page(u32 page, u8* date, u8* oob);
extern int en7512_nand_check_block_bad(u32 offset, u32 bmt_block);
extern int en7512_nand_erase(u32 offset);
extern int en7512_nand_mark_badblock(u32 offset, u32 bmt_block);
extern int en7512_nand_exec_write_page(u32 page, u8 *dat, u8 *oob);

/********************************************
*                                           *
* Interface for preloader/uboot/kernel      *
*                                           *
********************************************/
extern void set_bad_index_to_oob(u8 *oob, u16 index);
extern bmt_struct *init_bmt(struct nand_chip *chip, int size);
extern init_bbt_struct* start_init_bbt(void);
extern int write_bbt_or_bmt_to_flash(void);
extern int create_badblock_table_by_bbt(void);
extern bool update_bmt(u32 offset, update_reason_t reason, u8 *dat, u8 *oob);
extern int get_mapping_block_index_by_bmt(int index);
extern int get_mapping_block_index_by_bbt(int index);
extern int get_mapping_block_index(int index, u16 *phy_block_bbt);
extern int block_is_in_bmt_region(int index);

#endif /* _EN75XX_BMT_H */
