/***************************************************************************************
 *      Copyright(c) 2014 ECONET Incorporation All rights reserved.
 *
 *      This is unpublished proprietary source code of ECONET Networks Incorporation
 *
 *      The copyright notice above does not evidence any actual or intended
 *      publication of such source code.
 ***************************************************************************************
 */

/*======================================================================================
 * MODULE NAME: spi
 * FILE NAME: spi_nand_flash.c
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

/* INCLUDE FILE DECLARATIONS --------------------------------------------------------- */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/jiffies.h>

#include <asm/io.h>
#include <asm/tc3162/tc3162.h>

#include "../spi_controller.h"
#if defined(TCSUPPORT_NAND_BMT) && !defined(LZMA_IMG)
#include "../bmt.h"
#endif

#include "spi_nand_flash.h"

/* enable flash chip HW ECC */
#define SPINAND_ONDIEECC

#if IS_ENABLED(CONFIG_MTD_UBI)
#define UBIFS_BLANK_PAGE_FIXUP
#endif

/* NAMING CONSTANT DECLARATIONS ------------------------------------------------------ */

/* SPI NAND Command Set */
#define _SPI_NAND_OP_GET_FEATURE			0x0F	/* Get Feature */
#define _SPI_NAND_OP_SET_FEATURE			0x1F	/* Set Feature */
#define _SPI_NAND_OP_PAGE_READ				0x13	/* Load page data into cache of SPI NAND chip */
#define _SPI_NAND_OP_READ_FROM_CACHE_SINGLE		0x03	/* Read data from cache of SPI NAND chip, single speed*/
#define _SPI_NAND_OP_READ_FROM_CACHE_DUAL		0x3B	/* Read data from cache of SPI NAND chip, dual speed*/
#define _SPI_NAND_OP_READ_FROM_CACHE_QUAD		0x6B	/* Read data from cache of SPI NAND chip, quad speed*/
#define _SPI_NAND_OP_WRITE_ENABLE			0x06	/* Enable write data to  SPI NAND chip */
#define _SPI_NAND_OP_WRITE_DISABLE			0x04	/* Reseting the Write Enable Latch (WEL) */
#define _SPI_NAND_OP_PROGRAM_LOAD_SINGLE		0x02	/* Write data into cache of SPI NAND chip with cache reset, single speed */
#define _SPI_NAND_OP_PROGRAM_LOAD_QUAD			0x32	/* Write data into cache of SPI NAND chip with cache reset, quad speed */
#define _SPI_NAND_OP_PROGRAM_LOAD_RAMDOM_SINGLE		0x84	/* Write data into cache of SPI NAND chip, single speed */
#define _SPI_NAND_OP_PROGRAM_LOAD_RAMDON_QUAD		0x34	/* Write data into cache of SPI NAND chip, quad speed */

#define _SPI_NAND_OP_PROGRAM_EXECUTE			0x10	/* Write data from cache into SPI NAND chip */
#define _SPI_NAND_OP_READ_ID				0x9F	/* Read Manufacture ID and Device ID */
#define _SPI_NAND_OP_BLOCK_ERASE			0xD8	/* Erase Block */
#define _SPI_NAND_OP_RESET				0xFF	/* Reset */
#define _SPI_NAND_OP_DIE_SELECT				0xC2	/* Die Select */

/* SPI NAND register address of command set */
#define _SPI_NAND_ADDR_ECC				0x90	/* Address of ECC Config */
#define _SPI_NAND_ADDR_PROTECTION			0xA0	/* Address of protection */
#define _SPI_NAND_ADDR_FEATURE				0xB0	/* Address of feature */
#define _SPI_NAND_ADDR_STATUS				0xC0	/* Address of status */
#define _SPI_NAND_ADDR_FEATURE_4			0xD0	/* Address of status 4 */
#define _SPI_NAND_ADDR_STATUS_5				0xE0	/* Address of status 5 */
#define _SPI_NAND_ADDR_MANUFACTURE_ID			0x00	/* Address of Manufacture ID */
#define _SPI_NAND_ADDR_DEVICE_ID			0x01	/* Address of Device ID */

/* SPI NAND value of register address of command set */
#define _SPI_NAND_VAL_DISABLE_PROTECTION		0x0	/* Value for disable write protection */
#define _SPI_NAND_VAL_ENABLE_PROTECTION			0x38	/* Value for enable write protection */
#define _SPI_NAND_VAL_OIP				0x1	/* OIP = Operaton In Progress */
#define _SPI_NAND_VAL_ERASE_FAIL			0x4	/* E_FAIL = Erase Fail */
#define _SPI_NAND_VAL_PROGRAM_FAIL			0x8	/* P_FAIL = Program Fail */

/* Others Define */
#define _SPI_NAND_LEN_ONE_BYTE				(1)
#define _SPI_NAND_LEN_TWO_BYTE				(2)
#define _SPI_NAND_LEN_THREE_BYTE			(3)
#define _SPI_NAND_BLOCK_ROW_ADDRESS_OFFSET		(6)

#define _SPI_NAND_PAGE_SIZE  				4096
#define _SPI_NAND_OOB_SIZE  				256
#define _SPI_NAND_CACHE_SIZE 				(_SPI_NAND_PAGE_SIZE+_SPI_NAND_OOB_SIZE)

#define _SPI_FREQUENCY_ADJUST_REG			0xBFA200CC

#define LINUX_USE_OOB_START_OFFSET			4
#define MAX_LINUX_USE_OOB_SIZE				26
#define MAX_USE_OOB_SIZE				(LINUX_USE_OOB_START_OFFSET + MAX_LINUX_USE_OOB_SIZE + 2)

#define EMPTY_DATA					(0)
#define NONE_EMPTY_DATA					(1)
#define EMPTY_OOB					(0)
#define NONE_EMPTY_OOB					(1)

#define MIN(a,b)					((a) < (b) ? (a) : (b))

/* MACRO DECLARATIONS ---------------------------------------------------------------- */
#define _SPI_NAND_BLOCK_ALIGNED_CHECK( __addr__,__block_size__) ((__addr__) & ( __block_size__ - 1))
#define _SPI_NAND_GET_DEVICE_INFO_PTR			&(_current_flash_info_t)

/* Porting Replacement */
#define _SPI_NAND_PRINTF				printk
#if defined(SPI_NAND_FLASH_DEBUG)
#define _SPI_NAND_DEBUG_PRINTF				spi_nand_flash_debug_printf
#define _SPI_NAND_DEBUG_PRINTF_ARRAY			spi_nand_flash_debug_printf_array
#else
#define _SPI_NAND_DEBUG_PRINTF(args...)
#define _SPI_NAND_DEBUG_PRINTF_ARRAY(args...)
#endif
#define _SPI_NAND_ENABLE_MANUAL_MODE			SPI_CONTROLLER_Enable_Manual_Mode
#define _SPI_NAND_WRITE_ONE_BYTE			SPI_CONTROLLER_Write_One_Byte
#define _SPI_NAND_WRITE_NBYTE				SPI_CONTROLLER_Write_NByte
#define _SPI_NAND_READ_NBYTE				SPI_CONTROLLER_Read_NByte
#define _SPI_NAND_READ_CHIP_SELECT_HIGH			SPI_CONTROLLER_Chip_Select_High
#define _SPI_NAND_READ_CHIP_SELECT_LOW			SPI_CONTROLLER_Chip_Select_Low

#define IOMUX_CONTROL1					(0xBFA20104)

/* STATIC VARIABLE DECLARATIONS ------------------------------------------------------ */

#ifdef UBIFS_BLANK_PAGE_FIXUP
typedef enum{
	SPI_NAND_FLASH_UBIFS_BLANK_PAGE_ECC_MATCH = 0,
	SPI_NAND_FLASH_UBIFS_BLANK_PAGE_ECC_MISMATCH,
} UBIFS_BLANK_PAGE_ECC_T;

typedef enum{
	SPI_NAND_FLASH_UBIFS_BLANK_PAGE_FIXUP_SUCCESS = 0,
	SPI_NAND_FLASH_UBIFS_BLANK_PAGE_FIXUP_FAIL,
} UBIFS_BLANK_PAGE_FIXUP_T;

static SPI_NAND_FLASH_RTN_T spi_nand_write_page(u32 page_number,
						u32 data_offset,
						u8  *ptr_data,
						u32 data_len,
						u32 oob_offset,
						u8  *ptr_oob,
						u32 oob_len,
						SPI_NAND_FLASH_WRITE_SPEED_MODE_T speed_mode );
#endif

static struct mtd_info *g_en75xx_mtd;

#if defined(TCSUPPORT_NAND_BMT) && !defined(LZMA_IMG)
extern int nand_logic_size;
static bmt_struct *g_bmt;
static init_bbt_struct *g_bbt;
#endif

static struct SPI_NAND_FLASH_INFO_T _current_flash_info_t;	/* Store the current flash information */
static u32 _current_page_num = 0xFFFFFFFF;
static u8  _current_cache_page[_SPI_NAND_CACHE_SIZE];
static u8  _current_cache_page_data[_SPI_NAND_PAGE_SIZE];
static u8  _current_cache_page_oob[_SPI_NAND_OOB_SIZE];
static u8  _current_cache_page_oob_mapping[_SPI_NAND_OOB_SIZE];

static unsigned char _die_id;
static unsigned char _ondie_ecc_flag = 1;
static unsigned char _plane_select_bit;

#include "spi_nand_table.c"

/* LOCAL SUBPROGRAM BODIES------------------------------------------------------------ */
#if defined(SPI_NAND_FLASH_DEBUG)
static SPI_NAND_FLASH_DEBUG_LEVEL_T _SPI_NAND_DEBUG_LEVEL = SPI_NAND_FLASH_DEBUG_LEVEL_0;

static void spi_nand_flash_debug_printf( SPI_NAND_FLASH_DEBUG_LEVEL_T DEBUG_LEVEL, char *fmt, ... )
{
	if( _SPI_NAND_DEBUG_LEVEL >= DEBUG_LEVEL )
	{
		unsigned char str_buf[100];
		va_list argptr;
		int cnt;

		va_start(argptr, fmt);
		cnt = vsprintf(str_buf, fmt, argptr);
		va_end(argptr);

		_SPI_NAND_PRINTF("%s", str_buf);
	}
}

static void spi_nand_flash_debug_printf_array( SPI_NAND_FLASH_DEBUG_LEVEL_T DEBUG_LEVEL, const u8 *buf, u32 len )
{
	u32 idx_for_debug;

	if( _SPI_NAND_DEBUG_LEVEL >= DEBUG_LEVEL )
	{
		for(idx_for_debug=0; idx_for_debug< len; idx_for_debug++)
		{
			if( ((idx_for_debug) %8 == 0) )
			{
				_SPI_NAND_PRINTF("\n%04x: ", (idx_for_debug));
			}
			_SPI_NAND_PRINTF("%02x ", buf[idx_for_debug]);
		}
		_SPI_NAND_PRINTF("\n");
	}
}
#endif

static SPI_NAND_FLASH_RTN_T spi_nand_protocol_reset( void )
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	/* 1. Chip Select low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Send FFh opcode (Reset) */
	_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_SET_FEATURE );

	/* 5. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_protocol_reset\n");

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_set_feature( u8 addr, u8 protection )
 * PURPOSE : To implement the SPI nand protocol for set status register.
 * AUTHOR  : 
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : addr - register address
 *           data - The variable of this register.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2017/5/26.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_protocol_set_feature( u8 addr, u8 data )
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	/* 1. Chip Select low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Send 0Fh opcode (Set Feature) */
	_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_SET_FEATURE );

	/* 3. Offset addr */
	_SPI_NAND_WRITE_ONE_BYTE( addr );

	/* 4. Write new setting */
	_SPI_NAND_WRITE_ONE_BYTE( data );

	/* 5. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_protocol_set_feature %x: val=0x%x\n", addr, data);

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_get_feature( u8 addr, u8 *ptr_rtn_data )
 * PURPOSE : To implement the SPI nand protocol for get status register.
 * AUTHOR  :
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : addr - register address
 *   OUTPUT: ptr_rtn_protection  - A pointer to the ptr_rtn_protection variable.
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2017/5/26.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_protocol_get_feature( u8 addr, u8 *ptr_rtn_data )
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	/* 1. Chip Select low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Send 0Fh opcode (Get Feature) */
	_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_GET_FEATURE );

	/* 3. Offset addr */
	_SPI_NAND_WRITE_ONE_BYTE( addr );

	/* 4. Read 1 byte data */
	_SPI_NAND_READ_NBYTE( ptr_rtn_data, _SPI_NAND_LEN_ONE_BYTE, SPI_CONTROLLER_SPEED_SINGLE);

	/* 5. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_protocol_get_feature %x: val=0x%x\n", addr, *ptr_rtn_data);

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_set_status_reg_1( u8    protection )
 * PURPOSE : To implement the SPI nand protocol for set status register 1.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : protection - The protection variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/17 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static inline SPI_NAND_FLASH_RTN_T spi_nand_protocol_set_status_reg_1( u8 protection )
{
	/* Offset addr of protection (0xA0) */
	return spi_nand_protocol_set_feature(_SPI_NAND_ADDR_PROTECTION, protection);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_get_status_reg_1( u8    *ptr_rtn_protection )
 * PURPOSE : To implement the SPI nand protocol for get status register 1.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : None
 *   OUTPUT: ptr_rtn_protection  - A pointer to the ptr_rtn_protection variable.
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/17 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static inline SPI_NAND_FLASH_RTN_T spi_nand_protocol_get_status_reg_1( u8 *ptr_rtn_protection )
{
	/* Offset addr of protection (0xA0) */
	return spi_nand_protocol_get_feature(_SPI_NAND_ADDR_PROTECTION, ptr_rtn_protection);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_set_status_reg_2( u8   feature )
 * PURPOSE : To implement the SPI nand protocol for set status register 2.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : feature - The feature variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2015/01/21 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static inline SPI_NAND_FLASH_RTN_T spi_nand_protocol_set_status_reg_2( u8 feature )
{
	/* Offset addr of feature (0xB0) */
	return spi_nand_protocol_set_feature(_SPI_NAND_ADDR_FEATURE, feature);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_get_status_reg_2( u8   *ptr_rtn_feature )
 * PURPOSE : To implement the SPI nand protocol for get status register 2.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : None
 *   OUTPUT: ptr_rtn_feature  - A pointer to the ptr_rtn_feature variable.
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2015/01/21 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static inline SPI_NAND_FLASH_RTN_T spi_nand_protocol_get_status_reg_2( u8 *ptr_rtn_feature )
{
	/* Offset addr of protection (0xB0) */
	return spi_nand_protocol_get_feature(_SPI_NAND_ADDR_FEATURE, ptr_rtn_feature);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: SPI_NAND_FLASH_RTN_T spi_nand_protocol_get_status_reg_3( u8 *ptr_rtn_status )
 * PURPOSE : To implement the SPI nand protocol for get status register 3.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : stats_reg - The stats_reg variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/16 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static inline SPI_NAND_FLASH_RTN_T spi_nand_protocol_get_status_reg_3( u8 *ptr_rtn_status )
{
	/* Offset addr of status (0xC0) */
	return spi_nand_protocol_get_feature(_SPI_NAND_ADDR_STATUS, ptr_rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_set_status_reg_4( u8 feature )
 * PURPOSE : To implement the SPI nand protocol for set status register 4.
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : feature - The feature variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 *
 *------------------------------------------------------------------------------------
 */
static inline SPI_NAND_FLASH_RTN_T spi_nand_protocol_set_status_reg_4( u8 feature )
{
	/* Offset addr of feature 4 (0xD0) */
	return spi_nand_protocol_set_feature(_SPI_NAND_ADDR_FEATURE_4, feature);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_get_status_reg_4( u8 *ptr_rtn_status )
 * PURPOSE : To implement the SPI nand protocol for get status register 4.
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : None
 *   OUTPUT: ptr_rtn_status  - A pointer to the ptr_rtn_status variable.
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 *
 *------------------------------------------------------------------------------------
 */
static inline SPI_NAND_FLASH_RTN_T spi_nand_protocol_get_status_reg_4( u8 *ptr_rtn_status )
{
	/* Offset addr of feature 4 (0xD0) */
	return spi_nand_protocol_get_feature(_SPI_NAND_ADDR_FEATURE_4, ptr_rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_write_enable( void )
 * PURPOSE : To implement the SPI nand protocol for write enable.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : None
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/17 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_protocol_write_enable( void )
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	/* 1. Chip Select Low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Write op_cmd 0x06 (Write Enable (WREN)*/
	_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_WRITE_ENABLE );

	/* 3. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_write_disable( void )
 * PURPOSE : To implement the SPI nand protocol for write disable.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : None
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/17 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_protocol_write_disable( void )
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	/* 1. Chip Select Low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Write op_cmd 0x04 (Write Disable (WRDI)*/
	_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_WRITE_DISABLE );

	/* 3. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_block_erase( u32   block_idx )
 * PURPOSE : To implement the SPI nand protocol for block erase.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : block_idx - The block_idx variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/17 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_protocol_block_erase( u32 block_idx )
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
	u8 cmd[4];

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_protocol_block_erase : block idx =0x%x\n", block_idx);

	/* 1. Chip Select Low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Write op_cmd 0xD8 (Block Erase) */
	cmd[0] = _SPI_NAND_OP_BLOCK_ERASE;

	/* 3. Write block number */
	block_idx <<= _SPI_NAND_BLOCK_ROW_ADDRESS_OFFSET;	/* Row Address format in SPI NAND chip */
	cmd[1] = (block_idx >> 16) & 0xff;	/* dummy byte */
	cmd[2] = (block_idx >>  8) & 0xff;
	cmd[3] =  block_idx & 0xff;

	_SPI_NAND_WRITE_NBYTE(cmd, 4, SPI_CONTROLLER_SPEED_SINGLE);

	/* 4. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_read_id( struct SPI_NAND_FLASH_INFO_T *ptr_rtn_flash_id )
 * PURPOSE : To implement the SPI nand protocol for read id.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : None
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/12 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_protocol_read_id ( struct SPI_NAND_FLASH_INFO_T *ptr_rtn_flash_id )
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	/* 1. Chip Select Low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Write op_cmd 0x9F (Read ID) */
	_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_READ_ID );

	/* 3. Write Address Byte (0x00) */
	_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_ADDR_MANUFACTURE_ID );

	/* 4. Read data (Manufacture ID and Device ID) */
	_SPI_NAND_READ_NBYTE( &(ptr_rtn_flash_id->mfr_id), _SPI_NAND_LEN_ONE_BYTE, SPI_CONTROLLER_SPEED_SINGLE);
	_SPI_NAND_READ_NBYTE( &(ptr_rtn_flash_id->dev_id), _SPI_NAND_LEN_ONE_BYTE, SPI_CONTROLLER_SPEED_SINGLE);

	/* 5. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_protocol_read_id : mfr_id=0x%x, dev_id=0x%x\n", ptr_rtn_flash_id->mfr_id, ptr_rtn_flash_id->dev_id);

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_read_id_2( struct SPI_NAND_FLASH_INFO_T *ptr_rtn_flash_id )
 * PURPOSE : To implement the SPI nand protocol for read id.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : None
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/12 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_protocol_read_id_2 ( struct SPI_NAND_FLASH_INFO_T *ptr_rtn_flash_id )
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	/* 1. Chip Select Low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Write op_cmd 0x9F (Read ID) */
	_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_READ_ID );

	/* 3. Read data (Manufacture ID and Device ID) */
	_SPI_NAND_READ_NBYTE( &(ptr_rtn_flash_id->mfr_id), _SPI_NAND_LEN_ONE_BYTE, SPI_CONTROLLER_SPEED_SINGLE);
	_SPI_NAND_READ_NBYTE( &(ptr_rtn_flash_id->dev_id), _SPI_NAND_LEN_ONE_BYTE, SPI_CONTROLLER_SPEED_SINGLE);

	/* 4. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_protocol_read_id_2 : mfr_id=0x%x, dev_id=0x%x\n", ptr_rtn_flash_id->mfr_id, ptr_rtn_flash_id->dev_id);

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_page_read( u32    page_number )
 * PURPOSE : To implement the SPI nand protocol for page read.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : page_number - The page_number variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/17 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_protocol_page_read ( u32 page_number )
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
	u8 cmd[4];

	/* 1. Chip Select low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Send 13h opcode */
	cmd[0] = _SPI_NAND_OP_PAGE_READ;

	/* 3. Send page number */
	cmd[1] = (page_number >> 16) & 0xff;
	cmd[2] = (page_number >>  8) & 0xff;
	cmd[3] =  page_number & 0xff;

	_SPI_NAND_WRITE_NBYTE(cmd, 4, SPI_CONTROLLER_SPEED_SINGLE);

	/* 4. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_load_page_into_cache: value=0x%x\n", ((page_number ) & 0xff) );

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_read_from_cache( u32  data_offset,
 *                                                                          u32  len,
 *                                                                          u8   *ptr_rtn_buf,
 *									u32	 read_mode,
 *									SPI_NAND_FLASH_READ_DUMMY_BYTE_T dummy_mode)
 * PURPOSE : To implement the SPI nand protocol for read from cache with single speed.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : data_offset  - The data_offset variable of this function.
 *           len          - The len variable of this function.
 *   OUTPUT: ptr_rtn_buf  - A pointer to the ptr_rtn_buf variable.
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_protocol_read_from_cache( u32 data_offset, 
								u32 len, 
								u8 *ptr_rtn_buf, 
								u32 read_mode,
								SPI_NAND_FLASH_READ_DUMMY_BYTE_T dummy_mode )
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	/* 1. Chip Select low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Send opcode */
	switch (read_mode)
	{
		/* 03h */
		case SPI_NAND_FLASH_READ_SPEED_MODE_SINGLE:
			_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_READ_FROM_CACHE_SINGLE );
			break;

		/* 3Bh */
		case SPI_NAND_FLASH_READ_SPEED_MODE_DUAL:
			_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_READ_FROM_CACHE_DUAL );
			break;

		/* 6Bh */
		case SPI_NAND_FLASH_READ_SPEED_MODE_QUAD:
			_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_READ_FROM_CACHE_QUAD );
			break;

		default:
			break;
	}

	/* 3. Send data_offset addr */
	if( dummy_mode == SPI_NAND_FLASH_READ_DUMMY_BYTE_PREPEND )
	{
		_SPI_NAND_WRITE_ONE_BYTE( (0xff) );						/* dummy byte */
	}

	if( ((ptr_dev_info_t->feature) & SPI_NAND_FLASH_PLANE_SELECT_HAVE) )
	{
		if( _plane_select_bit == 0)
		{
			_SPI_NAND_WRITE_ONE_BYTE( ((data_offset >> 8 ) &(0xef)) );
		}
		if( _plane_select_bit == 1)
		{
			_SPI_NAND_WRITE_ONE_BYTE( ((data_offset >> 8 ) | (0x10)) );
		}				
	}
	else
	{
		_SPI_NAND_WRITE_ONE_BYTE( ((data_offset >> 8 ) &(0xff)) );
	}

	_SPI_NAND_WRITE_ONE_BYTE( ((data_offset      ) &(0xff)) );	

	if( dummy_mode == SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND )
	{
		_SPI_NAND_WRITE_ONE_BYTE( (0xff) );						/* dummy byte */
	}

	if( dummy_mode == SPI_NAND_FLASH_READ_DUMMY_BYTE_PREPEND && 
	  ((read_mode == SPI_NAND_FLASH_READ_SPEED_MODE_DUAL) || (read_mode == SPI_NAND_FLASH_READ_SPEED_MODE_QUAD)))
	{
		_SPI_NAND_WRITE_ONE_BYTE(0xff);			/* for dual/quad read dummy byte */
	}

	/* 4. Read n byte (len) data */
	switch (read_mode)
	{
		case SPI_NAND_FLASH_READ_SPEED_MODE_SINGLE:
			_SPI_NAND_READ_NBYTE( ptr_rtn_buf, len, SPI_CONTROLLER_SPEED_SINGLE);
			break;

		case SPI_NAND_FLASH_READ_SPEED_MODE_DUAL:
			_SPI_NAND_READ_NBYTE( ptr_rtn_buf, len, SPI_CONTROLLER_SPEED_DUAL);
			break;

		case SPI_NAND_FLASH_READ_SPEED_MODE_QUAD:
			_SPI_NAND_READ_NBYTE( ptr_rtn_buf, len, SPI_CONTROLLER_SPEED_QUAD);
			break;

		default:
			break;
	}

	/* 5. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_protocol_read_from_cache : data_offset=0x%x, buf=0x%x\n", data_offset, ptr_rtn_buf);	

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_program_load( u32     addr,
 *                                                                       u8      *ptr_data,
 *                                                                       u32     len,
 *									 u32 write_mode)
 * PURPOSE : To implement the SPI nand protocol for program load, with single speed.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : addr      - The addr variable of this function.
 *           ptr_data  - A pointer to the ptr_data variable.
 *           len       - The len variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_protocol_program_load ( u32 addr, 
							  u8 *ptr_data, 
							  u32 len,
							  u32 write_mode)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_protocol_program_load: addr=0x%x, len=0x%x\n", addr, len );

	/* 1. Chip Select low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Send opcode */
	switch (write_mode)
	{
		/* 02h */
		case SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE:
			_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_PROGRAM_LOAD_SINGLE );
			break;

		/* 32h */
		case SPI_NAND_FLASH_WRITE_SPEED_MODE_QUAD:
			_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_PROGRAM_LOAD_QUAD );
			break;

		default:
			break;
	}

	/* 3. Send address offset */
	if( ((ptr_dev_info_t->feature) & SPI_NAND_FLASH_PLANE_SELECT_HAVE) )
	{
		if( _plane_select_bit == 0)
		{
			_SPI_NAND_WRITE_ONE_BYTE( ((addr >> 8 ) & (0xef)) );
		}
		if( _plane_select_bit == 1)
		{
			_SPI_NAND_WRITE_ONE_BYTE( ((addr >> 8 ) | (0x10)) );
		}
	}
	else
	{
		_SPI_NAND_WRITE_ONE_BYTE( ((addr >> 8 ) & (0xff)) );
	}

	_SPI_NAND_WRITE_ONE_BYTE( ((addr)        & (0xff)) );

	/* 4. Send data */
	switch (write_mode)
	{
		case SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE:
			_SPI_NAND_WRITE_NBYTE( ptr_data, len, SPI_CONTROLLER_SPEED_SINGLE);
			break;

		case SPI_NAND_FLASH_WRITE_SPEED_MODE_QUAD:
			_SPI_NAND_WRITE_NBYTE( ptr_data, len, SPI_CONTROLLER_SPEED_QUAD);
			break;

		default:
			break;
	}

	/* 5. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_protocol_program_execute( u32  addr )
 * PURPOSE : To implement the SPI nand protocol for program execute.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : addr - The addr variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/17 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_protocol_program_execute ( u32 addr )
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
	u8 cmd[4];

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_protocol_program_execute: addr=0x%x\n", addr);

	/* 1. Chip Select low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Send 10h opcode */
	cmd[0] = _SPI_NAND_OP_PROGRAM_EXECUTE;

	/* 3. Send address offset */
	cmd[1] = (addr >> 16) & 0xff;
	cmd[2] = (addr >>  8) & 0xff;
	cmd[3] =  addr & 0xff;

	_SPI_NAND_WRITE_NBYTE(cmd, 4, SPI_CONTROLLER_SPEED_SINGLE);

	/* 4. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	return (rtn_status);
}

static SPI_NAND_FLASH_RTN_T spi_nand_protocol_die_select_1( u8 die_id)
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	/* 1. Chip Select low */
	_SPI_NAND_READ_CHIP_SELECT_LOW();

	/* 2. Send C2h opcode (Die Select) */
	_SPI_NAND_WRITE_ONE_BYTE( _SPI_NAND_OP_DIE_SELECT );

	/* 3. Send Die ID */
	_SPI_NAND_WRITE_ONE_BYTE( die_id );

	/* 5. Chip Select High */
	_SPI_NAND_READ_CHIP_SELECT_HIGH();

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_protocol_die_select_1\n");

	return (rtn_status);
}

static SPI_NAND_FLASH_RTN_T spi_nand_protocol_die_select_2( u8 die_id)
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
	unsigned char feature;

	rtn_status = spi_nand_protocol_get_status_reg_4(&feature);
	if(rtn_status != SPI_NAND_FLASH_RTN_NO_ERROR) {
		_SPI_NAND_PRINTF("spi_nand_protocol_die_select_2 get die select fail.\n");
		return (rtn_status);
	}

	if(die_id == 0) {
		feature &= ~(0x40);
	} else {
		feature |= 0x40;
	}

	rtn_status = spi_nand_protocol_set_status_reg_4(feature);

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_protocol_die_select_2\n");

	return (rtn_status);
}

static void spi_nand_select_die ( u32 page_number )
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	u8 die_id = 0;

	if(((ptr_dev_info_t->feature) & SPI_NAND_FLASH_DIE_SELECT_1_HAVE)) {
		/* single die = 1024blocks * 64pages */
		die_id = ((page_number >> 16) & 0xff);

		if (_die_id != die_id)
		{
			_die_id = die_id;
			spi_nand_protocol_die_select_1(die_id);

			_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spi_nand_protocol_die_select_1: die_id=0x%x\n", die_id);
		}
	} else if( ((ptr_dev_info_t->feature) & SPI_NAND_FLASH_DIE_SELECT_2_HAVE) ) {
		/* single die = 2plans * 1024blocks * 64pages */
		die_id = ((page_number >> 17) & 0xff);

		if (_die_id != die_id)
		{
			_die_id = die_id;
			spi_nand_protocol_die_select_2(die_id);

			_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spi_nand_protocol_die_select_2: die_id=0x%x\n", die_id);
		}
	}
}

static SPI_NAND_FLASH_RTN_T ecc_fail_check( u32 page_number )
{
	u8 status;
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	spi_nand_protocol_get_status_reg_3( &status);

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "ecc_fail_check: status=0x%x\n", status);

	if ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_GIGADEVICE) &&
		((ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F1GQ4UAYIG) ||
		 (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F1GQ4UBYIG) ||
		 (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F1GQ4UCYIG) ||
		 (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F1GQ4UEYIS) ||
		 (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F2GQ4UBYIG) ||
		 (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F2GQ4UE9IS) ||
		 (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F2GQ4UCYIG) ||
		 (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F4GQ4UBYIG) ||
		 (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F4GQ4UCYIG)))
	{
		if((ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F1GQ4UAYIG) ||
		   (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F1GQ4UBYIG) ||
		   (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F1GQ4UEYIS) ||
		   (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F2GQ4UBYIG) ||
		   (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F2GQ4UE9IS) ||
		   (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F4GQ4UBYIG))
		{
			if(((status & 0x30) >> 4) == 0x2)
			{
				rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
			}
		}

		if ((ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F1GQ4UCYIG) ||
		    (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F2GQ4UCYIG) ||
		    (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F4GQ4UCYIG))
		{
			if(((status & 0x70) >> 4) == 0x7)
			{
				rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
			}
		}
	}
	else
	if (ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_MXIC)
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_WINBOND)
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_ESMT) &&
		((ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_F50L512M41A) ||
		 (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_F50L1G41A0) ||
		 (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_F50L1G41LB) ||
		 (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_F50L2G41LB)))
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_ZENTEL) &&
		((ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_A5U12A21ASC) ||
		 (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_A5U1GA21BWS)))
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_ETRON)
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_TOSHIBA)
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_MICRON)
	{
		if(((status & 0x70) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_HEYANG)
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_PN) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_PN26G01AWSIUG)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_PN) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_PN26G02AWSIUG)))
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_ATO) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_ATO25D2GA)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_ATO_2) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_ATO25D2GB)))
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FM) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_FM25S01))
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FM) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_FM25G01B)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FM) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_FM25G02B)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FM) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_FM25G02C)))
	{
		if(((status & 0x70) >> 4) == 0x7)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_XTX) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_XT26G02B)))
	{
		if(((status & 0x70) >> 4) == 0x7)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_XTX) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_XT26G01A)))
	{
		if(((status & 0x3C) >> 2) == 0x8)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_XTX) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_XT26G02A)))
	{
		if(((status & 0x30) >> 4) == 0x2 )
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_MIRA) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_PSU1GS20BN)))
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_BIWIN) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_BWJX08U)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_BIWIN) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_BWET08U)))
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FORESEE) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_FS35ND02GS2F1)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FORESEE) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_FS35ND01GS1F1)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FORESEE) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_FS35ND02GD1F1)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FORESEE) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_FS35ND01GD1F1)))
	{
		if(((status & 0x70) >> 4) == 0x7)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_DS) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_DS35Q2GA)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_DS) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_DS35Q1GA)))
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FISON) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_CS11G0T0A0AA)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FISON) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_CS11G1T0A0AA)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FISON) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_CS11G0G0A0AA)))
	{
		if(((status & 0x70) >> 4) == 0x7)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}
	else
	if (((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_TYM) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_TYM25D2GA01)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_TYM) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_TYM25D2GA02)) ||
	    ((ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_TYM) && (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_TYM25D1GA03)))
	{
		if(((status & 0x30) >> 4) == 0x2)
		{
			rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
		}
	}

	if (rtn_status == SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK)
	{
		_SPI_NAND_PRINTF("[spinand_ecc_fail_check] : ECC cannot recover detected !, page=0x%x\n", page_number);
	}

	return (rtn_status);
}


/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_load_page_into_cache( long  page_number )
 * PURPOSE : To load page into SPI NAND chip.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : page_number - The page_number variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/16 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_load_page_into_cache( u32 page_number)
{
	u8 status;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_load_page_into_cache: page number =0x%x\n", page_number);

	if( _current_page_num == page_number )
	{
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_load_page_into_cache: page number == _current_page_num\n");
	}
	else
	{
		spi_nand_select_die ( page_number );

		spi_nand_protocol_page_read ( page_number );
		
		/*  Checking status for load page/erase/program complete */
		do{
			spi_nand_protocol_get_status_reg_3( &status);
		}while( status & _SPI_NAND_VAL_OIP);

		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_load_page_into_cache : status =0x%x\n", status);

		rtn_status = ecc_fail_check(page_number);
	}

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: void SPI_NAND_Flash_Clear_Read_Cache_Data( void )
 * PURPOSE : To clear the cache data for read. 
 *           (The next time to read data will get data from flash chip certainly.)
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : None
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2015/01/21 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static inline void SPI_NAND_Flash_Clear_Read_Cache_Data( void )
{
	_current_page_num = 0xFFFFFFFF;
}

/*------------------------------------------------------------------------------------
 * FUNCTION: spi_nand_set_clock_speed( u32 clock_factor)
 * PURPOSE : To set SPI clock. 
 *                 clock_factor = 0
 *                   EN7512: SPI clock = 500MHz / 40 = 12.5MHz
 *                   EN7522: SPI clock = 400MHz / 40 = 10MHz
 *                 clock_factor > 0
 *                   EN7512: SPI clock = 500MHz / (clock_factor * 2)
 *                   EN7522: SPI clock = 400MHz / (clock_factor * 2)
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : clock_factor - The SPI clock divider.
 * RETURN  : NONE.
 * NOTES   :
 * MODIFICTION HISTORY:
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_set_clock_speed( u32 clk)
{
	u32 val;
	u32 dividend;
	u32 clock_factor;

	if (!isFPGA) {
		if (isEN7526c || isEN751627) {
			dividend = 400;
		} else {
			dividend = 500;
		}

		clock_factor = (dividend / (clk * 2));

		val  = VPint(_SPI_FREQUENCY_ADJUST_REG);
		val &= 0xffff0000;
		VPint(_SPI_FREQUENCY_ADJUST_REG) = val;

		val |= (((clock_factor) << 8) | 1);
		VPint(_SPI_FREQUENCY_ADJUST_REG) = val;

		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1,
			"Set SPI Clock to %u Mhz\n", (dividend / (clock_factor * 2)));
	} else {
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, 
			"FPGA can not Set SPI Clock, FPGA SPI Clock is fixed to 25 Mhz\n");
	}

	return SPI_NAND_FLASH_RTN_NO_ERROR;
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_block_aligned_check( u32   addr,
 *                                                                     u32   len  )
 * PURPOSE : To check block align.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : addr - The addr variable of this function.
 *           len  - The len variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/15 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_block_aligned_check(u32 addr, u32 len)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "SPI_NAND_BLOCK_ALIGNED_CHECK_check: addr=0x%x, len=0x%x, block size =0x%x \n",
		addr, len, (ptr_dev_info_t->erase_size));

	if (_SPI_NAND_BLOCK_ALIGNED_CHECK(len, (ptr_dev_info_t->erase_size))) 
	{
		len = ( (len/ptr_dev_info_t->erase_size) + 1) * (ptr_dev_info_t->erase_size);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand__SPI_NAND_BLOCK_ALIGNED_CHECK_check: erase block aligned first check OK, addr:%x len:%x\n",
			addr, len, (ptr_dev_info_t->erase_size));
	}

	if (_SPI_NAND_BLOCK_ALIGNED_CHECK(addr, (ptr_dev_info_t->erase_size)) || _SPI_NAND_BLOCK_ALIGNED_CHECK(len, (ptr_dev_info_t->erase_size)) ) 
	{
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand__SPI_NAND_BLOCK_ALIGNED_CHECK_check: erase block not aligned, addr:0x%x len:0x%x, blocksize:0x%x\n",
			addr, len, (ptr_dev_info_t->erase_size));
		rtn_status = SPI_NAND_FLASH_RTN_ALIGNED_CHECK_FAIL;
	}

	return (rtn_status);
}

static SPI_NAND_FLASH_RTN_T spi_nand_erase_block ( u32 block_index)
{
	u8 status;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	spi_nand_select_die ( (block_index << _SPI_NAND_BLOCK_ROW_ADDRESS_OFFSET) );

	/* 2.2 Enable write_to flash */
	spi_nand_protocol_write_enable();

	/* 2.3 Erasing one block */
	spi_nand_protocol_block_erase( block_index );

	/* 2.4 Checking status for erase complete */
	do{
		spi_nand_protocol_get_status_reg_3( &status);
	}while( status & _SPI_NAND_VAL_OIP) ;

	/* 2.5 Disable write_flash */
	spi_nand_protocol_write_disable();

	/* 2.6 Check Erase Fail Bit */
	if( status & _SPI_NAND_VAL_ERASE_FAIL )
	{
		_SPI_NAND_PRINTF("spi_nand_erase_block : erase block fail, block=0x%x, status=0x%x\n", block_index, status);
		rtn_status = SPI_NAND_FLASH_RTN_ERASE_FAIL;
	}

	return rtn_status;
}


/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_erase_internal( u32     addr,
 *                                               				  u32     len )
 * PURPOSE : To erase flash internally.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : addr - The addr variable of this function.
 *           len - The size variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/15 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_erase_internal( u32 addr, u32 len )
{
	u32 block_index = 0;
	u32 erase_len = 0;
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
#if defined(TCSUPPORT_NAND_BMT) && !defined(LZMA_IMG)
	unsigned short phy_block_bbt;
	u32 logical_block, physical_block;
#endif

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "\nspi_nand_erase_internal (in): addr=0x%x, len=0x%x\n", addr, len );

	/* Switch to manual mode*/
	_SPI_NAND_ENABLE_MANUAL_MODE();

	SPI_NAND_Flash_Clear_Read_Cache_Data();

	/* 1. Check the address and len must aligned to NAND Flash block size */
	if( spi_nand_block_aligned_check( addr, len) == SPI_NAND_FLASH_RTN_NO_ERROR)
	{
		/* 2. Erase block one by one */
		while( erase_len < len )
		{
			/* 2.1 Caculate Block index */
			block_index = (addr / ptr_dev_info_t->erase_size);
#if defined(TCSUPPORT_NAND_BMT) && !defined(LZMA_IMG)
			logical_block = block_index;
			physical_block = get_mapping_block_index(logical_block, &phy_block_bbt);
			if( physical_block != logical_block)
			{
				_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "Bad Block Mapping, from %d block to %d block\n", logical_block, physical_block);
			}
			block_index = physical_block;
#endif
			_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_erase_internal: addr=0x%x, len=0x%x, block_idx=0x%x\n", addr, len, block_index );

			rtn_status = spi_nand_erase_block(block_index);

			/* 2.6 Check Erase Fail Bit */
			if(rtn_status != SPI_NAND_FLASH_RTN_NO_ERROR)
			{
#if defined(TCSUPPORT_NAND_BMT) && !defined(LZMA_IMG)
					if (update_bmt((block_index * ptr_dev_info_t->erase_size), UPDATE_ERASE_FAIL, NULL, NULL))
					{
						_SPI_NAND_PRINTF("Erase fail at block: %d, update BMT success\n", (addr / ptr_dev_info_t->erase_size));
						rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
					}
					else
					{
						_SPI_NAND_PRINTF("Erase fail at block: %d, update BMT fail\n", (addr / ptr_dev_info_t->erase_size));
						rtn_status = SPI_NAND_FLASH_RTN_ERASE_FAIL;
						break;
					}
#else
				_SPI_NAND_PRINTF("spi_nand_erase_internal : Erase Fail at addr=0x%x, len=0x%x, block_idx=0x%x\n", addr, len, block_index);
				rtn_status = SPI_NAND_FLASH_RTN_ERASE_FAIL;
				break;
#endif
			}

			/* 2.7 Erase next block if needed */
			addr		+= ptr_dev_info_t->erase_size;
			erase_len	+= ptr_dev_info_t->erase_size;
		}
	}
	else
	{
		rtn_status = SPI_NAND_FLASH_RTN_ALIGNED_CHECK_FAIL;
	}

	return (rtn_status);
}

static SPI_NAND_FLASH_RTN_T spi_nand_read_page (u32 page_number, SPI_NAND_FLASH_READ_SPEED_MODE_T speed_mode)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
	struct spi_nand_flash_oobfree *ptr_oob_entry_idx;
	u32 idx=0;
	u32 i, j;
	u16 read_addr;

	/* read from read_addr index in the page */
	read_addr = 0;

	/* Switch to manual mode*/
	_SPI_NAND_ENABLE_MANUAL_MODE();

	/* 1. Load Page into cache of NAND Flash Chip */
	if( spi_nand_load_page_into_cache(page_number) == SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK )
	{
		_SPI_NAND_PRINTF("spi_nand_read_page: Bad Block, ECC cannot recovery detecte, page=0x%x\n", page_number);
		rtn_status = SPI_NAND_FLASH_RTN_DETECTED_BAD_BLOCK;
	}

	/* 2. Read whole data from cache of NAND Flash Chip */
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_read_page: curren_page_num=0x%x, page_number=0x%x\n", _current_page_num, page_number);

	/* No matter what status, we must read the cache data to dram */
	if( (_current_page_num != page_number) )
	{
		memset(_current_cache_page, 0x0, sizeof(_current_cache_page));

		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spi_nand_read_page: before read, _current_cache_page:\n");
		_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &_current_cache_page[0], _SPI_NAND_CACHE_SIZE);

		if( ((ptr_dev_info_t->feature) & SPI_NAND_FLASH_PLANE_SELECT_HAVE) )
		{
			_plane_select_bit = ((page_number >> 6)& (0x1));

			_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1,"spi_nand_read_page: plane select = 0x%x\n",  _plane_select_bit);			
		}

		spi_nand_protocol_read_from_cache(read_addr, ((ptr_dev_info_t->page_size)+(ptr_dev_info_t->oob_size)), &_current_cache_page[0], speed_mode, ptr_dev_info_t->dummy_mode );

		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spi_nand_read_page: after read, _current_cache_page:\n");
		_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &_current_cache_page[0], _SPI_NAND_CACHE_SIZE);

		/* Divide read page into data segment and oob segment  */
		memcpy( &_current_cache_page_data[0], &_current_cache_page[0], (ptr_dev_info_t->page_size) );
		memcpy( &_current_cache_page_oob[0],  &_current_cache_page[(ptr_dev_info_t->page_size)], (ptr_dev_info_t->oob_size) );

		idx = 0;
		ptr_oob_entry_idx = (struct spi_nand_flash_oobfree *) &( (ptr_dev_info_t->oob_free_layout)->oobfree );

		/*  When OnDie ecc is enable, mapping oob area is neccessary */
		if (_ondie_ecc_flag)
		{
			/* Transter oob area from physical offset into logical offset */
			for( i=0; (i<SPI_NAND_FLASH_OOB_FREE_ENTRY_MAX) && (ptr_oob_entry_idx[i].len) && (idx< ((ptr_dev_info_t->oob_free_layout)->oobsize)) ; i++)
			{
				for(j=0; (j< (ptr_oob_entry_idx[i].len)) && (idx<(ptr_dev_info_t->oob_free_layout->oobsize)) ; j++)
				{
					/* _SPI_NAND_PRINTF("i=%d , j=%d, len=%d, idx=%d, size=%d\n", i, j,(ptr_oob_entry_idx[i].len), idx, (ptr_dev_info_t->oob_free_layout->oobsize) ); */
					_current_cache_page_oob_mapping[idx] = _current_cache_page_oob[(ptr_oob_entry_idx[i].offset)+j];
					idx++;
				}
			}
		}
		else
		{
			memcpy( &_current_cache_page_oob_mapping[0],  &_current_cache_page_oob[0], (ptr_dev_info_t->oob_size) );
		}

		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spi_nand_read_page: _current_cache_page:\n");
		_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &_current_cache_page[0], ((ptr_dev_info_t->page_size)+(ptr_dev_info_t->oob_size)));
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spi_nand_read_page: _current_cache_page_oob:\n");
		_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &_current_cache_page_oob[0], (ptr_dev_info_t->oob_size));
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spi_nand_read_page: _current_cache_page_oob_mapping:\n");
		_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &_current_cache_page_oob_mapping[0], (ptr_dev_info_t->oob_size));

		_current_page_num = page_number;
	}

	return rtn_status;
}

#ifdef UBIFS_BLANK_PAGE_FIXUP
UBIFS_BLANK_PAGE_ECC_T check_blank_page(u32 page_number)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	u32 block, page_per_block;
	u8 ecc_parity_0[8] = {0}, i = 0;
#if defined(SPI_NAND_FLASH_DEBUG)
	SPI_NAND_FLASH_DEBUG_LEVEL_T ubiDbgLv = SPI_NAND_FLASH_DEBUG_LEVEL_2;
#endif

	if (ptr_dev_info_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_WINBOND) {
		if((ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_W25N01GV) ||
		   (ptr_dev_info_t->dev_id == _SPI_NAND_DEVICE_ID_W25M02GV)) {
		   /* for winbond, if data has been written blank,
		    * the ECC parity is all 0.
		    */
			memset(ecc_parity_0, 0x0, sizeof(ecc_parity_0));

			for(i = 0; i < 4; i++) {
				if(memcmp(ecc_parity_0, &_current_cache_page_oob[i * 16 + 8], sizeof(ecc_parity_0)) == 0) {
					page_per_block = (ptr_dev_info_t->erase_size / ptr_dev_info_t->page_size);
					block = page_number / page_per_block;

					_SPI_NAND_DEBUG_PRINTF(ubiDbgLv, "page 0x%x detected ECC parity 0 at block:0x%x page offset:0x%x.\n", page_number, block, page_number % page_per_block);
					_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &_current_cache_page[0], ((ptr_dev_info_t->page_size)+(ptr_dev_info_t->oob_size)));
					return SPI_NAND_FLASH_UBIFS_BLANK_PAGE_ECC_MISMATCH; /* blank sector*/
				}
			}
		}

		return SPI_NAND_FLASH_UBIFS_BLANK_PAGE_ECC_MATCH; /* Good Page*/
	} else {
		return SPI_NAND_FLASH_UBIFS_BLANK_PAGE_ECC_MATCH; /* Good Page*/
	}
}

SPI_NAND_FLASH_RTN_T store_block(u32 block, u8 *block_buf)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	int i;
	u32 page_per_block;
	u32 start_page;
	SPI_NAND_FLASH_RTN_T rtn_status;
#if defined(SPI_NAND_FLASH_DEBUG)
	SPI_NAND_FLASH_DEBUG_LEVEL_T ubiDbgLv = SPI_NAND_FLASH_DEBUG_LEVEL_2;
#endif

	page_per_block = (ptr_dev_info_t->erase_size / ptr_dev_info_t->page_size);
	start_page = block * page_per_block;

	// read all pages in the block
	for(i = 0; i < page_per_block; i++) {
		rtn_status = spi_nand_read_page(i + start_page, SPI_NAND_FLASH_READ_SPEED_MODE_SINGLE);
		if(rtn_status == SPI_NAND_FLASH_RTN_NO_ERROR) {
			memcpy(block_buf + i * (ptr_dev_info_t->page_size + ptr_dev_info_t->oob_size),
				_current_cache_page, ptr_dev_info_t->page_size + ptr_dev_info_t->oob_size);

			_SPI_NAND_DEBUG_PRINTF(ubiDbgLv, "store_block: block:0x%x page offset:0x%x\n", block, i);
			_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &_current_cache_page[0], ((ptr_dev_info_t->page_size)+(ptr_dev_info_t->oob_size)));
		}
		else
		{
			_SPI_NAND_PRINTF("%s: fix blank page 0x%x read error\n",__func__, start_page+i);
			return rtn_status;
		}
	}

	return SPI_NAND_FLASH_RTN_NO_ERROR;
}

SPI_NAND_FLASH_RTN_T restore_block(u32 block, u8 *block_buf, u32 page_number)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	u32 i, j, k, idx;
	u32 page_per_block;
	u32 start_page;
	SPI_NAND_FLASH_RTN_T rtn_status;
	int isBlankData, isBlankOOB;
	u8 oob_mapping[_SPI_NAND_OOB_SIZE];
	u8 *page_buf = NULL;
	struct spi_nand_flash_oobfree *ptr_oob_entry_idx;
#if defined(SPI_NAND_FLASH_DEBUG)
	SPI_NAND_FLASH_DEBUG_LEVEL_T ubiDbgLv = SPI_NAND_FLASH_DEBUG_LEVEL_2;
#endif

	page_per_block = (ptr_dev_info_t->erase_size / ptr_dev_info_t->page_size);
	start_page = block * page_per_block;

	// read all pages in the block
	for(i = 0; i < page_per_block; i++) {
		if((i + start_page) == page_number) {
			_SPI_NAND_DEBUG_PRINTF(ubiDbgLv, "restore_block: skip source page block:0x%x page offset:0x%x\n", block, i);
			continue;
		}

		page_buf = (block_buf + i * (ptr_dev_info_t->page_size + ptr_dev_info_t->oob_size));

		isBlankData = 1;
		for(idx = 0; idx < ptr_dev_info_t->page_size; idx++) {
			if(page_buf[idx] != 0xFF) {
				isBlankData = 0;
				break;
			}
		}

		if(isBlankData == 1) {
			isBlankOOB = 1;
			ptr_oob_entry_idx = (struct spi_nand_flash_oobfree *) &( ptr_dev_info_t->oob_free_layout->oobfree );

			idx = 0;
			for(k = 0; (k < SPI_NAND_FLASH_OOB_FREE_ENTRY_MAX) && (ptr_oob_entry_idx[k].len) && (idx < ptr_dev_info_t->oob_free_layout->oobsize); k++)
			{
				for(j = 0; (j < ptr_oob_entry_idx[k].len) && (idx < ptr_dev_info_t->oob_free_layout->oobsize); j++)
				{
					if(page_buf[ptr_dev_info_t->page_size + (ptr_oob_entry_idx[k].offset) + j] != 0xFF) {
						isBlankOOB = 0;
						k = SPI_NAND_FLASH_OOB_FREE_ENTRY_MAX;
						break;
					}
					idx++;
				}
			}
		}

		if((isBlankData == 1) && (isBlankOOB == 1)) {
			_SPI_NAND_DEBUG_PRINTF(ubiDbgLv, "restore_block: skip blank page block:0x%x page offset:0x%x\n", block, i);
			continue;
		}

		ptr_oob_entry_idx = (struct spi_nand_flash_oobfree *) &( ptr_dev_info_t->oob_free_layout->oobfree );
		idx = 0;

		memset(oob_mapping, 0xFF, _SPI_NAND_OOB_SIZE);
		for(k = 0; (k < SPI_NAND_FLASH_OOB_FREE_ENTRY_MAX) && (ptr_oob_entry_idx[k].len) && (idx < ptr_dev_info_t->oob_free_layout->oobsize); k++)
		{
			for(j = 0; (j < ptr_oob_entry_idx[k].len) && (idx < ptr_dev_info_t->oob_free_layout->oobsize); j++)
			{
				oob_mapping[idx] = page_buf[ptr_dev_info_t->page_size + (ptr_oob_entry_idx[k].offset) + j];
				idx++;
			}
		}

		rtn_status = spi_nand_write_page(i + start_page, 
										 0, 
										 (block_buf + i * (ptr_dev_info_t->page_size + ptr_dev_info_t->oob_size)), 
										 ptr_dev_info_t->page_size, 
										 0, 
										 oob_mapping,
										 ptr_dev_info_t->oob_size, 
										 SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE);

		_SPI_NAND_DEBUG_PRINTF(ubiDbgLv, "restore_block: block:0x%x page offset:0x%x\n", block, i);

		if(rtn_status == SPI_NAND_FLASH_RTN_NO_ERROR) {
			_SPI_NAND_DEBUG_PRINTF(ubiDbgLv, "fixed page %x\n", start_page + i);
		} else {
			_SPI_NAND_PRINTF("%s: fix_ecc_0 0x%x write error \n",__func__, start_page+i);
			return rtn_status;
		}
	}

	return SPI_NAND_FLASH_RTN_NO_ERROR;
}


UBIFS_BLANK_PAGE_FIXUP_T fix_blank_page(u32 page_number)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	u8 *block_buf;
	u32 page_per_block;
	u32 block;
	SPI_NAND_FLASH_RTN_T rtn_status;

	page_per_block = (ptr_dev_info_t->erase_size / ptr_dev_info_t->page_size);
	block = page_number / page_per_block;

	block_buf = (u8 *) kmalloc((ptr_dev_info_t->page_size + ptr_dev_info_t->oob_size) * page_per_block, GFP_KERNEL);
	if(!block_buf) {
		_SPI_NAND_PRINTF("%s:can not allocate buffer\n", __func__);
		return SPI_NAND_FLASH_UBIFS_BLANK_PAGE_FIXUP_FAIL;
	}
	memset(block_buf, 0xff, (ptr_dev_info_t->page_size + ptr_dev_info_t->oob_size) * page_per_block);

	/* store block */
	rtn_status = store_block(block, block_buf);
	if(rtn_status != SPI_NAND_FLASH_RTN_NO_ERROR) {
		kfree(block_buf);
		return SPI_NAND_FLASH_UBIFS_BLANK_PAGE_FIXUP_FAIL;
	}

	/* erase block */
	rtn_status = spi_nand_erase_block(block);
	if(rtn_status != SPI_NAND_FLASH_RTN_NO_ERROR) {
		kfree(block_buf);
		return SPI_NAND_FLASH_UBIFS_BLANK_PAGE_FIXUP_FAIL;
	}

	/* restore block except page_number */
	rtn_status = restore_block(block, block_buf, page_number);
	if(rtn_status != SPI_NAND_FLASH_RTN_NO_ERROR) {
		kfree(block_buf);
		return SPI_NAND_FLASH_UBIFS_BLANK_PAGE_FIXUP_FAIL;
	}

	kfree(block_buf);
	return SPI_NAND_FLASH_UBIFS_BLANK_PAGE_FIXUP_SUCCESS;
}
#endif

static SPI_NAND_FLASH_RTN_T spi_nand_write_page(u32 page_number,
						u32 data_offset,
						u8  *ptr_data,
						u32 data_len,
						u32 oob_offset,
						u8  *ptr_oob,
						u32 oob_len,
						SPI_NAND_FLASH_WRITE_SPEED_MODE_T speed_mode )
{
	u8 status, status_2;
	u32 i=0,j=0,idx=0;
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t;
	struct spi_nand_flash_oobfree *ptr_oob_entry_idx;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
	u16 write_addr;
#ifdef UBIFS_BLANK_PAGE_FIXUP
#if defined(SPI_NAND_FLASH_DEBUG)
	SPI_NAND_FLASH_DEBUG_LEVEL_T ubiDbgLv = SPI_NAND_FLASH_DEBUG_LEVEL_2;
#endif
	static int isUbifsBlankPageFix = 0;
#endif

	/* write to write_addr index in the page */
	write_addr = 0;

	/* Switch to manual mode*/
	_SPI_NAND_ENABLE_MANUAL_MODE();

	ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;

#ifdef UBIFS_BLANK_PAGE_FIXUP
	if(isUbifsBlankPageFix == 0) {
		/* Read Current page data to software cache buffer */
		spi_nand_read_page(page_number, speed_mode);

		if(check_blank_page(page_number) == SPI_NAND_FLASH_UBIFS_BLANK_PAGE_ECC_MISMATCH) {
			isUbifsBlankPageFix = 1;
			_SPI_NAND_DEBUG_PRINTF(ubiDbgLv, "UBIFS_BLANK_PAGE_FIXUP, page:0x%x\n", page_number);
			fix_blank_page(page_number);
			isUbifsBlankPageFix = 0;

			/* Read Current page data to software cache buffer */
			spi_nand_read_page(page_number, speed_mode);
		}
	}
#else
	/* Read Current page data to software cache buffer */
	spi_nand_read_page(page_number, speed_mode);
#endif

	/* Rewirte the software cache buffer */
	if(data_len > 0)
	{
		memcpy( &_current_cache_page_data[data_offset], &ptr_data[0], data_len );
	}

	if( oob_len > 0 )	/* Write OOB */
	{
		/*  When OnDie ecc is enable,  mapping oob area is neccessary */
		if (_ondie_ecc_flag)
		{
			ptr_oob_entry_idx = (struct spi_nand_flash_oobfree *) &( ptr_dev_info_t->oob_free_layout->oobfree );

			for( i=0; (i<SPI_NAND_FLASH_OOB_FREE_ENTRY_MAX) && (ptr_oob_entry_idx[i].len) && ((idx< (ptr_dev_info_t->oob_free_layout->oobsize)) && (idx < oob_len))  ; i++)
			{
				for(j=0; (j< (ptr_oob_entry_idx[i].len)) && (idx<(ptr_dev_info_t->oob_free_layout->oobsize)) && ((idx< (ptr_dev_info_t->oob_free_layout->oobsize)) && (idx < oob_len)) ; j++)
				{
					_current_cache_page_oob[(ptr_oob_entry_idx[i].offset)+j] &= ptr_oob[idx];
					idx++;
				}
			}
		}
		else
		{
			memcpy( &_current_cache_page_oob[0], &ptr_oob[0], oob_len);
		}
	}

	memcpy( &_current_cache_page[0], &_current_cache_page_data[0], ptr_dev_info_t->page_size );
	memcpy( &_current_cache_page[ptr_dev_info_t->page_size],  &_current_cache_page_oob[0], ptr_dev_info_t->oob_size );

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spi_nand_write_page: page=0x%x, data_offset=0x%x, date_len=0x%x, oob_offset=0x%x, oob_len=0x%x\n", page_number, data_offset, data_len, oob_offset, oob_len);
	_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &_current_cache_page[0], ((ptr_dev_info_t->page_size) + (ptr_dev_info_t->oob_size)));		

	if( ((ptr_dev_info_t->feature) & SPI_NAND_FLASH_PLANE_SELECT_HAVE)  )
	{
		_plane_select_bit = ((page_number >> 6) & (0x1));

		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spi_nand_write_page: _plane_select_bit=0x%x\n", _plane_select_bit );
	}

	spi_nand_select_die ( page_number );

	/* Different Manafacture have different program flow and setting */
	if( ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_GIGADEVICE) ||
	    ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_PN) ||
	    ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_FM) ||
	    ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_XTX) ||
	    ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_FORESEE) ||
	    ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_FISON) ||
	    ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_TYM) ||
	    ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_ATO_2) ||
	   (((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_ATO) && ((ptr_dev_info_t->dev_id) == _SPI_NAND_DEVICE_ID_ATO25D2GA)))
	{
		spi_nand_protocol_program_load(write_addr, &_current_cache_page[0], ((ptr_dev_info_t->page_size) + (ptr_dev_info_t->oob_size)), speed_mode);

		/* Enable write_to flash */
		spi_nand_protocol_write_enable();
	}
	else
	{
		/* Enable write_to flash */
		spi_nand_protocol_write_enable();

		/* Proram data into buffer of SPI NAND chip */
		spi_nand_protocol_program_load(write_addr, &_current_cache_page[0], ((ptr_dev_info_t->page_size) + (ptr_dev_info_t->oob_size)), speed_mode);
	}

	/* Execute program data into SPI NAND chip  */
	spi_nand_protocol_program_execute ( page_number );

	/* Checking status for erase complete */
	do{
		spi_nand_protocol_get_status_reg_3( &status);
	}while( status & _SPI_NAND_VAL_OIP) ;

	/* Disable write_flash */
	spi_nand_protocol_write_disable();

	spi_nand_protocol_get_status_reg_1( &status_2);

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spi_nand_write_page]: status 1 = 0x%x, status 3 =0x%x\n", status_2, status);

	/* Check Program Fail Bit */
	if( status & _SPI_NAND_VAL_PROGRAM_FAIL )
	{
		_SPI_NAND_PRINTF("spi_nand_write_page : Program Fail at addr_offset =0x%x, page_number=0x%x, status=0x%x\n", data_offset, page_number, status);
		rtn_status = SPI_NAND_FLASH_RTN_PROGRAM_FAIL;
	}

	return (rtn_status);
}

static SPI_NAND_FLASH_RTN_T spi_nand_write_page_internal(u32 page_number,
							u32 data_offset,
							u8  *ptr_data,
							u32 data_len,
							u32 oob_offset,
							u8  *ptr_oob,
							u32 oob_len,
							SPI_NAND_FLASH_WRITE_SPEED_MODE_T speed_mode )
{
	u32 addr_offset = 0;
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
#if defined(TCSUPPORT_NAND_BMT) && !defined(LZMA_IMG)
	unsigned short phy_block_bbt;
	u32 logical_block, physical_block;
	u32 page_offset_in_block;
#endif

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spi_nand_write_page_internal]: enter\n");

	SPI_NAND_Flash_Clear_Read_Cache_Data();

#if defined(TCSUPPORT_NAND_BMT) && !defined(LZMA_IMG)
	page_offset_in_block = ((page_number * (ptr_dev_info_t->page_size))%(ptr_dev_info_t->erase_size))/(ptr_dev_info_t->page_size);
	logical_block = ((page_number * (ptr_dev_info_t->page_size))/(ptr_dev_info_t->erase_size)) ;
	physical_block = get_mapping_block_index(logical_block, &phy_block_bbt);

	if( physical_block != logical_block)
	{
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "Bad Block Mapping, from %d block to %d block\n", logical_block, physical_block);
	}

	page_number = (page_offset_in_block)+((physical_block*(ptr_dev_info_t->erase_size))/(ptr_dev_info_t->page_size));
#endif

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spi_nand_write_page_internal]: page_number = 0x%x\n", page_number);

#if defined(TCSUPPORT_NAND_BMT) && !defined(LZMA_IMG)
	if(block_is_in_bmt_region(physical_block))
	{
		memcpy(ptr_oob + OOB_INDEX_OFFSET, &phy_block_bbt, OOB_INDEX_SIZE);
	}

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spi_nand_write_page_internal: data_offset=0x%x, date_len=0x%x, oob_offset=0x%x, oob_len=0x%x\n", data_offset, data_len, oob_offset, oob_len);
	_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &ptr_oob[0], oob_len);

	rtn_status = spi_nand_write_page(page_number, addr_offset, ptr_data, data_len, 0, ptr_oob, MAX_USE_OOB_SIZE , speed_mode);
	if(rtn_status != SPI_NAND_FLASH_RTN_NO_ERROR)
	{
		_SPI_NAND_PRINTF("write fail at page: 0x%x \n", page_number);
		if (update_bmt(page_number * (ptr_dev_info_t->page_size), UPDATE_WRITE_FAIL, ptr_data, ptr_oob))
		{
			_SPI_NAND_PRINTF("Update BMT success\n");
			rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
		}
		else
		{
			_SPI_NAND_PRINTF("Update BMT fail\n");
		}
	}
#else
	rtn_status = spi_nand_write_page(page_number, addr_offset, ptr_data, data_len, 0, ptr_oob, MAX_USE_OOB_SIZE , speed_mode);
#endif

	SPI_NAND_Flash_Clear_Read_Cache_Data();

	return (rtn_status);
}

static SPI_NAND_FLASH_RTN_T spi_nand_read_page_internal ( u32 page_number, 
							 SPI_NAND_FLASH_READ_SPEED_MODE_T speed_mode )
{
	u32 logical_block, physical_block;
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
#if defined(TCSUPPORT_NAND_BMT) && !defined(LZMA_IMG)
	unsigned short phy_block_bbt;
	u32 page_offset_in_block;
#endif

#if defined(TCSUPPORT_NAND_BMT) && !defined(LZMA_IMG)
	page_offset_in_block = (((page_number * (ptr_dev_info_t->page_size))%(ptr_dev_info_t->erase_size))/ (ptr_dev_info_t->page_size));
	logical_block = ((page_number * (ptr_dev_info_t->page_size))/(ptr_dev_info_t->erase_size)) ;
	physical_block = get_mapping_block_index(logical_block, &phy_block_bbt);

	if( physical_block != logical_block)
	{
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "Bad Block Mapping, from %d block to %d block\n", logical_block, physical_block);
	}

	page_number = (page_offset_in_block)+ ((physical_block*(ptr_dev_info_t->erase_size))/(ptr_dev_info_t->page_size));
#endif
	rtn_status = spi_nand_read_page(page_number, speed_mode);

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static void spi_nand_manufacute_init( struct SPI_NAND_FLASH_INFO_T *ptr_device_t )
 * PURPOSE : To init SPI NAND Flash chip
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : None
 *   OUTPUT: None.
 * RETURN  : None.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2015/05/15 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static void spi_nand_manufacute_init( struct SPI_NAND_FLASH_INFO_T *ptr_device_t )
{
	unsigned char feature;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1,"SPI NAND Chip Init : Unlock all block and Enable Quad Mode\n"); 

	if (((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_GIGADEVICE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F1GQ4UAYIG)) ||
	    ((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_GIGADEVICE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F1GQ4UBYIG)) ||
	    ((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_GIGADEVICE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F1GQ4UCYIG)) ||
	    ((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_GIGADEVICE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F1GQ4UEYIS)) ||
	    ((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_GIGADEVICE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F2GQ4UBYIG)) ||
	    ((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_GIGADEVICE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F2GQ4UE9IS)) ||
	    ((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_GIGADEVICE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F2GQ4UCYIG)) ||
	    ((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_GIGADEVICE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F4GQ4UBYIG)) ||
	    ((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_GIGADEVICE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_GD5F4GQ4UCYIG)))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC1;
		spi_nand_protocol_set_status_reg_1(feature);

		spi_nand_protocol_get_status_reg_1(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After Unlock all block setup, the status register1 =0x%x\n", feature);					

		/* 2. Enable Qual mode */
		spi_nand_protocol_get_status_reg_2(&feature);
		feature |= 0x1;
		spi_nand_protocol_set_status_reg_2(feature);

		spi_nand_protocol_get_status_reg_2(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After enable qual mode setup, the status register2 =0x%x\n", feature);				
	}
	else if((ptr_device_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_MXIC)
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC1;
		spi_nand_protocol_set_status_reg_1(feature);

		spi_nand_protocol_get_status_reg_1(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After Unlock all block setup, the status register1 =0x%x\n", feature);					

		/* 2. Enable Qual mode */
		spi_nand_protocol_get_status_reg_2(&feature);
		feature |= 0x1;
		spi_nand_protocol_set_status_reg_2(feature);

		spi_nand_protocol_get_status_reg_2(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After enable qual mode setup, the status register2 =0x%x\n", feature);				
	}
	else if( (ptr_device_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_WINBOND)
	{
		if(((ptr_device_t->feature) & SPI_NAND_FLASH_DIE_SELECT_1_HAVE)) {
			_die_id = 0x00;
			spi_nand_protocol_die_select_1(_die_id);
		}

		/* Enable to modify the status regsiter 1 */
		feature = 0x58;
		spi_nand_protocol_set_status_reg_2(feature);

		/* Unlock all block and Enable Qual mode */
		feature = 0x81;
		spi_nand_protocol_set_status_reg_1(feature);

		/* Disable to modify the status regsiter 1 */
		feature = 0x18;
		spi_nand_protocol_set_status_reg_2(feature);

		spi_nand_protocol_get_status_reg_1(&feature);

		/* Unlock all block for Die_1 */
		if(((ptr_device_t->feature) & SPI_NAND_FLASH_DIE_SELECT_1_HAVE))
		{
			_die_id = 0x01;
			spi_nand_protocol_die_select_1(_die_id);

			/* Enable to modify the status regsiter 1 */
			feature = 0x58;
			spi_nand_protocol_set_status_reg_2(feature);

			/* Unlock all block and Enable Qual mode */
			feature = 0x81;
			spi_nand_protocol_set_status_reg_1(feature);

			/* Disable to modify the status regsiter 1 */
			feature = 0x18;
			spi_nand_protocol_set_status_reg_2(feature);

			spi_nand_protocol_get_status_reg_1(&feature);
		}

		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After Unlock all block setup, the status register1 =0x%x\n", feature);					
	}
	else if(((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_ESMT) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_F50L512M41A)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_ESMT) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_F50L1G41A0)))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);

		spi_nand_protocol_get_status_reg_1(&feature);

		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After Unlock all block setup, the status register1 =0x%x\n", feature);
	}
	else if(((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_ESMT) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_F50L1G41LB)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_ESMT) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_F50L2G41LB)))
	{
		if(((ptr_device_t->feature) & SPI_NAND_FLASH_DIE_SELECT_1_HAVE))
		{
			_die_id = 0x00;
			spi_nand_protocol_die_select_1(_die_id);
		}

		/* 1. Unlock All block */
		feature = 0x83;
		spi_nand_protocol_set_status_reg_1(feature);

		spi_nand_protocol_get_status_reg_1(&feature);

		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After Unlock all block setup, the status register1 =0x%x\n", feature);					

		/* Unlock all block for Die_1 */
		if(((ptr_device_t->feature) & SPI_NAND_FLASH_DIE_SELECT_1_HAVE))
		{
			_die_id = 0x01;
			spi_nand_protocol_die_select_1(_die_id);

			/* 1. Unlock All block */
			feature = 0x83;
			spi_nand_protocol_set_status_reg_1(feature);

			spi_nand_protocol_get_status_reg_1(&feature);

			_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After Unlock all block setup, the die %d status register1 =0x%x\n", _die_id, feature);					
		}
	}
	else if(((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_ZENTEL) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_A5U12A21ASC)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_ZENTEL) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_A5U1GA21BWS)))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);

		spi_nand_protocol_get_status_reg_1(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After Unlock all block setup, the status register1 =0x%x\n", feature);						
	}
	else if( (ptr_device_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_ETRON)
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC1;
		spi_nand_protocol_set_status_reg_1(feature);

		spi_nand_protocol_get_status_reg_1(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After Unlock all block setup, the status register1 =0x%x\n", feature);						

		/* 2. Enable Qual mode */
		spi_nand_protocol_get_status_reg_2(&feature);
		feature |= 0x1;
		spi_nand_protocol_set_status_reg_2(feature);

		spi_nand_protocol_get_status_reg_2(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After enable qual mode setup, the status register2 =0x%x\n", feature);						
	}
	else if( (ptr_device_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_TOSHIBA)
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);

		spi_nand_protocol_get_status_reg_1(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1,"After Unlock all block setup, the status register1 =0x%x\n", feature);												
	}
	else if( (ptr_device_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_MICRON)
	{
		if(((ptr_device_t->feature) & SPI_NAND_FLASH_DIE_SELECT_2_HAVE)) {
			_die_id = 0x00;
			spi_nand_protocol_die_select_2(_die_id);
		}

		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0x83;
		spi_nand_protocol_set_status_reg_1(feature);

		spi_nand_protocol_get_status_reg_1(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1,"After Unlock all block setup, the status register1 =0x%x\n", feature);

		/* Unlock all block for Die_1 */
		if( ((ptr_device_t->feature) & SPI_NAND_FLASH_DIE_SELECT_2_HAVE) )
		{
			_die_id = 0x01;
			spi_nand_protocol_die_select_2(_die_id);

			/* 1. Unlock All block */
			spi_nand_protocol_get_status_reg_1(&feature);
			feature &= 0x83;
			spi_nand_protocol_set_status_reg_1(feature);

			spi_nand_protocol_get_status_reg_1(&feature);
			_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1,"After Unlock all block setup, the die %d status register1 =0x%x\n", _die_id, feature);
		}
	}
	else if( (ptr_device_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_HEYANG)
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);

		/* 2. Enable Qual mode */
		spi_nand_protocol_get_status_reg_2(&feature);
		feature |= 0x1;
		spi_nand_protocol_set_status_reg_2(feature);
	}
	else if(((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_PN) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_PN26G01AWSIUG)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_PN) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_PN26G02AWSIUG)))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);

		/* 2. Enable Qual mode */
		spi_nand_protocol_get_status_reg_2(&feature);
		feature |= 0x1;
		spi_nand_protocol_set_status_reg_2(feature);
	}
	else if(((ptr_device_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_ATO) ||
		((ptr_device_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_ATO_2))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);
	}
	else if((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FM) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_FM25S01))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0x87;
		spi_nand_protocol_set_status_reg_1(feature);
	}
	else if(((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FM) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_FM25G01B)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FM) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_FM25G02B)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FM) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_FM25G02C)))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);

		/* 2. Enable Qual mode */
		spi_nand_protocol_get_status_reg_2(&feature);
		feature |= 0x1;
		spi_nand_protocol_set_status_reg_2(feature);
	}
	else if(((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_XTX) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_XT26G02B)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_XTX) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_XT26G02A)))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);

		/* 2. Enable Qual mode */
		spi_nand_protocol_get_status_reg_2(&feature);
		feature |= 0x1;
		spi_nand_protocol_set_status_reg_2(feature);
	}
	else if(((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_MIRA) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_PSU1GS20BN)))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);
	}
	else if(((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_BIWIN) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_BWJX08U)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_BIWIN) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_BWET08U)))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);

		/* 2. Enable Qual mode */
		spi_nand_protocol_get_status_reg_2(&feature);
		feature |= 0x1;
		spi_nand_protocol_set_status_reg_2(feature);
	}
	else if(((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FORESEE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_FS35ND02GS2F1)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FORESEE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_FS35ND01GS1F1)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FORESEE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_FS35ND02GD1F1)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FORESEE) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_FS35ND01GD1F1)))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);

		/* 2. Enable Qual mode */
		spi_nand_protocol_get_status_reg_2(&feature);
		feature |= 0x1;
		spi_nand_protocol_set_status_reg_2(feature);
	}
	else if(((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_DS) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_DS35Q2GA)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_DS) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_DS35Q1GA)))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);

		spi_nand_protocol_get_status_reg_1(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After Unlock all block setup, the status register1 =0x%x\n", feature);
	}
	else if(((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FISON) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_CS11G0T0A0AA)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FISON) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_CS11G1T0A0AA)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_FISON) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_CS11G0G0A0AA)))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);

		spi_nand_protocol_get_status_reg_1(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After Unlock all block setup, the status register1 =0x%x\n", feature);
	}
	else if(((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_TYM) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_TYM25D2GA01)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_TYM) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_TYM25D2GA02)) ||
		((ptr_device_t->mfr_id == _SPI_NAND_MANUFACTURER_ID_TYM) && (ptr_device_t->dev_id == _SPI_NAND_DEVICE_ID_TYM25D1GA03)))
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC7;
		spi_nand_protocol_set_status_reg_1(feature);

		spi_nand_protocol_get_status_reg_1(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After Unlock all block setup, the status register1 =0x%x\n", feature);
	}
	else
	{
		/* 1. Unlock All block */
		spi_nand_protocol_get_status_reg_1(&feature);
		feature &= 0xC1;
		spi_nand_protocol_set_status_reg_1(feature);

		spi_nand_protocol_get_status_reg_1(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After Unlock all block setup, the status register1 =0x%x\n", feature);					

		/* 2. Enable Qual mode */
		spi_nand_protocol_get_status_reg_2(&feature);
		feature |= 0x1;
		spi_nand_protocol_set_status_reg_2(feature);

		spi_nand_protocol_get_status_reg_2(&feature);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "After enable qual mode setup, the status register2 =0x%x\n", feature);		
	}
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_NAND_FLASH_RTN_T spi_nand_probe( struct SPI_NAND_FLASH_INFO_T *ptr_rtn_device_t )
 * PURPOSE : To probe SPI NAND flash id.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : None
 *   OUTPUT: rtn_index  - The rtn_index variable of this function.
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/12 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T spi_nand_probe( struct SPI_NAND_FLASH_INFO_T *ptr_rtn_device_t )
{
	u32 i = 0;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_PROBE_ERROR;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_probe: start \n");

	/* Protocol for read id */
	spi_nand_protocol_read_id( ptr_rtn_device_t );

	for ( i=0 ; i < (sizeof(spi_nand_flash_tables)/sizeof(struct SPI_NAND_FLASH_INFO_T)) ; i++) 
	{
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1,"spi_nand_probe: table[%d]: mfr_id=0x%x, dev_id=0x%x\n", i, spi_nand_flash_tables[i].mfr_id, spi_nand_flash_tables[i].dev_id );

		if ( ( (ptr_rtn_device_t->mfr_id) == spi_nand_flash_tables[i].mfr_id) &&
		     ( (ptr_rtn_device_t->dev_id) == spi_nand_flash_tables[i].dev_id)  )
		{
			ptr_rtn_device_t->device_size	= spi_nand_flash_tables[i].device_size;
			ptr_rtn_device_t->erase_size	= spi_nand_flash_tables[i].erase_size;
			ptr_rtn_device_t->page_size	= spi_nand_flash_tables[i].page_size;
			ptr_rtn_device_t->oob_size	= spi_nand_flash_tables[i].oob_size;
			ptr_rtn_device_t->dummy_mode	= spi_nand_flash_tables[i].dummy_mode;
			ptr_rtn_device_t->read_mode	= spi_nand_flash_tables[i].read_mode;
			ptr_rtn_device_t->write_mode	= spi_nand_flash_tables[i].write_mode;
			memcpy( &(ptr_rtn_device_t->ptr_name), &(spi_nand_flash_tables[i].ptr_name), sizeof(ptr_rtn_device_t->ptr_name));
			memcpy( &(ptr_rtn_device_t->oob_free_layout), &(spi_nand_flash_tables[i].oob_free_layout), sizeof(ptr_rtn_device_t->oob_free_layout));
			ptr_rtn_device_t->feature = spi_nand_flash_tables[i].feature;

			rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
			break;
		}
	}

	if ( rtn_status != SPI_NAND_FLASH_RTN_NO_ERROR )
	{
		/* Another protocol for read id  (For example, the GigaDevice SPI NADN chip for Type C */
		spi_nand_protocol_read_id_2( ptr_rtn_device_t );

		for ( i=0 ; i < (sizeof(spi_nand_flash_tables)/sizeof(struct SPI_NAND_FLASH_INFO_T)) ; i++)
		{
			_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1,"spi_nand_probe: table[%d]: mfr_id=0x%x, dev_id=0x%x\n", i, spi_nand_flash_tables[i].mfr_id, spi_nand_flash_tables[i].dev_id );

			if ( ( (ptr_rtn_device_t->mfr_id) == spi_nand_flash_tables[i].mfr_id) &&
			     ( (ptr_rtn_device_t->dev_id) == spi_nand_flash_tables[i].dev_id)  ) 
			{
				ptr_rtn_device_t->device_size	= spi_nand_flash_tables[i].device_size;
				ptr_rtn_device_t->erase_size	= spi_nand_flash_tables[i].erase_size;
				ptr_rtn_device_t->page_size	= spi_nand_flash_tables[i].page_size;
				ptr_rtn_device_t->oob_size	= spi_nand_flash_tables[i].oob_size;
				ptr_rtn_device_t->dummy_mode	= spi_nand_flash_tables[i].dummy_mode;
				ptr_rtn_device_t->read_mode	= spi_nand_flash_tables[i].read_mode;
				ptr_rtn_device_t->write_mode	= spi_nand_flash_tables[i].write_mode;
				memcpy( &(ptr_rtn_device_t->ptr_name), &(spi_nand_flash_tables[i].ptr_name), sizeof(ptr_rtn_device_t->ptr_name));
				memcpy( &(ptr_rtn_device_t->oob_free_layout), &(spi_nand_flash_tables[i].oob_free_layout), sizeof(ptr_rtn_device_t->oob_free_layout));
				ptr_rtn_device_t->feature = spi_nand_flash_tables[i].feature;

				rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
				break;
			}
		}
	}

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_probe: mfr_id=0x%x, dev_id=0x%x\n",
		ptr_rtn_device_t->mfr_id, ptr_rtn_device_t->dev_id);

	if(rtn_status == SPI_NAND_FLASH_RTN_NO_ERROR)
	{
		spi_nand_manufacute_init(ptr_rtn_device_t);

		if ((ptr_rtn_device_t->write_mode == SPI_NAND_FLASH_WRITE_SPEED_MODE_QUAD) ||
		    (ptr_rtn_device_t->read_mode == SPI_NAND_FLASH_READ_SPEED_MODE_QUAD)) {
			VPint(IOMUX_CONTROL1) |= (1 << 19);
			VPint(IOMUX_CONTROL1) &= ~((1 << 18) | (1 << 11) | (1 << 8) | (1 << 7) | (1 << 3));
		}
	}

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "spi_nand_probe: end \n");

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: SPI_NAND_FLASH_RTN_T SPI_NAND_Flash_Init( void )
 * PURPOSE : To provide interface for SPI NAND init.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : None
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/12 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_NAND_FLASH_RTN_T SPI_NAND_Flash_Init(void)
{
	SPI_NAND_FLASH_RTN_T rtn_status;

	/* 1. set SFC Clock to 50MHZ */
	spi_nand_set_clock_speed(50);

	/* 2. Enable Manual Mode */
	_SPI_NAND_ENABLE_MANUAL_MODE();

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "%s: before init, mtx_mode_tog=0x%lx, manaul_en=0x%lx\n",
		__FUNCTION__, VPint(0xbfa10014), VPint(0xbfa10020) );

	/* 3. Probe flash information */
	if (spi_nand_probe(&_current_flash_info_t) != SPI_NAND_FLASH_RTN_NO_ERROR)
	{
		pr_warn("SPI NAND Flash Detected Error!\n");
		rtn_status = SPI_NAND_FLASH_RTN_PROBE_ERROR;
	}
	else
	{
		pr_info("Detected SPI NAND Flash: %s, Flash Size: %dMB\n",
			_current_flash_info_t.ptr_name, _current_flash_info_t.device_size / 1024 / 1024);
		rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
	}

	return (rtn_status);
}

/* EXPORTED SUBPROGRAM BODIES -------------------------------------------------------- */

#if defined(TCSUPPORT_NAND_BMT) && !defined(LZMA_IMG)
#define BMT_BAD_BLOCK_INDEX_OFFSET (1)
#define POOL_GOOD_BLOCK_PERCENT 8/100
#define MAX_BMT_SIZE_PERCENTAGE 1/10

int en7512_nand_exec_read_page(u32 page, u8* date, u8* oob)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	rtn_status = spi_nand_read_page(page, ptr_dev_info_t->read_mode);
	if(rtn_status == SPI_NAND_FLASH_RTN_NO_ERROR)
	{
		/* Get data segment and oob segment  */
		memcpy( date, &_current_cache_page_data[0], ptr_dev_info_t->page_size );
		memcpy( oob,  &_current_cache_page_oob_mapping[0], ptr_dev_info_t->oob_size );

		return 0;
	}
	else
	{
		_SPI_NAND_PRINTF( "en7512_nand_exec_read_page: read error, page=0x%x\n", page);
		return -1;
	}
}

int en7512_nand_check_block_bad(u32 offset, u32 bmt_block)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
	u32 page_number;

	if( (0xbc000000 <= offset) && (offset<= 0xbfffffff))		/* Reserver address area for system */
	{
		if( (offset & 0xbfc00000) == 0xbfc00000)
		{
			offset &= 0x003fffff;
		}
		else
		{
			offset &= 0x03ffffff;
		}
	}

	/* Calculate page number */
	page_number = (offset / (ptr_dev_info_t->page_size));

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "en7512_nand_check_block_bad: read_addr=0x%x, page_number=0x%x\n", offset, page_number);

	SPI_NAND_Flash_Clear_Read_Cache_Data();

	rtn_status = spi_nand_read_page(page_number, ptr_dev_info_t->read_mode);

#if 0	/* No matter what status of spi_nand_read_page, we must check badblock by oob. */
	/* Otherwise, the bmt pool size will be changed */
	if (rtn_status != SPI_NAND_FLASH_RTN_NO_ERROR)
	{
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "en7512_nand_check_block_bad return error, block:%d\n",
			offset / ptr_dev_info_t->erase_size);
		return 1;
	}
#endif

	if(bmt_block)
	{
		if(_current_cache_page_oob_mapping[BMT_BAD_BLOCK_INDEX_OFFSET] != 0xff)
		{
			_SPI_NAND_PRINTF("Bad block detected at page_addr 0x%x, oob_buf[%d] is 0x%x\n",
				page_number, BMT_BAD_BLOCK_INDEX_OFFSET,_current_cache_page_oob_mapping[BMT_BAD_BLOCK_INDEX_OFFSET]);
			return 1;
		}
	}
	else
	{
		if(_current_cache_page_oob_mapping[0] != 0xff)
		{
			_SPI_NAND_PRINTF("Bad block detected at page_addr 0x%x, oob_buf[%d] is 0x%x\n",
				page_number, 0, _current_cache_page_oob_mapping[0]);
			return 1;
		}
	}

	return 0;  /* Good Block*/
}

int en7512_nand_erase(u32 offset)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "en7512_nand_erase: offset =0x%x, erase_size=0x%x\n", offset, (ptr_dev_info_t->erase_size));

	SPI_NAND_Flash_Clear_Read_Cache_Data();

	rtn_status = spi_nand_erase_block((offset / (ptr_dev_info_t->erase_size)));
	if(rtn_status == SPI_NAND_FLASH_RTN_NO_ERROR)
	{
		return 0;
	}
	else
	{
		_SPI_NAND_PRINTF("en7512_nand_erase : Fail \n");
		return -1;
	}
}

int en7512_nand_mark_badblock(u32 offset, u32 bmt_block)
{
	u32 page_number;
	u8 buf[8];
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	/* Calculate page number */
	page_number = ((offset / ptr_dev_info_t->erase_size) * ptr_dev_info_t->erase_size) / ptr_dev_info_t->page_size;

	memset(buf, 0xFF, 8);
	if(bmt_block)
	{
		buf[BMT_BAD_BLOCK_INDEX_OFFSET] = 0;
	}
	else
	{
		buf[0] = 0;
	}

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "en7512_nand_mark_badblock: buf info:\n");
	_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &buf[0], 8);

	_SPI_NAND_PRINTF("en7512_nand_mark_badblock: page_num=0x%x\n", page_number);

	spi_nand_erase_block((offset / (ptr_dev_info_t->erase_size)));
	rtn_status = spi_nand_write_page(page_number, 0, NULL, 0, 0, &buf[0], 8, ptr_dev_info_t->write_mode);
	if( rtn_status == SPI_NAND_FLASH_RTN_NO_ERROR)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

int en7512_nand_exec_write_page(u32 page, u8 *dat, u8 *oob)
{
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "en7512_nand_exec_write_page: page=0x%x\n", page);

	rtn_status = spi_nand_write_page(page, 0, dat, ptr_dev_info_t->page_size, 0, oob, ptr_dev_info_t->oob_size , ptr_dev_info_t->write_mode);
	if( rtn_status == SPI_NAND_FLASH_RTN_NO_ERROR)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

static int calc_bmt_pool_size(struct mtd_info *mtd, u32 *bmt_block_count_max)
{
	struct nand_chip *nand = (struct nand_chip *)mtd->priv;
	int chip_size = nand->chipsize;
	int block_size = 1u << nand->phys_erase_shift;
	int total_block = chip_size / block_size;
	int last_block = total_block - 1;
	u16 valid_block_num = 0;
	u16 need_valid_block_num = total_block * POOL_GOOD_BLOCK_PERCENT;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "calc_bmt_pool_size : need_valid_block_num=0x%x, total_block=0x%x\n",
		need_valid_block_num, total_block);

	*bmt_block_count_max = total_block * MAX_BMT_SIZE_PERCENTAGE;

	for (;last_block > 0; --last_block)
	{
		if (en7512_nand_check_block_bad(last_block * block_size, BAD_BLOCK_RAW))
		{
			continue;
		}
		else
		{
			valid_block_num++;
			if (valid_block_num == need_valid_block_num)
			{
				break;
			}
		}
	}

	return (total_block - last_block);
}

static int init_bmt_bbt(struct mtd_info *mtd)
{
	int bmt_pool_size;
	u32 bmt_block_count_max = 0;

	bmt_pool_size = calc_bmt_pool_size(mtd, &bmt_block_count_max);
	if ((u32)bmt_pool_size > bmt_block_count_max)
	{
		_SPI_NAND_PRINTF("Error: bmt pool size: %d > maximum size %d\n", bmt_pool_size, bmt_block_count_max);
		_SPI_NAND_PRINTF("Error: init bmt failed \n");
		return -1;
	}

	if (bmt_pool_size > MAX_BMT_SIZE)
		bmt_pool_size = MAX_BMT_SIZE;

	pr_info("BMT pool size: %d\n", bmt_pool_size);

	if (!g_bmt)
	{
		g_bmt = init_bmt((struct nand_chip *)mtd->priv, bmt_pool_size);
		if (!g_bmt)
		{
			_SPI_NAND_PRINTF("Error: init bmt failed \n");
			return -1;
		}
	}

	if (!g_bbt)
	{
		g_bbt = start_init_bbt();
		if (!g_bbt)
		{
			_SPI_NAND_PRINTF("Error: init bbt failed \n");
			return -1;
		}
	}

	if (write_bbt_or_bmt_to_flash() != 0)
	{
		_SPI_NAND_PRINTF("Error: save bbt or bmt to nand failed \n");
		return -1;
	}

	if (create_badblock_table_by_bbt())
	{
		_SPI_NAND_PRINTF("Error: create bad block table failed \n");
		return -1;
	}

	pr_info("BMT & BBT init success\n");

	mtd->size = nand_logic_size;

	return 0;
}
#endif

/*****************************************************************************************/
/*****************************************************************************************/
/* Porting */
/*****************************************************************************************/
/*****************************************************************************************/

/* feature/ status reg */
#define REG_BLOCK_LOCK			0xa0
#define REG_OTP				0xb0
#define REG_STATUS			0xc0	/* timing */

/* status */
#define STATUS_OIP_MASK			0x01
#define STATUS_READY			(0 << 0)
#define STATUS_BUSY			(1 << 0)

#define STATUS_E_FAIL_MASK		0x04
#define STATUS_E_FAIL			(1 << 2)

#define STATUS_P_FAIL_MASK		0x08
#define STATUS_P_FAIL			(1 << 3)

#define STATUS_ECC_MASK			0x30
#define STATUS_ECC_1BIT_CORRECTED	(1 << 4)
#define STATUS_ECC_ERROR		(2 << 4)
#define STATUS_ECC_RESERVED		(3 << 4)

/*ECC enable defines*/
#define OTP_ECC_MASK			0x10
#define OTP_ECC_OFF			0
#define OTP_ECC_ON			1

#define BUFSIZE				(2 * 2048)

static inline struct spinand_state *mtd_to_state(struct mtd_info *mtd)
{
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	struct en75xx_spinand_host *host = (struct en75xx_spinand_host *)chip->priv;

	return &host->state;
}

/*
 * spinand_read_id- Read SPI Nand ID
 * Description:
 *    Read ID: read two ID bytes from the SPI Nand device
 */
static inline int spinand_read_id(u8 *id)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;

	(id[0]) = ptr_dev_info_t->mfr_id;
	(id[1]) = ptr_dev_info_t->dev_id;

	return 0;
}

/*
 * spinand_read_status- send command 0xf to the SPI Nand status register
 * Description:
 *    After read, write, or erase, the Nand device is expected to set the
 *    busy status.
 *    This function is to allow reading the status of the command: read,
 *    write, and erase.
 *    Once the status turns to be ready, the other status bits also are
 *    valid status bits.
 */
static inline int spinand_read_status(uint8_t *status)
{
	return spi_nand_protocol_get_status_reg_3(status);
}

#define MAX_WAIT_JIFFIES  (40 * HZ)
static int wait_till_ready(void)
{
	unsigned long deadline;
	int retval;
	u8 stat = 0;

	deadline = jiffies + MAX_WAIT_JIFFIES;
	do {
		retval = spinand_read_status(&stat);
		if (retval < 0)
			return -1;
		else if (!(stat & 0x1))
			break;

		cond_resched();
	} while (!time_after_eq(jiffies, deadline));

	if ((stat & 0x1) == 0)
		return 0;

	return -1;
}
/**
 * spinand_get_otp- send command 0xf to read the SPI Nand OTP register
 * Description:
 *   There is one bit( bit 0x10 ) to set or to clear the internal ECC.
 *   Enable chip internal ECC, set the bit to 1
 *   Disable chip internal ECC, clear the bit to 0
 */
static int spinand_get_otp(u8 *otp)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;

	if( ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_PN) ||
	    ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_FM) ||
	    ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_FORESEE) ||
	   (((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_XTX) && ((ptr_dev_info_t->dev_id) == _SPI_NAND_DEVICE_ID_XT26G02B)))
	{
		return spi_nand_protocol_get_feature(_SPI_NAND_ADDR_ECC, otp);
	}
	else
	{
		return spi_nand_protocol_get_status_reg_2(otp);
	}
}

/**
 * spinand_set_otp- send command 0x1f to write the SPI Nand OTP register
 * Description:
 *   There is one bit( bit 0x10 ) to set or to clear the internal ECC.
 *   Enable chip internal ECC, set the bit to 1
 *   Disable chip internal ECC, clear the bit to 0
 */
static int spinand_set_otp(u8 *otp)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;

	if( ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_PN) ||
	    ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_FM) ||
	    ((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_FORESEE) ||
	   (((ptr_dev_info_t->mfr_id) == _SPI_NAND_MANUFACTURER_ID_XTX) && ((ptr_dev_info_t->dev_id) == _SPI_NAND_DEVICE_ID_XT26G02B)))
	{
		return spi_nand_protocol_set_feature(_SPI_NAND_ADDR_ECC, *otp);
	}
	else
	{
		return spi_nand_protocol_set_status_reg_2(*otp);
	}
}

#ifdef SPINAND_ONDIEECC
/**
 * spinand_enable_ecc- send command 0x1f to write the SPI Nand OTP register
 * Description:
 *   There is one bit( bit 0x10 ) to set or to clear the internal ECC.
 *   Enable chip internal ECC, set the bit to 1
 *   Disable chip internal ECC, clear the bit to 0
 */
static int spinand_enable_ecc(void)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	int i, retval;
	u8 otp = 0;
	u8 die_num;

	if (((ptr_dev_info_t->feature) & SPI_NAND_FLASH_DIE_SELECT_1_HAVE)) {
		die_num = (ptr_dev_info_t->device_size / ptr_dev_info_t->page_size) >> 16;

		for(i = 0; i < die_num; i++) {
			spi_nand_protocol_die_select_1(i);

			retval = spinand_get_otp(&otp);
			if (retval < 0) {
				return retval;
			}

			if ((otp & OTP_ECC_MASK) == OTP_ECC_MASK) {
				continue;
			}

			otp |= OTP_ECC_MASK;
			retval = spinand_set_otp(&otp);
			if (retval < 0) {
				return retval;
			}
		}
	} else if(((ptr_dev_info_t->feature) & SPI_NAND_FLASH_DIE_SELECT_2_HAVE)) {
		die_num = (ptr_dev_info_t->device_size / ptr_dev_info_t->page_size) >> 17;

		for(i = 0; i < die_num; i++) {
			spi_nand_protocol_die_select_2(i);

			retval = spinand_get_otp(&otp);
			if (retval < 0) {
				return retval;
			}

			if ((otp & OTP_ECC_MASK) == OTP_ECC_MASK)
				continue;

			otp |= OTP_ECC_MASK;
			retval = spinand_set_otp(&otp);
			if (retval < 0) {
				return retval;
			}
		}
	} else {
		retval = spinand_get_otp(&otp);
		if (retval < 0) {
			return retval;
		}

		if ((otp & OTP_ECC_MASK) == OTP_ECC_MASK) {
			_ondie_ecc_flag = 1;
			return 0;
		}

		otp |= OTP_ECC_MASK;
		retval = spinand_set_otp(&otp);
		if (retval < 0) {
			return retval;
		}
	}

	_ondie_ecc_flag = 1;

	return spinand_get_otp(&otp);
}
#endif

#if !defined(SPINAND_ONDIEECC)
static int spinand_disable_ecc(void)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	int i, retval;
	u8 otp = 0;
	u8 die_num;

	if (((ptr_dev_info_t->feature) & SPI_NAND_FLASH_DIE_SELECT_1_HAVE)) {
		die_num = (ptr_dev_info_t->device_size / ptr_dev_info_t->page_size) >> 16;

		for(i = 0; i < die_num; i++) {
			spi_nand_protocol_die_select_1(i);

			retval = spinand_get_otp(spi_nand, &otp);
			if (retval < 0) {
				return retval;
			}

			if ((otp & OTP_ECC_MASK) == OTP_ECC_MASK) {
				otp &= ~OTP_ECC_MASK;
				retval = spinand_set_otp(spi_nand, &otp);
				if (retval < 0) {
					return retval;
				}
			}
		}
	} else if(((ptr_dev_info_t->feature) & SPI_NAND_FLASH_DIE_SELECT_2_HAVE)) {
		die_num = (ptr_dev_info_t->device_size / ptr_dev_info_t->page_size) >> 17;

		for(i = 0; i < die_num; i++) {
			spi_nand_protocol_die_select_2(i);

			retval = spinand_get_otp(spi_nand, &otp);
			if (retval < 0) {
				return retval;
			}

			if ((otp & OTP_ECC_MASK) == OTP_ECC_MASK) {
				otp &= ~OTP_ECC_MASK;
				retval = spinand_set_otp(spi_nand, &otp);
				if (retval < 0) {
					return retval;
				}
			}
		}
	} else {
		retval = spinand_get_otp(spi_nand, &otp);
		if (retval < 0)
			return retval;

		if ((otp & OTP_ECC_MASK) == OTP_ECC_MASK) {
			otp &= ~OTP_ECC_MASK;
			retval = spinand_set_otp(spi_nand, &otp);
			if (retval < 0) {
				return retval;
			}
			_ondie_ecc_flag = 0;
			return spinand_get_otp(spi_nand, &otp);
		}
	}

	_ondie_ecc_flag = 0;

	return 0;
}
#endif

/**
 * spinand_program_page--to write a page with:
 * @page_id: the physical page location to write the page.
 * @offset:  the location from the cache starting from 0 to 2111
 * @len:     the number of bytes to write
 * @wbuf:    the buffer to hold the number of bytes
 *
 * Description:
 *   The commands used here are 0x06, 0x84, and 0x10--indicating that
 *   the write enable is first sent, the write cache command, and the
 *   write execute command.
 *   Poll to wait for the tPROG time to finish the transaction.
 */
static int spinand_program_page(struct mtd_info *mtd,
				u32 page_id, u32 offset, u32 len, u8 *buf)
{
	struct spinand_state *state = mtd_to_state(mtd);
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_program_page]: enter, page=0x%x\n", page_id);
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spinand_program_page: _current_cache_page_oob_mapping: state->oob_buf_len=0x%x, state->oob_buf=\n", (state->oob_buf_len));
	_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &(state->oob_buf[0]), (state->oob_buf_len));

	rtn_status = spi_nand_write_page_internal(page_id, (state->buf_idx), &state->buf[(state->buf_idx)], (state->buf_len),  0, (&state->oob_buf[0]), (state->oob_buf_len), ptr_dev_info_t->write_mode);
	if( rtn_status == SPI_NAND_FLASH_RTN_NO_ERROR)
	{
		return 0;
	}
	else
	{
		_SPI_NAND_PRINTF("spinand_program_page, error\n");
		return -1;
	}
}

/**
 * spinand_erase_block--to erase a page with:
 * @block_id: the physical block location to erase.
 *
 * Description:
 *   The commands used here are 0x06 and 0xd8--indicating an erase
 *   command to erase one block--64 pages
 *   It will first to enable the write enable bit (0x06 command),
 *   and then send the 0xd8 erase command
 *   Poll to wait for the tERS time to complete the tranaction.
 */
static int spinand_erase_block(u32 block_id)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1,"[spinand_erase_block]: enter, block id=0x%x \n", block_id);

	rtn_status = spi_nand_erase_internal( ((block_id)*(ptr_dev_info_t->erase_size)) , (ptr_dev_info_t->erase_size) );
	if(rtn_status == SPI_NAND_FLASH_RTN_NO_ERROR)
	{
		return 0;
	}
	else
	{
		_SPI_NAND_PRINTF("spinand_erase_block, error\n");
		return -1;
	}
}

static void spinand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct spinand_state *state = mtd_to_state(mtd);
	int min_oob_size;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_write_buf]: enter \n");

	if( (state->col) >= (mtd->writesize))	/* Write OOB area */
	{
		min_oob_size = MIN(len, (MAX_LINUX_USE_OOB_SIZE - ((state->col) - (mtd->writesize))));
		memcpy( &(state->oob_buf)[((state->col)-(mtd->writesize))+LINUX_USE_OOB_START_OFFSET], buf, min_oob_size);
		state->col += min_oob_size;
		state->oob_buf_len = min_oob_size;
	}
	else /* Write Data area */
	{
		memcpy( &(state->buf)[state->buf_idx], buf, len);
		state->col += len;
		state->buf_len += len;
	}
}

static void spinand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct spinand_state *state = mtd_to_state(mtd);
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_read_buf]: enter, len=0x%x, offset=0x%x\n", len, state->buf_idx);

	if( ((state->command) == NAND_CMD_READID) ||
	    ((state->command) == NAND_CMD_STATUS))
	{
		memcpy(buf, &state->buf, len);
	}
	else
	{
		if( (state->buf_idx) < ( ptr_dev_info_t->page_size ))		/* Read data area */
		{
			memcpy(buf, &_current_cache_page_data[state->buf_idx], len);
		}
		else	/* Read oob area */
		{
			/* dump_stack(); */
			memcpy(buf, &_current_cache_page_oob_mapping[ ((state->buf_idx)-(ptr_dev_info_t->page_size))+ LINUX_USE_OOB_START_OFFSET], len);
		}
	}

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spinand_read_buf : idx=0x%x, len=0x%x\n", (state->buf_idx), len);
	_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &buf[0], len);

	state->buf_idx += len;
}

#ifdef SPINAND_ONDIEECC
static int
spinand_write_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip, const uint8_t *buf, int oob_required, int page)
{
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_write_page_hwecc]: enter \n");

	spinand_write_buf(mtd, buf, mtd->writesize);
	spinand_write_buf(mtd, chip->oob_poi, mtd->oobsize);

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spinand_write_page_hwecc: data=\n");
	_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &buf[0], mtd->writesize);
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spinand_write_page_hwecc: oob=\n");
	_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &chip->oob_poi[0], mtd->oobsize);

	return 0;
}

static int
spinand_read_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip, uint8_t *buf, int oob_required, int page)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_read_page_hwecc]: enter, page=0x%x \n", page);

	if (buf == NULL)
	{
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_read_page_hwecc]buf is NULL\n");
	}

	memcpy(buf, &_current_cache_page_data[0], (ptr_dev_info_t->page_size));
	memcpy((chip->oob_poi), &_current_cache_page_oob_mapping[LINUX_USE_OOB_START_OFFSET], MAX_LINUX_USE_OOB_SIZE);

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spinand_read_page_hwecc: data:\n");
	_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &buf[0], mtd->writesize);
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spinand_read_page_hwecc: oob:\n");
	_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &chip->oob_poi[0], mtd->oobsize);
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_2, "spinand_read_page_hwecc: _current_cache_page_oob_mapping:\n");
	_SPI_NAND_DEBUG_PRINTF_ARRAY(SPI_NAND_FLASH_DEBUG_LEVEL_2, &_current_cache_page_oob_mapping[0], mtd->oobsize);

	return 0;
}
#endif

static int spinand_write_oob(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	struct spinand_state *state = mtd_to_state(mtd);
	SPI_NAND_FLASH_RTN_T rtn_status = SPI_NAND_FLASH_RTN_NO_ERROR;
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_write_oob]: enter, page=0x%x \n", page);

	memset(state->oob_buf, 0xff, MAX_USE_OOB_SIZE);
	memcpy(&(state->oob_buf)[LINUX_USE_OOB_START_OFFSET], chip->oob_poi, MAX_LINUX_USE_OOB_SIZE);

	rtn_status = spi_nand_write_page_internal(page, 0, NULL, 0,  0, (&state->oob_buf[0]), MAX_LINUX_USE_OOB_SIZE, ptr_dev_info_t->write_mode);
	if (rtn_status == SPI_NAND_FLASH_RTN_NO_ERROR)
	{
		return 0;
	}
	else
	{
		return -EIO;
	}
}

static int spinand_block_markbad(struct mtd_info *mtd, loff_t offset)
{
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_block_markbad]: enter , offset=0x%x\n", offset);
	return 0;
}

static int spinand_block_bad(struct mtd_info *mtd, loff_t ofs)
{
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_block_bad]: enter \n");
	return 0;
}

static void spinand_select_chip(struct mtd_info *mtd, int dev)
{
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_select_chip]: enter \n");
}

static int spinand_dev_ready(struct mtd_info *mtd)
{
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_dev_ready]: enter \n");
	return 1;
}

static void spinand_enable_hwecc(struct mtd_info *mtd, int mode)
{
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_enable_hwecc]: enter \n");
}

static int spinand_correct_data(struct mtd_info *mtd, u_char *dat,
				u_char *read_ecc, u_char *calc_ecc)
{
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_correct_data]: enter \n");
	return 0;
}

static int spinand_calculate_ecc(struct mtd_info *mtd, const u_char *dat, u_char *ecc_code)
{
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_calculate_ecc]: enter \n");
	return 0;
}

static uint8_t spinand_read_byte(struct mtd_info *mtd)
{
	struct spinand_state *state = mtd_to_state(mtd);
	u8 data;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_read_byte]: enter \n");

	data = state->buf[state->buf_idx];
	state->buf_idx++;

	return data;
}

static int spinand_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	unsigned long timeo = jiffies;
	int retval, state = chip->state;
	u8 status;

	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_wait]: enter \n");

	if (state == FL_ERASING)
		timeo += (HZ * 400) / 1000;
	else
		timeo += (HZ * 20) / 1000;

	while (time_before(jiffies, timeo)) {
		retval = spinand_read_status(&status);
		if ((status & STATUS_OIP_MASK) == STATUS_READY)
			return 0;
		cond_resched();
	}

	return 0;
}

/*
 * spinand_reset- send RESET command "0xff" to the Nand device.
 */
static void spinand_reset(void)
{
	_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_reset]: enter \n");

	spi_nand_protocol_reset();
}

static void spinand_cmdfunc(struct mtd_info *mtd, unsigned int command, int column, int page)
{
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;
	struct spinand_state *state = mtd_to_state(mtd);
	u16 block_id;

	state->command = command;

	switch (command) {
	/*
	 * READ0 - read in first  0x800 bytes
	 */
	case NAND_CMD_READ1:
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_cmdfunc]: NAND_CMD_READ1 \n");
	case NAND_CMD_READ0:
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_cmdfunc]: NAND_CMD_READ0 \n");	
		state->buf_idx = column;
		spi_nand_read_page_internal(page, ptr_dev_info_t->read_mode);
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1,"[spinand_cmdfunc]: NAND_CMD_READ0/1, End\n");
		break;
	/* READOOB reads only the OOB because no ECC is performed. */
	case NAND_CMD_READOOB:
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_cmdfunc]: NAND_CMD_READOOB, page=0x%x \n", page);
		state->buf_idx = column + (ptr_dev_info_t->page_size);
		spi_nand_read_page_internal(page, ptr_dev_info_t->read_mode);
		break;
	case NAND_CMD_RNDOUT:
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_cmdfunc]: NAND_CMD_RNDOUT \n");
		state->buf_idx = column;
		break;
	case NAND_CMD_READID:
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_cmdfunc]: NAND_CMD_READID \n");
		state->buf_idx = 0;
		spinand_read_id((u8 *)state->buf);
		break;
	/* ERASE1 stores the block and page address */
	case NAND_CMD_ERASE1:
		block_id = page /((mtd->erasesize)/(mtd->writesize));
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_cmdfunc]: NAND_CMD_ERASE1 \n");
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "erasesize=0x%x, writesiez=0x%x, page=0x%x, block_idx=0x%x\n", (mtd->erasesize), (mtd->writesize), page, block_id);
		spinand_erase_block(block_id);
		break;
	/* ERASE2 uses the block and page address from ERASE1 */
	case NAND_CMD_ERASE2:
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1,"[spinand_cmdfunc]: NAND_CMD_ERASE2 \n");
		break;
	/* SEQIN sets up the addr buffer and all registers except the length */
	case NAND_CMD_SEQIN:
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_cmdfunc]: NAND_CMD_SEQIN \n");
		state->col = column;
		state->row = page;
		state->buf_idx = column;
		state->buf_len = 0;
		state->oob_buf_len = 0 ;
		memset(state->buf, 0xff, BUFSIZE);
		memset(state->oob_buf, 0xff, MAX_USE_OOB_SIZE);
		break;
	/* PAGEPROG reuses all of the setup from SEQIN and adds the length */
	case NAND_CMD_PAGEPROG:
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_cmdfunc]: NAND_CMD_PAGEPROG \n");
		spinand_program_page(mtd, state->row, state->col,
			state->buf_idx, state->buf);
		break;
	case NAND_CMD_STATUS:
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_cmdfunc]: NAND_CMD_STATUS \n");
		spinand_get_otp(state->buf);
		if (!(state->buf[0] & 0x80))
			state->buf[0] = 0x80;
		state->buf_idx = 0;
		break;
	/* RESET command */
	case NAND_CMD_RESET:
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1, "[spinand_cmdfunc]: NAND_CMD_RESET \n");
		if (wait_till_ready())
			pr_warn("WAIT timedout!!!\n");
		/* a minimum of 250us must elapse before issuing RESET cmd*/
		udelay(250);
		spinand_reset();
		break;
	default:
		_SPI_NAND_DEBUG_PRINTF(SPI_NAND_FLASH_DEBUG_LEVEL_1,"[spinand_cmdfunc]: Unknown CMD: 0x%x\n", command);
	}
}

#ifdef SPINAND_ONDIEECC
static int spinand_ooblayout_ecc(struct mtd_info *mtd, int section,
				 struct mtd_oob_region *oobregion)
{
	return -ERANGE;
}

static int spinand_ooblayout_free(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *oobregion)
{
	if (section > 0)
		return -ERANGE;

	oobregion->offset = 0;
	oobregion->length = MAX_LINUX_USE_OOB_SIZE;

	return 0;
}

static const struct mtd_ooblayout_ops spinand_ooblayout_ops = {
	.ecc = spinand_ooblayout_ecc,
	.free = spinand_ooblayout_free,
};
#endif

static int en75xx_spinand_probe(void)
{
	struct en75xx_spinand_host *host;
	struct nand_chip *chip;
	struct mtd_info *mtd;
	struct nand_flash_dev spi_nand_flash_ids[2];
	struct SPI_NAND_FLASH_INFO_T *ptr_dev_info_t;
	int err = 0;

	/* 1. Allocate neccessary struct memory, and assigned related pointer */
	host = kzalloc(sizeof(struct en75xx_spinand_host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->state.buf = kzalloc(BUFSIZE, GFP_KERNEL); /* Data buffer */
	if (!host->state.buf)
	{
		err = -ENOMEM;
		goto out_err_free;
	}

	host->state.oob_buf = kzalloc(MAX_USE_OOB_SIZE, GFP_KERNEL); /* OOB buffer */
	if (!host->state.oob_buf)
	{
		err = -ENOMEM;
		goto out_err_free;
	}

	/* fill entire OOB cache by 0xff */
	memset(_current_cache_page_oob, 0xff, sizeof(_current_cache_page_oob));
	memset(_current_cache_page_oob_mapping, 0xff, sizeof(_current_cache_page_oob_mapping));

	SPI_NAND_Flash_Init();

	ptr_dev_info_t = _SPI_NAND_GET_DEVICE_INFO_PTR;

	chip = &host->nand_chip;
	chip->priv			= host;
	chip->read_byte			= spinand_read_byte;
	chip->read_buf			= spinand_read_buf;
	chip->write_buf			= spinand_write_buf;
	chip->waitfunc			= spinand_wait;
	chip->select_chip		= spinand_select_chip;
	chip->dev_ready			= spinand_dev_ready;
	chip->cmdfunc			= spinand_cmdfunc;
#ifdef SPINAND_ONDIEECC
	chip->ecc.mode			= NAND_ECC_HW;
	chip->ecc.size			= 0x200;
	chip->ecc.bytes			= 0x4;
	chip->ecc.steps			= 0x4;
	chip->ecc.total			= chip->ecc.steps * chip->ecc.bytes;
	chip->ecc.strength		= 1;
	chip->ecc.read_page		= spinand_read_page_hwecc;
	chip->ecc.write_page		= spinand_write_page_hwecc;

	/* enable OnDie ECC */
	if (spinand_enable_ecc() < 0)
		pr_warn("%s: enable ECC failed!\n", __func__);
#else
	chip->ecc.mode = NAND_ECC_SOFT;

	/* disable OnDie ECC */
	if (spinand_disable_ecc() < 0)
		pr_warn("%s: disable ECC failed!\n", __func__);
#endif

	/* for BMT, we need to revise driver architecture */
	chip->block_markbad		= spinand_block_markbad;	/* tmp null */
	chip->block_bad			= spinand_block_bad;		/* tmp null */
	chip->ecc.write_oob		= spinand_write_oob;
	chip->ecc.calculate		= spinand_calculate_ecc;
	chip->ecc.correct		= spinand_correct_data;
	chip->ecc.hwctl			= spinand_enable_hwecc;

	/* Chip does not allow subpage writes. */
	chip->options |= NAND_NO_SUBPAGE_WRITE;

	/* To skips the bbt scan during initialization. */
	chip->options |= NAND_SKIP_BBTSCAN;

	host->mtd = nand_to_mtd(chip);
	mtd = host->mtd;
	mtd->priv			= chip;
	mtd->name			= "EN7512-SPI_NAND";
	mtd->owner			= THIS_MODULE;
	mtd->oobsize			= MAX_LINUX_USE_OOB_SIZE;

#ifdef SPINAND_ONDIEECC
	mtd_set_ooblayout(mtd, &spinand_ooblayout_ops);
#endif

	memset(spi_nand_flash_ids, 0, sizeof(spi_nand_flash_ids));
	spi_nand_flash_ids[0].name	= ptr_dev_info_t->ptr_name;
	spi_nand_flash_ids[0].dev_id	= ptr_dev_info_t->dev_id;
	spi_nand_flash_ids[0].pagesize	= ptr_dev_info_t->page_size;
	spi_nand_flash_ids[0].chipsize	= (ptr_dev_info_t->device_size) >> 20;
	spi_nand_flash_ids[0].erasesize	= ptr_dev_info_t->erase_size;

	if (nand_scan_ident(mtd, 1, spi_nand_flash_ids))
	{
		pr_warn("%s fail!\n", "nand_scan_ident");
		err = -ENXIO;
		goto out_err_free;
	}

	if (nand_scan_tail(mtd))
	{
		pr_warn("%s fail!\n", "nand_scan_tail");
		err = -ENXIO;
		goto out_err_free;
	}

#if defined(TCSUPPORT_NAND_BMT) && !defined(LZMA_IMG)
	if (init_bmt_bbt(mtd))
	{
		err = -ENXIO;
		goto out_err_release;
	}
#endif

	/* register the partitions */
	err = mtd_device_parse_register(mtd, NULL, NULL, NULL, 0);
	if (err)
		goto out_err_release;

	g_en75xx_mtd = mtd;

	return 0;

out_err_release:
	nand_release(mtd);

out_err_free:
	kfree(host->state.oob_buf);
	kfree(host->state.buf);
	kfree(host);

	return err;
}

static int __init spi_nand_flash_init(void)
{
	pr_info("EcoNET SPI NAND driver init\n");

	if (IS_SPIFLASH) {
		/* boot from SPI NOR */
		pr_warn("EN75XX board use SPI NOR flash!\n");
		return -1;
	}

	return en75xx_spinand_probe();
}

static void __init spi_nand_flash_exit(void)
{
	struct mtd_info *mtd = g_en75xx_mtd;
	struct nand_chip *chip;
	struct en75xx_spinand_host *host;

	if (!mtd)
		return;

	chip = (struct nand_chip *)mtd->priv;
	host = (struct en75xx_spinand_host *)chip->priv;

	g_en75xx_mtd = NULL;

	nand_release(mtd);
	kfree(host->state.oob_buf);
	kfree(host->state.buf);
	kfree(host);
}

module_init(spi_nand_flash_init);
module_exit(spi_nand_flash_exit);

/* End of [spi_nand_flash.c] package */
