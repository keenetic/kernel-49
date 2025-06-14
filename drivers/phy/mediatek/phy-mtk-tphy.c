/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <dt-bindings/phy/phy.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

/* version V1 sub-banks offset base address */
/* banks shared by multiple phys */
#define SSUSB_SIFSLV_V1_SPLLC		0x000	/* shared by u3 phys */
#define SSUSB_SIFSLV_V1_U2FREQ		0x100	/* shared by u2 phys */
#define SSUSB_SIFSLV_V1_CHIP		0x300	/* shared by u3 phys */
/* u2 phy bank */
#define SSUSB_SIFSLV_V1_U2PHY_COM	0x000
/* u3/pcie/sata phy banks */
#define SSUSB_SIFSLV_V1_U3PHYD		0x000
#define SSUSB_SIFSLV_V1_U3PHYA		0x200

/* version V2 sub-banks offset base address */
/* u2 phy banks */
#define SSUSB_SIFSLV_V2_MISC		0x000
#define SSUSB_SIFSLV_V2_U2FREQ		0x100
#define SSUSB_SIFSLV_V2_U2PHY_COM	0x300
/* u3/pcie/sata phy banks */
#define SSUSB_SIFSLV_V2_SPLLC		0x000
#define SSUSB_SIFSLV_V2_CHIP		0x100
#define SSUSB_SIFSLV_V2_U3PHYD		0x200
#define SSUSB_SIFSLV_V2_U3PHYA		0x400

/* version V4 sub-banks offset base address */
/* pcie phy banks */
#define SSUSB_SIFSLV_V4_SPLLC		0x000
#define SSUSB_SIFSLV_V4_CHIP		0x100
#define SSUSB_SIFSLV_V4_U3PHYD		0x900
#define SSUSB_SIFSLV_V4_U3PHYA		0xb00

#define SSUSB_LN1_OFFSET		0x10000

#define U3P_MISC_REG1		0x04
#define MR1_EFUSE_AUTO_LOAD_DIS		BIT(6)

#define U3P_USBPHYACR0		0x000
#define PA0_RG_U2PLL_FORCE_ON		BIT(15)
#define PA0_RG_USB20_INTR_EN		BIT(5)

#define U3P_USBPHYACR1		0x004
#define PA1_RG_INTR_CAL		GENMASK(23, 19)
#define PA1_RG_INTR_CAL_VAL(x)	((0x1f & (x)) << 19)

#define U3P_USBPHYACR2		0x008
#define PA2_RG_SIF_U2PLL_FORCE_EN	BIT(18)

#define U3P_USBPHYACR5		0x014
#define PA5_RG_U2_HSTX_SRCAL_EN	BIT(15)
#define PA5_RG_U2_HSTX_SRCTRL		GENMASK(14, 12)
#define PA5_RG_U2_HSTX_SRCTRL_VAL(x)	((0x7 & (x)) << 12)
#define PA5_RG_U2_HS_100U_U3_EN	BIT(11)

#define U3P_USBPHYACR6		0x018
#define PA6_RG_U2_BC11_SW_EN		BIT(23)
#define PA6_RG_U2_OTG_VBUSCMP_EN	BIT(20)
#define PA6_RG_U2_SQTH		GENMASK(3, 0)
#define PA6_RG_U2_SQTH_VAL(x)	(0xf & (x))

#define U3P_U2PHYACR4		0x020
#define P2C_RG_USB20_GPIO_CTL		BIT(9)
#define P2C_USB20_GPIO_MODE		BIT(8)
#define P2C_U2_GPIO_CTR_MSK	(P2C_RG_USB20_GPIO_CTL | P2C_USB20_GPIO_MODE)

#define U3D_U2PHYDCR0		0x060
#define P2C_RG_SIF_U2PLL_FORCE_ON	BIT(24)

#define U3P_U2PHYDTM0		0x068
#define P2C_FORCE_UART_EN		BIT(26)
#define P2C_FORCE_DATAIN		BIT(23)
#define P2C_FORCE_DM_PULLDOWN		BIT(21)
#define P2C_FORCE_DP_PULLDOWN		BIT(20)
#define P2C_FORCE_XCVRSEL		BIT(19)
#define P2C_FORCE_SUSPENDM		BIT(18)
#define P2C_FORCE_TERMSEL		BIT(17)
#define P2C_RG_DATAIN			GENMASK(13, 10)
#define P2C_RG_DATAIN_VAL(x)		((0xf & (x)) << 10)
#define P2C_RG_DMPULLDOWN		BIT(7)
#define P2C_RG_DPPULLDOWN		BIT(6)
#define P2C_RG_XCVRSEL			GENMASK(5, 4)
#define P2C_RG_XCVRSEL_VAL(x)		((0x3 & (x)) << 4)
#define P2C_RG_SUSPENDM			BIT(3)
#define P2C_RG_TERMSEL			BIT(2)
#define P2C_DTM0_PART_MASK \
		(P2C_FORCE_DATAIN | P2C_FORCE_DM_PULLDOWN | \
		P2C_FORCE_DP_PULLDOWN | P2C_FORCE_XCVRSEL | \
		P2C_FORCE_TERMSEL | P2C_RG_DMPULLDOWN | \
		P2C_RG_DPPULLDOWN | P2C_RG_TERMSEL)

#define U3P_U2PHYDTM1		0x06C
#define P2C_RG_UART_EN			BIT(16)
#define P2C_FORCE_IDDIG		BIT(9)
#define P2C_RG_VBUSVALID		BIT(5)
#define P2C_RG_SESSEND			BIT(4)
#define P2C_RG_AVALID			BIT(2)
#define P2C_RG_IDDIG			BIT(1)

#define U3P_U3_CHIP_GPIO_CTLD		0x0c
#define P3C_REG_IP_SW_RST		BIT(31)
#define P3C_MCU_BUS_CK_GATE_EN		BIT(30)
#define P3C_FORCE_IP_SW_RST		BIT(29)

#define U3P_U3_CHIP_GPIO_CTLE		0x10
#define P3C_RG_SWRST_U3_PHYD		BIT(25)
#define P3C_RG_SWRST_U3_PHYD_FORCE_EN	BIT(24)

#define U3P_U3_PHYA_REG0	0x000
#define P3A_RG_IEXT_INTR		GENMASK(15, 10)
#define P3A_RG_IEXT_INTR_VAL(x)		((0x3f & (x)) << 10)
#define P3A_RG_CLKDRV_OFF		GENMASK(3, 2)
#define P3A_RG_CLKDRV_OFF_VAL(x)	((0x3 & (x)) << 2)

#define U3P_U3_PHYA_REG1	0x004
#define P3A_RG_CLKDRV_AMP		GENMASK(31, 29)
#define P3A_RG_CLKDRV_AMP_VAL(x)	((0x7 & (x)) << 29)

#define U3P_U3_PHYA_REG6	0x018
#define P3A_RG_TX_EIDLE_CM		GENMASK(31, 28)
#define P3A_RG_TX_EIDLE_CM_VAL(x)	((0xf & (x)) << 28)

#define U3P_U3_PHYA_REG9	0x024
#define P3A_RG_RX_DAC_MUX		GENMASK(5, 1)
#define P3A_RG_RX_DAC_MUX_VAL(x)	((0x1f & (x)) << 1)

#define U3P_U3_PHYA_DA_REG0	0x100
#define P3A_RG_XTAL_EXT_PE2H		GENMASK(17, 16)
#define P3A_RG_XTAL_EXT_PE2H_VAL(x)	((0x3 & (x)) << 16)
#define P3A_RG_XTAL_EXT_PE1H		GENMASK(13, 12)
#define P3A_RG_XTAL_EXT_PE1H_VAL(x)	((0x3 & (x)) << 12)
#define P3A_RG_XTAL_EXT_EN_U3		GENMASK(11, 10)
#define P3A_RG_XTAL_EXT_EN_U3_VAL(x)	((0x3 & (x)) << 10)

#define U3P_U3_PHYA_DA_REG4	0x108
#define P3A_RG_PLL_DIVEN_PE2H		GENMASK(21, 19)
#define P3A_RG_PLL_BC_PE2H		GENMASK(7, 6)
#define P3A_RG_PLL_BC_PE2H_VAL(x)	((0x3 & (x)) << 6)

#define U3P_U3_PHYA_DA_REG5	0x10c
#define P3A_RG_PLL_BR_PE2H		GENMASK(29, 28)
#define P3A_RG_PLL_BR_PE2H_VAL(x)	((0x3 & (x)) << 28)
#define P3A_RG_PLL_IC_PE2H		GENMASK(15, 12)
#define P3A_RG_PLL_IC_PE2H_VAL(x)	((0xf & (x)) << 12)

#define U3P_U3_PHYA_DA_REG6	0x110
#define P3A_RG_PLL_IR_PE2H		GENMASK(19, 16)
#define P3A_RG_PLL_IR_PE2H_VAL(x)	((0xf & (x)) << 16)

#define U3P_U3_PHYA_DA_REG7	0x114
#define P3A_RG_PLL_BP_PE2H		GENMASK(19, 16)
#define P3A_RG_PLL_BP_PE2H_VAL(x)	((0xf & (x)) << 16)

#define U3P_U3_PHYA_DA_REG20	0x13c
#define P3A_RG_PLL_DELTA1_PE2H		GENMASK(31, 16)
#define P3A_RG_PLL_DELTA1_PE2H_VAL(x)	((0xffff & (x)) << 16)

#define U3P_U3_PHYA_DA_REG25	0x148
#define P3A_RG_PLL_DELTA_PE2H		GENMASK(15, 0)
#define P3A_RG_PLL_DELTA_PE2H_VAL(x)	(0xffff & (x))

#define U3P_U3_PHYD_LFPS1		0x00c
#define P3D_RG_FWAKE_TH		GENMASK(21, 16)
#define P3D_RG_FWAKE_TH_VAL(x)	((0x3f & (x)) << 16)

#define U3P_U3_PHYD_IMPCAL0		0x010
#define P3D_RG_FORCE_TX_IMPEL		BIT(31)
#define P3D_RG_TX_IMPEL			GENMASK(28, 24)
#define P3D_RG_TX_IMPEL_VAL(x)		((0x1f & (x)) << 24)

#define U3P_U3_PHYD_IMPCAL1		0x014
#define P3D_RG_FORCE_RX_IMPEL		BIT(31)
#define P3D_RG_RX_IMPEL			GENMASK(28, 24)
#define P3D_RG_RX_IMPEL_VAL(x)		((0x1f & (x)) << 24)

#define U3P_U3_PHYD_RSV			0x054
#define P3D_RG_EFUSE_AUTO_LOAD_DIS	BIT(12)

#define U3P_U3_PHYD_CDR1		0x05c
#define P3D_RG_CDR_BIR_LTD1		GENMASK(28, 24)
#define P3D_RG_CDR_BIR_LTD1_VAL(x)	((0x1f & (x)) << 24)
#define P3D_RG_CDR_BIR_LTD0		GENMASK(12, 8)
#define P3D_RG_CDR_BIR_LTD0_VAL(x)	((0x1f & (x)) << 8)

#define U3P_U3_PHYD_RXDET1		0x128
#define P3D_RG_RXDET_STB2_SET		GENMASK(17, 9)
#define P3D_RG_RXDET_STB2_SET_VAL(x)	((0x1ff & (x)) << 9)

#define U3P_U3_PHYD_RXDET2		0x12c
#define P3D_RG_RXDET_STB2_SET_P3	GENMASK(8, 0)
#define P3D_RG_RXDET_STB2_SET_P3_VAL(x)	(0x1ff & (x))

#define U3P_U3_PHYD_REG19		0x338
#define P3D_RG_PLL_SSC_DELTA1		GENMASK(15, 0)
#define P3D_RG_PLL_SSC_DELTA1_VAL(x)	(0xffff & (x))

#define U3P_U3_PHYD_REG21		0x340
#define P3D_RG_PLL_SSC_DELTA		GENMASK(31, 16)
#define P3D_RG_PLL_SSC_DELTA_VAL(x)	((0xffff & (x)) << 16)

#define U3P_SPLLC_XTALCTL3		0x018
#define XC3_RG_U3_XTAL_RX_PWD		BIT(9)
#define XC3_RG_U3_FRC_XTAL_RX_PWD	BIT(8)

#define U3P_U2FREQ_FMCR0	0x00
#define P2F_RG_MONCLK_SEL	GENMASK(27, 26)
#define P2F_RG_MONCLK_SEL_VAL(x)	((0x3 & (x)) << 26)
#define P2F_RG_FREQDET_EN	BIT(24)
#define P2F_RG_CYCLECNT		GENMASK(23, 0)
#define P2F_RG_CYCLECNT_VAL(x)	((P2F_RG_CYCLECNT) & (x))

#define U3P_U2FREQ_VALUE	0x0c

#define U3P_U2FREQ_FMMONR1	0x10
#define P2F_USB_FM_VALID	BIT(0)
#define P2F_RG_FRCK_EN		BIT(8)

#define U3P_REF_CLK		26	/* MHZ */
#define U3P_SLEW_RATE_COEF	28
#define U3P_SR_COEF_DIVISOR	1000
#define U3P_FM_DET_CYCLE_CNT	1024

/* SATA register setting */
#define PHYD_CTRL_SIGNAL_MODE4		0x1c
/* CDR Charge Pump P-path current adjustment */
#define RG_CDR_BICLTD1_GEN1_MSK		GENMASK(23, 20)
#define RG_CDR_BICLTD1_GEN1_VAL(x)	((0xf & (x)) << 20)
#define RG_CDR_BICLTD0_GEN1_MSK		GENMASK(11, 8)
#define RG_CDR_BICLTD0_GEN1_VAL(x)	((0xf & (x)) << 8)

#define PHYD_DESIGN_OPTION2		0x24
/* Symbol lock count selection */
#define RG_LOCK_CNT_SEL_MSK		GENMASK(5, 4)
#define RG_LOCK_CNT_SEL_VAL(x)		((0x3 & (x)) << 4)

#define PHYD_DESIGN_OPTION9	0x40
/* COMWAK GAP width window */
#define RG_TG_MAX_MSK		GENMASK(20, 16)
#define RG_TG_MAX_VAL(x)	((0x1f & (x)) << 16)
/* COMINIT GAP width window */
#define RG_T2_MAX_MSK		GENMASK(13, 8)
#define RG_T2_MAX_VAL(x)	((0x3f & (x)) << 8)
/* COMWAK GAP width window */
#define RG_TG_MIN_MSK		GENMASK(7, 5)
#define RG_TG_MIN_VAL(x)	((0x7 & (x)) << 5)
/* COMINIT GAP width window */
#define RG_T2_MIN_MSK		GENMASK(4, 0)
#define RG_T2_MIN_VAL(x)	(0x1f & (x))

#define ANA_RG_CTRL_SIGNAL1		0x4c
/* TX driver tail current control for 0dB de-empahsis mdoe for Gen1 speed */
#define RG_IDRV_0DB_GEN1_MSK		GENMASK(13, 8)
#define RG_IDRV_0DB_GEN1_VAL(x)		((0x3f & (x)) << 8)

#define ANA_RG_CTRL_SIGNAL4		0x58
#define RG_CDR_BICLTR_GEN1_MSK		GENMASK(23, 20)
#define RG_CDR_BICLTR_GEN1_VAL(x)	((0xf & (x)) << 20)
/* Loop filter R1 resistance adjustment for Gen1 speed */
#define RG_CDR_BR_GEN2_MSK		GENMASK(10, 8)
#define RG_CDR_BR_GEN2_VAL(x)		((0x7 & (x)) << 8)

#define ANA_RG_CTRL_SIGNAL6		0x60
/* I-path capacitance adjustment for Gen1 */
#define RG_CDR_BC_GEN1_MSK		GENMASK(28, 24)
#define RG_CDR_BC_GEN1_VAL(x)		((0x1f & (x)) << 24)
#define RG_CDR_BIRLTR_GEN1_MSK		GENMASK(4, 0)
#define RG_CDR_BIRLTR_GEN1_VAL(x)	(0x1f & (x))

#define ANA_EQ_EYE_CTRL_SIGNAL1		0x6c
/* RX Gen1 LEQ tuning step */
#define RG_EQ_DLEQ_LFI_GEN1_MSK		GENMASK(11, 8)
#define RG_EQ_DLEQ_LFI_GEN1_VAL(x)	((0xf & (x)) << 8)

#define ANA_EQ_EYE_CTRL_SIGNAL4		0xd8
#define RG_CDR_BIRLTD0_GEN1_MSK		GENMASK(20, 16)
#define RG_CDR_BIRLTD0_GEN1_VAL(x)	((0x1f & (x)) << 16)

#define ANA_EQ_EYE_CTRL_SIGNAL5		0xdc
#define RG_CDR_BIRLTD0_GEN3_MSK		GENMASK(4, 0)
#define RG_CDR_BIRLTD0_GEN3_VAL(x)	(0x1f & (x))

/* PHY switch between pcie/usb3/sgmii/sata */
#define USB_PHY_SWITCH_CTRL	0x0
#define RG_PHY_SW_TYPE		GENMASK(3, 0)
#define RG_PHY_SW_PCIE		0x0
#define RG_PHY_SW_USB3		0x1
#define RG_PHY_SW_SGMII		0x2
#define RG_PHY_SW_SATA		0x3

enum mtk_phy_version {
	MTK_PHY_V1 = 1,
	MTK_PHY_V2,
	MTK_PHY_V3,
	MTK_PHY_V4,
};

struct mtk_phy_pdata {
	/* avoid RX sensitivity level degradation only for mt8173 */
	bool avoid_rx_sen_degradation;
	/*
	 * Some SoCs (e.g. mt8195) drop a bit when use auto load efuse,
	 * support sw way, also support it for v2/v3 optionally.
	 */
	bool sw_efuse_supported;
	enum mtk_phy_version version;
};

struct u2phy_banks {
	void __iomem *misc;
	void __iomem *fmreg;
	void __iomem *com;
};

struct u3phy_banks {
	void __iomem *spllc;
	void __iomem *chip;
	void __iomem *phyd; /* include u3phyd_bank2 */
	void __iomem *phya; /* include u3phya_da */
};

struct mtk_phy_instance {
	struct phy *phy;
	void __iomem *port_base;
	union {
		struct u2phy_banks u2_banks;
		struct u3phy_banks u3_banks;
	};
	struct clk *ref_clk;	/* reference clock of anolog phy */
	u32 index;
	u32 type;
	struct regmap *type_sw;
	u32 type_sw_reg;
	u32 type_sw_index;
	bool u3_pll_ssc_delta;
	bool u3_pll_ssc_delta1;
	bool efuse_sw_en;
	bool efuse_alv_en;
	bool efuse_alv_en_ln1;
	u32 efuse_alv;
	u32 efuse_intr;
	u32 efuse_tx_imp;
	u32 efuse_rx_imp;
	u32 efuse_alv_ln1;
	u32 efuse_intr_ln1;
	u32 efuse_tx_imp_ln1;
	u32 efuse_rx_imp_ln1;
};

struct mtk_tphy {
	struct device *dev;
	void __iomem *sif_base;	/* only shared sif */
	const struct mtk_phy_pdata *pdata;
	struct mtk_phy_instance **phys;
	int nphys;
	int src_ref_clk; /* MHZ, reference clock for slew rate calibrate */
	int src_coef; /* coefficient for slew rate calibrate */
};

static void hs_slew_rate_calibrate(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *fmreg = u2_banks->fmreg;
	void __iomem *com = u2_banks->com;
	int calibration_val;
	int fm_out;
	u32 tmp;

	/* enable USB ring oscillator */
	tmp = readl(com + U3P_USBPHYACR5);
	tmp |= PA5_RG_U2_HSTX_SRCAL_EN;
	writel(tmp, com + U3P_USBPHYACR5);
	udelay(1);

	/*enable free run clock */
	tmp = readl(fmreg + U3P_U2FREQ_FMMONR1);
	tmp |= P2F_RG_FRCK_EN;
	writel(tmp, fmreg + U3P_U2FREQ_FMMONR1);

	/* set cycle count as 1024, and select u2 channel */
	tmp = readl(fmreg + U3P_U2FREQ_FMCR0);
	tmp &= ~(P2F_RG_CYCLECNT | P2F_RG_MONCLK_SEL);
	tmp |= P2F_RG_CYCLECNT_VAL(U3P_FM_DET_CYCLE_CNT);
	if (tphy->pdata->version == MTK_PHY_V1)
		tmp |= P2F_RG_MONCLK_SEL_VAL(instance->index >> 1);

	writel(tmp, fmreg + U3P_U2FREQ_FMCR0);

	/* enable frequency meter */
	tmp = readl(fmreg + U3P_U2FREQ_FMCR0);
	tmp |= P2F_RG_FREQDET_EN;
	writel(tmp, fmreg + U3P_U2FREQ_FMCR0);

	/* ignore return value */
	readl_poll_timeout(fmreg + U3P_U2FREQ_FMMONR1, tmp,
		  (tmp & P2F_USB_FM_VALID), 10, 200);

	fm_out = readl(fmreg + U3P_U2FREQ_VALUE);

	/* disable frequency meter */
	tmp = readl(fmreg + U3P_U2FREQ_FMCR0);
	tmp &= ~P2F_RG_FREQDET_EN;
	writel(tmp, fmreg + U3P_U2FREQ_FMCR0);

	/*disable free run clock */
	tmp = readl(fmreg + U3P_U2FREQ_FMMONR1);
	tmp &= ~P2F_RG_FRCK_EN;
	writel(tmp, fmreg + U3P_U2FREQ_FMMONR1);

	if (fm_out) {
		/* ( 1024 / FM_OUT ) x reference clock frequency x coef */
		tmp = tphy->src_ref_clk * tphy->src_coef;
		tmp = (tmp * U3P_FM_DET_CYCLE_CNT) / fm_out;
		calibration_val = DIV_ROUND_CLOSEST(tmp, U3P_SR_COEF_DIVISOR);
	} else {
		/* if FM detection fail, set default value */
		calibration_val = 4;
	}
	dev_dbg(tphy->dev, "phy:%d, fm_out:%d, calib:%d (clk:%d, coef:%d)\n",
		instance->index, fm_out, calibration_val,
		tphy->src_ref_clk, tphy->src_coef);

	/* set HS slew rate */
	tmp = readl(com + U3P_USBPHYACR5);
	tmp &= ~PA5_RG_U2_HSTX_SRCTRL;
	tmp |= PA5_RG_U2_HSTX_SRCTRL_VAL(calibration_val);
	writel(tmp, com + U3P_USBPHYACR5);

	/* disable USB ring oscillator */
	tmp = readl(com + U3P_USBPHYACR5);
	tmp &= ~PA5_RG_U2_HSTX_SRCAL_EN;
	writel(tmp, com + U3P_USBPHYACR5);
}

static void u3_phy_instance_init(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	u32 tmp;

	/* gating PCIe Analog XTAL clock */
	tmp = readl(u3_banks->spllc + U3P_SPLLC_XTALCTL3);
	tmp |= XC3_RG_U3_XTAL_RX_PWD | XC3_RG_U3_FRC_XTAL_RX_PWD;
	writel(tmp, u3_banks->spllc + U3P_SPLLC_XTALCTL3);

	/* gating XSQ */
	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG0);
	tmp &= ~P3A_RG_XTAL_EXT_EN_U3;
	tmp |= P3A_RG_XTAL_EXT_EN_U3_VAL(2);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG0);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG9);
	tmp &= ~P3A_RG_RX_DAC_MUX;
	tmp |= P3A_RG_RX_DAC_MUX_VAL(4);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG9);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG6);
	tmp &= ~P3A_RG_TX_EIDLE_CM;
	tmp |= P3A_RG_TX_EIDLE_CM_VAL(0xe);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG6);

	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_CDR1);
	tmp &= ~(P3D_RG_CDR_BIR_LTD0 | P3D_RG_CDR_BIR_LTD1);
	tmp |= P3D_RG_CDR_BIR_LTD0_VAL(0xc) | P3D_RG_CDR_BIR_LTD1_VAL(0x3);
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_CDR1);

	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_LFPS1);
	tmp &= ~P3D_RG_FWAKE_TH;
	tmp |= P3D_RG_FWAKE_TH_VAL(0x34);
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_LFPS1);

	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RXDET1);
	tmp &= ~P3D_RG_RXDET_STB2_SET;
	tmp |= P3D_RG_RXDET_STB2_SET_VAL(0x10);
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RXDET1);

	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RXDET2);
	tmp &= ~P3D_RG_RXDET_STB2_SET_P3;
	tmp |= P3D_RG_RXDET_STB2_SET_P3_VAL(0x10);
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RXDET2);

	if (instance->u3_pll_ssc_delta1) {
		tmp = readl(u3_banks->phyd + U3P_U3_PHYD_REG19);
		tmp &= ~P3D_RG_PLL_SSC_DELTA1;
		tmp |= P3D_RG_PLL_SSC_DELTA1_VAL(0x1c3);
		writel(tmp, u3_banks->phyd + U3P_U3_PHYD_REG19);
	}

	if (instance->u3_pll_ssc_delta) {
		tmp = readl(u3_banks->phyd + U3P_U3_PHYD_REG21);
		tmp &= ~P3D_RG_PLL_SSC_DELTA;
		tmp |= P3D_RG_PLL_SSC_DELTA_VAL(0x1c3);
		writel(tmp, u3_banks->phyd + U3P_U3_PHYD_REG21);
	}

	dev_dbg(tphy->dev, "%s(%d)\n", __func__, instance->index);
}

static void u2_phy_instance_init(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 index = instance->index;
	u32 tmp;

	/* switch to USB function, and enable usb pll */
	tmp = readl(com + U3P_U2PHYDTM0);
	tmp &= ~(P2C_FORCE_UART_EN | P2C_FORCE_SUSPENDM);
	tmp |= P2C_RG_XCVRSEL_VAL(1) | P2C_RG_DATAIN_VAL(0);
	writel(tmp, com + U3P_U2PHYDTM0);

	tmp = readl(com + U3P_U2PHYDTM1);
	tmp &= ~P2C_RG_UART_EN;
	writel(tmp, com + U3P_U2PHYDTM1);

	tmp = readl(com + U3P_USBPHYACR0);
	tmp |= PA0_RG_USB20_INTR_EN;
	writel(tmp, com + U3P_USBPHYACR0);

	/* disable switch 100uA current to SSUSB */
	tmp = readl(com + U3P_USBPHYACR5);
	tmp &= ~PA5_RG_U2_HS_100U_U3_EN;
	writel(tmp, com + U3P_USBPHYACR5);

	tmp = readl(com + U3P_U2PHYACR4);
	tmp &= ~P2C_U2_GPIO_CTR_MSK;
	writel(tmp, com + U3P_U2PHYACR4);

	if (tphy->pdata->avoid_rx_sen_degradation) {
		if (!index) {
			tmp = readl(com + U3P_USBPHYACR2);
			tmp |= PA2_RG_SIF_U2PLL_FORCE_EN;
			writel(tmp, com + U3P_USBPHYACR2);

			tmp = readl(com + U3D_U2PHYDCR0);
			tmp &= ~P2C_RG_SIF_U2PLL_FORCE_ON;
			writel(tmp, com + U3D_U2PHYDCR0);
		} else {
			tmp = readl(com + U3D_U2PHYDCR0);
			tmp |= P2C_RG_SIF_U2PLL_FORCE_ON;
			writel(tmp, com + U3D_U2PHYDCR0);

			tmp = readl(com + U3P_U2PHYDTM0);
			tmp |= P2C_RG_SUSPENDM | P2C_FORCE_SUSPENDM;
			writel(tmp, com + U3P_U2PHYDTM0);
		}
	}

	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_BC11_SW_EN;	/* DP/DM BC1.1 path Disable */
	tmp &= ~PA6_RG_U2_SQTH;
	tmp |= PA6_RG_U2_SQTH_VAL(2);
	writel(tmp, com + U3P_USBPHYACR6);

	dev_dbg(tphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_power_on(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 index = instance->index;
	u32 tmp;

	tmp = readl(com + U3P_U2PHYDTM0);
	tmp &= ~(P2C_RG_XCVRSEL | P2C_RG_DATAIN | P2C_DTM0_PART_MASK);
	writel(tmp, com + U3P_U2PHYDTM0);

	/* OTG Enable */
	tmp = readl(com + U3P_USBPHYACR6);
	tmp |= PA6_RG_U2_OTG_VBUSCMP_EN;
	writel(tmp, com + U3P_USBPHYACR6);

	tmp = readl(com + U3P_U2PHYDTM1);
	tmp |= P2C_RG_VBUSVALID | P2C_RG_AVALID;
	tmp &= ~P2C_RG_SESSEND;
	writel(tmp, com + U3P_U2PHYDTM1);

	if (tphy->pdata->avoid_rx_sen_degradation && index) {
		tmp = readl(com + U3D_U2PHYDCR0);
		tmp |= P2C_RG_SIF_U2PLL_FORCE_ON;
		writel(tmp, com + U3D_U2PHYDCR0);

		tmp = readl(com + U3P_U2PHYDTM0);
		tmp |= P2C_RG_SUSPENDM | P2C_FORCE_SUSPENDM;
		writel(tmp, com + U3P_U2PHYDTM0);
	}
	dev_dbg(tphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_power_off(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 index = instance->index;
	u32 tmp;

	tmp = readl(com + U3P_U2PHYDTM0);
	tmp &= ~(P2C_RG_XCVRSEL | P2C_RG_DATAIN);
	writel(tmp, com + U3P_U2PHYDTM0);

	/* OTG Disable */
	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_OTG_VBUSCMP_EN;
	writel(tmp, com + U3P_USBPHYACR6);

	tmp = readl(com + U3P_U2PHYDTM1);
	tmp &= ~(P2C_RG_VBUSVALID | P2C_RG_AVALID);
	tmp |= P2C_RG_SESSEND;
	writel(tmp, com + U3P_U2PHYDTM1);

	if (tphy->pdata->avoid_rx_sen_degradation && index) {
		tmp = readl(com + U3P_U2PHYDTM0);
		tmp &= ~(P2C_RG_SUSPENDM | P2C_FORCE_SUSPENDM);
		writel(tmp, com + U3P_U2PHYDTM0);

		tmp = readl(com + U3D_U2PHYDCR0);
		tmp &= ~P2C_RG_SIF_U2PLL_FORCE_ON;
		writel(tmp, com + U3D_U2PHYDCR0);
	}

	dev_dbg(tphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_exit(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 index = instance->index;
	u32 tmp;

	if (tphy->pdata->avoid_rx_sen_degradation && index) {
		tmp = readl(com + U3D_U2PHYDCR0);
		tmp &= ~P2C_RG_SIF_U2PLL_FORCE_ON;
		writel(tmp, com + U3D_U2PHYDCR0);

		tmp = readl(com + U3P_U2PHYDTM0);
		tmp &= ~P2C_FORCE_SUSPENDM;
		writel(tmp, com + U3P_U2PHYDTM0);
	}
}

static void u2_phy_instance_set_mode(struct mtk_tphy *tphy,
				     struct mtk_phy_instance *instance,
				     enum phy_mode mode)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	u32 tmp;

	tmp = readl(u2_banks->com + U3P_U2PHYDTM1);
	switch (mode) {
	case PHY_MODE_USB_DEVICE:
		tmp |= P2C_FORCE_IDDIG | P2C_RG_IDDIG;
		break;
	case PHY_MODE_USB_HOST:
		tmp |= P2C_FORCE_IDDIG;
		tmp &= ~P2C_RG_IDDIG;
		break;
	case PHY_MODE_USB_OTG:
		tmp &= ~(P2C_FORCE_IDDIG | P2C_RG_IDDIG);
		break;
	default:
		return;
	}
	writel(tmp, u2_banks->com + U3P_U2PHYDTM1);
}

static void pcie_phy_instance_init(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	u32 tmp;

	if (tphy->pdata->version != MTK_PHY_V1)
		return;

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG0);
	tmp &= ~(P3A_RG_XTAL_EXT_PE1H | P3A_RG_XTAL_EXT_PE2H);
	tmp |= P3A_RG_XTAL_EXT_PE1H_VAL(0x2) | P3A_RG_XTAL_EXT_PE2H_VAL(0x2);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG0);

	/* ref clk drive */
	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG1);
	tmp &= ~P3A_RG_CLKDRV_AMP;
	tmp |= P3A_RG_CLKDRV_AMP_VAL(0x4);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG1);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG0);
	tmp &= ~P3A_RG_CLKDRV_OFF;
	tmp |= P3A_RG_CLKDRV_OFF_VAL(0x1);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG0);

	/* SSC delta -5000ppm */
	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG20);
	tmp &= ~P3A_RG_PLL_DELTA1_PE2H;
	tmp |= P3A_RG_PLL_DELTA1_PE2H_VAL(0x3c);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG20);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG25);
	tmp &= ~P3A_RG_PLL_DELTA_PE2H;
	tmp |= P3A_RG_PLL_DELTA_PE2H_VAL(0x36);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG25);

	/* change pll BW 0.6M */
	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG5);
	tmp &= ~(P3A_RG_PLL_BR_PE2H | P3A_RG_PLL_IC_PE2H);
	tmp |= P3A_RG_PLL_BR_PE2H_VAL(0x1) | P3A_RG_PLL_IC_PE2H_VAL(0x1);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG5);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG4);
	tmp &= ~(P3A_RG_PLL_DIVEN_PE2H | P3A_RG_PLL_BC_PE2H);
	tmp |= P3A_RG_PLL_BC_PE2H_VAL(0x3);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG4);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG6);
	tmp &= ~P3A_RG_PLL_IR_PE2H;
	tmp |= P3A_RG_PLL_IR_PE2H_VAL(0x2);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG6);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG7);
	tmp &= ~P3A_RG_PLL_BP_PE2H;
	tmp |= P3A_RG_PLL_BP_PE2H_VAL(0xa);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG7);

	/* Tx Detect Rx Timing: 10us -> 5us */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RXDET1);
	tmp &= ~P3D_RG_RXDET_STB2_SET;
	tmp |= P3D_RG_RXDET_STB2_SET_VAL(0x10);
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RXDET1);

	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RXDET2);
	tmp &= ~P3D_RG_RXDET_STB2_SET_P3;
	tmp |= P3D_RG_RXDET_STB2_SET_P3_VAL(0x10);
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RXDET2);

	/* wait for PCIe subsys register to active */
	usleep_range(2500, 3000);
	dev_dbg(tphy->dev, "%s(%d)\n", __func__, instance->index);
}

/* following define for SSC change setting */
#define PHYD_MIX3 0x538
#define PLL_SSCEN		BIT(14)
#define FORCE_PLL_SSCEN	BIT(15)

#define REG16	0x83c
#define REG19	0x838
#define SSC_DITHER_GEN2	GENMASK(31, 16)

#define REG23	0x844
#define REG25	0x848
#define SSC_DITHER_GEN1	GENMASK(15, 0)

static void pcie_phy_instance_power_on(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *bank = &instance->u3_banks;
	u32 tmp;

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLD);
	tmp &= ~(P3C_FORCE_IP_SW_RST | P3C_REG_IP_SW_RST);
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLD);

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLE);
	tmp &= ~(P3C_RG_SWRST_U3_PHYD_FORCE_EN | P3C_RG_SWRST_U3_PHYD);
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLE);

	/* SSC setting is only for PCIe Gen2 SoCs. */
	if (tphy->pdata->version != MTK_PHY_V2)
		return;

	/* Set SSC for Gen2 */
	tmp = readl(bank->phya + REG16);
	tmp &= SSC_DITHER_GEN2;
	writel(tmp, bank->phya + REG16);
	tmp = readl(bank->phya + REG19);
	tmp &= SSC_DITHER_GEN2;
	writel(tmp, bank->phya + REG19);
	/* Set SSC for Gen1*/
	tmp = readl(bank->phya + REG23);
	tmp &= SSC_DITHER_GEN1;
	writel(tmp, bank->phya + REG23);
	tmp = readl(bank->phya + REG25);
	tmp &= SSC_DITHER_GEN1;
	writel(tmp, bank->phya + REG25);

	/* toggle for enable SSC */
	tmp = readl(bank->phya + PHYD_MIX3);
	tmp |= PLL_SSCEN | FORCE_PLL_SSCEN;
	writel(tmp, bank->phya + PHYD_MIX3);
	/* we don't know why udelay is needed for mt7622 port1, keep it here */
	udelay(500);
	tmp &= ~PLL_SSCEN;
	writel(tmp, bank->phya + PHYD_MIX3);
}

static void pcie_phy_instance_power_off(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)

{
	struct u3phy_banks *bank = &instance->u3_banks;
	u32 tmp;

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLD);
	tmp |= P3C_FORCE_IP_SW_RST | P3C_REG_IP_SW_RST;
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLD);

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLE);
	tmp |= P3C_RG_SWRST_U3_PHYD_FORCE_EN | P3C_RG_SWRST_U3_PHYD;
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLE);
}

static void sata_phy_instance_init(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	void __iomem *phyd = u3_banks->phyd;
	u32 tmp;

	/* charge current adjustment */
	tmp = readl(phyd + ANA_RG_CTRL_SIGNAL6);
	tmp &= ~(RG_CDR_BIRLTR_GEN1_MSK | RG_CDR_BC_GEN1_MSK);
	tmp |= RG_CDR_BIRLTR_GEN1_VAL(0x6) | RG_CDR_BC_GEN1_VAL(0x1a);
	writel(tmp, phyd + ANA_RG_CTRL_SIGNAL6);

	tmp = readl(phyd + ANA_EQ_EYE_CTRL_SIGNAL4);
	tmp &= ~RG_CDR_BIRLTD0_GEN1_MSK;
	tmp |= RG_CDR_BIRLTD0_GEN1_VAL(0x18);
	writel(tmp, phyd + ANA_EQ_EYE_CTRL_SIGNAL4);

	tmp = readl(phyd + ANA_EQ_EYE_CTRL_SIGNAL5);
	tmp &= ~RG_CDR_BIRLTD0_GEN3_MSK;
	tmp |= RG_CDR_BIRLTD0_GEN3_VAL(0x06);
	writel(tmp, phyd + ANA_EQ_EYE_CTRL_SIGNAL5);

	tmp = readl(phyd + ANA_RG_CTRL_SIGNAL4);
	tmp &= ~(RG_CDR_BICLTR_GEN1_MSK | RG_CDR_BR_GEN2_MSK);
	tmp |= RG_CDR_BICLTR_GEN1_VAL(0x0c) | RG_CDR_BR_GEN2_VAL(0x07);
	writel(tmp, phyd + ANA_RG_CTRL_SIGNAL4);

	tmp = readl(phyd + PHYD_CTRL_SIGNAL_MODE4);
	tmp &= ~(RG_CDR_BICLTD0_GEN1_MSK | RG_CDR_BICLTD1_GEN1_MSK);
	tmp |= RG_CDR_BICLTD0_GEN1_VAL(0x08) | RG_CDR_BICLTD1_GEN1_VAL(0x02);
	writel(tmp, phyd + PHYD_CTRL_SIGNAL_MODE4);

	tmp = readl(phyd + PHYD_DESIGN_OPTION2);
	tmp &= ~RG_LOCK_CNT_SEL_MSK;
	tmp |= RG_LOCK_CNT_SEL_VAL(0x02);
	writel(tmp, phyd + PHYD_DESIGN_OPTION2);

	tmp = readl(phyd + PHYD_DESIGN_OPTION9);
	tmp &= ~(RG_T2_MIN_MSK | RG_TG_MIN_MSK |
		 RG_T2_MAX_MSK | RG_TG_MAX_MSK);
	tmp |= RG_T2_MIN_VAL(0x12) | RG_TG_MIN_VAL(0x04) |
	       RG_T2_MAX_VAL(0x31) | RG_TG_MAX_VAL(0x0e);
	writel(tmp, phyd + PHYD_DESIGN_OPTION9);

	tmp = readl(phyd + ANA_RG_CTRL_SIGNAL1);
	tmp &= ~RG_IDRV_0DB_GEN1_MSK;
	tmp |= RG_IDRV_0DB_GEN1_VAL(0x20);
	writel(tmp, phyd + ANA_RG_CTRL_SIGNAL1);

	tmp = readl(phyd + ANA_EQ_EYE_CTRL_SIGNAL1);
	tmp &= ~RG_EQ_DLEQ_LFI_GEN1_MSK;
	tmp |= RG_EQ_DLEQ_LFI_GEN1_VAL(0x03);
	writel(tmp, phyd + ANA_EQ_EYE_CTRL_SIGNAL1);

	dev_dbg(tphy->dev, "%s(%d)\n", __func__, instance->index);
}

static void phy_v1_banks_init(struct mtk_tphy *tphy,
			      struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	struct u3phy_banks *u3_banks = &instance->u3_banks;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_banks->misc = NULL;
		u2_banks->fmreg = tphy->sif_base + SSUSB_SIFSLV_V1_U2FREQ;
		u2_banks->com = instance->port_base + SSUSB_SIFSLV_V1_U2PHY_COM;
		break;
	case PHY_TYPE_USB3:
	case PHY_TYPE_PCIE:
		u3_banks->spllc = tphy->sif_base + SSUSB_SIFSLV_V1_SPLLC;
		u3_banks->chip = tphy->sif_base + SSUSB_SIFSLV_V1_CHIP;
		u3_banks->phyd = instance->port_base + SSUSB_SIFSLV_V1_U3PHYD;
		u3_banks->phya = instance->port_base + SSUSB_SIFSLV_V1_U3PHYA;
		break;
	case PHY_TYPE_SATA:
		u3_banks->phyd = instance->port_base + SSUSB_SIFSLV_V1_U3PHYD;
		break;
	default:
		dev_err(tphy->dev, "incompatible PHY type\n");
		return;
	}
}

static void phy_v2_banks_init(struct mtk_tphy *tphy,
			      struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	struct u3phy_banks *u3_banks = &instance->u3_banks;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_banks->misc = instance->port_base + SSUSB_SIFSLV_V2_MISC;
		u2_banks->fmreg = instance->port_base + SSUSB_SIFSLV_V2_U2FREQ;
		u2_banks->com = instance->port_base + SSUSB_SIFSLV_V2_U2PHY_COM;
		break;
	case PHY_TYPE_USB3:
	case PHY_TYPE_PCIE:
		u3_banks->spllc = instance->port_base + SSUSB_SIFSLV_V2_SPLLC;
		u3_banks->chip = instance->port_base + SSUSB_SIFSLV_V2_CHIP;
		u3_banks->phyd = instance->port_base + SSUSB_SIFSLV_V2_U3PHYD;
		u3_banks->phya = instance->port_base + SSUSB_SIFSLV_V2_U3PHYA;
		break;
	default:
		dev_err(tphy->dev, "incompatible PHY type\n");
		return;
	}
}

static void phy_v4_banks_init(struct mtk_tphy *tphy,
			      struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	struct u3phy_banks *u3_banks = &instance->u3_banks;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_banks->misc = instance->port_base + SSUSB_SIFSLV_V2_MISC;
		u2_banks->fmreg = instance->port_base + SSUSB_SIFSLV_V2_U2FREQ;
		u2_banks->com = instance->port_base + SSUSB_SIFSLV_V2_U2PHY_COM;
		break;
	case PHY_TYPE_USB3:
		u3_banks->spllc = instance->port_base + SSUSB_SIFSLV_V2_SPLLC;
		u3_banks->chip = instance->port_base + SSUSB_SIFSLV_V2_CHIP;
		u3_banks->phyd = instance->port_base + SSUSB_SIFSLV_V2_U3PHYD;
		u3_banks->phya = instance->port_base + SSUSB_SIFSLV_V2_U3PHYA;
		break;
	case PHY_TYPE_PCIE:
		u3_banks->spllc = instance->port_base + SSUSB_SIFSLV_V4_SPLLC;
		u3_banks->chip = instance->port_base + SSUSB_SIFSLV_V4_CHIP;
		u3_banks->phyd = instance->port_base + SSUSB_SIFSLV_V4_U3PHYD;
		u3_banks->phya = instance->port_base + SSUSB_SIFSLV_V4_U3PHYA;
		break;
	default:
		dev_err(tphy->dev, "incompatible PHY type\n");
		return;
	}
}

static void phy_parse_property(struct mtk_tphy *tphy,
				struct mtk_phy_instance *instance)
{
	struct device *dev = &instance->phy->dev;

	if (instance->type == PHY_TYPE_USB3) {
		instance->u3_pll_ssc_delta =
			device_property_read_bool(dev,
				"mediatek,usb3-pll-ssc-delta");
		instance->u3_pll_ssc_delta1 =
			device_property_read_bool(dev,
				"mediatek,usb3-pll-ssc-delta1");

		dev_dbg(dev, "u3_pll_ssc_delta:%i, u3_pll_ssc_delta1:%i\n",
			instance->u3_pll_ssc_delta,
			instance->u3_pll_ssc_delta1);
	}
}

/* type switch for usb3/pcie/sgmii/sata */
static int phy_type_syscon_get(struct mtk_phy_instance *instance,
			       struct device_node *dn)
{
	struct of_phandle_args args;
	int ret;

	/* type switch function is optional */
	if (!of_property_read_bool(dn, "mediatek,syscon-type"))
		return 0;

	ret = of_parse_phandle_with_fixed_args(dn, "mediatek,syscon-type",
					       2, 0, &args);
	if (ret)
		return ret;

	instance->type_sw_reg = args.args[0];
	instance->type_sw_index = args.args[1] & 0x3; /* <=3 */
	instance->type_sw = syscon_node_to_regmap(args.np);
	of_node_put(args.np);
	dev_info(&instance->phy->dev, "type_sw - reg %#x, index %d\n",
		 instance->type_sw_reg, instance->type_sw_index);

	return PTR_ERR_OR_ZERO(instance->type_sw);
}

static int phy_type_set(struct mtk_phy_instance *instance)
{
	int type;
	u32 offset;

	if (!instance->type_sw)
		return 0;

	switch (instance->type) {
	case PHY_TYPE_USB3:
		type = RG_PHY_SW_USB3;
		break;
	case PHY_TYPE_PCIE:
		type = RG_PHY_SW_PCIE;
		break;
	case PHY_TYPE_SGMII:
		type = RG_PHY_SW_SGMII;
		break;
	case PHY_TYPE_SATA:
		type = RG_PHY_SW_SATA;
		break;
	case PHY_TYPE_USB2:
	default:
		return 0;
	}

	offset = instance->type_sw_index * BITS_PER_BYTE;
	regmap_update_bits(instance->type_sw, instance->type_sw_reg,
			   RG_PHY_SW_TYPE << offset, type << offset);

	return 0;
}

static int phy_efuse_get(struct mtk_tphy *tphy, struct mtk_phy_instance *instance)
{
	struct device *dev = &instance->phy->dev;
	const char *prefix;
	int ret = 0;
	bool alv, efuse_sw_en = false;

	/* tphy v1 doesn't support sw efuse, skip it */
	if (!tphy->pdata->sw_efuse_supported) {
		instance->efuse_sw_en = false;
		return 0;
	}

	/* software efuse is optional */
	instance->efuse_sw_en = device_property_read_bool(dev, "nvmem-cells");
	if (!instance->efuse_sw_en)
		return 0;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		prefix = "u2";
		alv = of_property_read_bool(dev->of_node, "auto_load_valid");
		if (alv) {
			instance->efuse_alv_en = alv;
			ret = nvmem_cell_read_variable_le_u32(dev, "auto_load_valid",
							      &instance->efuse_alv);
			if (ret)
				dev_err(dev, "fail to get %s %s efuse, %d\n",
					prefix, "auto_load_valid", ret);
			else
				dev_dbg(dev, "%s %s efuse: exist with value: %u\n",
					prefix, "auto_load_valid", instance->efuse_alv);
		}

		ret = nvmem_cell_read_variable_le_u32(dev, "intr",
						      &instance->efuse_intr);
		if (ret)
			dev_err(dev, "fail to get %s %s efuse, %d\n",
				prefix, "intr", ret);

		/* no efuse, ignore it */
		if (!instance->efuse_intr) {
			dev_warn(dev, "no %s efuse, but dts enable it\n", prefix);
			break;
		}

		efuse_sw_en = true;

		dev_dbg(dev, "%s efuse intr: %x\n", prefix, instance->efuse_intr);
		break;

	case PHY_TYPE_USB3:
	case PHY_TYPE_PCIE:
		prefix = (instance->type == PHY_TYPE_PCIE) ? "pcie" : "u3";
		alv = of_property_read_bool(dev->of_node, "auto_load_valid");
		if (alv) {
			instance->efuse_alv_en = alv;
			ret = nvmem_cell_read_variable_le_u32(dev, "auto_load_valid",
							      &instance->efuse_alv);
			if (ret)
				dev_err(dev, "fail to get %s %s efuse, %d\n",
					prefix, "auto_load_valid", ret);
			else
				dev_dbg(dev, "%s %s efuse: exist with value: %u\n",
					prefix, "auto_load_valid", instance->efuse_alv);
		}

		ret = nvmem_cell_read_variable_le_u32(dev, "intr",
						      &instance->efuse_intr);
		if (ret)
			dev_err(dev, "fail to get %s %s efuse, %d\n",
				prefix, "intr", ret);

		ret = nvmem_cell_read_variable_le_u32(dev, "rx_imp",
						      &instance->efuse_rx_imp);
		if (ret)
			dev_err(dev, "fail to get %s %s efuse, %d\n",
				prefix, "rx_imp", ret);

		ret = nvmem_cell_read_variable_le_u32(dev, "tx_imp",
						      &instance->efuse_tx_imp);
		if (ret)
			dev_err(dev, "fail to get %s %s efuse, %d\n",
				prefix, "tx_imp", ret);

		/* no efuse, ignore it */
		if (!instance->efuse_intr &&
		    !instance->efuse_rx_imp &&
		    !instance->efuse_tx_imp) {
			dev_warn(dev, "no %s efuse, but dts enable it\n", prefix);
			break;
		}

		efuse_sw_en = true;

		dev_dbg(dev, "%s efuse intr: %x, rx_imp: %x, tx_imp: %x\n",
			prefix, instance->efuse_intr, instance->efuse_rx_imp,
			instance->efuse_tx_imp);

		if (tphy->pdata->version != MTK_PHY_V4)
			break;

		prefix = "pcie ln1";
		efuse_sw_en = false;

		alv = of_property_read_bool(dev->of_node, "auto_load_valid_ln1");
		if (alv) {
			instance->efuse_alv_en_ln1 = alv;
			ret = nvmem_cell_read_variable_le_u32(dev, "auto_load_valid_ln1",
							      &instance->efuse_alv_ln1);
			if (ret)
				dev_err(dev, "fail to get %s %s efuse, %d\n",
					prefix, "auto_load_valid_ln1", ret);
			else
				dev_dbg(dev, "%s %s efuse: exist with value: %u\n",
					prefix, "auto_load_valid_ln1", instance->efuse_alv_ln1);
		}

		ret = nvmem_cell_read_variable_le_u32(dev, "intr_ln1",
						      &instance->efuse_intr_ln1);
		if (ret)
			dev_err(dev, "fail to get %s %s efuse, %d\n",
				prefix, "intr_ln1", ret);

		ret = nvmem_cell_read_variable_le_u32(dev, "rx_imp_ln1",
						      &instance->efuse_rx_imp_ln1);
		if (ret)
			dev_err(dev, "fail to get %s %s efuse, %d\n",
				prefix, "rx_imp_ln1", ret);

		ret = nvmem_cell_read_variable_le_u32(dev, "tx_imp_ln1",
						      &instance->efuse_tx_imp_ln1);
		if (ret)
			dev_err(dev, "fail to get %s %s efuse, %d\n",
				prefix, "tx_imp_ln1", ret);

		/* no efuse, ignore it */
		if (!instance->efuse_intr_ln1 &&
		    !instance->efuse_rx_imp_ln1 &&
		    !instance->efuse_tx_imp_ln1) {
			dev_warn(dev, "no %s efuse, but dts enable it\n", prefix);
			break;
		}

		efuse_sw_en = true;

		dev_dbg(dev, "%s efuse intr: %x, rx_imp: %x, tx_imp: %x\n",
			prefix, instance->efuse_intr_ln1, instance->efuse_rx_imp_ln1,
			instance->efuse_tx_imp_ln1);
		break;

	default:
		dev_err(dev, "no sw efuse for type %d\n", instance->type);
		ret = -EINVAL;
		break;
	}

	instance->efuse_sw_en = efuse_sw_en;

	return ret;
}

static void phy_efuse_set(struct mtk_phy_instance *instance)
{
	struct device *dev = &instance->phy->dev;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	const char *prefix;
	u32 tmp;

	if (!instance->efuse_sw_en)
		return;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		prefix = "u2";
		if (instance->efuse_alv_en &&
		    instance->efuse_alv == 1)
			break;

		tmp = readl(u2_banks->misc + U3P_MISC_REG1);
		tmp |= MR1_EFUSE_AUTO_LOAD_DIS;
		writel(tmp, u2_banks->misc + U3P_MISC_REG1);

		tmp = readl(u2_banks->com + U3P_USBPHYACR1);
		tmp &= ~PA1_RG_INTR_CAL;
		tmp |= PA1_RG_INTR_CAL_VAL(instance->efuse_intr);
		writel(tmp, u2_banks->com + U3P_USBPHYACR1);

		dev_info(dev, "%s: set efuse, intr: %x\n",
			 prefix, instance->efuse_intr);
		break;

	case PHY_TYPE_USB3:
		prefix = "u3";
		if (instance->efuse_alv_en &&
		    instance->efuse_alv == 1)
			break;

		tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RSV);
		tmp |= P3D_RG_EFUSE_AUTO_LOAD_DIS;
		writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RSV);

		tmp = readl(u3_banks->phyd + U3P_U3_PHYD_IMPCAL0);
		tmp &= ~P3D_RG_TX_IMPEL;
		tmp |= P3D_RG_TX_IMPEL_VAL(instance->efuse_tx_imp);
		tmp |= P3D_RG_FORCE_TX_IMPEL;
		writel(tmp, u3_banks->phyd + U3P_U3_PHYD_IMPCAL0);

		tmp = readl(u3_banks->phyd + U3P_U3_PHYD_IMPCAL1);
		tmp &= ~P3D_RG_RX_IMPEL;
		tmp |= P3D_RG_RX_IMPEL_VAL(instance->efuse_rx_imp);
		tmp |= P3D_RG_FORCE_RX_IMPEL;
		writel(tmp, u3_banks->phyd + U3P_U3_PHYD_IMPCAL1);

		tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG0);
		tmp &= ~P3A_RG_IEXT_INTR;
		tmp |= P3A_RG_IEXT_INTR_VAL(instance->efuse_intr);
		writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG0);

		dev_info(dev, "%s: set efuse, intr: %x, rx_imp: %x, tx_imp: %x\n",
			prefix, instance->efuse_intr, instance->efuse_rx_imp,
			instance->efuse_tx_imp);
		break;

	case PHY_TYPE_PCIE:
		prefix = "pcie";
		if (instance->efuse_alv_en &&
		    instance->efuse_alv == 1)
			break;

		tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RSV);
		tmp |= P3D_RG_EFUSE_AUTO_LOAD_DIS;
		writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RSV);

		tmp = readl(u3_banks->phyd + U3P_U3_PHYD_IMPCAL0);
		tmp &= ~P3D_RG_TX_IMPEL;
		tmp |= P3D_RG_TX_IMPEL_VAL(instance->efuse_tx_imp);
		tmp |= P3D_RG_FORCE_TX_IMPEL;
		writel(tmp, u3_banks->phyd + U3P_U3_PHYD_IMPCAL0);

		tmp = readl(u3_banks->phyd + U3P_U3_PHYD_IMPCAL1);
		tmp &= ~P3D_RG_RX_IMPEL;
		tmp |= P3D_RG_RX_IMPEL_VAL(instance->efuse_rx_imp);
		tmp |= P3D_RG_FORCE_RX_IMPEL;
		writel(tmp, u3_banks->phyd + U3P_U3_PHYD_IMPCAL1);

		tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG0);
		tmp &= ~P3A_RG_IEXT_INTR;
		tmp |= P3A_RG_IEXT_INTR_VAL(instance->efuse_intr);
		writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG0);

		dev_info(dev, "%s: set efuse, intr: %x, rx_imp: %x, tx_imp: %x\n",
			prefix, instance->efuse_intr, instance->efuse_rx_imp,
			instance->efuse_tx_imp);

		if ((!instance->efuse_intr_ln1 &&
		     !instance->efuse_rx_imp_ln1 &&
		     !instance->efuse_tx_imp_ln1) ||
		    (instance->efuse_alv_en_ln1 &&
		     instance->efuse_alv_ln1 == 1))
			break;

		prefix = "pcie_ln1";

		tmp = readl(u3_banks->phyd + SSUSB_LN1_OFFSET + U3P_U3_PHYD_RSV);
		tmp |= P3D_RG_EFUSE_AUTO_LOAD_DIS;
		writel(tmp, u3_banks->phyd + SSUSB_LN1_OFFSET + U3P_U3_PHYD_RSV);

		tmp = readl(u3_banks->phyd + SSUSB_LN1_OFFSET + U3P_U3_PHYD_IMPCAL0);
		tmp &= ~P3D_RG_TX_IMPEL;
		tmp |= P3D_RG_TX_IMPEL_VAL(instance->efuse_tx_imp_ln1);
		tmp |= P3D_RG_FORCE_TX_IMPEL;
		writel(tmp, u3_banks->phyd + SSUSB_LN1_OFFSET + U3P_U3_PHYD_IMPCAL0);

		tmp = readl(u3_banks->phyd + SSUSB_LN1_OFFSET + U3P_U3_PHYD_IMPCAL1);
		tmp &= ~P3D_RG_RX_IMPEL;
		tmp |= P3D_RG_RX_IMPEL_VAL(instance->efuse_rx_imp_ln1);
		tmp |= P3D_RG_FORCE_RX_IMPEL;
		writel(tmp, u3_banks->phyd + SSUSB_LN1_OFFSET + U3P_U3_PHYD_IMPCAL1);

		tmp = readl(u3_banks->phya + SSUSB_LN1_OFFSET + U3P_U3_PHYA_REG0);
		tmp &= ~P3A_RG_IEXT_INTR;
		tmp |= P3A_RG_IEXT_INTR_VAL(instance->efuse_intr_ln1);
		writel(tmp, u3_banks->phya + SSUSB_LN1_OFFSET + U3P_U3_PHYA_REG0);

		dev_info(dev, "%s: set efuse, intr: %x, rx_imp: %x, tx_imp: %x\n",
			prefix, instance->efuse_intr_ln1, instance->efuse_rx_imp_ln1,
			instance->efuse_tx_imp_ln1);
		break;

	default:
		dev_warn(dev, "no sw efuse for type %d\n", instance->type);
		break;
	}
}

static int mtk_phy_init(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);
	int ret;

	ret = clk_prepare_enable(instance->ref_clk);
	if (ret) {
		dev_err(tphy->dev, "failed to enable ref_clk\n");
		return ret;
	}

	phy_efuse_set(instance);

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_phy_instance_init(tphy, instance);
		break;
	case PHY_TYPE_USB3:
		u3_phy_instance_init(tphy, instance);
		break;
	case PHY_TYPE_PCIE:
		pcie_phy_instance_init(tphy, instance);
		break;
	case PHY_TYPE_SATA:
		sata_phy_instance_init(tphy, instance);
		break;
	case PHY_TYPE_SGMII:
		/* nothing to do, only used to set type */
		break;
	default:
		dev_err(tphy->dev, "incompatible PHY type\n");
		clk_disable_unprepare(instance->ref_clk);
		return -EINVAL;
	}

	return 0;
}

static int mtk_phy_power_on(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2) {
		u2_phy_instance_power_on(tphy, instance);
		hs_slew_rate_calibrate(tphy, instance);
	} else if (instance->type == PHY_TYPE_PCIE) {
		pcie_phy_instance_power_on(tphy, instance);
	}

	return 0;
}

static int mtk_phy_power_off(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2)
		u2_phy_instance_power_off(tphy, instance);
	else if (instance->type == PHY_TYPE_PCIE)
		pcie_phy_instance_power_off(tphy, instance);

	return 0;
}

static int mtk_phy_exit(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2)
		u2_phy_instance_exit(tphy, instance);

	clk_disable_unprepare(instance->ref_clk);
	return 0;
}

static int mtk_phy_set_mode(struct phy *phy, enum phy_mode mode)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2)
		u2_phy_instance_set_mode(tphy, instance, mode);

	return 0;
}

static struct phy *mtk_phy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct mtk_tphy *tphy = dev_get_drvdata(dev);
	struct mtk_phy_instance *instance = NULL;
	struct device_node *phy_np = args->np;
	int index;
	int ret;

	if (args->args_count != 1) {
		dev_err(dev, "invalid number of cells in 'phy' property\n");
		return ERR_PTR(-EINVAL);
	}

	for (index = 0; index < tphy->nphys; index++)
		if (phy_np == tphy->phys[index]->phy->dev.of_node) {
			instance = tphy->phys[index];
			break;
		}

	if (!instance) {
		dev_err(dev, "failed to find appropriate phy\n");
		return ERR_PTR(-EINVAL);
	}

	instance->type = args->args[0];
	if (!(instance->type == PHY_TYPE_USB2 ||
	      instance->type == PHY_TYPE_USB3 ||
	      instance->type == PHY_TYPE_PCIE ||
	      instance->type == PHY_TYPE_SATA ||
	      instance->type == PHY_TYPE_SGMII)) {
		dev_err(dev, "unsupported device type: %d\n", instance->type);
		return ERR_PTR(-EINVAL);
	}

	if (tphy->pdata->version == MTK_PHY_V1) {
		phy_v1_banks_init(tphy, instance);
	} else if (tphy->pdata->version == MTK_PHY_V2) {
		phy_v2_banks_init(tphy, instance);
	} else if (tphy->pdata->version == MTK_PHY_V4) {
		phy_v4_banks_init(tphy, instance);
	} else {
		dev_err(dev, "phy version is not supported\n");
		return ERR_PTR(-EINVAL);
	}

	ret = phy_efuse_get(tphy, instance);
	if (ret)
		return ERR_PTR(ret);

	phy_parse_property(tphy, instance);
	phy_type_set(instance);

	return instance->phy;
}

static const struct phy_ops mtk_tphy_ops = {
	.init		= mtk_phy_init,
	.exit		= mtk_phy_exit,
	.power_on	= mtk_phy_power_on,
	.power_off	= mtk_phy_power_off,
	.set_mode	= mtk_phy_set_mode,
	.owner		= THIS_MODULE,
};

static const struct mtk_phy_pdata tphy_v1_pdata = {
	.avoid_rx_sen_degradation = false,
	.version = MTK_PHY_V1,
};

static const struct mtk_phy_pdata tphy_v2_pdata = {
	.avoid_rx_sen_degradation = false,
	.sw_efuse_supported = true,
	.version = MTK_PHY_V2,
};

static const struct mtk_phy_pdata mt8173_pdata = {
	.avoid_rx_sen_degradation = true,
	.version = MTK_PHY_V1,
};

static const struct mtk_phy_pdata tphy_v4_pdata = {
	.avoid_rx_sen_degradation = false,
	.sw_efuse_supported = true,
	.version = MTK_PHY_V4,
};

static const struct of_device_id mtk_tphy_id_table[] = {
	{ .compatible = "mediatek,mt2701-u3phy", .data = &tphy_v1_pdata },
	{ .compatible = "mediatek,mt2712-u3phy", .data = &tphy_v2_pdata },
	{ .compatible = "mediatek,mt8173-u3phy", .data = &mt8173_pdata },
	{ .compatible = "mediatek,generic-tphy-v1", .data = &tphy_v1_pdata },
	{ .compatible = "mediatek,generic-tphy-v2", .data = &tphy_v2_pdata },
	{ .compatible = "mediatek,generic-tphy-v4", .data = &tphy_v4_pdata },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_tphy_id_table);

static int mtk_tphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	struct phy_provider *provider;
	struct resource *sif_res;
	struct mtk_tphy *tphy;
	struct resource res;
	int port, retval;

	tphy = devm_kzalloc(dev, sizeof(*tphy), GFP_KERNEL);
	if (!tphy)
		return -ENOMEM;

	tphy->pdata = of_device_get_match_data(dev);
	if (!tphy->pdata)
		return -EINVAL;

	tphy->nphys = of_get_child_count(np);
	tphy->phys = devm_kcalloc(dev, tphy->nphys,
				       sizeof(*tphy->phys), GFP_KERNEL);
	if (!tphy->phys)
		return -ENOMEM;

	tphy->dev = dev;
	platform_set_drvdata(pdev, tphy);

	sif_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	/* SATA phy of V1 needn't it if not shared with PCIe or USB */
	if (sif_res && tphy->pdata->version == MTK_PHY_V1) {
		/* get banks shared by multiple phys */
		tphy->sif_base = devm_ioremap_resource(dev, sif_res);
		if (IS_ERR(tphy->sif_base)) {
			dev_err(dev, "failed to remap sif regs\n");
			return PTR_ERR(tphy->sif_base);
		}
	}

	tphy->src_ref_clk = U3P_REF_CLK;
	tphy->src_coef = U3P_SLEW_RATE_COEF;
	/* update parameters of slew rate calibrate if exist */
	device_property_read_u32(dev, "mediatek,src-ref-clk-mhz",
		&tphy->src_ref_clk);
	device_property_read_u32(dev, "mediatek,src-coef", &tphy->src_coef);

	port = 0;
	for_each_child_of_node(np, child_np) {
		struct mtk_phy_instance *instance;
		struct phy *phy;

		instance = devm_kzalloc(dev, sizeof(*instance), GFP_KERNEL);
		if (!instance) {
			retval = -ENOMEM;
			goto put_child;
		}

		tphy->phys[port] = instance;

		phy = devm_phy_create(dev, child_np, &mtk_tphy_ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "failed to create phy\n");
			retval = PTR_ERR(phy);
			goto put_child;
		}

		retval = of_address_to_resource(child_np, 0, &res);
		if (retval) {
			dev_err(dev, "failed to get address resource(id-%d)\n",
				port);
			goto put_child;
		}

		instance->port_base = devm_ioremap_resource(&phy->dev, &res);
		if (IS_ERR(instance->port_base)) {
			dev_err(dev, "failed to remap phy regs\n");
			retval = PTR_ERR(instance->port_base);
			goto put_child;
		}

		instance->phy = phy;
		instance->index = port;
		phy_set_drvdata(phy, instance);
		port++;

		instance->ref_clk = devm_clk_get_optional(&phy->dev, "ref");
		if (IS_ERR(instance->ref_clk)) {
			dev_err(dev, "failed to get ref_clk(id-%d)\n", port);
			retval = PTR_ERR(instance->ref_clk);
			goto put_child;
		}

		retval = phy_type_syscon_get(instance, child_np);
		if (retval)
			goto put_child;
	}

	provider = devm_of_phy_provider_register(dev, mtk_phy_xlate);

	return PTR_ERR_OR_ZERO(provider);
put_child:
	of_node_put(child_np);
	return retval;
}

static struct platform_driver mtk_tphy_driver = {
	.probe		= mtk_tphy_probe,
	.driver		= {
		.name	= "mtk-tphy",
		.of_match_table = mtk_tphy_id_table,
	},
};

module_platform_driver(mtk_tphy_driver);

MODULE_AUTHOR("Chunfeng Yun <chunfeng.yun@mediatek.com>");
MODULE_DESCRIPTION("MediaTek T-PHY driver");
MODULE_LICENSE("GPL v2");
