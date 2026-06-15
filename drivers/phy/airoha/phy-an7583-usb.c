// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 AIROHA Inc
 */

#include <dt-bindings/phy/phy.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>

#include <soc/airoha/efuse.h>

/* u2 phy banks */
#define SSUSB_SIFSLV_V2_U2FREQ		0x100
#define SSUSB_SIFSLV_V2_U2PHY_COM	0x300

#define U3P_USBPHYACR1			0x004
#define PA1_RG_U2_INTR_CAL		GENMASK(23, 19)
#define PA1_RG_U2_INTR_CAL_VAL(x)	((0x1f & (x)) << 19)

#define U3P_USBPHYACR4			0x010
#define PA4_RG_U2_FS_CR			GENMASK(10, 8)
#define PA4_RG_U2_FS_CR_VAL(x)		((0x7 & (x)) << 8)
#define PA4_RG_U2_FS_SR			GENMASK(2, 0)
#define PA4_RG_U2_FS_SR_VAL(x)		 (0x7 & (x))

#define U3P_USBPHYACR5			0x014
#define PA5_RG_U2_HSTX_SRCAL_EN		BIT(15)
#define PA5_RG_U2_HSTX_SRCTRL		GENMASK(14, 12)
#define PA5_RG_U2_HSTX_SRCTRL_VAL(x)	((0x7 & (x)) << 12)
#define PA5_RG_U2_HS_100U_U3_EN		BIT(11)

#define U3P_USBPHYACR6			0x018
#define PA6_RG_U2_BC11_SW_EN		BIT(23)
#define PA6_RG_U2_DISCTH		GENMASK(7, 4)
#define PA6_RG_U2_DISCTH_VAL(x)		((0xf & (x)) << 4)
#define PA6_RG_U2_SQTH			GENMASK(3, 0)
#define PA6_RG_U2_SQTH_VAL(x)		 (0xf & (x))

#define U3P_U2FREQ_FMCR0		0x00
#define P2F_RG_MONCLK_SEL		GENMASK(27, 26)
#define P2F_RG_MONCLK_SEL_VAL(x)	((0x3 & (x)) << 26)
#define P2F_RG_FREQDET_EN		BIT(24)
#define P2F_RG_CYCLECNT			GENMASK(23, 0)
#define P2F_RG_CYCLECNT_VAL(x)		((P2F_RG_CYCLECNT) & (x))

#define U3P_U2FREQ_FMMONR0		0x0c

#define U3P_U2FREQ_FMMONR1		0x10
#define P2F_USB_FM_VALID		BIT(0)
#define P2F_RG_FRCK_EN			BIT(8)

#define U3P_REF_CLK			20	/* MHZ */
#define U3P_SLEW_RATE_COEF		28
#define U3P_SR_COEF_DIVISOR		1000
#define U3P_FM_DET_CYCLE_CNT		1024

enum qp_top {
	QP_DIG,
	QP_PMA,
	QP_ANA,
	QP_TOP_MAX
};

struct u2_banks {
	void __iomem *fmreg;
	void __iomem *com;
	u32 id;
};

struct u3_banks {
	void __iomem *top[QP_TOP_MAX];
};

struct airoha_uphy_instance {
	struct phy *phy;
	void __iomem *port_base;
	union {
		struct u2_banks u2b;
		struct u3_banks u3b;
	};
	u32 index;
	u32 type;
};

struct airoha_uphy {
	struct device *dev;
	struct airoha_uphy_instance **phys;
	int nphys;

	/* efuse data */
	u32 ssusb_tx_term_sel;
	u32 ssusb_rx_imp_sel;
	u32 usb20_p0_intr_cal;
	u32 usb20_p1_intr_cal;
};

struct qp_reg_entry {
	enum qp_top top : 16;
	u16 reg;
	u32 val;
	u32 msk;
	u32 ofs;
};

static void qp_reg_rw(void __iomem *qp_reg, u32 val, u32 msk, u32 ofs)
{
	u32 tmp = readl(qp_reg);

	if (tmp == 0xdeadbeef)
		return;

	tmp &= ~(msk << ofs);
	tmp |= ((val & msk) << ofs);
	writel(tmp, qp_reg);
}

static void qp_pma_md(struct u3_banks *u3b, u32 reg, u32 val, u32 msk, u32 ofs)
{
	qp_reg_rw(u3b->top[QP_PMA] + reg, val, msk, ofs);
}

static void qp_ana_md(struct u3_banks *u3b, u32 reg, u32 val, u32 msk, u32 ofs)
{
	qp_reg_rw(u3b->top[QP_ANA] + reg, val, msk, ofs);
}

static void qp_reg_batch(struct u3_banks *u3b, const struct qp_reg_entry *tbl,
			 size_t tbl_size)
{
	size_t i;

	for (i = 0; i < tbl_size; i++) {
		const struct qp_reg_entry *re = &tbl[i];
		void __iomem *reg = u3b->top[re->top] + (u32)re->reg;

		qp_reg_rw(reg, re->val, re->msk, re->ofs);
	}
}

static inline void qp_base_init(struct u3_banks *u3b)
{
	static const struct qp_reg_entry QP_INIT_TBL[] = {
		/* Digital CLK trigger reverse */
		{ QP_PMA, 0x100,          0,        0x1, 12 },

		/* RX USB PCIE enable */
		{ QP_PMA, 0x6b8,          1,        0x1, 26 },
		{ QP_PMA, 0x600,          0,        0x1,  1 },

		/* 50MHz XTAL */
		{ QP_ANA, 0x030,          1,        0x1,  1 },
		{ QP_ANA, 0x030,          1,        0x3,  2 },
		{ QP_ANA, 0x03c,          1,        0x3, 25 },
		{ QP_PMA, 0x410,          0,        0x1,  2 },
		{ QP_DIG, 0x340,          0,        0x1, 25 },
		{ QP_DIG, 0x340,          0,        0x1, 26 },
		{ QP_DIG, 0x340,          1,        0x1, 24 },
		{ QP_DIG, 0x340,          1,        0x1, 25 },
		{ QP_DIG, 0x340,          1,        0x1, 26 },

		/* PLL RG */
		{ QP_PMA, 0x408,          1,        0x1, 22 },
		{ QP_PMA, 0x408,          4,        0xf, 16 },
		{ QP_PMA, 0x408,          0,        0x3, 12 },
		{ QP_PMA, 0x408,          0,        0x3, 20 },
		{ QP_PMA, 0x408,          3,        0x3,  0 },
		{ QP_PMA, 0x408,          5,        0x7,  2 },
		{ QP_PMA, 0x408,          1,        0x3,  6 },
		{ QP_PMA, 0x408,          1,        0x1, 14 },
		{ QP_PMA, 0x408,          1,        0x1, 27 },
		{ QP_PMA, 0x410,          1,        0x3,  3 },
		{ QP_PMA, 0x408,          1,        0x1, 30 },
		{ QP_PMA, 0x40c,      0x21e,     0xffff,  0 },
		{ QP_PMA, 0x40c,      0x18c,     0xffff, 16 },
		{ QP_PMA, 0x404,          1,        0x1, 30 },
		{ QP_PMA, 0x248, 0x48000000, 0x7fffffff,  0 },
		{ QP_PMA, 0x208,          1,        0x3, 16 },
		{ QP_PMA, 0x230, 0x48000000, 0x7fffffff,  0 },

		/* RX RG 5G */
		{ QP_ANA, 0x00c,          0,        0x3,  4 },
		{ QP_ANA, 0x004,          1,        0x1, 11 },
		{ QP_ANA, 0x018,        0xc,       0x1f,  8 },
		{ QP_ANA, 0x020,          0,        0x1, 13 },
		{ QP_ANA, 0x01c,          0,        0x1, 30 },
		{ QP_ANA, 0x01c,          1,        0x1,  6 },
		{ QP_ANA, 0x018,          3,        0x7, 24 },
		{ QP_ANA, 0x01c,       0x19,       0x3f,  0 },
		{ QP_PMA, 0x6e0,          1,        0x1, 26 },
		{ QP_PMA, 0x6ec,      0x1f0,      0x7ff, 21 },

		/* EQ all */
		{ QP_PMA, 0x6f0,          0,        0x1, 28 },
		{ QP_PMA, 0x6f0,          0,        0x1, 27 },
		{ QP_PMA, 0x6f0,          0,        0x1, 26 },
		{ QP_PMA, 0x6e0,          1,        0x1, 29 },
		{ QP_PMA, 0x6e0,          1,        0x1, 28 },
		{ QP_PMA, 0x6e0,          1,        0x1, 27 },
		{ QP_PMA, 0x6c0,          0,        0x1, 31 },
		{ QP_PMA, 0x654,          1,        0x1, 19 },
		{ QP_PMA, 0x6b8,          2,      0x3ff, 16 },

		/* PI CAL */
		{ QP_PMA, 0x614,       0x10,       0xff,  0 },
		{ QP_ANA, 0x040,          1,        0x1,  3 },
		{ QP_ANA, 0x040,          1,        0x1,  4 },
		{ QP_PMA, 0x00c,          0,        0x7, 28 },
		{ QP_ANA, 0x004,          0,        0x1, 10 },

		/* Common setting */
		{ QP_DIG, 0x30c,          1,        0x1, 24 },
		{ QP_DIG, 0x30c,          1,        0x1, 25 },
		{ QP_PMA, 0x000,        0xa,       0x3f,  4 },
		{ QP_PMA, 0x000,        0x4,        0xf, 12 },
		{ QP_ANA, 0x02c,          0,        0x1, 25 },
		{ QP_ANA, 0x02c,          1,        0x1, 27 },
		{ QP_PMA, 0x41c,          1,        0x1,  0 },
		{ QP_PMA, 0x41c,          1,        0x1,  1 },
		{ QP_PMA, 0x41c,          1,        0x1,  2 },

		/* RX speed up */
		{ QP_PMA, 0x63c,       0x28,    0xfffff, 10 },
		{ QP_PMA, 0x640,       0x64,    0xfffff,  0 },
		{ QP_PMA, 0x644,     0x2710,    0xfffff,  0 },
		{ QP_PMA, 0x630,      0x9c4,     0x1fff, 16 },
		{ QP_PMA, 0x6dc,      0x9c4,     0x1fff,  0 },
		{ QP_PMA, 0x6f0,      0x9c4,     0x1fff, 13 },
		{ QP_PMA, 0x6f0,      0x9c4,     0x1fff,  0 },

		/* TCL avoid LT noise impact */
		{ QP_ANA, 0x028,          1,        0x1, 24 },

		/* PLL auto init */
		{ QP_PMA, 0x400,          1,        0x1,  0 },
	};

	/* QPHY base init */
	qp_reg_batch(u3b, QP_INIT_TBL, ARRAY_SIZE(QP_INIT_TBL));

	usleep_range(100, 200);

	qp_pma_md(u3b, 0x41c, 0, 0x1, 0);
}

static bool qp_read_kband(struct u3_banks *u3b, u32 *val)
{
	static const struct qp_reg_entry QP_ICO_TBL[] = {
		{ QP_PMA, 0x410,    0,  0x1,  2 },
		{ QP_ANA, 0x030, 0x15, 0x3f, 21 },
		{ QP_ANA, 0x03c,    1,  0x3,  5 },
		{ QP_ANA, 0x03c,    1,  0x1, 19 },
		{ QP_PMA, 0x408,    1,  0x1, 29 },
		{ QP_PMA, 0x320,    0,  0x3, 11 },
		{ QP_PMA, 0x320,    0,  0x1, 13 },
		{ QP_PMA, 0x408,    1,  0x1, 14 },
	};
	u32 tmp1, tmp2;

	usleep_range(50000, 51000);

	/* Default ICO setting */
	qp_reg_batch(u3b, QP_ICO_TBL, ARRAY_SIZE(QP_ICO_TBL));

	/* Read KBand */
	qp_ana_md(u3b, 0x038, 0, 0x1, 19);
	usleep_range(5000, 6000);

	tmp1 = readl(u3b->top[QP_PMA] + 0x364);
	tmp1 = (tmp1 >> 16) & 0xff;

	qp_ana_md(u3b, 0x038, 1, 0x1, 19);
	usleep_range(5000, 6000);

	tmp2 = readl(u3b->top[QP_PMA] + 0x364);
	tmp2 = (tmp2 >> 16) & 0xff;

	*val = (tmp1 << 4) | tmp2;

	if ((*val & 0x800) && (*val != 0xfff))
		return true;

	/* KBAND_DONE = 0 or KBAND_Code = 0xFFF */
	return false;
}

static inline void qp_enable_tx_pll(struct u3_banks *u3b)
{
	static const struct qp_reg_entry QP_TX_PLL_TBL[] = {
		/* PLL reset to default */
		{ QP_PMA, 0x03c, 0, 0x1, 14 },
		{ QP_PMA, 0x03c, 0, 0x3, 16 },
		{ QP_PMA, 0x31c, 0, 0x1,  4 },
		{ QP_PMA, 0x31c, 0, 0x1,  5 },

		/* PLL force stable -> auto mode */
		{ QP_PMA, 0x418, 1, 0x1, 19 },
		{ QP_PMA, 0x418, 1, 0x1, 18 },
		{ QP_PMA, 0x418, 0, 0x1, 19 },
		{ QP_PMA, 0x418, 0, 0x1, 18 },

		/* TX force enable -> auto mode */
		{ QP_PMA, 0x318, 1, 0x1, 12 },
		{ QP_PMA, 0x318, 0, 0x1, 13 },
		{ QP_PMA, 0x318, 0, 0x1, 12 },
	};

	qp_reg_batch(u3b, QP_TX_PLL_TBL, ARRAY_SIZE(QP_TX_PLL_TBL));
}

static inline u32 qp_reconfig_pll(struct u3_banks *u3b)
{
	static const struct qp_reg_entry QP_CPR_TBL[] = {
		{ QP_PMA, 0x410, 1,  0x1,  2 },
		{ QP_ANA, 0x030, 0, 0x3f, 21 },
		{ QP_ANA, 0x03c, 3,  0x3,  5 },
		{ QP_ANA, 0x03c, 0,  0x1, 19 },
		{ QP_PMA, 0x408, 0,  0x1, 29 },
		{ QP_PMA, 0x408, 0,  0x1, 14 },
		{ QP_ANA, 0x03c, 1,  0x1, 14 },
		{ QP_ANA, 0x03c, 3,  0x3, 16 },
	};
	u32 i;

	/* TX force disable */
	qp_pma_md(u3b, 0x318, 0, 0x1, 12);
	qp_pma_md(u3b, 0x318, 1, 0x1, 13);

	/* PLL force unstable */
	qp_pma_md(u3b, 0x418, 1, 0x1, 19);

	for (i = 0; i < 50; i++) {
		u32 kband = 0;

		/* PLL disable */
		qp_pma_md(u3b, 0x320, 3, 0x3, 11);
		qp_pma_md(u3b, 0x320, 1, 0x1, 13);
		qp_pma_md(u3b, 0x31c, 0, 0x1, 4);
		qp_pma_md(u3b, 0x31c, 1, 0x1, 5);

		/* PLL config for CPR */
		qp_reg_batch(u3b, QP_CPR_TBL, ARRAY_SIZE(QP_CPR_TBL));

		/* PLL enable */
		qp_pma_md(u3b, 0x31c, 1, 0x1, 4);
		qp_pma_md(u3b, 0x31c, 1, 0x1, 5);

		if (qp_read_kband(u3b, &kband))
			break;
	}

	qp_enable_tx_pll(u3b);

	return i + 1;
}

static void u3_phy_instance_init(struct airoha_uphy *uphy,
				 struct airoha_uphy_instance *instance)
{
	struct u3_banks *u3b = &instance->u3b;
	u32 ssusb_tx_term_sel = uphy->ssusb_tx_term_sel;
	u32 ssusb_rx_imp_sel = uphy->ssusb_rx_imp_sel;
	int leqos, leqos_orig;
	u32 kband = 0;

	if (ssusb_tx_term_sel == EFUSE_INVALID_VAL)
		ssusb_tx_term_sel = 0x1;

	if (ssusb_rx_imp_sel == EFUSE_INVALID_VAL)
		ssusb_rx_imp_sel = 0x10;

	/* apply eFUSE data */
	qp_pma_md(u3b, 0x320, ssusb_tx_term_sel, 0x3, 5);
	qp_pma_md(u3b, 0x320, 1, 0x1, 7);

	qp_pma_md(u3b, 0x324, ssusb_rx_imp_sel, 0x1f, 26);
	qp_pma_md(u3b, 0x324, 1, 0x1, 31);

	qp_base_init(u3b);

	if (!qp_read_kband(u3b, &kband)) {
		u32 elapsed = qp_reconfig_pll(u3b);

		dev_warn(uphy->dev, "phy: %d, PLL config for CPR: %u\n",
			 instance->index, elapsed);
	}

	qp_read_kband(u3b, &kband);

	dev_dbg(uphy->dev, "phy: %d, KBand code: 0x%x\n",
		instance->index, kband);

	usleep_range(1000, 2000);

	/* force leqos shift3 */
	qp_pma_md(u3b, 0x65c, 1, 0x1, 29);
	qp_pma_md(u3b, 0x6e0, 1, 0x1, 10);
	usleep_range(2000, 3000);

	leqos = readl(u3b->top[QP_PMA] + 0x6d4);
	leqos = (leqos >> 24) & 0x1f;
	leqos_orig = leqos;

	if (leqos > 0x10)
		leqos = -(leqos & 0xf);

	if (leqos >= 0)
		leqos -= 3;
	else
		leqos += 3;

	if (leqos < 0)
		leqos = (-leqos) + 0x10;

	qp_pma_md(u3b, 0x110, leqos, 0x1f, 8);
	qp_pma_md(u3b, 0x130, 1, 0x1, 31);

	qp_pma_md(u3b, 0x65c, 0, 0x1, 29);
	qp_pma_md(u3b, 0x6e0, 0, 0x1, 10);

	dev_dbg(uphy->dev, "phy: %d, leqos: %x -> %x\n",
		instance->index, leqos_orig, leqos);
}

static void hs_slew_rate_calibrate(struct airoha_uphy *uphy,
				   struct airoha_uphy_instance *instance)
{
	struct u2_banks *u2b = &instance->u2b;
	int calibration_val;
	int fm_out;
	u32 tmp;

	/* enable USB ring oscillator */
	tmp = readl(u2b->com + U3P_USBPHYACR5);
	tmp |= PA5_RG_U2_HSTX_SRCAL_EN;
	writel(tmp, u2b->com + U3P_USBPHYACR5);
	usleep_range(100, 200);

	/* enable free run clock */
	tmp = readl(u2b->fmreg + U3P_U2FREQ_FMMONR1);
	tmp |= P2F_RG_FRCK_EN;
	writel(tmp, u2b->fmreg + U3P_U2FREQ_FMMONR1);

	/* set cycle count as 1024, and select u2 channel */
	tmp = readl(u2b->fmreg + U3P_U2FREQ_FMCR0);
	tmp &= ~(P2F_RG_CYCLECNT | P2F_RG_MONCLK_SEL);
	tmp |= P2F_RG_CYCLECNT_VAL(U3P_FM_DET_CYCLE_CNT);
	tmp |= P2F_RG_MONCLK_SEL_VAL(u2b->id + 1);
	writel(tmp, u2b->fmreg + U3P_U2FREQ_FMCR0);

	/* enable frequency meter */
	tmp = readl(u2b->fmreg + U3P_U2FREQ_FMCR0);
	tmp |= P2F_RG_FREQDET_EN;
	writel(tmp, u2b->fmreg + U3P_U2FREQ_FMCR0);

	/* poll frequency meter ready */
	readl_poll_timeout(u2b->fmreg + U3P_U2FREQ_FMMONR1, tmp,
			   (tmp & P2F_USB_FM_VALID), 100, 2000);

	fm_out = readl(u2b->fmreg + U3P_U2FREQ_FMMONR0);

	/* disable frequency meter */
	tmp = readl(u2b->fmreg + U3P_U2FREQ_FMCR0);
	tmp &= ~P2F_RG_FREQDET_EN;
	writel(tmp, u2b->fmreg + U3P_U2FREQ_FMCR0);

	/* disable free run clock */
	tmp = readl(u2b->fmreg + U3P_U2FREQ_FMMONR1);
	tmp &= ~P2F_RG_FRCK_EN;
	writel(tmp, u2b->fmreg + U3P_U2FREQ_FMMONR1);

	/* disable USB ring oscillator */
	tmp = readl(u2b->com + U3P_USBPHYACR5);
	tmp &= ~PA5_RG_U2_HSTX_SRCAL_EN;
	writel(tmp, u2b->com + U3P_USBPHYACR5);
	usleep_range(100, 200);

	if (fm_out) {
		/* ( 1024 / FM_OUT ) x reference clock frequency x coef */
		tmp = U3P_REF_CLK * U3P_SLEW_RATE_COEF;
		tmp = (tmp * U3P_FM_DET_CYCLE_CNT) / fm_out;
		calibration_val = DIV_ROUND_CLOSEST(tmp, U3P_SR_COEF_DIVISOR);
	} else {
		/* if FM detection fail, set default value */
		dev_warn(uphy->dev, "phy: %d, FM detection failed, use default\n",
			 instance->index);

		calibration_val = 5;
	}

	/* set HS slew rate */
	tmp = readl(u2b->com + U3P_USBPHYACR5);
	tmp &= ~PA5_RG_U2_HSTX_SRCTRL;
	tmp |=  PA5_RG_U2_HSTX_SRCTRL_VAL(calibration_val);
	writel(tmp, u2b->com + U3P_USBPHYACR5);
	udelay(1);

	dev_dbg(uphy->dev, "phy: %d, fm_out: %d, calib: %d\n",
		instance->index, fm_out, calibration_val);
}

static void u2_phy_instance_init(struct airoha_uphy *uphy,
				 struct airoha_uphy_instance *instance)
{
	struct u2_banks *u2b = &instance->u2b;
	u32 u2_intr_cal = uphy->usb20_p0_intr_cal;
	u32 tmp;

	if (u2b->id > 0)
		u2_intr_cal = uphy->usb20_p1_intr_cal;

	/* apply eFUSE data */
	if (u2_intr_cal != EFUSE_INVALID_VAL) {
		tmp = readl(u2b->com + U3P_USBPHYACR1);
		tmp &= ~PA1_RG_U2_INTR_CAL;
		tmp |=  PA1_RG_U2_INTR_CAL_VAL(u2_intr_cal);
		writel(tmp, u2b->com + U3P_USBPHYACR1);
	}

	tmp = readl(u2b->com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_BC11_SW_EN;	/* DP/DM BC1.1 path Disable */
	writel(tmp, u2b->com + U3P_USBPHYACR6);
	usleep_range(500, 800);

	tmp = readl(u2b->com + U3P_USBPHYACR4);
	tmp &= ~PA4_RG_U2_FS_CR;
	tmp &= ~PA4_RG_U2_FS_SR;
	tmp |=  PA4_RG_U2_FS_CR_VAL(6);
	tmp |=  PA4_RG_U2_FS_SR_VAL(2);
	writel(tmp, u2b->com + U3P_USBPHYACR4);

	tmp = readl(u2b->com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_SQTH;
	tmp |=  PA6_RG_U2_SQTH_VAL(0x8);
	tmp &= ~PA6_RG_U2_DISCTH;
	tmp |=  PA6_RG_U2_DISCTH_VAL(0x9);
	writel(tmp, u2b->com + U3P_USBPHYACR6);
}

static int airoha_uphy_init(struct phy *phy)
{
	struct airoha_uphy_instance *instance = phy_get_drvdata(phy);
	struct airoha_uphy *uphy = dev_get_drvdata(phy->dev.parent);

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_phy_instance_init(uphy, instance);
		break;
	case PHY_TYPE_USB3:
		u3_phy_instance_init(uphy, instance);
		break;
	default:
		dev_err(uphy->dev, "incompatible PHY type\n");
		return -EINVAL;
	}

	return 0;
}

static int airoha_uphy_power_on(struct phy *phy)
{
	struct airoha_uphy_instance *instance = phy_get_drvdata(phy);
	struct airoha_uphy *uphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2)
		hs_slew_rate_calibrate(uphy, instance);

	return 0;
}

static int airoha_uphy_exit(struct phy *phy)
{
	return 0;
}

static const struct phy_ops airoha_uphy_ops = {
	.init		= airoha_uphy_init,
	.exit		= airoha_uphy_exit,
	.power_on	= airoha_uphy_power_on,
	.owner		= THIS_MODULE,
};

static void read_efuse_cal_data(struct airoha_uphy *uphy)
{
	void *mem;
	u32 efuse_vaild;

	uphy->ssusb_tx_term_sel = EFUSE_INVALID_VAL;
	uphy->ssusb_rx_imp_sel  = EFUSE_INVALID_VAL;
	uphy->usb20_p0_intr_cal = EFUSE_INVALID_VAL;
	uphy->usb20_p1_intr_cal = EFUSE_INVALID_VAL;

	mem = airoha_efuse_load();
	if (!mem)
		return;

	efuse_vaild = airoha_efuse_get(mem, SDS_USB_efuse_valid_LSB, 3);
	if (!efuse_vaild)
		dev_warn(uphy->dev, "USB eFuse not vaild\n");

	if (efuse_vaild & BIT(0)) {
		uphy->ssusb_tx_term_sel = airoha_efuse_get(mem,
						DA_QP_SSUSB_TX_TERM_SEL_LSB, 2);

		uphy->ssusb_rx_imp_sel = airoha_efuse_get(mem,
						DA_QP_SSUSB_RX_IMP_SEL_LSB, 5);

		uphy->usb20_p0_intr_cal = airoha_efuse_get(mem,
						RG_USB20_P0_INTR_CTRL_LSB, 5);
	}

	if (efuse_vaild & BIT(1))
		uphy->usb20_p1_intr_cal = airoha_efuse_get(mem,
						RG_USB20_P1_INTR_CTRL_LSB, 5);

	airoha_efuse_free(mem);
}

static void __iomem *phy_map_resource_byname(struct device *dev,
					     const char *name)
{
	struct device_node *np = dev->of_node;
	struct resource res;
	int index;

	index = of_property_match_string(np, "reg-names", name);
	if (index < 0 || of_address_to_resource(np, index, &res))
		return ERR_PTR(-ENODEV);

	return devm_ioremap_resource(dev, &res);
}

static struct phy *airoha_uphy_xlate(struct device *dev,
				     struct of_phandle_args *args)
{
	struct airoha_uphy *uphy = dev_get_drvdata(dev);
	struct airoha_uphy_instance *instance = NULL;
	struct device_node *phy_np = args->np;
	int index;

	if (args->args_count != 1) {
		dev_err(dev, "invalid number of cells in 'phy' property\n");
		return ERR_PTR(-EINVAL);
	}

	for (index = 0; index < uphy->nphys; index++) {
		if (phy_np == uphy->phys[index]->phy->dev.of_node) {
			instance = uphy->phys[index];
			break;
		}
	}

	if (!instance) {
		dev_err(dev, "failed to find appropriate phy\n");
		return ERR_PTR(-EINVAL);
	}

	instance->type = args->args[0];

	if (instance->type == PHY_TYPE_USB2) {
		struct u2_banks *u2b = &instance->u2b;

		u2b->fmreg = instance->port_base + SSUSB_SIFSLV_V2_U2FREQ;
		u2b->com = instance->port_base + SSUSB_SIFSLV_V2_U2PHY_COM;
		u2b->id++;
	} else if (instance->type == PHY_TYPE_USB3) {
		struct u3_banks *u3b = &instance->u3b;
		struct device *phy_dev = &instance->phy->dev;

		u3b->top[QP_DIG] = phy_map_resource_byname(phy_dev, "dig");
		if (IS_ERR(u3b->top[QP_DIG])) {
			dev_err(phy_dev, "failed to remap %s regs\n", "dig");
			return u3b->top[QP_DIG];
		}

		u3b->top[QP_ANA] = phy_map_resource_byname(phy_dev, "ana");
		if (IS_ERR(u3b->top[QP_ANA])) {
			dev_err(phy_dev, "failed to remap %s regs\n", "ana");
			return u3b->top[QP_ANA];
		}

		u3b->top[QP_PMA] = phy_map_resource_byname(phy_dev, "pma");
		if (IS_ERR(u3b->top[QP_PMA])) {
			dev_err(phy_dev, "failed to remap %s regs\n", "pma");
			return u3b->top[QP_PMA];
		}
	} else {
		dev_err(dev, "unsupported device type: %d\n", instance->type);
		return ERR_PTR(-EINVAL);
	}

	return instance->phy;
}

static int airoha_uphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	struct phy_provider *provider;
	struct airoha_uphy *uphy;
	struct resource res;
	int port, err;

	uphy = devm_kzalloc(dev, sizeof(*uphy), GFP_KERNEL);
	if (!uphy)
		return -ENOMEM;

	uphy->nphys = of_get_child_count(np);
	uphy->phys = devm_kcalloc(dev, uphy->nphys,
				  sizeof(*uphy->phys), GFP_KERNEL);
	if (!uphy->phys)
		return -ENOMEM;

	uphy->dev = dev;
	platform_set_drvdata(pdev, uphy);

	port = 0;
	for_each_child_of_node(np, child_np) {
		struct airoha_uphy_instance *instance;
		struct phy *phy;

		instance = devm_kzalloc(dev, sizeof(*instance), GFP_KERNEL);
		if (!instance) {
			err = -ENOMEM;
			goto put_child;
		}

		uphy->phys[port] = instance;

		phy = devm_phy_create(dev, child_np, &airoha_uphy_ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "failed to create phy\n");
			err = PTR_ERR(phy);
			goto put_child;
		}

		err = of_address_to_resource(child_np, 0, &res);
		if (err) {
			dev_err(dev, "failed to get address resource(id-%d)\n",
				port);
			goto put_child;
		}

		instance->port_base = devm_ioremap_resource(&phy->dev, &res);
		if (IS_ERR(instance->port_base)) {
			dev_err(dev, "failed to remap phy regs\n");
			err = PTR_ERR(instance->port_base);
			goto put_child;
		}

		instance->phy = phy;
		instance->index = port;
		phy_set_drvdata(phy, instance);
		port++;
	}

	read_efuse_cal_data(uphy);

	provider = devm_of_phy_provider_register(dev, airoha_uphy_xlate);

	return PTR_ERR_OR_ZERO(provider);
put_child:
	of_node_put(child_np);
	return err;
}

static const struct of_device_id airoha_usb_qphy_id_table[] = {
	{ .compatible = "airoha,an7583-uphy" },
	{ },
};
MODULE_DEVICE_TABLE(of, airoha_usb_qphy_id_table);

static struct platform_driver airoha_usb_qphy_driver = {
	.probe		= airoha_uphy_probe,
	.driver		= {
		.name	= "airoha-usb-qphy",
		.of_match_table = airoha_usb_qphy_id_table,
	},
};

module_platform_driver(airoha_usb_qphy_driver);

MODULE_DESCRIPTION("Airoha USB QPHY driver");
MODULE_LICENSE("GPL v2");
