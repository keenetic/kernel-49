 /***************************************************************************************
 *      Copyright(c) 2014 ECONET Incorporation All rights reserved.
 *
 *      This is unpublished proprietary source code of ECONET Incorporation
 *
 *      The copyright notice above does not evidence any actual or intended
 *      publication of such source code.
 ***************************************************************************************
 */

/*======================================================================================
 * MODULE NAME: spi
 * FILE NAME: spi_nand_flash.h
 * DATE: 2014/11/21
 * VERSION: 1.00
 * PURPOSE: To Provide SPI NAND Access interface.
 * NOTES:
 *
 * AUTHOR : Chuck Kuo         REVIEWED by
 *
 * DEPENDENCIES
 *
 * * $History: $
 * MODIFICTION HISTORY:
 * Version 1.00 - Date 2014/11/21 By Chuck Kuo
 * ** This is the first versoin for creating to support the functions of
 *    current module.
 *
 *======================================================================================
 */

#ifndef __SPI_NAND_FLASH_H__
#define __SPI_NAND_FLASH_H__

/* INCLUDE FILE DECLARATIONS --------------------------------------------------------- */
#include <asm/types.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>

//#define SPI_NAND_FLASH_DEBUG

/* MACRO DECLARATIONS ---------------------------------------------------------------- */
#define SPI_NAND_FLASH_OOB_FREE_ENTRY_MAX	32

/* TYPE DECLARATIONS ----------------------------------------------------------------- */
typedef enum {
	SPI_NAND_FLASH_READ_DUMMY_BYTE_PREPEND,
	SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,

	SPI_NAND_FLASH_READ_DUMMY_BYTE_DEF_NO
} SPI_NAND_FLASH_READ_DUMMY_BYTE_T;

typedef enum {
	SPI_NAND_FLASH_RTN_NO_ERROR =0,
	SPI_NAND_FLASH_RTN_PROBE_ERROR,
	SPI_NAND_FLASH_RTN_ALIGNED_CHECK_FAIL,
	SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK,
	SPI_NAND_FLASH_RTN_ERASE_FAIL,
	SPI_NAND_FLASH_RTN_PROGRAM_FAIL,

	SPI_NAND_FLASH_RTN_DEF_NO
} SPI_NAND_FLASH_RTN_T;

typedef enum {
	SPI_NAND_FLASH_READ_SPEED_MODE_SINGLE = 0,
	SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
	SPI_NAND_FLASH_READ_SPEED_MODE_QUAD,

	SPI_NAND_FLASH_READ_SPEED_MODE_DEF_NO
} SPI_NAND_FLASH_READ_SPEED_MODE_T;


typedef enum {
	SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE = 0,
	SPI_NAND_FLASH_WRITE_SPEED_MODE_QUAD,

	SPI_NAND_FLASH_WRITE_SPEED_MODE_DEF_NO
} SPI_NAND_FLASH_WRITE_SPEED_MODE_T;

typedef enum {
	SPI_NAND_FLASH_DEBUG_LEVEL_0 = 0,
	SPI_NAND_FLASH_DEBUG_LEVEL_1,
	SPI_NAND_FLASH_DEBUG_LEVEL_2,

	SPI_NAND_FLASH_DEBUG_LEVEL_DEF_NO
} SPI_NAND_FLASH_DEBUG_LEVEL_T;

/* Bitwise */
#define SPI_NAND_FLASH_FEATURE_NONE		( 0x00 )
#define SPI_NAND_FLASH_PLANE_SELECT_HAVE	( 0x01 << 0 )
#define SPI_NAND_FLASH_DIE_SELECT_1_HAVE	( 0x01 << 1 )
#define SPI_NAND_FLASH_DIE_SELECT_2_HAVE	( 0x01 << 2 )

struct spi_nand_flash_oobfree {
	unsigned long offset;
	unsigned long len;
};

struct spi_nand_flash_ooblayout {
	unsigned long oobsize;
	struct spi_nand_flash_oobfree oobfree[SPI_NAND_FLASH_OOB_FREE_ENTRY_MAX];
};

struct SPI_NAND_FLASH_INFO_T {
	char					*ptr_name;
	u8					mfr_id;
	u8					dev_id;
	u32					device_size;	/* Flash total Size */
	u32					page_size;	/* Page Size 		*/
	u32					erase_size;	/* Block Size 		*/
	u32					oob_size;	/* Spare Area (OOB) Size */
	SPI_NAND_FLASH_READ_DUMMY_BYTE_T	dummy_mode;
	SPI_NAND_FLASH_READ_SPEED_MODE_T	read_mode;
	SPI_NAND_FLASH_WRITE_SPEED_MODE_T	write_mode;
	struct spi_nand_flash_ooblayout		*oob_free_layout;
	u32					feature;
};

struct spinand_state {
	uint32_t		col;
	uint32_t		row;
	int			buf_idx;
	u8			*buf;
	uint32_t		buf_len;
	int			oob_idx;
	u8 			*oob_buf;
	uint32_t		oob_buf_len;
	uint32_t		command;
};

struct en75xx_spinand_host {
	struct nand_chip	nand_chip;
	struct mtd_info		*mtd;
	struct spinand_state	state;
	void			*priv;
};

#endif /* ifndef __SPI_NAND_FLASH_H__ */

/* End of [spi_nand_flash.h] package */
