#ifndef __DEV_WOE_H__
#define __DEV_WOE_H__

#define WHNAT_MTK_DRV_NAME		"whnat_dev"

#define WED_DEV_NUM			2
#define WDMA_IRQ_NUM			3

#if defined(CONFIG_ECONET_EN7528)
/* WDMA */
#define WDMA0_CFG_BASE			0x1fa00000
#define WDMA0_OFST0			0x62046220
#define WDMA0_OFST1			0x61006000

#define WDMA1_CFG_BASE			0x1fa00000
#define WDMA1_OFST0			0x66046620
#define WDMA1_OFST1			0x65006400

/* PCIe */
#define PCIE_BASE_ADDR0			0x1fb81000
#define PCIE_BASE_ADDR1			0x1fb83000

#define PCIE_SLOT_WED0			0
#define PCIE_SLOT_WED1			1

/* EcoNet doesn't support cr mirror HW */
#define CFG_CR_MIRROR_SUPPORT		0

/* Enable SER recovery */
#define WED_SER_RECOVERY_DEFAULT	TRUE

#else

#error "Unknown SoC for WoE support"

#endif

struct whnat_mtk_pdata {
	u32 wdma_base[WED_DEV_NUM];
	u32 wdma_irq[WED_DEV_NUM][WDMA_IRQ_NUM];
};

#endif /* __DEV_WOE_H__ */
