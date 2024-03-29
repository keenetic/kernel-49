#ifndef __RT_RDM_MT7622_H__
#define __RT_RDM_MT7622_H__

#define MEDIATEK_TOPRGU_BASE	0x10000000
#define MEDIATEK_TOPRGU_SIZE	0x00480000

#define MEDIATEK_PHRSYS_BASE	0x11000000
#define MEDIATEK_PHRSYS_SIZE	0x002B0000

#define MEDIATEK_WBSYS_BASE	0x18000000
#define MEDIATEK_WBSYS_SIZE	0x00100000

#define MEDIATEK_HIFSYS_BASE	0x1A000000
#define MEDIATEK_HIFSYS_SIZE	0x00250000

#define MEDIATEK_HIFAXI_BASE	0x1AF00000
#define MEDIATEK_HIFAXI_SIZE	0x00001100

#define MEDIATEK_ETHSYS_BASE	0x1B000000
#define MEDIATEK_ETHSYS_SIZE	0x00130000

#define RALINK_SYSCTL_BASE	MEDIATEK_ETHSYS_BASE
#define RALINK_11N_MAC_BASE	MEDIATEK_WBSYS_BASE

enum {
	TOPRGU,
	PHRSYS,
	WBSYS,
	HIFSYS,
	ETHSYS
};

#endif
