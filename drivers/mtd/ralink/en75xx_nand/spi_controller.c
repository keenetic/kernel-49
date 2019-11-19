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
 * FILE NAME: spi_controller.c
 * DATE: 2014/12/16
 * VERSION: 1.00
 * PURPOSE: To Provide SPI Controller Access interface.
 * NOTES:
 *
 * AUTHOR : Chuck Kuo         REVIEWED by
 *
 * FUNCTIONS
 *
 *      SPI_CONTROLLER_Enable_Manual_Mode To provide interface for Enable SPI Controller Manual Mode.
 *      SPI_CONTROLLER_Write_One_Byte     To provide interface for write one byte to SPI bus.
 *      SPI_CONTROLLER_Write_NByte        To provide interface for write N bytes to SPI bus.
 *      SPI_CONTROLLER_Read_NByte         To provide interface for read N bytes from SPI bus.
 *      SPI_CONTROLLER_Chip_Select_Low    To provide interface for set chip select low in SPI bus.
 *      SPI_CONTROLLER_Chip_Select_High   To provide interface for set chip select high in SPI bus.
 *
 * DEPENDENCIES
 *
 * * $History: $
 * MODIFICTION HISTORY:
 * Version 1.00 - Date 2014/12/16 By Chuck Kuo
 * ** This is the first versoin for creating to support the functions of
 *    current module.
 *
 *======================================================================================
 */

/* INCLUDE FILE DECLARATIONS --------------------------------------------------------- */
//#include <asm/types.h>
#include <asm/io.h>
#include <asm/string.h>
#include <asm/tc3162/tc3162.h>

#include "spi_controller.h"

/* NAMING CONSTANT DECLARATIONS ------------------------------------------------------ */

/* SPI Controller Register Definition */
#define _SPI_CONTROLLER_REGS_BASE			0xBFA10000
#define _SPI_CONTROLLER_REGS_READ_MODE			(_SPI_CONTROLLER_REGS_BASE + 0x0000)
#define _SPI_CONTROLLER_REGS_READ_IDLE_EN		(_SPI_CONTROLLER_REGS_BASE + 0x0004)
#define _SPI_CONTROLLER_REGS_SIDLY			(_SPI_CONTROLLER_REGS_BASE + 0x0008)
#define _SPI_CONTROLLER_REGS_CSHEXT			(_SPI_CONTROLLER_REGS_BASE + 0x000C)
#define _SPI_CONTROLLER_REGS_CSLEXT			(_SPI_CONTROLLER_REGS_BASE + 0x0010)
#define _SPI_CONTROLLER_REGS_MTX_MODE_TOG		(_SPI_CONTROLLER_REGS_BASE + 0x0014)
#define _SPI_CONTROLLER_REGS_RDCTL_FSM			(_SPI_CONTROLLER_REGS_BASE + 0x0018)
#define _SPI_CONTROLLER_REGS_MACMUX_SEL			(_SPI_CONTROLLER_REGS_BASE + 0x001C)
#define _SPI_CONTROLLER_REGS_MANUAL_EN			(_SPI_CONTROLLER_REGS_BASE + 0x0020)
#define _SPI_CONTROLLER_REGS_MANUAL_OPFIFO_EMPTY	(_SPI_CONTROLLER_REGS_BASE + 0x0024)
#define _SPI_CONTROLLER_REGS_MANUAL_OPFIFO_WDATA	(_SPI_CONTROLLER_REGS_BASE + 0x0028)
#define _SPI_CONTROLLER_REGS_MANUAL_OPFIFO_FULL		(_SPI_CONTROLLER_REGS_BASE + 0x002C)
#define _SPI_CONTROLLER_REGS_MANUAL_OPFIFO_WR		(_SPI_CONTROLLER_REGS_BASE + 0x0030)
#define _SPI_CONTROLLER_REGS_MANUAL_DFIFO_FULL		(_SPI_CONTROLLER_REGS_BASE + 0x0034)
#define _SPI_CONTROLLER_REGS_MANUAL_DFIFO_WDATA		(_SPI_CONTROLLER_REGS_BASE + 0x0038)
#define _SPI_CONTROLLER_REGS_MANUAL_DFIFO_EMPTY		(_SPI_CONTROLLER_REGS_BASE + 0x003C)
#define _SPI_CONTROLLER_REGS_MANUAL_DFIFO_RD		(_SPI_CONTROLLER_REGS_BASE + 0x0040)
#define _SPI_CONTROLLER_REGS_MANUAL_DFIFO_RDATA		(_SPI_CONTROLLER_REGS_BASE + 0x0044)
#define _SPI_CONTROLLER_REGS_DUMMY			(_SPI_CONTROLLER_REGS_BASE + 0x0080)
#define _SPI_CONTROLLER_REGS_PROBE_SEL			(_SPI_CONTROLLER_REGS_BASE + 0x0088)
#define _SPI_CONTROLLER_REGS_INTERRUPT			(_SPI_CONTROLLER_REGS_BASE + 0x0090)
#define _SPI_CONTROLLER_REGS_INTERRUPT_EN		(_SPI_CONTROLLER_REGS_BASE + 0x0094)
#define _SPI_CONTROLLER_REGS_SI_CK_SEL			(_SPI_CONTROLLER_REGS_BASE + 0x009C)
#define _SPI_CONTROLLER_REGS_SW_CFGNANDADDR_VAL		(_SPI_CONTROLLER_REGS_BASE + 0x010C)
#define _SPI_CONTROLLER_REGS_SW_CFGNANDADDR_EN		(_SPI_CONTROLLER_REGS_BASE + 0x0110)
#define _SPI_CONTROLLER_REGS_NFI2SPI_EN			(_SPI_CONTROLLER_REGS_BASE + 0x0130)

/* Register Value Definition */
#define _SPI_CONTROLLER_VAL_OP_LEN_MAX			(0x1ff)
#define _SPI_CONTROLLER_VAL_OP_LEN_ONE			(1)
#define _SPI_CONTROLLER_VAL_OP_LEN_TWO			(2)
#define _SPI_CONTROLLER_VAL_OP_LEN_THREE		(3)
#define _SPI_CONTROLLER_VAL_OP_LEN_FOUR			(4)
#define _SPI_CONTROLLER_VAL_OP_LEN_FIVE			(5)
#define _SPI_CONTROLLER_VAL_OP_CMD_MASK			(0x1f)
#define _SPI_CONTROLLER_VAL_OP_LEN_MASK			(0x1ff)
#define _SPI_CONTROLLER_VAL_OP_SHIFT			(0x9)
#define _SPI_CONTROLLER_VAL_OP_ENABLE			(0x1)
#define _SPI_CONTROLLER_VAL_DFIFO_MASK			(0xff)
#define _SPI_CONTROLLER_VAL_READ_IDLE_DISABLE		(0x0)
#define _SPI_CONTROLLER_VAL_MANUAL_MTXMODE		(0x9)
#define _SPI_CONTROLLER_VAL_MANUAL_MANUALEN		(0x1)
#define _SPI_CONTROLLER_VAL_DDATA_ENABLE		(0x1)
#define _SPI_CONTROLLER_VAL_AUTO_MTXMODE		(0x0)
#define _SPI_CONTROLLER_VAL_MANUAL_MANUALDISABLE	(0x0)
#define _SPI_CONTROLLER_VAL_NFI2SPI_ENABLE		(1)
#define _SPI_CONTROLLER_VAL_NFI2SPI_DISABLE		(0)

#define _SPI_CONTROLLER_CHECK_TIMES			(10000)

/* MACRO DECLARATIONS ---------------------------------------------------------------- */
#ifndef VPint
#define VPint						*(volatile unsigned long int *)
#endif

#define WriteReg(reg, data)				(VPint(reg) = data)
#define ReadReg(reg)					(VPint(reg))
#define bReadReg(reg, mask)				(VPint(reg) & mask)

#define _SPI_CONTROLLER_PRINTF				printk
#if defined(SPI_CONTROLLER_DEBUG)
#define _SPI_CONTROLLER_DEBUG_PRINTF			spi_controller_debug_printf
#define _SPI_CONTROLLER_DEBUG_PRINTF_ARRAY		spi_controller_debug_printf_array
#else
#define _SPI_CONTROLLER_DEBUG_PRINTF(args...)
#define _SPI_CONTROLLER_DEBUG_PRINTF_ARRAY(args...)
#endif
#define _SPI_CONTROLLER_GET_CONF_PTR			&(_spi_controller_conf_t)
#define _SPI_CONTROLLER_MEMCPY				memcpy

/* STATIC VARIABLE DECLARATIONS ------------------------------------------------------ */
static SPI_CONTROLLER_CONF_T _spi_controller_conf_t;

/* LOCAL SUBPROGRAM BODIES------------------------------------------------------------ */
#if defined(SPI_CONTROLLER_DEBUG)
/* For control printf debug message or not */
static u8 _SPI_CONTROLLER_DEBUG_FLAG = 0;

static void spi_controller_debug_printf( char *fmt, ... )
{
	if( _SPI_CONTROLLER_DEBUG_FLAG == 1 )
	{
		unsigned char  str_buf[100];
		va_list argptr;
		int cnt;

		va_start(argptr, fmt);
		cnt = vsprintf(str_buf, fmt, argptr);
		va_end(argptr);

		printk("%s", str_buf);
	}
}

static void spi_controller_debug_printf_array (u8 *ptr_data, u32 len)
{
	u32 idx;

	for(idx=0 ; idx<len; idx++)
	{
		_SPI_CONTROLLER_DEBUG_PRINTF("0x%x ", *(ptr_data+idx));
	}
}
#endif

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_CONTROLLER_RTN_T spi_controller_set_opfifo( u8  op_cmd,
 *                                                           u32  op_len )
 * PURPOSE : To setup SPI Controller opfifo.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : op_cmd - The op_cmd variable of this function.
 *           op_len - The op_len variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/16 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_CONTROLLER_RTN_T spi_controller_set_opfifo(u8 op_cmd, u32 op_len)
{
	SPI_CONTROLLER_RTN_T rtn_status = SPI_CONTROLLER_RTN_NO_ERROR;

	_SPI_CONTROLLER_DEBUG_PRINTF("spi_controller_set_opfifo: set op_cmd =0x%x, op_len=0x%x\n", op_cmd, op_len);

	/* 1. Write op_cmd to register OPFIFO_WDATA */
	WriteReg( _SPI_CONTROLLER_REGS_MANUAL_OPFIFO_WDATA, ((((op_cmd) & _SPI_CONTROLLER_VAL_OP_CMD_MASK) << _SPI_CONTROLLER_VAL_OP_SHIFT) | ((op_len) & _SPI_CONTROLLER_VAL_OP_LEN_MASK)));

	/* 2. Wait until opfifo is not full */
	while(ReadReg( _SPI_CONTROLLER_REGS_MANUAL_OPFIFO_FULL ));

	/* 3. Enable write from register OPFIFO_WDATA to opfifo */
	WriteReg( _SPI_CONTROLLER_REGS_MANUAL_OPFIFO_WR, _SPI_CONTROLLER_VAL_OP_ENABLE);

	/* 4. Wait until opfifo is empty */
	while(!ReadReg( _SPI_CONTROLLER_REGS_MANUAL_OPFIFO_EMPTY ));

	return(rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_CONTROLLER_RTN_T spi_controller_read_data_fifo( u8      *ptr_rtn_data,
 *                                                               u32     data_len  )
 * PURPOSE : To read data from SPI Controller data pfifo.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : data_len  - The data_len variable of this function.
 *   OUTPUT: ptr_rtn_data  - The ptr_rtn_data variable of this function.
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/16 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_CONTROLLER_RTN_T spi_controller_read_data_fifo( u8 *ptr_rtn_data, u32 data_len)
{
	u32 idx;
	SPI_CONTROLLER_RTN_T rtn_status = SPI_CONTROLLER_RTN_NO_ERROR;

	for( idx =0 ; idx<data_len ; idx ++)
	{
		/* 1. wait until dfifo is not empty */
		while(ReadReg( _SPI_CONTROLLER_REGS_MANUAL_DFIFO_EMPTY ));

		/* 2. read from dfifo to register DFIFO_RDATA */

		*(ptr_rtn_data+idx) = (ReadReg( _SPI_CONTROLLER_REGS_MANUAL_DFIFO_RDATA )) &_SPI_CONTROLLER_VAL_DFIFO_MASK;

		_SPI_CONTROLLER_DEBUG_PRINTF(" spi_controller_read_data_fifo : read_data = 0x%x\n", *(ptr_rtn_data+idx));

		/* 3. enable register DFIFO_RD to read next byte */
		WriteReg( _SPI_CONTROLLER_REGS_MANUAL_DFIFO_RD, _SPI_CONTROLLER_VAL_DDATA_ENABLE);
	}

	return(rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: static SPI_CONTROLLER_RTN_T spi_controller_write_data_fifo( u8     *ptr_data,
 *                                                                u32    data_len )
 * PURPOSE : To write data from SPI Controller data pfifo.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : ptr_data     - The data variable of this function.
 *           data_len - The data_len variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/16 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
static SPI_CONTROLLER_RTN_T spi_controller_write_data_fifo(u8 *ptr_data, u32 data_len)
{
	u32 idx;
	SPI_CONTROLLER_RTN_T rtn_status = SPI_CONTROLLER_RTN_NO_ERROR;

	_SPI_CONTROLLER_DEBUG_PRINTF("spi_controller_write_data_fifo : len=0x%x, data: 0x%x\n", data_len, *ptr_data);
	_SPI_CONTROLLER_DEBUG_PRINTF_ARRAY(ptr_data, data_len);

	for( idx =0 ; idx < data_len ; idx++)
	{
		/* 1. Wait until dfifo is not full */
		while(ReadReg( _SPI_CONTROLLER_REGS_MANUAL_DFIFO_FULL ));

		/* 2. Write data  to register DFIFO_WDATA */
		WriteReg( _SPI_CONTROLLER_REGS_MANUAL_DFIFO_WDATA, ((*(ptr_data+idx)) & _SPI_CONTROLLER_VAL_DFIFO_MASK));

		_SPI_CONTROLLER_DEBUG_PRINTF(" spi_controller_write_data_fifo: write data =0x%x\n", ((*(ptr_data+idx)) & _SPI_CONTROLLER_VAL_DFIFO_MASK));
	}

	/* Wait until dfifo is not full */
	while(ReadReg( _SPI_CONTROLLER_REGS_MANUAL_DFIFO_FULL ));

	return(rtn_status);
}

/* EXPORTED SUBPROGRAM BODIES -------------------------------------------------------- */
SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Set_Configure( SPI_CONTROLLER_CONF_T *ptr_spi_controller_conf_t )
{
	SPI_CONTROLLER_CONF_T *ptr_spi_conf_t;

	ptr_spi_conf_t = _SPI_CONTROLLER_GET_CONF_PTR;

	/* Store new setting */
	_SPI_CONTROLLER_MEMCPY(ptr_spi_conf_t, ptr_spi_controller_conf_t, sizeof(SPI_CONTROLLER_CONF_T) );

	/* Setting Mode */
	if( (ptr_spi_conf_t->mode) == SPI_CONTROLLER_MODE_AUTO )
	{
		_SPI_CONTROLLER_DEBUG_PRINTF("SPI_CONTROLLER_Set_Configure: AUTO Mode\n");

		/* Switch out DMA circuit  */
		if (isEN7526c || isEN751627) {
			WriteReg( _SPI_CONTROLLER_REGS_NFI2SPI_EN, _SPI_CONTROLLER_VAL_NFI2SPI_DISABLE);
		}

		/* manaul mode -> auto mode */
		/*Set 0  to SF_MTX_MODE_TOG */
		WriteReg( _SPI_CONTROLLER_REGS_MTX_MODE_TOG, _SPI_CONTROLLER_VAL_AUTO_MTXMODE);

		/*Enable Auto Mode */
		WriteReg( _SPI_CONTROLLER_REGS_MANUAL_EN, _SPI_CONTROLLER_VAL_MANUAL_MANUALDISABLE);
	}

	if( (ptr_spi_conf_t->mode) == SPI_CONTROLLER_MODE_MANUAL)
	{
		_SPI_CONTROLLER_DEBUG_PRINTF("SPI_CONTROLLER_Set_Configure: Manual Mode\n");

		/* Switch out DMA circuit  */
		if (isEN7526c || isEN751627) {
			WriteReg( _SPI_CONTROLLER_REGS_NFI2SPI_EN, _SPI_CONTROLLER_VAL_NFI2SPI_DISABLE);
		}

		/* disable read_idle_enable */
		WriteReg( _SPI_CONTROLLER_REGS_READ_IDLE_EN , _SPI_CONTROLLER_VAL_READ_IDLE_DISABLE);

		/*wait until auto read status is IDLE */
		while(ReadReg( _SPI_CONTROLLER_REGS_RDCTL_FSM ));

		/*auto mode -> manaul mode */
		/*Set 9  to SF_MTX_MODE_TOG */
		WriteReg( _SPI_CONTROLLER_REGS_MTX_MODE_TOG, _SPI_CONTROLLER_VAL_MANUAL_MTXMODE);

		/*Enable Manual Mode */
		WriteReg( _SPI_CONTROLLER_REGS_MANUAL_EN, _SPI_CONTROLLER_VAL_MANUAL_MANUALEN);
	}

#if defined(TCSUPPORT_SPI_CONTROLLER_ECC)
	if( (ptr_spi_conf_t->mode) == SPI_CONTROLLER_MODE_DMA)
	{
		_SPI_CONTROLLER_DEBUG_PRINTF("SPI_CONTROLLER_Set_Configure: DMA Mode\n");

		/* Switch into DMA circuit  */
		if (isEN751627 || isEN7526c) {
			WriteReg( _SPI_CONTROLLER_REGS_NFI2SPI_EN, _SPI_CONTROLLER_VAL_NFI2SPI_ENABLE);
		}

		/* manaul mode -> auto mode */
		/*Set 0  to SF_MTX_MODE_TOG */
		WriteReg( _SPI_CONTROLLER_REGS_MTX_MODE_TOG, _SPI_CONTROLLER_VAL_AUTO_MTXMODE);

		/*Enable Auto Mode */
		WriteReg( _SPI_CONTROLLER_REGS_MANUAL_EN, _SPI_CONTROLLER_VAL_MANUAL_MANUALDISABLE);
	}
#endif

	/* Set dummy byte number */
	WriteReg(_SPI_CONTROLLER_REGS_DUMMY, (ptr_spi_conf_t->dummy_byte_num) );

	return (SPI_CONTROLLER_RTN_NO_ERROR);
}

SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Get_Configure( SPI_CONTROLLER_CONF_T *ptr_rtn_spi_controller_conf_t )
{
	SPI_CONTROLLER_CONF_T *ptr_spi_controller_conf_info_t;

	ptr_spi_controller_conf_info_t = _SPI_CONTROLLER_GET_CONF_PTR;
	_SPI_CONTROLLER_MEMCPY( ptr_rtn_spi_controller_conf_t, ptr_spi_controller_conf_info_t, sizeof(SPI_CONTROLLER_CONF_T) );

	return (SPI_CONTROLLER_RTN_NO_ERROR);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Enable_Manual_Mode( void )
 * PURPOSE : To provide interface for enable SPI Controller Manual Mode Enable.
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
 * Date 2014/12/16 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Enable_Manual_Mode( void )
{
	SPI_CONTROLLER_RTN_T rtn_status = SPI_CONTROLLER_RTN_NO_ERROR; 
	SPI_CONTROLLER_CONF_T spi_conf_t;

	/* Switch to manual mode*/
	spi_conf_t.dummy_byte_num = 1 ;
	spi_conf_t.mode = SPI_CONTROLLER_MODE_MANUAL;
	SPI_CONTROLLER_Set_Configure(&spi_conf_t);

	return (rtn_status);
}


/*------------------------------------------------------------------------------------
 * FUNCTION: SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Write_One_Byte_With_Cmd( u8  data )
 * PURPOSE : To provide interface for write one byte to SPI bus.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : data - The data variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/16 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Write_One_Byte_With_Cmd( u8 op_cmd, u8 data )
{
	SPI_CONTROLLER_RTN_T rtn_status = SPI_CONTROLLER_RTN_NO_ERROR;

	_SPI_CONTROLLER_DEBUG_PRINTF("SPI_CONTROLLER_Write_One_Byte : data=0x%x\n", data);

	/* 1. Set opcode to SPI Controller */
	spi_controller_set_opfifo( op_cmd, _SPI_CONTROLLER_VAL_OP_LEN_ONE);

	/* 2. Write data to SPI Controller */
	spi_controller_write_data_fifo( &data, _SPI_CONTROLLER_VAL_OP_LEN_ONE);

	return (rtn_status);
}

SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Write_One_Byte( u8 data )
{
	return SPI_CONTROLLER_Write_One_Byte_With_Cmd(_SPI_CONTROLLER_VAL_OP_OUTS, data);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: SPI_CONTROLLER_RTN_T SPI_CONTROLLER_WRITE_NBYTES( u8                        *ptr_data,
 *                                                             u32                       len,
 *                                                             SPI_CONTROLLER_SPEED_T    speed )
 * PURPOSE : To provide interface for write N bytes to SPI bus.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : ptr_data  - The data variable of this function.
 *           len   - The len variable of this function.
 *           speed - The speed variable of this function.
 *   OUTPUT: None
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/16 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Write_NByte( u8 *ptr_data, u32 len, SPI_CONTROLLER_SPEED_T speed )
{
	u8 op_cmd = _SPI_CONTROLLER_VAL_OP_OUTS;
	u32 data_len, remain_len;
	SPI_CONTROLLER_RTN_T rtn_status = SPI_CONTROLLER_RTN_NO_ERROR;

	_SPI_CONTROLLER_DEBUG_PRINTF("SPI_CONTROLLER_Write_NByte: len=0x%x\n", len );
	_SPI_CONTROLLER_DEBUG_PRINTF_ARRAY(ptr_data, len);

	/* 1. Mapping the op code */
	switch( speed )
	{
		case SPI_CONTROLLER_SPEED_SINGLE :
			op_cmd = _SPI_CONTROLLER_VAL_OP_OUTS;
			break;
		case SPI_CONTROLLER_SPEED_DUAL :
			op_cmd = _SPI_CONTROLLER_VAL_OP_OUTD;
			break;
		case SPI_CONTROLLER_SPEED_QUAD :
			op_cmd = _SPI_CONTROLLER_VAL_OP_OUTQ;
			break;
	}

	remain_len = len;
	while (remain_len > 0)
	{
		/* Controller at most process limitation one time */
		if( remain_len > _SPI_CONTROLLER_VAL_OP_LEN_MAX )
		{
			data_len = _SPI_CONTROLLER_VAL_OP_LEN_MAX;
		}
		else
		{
			data_len = remain_len;
		}

		/* 2. Set opcode to SPI Controller */
		spi_controller_set_opfifo( op_cmd, data_len);

		/* 3. Write data to SPI Controller */
		spi_controller_write_data_fifo( &ptr_data[len - remain_len], data_len );

		remain_len -= data_len;
	}

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: SPI_CONTROLLER_RTN_T SPI_CONTROLLER_READ_NBYTES( u8                         *ptr_rtn_data,
 *                                                            u8                         len,
 *                                                            SPI_CONTROLLER_SPEED_T     speed     )
 * PURPOSE : To provide interface for read N bytes from SPI bus.
 * AUTHOR  : Chuck Kuo
 * CALLED BY
 *   -
 * CALLS
 *   -
 * PARAMs  :
 *   INPUT : len       - The len variable of this function.
 *           speed     - The speed variable of this function.
 *   OUTPUT: ptr_rtn_data  - The ptr_rtn_data variable of this function.
 * RETURN  : SPI_RTN_NO_ERROR - Successful.   Otherwise - Failed.
 * NOTES   :
 * MODIFICTION HISTORY:
 * Date 2014/12/16 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Read_NByte(u8 *ptr_rtn_data, u32 len, SPI_CONTROLLER_SPEED_T speed)
{
	u8 op_cmd = _SPI_CONTROLLER_VAL_OP_INS;
	u32 data_len, remain_len;
	SPI_CONTROLLER_RTN_T rtn_status = SPI_CONTROLLER_RTN_NO_ERROR;

	_SPI_CONTROLLER_DEBUG_PRINTF("SPI_CONTROLLER_Read_NByte : \n");

	/* 1. Mapping the op code */
	switch( speed )
	{
		case SPI_CONTROLLER_SPEED_SINGLE :
			op_cmd = _SPI_CONTROLLER_VAL_OP_INS;
			break;
		case SPI_CONTROLLER_SPEED_DUAL :
			op_cmd = _SPI_CONTROLLER_VAL_OP_IND;
			break;
		case SPI_CONTROLLER_SPEED_QUAD :
			op_cmd = _SPI_CONTROLLER_VAL_OP_INQ;
			break;
	}

	remain_len = len;
	while (remain_len > 0)
	{
		/* Controller at most process limitation one time */
		if( remain_len > _SPI_CONTROLLER_VAL_OP_LEN_MAX )
		{
			data_len = _SPI_CONTROLLER_VAL_OP_LEN_MAX;
		}
		else
		{
			data_len = remain_len;
		}

		/* 2. Set opcode to SPI Controller */
		spi_controller_set_opfifo( op_cmd, data_len);

		/* 3. Read data through SPI Controller */
		spi_controller_read_data_fifo( &ptr_rtn_data[len - remain_len], data_len );

		remain_len -= data_len;
	}

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Chip_Select_Low( void )
 * PURPOSE : To provide interface for set chip select low in SPI bus.
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
 * Date 2014/12/16 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Chip_Select_Low(void)
{
	SPI_CONTROLLER_RTN_T rtn_status = SPI_CONTROLLER_RTN_NO_ERROR; 

	spi_controller_set_opfifo( _SPI_CONTROLLER_VAL_OP_CSL, _SPI_CONTROLLER_VAL_OP_LEN_ONE);

	if (isEN751221)
		spi_controller_set_opfifo( _SPI_CONTROLLER_VAL_OP_CSL, _SPI_CONTROLLER_VAL_OP_LEN_ONE);

	return (rtn_status);
}

/*------------------------------------------------------------------------------------
 * FUNCTION: SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Chip_Select_High( void )
 * PURPOSE : To provide interface for set chip select high in SPI bus.
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
 * Date 2014/12/16 by Chuck Kuo - The first revision for this function.
 *
 *------------------------------------------------------------------------------------
 */
SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Chip_Select_High(void)
{
	SPI_CONTROLLER_RTN_T rtn_status = SPI_CONTROLLER_RTN_NO_ERROR; 

	spi_controller_set_opfifo( _SPI_CONTROLLER_VAL_OP_CSH, _SPI_CONTROLLER_VAL_OP_LEN_ONE);

	if (isEN751221)
		spi_controller_set_opfifo( _SPI_CONTROLLER_VAL_OP_CK, _SPI_CONTROLLER_VAL_OP_LEN_FIVE);

	return (rtn_status);
}

/* End of [spi_controller.c] package */
