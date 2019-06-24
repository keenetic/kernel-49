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
#ifndef __ASM_MACH_MIPS_TC3262_SURFBOARDINT_H
#define __ASM_MACH_MIPS_TC3262_SURFBOARDINT_H

#include "tc3262_int_source.h"

#if defined(CONFIG_MIPS_TC3262_1004K)

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

#endif

extern void tc_enable_irq(unsigned int irq);
extern void tc_disable_irq(unsigned int irq);

#endif
