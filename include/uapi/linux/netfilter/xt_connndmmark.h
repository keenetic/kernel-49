#ifndef _XT_CONNNDMMARK_H
#define _XT_CONNNDMMARK_H

#include <linux/types.h>

/* Copyright (C) 2002,2004 MARA Systems AB <http://www.marasystems.com>
 * by Henrik Nordstrom <hno@marasystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

enum {
	XT_CONNNDMMARK_SET = 0,
	XT_CONNNDMMARK_RESTORE
};

struct xt_connndmmark_tginfo {
	__u8 ctmark, ctmask, nfmask;
	__u8 mode;
};

struct xt_connndmmark_mtinfo {
	__u8 mark, mask;
	__u8 invert;
};

#endif /*_XT_CONNNDMMARK_H*/
