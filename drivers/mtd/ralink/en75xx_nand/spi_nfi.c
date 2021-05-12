/***************************************************************************************
 *      Copyright(c) 2016 ECONET Incorporation All rights reserved.
 *
 *      This is unpublished proprietary source code of ECONET Incorporation
 *
 *      The copyright notice above does not evidence any actual or intended
 *      publication of such source code.
 ***************************************************************************************
 */

/*======================================================================================
 * MODULE NAME: spi
 * FILE NAME: spi_nfi.c
 * DATE: 2016/03/18
 * VERSION: 1.00
 * PURPOSE: To Provide SPI NFI(DMA) Access Internace.
 * NOTES:
 *
 * AUTHOR : Chuck Kuo         REVIEWED by
 *
 * FUNCTIONS
 *
 * DEPENDENCIES
 *
 * * $History: $
 * MODIFICTION HISTORY:
 * Version 1.00 - Date 2016/03/18 By Chuck Kuo
 * ** This is the first versoin for creating to support the functions of
 *    current module.
 *
 *======================================================================================
 */


/* INCLUDE FILE DECLARATIONS --------------------------------------------------------- */
#include <asm/io.h>
#include <asm/string.h>
#include <asm/tc3162/tc3162.h>

#include <linux/delay.h>

#include "spi_nfi.h"

/* NAMING CONSTANT DECLARATIONS ------------------------------------------------------ */

#define delay1us	udelay

/*******************************************************************************
 * NFI Register Definition
 *******************************************************************************/
#define _SPI_NFI_REGS_BASE			0xBFA11000
#define _SPI_NFI_REGS_CNFG			(_SPI_NFI_REGS_BASE + 0x0000)
#define _SPI_NFI_REGS_PAGEFMT		(_SPI_NFI_REGS_BASE + 0x0004)
#define _SPI_NFI_REGS_CON			(_SPI_NFI_REGS_BASE + 0x0008)
#define _SPI_NFI_REGS_INTR_EN		(_SPI_NFI_REGS_BASE + 0x0010)
#define _SPI_NFI_REGS_INTR			(_SPI_NFI_REGS_BASE + 0x0014)
#define _SPI_NFI_REGS_CMD			(_SPI_NFI_REGS_BASE + 0x0020)
#define _SPI_NFI_REGS_STA			(_SPI_NFI_REGS_BASE + 0x0060)
#define _SPI_NFI_REGS_FIFOSTA		(_SPI_NFI_REGS_BASE + 0x0064)
#define _SPI_NFI_REGS_STRADDR		(_SPI_NFI_REGS_BASE + 0x0080)
#define _SPI_NFI_REGS_FDM0L			(_SPI_NFI_REGS_BASE + 0x00A0)
#define _SPI_NFI_REGS_FDM0M			(_SPI_NFI_REGS_BASE + 0x00A4)
#define _SPI_NFI_REGS_FDM7L			(_SPI_NFI_REGS_BASE + 0x00D8)
#define _SPI_NFI_REGS_FDM7M			(_SPI_NFI_REGS_BASE + 0x00DC)
#define _SPI_NFI_REGS_FIFODATA0		(_SPI_NFI_REGS_BASE + 0x0190)
#define _SPI_NFI_REGS_FIFODATA1		(_SPI_NFI_REGS_BASE + 0x0194)
#define _SPI_NFI_REGS_FIFODATA2		(_SPI_NFI_REGS_BASE + 0x0198)
#define _SPI_NFI_REGS_FIFODATA3		(_SPI_NFI_REGS_BASE + 0x019C)
#define _SPI_NFI_REGS_MASTERSTA		(_SPI_NFI_REGS_BASE + 0x0224)
#define _SPI_NFI_REGS_SECCUS_SIZE	(_SPI_NFI_REGS_BASE + 0x022C)
#define _SPI_NFI_REGS_RD_CTL2		(_SPI_NFI_REGS_BASE + 0x0510)
#define _SPI_NFI_REGS_RD_CTL3		(_SPI_NFI_REGS_BASE + 0x0514)
#define _SPI_NFI_REGS_PG_CTL1		(_SPI_NFI_REGS_BASE + 0x0524)
#define _SPI_NFI_REGS_PG_CTL2		(_SPI_NFI_REGS_BASE + 0x0528)
#define _SPI_NFI_REGS_NOR_PROG_ADDR	(_SPI_NFI_REGS_BASE + 0x052C)
#define _SPI_NFI_REGS_NOR_RD_ADDR	(_SPI_NFI_REGS_BASE + 0x0534)
#define _SPI_NFI_REGS_SNF_MISC_CTL	(_SPI_NFI_REGS_BASE + 0x0538)
#define _SPI_NFI_REGS_SNF_MISC_CTL2	(_SPI_NFI_REGS_BASE + 0x053C)
#define _SPI_NFI_REGS_SNF_STA_CTL1	(_SPI_NFI_REGS_BASE + 0x0550)
#define _SPI_NFI_REGS_SNF_STA_CTL2	(_SPI_NFI_REGS_BASE + 0x0554)


/*******************************************************************************
 * NFI Register Field Definition
 *******************************************************************************/

/* NFI_CNFG */
#define _SPI_NFI_REGS_CNFG_AHB						(0x0001)
#define _SPI_NFI_REGS_CNFG_READ_EN					(0x0002)
#define _SPI_NFI_REGS_CNFG_DMA_BURST_EN				(0x0004)
/* Flash -> SRAM */
#define _SPI_NFI_REGS_CNFG_DMA_WR_SWAP_EN			(0x0008)
/* SRAM -> Flash */
#define _SPI_NFI_REGS_CNFG_DMA_RD_SWAP_EN			(0x0010)
#define _SPI_NFI_REGS_CNFG_ECC_DATA_SOURCE_INV_EN	(0x0020)
#define _SPI_NFI_REGS_CNFG_HW_ECC_EN				(0x0100)
#define _SPI_NFI_REGS_CNFG_AUTO_FMT_EN				(0x0200)

#define _SPI_NFI_REGS_CONF_OP_PRGM			(3)
#define _SPI_NFI_REGS_CONF_OP_READ			(6)
#define _SPI_NFI_REGS_CONF_OP_MASK			(0x7000)
#define _SPI_NFI_REGS_CONF_OP_SHIFT			(12)

/* Flash -> SRAM */
#define _SPI_NFI_REGS_CNFG_DMA_WR_SWAP_SHIFT	(0x0003)
/* SRAM -> Flash */
#define _SPI_NFI_REGS_CNFG_DMA_RD_SWAP_SHIFT	(0x0004)
#define _SPI_NFI_REGS_CNFG_DMA_WR_SWAP_MASK		(1 << _SPI_NFI_REGS_CNFG_DMA_WR_SWAP_SHIFT)
#define _SPI_NFI_REGS_CNFG_DMA_RD_SWAP_MASK		(1 << _SPI_NFI_REGS_CNFG_DMA_RD_SWAP_SHIFT)

#define _SPI_NFI_REGS_CNFG_ECC_DATA_SOURCE_INV_SHIFT	(0x0005)
#define _SPI_NFI_REGS_CNFG_ECC_DATA_SOURCE_INV_MASK		(1 << _SPI_NFI_REGS_CNFG_ECC_DATA_SOURCE_INV_SHIFT)

/* NFI_PAGEFMT */
#define _SPI_NFI_REGS_PAGEFMT_PAGE_512		(0x0000)
#define _SPI_NFI_REGS_PAGEFMT_PAGE_2K		(0x0001)
#define _SPI_NFI_REGS_PAGEFMT_PAGE_4K		(0x0002)
#define _SPI_NFI_REGS_PAGEFMT_PAGE_MASK		(0x0003)
#define _SPI_NFI_REGS_PAGEFMT_PAGE_SHIFT	(0x0000)

#define _SPI_NFI_REGS_PAGEFMT_SPARE_16			(0x0000)
#define _SPI_NFI_REGS_PAGEFMT_SPARE_26			(0x0001)
#define _SPI_NFI_REGS_PAGEFMT_SPARE_27			(0x0002)
#define _SPI_NFI_REGS_PAGEFMT_SPARE_28			(0x0003)
#define _SPI_NFI_REGS_PAGEFMT_SPARE_MASK		(0x0030)
#define _SPI_NFI_REGS_PAGEFMT_SPARE_SHIFT		(4)

#define _SPI_NFI_REGS_PAGEFMT_FDM_MASK			(0x0F00)
#define _SPI_NFI_REGS_PAGEFMT_FDM_SHIFT			(8)
#define _SPI_NFI_REGS_PAGEFMT_FDM_ECC_MASK  	(0xF000)
#define _SPI_NFI_REGS_PAGEFMT_FDM_ECC_SHIFT 	(12)

#define _SPI_NFI_REGS_PPAGEFMT_SPARE_16     	(0x0000)
#define _SPI_NFI_REGS_PPAGEFMT_SPARE_26     	(0x0001)
#define _SPI_NFI_REGS_PPAGEFMT_SPARE_27     	(0x0002)
#define _SPI_NFI_REGS_PPAGEFMT_SPARE_28     	(0x0003)
#define _SPI_NFI_REGS_PPAGEFMT_SPARE_MASK   	(0x0030)
#define _SPI_NFI_REGS_PPAGEFMT_SPARE_SHIFT  	(4)

/* NFI_CON */
#define _SPI_NFI_REGS_CON_SEC_MASK				(0xF000)
#define _SPI_NFI_REGS_CON_WR_TRIG				(0x0200)
#define _SPI_NFI_REGS_CON_RD_TRIG				(0x0100)
#define _SPI_NFI_REGS_CON_SEC_SHIFT				(12)
#define _SPI_NFI_REGS_CON_RESET_VALUE			(0x3)

/* NFI_INTR_EN */
#define _SPI_NFI_REGS_INTR_EN_AHB_DONE_EN		(0x0040)

/* NFI_REGS_INTR */
#define _SPI_NFI_REGS_INTR_AHB_DONE_CHECK		(0x0040)

/* NFI_SECCUS_SIZE */
#define _SPI_NFI_REGS_SECCUS_SIZE_EN			(0x00010000)
#define _SPI_NFI_REGS_SECCUS_SIZE_MASK			(0x00001FFF)
#define _SPI_NFI_REGS_SECCUS_SIZE_SHIFT			(0)

/* NFI_SNF_MISC_CTL */
#define _SPI_NFI_REGS_SNF_MISC_CTL_DATA_RW_MODE_SHIFT	(16)

/* NFI_SNF_MISC_CTL2 */
#define _SPI_NFI_REGS_SNF_MISC_CTL2_WR_MASK		(0x1FFF0000)
#define _SPI_NFI_REGS_SNF_MISC_CTL2_WR_SHIFT	(16)
#define _SPI_NFI_REGS_SNF_MISC_CTL2_RD_MASK		(0x00001FFF)
#define _SPI_NFI_REGS_SNF_MISC_CTL2_RD_SHIFT	(0)

/* NFI_REGS_CMD */
#define _SPI_NFI_REGS_CMD_READ_VALUE			(0x00)
#define _SPI_NFI_REGS_CMD_WRITE_VALUE			(0x80)

/* NFI_REGS_PG_CTL1 */
#define _SPI_NFI_REGS_PG_CTL1_SHIFT				(8)

/* SNF_STA_CTL1 */
#define _SPI_NFI_REGS_LOAD_TO_CACHE_DONE		(0x04000000)
#define _SPI_NFI_REGS_READ_FROM_CACHE_DONE		(0x02000000)

/* FUNCTION DECLARATIONS ------------------------------------------------------ */
void SPI_NFI_TRIGGER(SPI_NFI_CONF_DMA_TRIGGER_T rw);

/* MACRO DECLARATIONS ---------------------------------------------------------------- */

#define READ_REGISTER_UINT32(reg) \
	(*(volatile unsigned int  * const)(reg))

#define WRITE_REGISTER_UINT32(reg, val) \
	(*(volatile unsigned int  * const)(reg)) = (val)

#define INREG32(x)          READ_REGISTER_UINT32((unsigned int *)((void*)(x)))
#define OUTREG32(x, y)      WRITE_REGISTER_UINT32((unsigned int *)((void*)(x)), (unsigned int )(y))
#define SETREG32(x, y)      OUTREG32(x, INREG32(x)|(y))
#define CLRREG32(x, y)      OUTREG32(x, INREG32(x)&~(y))
#define MASKREG32(x, y, z)  OUTREG32(x, (INREG32(x)&~(y))|(z))

#define _SPI_NFI_REG8_READ(addr)						INREG32(addr)
#define _SPI_NFI_REG8_WRITE(addr, data)					OUTREG32(addr, data)
#define _SPI_NFI_REG8_SETBITS(addr, data)				SETREG32(addr, data)
#define _SPI_NFI_REG8_CLRBITS(addr, data)				CLRREG32(addr, data)
#define _SPI_NFI_REG8_SETMASKBITS(addr, mask, data)	MASKREG32(addr, mask, data)

#define _SPI_NFI_REG16_READ(addr)						INREG32(addr)
#define _SPI_NFI_REG16_WRITE(addr, data)				OUTREG32(addr, data)
#define _SPI_NFI_REG16_SETBITS(addr, data)				SETREG32(addr, data)
#define _SPI_NFI_REG16_CLRBITS(addr, data)				CLRREG32(addr, data)
#define _SPI_NFI_REG16_SETMASKBITS(addr, mask, data)	MASKREG32(addr, mask, data)

#define _SPI_NFI_REG32_READ(addr)						INREG32(addr)
#define _SPI_NFI_REG32_WRITE(addr, data)				OUTREG32(addr, data)
#define _SPI_NFI_REG32_SETBITS(addr, data)				SETREG32(addr, data)
#define _SPI_NFI_REG32_CLRBITS(addr, data)				CLRREG32(addr, data)
#define _SPI_NFI_REG32_SETMASKBITS(addr, mask, data)	MASKREG32(addr, mask, data)

#define _SPI_NFI_GET_CONF_PTR							&(_spi_nfi_conf_info_t)
#define _SPI_NFI_GET_FDM_PTR							&(_spi_nfi_fdm_value)
#define _SPI_NFI_SET_FDM_PTR							&(_spi_nfi_fdm_value)
#define _SPI_NFI_DATA_SIZE_WITH_ECC						(512)
#define _SPI_NFI_CHECK_DONE_MAX_TIMES					(1000000)

#define _SPI_NFI_PRINTF					printk
#if defined(SPI_NFI_DEBUG)
#define _SPI_NFI_DEBUG_PRINTF				spi_nfi_debug_printf
#else
#define _SPI_NFI_DEBUG_PRINTF(args...)
#endif

#define _SPI_NFI_MEMCPY					memcpy
#define _SPI_NFI_MEMSET					memset
#define _SPI_NFI_MAX_FDM_NUMBER			(64)
#define _SPI_NFI_MAX_FDM_PER_SEC		(8)

/* TYPE DECLARATIONS ----------------------------------------------------------------- */

/* STATIC VARIABLE DECLARATIONS ------------------------------------------------------ */
static SPI_NFI_CONF_T	_spi_nfi_conf_info_t;
static u8		_spi_nfi_fdm_value[_SPI_NFI_MAX_FDM_NUMBER];

/* LOCAL SUBPROGRAM BODIES------------------------------------------------------------ */

#if defined(SPI_NFI_DEBUG)
static u8 _SPI_NFI_DEBUG_FLAG = 0;	/* For control printf debug message or not */

static void spi_nfi_debug_printf( char *fmt, ... )
{
	if( _SPI_NFI_DEBUG_FLAG == 1 )
	{
		unsigned char 		str_buf[100];
		va_list 			argptr;
		int 				cnt;

		va_start(argptr, fmt);
		cnt = vsprintf(str_buf, fmt, argptr);
		va_end(argptr);

		_SPI_NFI_PRINTF("%s", str_buf);
	}
}
#endif

SPI_NFI_RTN_T spi_nfi_get_fdm_from_register(void)
{
	u32 idx, i, j, reg_addr, val;
	u8 *fdm_value = (u8 *)_SPI_NFI_GET_FDM_PTR;
	SPI_NFI_CONF_T *spi_nfi_conf_info_t = _SPI_NFI_GET_CONF_PTR;
	u8 spi_nfi_mapping_fdm_value[_SPI_NFI_MAX_FDM_NUMBER];

	_SPI_NFI_MEMSET(spi_nfi_mapping_fdm_value, 0xff, _SPI_NFI_MAX_FDM_NUMBER);
	_SPI_NFI_MEMSET(fdm_value, 0xff, _SPI_NFI_MAX_FDM_NUMBER);

	idx = 0;
	for( reg_addr = _SPI_NFI_REGS_FDM0L ; reg_addr <= _SPI_NFI_REGS_FDM7M ; reg_addr+=4 )
	{
		val = _SPI_NFI_REG32_READ(reg_addr);
		spi_nfi_mapping_fdm_value[idx++] = ( val & 0xFF) ;
		spi_nfi_mapping_fdm_value[idx++] = ((val >> 8) & 0xFF) ;
		spi_nfi_mapping_fdm_value[idx++] = ((val >> 16) & 0xFF) ;
		spi_nfi_mapping_fdm_value[idx++] = ((val >> 24) & 0xFF) ;
	}

	j=0;
	for(idx=0 ; idx< (spi_nfi_conf_info_t->sec_num) ; idx++)
	{
		for(i =0; i< (spi_nfi_conf_info_t->fdm_num); i++)
		{
			fdm_value[j] = spi_nfi_mapping_fdm_value[(idx*_SPI_NFI_MAX_FDM_PER_SEC)+i];
			j++;
		}
	}

	return (SPI_NFI_RTN_NO_ERROR);
}

SPI_NFI_RTN_T spi_nfi_set_fdm_into_register(void)
{
	u32 idx, i, j, reg_addr, val;
	u8 *fdm_value = (u8 *)_SPI_NFI_GET_FDM_PTR;
	SPI_NFI_CONF_T *spi_nfi_conf_info_t = _SPI_NFI_GET_CONF_PTR;
	u8 spi_nfi_mapping_fdm_value[_SPI_NFI_MAX_FDM_NUMBER];

	_SPI_NFI_MEMSET(spi_nfi_mapping_fdm_value, 0xff, _SPI_NFI_MAX_FDM_NUMBER);

	j=0;
	for(idx=0 ; idx< (spi_nfi_conf_info_t->sec_num) ; idx++)
	{
		for(i =0; i< (spi_nfi_conf_info_t->fdm_num); i++)
		{
			spi_nfi_mapping_fdm_value[(idx*_SPI_NFI_MAX_FDM_PER_SEC)+i] = fdm_value[j];
			j++;
		}
	}

	idx = 0;
	for( reg_addr = _SPI_NFI_REGS_FDM0L ; reg_addr <= _SPI_NFI_REGS_FDM7M ; reg_addr+=4 )
	{
		val = 0;

		val |= (spi_nfi_mapping_fdm_value[idx++] & (0xFF));
		val |= ((spi_nfi_mapping_fdm_value[idx++] & (0xFF)) << 8);
		val |= ((spi_nfi_mapping_fdm_value[idx++] & (0xFF)) << 16);
		val |= ((spi_nfi_mapping_fdm_value[idx++] & (0xFF)) << 24);

		 _SPI_NFI_REG32_WRITE(reg_addr, val);

		_SPI_NFI_DEBUG_PRINTF("spi_nfi_set_fdm_into_register : reg(0x%x)=0x%x\n", reg_addr, _SPI_NFI_REG32_READ(reg_addr));
	}

	return (SPI_NFI_RTN_NO_ERROR);
}

/* EXPORTED SUBPROGRAM BODIES -------------------------------------------------------- */

SPI_NFI_RTN_T SPI_NFI_Regs_Dump( void )
{
	u32 idx;

	for(idx = _SPI_NFI_REGS_BASE ; idx <= _SPI_NFI_REGS_SNF_STA_CTL2 ; idx +=4)
	{
		_SPI_NFI_PRINTF("reg(0x%x) = 0x%x\n", idx, _SPI_NFI_REG32_READ(idx) );
	}

	return (SPI_NFI_RTN_NO_ERROR);
}

SPI_NFI_RTN_T SPI_NFI_Read_SPI_NAND_FDM(u8 *ptr_rtn_oob, u32 oob_len)
{
	u8 *fdm_value = (u8 *)_SPI_NFI_GET_FDM_PTR;

	spi_nfi_get_fdm_from_register();

	_SPI_NFI_MEMCPY(ptr_rtn_oob, fdm_value, oob_len);

	return (SPI_NFI_RTN_NO_ERROR);
}

SPI_NFI_RTN_T SPI_NFI_Write_SPI_NAND_FDM(u8 *ptr_oob, u32 oob_len)
{
	u8 *fdm_value = (u8 *)_SPI_NFI_GET_FDM_PTR;

	if( oob_len > _SPI_NFI_MAX_FDM_NUMBER )
	{
		_SPI_NFI_MEMCPY(fdm_value, ptr_oob, _SPI_NFI_MAX_FDM_NUMBER);
	}
	else
	{
		_SPI_NFI_MEMCPY(fdm_value, ptr_oob, oob_len);
	}

	spi_nfi_set_fdm_into_register();

	return (SPI_NFI_RTN_NO_ERROR);
}

SPI_NFI_RTN_T SPI_NFI_Read_SPI_NAND_Page(SPI_NFI_MISC_SPEDD_CONTROL_T speed_mode, u32 read_cmd, u16 read_addr, u32 *prt_rtn_data)
{
	u32 check_cnt;
	SPI_NFI_CONF_T *spi_nfi_conf_info_t = _SPI_NFI_GET_CONF_PTR;
	SPI_NFI_RTN_T rtn_status = SPI_NFI_RTN_NO_ERROR;

	/* Set DMA destination address */
	_SPI_NFI_REG32_WRITE( _SPI_NFI_REGS_STRADDR, prt_rtn_data);

	/* Set Read length */
	if( (spi_nfi_conf_info_t->cus_sec_size_en_t) == SPI_NFI_CONF_CUS_SEC_SIZE_Disable )
	{
		_SPI_NFI_REG32_SETMASKBITS(_SPI_NFI_REGS_SNF_MISC_CTL2, _SPI_NFI_REGS_SNF_MISC_CTL2_RD_MASK,	\
				((_SPI_NFI_DATA_SIZE_WITH_ECC + (spi_nfi_conf_info_t->spare_size_t)) * (spi_nfi_conf_info_t->sec_num)) << _SPI_NFI_REGS_SNF_MISC_CTL2_RD_SHIFT );
	}
	if( (spi_nfi_conf_info_t->cus_sec_size_en_t) == SPI_NFI_CONF_CUS_SEC_SIZE_Enable )
	{
		_SPI_NFI_REG32_SETMASKBITS(_SPI_NFI_REGS_SNF_MISC_CTL2, _SPI_NFI_REGS_SNF_MISC_CTL2_RD_MASK,	\
				((spi_nfi_conf_info_t->sec_size) * (spi_nfi_conf_info_t->sec_num)) << _SPI_NFI_REGS_SNF_MISC_CTL2_RD_SHIFT );
	}

	/* Set Read Command */
	_SPI_NFI_REG32_WRITE(_SPI_NFI_REGS_RD_CTL2, (read_cmd & 0xFF));

	/* Set Read mode */
	_SPI_NFI_REG32_WRITE(_SPI_NFI_REGS_SNF_MISC_CTL, (speed_mode << _SPI_NFI_REGS_SNF_MISC_CTL_DATA_RW_MODE_SHIFT));

	/* Set Read Address (Note : Controller will use following register, depend on the Hardware TRAP of SPI NAND/SPI NOR  )*/
	_SPI_NFI_REG32_WRITE( _SPI_NFI_REGS_RD_CTL3, read_addr);		/* Set Address into SPI NAND address register*/

	/* Set NFI Read */
	_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CONF_OP_MASK, \
						(_SPI_NFI_REGS_CONF_OP_READ << _SPI_NFI_REGS_CONF_OP_SHIFT ));
	_SPI_NFI_REG16_SETBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CNFG_READ_EN);
	_SPI_NFI_REG16_SETBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CNFG_AHB);

	_SPI_NFI_REG16_WRITE( _SPI_NFI_REGS_CMD, _SPI_NFI_REGS_CMD_READ_VALUE);

	/* Trigger DMA read active*/
	SPI_NFI_TRIGGER(SPI_NFI_CON_DMA_TRIGGER_READ);

	/* Check read from cache  done or not */
	for( check_cnt = 0 ; check_cnt < _SPI_NFI_CHECK_DONE_MAX_TIMES ; check_cnt ++)
	{
		if( (_SPI_NFI_REG16_READ(_SPI_NFI_REGS_SNF_STA_CTL1)& (_SPI_NFI_REGS_READ_FROM_CACHE_DONE)) != 0 )
		{
			/* Clear this bit is neccessary for NFI state machine */
			_SPI_NFI_REG32_SETBITS(_SPI_NFI_REGS_SNF_STA_CTL1, _SPI_NFI_REGS_READ_FROM_CACHE_DONE);
			break;
		}
	}
	if(check_cnt == _SPI_NFI_CHECK_DONE_MAX_TIMES)
	{
		_SPI_NFI_PRINTF("[Error] Read DMA : Check READ FROM CACHE Done Timeout ! \n");
		rtn_status = SPI_NFI_RTN_READ_FROM_CACHE_DONE_TIMEOUT;
	}

	/* Check DMA done or not */
	for( check_cnt = 0 ; check_cnt < _SPI_NFI_CHECK_DONE_MAX_TIMES ; check_cnt ++)
	{
		if( (_SPI_NFI_REG16_READ(_SPI_NFI_REGS_INTR)& (_SPI_NFI_REGS_INTR_AHB_DONE_CHECK)) != 0 )
		{
			break;
		}
	}
	if(check_cnt == _SPI_NFI_CHECK_DONE_MAX_TIMES)
	{
		_SPI_NFI_PRINTF("[Error] Read DMA : Check AHB Done Timeout ! \n");
		rtn_status = SPI_NFI_RTN_CHECK_AHB_DONE_TIMEOUT;
	}

	/* Does DMA read need delay for data ready from controller to DRAM */
	delay1us(1);

	return (rtn_status);
}

SPI_NFI_RTN_T SPI_NFI_Write_SPI_NAND_page(SPI_NFI_MISC_SPEDD_CONTROL_T speed_mode, u32 write_cmd, u16 write_addr, u32 *prt_data)
{
	volatile u32 check_cnt;
	SPI_NFI_CONF_T *spi_nfi_conf_info_t = _SPI_NFI_GET_CONF_PTR;
	SPI_NFI_RTN_T rtn_status = SPI_NFI_RTN_NO_ERROR;

	_SPI_NFI_DEBUG_PRINTF("SPI_NFI_Write_SPI_NAND_page : enter, speed_mode=%d, write_cmd=0x%x, write_addr=0x%x, prt_data=0x%x\n",
		speed_mode, write_cmd, write_addr, prt_data);

	/* Set DMA destination address */
	_SPI_NFI_REG32_WRITE( _SPI_NFI_REGS_STRADDR, prt_data);

	_SPI_NFI_DEBUG_PRINTF("SPI_NFI_Write_SPI_NAND_page: _SPI_NFI_REGS_STRADDR=0x%x\n", _SPI_NFI_REG32_READ(_SPI_NFI_REGS_STRADDR));
	_SPI_NFI_DEBUG_PRINTF("SPI_NFI_Write_SPI_NAND_page\n");

	/* Set Write length */
	if( (spi_nfi_conf_info_t->cus_sec_size_en_t) == SPI_NFI_CONF_CUS_SEC_SIZE_Disable )
	{
		_SPI_NFI_REG32_SETMASKBITS(_SPI_NFI_REGS_SNF_MISC_CTL2, _SPI_NFI_REGS_SNF_MISC_CTL2_WR_MASK,	\
				((_SPI_NFI_DATA_SIZE_WITH_ECC + (spi_nfi_conf_info_t->spare_size_t)) * (spi_nfi_conf_info_t->sec_num)) << _SPI_NFI_REGS_SNF_MISC_CTL2_WR_SHIFT );
	}
	if( (spi_nfi_conf_info_t->cus_sec_size_en_t) == SPI_NFI_CONF_CUS_SEC_SIZE_Enable )
	{
		_SPI_NFI_REG32_SETMASKBITS(_SPI_NFI_REGS_SNF_MISC_CTL2, _SPI_NFI_REGS_SNF_MISC_CTL2_WR_MASK,	\
				((spi_nfi_conf_info_t->sec_size) * (spi_nfi_conf_info_t->sec_num)) << _SPI_NFI_REGS_SNF_MISC_CTL2_WR_SHIFT );
	}

	/* Set Write Command */
	_SPI_NFI_REG32_WRITE( _SPI_NFI_REGS_PG_CTL1, ((write_cmd & 0xFF) << _SPI_NFI_REGS_PG_CTL1_SHIFT));

	/* Set Write mode */
	_SPI_NFI_REG32_WRITE(_SPI_NFI_REGS_SNF_MISC_CTL, (speed_mode << _SPI_NFI_REGS_SNF_MISC_CTL_DATA_RW_MODE_SHIFT));

	/* Set Write Address (Note : Controller will use following register, depend on the Hardware TRAP of SPI NAND/SPI NOR  )*/
	_SPI_NFI_REG32_WRITE( _SPI_NFI_REGS_PG_CTL2, write_addr);		/* Set Address into SPI NAND address register*/

	/* Set NFI Write */
	_SPI_NFI_REG16_CLRBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CNFG_READ_EN);
	_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CONF_OP_MASK, \
								(_SPI_NFI_REGS_CONF_OP_PRGM << _SPI_NFI_REGS_CONF_OP_SHIFT ));

	_SPI_NFI_REG16_SETBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CNFG_AHB);
	_SPI_NFI_REG16_WRITE( _SPI_NFI_REGS_CMD, _SPI_NFI_REGS_CMD_WRITE_VALUE);

	/* Trigger DMA write active*/
	SPI_NFI_TRIGGER(SPI_NFI_CON_DMA_TRIGGER_WRITE);

	/* Does DMA write need delay for data ready from controller to Flash */
	delay1us(1);

	/* Check DMA done or not */
	for( check_cnt = 0 ; check_cnt < _SPI_NFI_CHECK_DONE_MAX_TIMES ; check_cnt ++)
	{
		if( (_SPI_NFI_REG16_READ(_SPI_NFI_REGS_INTR)& (_SPI_NFI_REGS_INTR_AHB_DONE_CHECK)) != 0 )
		{
			break;
		}
	}
	if(check_cnt == _SPI_NFI_CHECK_DONE_MAX_TIMES)
	{
		_SPI_NFI_PRINTF("[Error] Write DMA : Check AHB Done Timeout ! \n");
		rtn_status = SPI_NFI_RTN_CHECK_AHB_DONE_TIMEOUT;
	}

	/* Check load to cache  done or not */
	for( check_cnt = 0 ; check_cnt < _SPI_NFI_CHECK_DONE_MAX_TIMES ; check_cnt ++)
	{
		if( (_SPI_NFI_REG16_READ(_SPI_NFI_REGS_SNF_STA_CTL1)& (_SPI_NFI_REGS_LOAD_TO_CACHE_DONE)) != 0 )
		{
			/* Clear this bit is neccessary for NFI state machine */
			_SPI_NFI_REG32_SETBITS(_SPI_NFI_REGS_SNF_STA_CTL1, _SPI_NFI_REGS_LOAD_TO_CACHE_DONE);
			break;
		}
	}
	if(check_cnt == _SPI_NFI_CHECK_DONE_MAX_TIMES)
	{
		_SPI_NFI_PRINTF("[Error] Write DMA : Check LOAD TO CACHE Done Timeout ! \n");
		rtn_status = SPI_NFI_RTN_LOAD_TO_CACHE_DONE_TIMEOUT;
	}

	_SPI_NFI_DEBUG_PRINTF("SPI_NFI_Write_SPI_NAND_page : exit \n");

	return (rtn_status);
}

#ifdef CONFIG_MTD_SPINOR_ECONET
SPI_NFI_RTN_T SPI_NFI_Read_SPI_NOR(u8 opcode, u16 read_addr, u32 *prt_rtn_data)
{
	u32 check_cnt;
	SPI_NFI_CONF_T *spi_nfi_conf_info_t = _SPI_NFI_GET_CONF_PTR;
	SPI_NFI_RTN_T rtn_status = SPI_NFI_RTN_NO_ERROR;

	/* Set Read length */
	if( (spi_nfi_conf_info_t->cus_sec_size_en_t) == SPI_NFI_CONF_CUS_SEC_SIZE_Disable )
	{
		_SPI_NFI_REG32_SETMASKBITS(_SPI_NFI_REGS_SNF_MISC_CTL2, _SPI_NFI_REGS_SNF_MISC_CTL2_RD_MASK,	\
				((_SPI_NFI_DATA_SIZE_WITH_ECC + (spi_nfi_conf_info_t->spare_size_t)) * (spi_nfi_conf_info_t->sec_num)) << _SPI_NFI_REGS_CON_SEC_SHIFT );
	}
	if( (spi_nfi_conf_info_t->cus_sec_size_en_t) == SPI_NFI_CONF_CUS_SEC_SIZE_Enable )
	{
		_SPI_NFI_REG32_SETMASKBITS(_SPI_NFI_REGS_SNF_MISC_CTL2, _SPI_NFI_REGS_SNF_MISC_CTL2_RD_MASK,	\
				((spi_nfi_conf_info_t->sec_size) * (spi_nfi_conf_info_t->sec_num)) << _SPI_NFI_REGS_CON_SEC_SHIFT );
	}

	/* Set Read Command */
	_SPI_NFI_REG32_WRITE( _SPI_NFI_REGS_RD_CTL2, (u32) opcode);

	/* Set Read Address (Note : Controller will use following register, depend on the Hardware TRAP of SPI NAND/SPI NOR  )*/
	_SPI_NFI_REG32_WRITE( _SPI_NFI_REGS_NOR_RD_ADDR, read_addr);	/* Set Address into SPI NOR address register*/

	/* Reset NFI statemachile and flush fifo*/
	_SPI_NFI_REG16_SETBITS( _SPI_NFI_REGS_CON, _SPI_NFI_REGS_CON_RESET_VALUE);

	/* Set NFI Read */
	_SPI_NFI_REG16_WRITE( _SPI_NFI_REGS_CMD, _SPI_NFI_REGS_CMD_READ_VALUE);

	/* Set DMA destination address */
	_SPI_NFI_REG32_WRITE( _SPI_NFI_REGS_STRADDR, prt_rtn_data);

	/* Trigger DMA read active*/
	_SPI_NFI_REG16_CLRBITS( _SPI_NFI_REGS_CON, _SPI_NFI_REGS_CON_RD_TRIG);
	 /* [Note : Is here need to have little time delay or not ? */
	_SPI_NFI_REG16_SETBITS( _SPI_NFI_REGS_CON, _SPI_NFI_REGS_CON_RD_TRIG);

	/* Check DMA done or not */
	for( check_cnt = 0 ; check_cnt < _SPI_NFI_CHECK_DONE_MAX_TIMES ; check_cnt ++)
	{
		if( (_SPI_NFI_REG16_READ(_SPI_NFI_REGS_INTR)& (_SPI_NFI_REGS_INTR_AHB_DONE_CHECK)) != 0 )
		{
			break;
		}
	}
	if(check_cnt == _SPI_NFI_CHECK_DONE_MAX_TIMES)
	{
		_SPI_NFI_PRINTF("[Error] Read DMA : Check AHB Done Timeout ! \n");
		rtn_status = SPI_NFI_RTN_CHECK_AHB_DONE_TIMEOUT;
	}

	return (rtn_status);
}

SPI_NFI_RTN_T SPI_NFI_Write_SPI_NOR(u8 opcode, u16 write_addr, u32 *prt_data)
{
	u32 check_cnt;
	SPI_NFI_CONF_T *spi_nfi_conf_info_t = _SPI_NFI_GET_CONF_PTR;
	SPI_NFI_RTN_T rtn_status = SPI_NFI_RTN_NO_ERROR;

	/* Set Write length */
	if( (spi_nfi_conf_info_t->cus_sec_size_en_t) == SPI_NFI_CONF_CUS_SEC_SIZE_Disable )
	{
		_SPI_NFI_REG32_SETMASKBITS(_SPI_NFI_REGS_SNF_MISC_CTL2, _SPI_NFI_REGS_SNF_MISC_CTL2_RD_MASK,	\
				((_SPI_NFI_DATA_SIZE_WITH_ECC + (spi_nfi_conf_info_t->spare_size_t)) * (spi_nfi_conf_info_t->sec_num)) << _SPI_NFI_REGS_CON_SEC_SHIFT );
	}
	if( (spi_nfi_conf_info_t->cus_sec_size_en_t) == SPI_NFI_CONF_CUS_SEC_SIZE_Enable )
	{
		_SPI_NFI_REG32_SETMASKBITS(_SPI_NFI_REGS_SNF_MISC_CTL2, _SPI_NFI_REGS_SNF_MISC_CTL2_RD_MASK,	\
				((spi_nfi_conf_info_t->sec_size) * (spi_nfi_conf_info_t->sec_num)) << _SPI_NFI_REGS_CON_SEC_SHIFT );
	}

	/* Set Write Command */
	_SPI_NFI_REG32_WRITE( _SPI_NFI_REGS_PG_CTL1, ((u32) opcode) << _SPI_NFI_REGS_PG_CTL1_SHIFT);

	/* Set Write Address (Note : Controller will use following register, depend on the Hardware TRAP of SPI NAND/SPI NOR  )*/
	_SPI_NFI_REG32_WRITE( _SPI_NFI_REGS_NOR_PROG_ADDR, write_addr);	/* Set Address into SPI NOR address register*/

	/* Reset NFI statemachile and flush fifo*/
	_SPI_NFI_REG16_SETBITS( _SPI_NFI_REGS_CON, _SPI_NFI_REGS_CON_RESET_VALUE);

	/* Set NFI Write */
	_SPI_NFI_REG16_WRITE( _SPI_NFI_REGS_CMD, _SPI_NFI_REGS_CMD_WRITE_VALUE);

	/* Set DMA destination address */
	_SPI_NFI_REG32_WRITE( _SPI_NFI_REGS_STRADDR, prt_data);

	/* Trigger DMA read active*/
	_SPI_NFI_REG16_CLRBITS( _SPI_NFI_REGS_CON, _SPI_NFI_REGS_CON_WR_TRIG);
	 /* [Note : Is here need to have little time delay or not ? */
	_SPI_NFI_REG16_SETBITS( _SPI_NFI_REGS_CON, _SPI_NFI_REGS_CON_WR_TRIG);

	/* Check DMA done or not */
	for( check_cnt = 0 ; check_cnt < _SPI_NFI_CHECK_DONE_MAX_TIMES ; check_cnt ++)
	{
		if( (_SPI_NFI_REG16_READ(_SPI_NFI_REGS_INTR)& (_SPI_NFI_REGS_INTR_AHB_DONE_CHECK)) != 0 )
		{
			break;
		}
	}
	if(check_cnt == _SPI_NFI_CHECK_DONE_MAX_TIMES)
	{
		_SPI_NFI_PRINTF("[Error] Write DMA : Check AHB Done Timeout ! \n");
		rtn_status = SPI_NFI_RTN_CHECK_AHB_DONE_TIMEOUT;
	}

	return (rtn_status);
}
#endif

SPI_NFI_RTN_T SPI_NFI_Get_Configure( SPI_NFI_CONF_T *ptr_rtn_nfi_conf_t )
{
	SPI_NFI_CONF_T *ptr_spi_nfi_conf_info_t = _SPI_NFI_GET_CONF_PTR;

	_SPI_NFI_MEMCPY(ptr_rtn_nfi_conf_t, ptr_spi_nfi_conf_info_t, sizeof(SPI_NFI_CONF_T));

	return (SPI_NFI_RTN_NO_ERROR);
}

SPI_NFI_RTN_T SPI_NFI_Set_Configure( SPI_NFI_CONF_T *ptr_nfi_conf_t )
{
	SPI_NFI_CONF_T *ptr_spi_nfi_conf_info_t = _SPI_NFI_GET_CONF_PTR;

	/* Store new setting */
	_SPI_NFI_MEMCPY(ptr_spi_nfi_conf_info_t, ptr_nfi_conf_t, sizeof(SPI_NFI_CONF_T));

	_SPI_NFI_DEBUG_PRINTF("SPI_NFI_Set_Configure: hw_ecc_t= 0x%x\n", ptr_nfi_conf_t->hw_ecc_t );

	/* Set Auto FDM */
	if( (ptr_nfi_conf_t->auto_fdm_t) == SPI_NFI_CON_AUTO_FDM_Disable )
	{
		_SPI_NFI_REG16_CLRBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CNFG_AUTO_FMT_EN);
	}
	if( (ptr_nfi_conf_t->auto_fdm_t) == SPI_NFI_CON_AUTO_FDM_Enable )
	{
		_SPI_NFI_REG16_SETBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CNFG_AUTO_FMT_EN);
	}

	/* Set Hardware ECC */
	if( (ptr_nfi_conf_t->hw_ecc_t) == SPI_NFI_CON_HW_ECC_Disable )
	{
		_SPI_NFI_REG16_CLRBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CNFG_HW_ECC_EN);
	}
	if( (ptr_nfi_conf_t->hw_ecc_t) == SPI_NFI_CON_HW_ECC_Enable )
	{
		_SPI_NFI_REG16_SETBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CNFG_HW_ECC_EN);
	}

	/* Set DMA BURST */
	if( (ptr_nfi_conf_t->dma_burst_t) == SPI_NFI_CON_DMA_BURST_Disable )
	{
		_SPI_NFI_REG16_CLRBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CNFG_DMA_BURST_EN);
	}
	if( (ptr_nfi_conf_t->dma_burst_t) == SPI_NFI_CON_DMA_BURST_Enable )
	{
		_SPI_NFI_REG16_SETBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CNFG_DMA_BURST_EN);
	}

	/* Set FDM Number */
	_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_PAGEFMT, _SPI_NFI_REGS_PAGEFMT_FDM_MASK,	\
						(ptr_nfi_conf_t->fdm_num)<< _SPI_NFI_REGS_PAGEFMT_FDM_SHIFT );

	/* Set FDM ECC Number */
	_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_PAGEFMT, _SPI_NFI_REGS_PAGEFMT_FDM_ECC_MASK,	\
						(ptr_nfi_conf_t->fdm_ecc_num)<< _SPI_NFI_REGS_PAGEFMT_FDM_ECC_SHIFT );

	/* Set SPARE Size */
	if( (ptr_nfi_conf_t->spare_size_t) == SPI_NFI_CONF_SPARE_SIZE_16BYTE )
	{
		_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_PAGEFMT, _SPI_NFI_REGS_PAGEFMT_SPARE_MASK,	\
						_SPI_NFI_REGS_PAGEFMT_SPARE_16 << _SPI_NFI_REGS_PAGEFMT_SPARE_SHIFT );
	}
	if( (ptr_nfi_conf_t->spare_size_t) == SPI_NFI_CONF_SPARE_SIZE_26BYTE )
	{
		_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_PAGEFMT, _SPI_NFI_REGS_PAGEFMT_SPARE_MASK,	\
						_SPI_NFI_REGS_PAGEFMT_SPARE_26 << _SPI_NFI_REGS_PAGEFMT_SPARE_SHIFT );
	}
	if( (ptr_nfi_conf_t->spare_size_t) == SPI_NFI_CONF_SPARE_SIZE_27BYTE )
	{
		_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_PAGEFMT, _SPI_NFI_REGS_PAGEFMT_SPARE_MASK,	\
						_SPI_NFI_REGS_PAGEFMT_SPARE_27 << _SPI_NFI_REGS_PAGEFMT_SPARE_SHIFT );
	}
	if( (ptr_nfi_conf_t->spare_size_t) == SPI_NFI_CONF_SPARE_SIZE_28BYTE )
	{
		_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_PAGEFMT, _SPI_NFI_REGS_PAGEFMT_SPARE_MASK,	\
						_SPI_NFI_REGS_PAGEFMT_SPARE_28 << _SPI_NFI_REGS_PAGEFMT_SPARE_SHIFT );
	}

	/* Set PAGE Size */
	if( (ptr_nfi_conf_t->page_size_t) == SPI_NFI_CONF_PAGE_SIZE_512BYTE )
	{
		_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_PAGEFMT, _SPI_NFI_REGS_PAGEFMT_PAGE_MASK,	\
						_SPI_NFI_REGS_PAGEFMT_PAGE_512 << _SPI_NFI_REGS_PAGEFMT_PAGE_SHIFT );
	}
	if( (ptr_nfi_conf_t->page_size_t) == SPI_NFI_CONF_PAGE_SIZE_2KBYTE )
	{
		_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_PAGEFMT, _SPI_NFI_REGS_PAGEFMT_PAGE_MASK,	\
						_SPI_NFI_REGS_PAGEFMT_PAGE_2K << _SPI_NFI_REGS_PAGEFMT_PAGE_SHIFT );
	}
	if( (ptr_nfi_conf_t->page_size_t) == SPI_NFI_CONF_PAGE_SIZE_4KBYTE )
	{
		_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_PAGEFMT, _SPI_NFI_REGS_PAGEFMT_PAGE_MASK,	\
						_SPI_NFI_REGS_PAGEFMT_PAGE_4K << _SPI_NFI_REGS_PAGEFMT_PAGE_SHIFT );
	}

	/* Set sector number */
	_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_CON, _SPI_NFI_REGS_CON_SEC_MASK,	\
					(ptr_nfi_conf_t->sec_num)<< _SPI_NFI_REGS_CON_SEC_SHIFT );

	/* Enable Customer setting sector size or not */
	if( (ptr_nfi_conf_t->cus_sec_size_en_t) == SPI_NFI_CONF_CUS_SEC_SIZE_Disable )
	{
		_SPI_NFI_REG32_CLRBITS(_SPI_NFI_REGS_SECCUS_SIZE, _SPI_NFI_REGS_SECCUS_SIZE_EN);
	}
	if( (ptr_nfi_conf_t->cus_sec_size_en_t) == SPI_NFI_CONF_CUS_SEC_SIZE_Enable )
	{
		_SPI_NFI_REG32_SETBITS(_SPI_NFI_REGS_SECCUS_SIZE, _SPI_NFI_REGS_SECCUS_SIZE_EN);
	}

	/* Set Customer sector size */
	_SPI_NFI_REG32_SETMASKBITS(_SPI_NFI_REGS_SECCUS_SIZE, _SPI_NFI_REGS_SECCUS_SIZE_MASK,	\
					(ptr_nfi_conf_t->sec_size)<< _SPI_NFI_REGS_SECCUS_SIZE_SHIFT );

	return SPI_NFI_RTN_NO_ERROR;
}

void SPI_NFI_Reset( void )
{
	/* Reset NFI statemachile and flush fifo*/
	_SPI_NFI_REG16_WRITE( _SPI_NFI_REGS_CON, _SPI_NFI_REGS_CON_RESET_VALUE);
}

SPI_NFI_RTN_T SPI_NFI_Init( void )
{
	/* Enable AHB Done Interrupt Function */
	_SPI_NFI_REG16_SETBITS( _SPI_NFI_REGS_INTR_EN, _SPI_NFI_REGS_INTR_EN_AHB_DONE_EN);

	return SPI_NFI_RTN_NO_ERROR;
}

/* Trigger DMA read active*/
void SPI_NFI_TRIGGER(SPI_NFI_CONF_DMA_TRIGGER_T rw)
{
	if(rw == SPI_NFI_CON_DMA_TRIGGER_READ) {
		_SPI_NFI_REG16_CLRBITS( _SPI_NFI_REGS_CON, _SPI_NFI_REGS_CON_RD_TRIG);
		_SPI_NFI_REG16_SETBITS( _SPI_NFI_REGS_CON, _SPI_NFI_REGS_CON_RD_TRIG);
	} else {
		_SPI_NFI_REG16_CLRBITS( _SPI_NFI_REGS_CON, _SPI_NFI_REGS_CON_WR_TRIG);
		_SPI_NFI_REG16_SETBITS( _SPI_NFI_REGS_CON, _SPI_NFI_REGS_CON_WR_TRIG);
	}
}

/* Set DMA(flash -> SRAM) byte swap*/
void SPI_NFI_DMA_WR_BYTE_SWAP(SPI_NFI_CONF_DMA_WR_BYTE_SWAP_T enable)
{
	_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CNFG_DMA_WR_SWAP_MASK,
					enable << _SPI_NFI_REGS_CNFG_DMA_WR_SWAP_SHIFT);
}

/* Set ECC decode invert*/
void SPI_NFI_ECC_DATA_SOURCE_INV(SPI_NFI_CONF_ECC_DATA_SOURCE_INV_T enable)
{
	_SPI_NFI_REG16_SETMASKBITS(_SPI_NFI_REGS_CNFG, _SPI_NFI_REGS_CNFG_ECC_DATA_SOURCE_INV_MASK,
					enable << _SPI_NFI_REGS_CNFG_ECC_DATA_SOURCE_INV_SHIFT);
}

