#ifndef _RA_FOE_RES_WANTED
#define _RA_FOE_RES_WANTED

#if defined(CONFIG_MACH_MT7622) || \
    defined(CONFIG_MACH_MT7981) || \
    defined(CONFIG_MACH_MT7986)
#define FOE_HAS_MIB_TABLE
#endif

#if defined(CONFIG_MACH_MT7622) || \
    defined(CONFIG_MACH_MT7623) || \
    defined(CONFIG_MACH_MT7629) || \
    defined(CONFIG_MACH_MT7981) || \
    defined(CONFIG_MACH_MT7986)
#define FOE_HAS_PSP_TABLE
#endif

#if defined(CONFIG_MACH_MT7981) || \
    defined(CONFIG_MACH_MT7986)
#define FOE_ENTRY_SIZE_IP4	96
#define FOE_ENTRY_SIZE_IP6	96
#else
#define FOE_ENTRY_SIZE_IP4	64
#define FOE_ENTRY_SIZE_IP6	80
#endif

#if defined(CONFIG_RA_HW_NAT_TBL_1K)
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
#else
#error "FoE table size not defined"
#endif

#if defined(CONFIG_RA_HW_NAT_IPV6)
#define FOE_ENTRY_SIZE		FOE_ENTRY_SIZE_IP6
#else
#define FOE_ENTRY_SIZE		FOE_ENTRY_SIZE_IP4
#endif

#define MIB_ENTRY_SIZE		16
#define PSP_ENTRY_SIZE		16

#define FOE_TABLE_SIZE		(FOE_ENTRY_COUNT * FOE_ENTRY_SIZE)
#define MIB_TABLE_SIZE		(FOE_ENTRY_COUNT * MIB_ENTRY_SIZE)
#define PSP_TABLE_SIZE		(FOE_ENTRY_COUNT * PSP_ENTRY_SIZE)

struct ra_foe_resources {
#if defined(CONFIG_ARCH_MEDIATEK)
	void __iomem *ethdma_sys_base;
	void __iomem *ethdma_fe_base;
#endif
	void *foe_virt;
	dma_addr_t foe_phys;
	u32 foe_size;
#ifdef FOE_HAS_MIB_TABLE
	void *mib_virt;
	dma_addr_t mib_phys;
	u32 mib_size;
#endif
#ifdef FOE_HAS_PSP_TABLE
	void *psp_virt;
	dma_addr_t psp_phys;
	u32 psp_size;
#endif
};

int get_foe_resources(void *foe_res_ptr, size_t foe_res_len);

#endif
