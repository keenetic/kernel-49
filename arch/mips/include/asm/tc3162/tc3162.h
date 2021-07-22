/*
** $Id: tc3162.h,v 1.7 2011/01/07 06:05:58 pork Exp $
*/
/************************************************************************
 *
 *	Copyright (C) 2006 Trendchip Technologies, Corp.
 *	All Rights Reserved.
 *
 * Trendchip Confidential; Need to Know only.
 * Protected as an unpublished work.
 *
 * The computer program listings, specifications and documentation
 * herein are the property of Trendchip Technologies, Co. and shall
 * not be reproduced, copied, disclosed, or used in whole or in part
 * for any reason without the prior express written permission of
 * Trendchip Technologeis, Co.
 *
 *************************************************************************/
/*
** $Log: tc3162.h,v $
** Revision 1.7  2011/01/07 06:05:58  pork
** add the definition of INT!16,INT32,SINT15,SINT7
**
** Revision 1.6  2010/09/20 07:08:02  shnwind
** decrease nf_conntrack buffer size
**
** Revision 1.5  2010/09/03 16:43:07  here
** [Ehance] TC3182 GMAC Driver is support TC-Console & WAN2LAN function & update the tc3182 dmt version (3.12.8.83)
**
** Revision 1.4  2010/09/02 07:04:50  here
** [Ehance] Support TC3162U/TC3182 Auto-Bench
**
** Revision 1.3  2010/08/30 07:53:02  lino
** add power saving mode kernel module support
**
** Revision 1.2  2010/06/05 05:40:29  lino
** add tc3182 asic board support
**
** Revision 1.1.1.1  2010/04/09 09:39:21  feiyan
** New TC Linux Make Flow Trunk
**
** Revision 1.4  2010/01/14 10:56:42  shnwind
** recommit
**
** Revision 1.3  2010/01/14 08:00:10  shnwind
** add TC3182 support
**
** Revision 1.2  2010/01/10 15:27:26  here
** [Ehancement]TC3162U MAC EEE is operated at 100M-FD, SAR interface is accroding the SAR_CLK to calculate atm rate.
**
** Revision 1.1.1.1  2009/12/17 01:42:47  josephxu
** 20091217, from Hinchu ,with VoIP
**
** Revision 1.2  2006/07/06 07:24:57  lino
** update copyright year
**
** Revision 1.1.1.1  2005/11/02 05:45:38  lino
** no message
**
** Revision 1.5  2005/09/27 08:01:38  bread.hsu
** adding IMEM support for Tc3162L2
**
** Revision 1.4  2005/09/14 11:06:20  bread.hsu
** new definition for TC3162L2
**
** Revision 1.3  2005/06/17 16:26:16  jasonlin
** Remove redundant code to gain extra 100K bytes free memory.
** Add "CODE_REDUCTION" definition to switch
**
** Revision 1.2  2005/06/14 10:02:01  jasonlin
** Merge TC3162L2 source code into new main trunk
**
** Revision 1.1.1.1  2005/03/30 14:04:22  jasonlin
** Import Linos source code
**
** Revision 1.4  2004/11/15 03:43:17  lino
** rename ATM SAR max packet length register
**
** Revision 1.3  2004/09/01 13:15:47  lino
** fixed when pc shutdown, system will reboot
**
** Revision 1.2  2004/08/27 12:16:37  lino
** change SYS_HCLK to 96Mhz
**
** Revision 1.1  2004/07/02 08:03:04  lino
** tc3160 and tc3162 code merge
**
*/

#ifndef _TC3162_H_
#define _TC3162_H_

#include <asm/setup.h>

#include "tc3262_int_source.h"

#ifndef INT32
#define INT32
typedef signed long int int32;    		/* 32-bit signed integer        */
#endif

#ifndef SINT31
#define SINT31
typedef signed long int sint31;        	/* 32-bit signed integer        */
#endif

#ifndef UINT32
#define UINT32
typedef unsigned long int uint32; 		/* 32-bit unsigned integer      */
#endif

#ifndef SINT15
#define SINT15
typedef signed short sint15;            /* 16-bit signed integer        */
#endif

#ifndef INT16
#define INT16
typedef signed short int int16;         /* 16-bit signed integer        */
#endif

#ifndef UINT16
#define UINT16
typedef unsigned short uint16;          /* 16-bit unsigned integer      */
#endif

#ifndef SINT7
#define SINT7
typedef signed char sint7;              /* 8-bit signed integer         */
#endif

#ifndef UINT8
#define UINT8
typedef unsigned char uint8;            /* 8-bit unsigned integer       */
#endif

#ifndef VPint
#define VPint			*(volatile unsigned long int *)
#endif
#ifndef VPshort
#define VPshort			*(volatile unsigned short *)
#endif
#ifndef VPchar
#define VPchar			*(volatile unsigned char *)
#endif

static inline uint32 regRead32(uint32 reg)
{
	return VPint(reg);
}

static inline void regWrite32(uint32 reg, uint32 val)
{
	VPint(reg) = val;
}

#define isRT63365 		(((VPint(0xbfb00064) & 0xffff0000)) == 0x00040000)
#define isMT751020		(((VPint(0xbfb00064) & 0xffff0000)) == 0x00050000)
#define isMT7505		(((VPint(0xbfb00064) & 0xffff0000)) == 0x00060000)
#define isEN7526c		(((VPint(0xbfb00064) & 0xffff0000)) == 0x00080000)
#define isEN751221		((((VPint(0xbfb00064) & 0xffff0000)) == 0x00070000) || isEN7526c)
#define isEN7528		(((VPint(0xbfb00064) & 0xffff0000)) == 0x000B0000)
#ifdef CONFIG_ECONET_EN7528
#define isEN751627		((((VPint(0xbfb00064) & 0xffff0000)) == 0x00090000) || isEN7528)
#else
#define isEN751627		(((VPint(0xbfb00064) & 0xffff0000)) == 0x00090000)
#endif
#define isEN7580 		(((VPint(0xbfb00064) & 0xffff0000)) == 0x000A0000)

/* Support old xDSL chips */
#define isTC3162L2P2		((((unsigned char)(VPint(0xbfb0008c)>>12)&0xff)!=0)&&(((VPint(0xbfb00064)&0xffffffff))==0x00000000)?1:0)
#define isTC3162L3P3		((((unsigned char)(VPint(0xbfb0008c)>>12)&0xff)==7)&&(((VPint(0xbfb00064)&0xffffffff))==0x00000000)?1:0)
#define isTC3162L4P4		((((unsigned char)(VPint(0xbfb0008c)>>12)&0xff)==8)&&(((VPint(0xbfb00064)&0xffffffff))==0x00000000)?1:0)
#define isTC3162L5P5E2		((((unsigned char)(VPint(0xbfb0008c)>>12)&0xff)==0xa)&&(((VPint(0xbfb00064)&0xffffffff))==0x00000000)?1:0)
#define isTC3162L5P5E3		((((unsigned char)(VPint(0xbfb0008c)>>12)&0xff)==0xb)&&(((VPint(0xbfb00064)&0xffffffff))==0x00000000)?1:0)
#define isTC3162L5P5		(isTC3162L5P5E2 || isTC3162L5P5E3)
#define isTC3162U		((((unsigned char)(VPint(0xbfb0008c)>>12)&0xff)==0x10)&&(((VPint(0xbfb00064)&0xffffffff))==0x00000000)?1:0)
#define isRT63260		((((unsigned char)(VPint(0xbfb0008c)>>12)&0xff)==0x20)&&(((VPint(0xbfb00064)&0xffffffff))==0x00000000)?1:0)
#define isTC3169		(((VPint(0xbfb00064)&0xffff0000))==0x00000000)
#define isTC3182		(((VPint(0xbfb00064)&0xffff0000))==0x00010000)
#define isRT65168		(((VPint(0xbfb00064)&0xffff0000))==0x00020000)
#define isRT63165		(((VPint(0xbfb00064)&0xffff0000))==0x00030000)

#define PDIDR			(VPint(0xBFB0005C)&0xFFFF)
#define isEN751221FPGA		((VPint(0xBFB0008C) & (1 << 29)) ? 0 : 1) //used for 7512/7521
#define isGenernalFPGA		((VPint(0xBFB0008C) & (1 << 31)) ? 1 : 0) //used for 63365/751020
#define isGenernalFPGA_2	(((VPint(CR_AHB_SSTR) & 0x1) == 0) ? 1 : 0) //used for EN7526c and later version
#if defined(CONFIG_ECONET_EN7516) || \
    defined(CONFIG_ECONET_EN7527) || \
    defined(CONFIG_ECONET_EN7528)
#define isFPGA			0 // GET_IS_FPGA
#define SYNC_TYPE4()		__asm__ volatile ("sync 0x4")
#else
#define isFPGA			0 //(isEN7526c ? isGenernalFPGA_2 : (isEN751221 ? isEN751221FPGA : isGenernalFPGA))
#endif

#define isEN751627QFP		(((VPint(0xbfa20174) & 0x8000) == 0x8000) ? 1 : 0)

#define EFUSE_VERIFY_DATA0	(0xBFBF8214)
#define EFUSE_VERIFY_DATA1	(0xBFBF8218)
#define EFUSE_PKG_MASK_7516	(0xC0000)
#define EFUSE_REMARK_BIT_7516	(1 << 0)
#define EFUSE_PKG_REMARK_SHITF_7516	2
#define EFUSE_PKG_MASK		(0x3F)
#define EFUSE_REMARK_BIT	(1 << 6)
#define EFUSE_PKG_REMARK_SHITF	7

#define EFUSE_EN7512		(0x4)
#define EFUSE_EN7513		(0x5)
#define EFUSE_EN7513G		(0x6)
#define EFUSE_EN7516G		(0x80000)
#define EFUSE_EN7561G		(0xC0000)
#define EFUSE_EN7527H		(0x0)
#define EFUSE_EN7527G		(0x0)

#define isEN7512		(isEN751221 && \
				( (VPint(EFUSE_VERIFY_DATA0) & EFUSE_REMARK_BIT)? \
				(((VPint(EFUSE_VERIFY_DATA0) >> EFUSE_PKG_REMARK_SHITF) & EFUSE_PKG_MASK) == EFUSE_EN7512): \
				 ((VPint(EFUSE_VERIFY_DATA0) & EFUSE_PKG_MASK) == EFUSE_EN7512)))

#define isEN7513		(isEN751221 && \
				( (VPint(EFUSE_VERIFY_DATA0) & EFUSE_REMARK_BIT)? \
				(((VPint(EFUSE_VERIFY_DATA0) >> EFUSE_PKG_REMARK_SHITF) & EFUSE_PKG_MASK) == EFUSE_EN7513): \
				 ((VPint(EFUSE_VERIFY_DATA0) & EFUSE_PKG_MASK) == EFUSE_EN7513)))

#define isEN7513G		(isEN751221 && \
				( (VPint(EFUSE_VERIFY_DATA0) & EFUSE_REMARK_BIT)? \
				(((VPint(EFUSE_VERIFY_DATA0) >> EFUSE_PKG_REMARK_SHITF) & EFUSE_PKG_MASK) == EFUSE_EN7513G): \
				 ((VPint(EFUSE_VERIFY_DATA0) & EFUSE_PKG_MASK) == EFUSE_EN7513G)))

#define isEN7516G		(isEN751627 && \
				( (VPint(EFUSE_VERIFY_DATA0) & EFUSE_REMARK_BIT_7516)? \
				(((VPint(EFUSE_VERIFY_DATA0) >> EFUSE_PKG_REMARK_SHITF_7516) & EFUSE_PKG_MASK_7516) == EFUSE_EN7516G): \
				 ((VPint(EFUSE_VERIFY_DATA0) & EFUSE_PKG_MASK_7516) == EFUSE_EN7516G)))

#define isEN7561G		(isEN751627 && !isEN751627QFP && \
				( (VPint(EFUSE_VERIFY_DATA0) & EFUSE_REMARK_BIT_7516)? \
				(((VPint(EFUSE_VERIFY_DATA0) >> EFUSE_PKG_REMARK_SHITF_7516) & EFUSE_PKG_MASK_7516) == EFUSE_EN7561G): \
				 ((VPint(EFUSE_VERIFY_DATA0) & EFUSE_PKG_MASK_7516) == EFUSE_EN7516G)))

#define isEN7527H		(isEN751627 && isEN751627QFP && \
				( (VPint(EFUSE_VERIFY_DATA0) & EFUSE_REMARK_BIT_7516)? \
				(((VPint(EFUSE_VERIFY_DATA0) >> EFUSE_PKG_REMARK_SHITF_7516) & EFUSE_PKG_MASK_7516) == EFUSE_EN7527H): \
				 ((VPint(EFUSE_VERIFY_DATA0) & EFUSE_PKG_MASK_7516) == EFUSE_EN7527H)))

#define isEN7527G		(isEN751627 && !isEN751627QFP && \
				( (VPint(EFUSE_VERIFY_DATA0) & EFUSE_REMARK_BIT_7516)? \
				(((VPint(EFUSE_VERIFY_DATA0) >> EFUSE_PKG_REMARK_SHITF_7516) & EFUSE_PKG_MASK_7516) == EFUSE_EN7527G): \
				 ((VPint(EFUSE_VERIFY_DATA0) & EFUSE_PKG_MASK_7516) == EFUSE_EN7527G)))

#define isEN7528HU		(isEN7528 && (GET_PACKAGE_ID == 0x0))
#define isEN7528DU		(isEN7528 && (GET_PACKAGE_ID == 0x1))
#define isEN7561DU		(isEN7528 && (GET_PACKAGE_ID == 0x2))
#define isEN7526FH_EN7528DU	(isEN7528 && (GET_PACKAGE_ID == 0x3))
#define isEN7521G_EN7528DU	(isEN7528 && (GET_PACKAGE_ID == 0x7))

#define EFUSE_DDR3_BIT		(1 << 23)
#define EFUSE_DDR3_REMARK_BIT	(1 << 24)
#define EFUSE_IS_DDR3		( (VPint(EFUSE_VERIFY_DATA0) & EFUSE_REMARK_BIT)? \
				 ((VPint(EFUSE_VERIFY_DATA0) & EFUSE_DDR3_REMARK_BIT)): \
				 ((VPint(EFUSE_VERIFY_DATA0) & EFUSE_DDR3_BIT)))

#define REG_SAVE_INFO		0xBFB00284
#define GET_REG_SAVE_INFO_POINT	((volatile SYS_GLOBAL_PARM_T *)REG_SAVE_INFO)

typedef union {
	struct {
		uint32 packageID		:  4;
		uint32 isDDR4			:  1;
		uint32 isSecureHwTrapEn		:  1; /* detect secure HW trap */
		uint32 isSecureModeEn		:  1; /* RSA key has been written or not */
		uint32 isFlashBoot 		:  1;
		uint32 isCtrlEcc		:  1;
		uint32 isFpga			:  1;
		uint32 sys_clk			: 10; /* bus clock can support up to 1024MHz */
		uint32 dram_size		: 12; /* DRAM size can support up to 2048MB */
	} raw ;
	uint32 word;
} SYS_GLOBAL_PARM_T ;

#define GET_IS_DDR4			(GET_REG_SAVE_INFO_POINT->raw.isDDR4)
#define GET_DRAM_SIZE			(GET_REG_SAVE_INFO_POINT->raw.dram_size)
#define GET_SYS_CLK			(GET_REG_SAVE_INFO_POINT->raw.sys_clk)
#define GET_IS_FPGA			(GET_REG_SAVE_INFO_POINT->raw.isFpga)
#define GET_IS_SPI_ECC			(GET_REG_SAVE_INFO_POINT->raw.isCtrlEcc)
#define GET_PACKAGE_ID			(GET_REG_SAVE_INFO_POINT->raw.packageID)
#define GET_IS_SECURE_MODE		(GET_REG_SAVE_INFO_POINT->raw.isSecureModeEn)
#define GET_IS_SECURE_HWTRAP		(GET_REG_SAVE_INFO_POINT->raw.isSecureHwTrapEn)

#define SYS_HCLK		(GET_SYS_CLK)
#define SAR_CLK			((SYS_HCLK)/(4.0))		//more accurate if 4.0 not 4

/* define CPU timer clock, FPGA is 50Mhz, ASIC is 200Mhz */
#define	CPUTMR_CLK		(isFPGA ? (50*1000000) : (200*1000000))

#define isMT7530		(((VPint(0xbfb58000 + 0x7ffc) & 0xffff0000)) == 0x75300000)

#define DSPRAM_BASE		0x9c000000

#define ENABLE          1
#define DISABLE         0

#define WAN2LAN_CH_ID	(1<<31)

#define tc_inb(offset) 			(*(volatile unsigned char *)(offset))
#define tc_inw(offset) 			(*(volatile unsigned short *)(offset))
#define tc_inl(offset) 			(*(volatile unsigned long *)(offset))

#define tc_outb(offset,val) 		(*(volatile unsigned char *)(offset) = val)
#define tc_outw(offset,val) 		(*(volatile unsigned short *)(offset) = val)
#define tc_outl(offset,val) 		(*(volatile unsigned long *)(offset) = val)

#define IS_SPIFLASH			((~(VPint(0xBFA10114))) & 0x2)
#define IS_NANDFLASH			   (VPint(0xBFA10114)   & 0x2)
#ifdef TCSUPPORT_SPI_CONTROLLER_ECC
#define isSpiControllerECC		(GET_IS_SPI_ECC)
#else
#define isSpiControllerECC		(0)
#endif
#define isSpiNandAndCtrlECC		(IS_NANDFLASH && isSpiControllerECC)

/*****************************
 * RBUS CORE Module Registers *
 *****************************/
#define ARB_CFG 		 0xBFA00008
#define ROUND_ROBIN_ENABLE	 (1<<30)
#define ROUND_ROBIN_DISBALE	~(1<<30)

#if defined(CONFIG_ECONET_EN7528)
#define RBUS_TIMEOUT_STS0	 0xBFA000D0
#define RBUS_TIMEOUT_CFG0	 0xBFA000D8
#define RBUS_TIMEOUT_CFG1	 0xBFA000DC
#define RBUS_TIMEOUT_CFG2	 0xBFA000E0
#endif

/*****************************
 * DMC Module Registers *
 *****************************/

#define CR_DMC_BASE     	0xBFB20000
#define CR_DMC_SRT      	(0x00 | CR_DMC_BASE)
#define CR_DMC_STC      	(0x01 | CR_DMC_BASE)
#define CR_DMC_SAMT      	(0x02 | CR_DMC_BASE)
#define CR_DMC_SCR      	(0x03 | CR_DMC_BASE)

/* RT63165 specific */
/* DDR self refresh control register */
#define CR_DMC_DDR_SR    	(0x18 | CR_DMC_BASE)
/* DDR self refresh target count */
#define CR_DMC_DDR_SR_CNT  	(0x1c | CR_DMC_BASE)
#define CR_DMC_DDR_CFG0    	(0x40 | CR_DMC_BASE)
#define CR_DMC_DDR_CFG1    	(0x44 | CR_DMC_BASE)
#define CR_DMC_DDR_CFG2    	(0x48 | CR_DMC_BASE)
#define CR_DMC_DDR_CFG3    	(0x4c | CR_DMC_BASE)
#define CR_DMC_DDR_CFG4    	(0x50 | CR_DMC_BASE)
#define CR_DMC_DDR_CFG8    	(0x60 | CR_DMC_BASE)
#define CR_DMC_DDR_CFG9    	(0x64 | CR_DMC_BASE)

#define CR_DMC_CTL0      	(0x70 | CR_DMC_BASE)
#define CR_DMC_CTL1      	(0x74 | CR_DMC_BASE)
#define CR_DMC_CTL2      	(0x78 | CR_DMC_BASE)
#define CR_DMC_CTL3     	(0x7c | CR_DMC_BASE)
#define CR_DMC_CTL4     	(0x80 | CR_DMC_BASE)

#define CR_DMC_DCSR     	(0xb0 | CR_DMC_BASE)

#define CR_DMC_ISPCFGR     	(0xc0 | CR_DMC_BASE)
#define CR_DMC_DSPCFGR     	(0xc4 | CR_DMC_BASE)
/*****************************
 * GDMA Module Registers *
 *****************************/

#define CR_GDMA_BASE     	0xBFB30000
#define CR_GDMA_DCSA      	(0x00 | CR_GDMA_BASE)
#define CR_GDMA_DCDA      	(0x04 | CR_GDMA_BASE)
#define CR_GDMA_DCBT      	(0x08 | CR_GDMA_BASE)
#define CR_GDMA_DCBL      	(0x0a | CR_GDMA_BASE)
#define CR_GDMA_DCC      	(0x0c | CR_GDMA_BASE)
#define CR_GDMA_DCS      	(0x0e | CR_GDMA_BASE)
#define CR_GDMA_DCKSUM     	(0x10 | CR_GDMA_BASE)

/*****************************
 * SPI Module Registers *
 *****************************/

#define CR_SPI_BASE     	0xBFBC0000
#define CR_SPI_CTL      	(0x00 | CR_SPI_BASE)
#define CR_SPI_OPCODE      	(0x04 | CR_SPI_BASE)
#define CR_SPI_DATA      	(0x08 | CR_SPI_BASE)

/*****************************
 * Ethernet Module Registers *
 *****************************/

#define CR_MAC_BASE     	0xBFB50000
#define CR_MAC_ISR      	(0x00 | CR_MAC_BASE)// --- Interrupt Status Register ---
#define CR_MAC_IMR      	(0x04 | CR_MAC_BASE)// --- Interrupt Mask Register ---
#define CR_MAC_MADR  	   	(0x08 | CR_MAC_BASE)// --- MAC Address Register [47:32] ---
#define CR_MAC_LADR     	(0x0c | CR_MAC_BASE)// --- MAC Address Register [31:0] ---
#define CR_MAC_EEE		(0x10 | CR_MAC_BASE)
#define CR_MAC_TXPD     	(0x18 | CR_MAC_BASE)// --- Transmit Poll Demand Register ---
#define CR_MAC_RXPD     	(0x1c | CR_MAC_BASE)// --- Receive Poll Demand Register ---
#define CR_MAC_TXR_BADR 	(0x20 | CR_MAC_BASE)// --- Transmit Ring Base Address Register ---
#define CR_MAC_RXR_BADR 	(0x24 | CR_MAC_BASE)// --- Receive Ring Base Address Register ---
#define CR_MAC_ITC      	(0x28 | CR_MAC_BASE)// --- Interrupt Timer Control Register ---
#define CR_MAC_TXR_SIZE  	   (0x2c | CR_MAC_BASE)// --- Transmit Ring Size Register ---
#define CR_MAC_RXR_SIZE      (0x30 | CR_MAC_BASE)// --- Receive Ring Size Register ---
#define CR_MAC_RXR_SWIDX     (0x34 | CR_MAC_BASE)// --- Receive Ring Software Index Register ---
#define CR_MAC_TXDESP_SIZE   (0x38 | CR_MAC_BASE)// --- Transmit Descriptor Size Register ---
#define CR_MAC_RXDESP_SIZE   (0x3c | CR_MAC_BASE)// --- Receive Descriptor Size Register ---
#define CR_MAC_PRIORITY_CFG  (0x50 | CR_MAC_BASE)// --- Priority Configuration Register ---
#define CR_MAC_VLAN_CFG      (0x54 | CR_MAC_BASE)// --- VLAN Configuration Register ---
#define CR_MAC_TOS0_CFG      (0x58 | CR_MAC_BASE)// --- TOS 0 Configuration Register ---
#define CR_MAC_TOS1_CFG      (0x5c | CR_MAC_BASE)// --- TOS 1 Configuration Register ---
#define CR_MAC_TOS2_CFG      (0x60 | CR_MAC_BASE)// --- TOS 2 Configuration Register ---
#define CR_MAC_TOS3_CFG      (0x64 | CR_MAC_BASE)// --- TOS 3 Configuration Register ---
#define CR_MAC_TCP_CFG       (0x68 | CR_MAC_BASE)// --- TCP Configuration Register ---
#define CR_MAC_SWTAG_CFG     (0x6c | CR_MAC_BASE)// --- Software Tagging Configuration Register ---
#define CR_MAC_PMBL_CYC_NUM  (0x70 | CR_MAC_BASE)// --- Preamble Cycle Number Register ---
#define CR_MAC_FCTL_CYC_NUM  (0x74 | CR_MAC_BASE)// --- Flow Control Cycle Number Register ---
#define CR_MAC_JAM_CYC_NUM   (0x78 | CR_MAC_BASE)// --- JAM Cycle Number Register ---
#define CR_MAC_DEFER_VAL     (0x7c | CR_MAC_BASE)// --- Defer Value Register ---
#define CR_MAC_RANDOM_POLY   (0x80 | CR_MAC_BASE)// --- Random Polynomial Register ---
#define CR_MAC_MACCR    	(0x88 | CR_MAC_BASE)// --- MAC Control Register ---
#define CR_MAC_MACSR    	(0x8c | CR_MAC_BASE)// --- MAC Status Register ---
#define CR_MAC_PHYCR    	(0x90 | CR_MAC_BASE)// --- PHY Control Register ---
#define CR_MAC_PHYWDATA 	(0x94 | CR_MAC_BASE)// --- PHY Write Data Register ---
#define CR_MAC_FCR      	(0x98 | CR_MAC_BASE)// --- Flow Control Register ---
#define CR_MAC_BPR      	(0x9c | CR_MAC_BASE)// --- Back Pressure Register ---
#define CR_MAC_DESP_IDX        (0xc4 | CR_MAC_BASE)// --- Current Tx/Rx Descriptor Index ---
#define CR_MAC_WOLCR    	(0xa0 | CR_MAC_BASE)// --- Wake-On-LAN Control Register ---
#define CR_MAC_WOLSR    	(0xa4 | CR_MAC_BASE)// --- Wake-On-LAN Status Register ---
#define CR_MAC_WFCRC    	(0xa8 | CR_MAC_BASE)// --- Wake-up Frame CRC Register ---
#define CR_MAC_WFBM1    	(0xb0 | CR_MAC_BASE)// --- Wake-up Frame Byte Mask 1st Double Word Register ---
#define CR_MAC_WFBM2    	(0xb4 | CR_MAC_BASE)// --- Wake-up Frame Byte Mask 2nd Double Word Register ---
#define CR_MAC_WFBM3    	(0xb8 | CR_MAC_BASE)// --- Wake-up Frame Byte Mask 3rd Double Word Register ---
#define CR_MAC_WFBM4    	(0xbc | CR_MAC_BASE)// --- Wake-up Frame Byte Mask 4th Double Word Register ---
#define CR_MAC_DMA_FSM  	(0xc8 | CR_MAC_BASE)// --- DMA State Machine
#define CR_MAC_TM       	(0xcc | CR_MAC_BASE)// --- Test Mode Register ---
#define CR_MAC_XMPG_CNT 	(0xdc | CR_MAC_BASE)// --- XM and PG Counter Register ---
#define CR_MAC_RUNT_TLCC_CNT   (0xe0 | CR_MAC_BASE)// --- Receive Runt and Transmit Late Collision Packet Counter Register ---
#define CR_MAC_RCRC_RLONG_CNT  (0xe4 | CR_MAC_BASE)// --- Receive CRC Error and Long Packet Counter Register ---
#define CR_MAC_RLOSS_RCOL_CNT  (0xe8 | CR_MAC_BASE)// --- Receive Packet Loss and Receive Collision Counter Register ---
#define CR_MAC_BROADCAST_CNT  	(0xec | CR_MAC_BASE)// --- Receive Broadcast Counter Register ---
#define CR_MAC_MULTICAST_CNT  	(0xf0 | CR_MAC_BASE)// --- Receive Multicast Counter Register ---
#define CR_MAC_RX_CNT   	(0xf4 | CR_MAC_BASE)// --- Receive Good Packet Counter Register ---
#define CR_MAC_TX_CNT   	(0xf8 | CR_MAC_BASE)// --- Transmit Good Packet Counter Register ---

/*************************
 * UART Module Registers *
 *************************/
#ifdef __BIG_ENDIAN
#define CR_UART_OFFSET		(0x03)
#else
#define CR_UART_OFFSET		(0x0)
#endif

#define	CR_UART_BASE    	0xBFBF0000
#define	CR_UART_RBR     	(0x00+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_THR     	(0x00+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_IER     	(0x04+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_IIR     	(0x08+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_FCR     	(0x08+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_LCR     	(0x0c+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_MCR     	(0x10+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_LSR     	(0x14+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_MSR     	(0x18+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_SCR     	(0x1c+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_BRDL    	(0x00+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_BRDH    	(0x04+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_WORDA		(0x20+CR_UART_BASE+0x00)
#define	CR_UART_HWORDA		(0x28+CR_UART_BASE+0x00)
#define	CR_UART_MISCC		(0x24+CR_UART_BASE+CR_UART_OFFSET)
#define	CR_UART_XYD     	(0x2c+CR_UART_BASE)

#define	UART_BRD_ACCESS		0x80
#define	UART_XYD_Y		65000
#define	UART_UCLK_115200	0
#define	UART_UCLK_57600		1
#define	UART_UCLK_38400		2
#define	UART_UCLK_28800		3
#define	UART_UCLK_19200		4
#define	UART_UCLK_14400		5
#define	UART_UCLK_9600		6
#define	UART_UCLK_4800		7
#define	UART_UCLK_2400		8
#define	UART_UCLK_1200		9
#define	UART_UCLK_600		10
#define	UART_UCLK_300		11
#define	UART_UCLK_110		12
#define	UART_BRDL		0x03
#define	UART_BRDH		0x00
#define	UART_BRDL_20M		0x01
#define	UART_BRDH_20M		0x00
#define	UART_LCR		0x03
#define	UART_FCR		0x0f
#define	UART_WATERMARK		(0x0<<6)
#define	UART_MCR		0x0
#define	UART_MISCC		0x0
#define	UART_IER		0x01

#define	IER_RECEIVED_DATA_INTERRUPT_ENABLE	0x01
#define	IER_THRE_INTERRUPT_ENABLE			0x02
#define	IER_LINE_STATUS_INTERRUPT_ENABLE	0x04

#define	IIR_INDICATOR						VPchar(CR_UART_IIR)
#define	IIR_RECEIVED_LINE_STATUS			0x06
#define	IIR_RECEIVED_DATA_AVAILABLE			0x04
#define IIR_RECEIVER_IDLE_TRIGGER			0x0C
#define	IIR_TRANSMITTED_REGISTER_EMPTY		0x02
#define	LSR_INDICATOR						VPchar(CR_UART_LSR)
#define	LSR_RECEIVED_DATA_READY				0x01
#define	LSR_OVERRUN							0x02
#define	LSR_PARITY_ERROR					0x04
#define	LSR_FRAME_ERROR						0x08
#define	LSR_BREAK							0x10
#define	LSR_THRE							0x20
#define	LSR_THE								0x40
#define	LSR_RFIFO_FLAG						0x80

#define uartTxIntOn()		VPchar(CR_UART_IER) |= IER_THRE_INTERRUPT_ENABLE
#define uartTxIntOff()		VPchar(CR_UART_IER) &= ~IER_THRE_INTERRUPT_ENABLE
#define uartRxIntOn()		VPchar(CR_UART_IER) |= IER_RECEIVED_DATA_INTERRUPT_ENABLE
#define	uartRxIntOff()		VPchar(CR_UART_IER) &= ~IER_RECEIVED_DATA_INTERRUPT_ENABLE

/*************************
 * UART2 Module Registers *
 *************************/
#if defined(CONFIG_ECONET_EN7516) || \
    defined(CONFIG_ECONET_EN7527) || \
    defined(CONFIG_ECONET_EN7528)
#define	CR_UART3_BASE		0xBFBE1000
#endif
#define	CR_UART2_BASE		0xBFBF0300
#define	CR_UART2_RBR		(0x00+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_THR		(0x00+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_IER		(0x04+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_IIR		(0x08+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_FCR		(0x08+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_LCR		(0x0c+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_MCR		(0x10+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_LSR		(0x14+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_MSR		(0x18+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_SCR		(0x1c+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_BRDL		(0x00+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_BRDH		(0x04+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_WORDA		(0x20+CR_UART2_BASE+0x00)
#define	CR_UART2_HWORDA		(0x28+CR_UART2_BASE+0x00)
#define	CR_UART2_MISCC		(0x24+CR_UART2_BASE+CR_UART_OFFSET)
#define	CR_UART2_XYD		(0x2c+CR_UART2_BASE)

#define	IIR_INDICATOR2		VPchar(CR_UART2_IIR)
#define	LSR_INDICATOR2		VPchar(CR_UART2_LSR)

/*************************
 * HSUART Module Registers *
 *************************/
#define	CR_HSUART_BASE    	0xBFBF0300
#define	CR_HSUART_RBR     	(0x00+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_THR     	(0x00+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_IER     	(0x04+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_IIR     	(0x08+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_FCR     	(0x08+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_LCR     	(0x0c+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_MCR     	(0x10+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_LSR     	(0x14+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_MSR     	(0x18+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_SCR     	(0x1c+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_BRDL    	(0x00+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_BRDH    	(0x04+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_WORDA		(0x20+CR_HSUART_BASE+0x00)
#define	CR_HSUART_HWORDA	(0x28+CR_HSUART_BASE+0x00)
#define	CR_HSUART_MISCC		(0x24+CR_HSUART_BASE+CR_UART_OFFSET)
#define	CR_HSUART_XYD     	(0x2c+CR_HSUART_BASE)

/**********************************
 * Interrupt Controller Registers *
 **********************************/
#define CR_INTC_BASE    0xBFB40000
			// --- Interrupt Type Register ---
#define CR_INTC_ITR     (CR_INTC_BASE+0x0000)
			// --- Interrupt Mask Register ---
#define CR_INTC_IMR     (CR_INTC_BASE+0x0004)
			// --- Interrupt Pending Register ---
#define CR_INTC_IPR     (CR_INTC_BASE+0x0008)
			// --- Interrupt Set Register ---
#define CR_INTC_ISR    	(CR_INTC_BASE+0x000c)
			// --- Interrupt Priority Register 0 ---
#define CR_INTC_IPR0    (CR_INTC_BASE+0x0010)
			// --- Interrupt Priority Register 1 ---
#define CR_INTC_IPR1    (CR_INTC_BASE+0x0014)
			// --- Interrupt Priority Register 2 ---
#define CR_INTC_IPR2    (CR_INTC_BASE+0x0018)
			// --- Interrupt Priority Register 3 ---
#define CR_INTC_IPR3    (CR_INTC_BASE+0x001c)
			// --- Interrupt Priority Register 4 ---
#define CR_INTC_IPR4    (CR_INTC_BASE+0x0020)
			// --- Interrupt Priority Register 5 ---
#define CR_INTC_IPR5    (CR_INTC_BASE+0x0024)
			// --- Interrupt Priority Register 6 ---
#define CR_INTC_IPR6    (CR_INTC_BASE+0x0028)
			// --- Interrupt Priority Register 7 ---
#define CR_INTC_IPR7    (CR_INTC_BASE+0x002c)
			// --- Interrupt Vector egister ---

			// --- Interrupt VPE and SRS Register 0 ---
#define CR_INTC_IVSR0   (CR_INTC_BASE+0x0030)
			// --- Interrupt VPE and SRS Register 1 ---
#define CR_INTC_IVSR1   (CR_INTC_BASE+0x0034)
			// --- Interrupt VPE and SRS Register 2 ---
#define CR_INTC_IVSR2   (CR_INTC_BASE+0x0038)
			// --- Interrupt VPE and SRS Register 3 ---
#define CR_INTC_IVSR3   (CR_INTC_BASE+0x003c)
			// --- Interrupt VPE and SRS Register 4 ---
#define CR_INTC_IVSR4   (CR_INTC_BASE+0x0040)
			// --- Interrupt VPE and SRS Register 5 ---
#define CR_INTC_IVSR5   (CR_INTC_BASE+0x0044)
			// --- Interrupt VPE and SRS Register 6 ---
#define CR_INTC_IVSR6   (CR_INTC_BASE+0x0048)
			// --- Interrupt VPE and SRS Register 7 ---
#define CR_INTC_IVSR7   (CR_INTC_BASE+0x004c)
			// --- Interrupt Vector egister ---
#define CR_INTC_IVR     (CR_INTC_BASE+0x0050)

/* RT63165 */
			// --- Interrupt Mask Register ---
#define CR_INTC_IMR_1   (CR_INTC_BASE+0x0050)
			// --- Interrupt Pending Register ---
#define CR_INTC_IPR_1   (CR_INTC_BASE+0x0054)
			// --- Interrupt Priority Register 8 ---
#define CR_INTC_IPSR8	(CR_INTC_BASE+0x0058)
			// --- Interrupt Priority Register 9 ---
#define CR_INTC_IPSR9	(CR_INTC_BASE+0x005c)
			// --- Interrupt VPE and SRS Register 8 ---
#define CR_INTC_IVSR8   (CR_INTC_BASE+0x0060)
			// --- Interrupt VPE and SRS Register 9 ---
#define CR_INTC_IVSR9   (CR_INTC_BASE+0x0064)

enum
interrupt_priority
{
		IPL0,	IPL1,	IPL2,	IPL3,	IPL4,
		IPL5,	IPL6,	IPL7,	IPL8,	IPL9,
		IPL10,	IPL11,	IPL12,	IPL13,	IPL14,
		IPL15,	IPL16,	IPL17,	IPL18,	IPL19,
		IPL20,	IPL21,	IPL22,	IPL23,	IPL24,
		IPL25,	IPL26,	IPL27,	IPL28,	IPL29,
		IPL30,	IPL31
};

/**************************
 * Timer Module Registers *
 **************************/
#define CR_CPUTMR_BASE 		0xBFBF0400
#define CR_CPUTMR_CTL    	(CR_CPUTMR_BASE + 0x00)
#define CR_CPUTMR_CMR0    	(CR_CPUTMR_BASE + 0x04)
#define CR_CPUTMR_CNT0    	(CR_CPUTMR_BASE + 0x08)
#define CR_CPUTMR_CMR1    	(CR_CPUTMR_BASE + 0x0c)
#define CR_CPUTMR_CNT1    	(CR_CPUTMR_BASE + 0x10)

#ifdef CONFIG_MIPS_TC3262_1004K
#define CR_CPUTMR_23_BASE	0xBFBE0000
#define CR_CPUTMR_23_CTL	(CR_CPUTMR_23_BASE + 0x00)
#define CR_CPUTMR_CMR2		(CR_CPUTMR_23_BASE + 0x04)
#define CR_CPUTMR_CNT2		(CR_CPUTMR_23_BASE + 0x08)
#define CR_CPUTMR_CMR3		(CR_CPUTMR_23_BASE + 0x0c)
#define CR_CPUTMR_CNT3		(CR_CPUTMR_23_BASE + 0x10)
#endif

/*************************
 * GPIO Module Registers *
 *************************/
#define CR_GPIO_BASE       	0xBFBF0200
#define CR_GPIO_CTRL	    (CR_GPIO_BASE + 0x00)
#define CR_GPIO_DATA	    (CR_GPIO_BASE + 0x04)
#define CR_GPIO_INTS      	(CR_GPIO_BASE + 0x08)
#define CR_GPIO_EDET	    (CR_GPIO_BASE + 0x0C)
#define CR_GPIO_LDET       	(CR_GPIO_BASE + 0x10)
#define CR_GPIO_ODRAIN      (CR_GPIO_BASE + 0x14)
#define CR_GPIO_CTRL1	    (CR_GPIO_BASE + 0x20)

#define GPIO_IN				0x0
#define GPIO_OUT			0x1
#define GPIO_ALT_IN			0x2
#define GPIO_ALT_OUT		0x3

#define GPIO_E_DIS			0x0
#define GPIO_E_RISE			0x1
#define GPIO_E_FALL			0x2
#define GPIO_E_BOTH			0x3

#define GPIO_L_DIS			0x0
#define GPIO_L_HIGH			0x1
#define GPIO_L_LOW			0x2
#define GPIO_L_BOTH			0x3

/*****************************
 * Arbiter/Decoder Registers *
 *****************************/
#define CR_AHB_BASE       	0xBFB00000
#define CR_AHB_AACS	    	(CR_AHB_BASE + 0x00)
#define CR_AHB_ABEM      	(CR_AHB_BASE + 0x08)
#define CR_AHB_ABEA		    (CR_AHB_BASE + 0x0C)
#define CR_AHB_DMB0       	(CR_AHB_BASE + 0x10)
#define CR_AHB_DMB1       	(CR_AHB_BASE + 0x14)
#define CR_AHB_DMB2       	(CR_AHB_BASE + 0x18)
#define CR_AHB_DMB3       	(CR_AHB_BASE + 0x1C)
#define CR_AHB_SMB0       	(CR_AHB_BASE + 0x20)
#define CR_AHB_SMB1       	(CR_AHB_BASE + 0x24)
#define CR_AHB_SMB2       	(CR_AHB_BASE + 0x28)
#define CR_AHB_SMB3       	(CR_AHB_BASE + 0x2C)
#define CR_AHB_SMB4       	(CR_AHB_BASE + 0x30)
#define CR_AHB_SMB5       	(CR_AHB_BASE + 0x34)

/* RT63165 */
#define CR_ERR_ADDR    		(CR_AHB_BASE + 0x3c)
#define CR_PRATIR      		(CR_AHB_BASE + 0x58)
#define CR_MON_TMR     		(CR_AHB_BASE + 0x60)

#define CR_AHB_RSTCR		(CR_AHB_BASE + 0x40)

#define CR_AHB_PMCR       	(CR_AHB_BASE + 0x80)
#define CR_AHB_DMTCR       	(CR_AHB_BASE + 0x84)
#define CR_AHB_PCIC	       	(CR_AHB_BASE + 0x88)

#if defined(CONFIG_ECONET_EN7528)
#define BOOT_TRAP_CONF		(0xBFB000B8)
#endif

#if defined(CONFIG_ECONET_EN7516) || \
    defined(CONFIG_ECONET_EN7527) || \
    defined(CONFIG_ECONET_EN7528)
#define CR_AHB_HWCONF		(0xBFA20174)
#else
#define CR_AHB_HWCONF       (CR_AHB_BASE + 0x8C)
#endif
#define CR_AHB_SSR       	(CR_AHB_BASE + 0x90)
#define CR_AHB_SSTR			(CR_AHB_BASE + 0x9C)
#define CR_IMEM       	(CR_AHB_BASE + 0x9C)
#define CR_DMEM       	(CR_AHB_BASE + 0xA0)

/* RT63365 */
#define CR_CRCC_REG		(CR_AHB_BASE + 0xA0)
#define CR_AHB_UHCR		(CR_AHB_BASE + 0xA8)
#define CR_AHB_ABMR       	(CR_AHB_BASE + 0xB8)
#define CR_CKGEN_CONF		(CR_AHB_BASE + 0xC0)
#define CR_PSMCR       		(CR_AHB_BASE + 0xCC)
#define CR_PSMDR       		(CR_AHB_BASE + 0xD0)
#define CR_PSMMR       		(CR_AHB_BASE + 0xD0)

/* RT63165 */
#define CR_SRAM       		(CR_AHB_BASE + 0xF4)

/* RT63365 */
#define CR_AHB_CLK			(CR_AHB_BASE + 0x1c0)
#define CR_CLK_CFG     		(CR_AHB_BASE + 0x82c)
#define CR_RSTCTRL2    		(CR_AHB_BASE + 0x834)
#define CR_GPIO_SHR		(CR_AHB_BASE + 0x860)

#define CR_BUSTIMEOUT_SWITCH 	(CR_AHB_BASE + 0x92c)

/*************************************************
 * SRAM/FLASH/ROM Controller Operation Registers *
 *************************************************/
#define CR_SMC_BASE       	0xBFB10000
#define CR_SMC_BCR0	    	(CR_SMC_BASE + 0x00)
#define CR_SMC_BCR1	    	(CR_SMC_BASE + 0x04)
#define CR_SMC_BCR2	    	(CR_SMC_BASE + 0x08)
#define CR_SMC_BCR3	    	(CR_SMC_BASE + 0x0C)
#define CR_SMC_BCR4	    	(CR_SMC_BASE + 0x10)
#define CR_SMC_BCR5	    	(CR_SMC_BASE + 0x14)

/*****************************
 * Clock Generator Registers *
 *****************************/

/****************************
 * USB Module Registers *
 ****************************/

/*************************
 * HOST BRIDGE Registers *
 * ***********************/
#define HOST_BRIDGE_BASE 	0xBFB80000
#define CR_CFG_ADDR_REG 	(HOST_BRIDGE_BASE+0x0020)
#define CR_CFG_DATA_REG 	(HOST_BRIDGE_BASE+0x0024)
/****************************
 * ATM SAR Module Registers *
 ****************************/
#define TSCONTROL_BASE			0xBFB00000
#define TSARM_REGISTER_BASE		(TSCONTROL_BASE + 0x00060000)

/* ----- General configuration registers  ----- */

/* ----- Reset And Identify register  ----- */
#define TSARM_RAI				VPint(TSARM_REGISTER_BASE + 0x0000)
/* ----- General Configuration register  ----- */
#define TSARM_GFR				VPint(TSARM_REGISTER_BASE + 0x0004)
/* ----- Traffic Scheduler Timer Base Counter register  ----- */
#define TSARM_TSTBR				VPint(TSARM_REGISTER_BASE + 0x0008)
/* ----- Receive Maximum Packet Length register  ----- */
#define TSARM_RMPLR				VPint(TSARM_REGISTER_BASE + 0x000c)
/* ----- Transmit Priority 0/1 Data Buffer Control and Status Register  ----- */
#define TSARM_TXDBCSR_P01		VPint(TSARM_REGISTER_BASE + 0x0010)
/* ----- TX OAM Buffer Control and Status register  ----- */
#define TSARM_TXMBCSR			VPint(TSARM_REGISTER_BASE + 0x0014)
/* ----- RX Data Buffer Control and Status register  ----- */
#define TSARM_RXDBCSR			VPint(TSARM_REGISTER_BASE + 0x0018)
/* ----- RX OAM Buffer Control and Status register  ----- */
#define TSARM_RXMBCSR			VPint(TSARM_REGISTER_BASE + 0x001c)
/* ----- Last IRQ Status register  ----- */
#define TSARM_LIRQ				VPint(TSARM_REGISTER_BASE + 0x0020)
/* ----- IRQ Queue Base Address register  ----- */
#define TSARM_IRQBA				VPint(TSARM_REGISTER_BASE + 0x0024)
/* ----- IRQ Queue Entry Length register  ----- */
#define TSARM_IRQLEN			VPint(TSARM_REGISTER_BASE + 0x0028)
/* ----- IRQ Head Indication register  ----- */
#define TSARM_IRQH				VPint(TSARM_REGISTER_BASE + 0x002c)
/* ----- Clear IRQ Entry register  ----- */
#define TSARM_IRQC				VPint(TSARM_REGISTER_BASE + 0x0030)
//Traffic Scheduler Line Rate Counter Register
#define TSARM_TXSLRC			VPint(TSARM_REGISTER_BASE + 0x0034)
//Transmit Priority 2/3 Data Buffer Control and Status Register
#define TSARM_TXDBCSR_P23		VPint(TSARM_REGISTER_BASE + 0x0038)

/* ----- VC IRQ Mask register  ----- */
#define TSARM_IRQM_BASE			(TSARM_REGISTER_BASE + 0x0040)
#define TSARM_IRQM(vc)			VPint(TSARM_IRQM_BASE + (vc * 4))
#define TSARM_IRQMCC			VPint(TSARM_IRQM_BASE + 0x0040)
#define TSARM_IRQ_QUE_THRE		VPint(TSARM_REGISTER_BASE + 0x0084)		//IRQ Queue Threshold Register
#define TSARM_IRQ_TIMEOUT_CTRL 	VPint(TSARM_REGISTER_BASE + 0x0088)		//IRQ Timeout Control Register

/* ----- VC Configuration register  ----- */
#define TSARM_VCCR_BASE			(TSARM_REGISTER_BASE + 0x0100)
#define TSARM_VCCR(vc)			VPint(TSARM_VCCR_BASE + (vc * 4))
#define TSARM_CCCR				VPint(TSARM_VCCR_BASE + 0x0040)

/* ----- DMA WRR Configuration Register (DMA_WRR_WEIT) (for TC3162L4) ----- */
#define TSARM_DMAWRRCR			VPint(TSARM_REGISTER_BASE + 0x0150)

/* ----- Transmit Buffer Descriptor register  ----- */
#define TSARM_TXDCBDA_BASE		(TSARM_REGISTER_BASE + 0x0200)
#define TSARM_TXDCBDA(vc)		VPint(TSARM_TXDCBDA_BASE + (vc * 4))
#define TSARM_TXMCBDA_BASE		(TSARM_REGISTER_BASE + 0x0240)
#define TSARM_TXMCBDA(vc)		VPint(TSARM_TXMCBDA_BASE + (vc * 4))

#define TSARM_CC_TX_BD_BASE				VPint(TSARM_REGISTER_BASE + 0x0228)		//Control Channel Transmit BD Base Address 0x228
#define TSARM_CC_TX_BD_MNG_BASE			VPint(TSARM_REGISTER_BASE + 0x0268)		//Control Channel Transmit BD Management Base
#define TSARM_VC_TX_BD_PRIORITY01_BASE		(TSARM_REGISTER_BASE + 0x0280)
#define TSARM_VC_TX_BD_PRIORITY01(vc)		VPint(TSARM_VC_TX_BD_PRIORITY01_BASE + vc * 4)		//VC0 Transmit BD Data Priority 0/1 Base 280
#define TSARM_VC_TX_BD_PRIORITY23_BASE		(TSARM_REGISTER_BASE + 0x02c0)
#define TSARM_VC_TX_BD_PRIORITY23(vc)		VPint(TSARM_VC_TX_BD_PRIORITY23_BASE + vc * 4)		//VC0 Transmit BD Data Priority 0/1 Base 280

/* ----- Receive Buffer Descriptor register  ----- */
#define TSARM_RXDCBDA_BASE		(TSARM_REGISTER_BASE + 0x0300)
#define TSARM_RXDCBDA(vc)		VPint(TSARM_RXDCBDA_BASE + (vc * 4))
#define TSARM_RXMCBDA_BASE		(TSARM_REGISTER_BASE + 0x0340)
#define TSARM_RXMCBDA(vc)		VPint(TSARM_RXMCBDA_BASE + (vc * 4))

#define TSARM_CC_RX_BD_BASE			VPint(TSARM_REGISTER_BASE + 0x328)		//Control Channel Receive BD Base Address	0x328
#define TSARM_CC_RX_BD_MNG_BASE		VPint(TSARM_REGISTER_BASE + 0x368)		//Control Channel Receive BD Management Base	0x368
#define TSARM_VC_RX_DATA_BASE				(TSARM_REGISTER_BASE + 0x380)
#define TSARM_VC_RX_DATA(vc)			VPint(TSARM_VC_RX_DATA_BASE + vc * 4)	//VC0 Receive BD Data Base	0x380

/* ----- Traffic Scheduler register  ----- */
#define TSARM_PCR_BASE			(TSARM_REGISTER_BASE + 0x0400)
#define TSARM_PCR(vc)			VPint(TSARM_PCR_BASE + (vc * 4))
#define TSARM_SCR_BASE			(TSARM_REGISTER_BASE + 0x0440)
#define TSARM_SCR(vc)			VPint(TSARM_SCR_BASE + (vc * 4))
#define TSARM_MBSTP_BASE		(TSARM_REGISTER_BASE + 0x0480)
#define TSARM_MBSTP(vc)			VPint(TSARM_MBSTP_BASE + (vc * 4))

#define TSARM_MAX_FRAME_SIZE_BASE	(TSARM_REGISTER_BASE + 0x04c0)
#define TSARM_MAX_FRAME_SIZE(vc)		VPint(TSARM_MAX_FRAME_SIZE_BASE + (vc * 4))
/* define for TC3162L4 */
#define TSARM_TRAFFIC_SHAPER_WEIGHT_BASE (TSARM_REGISTER_BASE + 0x0500)
#define TSARM_TRAFFIC_SHAPER_WEIGHT(vc)     VPint(TSARM_TRAFFIC_SHAPER_WEIGHT_BASE + (vc * 4))

/* ----- TX Statistic Counter register  ----- */
#define TSARM_TDCNT_BASE		(TSARM_REGISTER_BASE + 0x0600)
#define TSARM_TDCNT(vc)			VPint(TSARM_TDCNT_BASE + (vc * 4))
#define TSARM_TDCNTCC			VPint(TSARM_TDCNT_BASE + 0x0040)

/* ----- RX Statistic Counter register  ----- */
#define TSARM_RDCNT_BASE		(TSARM_REGISTER_BASE + 0x0700)
#define TSARM_RDCNT(vc)			VPint(TSARM_RDCNT_BASE + (vc * 4))
#define TSARM_RDCNTCC			VPint(TSARM_RDCNT_BASE + 0x0040)
#define TSARM_MISCNT			VPint(TSARM_RDCNT_BASE + 0x0044)

#define TSARM_MPOA_GCR				VPint(TSARM_REGISTER_BASE + 0x0800)			//MPOA global control register
#define TSARM_VC_MPOA_CTRL_BASE			(TSARM_REGISTER_BASE + 0x0810)			//VC0 ~9  MPOA Control register
#define TSARM_VC_MPOA_CTRL(vc)			VPint(TSARM_VC_MPOA_CTRL_BASE + vc * 4)
#define TSARM_MPOA_HFIV11				VPint(TSARM_REGISTER_BASE + 0x0850)			//MPOA header Field1 Insertion Value1
#define TSARM_MPOA_HFIV12				VPint(TSARM_REGISTER_BASE + 0x0854)			//MPOA header Field1 Insertion Value2
#define TSARM_MPOA_HFIV13				VPint(TSARM_REGISTER_BASE + 0x0858)			//MPOA header Field2 Insertion Value1
#define TSARM_MPOA_HFIV21				VPint(TSARM_REGISTER_BASE + 0x0860)			//MPOA header Field2 Insertion Value2
#define TSARM_MPOA_HFIV22				VPint(TSARM_REGISTER_BASE + 0x0864)			//MPOA header Field2 Insertion Value2
#define TSARM_MPOA_HFIV23				VPint(TSARM_REGISTER_BASE + 0x0868)			//MPOA header Field2 Insertion Value2
#define TSARM_MPOA_HFIV31				VPint(TSARM_REGISTER_BASE + 0x0870)			//MPOA header Field3 Insertion Value1
#define TSARM_MPOA_HFIV32				VPint(TSARM_REGISTER_BASE + 0x0874)			//MPOA header Field3 Insertion Value2
#define TSARM_MPOA_HFIV33				VPint(TSARM_REGISTER_BASE + 0x0878)			//MPOA header Field3 Insertion Value3
#define TSARM_MPOA_HFIV41				VPint(TSARM_REGISTER_BASE + 0x0880)			//MPOA header Field4 Insertion Value1
#define TSARM_MPOA_HFIV42				VPint(TSARM_REGISTER_BASE + 0x0884)			//MPOA header Field4 Insertion Value2
#define TSARM_MPOA_HFIV43				VPint(TSARM_REGISTER_BASE + 0x0888)			//MPOA header Field4 Insertion Value2

/**************************
 * USB Module Registers *
 **************************/

#define LA_DEBUG_TRIGGER(addr,val) VPint(0xbfc00000+addr) = val

#endif /* _TC3162_H_ */
