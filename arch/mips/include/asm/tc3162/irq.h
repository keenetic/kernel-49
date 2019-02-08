/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003 by Ralf Baechle
 */
#ifndef __ASM_MACH_MIPS_TC3262_IRQ_H
#define __ASM_MACH_MIPS_TC3262_IRQ_H

#define MIPS_CPU_IRQ_BASE	0

#include <asm/tc3162/surfboardint.h>

#define NR_IRQS		(SURFBOARDINT_END + 1)

#include_next <irq.h>

#endif
