#ifndef _RA_FOE_RES_WANTED
#define _RA_FOE_RES_WANTED

#if defined(CONFIG_MACH_MT7622) || \
    defined(CONFIG_MACH_MT7981) || \
    defined(CONFIG_MACH_MT7986) || \
    defined(CONFIG_MACH_MT7988)
#define FOE_HAS_MIB_TABLE
#endif

#if defined(CONFIG_MACH_MT7622) || \
    defined(CONFIG_MACH_MT7623) || \
    defined(CONFIG_MACH_MT7629) || \
    defined(CONFIG_MACH_MT7981) || \
    defined(CONFIG_MACH_MT7986) || \
    defined(CONFIG_MACH_MT7988)
#define FOE_HAS_PSP_TABLE
#endif

#if defined(CONFIG_MACH_MT7981) || \
    defined(CONFIG_MACH_MT7986)
#define FOE_ENTRY_SIZE_IP4	96
#define FOE_ENTRY_SIZE_IP6	96
#elif defined(CONFIG_MACH_MT7988)
#define FOE_ENTRY_SIZE_IP4	128
#define FOE_ENTRY_SIZE_IP6	128
#elif defined(CONFIG_MACH_AN7552) || \
      defined(CONFIG_MACH_AN7581) || \
      defined(CONFIG_MACH_AN7583)
#define FOE_ENTRY_SIZE_IP4	80
#define FOE_ENTRY_SIZE_IP6	80
#else
#define FOE_ENTRY_SIZE_IP4	64
#define FOE_ENTRY_SIZE_IP6	80
#endif

#if defined(CONFIG_RA_HW_NAT_TBL_OFF)
#define FOE_ENTRY_COUNT		0
#elif defined(CONFIG_RA_HW_NAT_TBL_1K)
#define FOE_ENTRY_COUNT		1024
#elif defined(CONFIG_RA_HW_NAT_TBL_2K)
#define FOE_ENTRY_COUNT		2048
#elif defined(CONFIG_RA_HW_NAT_TBL_4K)
#define FOE_ENTRY_COUNT		4096
#elif defined(CONFIG_RA_HW_NAT_TBL_8K)
#define FOE_ENTRY_COUNT		8192
#elif defined(CONFIG_RA_HW_NAT_TBL_16K)
#define FOE_ENTRY_COUNT		16384
#elif defined(CONFIG_RA_HW_NAT_TBL_32K)
#define FOE_ENTRY_COUNT		32768
#elif defined(CONFIG_RA_HW_NAT_TBL_64K)
#define FOE_ENTRY_COUNT		65536
#else
#error "FoE table size not defined"
#endif

#if defined(CONFIG_RA_HW_NAT_IPV6)
#define FOE_ENTRY_SIZE		FOE_ENTRY_SIZE_IP6
#else
#define FOE_ENTRY_SIZE		FOE_ENTRY_SIZE_IP4
#endif

#if defined(CONFIG_RA_HW_NAT_TRIPLE_PPE)
#define MAX_PPE_NUM		3
#elif defined(CONFIG_RA_HW_NAT_DUAL_PPE)
#define MAX_PPE_NUM		2
#else
#define MAX_PPE_NUM		1
#endif

struct ppe_tbl {
	void *virt;
	dma_addr_t phys;
	u32 size;
};

#if defined(CONFIG_ARCH_AIROHA)

#if defined(CONFIG_MACH_AN7552)
#define FOE_ENTRY_NUM_SRAM	512
#elif defined(CONFIG_MACH_AN7581)
#define FOE_ENTRY_NUM_SRAM	8192	/* 8192 per PPE */
#elif defined(CONFIG_MACH_AN7583)
#define FOE_ENTRY_NUM_SRAM	8192
#endif

#if FOE_ENTRY_COUNT > 32768 && MAX_PPE_NUM > 1
#error "Dual PPE mode support max 32K FoE entries in DRAM"
#endif

#if defined(CONFIG_RA_HW_NAT_TBL_SRAM_OFF)
#define FOE_ENTRY_NUM_SRAM	0
#endif

#if FOE_ENTRY_COUNT == 65536
#define FOE_ENTRY_NUM_DRAM	(FOE_ENTRY_COUNT - FOE_ENTRY_NUM_SRAM)
#else
#define FOE_ENTRY_NUM_DRAM	 FOE_ENTRY_COUNT
#endif

#define FOE_ENTRY_TOTAL		(FOE_ENTRY_NUM_SRAM + FOE_ENTRY_NUM_DRAM)

#define FOE_TABLE_SIZE		(FOE_ENTRY_NUM_DRAM * FOE_ENTRY_SIZE)

#define PSE_SPORT_NUM		32	/* SPORT max 5 bits */
#define PSE_FPORT_NUM		10

struct ra_foe_resources {
	void __iomem *fe_base;
	void __iomem *qdma_base;
	struct ppe_tbl foe;
	u8 ppe_sp_map[PSE_SPORT_NUM];
	u8 cdm_sp_map[PSE_SPORT_NUM];
	u8 ppe_fp_map[PSE_FPORT_NUM];
	u8 cdm_fp_map[PSE_FPORT_NUM];
	u8 lro_rings;
};

#else

#define MIB_ENTRY_SIZE		16
#define PSP_ENTRY_SIZE		16

#define FOE_TABLE_SIZE		(FOE_ENTRY_COUNT * FOE_ENTRY_SIZE)
#define MIB_TABLE_SIZE		(FOE_ENTRY_COUNT * MIB_ENTRY_SIZE)
#define PSP_TABLE_SIZE		(FOE_ENTRY_COUNT * PSP_ENTRY_SIZE)

#define PSE_PORTS_NUM		16	/* SPORT max 4 bits */

struct ra_foe_resources {
#ifdef CONFIG_ARCH_MEDIATEK
	void __iomem *ethdma_sys_base;
	void __iomem *ethdma_fe_base;
#endif
	struct ppe_tbl foe[MAX_PPE_NUM];
#ifdef FOE_HAS_MIB_TABLE
	struct ppe_tbl mib[MAX_PPE_NUM];
#endif
#ifdef FOE_HAS_PSP_TABLE
	struct ppe_tbl psp[MAX_PPE_NUM];
#endif
	u8 ppe_map[PSE_PORTS_NUM];
	u8 lro_rings;
};

#endif

int get_foe_resources(void *foe_res_ptr, size_t foe_res_len);

#endif
