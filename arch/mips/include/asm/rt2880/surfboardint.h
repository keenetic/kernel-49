/*
 * Copyright (C) 2001 Palmchip Corporation.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * Defines for the Surfboard interrupt controller.
 *
 */
#ifndef __ASM_MACH_MIPS_RT2880_SURFBOARDINT_H
#define __ASM_MACH_MIPS_RT2880_SURFBOARDINT_H

#if defined(CONFIG_RALINK_MT7621)

#include <linux/irqchip/mips-gic.h>

/* MIPS GIC controller */
#define GIC_NUM_INTRS			64
#define MIPS_GIC_IRQ_BASE		(MIPS_CPU_IRQ_BASE + 8)

#define SURFBOARDINT_FE			(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(3))		/* FE */
#define SURFBOARDINT_PCIE0		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(4))		/* PCIE0 */
#define SURFBOARDINT_AUX_TIMER		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(5))		/* AUX timer (systick) */
#define SURFBOARDINT_SYSCTL		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(6))		/* SYSCTL */
#define SURFBOARDINT_I2C		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(8))		/* I2C */
#define SURFBOARDINT_DRAMC		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(9))		/* DRAMC */
#define SURFBOARDINT_PCM		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(10))	/* PCM */
#define SURFBOARDINT_HSGDMA		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(11))	/* HSGDMA */
#define SURFBOARDINT_GPIO		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(12))	/* GPIO */
#define SURFBOARDINT_DMA		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(13))	/* GDMA */
#define SURFBOARDINT_NAND		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(14))	/* NAND */
#define SURFBOARDINT_NAND_ECC		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(15))	/* NFI ECC */
#define SURFBOARDINT_I2S		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(16))	/* I2S */
#define SURFBOARDINT_SPI		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(17))	/* SPI */
#define SURFBOARDINT_SPDIF		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(18))	/* SPDIF */
#define SURFBOARDINT_CRYPTO		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(19))	/* CryptoEngine */
#define SURFBOARDINT_SDXC		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(20))	/* SDXC */
#define SURFBOARDINT_R2P		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(21))	/* Rbus to Pbus */
#define SURFBOARDINT_USB		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(22))	/* USB */
#define SURFBOARDINT_ESW		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(23))	/* Switch */
#define SURFBOARDINT_PCIE1		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(24))	/* PCIE1 */
#define SURFBOARDINT_PCIE2		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(25))	/* PCIE2 */
#define SURFBOARDINT_UART_LITE1		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(26))	/* UART Lite */
#define SURFBOARDINT_UART_LITE2		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(27))	/* UART Lite */
#define SURFBOARDINT_UART_LITE3		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(28))	/* UART Lite */
#define SURFBOARDINT_WDG		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(29))	/* WDG timer */
#define SURFBOARDINT_TIMER0		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(30))	/* Timer0 */
#define SURFBOARDINT_TIMER1		(MIPS_GIC_IRQ_BASE + GIC_SHARED_TO_HWIRQ(31))	/* Timer1 */
#define SURFBOARDINT_UART1		SURFBOARDINT_UART_LITE1
#define SURFBOARDINT_UART2		SURFBOARDINT_UART_LITE2

#define SURFBOARDINT_END		(MIPS_GIC_IRQ_BASE + GIC_NUM_LOCAL_INTRS + GIC_NUM_INTRS - 1)

#elif defined(CONFIG_RALINK_MT7628)

#define MIPS_INTC_CHAIN_HW0		(MIPS_CPU_IRQ_BASE + 2)		/* Chain IP2 */
#define MIPS_INTC_CHAIN_HW1		(MIPS_CPU_IRQ_BASE + 3)		/* Chain IP3 */
#define SURFBOARDINT_PCIE0		(MIPS_CPU_IRQ_BASE + 4)		/* PCIe Slot0 */
#define SURFBOARDINT_FE			(MIPS_CPU_IRQ_BASE + 5)		/* Frame Engine */
#define SURFBOARDINT_WLAN		(MIPS_CPU_IRQ_BASE + 6)		/* Wireless */
#define SURFBOARDINT_MIPS_TIMER		(MIPS_CPU_IRQ_BASE + 7)		/* MIPS Timer */

#define INTC_NUM_INTRS			32
#define MIPS_INTC_IRQ_BASE		(MIPS_CPU_IRQ_BASE + 8)

#define SURFBOARDINT_SYSCTL		(MIPS_INTC_IRQ_BASE + 0)	/* SYSCTL */
#define SURFBOARDINT_ILL_ACC		(MIPS_INTC_IRQ_BASE + 3)	/* Illegal access */
#define SURFBOARDINT_PCM		(MIPS_INTC_IRQ_BASE + 4)	/* PCM */
#define SURFBOARDINT_GPIO		(MIPS_INTC_IRQ_BASE + 6)	/* GPIO */
#define SURFBOARDINT_DMA		(MIPS_INTC_IRQ_BASE + 7)	/* DMA */
#define SURFBOARDINT_NAND		(MIPS_INTC_IRQ_BASE + 8)	/* NAND */
#define SURFBOARDINT_PCTRL		(MIPS_INTC_IRQ_BASE + 9)	/* Performance counter */
#define SURFBOARDINT_I2S		(MIPS_INTC_IRQ_BASE + 10)	/* I2S */
#define SURFBOARDINT_SPI		(MIPS_INTC_IRQ_BASE + 11)	/* SPI */

#define SURFBOARDINT_SDXC		(MIPS_INTC_IRQ_BASE + 14)	/* SDXC */
#define SURFBOARDINT_R2P		(MIPS_INTC_IRQ_BASE + 15)	/* Rbus to Pbus */

#define SURFBOARDINT_AESENGINE		(MIPS_INTC_IRQ_BASE + 13)	/* AES Engine */
#define SURFBOARDINT_UART_LITE1		(MIPS_INTC_IRQ_BASE + 20)	/* UART Lite 1 */
#define SURFBOARDINT_UART_LITE2		(MIPS_INTC_IRQ_BASE + 21)	/* UART Lite 2 */
#define SURFBOARDINT_UART_LITE3		(MIPS_INTC_IRQ_BASE + 22)	/* UART Lite 3 */
#define SURFBOARDINT_WDG		(MIPS_INTC_IRQ_BASE + 23)	/* WDG timer */
#define SURFBOARDINT_TIMER0		(MIPS_INTC_IRQ_BASE + 24)	/* Timer0 */
#define SURFBOARDINT_TIMER1		(MIPS_INTC_IRQ_BASE + 25)	/* Timer1 */
#define SURFBOARDINT_PWM		(MIPS_INTC_IRQ_BASE + 26)	/* PWM */
#define SURFBOARDINT_WWAKE		(MIPS_INTC_IRQ_BASE + 27)	/* WLAN Wake */
#define SURFBOARDINT_UART1		SURFBOARDINT_UART_LITE1
#define SURFBOARDINT_UART2		SURFBOARDINT_UART_LITE2

#define SURFBOARDINT_ESW		(MIPS_INTC_IRQ_BASE + 17)	/* Embedded Switch */
#define SURFBOARDINT_UHST		(MIPS_INTC_IRQ_BASE + 18)	/* USB Host */
#define SURFBOARDINT_UDEV		(MIPS_INTC_IRQ_BASE + 19)	/* USB Device */

#define SURFBOARDINT_END		(MIPS_INTC_IRQ_BASE + INTC_NUM_INTRS - 1)

#elif defined(CONFIG_MIPS_TC3262_1004K)

#include <asm/tc3162/tc3262_int_source.h>

/* MIPS GIC controller (via EIC) */
#define GIC_NUM_INTRS			64
#define MIPS_GIC_IRQ_BASE		(MIPS_CPU_IRQ_BASE + 0)

#define SURFBOARDINT_UART_LITE1		UART_INT	/* UART Lite1 */
#define SURFBOARDINT_TIMER0		TIMER0_INT,	/* Timer0 */
#define SURFBOARDINT_TIMER1		TIMER1_INT	/* Timer1 */
#define SURFBOARDINT_TIMER2		TIMER2_INT	/* Timer2 */
#define SURFBOARDINT_WDG		TIMER5_INT	/* WDG timer */
#define SURFBOARDINT_GPIO		GPIO_INT	/* GPIO */
#define SURFBOARDINT_PCM		PCM1_INT	/* PCM */
#define SURFBOARDINT_ESW		MAC1_INT	/* ESW */
#define SURFBOARDINT_UART_LITE2		UART2_INT	/* UART Lite2 */
#define SURFBOARDINT_USB		IRQ_RT3XXX_USB	/* USB */
#define SURFBOARDINT_DMT		DMT_INT		/* xDSL DMT */
#define SURFBOARDINT_QDMA1		QDMA_LAN0_INTR	/* QDMA1, INT0 */
#define SURFBOARDINT_QDMA2		QDMA_WAN0_INTR	/* QDMA2, INT0 */
#define SURFBOARDINT_PCIE0		PCIE_0_INT	/* PCIE0 */
#define SURFBOARDINT_PCIE1		PCIE_A_INT	/* PCIE1 */
#define SURFBOARDINT_CRYPTO		CRYPTO_INT	/* CryptoEngine */
#define SURFBOARDINT_QDMA1_INT1		QDMA_LAN1_INTR	/* QDMA1, INT1 */
#define SURFBOARDINT_QDMA1_INT2		QDMA_LAN2_INTR	/* QDMA1, INT2 */
#define SURFBOARDINT_QDMA1_INT3		QDMA_LAN3_INTR	/* QDMA1, INT3 */
#define SURFBOARDINT_QDMA2_INT1		QDMA_WAN1_INTR	/* QDMA2, INT1 */
#define SURFBOARDINT_QDMA2_INT2		QDMA_WAN2_INTR	/* QDMA2, INT2 */
#define SURFBOARDINT_QDMA2_INT3		QDMA_WAN3_INTR	/* QDMA2, INT3 */
#define SURFBOARDINT_UART1		SURFBOARDINT_UART_LITE1
#define SURFBOARDINT_UART2		SURFBOARDINT_UART_LITE2

#define SURFBOARDINT_END		(INTR_SOURCES_NUM - 1)

#elif defined(CONFIG_MIPS_TC3262_34K)

#include <asm/tc3162/tc3262_int_source.h>

#define SURFBOARDINT_UART1		UART_INT	/* UART1 */
#define SURFBOARDINT_GPIO		GPIO_INT	/* GPIO */
#define SURFBOARDINT_PCM		PCM1_INT	/* PCM1 */
#define SURFBOARDINT_DMA		APB_DMA0_INT	/* DMA */
#define SURFBOARDINT_ESW		ESW_INT		/* ESW */
#define SURFBOARDINT_USB		IRQ_RT3XXX_USB	/* USB */
#define SURFBOARDINT_FE			FE_MAC_INT	/* Frame Engine */
#define SURFBOARDINT_QDMA		SAR_INT		/* QDMA */
#define SURFBOARDINT_PCIE0		PCIE_0_INT	/* PCIe0 */
#define SURFBOARDINT_PCIE1		PCIE_A_INT	/* PCIe1 */
#define SURFBOARDINT_CRYPTO		CRYPTO_INT	/* CryptoEngine */

#define SURFBOARDINT_END		40	/* 1 offset + 40 TC */

#else
#error "Ralink chip undefined for INT map"
#endif

/*
 * Surfboard registers are memory mapped on 32-bit aligned boundaries and
 * only word access are allowed.
 */
#ifdef CONFIG_MIPS_TC3262
extern void tc_enable_irq(unsigned int irq);
extern void tc_disable_irq(unsigned int irq);
#else
#define RALINK_IRQ0STAT		(RALINK_INTCL_BASE + 0x9C) //IRQ_STAT
#define RALINK_IRQ1STAT		(RALINK_INTCL_BASE + 0xA0) //FIQ_STAT
#define RALINK_INTTYPE		(RALINK_INTCL_BASE + 0x6C) //FIQ_SEL
#define RALINK_INTRAW		(RALINK_INTCL_BASE + 0xA4) //INT_PURE
#define RALINK_INTENA		(RALINK_INTCL_BASE + 0x80) //IRQ_MASK_SET
#define RALINK_FIQENA		(RALINK_INTCL_BASE + 0x84) //FIQ_MASK_SET
#define RALINK_INTDIS		(RALINK_INTCL_BASE + 0x78) //IRQ_MASK_CLR
#define RALINK_FIQDIS		(RALINK_INTCL_BASE + 0x7C) //FIQ_MASK_CLR
#endif

/* bobtseng added ++, 2006.3.6. */
#define read_32bit_cp0_register(source)                         \
({ int __res;                                                   \
        __asm__ __volatile__(                                   \
        ".set\tpush\n\t"                                        \
        ".set\treorder\n\t"                                     \
        "mfc0\t%0,"STR(source)"\n\t"                            \
        ".set\tpop"                                             \
        : "=r" (__res));                                        \
        __res;})

#define write_32bit_cp0_register(register,value)                \
        __asm__ __volatile__(                                   \
        "mtc0\t%0,"STR(register)"\n\t"                          \
        "nop"                                                   \
        : : "r" (value));

/* bobtseng added --, 2006.3.6. */

#endif
