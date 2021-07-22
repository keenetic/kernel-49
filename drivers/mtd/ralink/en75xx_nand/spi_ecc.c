
/* INCLUDE FILE DECLARATIONS --------------------------------------------------------- */
#include <asm/io.h>
#include <asm/string.h>
#include <asm/tc3162/tc3162.h>

#include "spi_ecc.h"

/* MACRO DECLARATIONS ---------------------------------------------------------------- */

/*******************************************************************************
 * ECC Register Definition
 *******************************************************************************/
#define _SPI_ECC_REGS_BASE						0xBFA12000
#define _SPI_ECC_REGS_ECCCON					(_SPI_ECC_REGS_BASE + 0x0000)
#define _SPI_ECC_REGS_ENCCNFG					(_SPI_ECC_REGS_BASE + 0x0004)
#define _SPI_ECC_REGS_ENCDIADDR					(_SPI_ECC_REGS_BASE + 0x0008)
#define _SPI_ECC_REGS_ENCIDLE					(_SPI_ECC_REGS_BASE + 0x000C)
#define _SPI_ECC_REGS_ENCPAR0					(_SPI_ECC_REGS_BASE + 0x0010)
#define _SPI_ECC_REGS_ENCPAR1					(_SPI_ECC_REGS_BASE + 0x0014)
#define _SPI_ECC_REGS_ENCPAR2					(_SPI_ECC_REGS_BASE + 0x0018)
#define _SPI_ECC_REGS_ENCPAR3					(_SPI_ECC_REGS_BASE + 0x001C)
#define _SPI_ECC_REGS_ENCPAR4					(_SPI_ECC_REGS_BASE + 0x0020)
#define _SPI_ECC_REGS_ENCPAR5					(_SPI_ECC_REGS_BASE + 0x0024)
#define _SPI_ECC_REGS_ENCPAR6					(_SPI_ECC_REGS_BASE + 0x0028)
#define _SPI_ECC_REGS_ENCSTA					(_SPI_ECC_REGS_BASE + 0x002C)
#define _SPI_ECC_REGS_ENCIRQEN					(_SPI_ECC_REGS_BASE + 0x0030)
#define _SPI_ECC_REGS_ENCIRQSTA					(_SPI_ECC_REGS_BASE + 0x0034)
#define _SPI_ECC_REGS_PIO_DIRDY					(_SPI_ECC_REGS_BASE + 0x0080)
#define _SPI_ECC_REGS_PIO_DI					(_SPI_ECC_REGS_BASE + 0x0084)
#define _SPI_ECC_REGS_DECCON					(_SPI_ECC_REGS_BASE + 0x0100)
#define _SPI_ECC_REGS_DECCNFG					(_SPI_ECC_REGS_BASE + 0x0104)
#define _SPI_ECC_REGS_DECDIADDR					(_SPI_ECC_REGS_BASE + 0x0108)
#define _SPI_ECC_REGS_DECIDLE					(_SPI_ECC_REGS_BASE + 0x010C)
#define _SPI_ECC_REGS_DECFER					(_SPI_ECC_REGS_BASE + 0x0110)
#define _SPI_ECC_REGS_DECNUM0					(_SPI_ECC_REGS_BASE + 0x0114)
#define _SPI_ECC_REGS_DECNUM1					(_SPI_ECC_REGS_BASE + 0x0118)
#define _SPI_ECC_REGS_DECDONE					(_SPI_ECC_REGS_BASE + 0x011C)
#define _SPI_ECC_REGS_DECEL0					(_SPI_ECC_REGS_BASE + 0x0120)
#define _SPI_ECC_REGS_DECEL1					(_SPI_ECC_REGS_BASE + 0x0124)
#define _SPI_ECC_REGS_DECEL2					(_SPI_ECC_REGS_BASE + 0x0128)
#define _SPI_ECC_REGS_DECEL3					(_SPI_ECC_REGS_BASE + 0x012C)
#define _SPI_ECC_REGS_DECEL4					(_SPI_ECC_REGS_BASE + 0x0130)
#define _SPI_ECC_REGS_DECEL5					(_SPI_ECC_REGS_BASE + 0x0134)
#define _SPI_ECC_REGS_DECEL6					(_SPI_ECC_REGS_BASE + 0x0138)
#define _SPI_ECC_REGS_DECEL7					(_SPI_ECC_REGS_BASE + 0x013C)
#define _SPI_ECC_REGS_DECIRQEN					(_SPI_ECC_REGS_BASE + 0x0140)
#define _SPI_ECC_REGS_DECIRQSTA					(_SPI_ECC_REGS_BASE + 0x0144)
#define _SPI_ECC_REGS_DECFSM					(_SPI_ECC_REGS_BASE + 0x014C)


/*******************************************************************************
 * ECC Register Field Definition
 *******************************************************************************/

/* ECC_ENCCON */
#define _SPI_ECC_REGS_ECCCON_ENABLE				(0x1)

/* ECC_ENCCNFG */
#define _SPI_ECC_REGS_ENCCNFG_ENCMS_MASK		(0x1FF80000)
#define _SPI_ECC_REGS_ENCCNFG_ENCMS_SHIFT		(19)

#define _SPI_ECC_REGS_ENCCNFG_ENCMODE_MASK		(0x00000030)
#define _SPI_ECC_REGS_ENCCNFG_ENCMODE_SHIFT		(4)
#define _SPI_ECC_REGS_ENCCNFG_ENCMODE_NFIMODE	(0x01)

#define _SPI_ECC_REGS_ENCCNFG_ENCTNUM_MASK		(0x00000007)
#define _SPI_ECC_REGS_ENCCNFG_ENCTNUM_SHIFT		(0)
#define _SPI_ECC_REGS_ENCCNFG_ENCTNUM_4BITS		(0)
#define _SPI_ECC_REGS_ENCCNFG_ENCTNUM_6BITS		(1)
#define _SPI_ECC_REGS_ENCCNFG_ENCTNUM_8BITS		(2)
#define _SPI_ECC_REGS_ENCCNFG_ENCTNUM_10BITS	(3)
#define _SPI_ECC_REGS_ENCCNFG_ENCTNUM_12BITS	(4)
#define _SPI_ECC_REGS_ENCCNFG_ENCTNUM_14BITS	(5)
#define _SPI_ECC_REGS_ENCCNFG_ENCTNUM_16BITS	(6)

/* ECC_ENCIDLE */
#define _SPI_ECC_REGS_ENCIDLE_STAT_PROCESSING	(0)
#define _SPI_ECC_REGS_ENCIDLE_STAT_IDLE			(1)

/* ECC_ENCODE_IRQEN*/
#define _SPI_ECC_REGS_ENCIRQEN_IRQEN			(0x1)

/*ECC_ENCODE_IRQSTATUS */
#define _SPI_ECC_REGS_ENCIRQSTA_PROCESSING		(0)
#define _SPI_ECC_REGS_ENCIRQSTA_DONE			(1)



/* ECC_DECCON */
#define _SPI_ECC_REGS_DECCON_ENABLE				(0x1)

/* ECC_DECCNFG */
#define _SPI_ECC_REGS_DECCNFG_DECMS_MASK		(0x1FFF0000)
#define _SPI_ECC_REGS_DECCNFG_DECMS_SHIFT		(16)

#define _SPI_ECC_REGS_DECCNFG_DECCON_MASK		(0x00003000)
#define _SPI_ECC_REGS_DECCNFG_DECCON_SHIFT		(12)
#define _SPI_ECC_REGS_DECCNFG_DECCON_VALUE		(0x3)

#define _SPI_ECC_REGS_DECCNFG_DECMODE_MASK		(0x00000030)
#define _SPI_ECC_REGS_DECCNFG_DECMODE_SHIFT		(4)
#define _SPI_ECC_REGS_DECCNFG_DECMODE_NFIMODE	(0x01)

#define _SPI_ECC_REGS_DECCNFG_DECEMPTY_MASK		(0x80000000)
#define _SPI_ECC_REGS_DECCNFG_DECEMPTY_SHIFT	(31)
#if defined(CONFIG_ECONET_EN7516) || \
    defined(CONFIG_ECONET_EN7527) || \
    defined(CONFIG_ECONET_EN7528)
#define _SPI_ECC_REGS_DECCNFG_DECEMPTY_VALUE	(0x0)
#else
#define _SPI_ECC_REGS_DECCNFG_DECEMPTY_VALUE	(0x1)
#endif

#define _SPI_ECC_REGS_DECCNFG_DECTNUM_MASK		(0x00000007)
#define _SPI_ECC_REGS_DECCNFG_DECTNUM_SHIFT		(0)
#define _SPI_ECC_REGS_DECCNFG_DECTNUM_4BITS		(0)
#define _SPI_ECC_REGS_DECCNFG_DECTNUM_6BITS		(1)
#define _SPI_ECC_REGS_DECCNFG_DECTNUM_8BITS		(2)
#define _SPI_ECC_REGS_DECCNFG_DECTNUM_10BITS	(3)
#define _SPI_ECC_REGS_DECCNFG_DECTNUM_12BITS	(4)
#define _SPI_ECC_REGS_DECCNFG_DECTNUM_14BITS	(5)
#define _SPI_ECC_REGS_DECCNFG_DECTNUM_16BITS	(6)

/* ECC_DECIDLE */
#define _SPI_ECC_REGS_DECIDLE_STAT_PROCESSING	(0)
#define _SPI_ECC_REGS_DECIDLE_STAT_IDLE			(1)

/* ECC_DECODE_IRQEN*/
#define _SPI_ECC_REGS_DECIRQEN_IRQEN			(0x1)

/*ECC_DECODE_IRQSTATUS */
#define _SPI_ECC_REGS_DECIRQSTA_PROCESSING		(0)
#define _SPI_ECC_REGS_DECIRQSTA_DONE			(1)

/* ECC_DECODE NUM0 */
#define _SPI_ECC_REGS_DECNUM0_ERRNUM0_MASK		(0x1F)
#define _SPI_ECC_REGS_DECNUM0_ERRNUM0_SHIFT		(0)

#define _SPI_ECC_REGS_DECNUM0_ERRNUM1_MASK		(0x3E0)
#define _SPI_ECC_REGS_DECNUM0_ERRNUM1_SHIFT		(5)

#define _SPI_ECC_REGS_DECNUM0_ERRNUM2_MASK		(0x7C00)
#define _SPI_ECC_REGS_DECNUM0_ERRNUM2_SHIFT		(10)

#define _SPI_ECC_REGS_DECNUM0_ERRNUM3_MASK		(0x000F8000)
#define _SPI_ECC_REGS_DECNUM0_ERRNUM3_SHIFT		(15)

/* ECC_DECODE NUM1 */
#define _SPI_ECC_REGS_DECNUM1_ERRNUM4_MASK		(0x1F)
#define _SPI_ECC_REGS_DECNUM1_ERRNUM4_SHIFT		(0)

#define _SPI_ECC_REGS_DECNUM1_ERRNUM5_MASK		(0x3E0)
#define _SPI_ECC_REGS_DECNUM1_ERRNUM5_SHIFT		(5)

#define _SPI_ECC_REGS_DECNUM1_ERRNUM6_MASK		(0x7C00)
#define _SPI_ECC_REGS_DECNUM1_ERRNUM6_SHIFT		(10)

#define _SPI_ECC_REGS_DECNUM1_ERRNUM7_MASK		(0x000F8000)
#define _SPI_ECC_REGS_DECNUM1_ERRNUM7_SHIFT		(15)

#define _SPI_ECC_UNCORRECTABLE_VALUE			(0x1F)

#define READ_REGISTER_UINT32(reg) \
    (*(volatile unsigned int  * const)(reg))

#define WRITE_REGISTER_UINT32(reg, val) \
    (*(volatile unsigned int  * const)(reg)) = (val)

#define INREG32(x)          READ_REGISTER_UINT32((unsigned int *)((void*)(x)))
#define OUTREG32(x, y)      WRITE_REGISTER_UINT32((unsigned int *)((void*)(x)), (unsigned int )(y))
#define SETREG32(x, y)      OUTREG32(x, INREG32(x)|(y))
#define CLRREG32(x, y)      OUTREG32(x, INREG32(x)&~(y))
#define MASKREG32(x, y, z)  OUTREG32(x, (INREG32(x)&~(y))|(z))

#define _SPI_ECC_REG8_READ(addr)						INREG32(addr)
#define _SPI_ECC_REG8_WRITE(addr, data)					OUTREG32(addr, data)
#define _SPI_ECC_REG8_SETBITS(addr, data)				SETREG32(addr, data)
#define _SPI_ECC_REG8_CLRBITS(addr, data)				CLRREG32(addr, data)
#define _SPI_ECC_REG8_SETMASKBITS(addr, mask, data)		MASKREG32(addr, mask, data)

#define _SPI_ECC_REG16_READ(addr)						INREG32(addr)
#define _SPI_ECC_REG16_WRITE(addr, data)				OUTREG32(addr, data)
#define _SPI_ECC_REG16_SETBITS(addr, data)				SETREG32(addr, data)
#define _SPI_ECC_REG16_CLRBITS(addr, data)				CLRREG32(addr, data)
#define _SPI_ECC_REG16_SETMASKBITS(addr, mask, data)	MASKREG32(addr, mask, data)

#define _SPI_ECC_REG32_READ(addr)						INREG32(addr)
#define _SPI_ECC_REG32_WRITE(addr, data)				OUTREG32(addr, data)
#define _SPI_ECC_REG32_SETBITS(addr, data)				SETREG32(addr, data)
#define _SPI_ECC_REG32_CLRBITS(addr, data)				CLRREG32(addr, data)
#define _SPI_ECC_REG32_SETMASKBITS(addr, mask, data)	MASKREG32(addr, mask, data)

#define _SPI_ECC_GET_ENCODE_INFO_PTR	&(_spi_ecc_encode_conf_info_t)
#define _SPI_ECC_GET_DECODE_INFO_PTR	&(_spi_ecc_decode_conf_info_t)

#define _SPI_ECC_PRINTF			printk
#if defined(SPI_ECC_DEBUG)
#define _SPI_ECC_DEBUG_PRINTF		spi_ecc_debug_printf
#else
#define _SPI_ECC_DEBUG_PRINTF(args...)
#endif

#define _SPI_ECC_MEMCPY			memcpy

/* FUNCTION DECLARATIONS ------------------------------------------------------ */

/* TYPE DECLARATIONS ----------------------------------------------------------------- */

/* STATIC VARIABLE DECLARATIONS ------------------------------------------------------ */
static SPI_ECC_ENCODE_CONF_T _spi_ecc_encode_conf_info_t;
static SPI_ECC_DECODE_CONF_T _spi_ecc_decode_conf_info_t;

/* LOCAL SUBPROGRAM BODIES------------------------------------------------------------ */

#if defined(SPI_ECC_DEBUG)
static u8 _SPI_ECC_DEBUG_FLAG = 0;	/* For control printf debug message or not */

static void spi_ecc_debug_printf( char *fmt, ... )
{
	if( _SPI_ECC_DEBUG_FLAG == 1 )
	{
		unsigned char 		str_buf[100];
		va_list 			argptr;
		int 				cnt;

		va_start(argptr, fmt);
		cnt = vsprintf(str_buf, fmt, argptr);
		va_end(argptr);

		_SPI_ECC_PRINTF	("%s", str_buf);
	}
}
#endif

/* EXPORTED SUBPROGRAM BODIES -------------------------------------------------------- */

SPI_ECC_RTN_T SPI_ECC_Regs_Dump( void )
{
	u32 idx;

	for(idx = _SPI_ECC_REGS_BASE ; idx <= _SPI_ECC_REGS_DECFSM ; idx +=4)
	{
		_SPI_ECC_PRINTF("reg(0x%x) = 0x%x\n", idx, _SPI_ECC_REG32_READ(idx) );
	}

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_Encode_Check_Idle( SPI_ECC_ENCODE_STATUS_T *prt_rtn_encode_status_t )
{
	if( _SPI_ECC_REG16_READ(_SPI_ECC_REGS_ENCIDLE) == _SPI_ECC_REGS_ENCIDLE_STAT_PROCESSING )
	{
		*prt_rtn_encode_status_t = SPI_ECC_ENCODE_STATUS_PROCESSING ;
	}
	if( _SPI_ECC_REG16_READ(_SPI_ECC_REGS_ENCIDLE) == _SPI_ECC_REGS_ENCIDLE_STAT_IDLE )
	{
		*prt_rtn_encode_status_t = SPI_ECC_ENCODE_STATUS_IDLE ;
	}

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_Encode_Check_Done( SPI_ECC_ENCODE_STATUS_T *prt_rtn_encode_status_t )
{
	if( _SPI_ECC_REG16_READ(_SPI_ECC_REGS_ENCIRQSTA) == _SPI_ECC_REGS_ENCIRQSTA_PROCESSING )
	{
		*prt_rtn_encode_status_t = SPI_ECC_ENCODE_STATUS_PROCESSING ;
	}
	if( _SPI_ECC_REG16_READ(_SPI_ECC_REGS_ENCIRQSTA) == _SPI_ECC_REGS_ENCIRQSTA_DONE )
	{
		*prt_rtn_encode_status_t = SPI_ECC_ENCODE_STATUS_DONE ;
	}

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_Encode_Get_Configure( SPI_ECC_ENCODE_CONF_T  *ptr_rtn_encode_conf_t )
{
	SPI_ECC_ENCODE_CONF_T *encode_conf_t = _SPI_ECC_GET_ENCODE_INFO_PTR;

	_SPI_ECC_MEMCPY(ptr_rtn_encode_conf_t, encode_conf_t, sizeof(SPI_ECC_ENCODE_CONF_T));

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_Encode_Set_Configure( SPI_ECC_ENCODE_CONF_T  *ptr_encode_conf_t )
{
	SPI_ECC_ENCODE_CONF_T *spi_ecc_encode_info_t = _SPI_ECC_GET_ENCODE_INFO_PTR;

	/* Store new setting */
	_SPI_ECC_MEMCPY(spi_ecc_encode_info_t, ptr_encode_conf_t, sizeof(SPI_ECC_ENCODE_CONF_T));

	/* Set Block size */
	_SPI_ECC_REG32_SETMASKBITS( _SPI_ECC_REGS_ENCCNFG,  _SPI_ECC_REGS_ENCCNFG_ENCMS_MASK,	\
					(ptr_encode_conf_t->encode_block_size) << _SPI_ECC_REGS_ENCCNFG_ENCMS_SHIFT );

	/* Set ECC Ability */
	if( (ptr_encode_conf_t->encode_ecc_abiliry) == SPI_ECC_ENCODE_ABILITY_4BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_ENCCNFG, _SPI_ECC_REGS_ENCCNFG_ENCTNUM_MASK,	\
						_SPI_ECC_REGS_ENCCNFG_ENCTNUM_4BITS << _SPI_ECC_REGS_ENCCNFG_ENCTNUM_SHIFT );
	}
	if( (ptr_encode_conf_t->encode_ecc_abiliry) == SPI_ECC_ENCODE_ABILITY_6BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_ENCCNFG, _SPI_ECC_REGS_ENCCNFG_ENCTNUM_MASK,	\
						_SPI_ECC_REGS_ENCCNFG_ENCTNUM_6BITS << _SPI_ECC_REGS_ENCCNFG_ENCTNUM_SHIFT );
	}
	if( (ptr_encode_conf_t->encode_ecc_abiliry) == SPI_ECC_ENCODE_ABILITY_8BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_ENCCNFG, _SPI_ECC_REGS_ENCCNFG_ENCTNUM_MASK,	\
						_SPI_ECC_REGS_ENCCNFG_ENCTNUM_8BITS << _SPI_ECC_REGS_ENCCNFG_ENCTNUM_SHIFT );
	}
	if( (ptr_encode_conf_t->encode_ecc_abiliry) == SPI_ECC_ENCODE_ABILITY_10BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_ENCCNFG, _SPI_ECC_REGS_ENCCNFG_ENCTNUM_MASK,	\
						_SPI_ECC_REGS_ENCCNFG_ENCTNUM_10BITS << _SPI_ECC_REGS_ENCCNFG_ENCTNUM_SHIFT );
	}
	if( (ptr_encode_conf_t->encode_ecc_abiliry) == SPI_ECC_ENCODE_ABILITY_12BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_ENCCNFG, _SPI_ECC_REGS_ENCCNFG_ENCTNUM_MASK,	\
						_SPI_ECC_REGS_ENCCNFG_ENCTNUM_12BITS << _SPI_ECC_REGS_ENCCNFG_ENCTNUM_SHIFT );
	}
	if( (ptr_encode_conf_t->encode_ecc_abiliry) == SPI_ECC_ENCODE_ABILITY_14BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_ENCCNFG, _SPI_ECC_REGS_ENCCNFG_ENCTNUM_MASK,	\
						_SPI_ECC_REGS_ENCCNFG_ENCTNUM_14BITS << _SPI_ECC_REGS_ENCCNFG_ENCTNUM_SHIFT );
	}
	if( (ptr_encode_conf_t->encode_ecc_abiliry) == SPI_ECC_ENCODE_ABILITY_16BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_ENCCNFG, _SPI_ECC_REGS_ENCCNFG_ENCTNUM_MASK,	\
						_SPI_ECC_REGS_ENCCNFG_ENCTNUM_16BITS << _SPI_ECC_REGS_ENCCNFG_ENCTNUM_SHIFT );
	}

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_Encode_Enable( void)
{
	_SPI_ECC_REG16_SETBITS(_SPI_ECC_REGS_ECCCON, _SPI_ECC_REGS_ECCCON_ENABLE);

	_SPI_ECC_DEBUG_PRINTF("SPI_ECC_Encode_Set_Configure : encode enable reg(0x%x) = 0x%x\n",
		_SPI_ECC_REGS_ECCCON, _SPI_ECC_REG16_READ(_SPI_ECC_REGS_ECCCON) );

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_Encode_Disable( void)
{
	_SPI_ECC_REG16_CLRBITS(_SPI_ECC_REGS_ECCCON, _SPI_ECC_REGS_ECCCON_ENABLE);

	_SPI_ECC_DEBUG_PRINTF("SPI_ECC_Encode_Set_Configure : encode disable reg(0x%x) = 0x%x\n",
		 _SPI_ECC_REGS_ECCCON, _SPI_ECC_REG16_READ(_SPI_ECC_REGS_ECCCON) );

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_Encode_Init( void )
{
	/* Set Encode Mode as NFI mode */
	_SPI_ECC_REG32_SETMASKBITS( _SPI_ECC_REGS_ENCCNFG, _SPI_ECC_REGS_ENCCNFG_ENCMODE_MASK,	\
					_SPI_ECC_REGS_ENCCNFG_ENCMODE_NFIMODE << _SPI_ECC_REGS_ENCCNFG_ENCMODE_SHIFT );

	/* Enable Encoder IRQ function */
	_SPI_ECC_REG16_SETBITS( _SPI_ECC_REGS_ENCIRQEN, _SPI_ECC_REGS_ENCIRQEN_IRQEN);

	return (SPI_ECC_RTN_NO_ERROR);
}

/*******************************************************************************************************/
/*******************************************************************************************************/

SPI_ECC_RTN_T SPI_ECC_Decode_Check_Idle( SPI_ECC_DECODE_STATUS_T *prt_rtn_decode_status_t )
{
	if( _SPI_ECC_REG16_READ(_SPI_ECC_REGS_DECIDLE) == _SPI_ECC_REGS_DECIDLE_STAT_PROCESSING )
	{
		*prt_rtn_decode_status_t = SPI_ECC_DECODE_STATUS_PROCESSING ;
	}
	if( _SPI_ECC_REG16_READ(_SPI_ECC_REGS_DECIDLE) == _SPI_ECC_REGS_DECIDLE_STAT_IDLE )
	{
		*prt_rtn_decode_status_t = SPI_ECC_DECODE_STATUS_IDLE ;
	}

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_Decode_Check_Done( SPI_ECC_DECODE_STATUS_T *prt_rtn_decode_status_t )
{
	u32 ret_val = 0;

	ret_val = _SPI_ECC_REG16_READ(_SPI_ECC_REGS_DECIRQSTA);

	if( ret_val != _SPI_ECC_REGS_DECIRQSTA_PROCESSING )
	{
		_SPI_ECC_DEBUG_PRINTF("SPI_ECC_Decode_Check_Done : decode done, ret_val = 0x%x\n", ret_val);
		*prt_rtn_decode_status_t = SPI_ECC_DECODE_STATUS_DONE ;
	}
	else
	{
		*prt_rtn_decode_status_t = SPI_ECC_DECODE_STATUS_PROCESSING ;
	}

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_DECODE_Check_Correction_Status( void )
{
	u32 dec_err_number_reg0;
	u32 dec_err_number_reg1;
	SPI_ECC_RTN_T rtn_status = SPI_ECC_RTN_NO_ERROR;

	dec_err_number_reg0 = _SPI_ECC_REG32_READ(_SPI_ECC_REGS_DECNUM0);

	/* Sector 0 can be correctalbe or not */
	if( ((dec_err_number_reg0 & _SPI_ECC_REGS_DECNUM0_ERRNUM0_MASK )  \
			>> _SPI_ECC_REGS_DECNUM0_ERRNUM0_SHIFT) ==  _SPI_ECC_UNCORRECTABLE_VALUE )
	{
		_SPI_ECC_PRINTF("SPI_ECC_DECODE_Check_Correction_Status : sector 0 uncorrectable.\n");
		rtn_status = SPI_ECC_RTN_CORRECTION_ERROR;
	}

	/* Sector 1 can be correctalbe or not */
	if( ((dec_err_number_reg0 & _SPI_ECC_REGS_DECNUM0_ERRNUM1_MASK )  \
			>> _SPI_ECC_REGS_DECNUM0_ERRNUM1_SHIFT) ==  _SPI_ECC_UNCORRECTABLE_VALUE )
	{
		_SPI_ECC_PRINTF("SPI_ECC_DECODE_Check_Correction_Status : sector 1 uncorrectable.\n");
		rtn_status = SPI_ECC_RTN_CORRECTION_ERROR;
	}

	/* Sector 2 can be correctalbe or not */
	if( ((dec_err_number_reg0 & _SPI_ECC_REGS_DECNUM0_ERRNUM2_MASK )  \
			>> _SPI_ECC_REGS_DECNUM0_ERRNUM2_SHIFT) ==  _SPI_ECC_UNCORRECTABLE_VALUE )
	{
		_SPI_ECC_PRINTF("SPI_ECC_DECODE_Check_Correction_Status : sector 2 uncorrectable.\n");
		rtn_status = SPI_ECC_RTN_CORRECTION_ERROR;
	}

	/* Sector 3 can be correctalbe or not */
	if( ((dec_err_number_reg0 & _SPI_ECC_REGS_DECNUM0_ERRNUM3_MASK )  \
			>> _SPI_ECC_REGS_DECNUM0_ERRNUM3_SHIFT) ==  _SPI_ECC_UNCORRECTABLE_VALUE )
	{
		_SPI_ECC_PRINTF("SPI_ECC_DECODE_Check_Correction_Status : sector 3 uncorrectable.\n");
		rtn_status = SPI_ECC_RTN_CORRECTION_ERROR;
	}

	dec_err_number_reg1 = _SPI_ECC_REG32_READ(_SPI_ECC_REGS_DECNUM1);

	/* Sector 4 can be correctalbe or not */
	if( ((dec_err_number_reg1 & _SPI_ECC_REGS_DECNUM1_ERRNUM4_MASK )  \
			>> _SPI_ECC_REGS_DECNUM1_ERRNUM4_SHIFT) ==  _SPI_ECC_UNCORRECTABLE_VALUE )
	{
		_SPI_ECC_PRINTF("SPI_ECC_DECODE_Check_Correction_Status : sector 4 uncorrectable.\n");
		rtn_status = SPI_ECC_RTN_CORRECTION_ERROR;
	}
	/* Sector 5 can be correctalbe or not */
	if( ((dec_err_number_reg1 & _SPI_ECC_REGS_DECNUM1_ERRNUM5_MASK )  \
			>> _SPI_ECC_REGS_DECNUM1_ERRNUM5_SHIFT) ==  _SPI_ECC_UNCORRECTABLE_VALUE )
	{
		_SPI_ECC_PRINTF("SPI_ECC_DECODE_Check_Correction_Status : sector 5 uncorrectable.\n");
		rtn_status = SPI_ECC_RTN_CORRECTION_ERROR;
	}
	/* Sector 6 can be correctalbe or not */
	if( ((dec_err_number_reg1 & _SPI_ECC_REGS_DECNUM1_ERRNUM6_MASK )  \
			>> _SPI_ECC_REGS_DECNUM1_ERRNUM6_SHIFT) ==  _SPI_ECC_UNCORRECTABLE_VALUE )
	{
		_SPI_ECC_PRINTF("SPI_ECC_DECODE_Check_Correction_Status : sector 6 uncorrectable.\n");
		rtn_status = SPI_ECC_RTN_CORRECTION_ERROR;
	}
	/* Sector 7 can be correctalbe or not */
	if( ((dec_err_number_reg1 & _SPI_ECC_REGS_DECNUM1_ERRNUM7_MASK )  \
			>> _SPI_ECC_REGS_DECNUM1_ERRNUM7_SHIFT) ==  _SPI_ECC_UNCORRECTABLE_VALUE )
	{
		_SPI_ECC_PRINTF("SPI_ECC_DECODE_Check_Correction_Status : sector 7 uncorrectable.\n");
		rtn_status = SPI_ECC_RTN_CORRECTION_ERROR;
	}

	return ( rtn_status );
}

SPI_ECC_RTN_T SPI_ECC_Decode_Get_Configure( SPI_ECC_DECODE_CONF_T  *ptr_rtn_decode_conf_t )
{
	SPI_ECC_DECODE_CONF_T *decode_conf_t = _SPI_ECC_GET_DECODE_INFO_PTR;

	_SPI_ECC_MEMCPY(ptr_rtn_decode_conf_t, decode_conf_t, sizeof(SPI_ECC_DECODE_CONF_T));

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_Decode_Set_Configure( SPI_ECC_DECODE_CONF_T  *ptr_decode_conf_t )
{
	SPI_ECC_DECODE_CONF_T *spi_ecc_decode_info_t = _SPI_ECC_GET_DECODE_INFO_PTR;

	/* Store new setting */
	_SPI_ECC_MEMCPY(spi_ecc_decode_info_t, ptr_decode_conf_t, sizeof(SPI_ECC_DECODE_CONF_T));

	/* Set Block size */
	_SPI_ECC_REG32_SETMASKBITS( _SPI_ECC_REGS_DECCNFG,  _SPI_ECC_REGS_DECCNFG_DECMS_MASK,	\
					(ptr_decode_conf_t->decode_block_size) << _SPI_ECC_REGS_DECCNFG_DECMS_SHIFT );

	/* Set ECC Ability */
	if( (ptr_decode_conf_t->decode_ecc_abiliry) == SPI_ECC_DECODE_ABILITY_4BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_DECCNFG, _SPI_ECC_REGS_DECCNFG_DECTNUM_MASK,	\
					_SPI_ECC_REGS_DECCNFG_DECTNUM_4BITS << _SPI_ECC_REGS_DECCNFG_DECTNUM_SHIFT );
	}
	if( (ptr_decode_conf_t->decode_ecc_abiliry) == SPI_ECC_DECODE_ABILITY_6BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_DECCNFG, _SPI_ECC_REGS_DECCNFG_DECTNUM_MASK,	\
						_SPI_ECC_REGS_DECCNFG_DECTNUM_6BITS << _SPI_ECC_REGS_DECCNFG_DECTNUM_SHIFT );
	}
	if( (ptr_decode_conf_t->decode_ecc_abiliry) == SPI_ECC_DECODE_ABILITY_8BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_DECCNFG, _SPI_ECC_REGS_DECCNFG_DECTNUM_MASK,	\
						_SPI_ECC_REGS_DECCNFG_DECTNUM_8BITS << _SPI_ECC_REGS_DECCNFG_DECTNUM_SHIFT );
	}
	if( (ptr_decode_conf_t->decode_ecc_abiliry) == SPI_ECC_DECODE_ABILITY_10BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_DECCNFG, _SPI_ECC_REGS_DECCNFG_DECTNUM_MASK,	\
						_SPI_ECC_REGS_DECCNFG_DECTNUM_10BITS << _SPI_ECC_REGS_DECCNFG_DECTNUM_SHIFT );
	}
	if( (ptr_decode_conf_t->decode_ecc_abiliry) == SPI_ECC_DECODE_ABILITY_12BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_DECCNFG, _SPI_ECC_REGS_DECCNFG_DECTNUM_MASK,	\
						_SPI_ECC_REGS_DECCNFG_DECTNUM_12BITS << _SPI_ECC_REGS_DECCNFG_DECTNUM_SHIFT );
	}
	if( (ptr_decode_conf_t->decode_ecc_abiliry) == SPI_ECC_DECODE_ABILITY_14BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_DECCNFG, _SPI_ECC_REGS_DECCNFG_DECTNUM_MASK,	\
						_SPI_ECC_REGS_DECCNFG_DECTNUM_14BITS << _SPI_ECC_REGS_DECCNFG_DECTNUM_SHIFT );
	}
	if( (ptr_decode_conf_t->decode_ecc_abiliry) == SPI_ECC_DECODE_ABILITY_16BITS )
	{
		_SPI_ECC_REG32_SETMASKBITS(_SPI_ECC_REGS_DECCNFG, _SPI_ECC_REGS_DECCNFG_DECTNUM_MASK,	\
						_SPI_ECC_REGS_DECCNFG_DECTNUM_16BITS << _SPI_ECC_REGS_DECCNFG_DECTNUM_SHIFT );
	}

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_Decode_Enable( void)
{
	_SPI_ECC_REG16_SETBITS(_SPI_ECC_REGS_DECCON, _SPI_ECC_REGS_DECCON_ENABLE);

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_Decode_Disable( void)
{
	_SPI_ECC_REG16_CLRBITS(_SPI_ECC_REGS_DECCON, _SPI_ECC_REGS_DECCON_ENABLE);

	return (SPI_ECC_RTN_NO_ERROR);
}

SPI_ECC_RTN_T SPI_ECC_Decode_Init( void )
{
	/* Set Decode Mode as NFI mode */
	_SPI_ECC_REG32_SETMASKBITS( _SPI_ECC_REGS_DECCNFG, _SPI_ECC_REGS_DECCNFG_DECMODE_MASK,	\
					_SPI_ECC_REGS_DECCNFG_DECMODE_NFIMODE << _SPI_ECC_REGS_DECCNFG_DECMODE_SHIFT );

	/* Set Decode Mode have igore empty data function */
	_SPI_ECC_REG32_SETMASKBITS( _SPI_ECC_REGS_DECCNFG, _SPI_ECC_REGS_DECCNFG_DECEMPTY_MASK,	\
					_SPI_ECC_REGS_DECCNFG_DECEMPTY_VALUE << _SPI_ECC_REGS_DECCNFG_DECEMPTY_SHIFT );

	/* Set Decode has most poweful ability */
	_SPI_ECC_REG32_SETMASKBITS( _SPI_ECC_REGS_DECCNFG, _SPI_ECC_REGS_DECCNFG_DECCON_MASK,	\
					_SPI_ECC_REGS_DECCNFG_DECCON_VALUE << _SPI_ECC_REGS_DECCNFG_DECCON_SHIFT );

	/* Enable Decoder IRQ function */
	_SPI_ECC_REG16_SETBITS( _SPI_ECC_REGS_DECIRQEN, _SPI_ECC_REGS_DECIRQEN_IRQEN);

	return (SPI_ECC_RTN_NO_ERROR);
}

