/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 1999, 2000, 03, 04 Ralf Baechle
 * Copyright (C) 2000, 2002  Maciej W. Rozycki
 * Copyright (C) 1990, 1999, 2000 Silicon Graphics, Inc.
 */
#ifndef __ASM_MACH_MIPS_TC3262_SPACES_H
#define __ASM_MACH_MIPS_TC3262_SPACES_H

#if defined(CONFIG_ECONET_EN7512)
#define EN75XX_HIGHMEM_START	0x40000000
#else
#define EN75XX_HIGHMEM_START	0x9c000000
#endif

#define HIGHMEM_START		_AC(EN75XX_HIGHMEM_START, UL)

#include <asm/mach-generic/spaces.h>

#endif
