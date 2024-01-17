/*******************************************************************************
 *  PWM Drvier
 *
 * Mediatek Pulse Width Modulator driver
 *
 * Copyright (C) 2015 John Crispin <blogic at openwrt.org>
 * Copyright (C) 2017 Zhi Mao <zhi.mao@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public Licence,
 * version 2, as publish by the Free Software Foundation.
 *
 * This program is distributed and in hope it will be useful, but WITHOUT
 * ANY WARRNTY; without even the implied warranty of MERCHANTABITLITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/types.h>

#define PWM_EN_REG		0x0000
#define PWMCON			0x00
#define PWMGDUR			0x0c
#define PWMWAVENUM		0x28
#define PWMDWIDTH		0x2c
#define PWM45DWIDTH_FIXUP	0x30
#define PWMTHRES		0x30
#define PWM45THRES_FIXUP	0x34

#define PWM_CLK_DIV_MAX		7
#define PWM_NUM_MAX		8

#define REG_V1			1
#define REG_V2			2

#define PWM_CLK_NAME_MAIN	"main"
#define PWM_CLK_NAME_TOP	"top"

static const char * const mtk_pwm_clk_name[PWM_NUM_MAX] = {
	"pwm1", "pwm2", "pwm3", "pwm4", "pwm5", "pwm6", "pwm7", "pwm8"
};

struct mtk_com_pwm_data {
	unsigned int pwm_nums;
	bool pwm45_fixup;
	int reg_ver;
};

/**
 * struct mtk_pwm_chip - struct representing pwm chip
 *
 * @mmio_base: base address of pwm chip
 * @chip: linux pwm chip representation
 */
struct mtk_pwm_chip {
	struct device *dev;
	void __iomem *mmio_base;
	struct pwm_chip chip;
	struct clk *clk_top;
	struct clk *clk_main;
	struct clk *clk_pwm[PWM_NUM_MAX];
	const struct mtk_com_pwm_data *data;
};

/*==========================================*/
static const unsigned int mtk_pwm_reg_offset_v1[] = {
	0x0010, 0x0050, 0x0090, 0x00d0, 0x0110, 0x0150, 0x0190, 0x0220
};

static const unsigned int mtk_pwm_reg_offset_v2[] = {
	0x0080, 0x00c0, 0x0100, 0x0140, 0x0180, 0x01c0, 0x0200, 0x0240
};

/*==========================================*/
static const struct mtk_com_pwm_data mt7622_pwm_data = {
	.pwm_nums = 6,
	.pwm45_fixup = false,
	.reg_ver = REG_V1,
};

static const struct mtk_com_pwm_data mt7623_pwm_data = {
	.pwm_nums = 5,
	.pwm45_fixup = true,
	.reg_ver = REG_V1,
};

static const struct mtk_com_pwm_data mt7981_pwm_data = {
	.pwm_nums = 3,
	.pwm45_fixup = false,
	.reg_ver = REG_V2,
};

static const struct mtk_com_pwm_data mt7986_pwm_data = {
	.pwm_nums = 2,
	.pwm45_fixup = false,
	.reg_ver = REG_V1,
};

static const struct mtk_com_pwm_data mt7988_pwm_data = {
	.pwm_nums = 8,
	.pwm45_fixup = false,
	.reg_ver = REG_V2,
};
/*==========================================*/

static const struct of_device_id mtk_pwm_of_match[] = {
	{.compatible = "mediatek,mt7622-pwm", .data = &mt7622_pwm_data},
	{.compatible = "mediatek,mt7623-pwm", .data = &mt7623_pwm_data},
	{.compatible = "mediatek,mt7981-pwm", .data = &mt7981_pwm_data},
	{.compatible = "mediatek,mt7986-pwm", .data = &mt7986_pwm_data},
	{.compatible = "mediatek,mt7988-pwm", .data = &mt7988_pwm_data},
	{},
};

static inline struct mtk_pwm_chip *to_mtk_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct mtk_pwm_chip, chip);
}

static int mtk_pwm_clk_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_pwm_chip *pc = to_mtk_pwm_chip(chip);
	int ret = 0;

	ret = clk_prepare_enable(pc->clk_top);
	if (ret < 0)
		return ret;

	ret = clk_prepare_enable(pc->clk_main);
	if (ret < 0) {
		clk_disable_unprepare(pc->clk_top);
		return ret;
	}

	ret = clk_prepare_enable(pc->clk_pwm[pwm->hwpwm]);
	if (ret < 0) {
		clk_disable_unprepare(pc->clk_main);
		clk_disable_unprepare(pc->clk_top);
		return ret;
	}

	return ret;
}

static void mtk_pwm_clk_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_pwm_chip *pc = to_mtk_pwm_chip(chip);

	clk_disable_unprepare(pc->clk_pwm[pwm->hwpwm]);
	clk_disable_unprepare(pc->clk_main);
	clk_disable_unprepare(pc->clk_top);
}

static inline u32 mtk_pwm_readl(struct mtk_pwm_chip *chip,
				unsigned int num, unsigned int offset)
{
	u32 pwm_offset;

	switch (chip->data->reg_ver) {
	case REG_V2:
		pwm_offset = mtk_pwm_reg_offset_v2[num];
		break;

	case REG_V1:
	default:
		pwm_offset = mtk_pwm_reg_offset_v1[num];
	}

	return readl(chip->mmio_base + pwm_offset + offset);
}

static inline void mtk_pwm_writel(struct mtk_pwm_chip *chip,
				  unsigned int num, unsigned int offset,
				  u32 value)
{
	u32 pwm_offset;

	switch (chip->data->reg_ver) {
	case REG_V2:
		pwm_offset = mtk_pwm_reg_offset_v2[num];
		break;

	case REG_V1:
	default:
		pwm_offset = mtk_pwm_reg_offset_v1[num];
	}

	writel(value, chip->mmio_base + pwm_offset + offset);
}

static int mtk_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			int duty_ns, int period_ns)
{
	struct mtk_pwm_chip *pc = to_mtk_pwm_chip(chip);
	struct clk *clk = pc->clk_pwm[pwm->hwpwm];
	u64 resolution;
	u32 clkdiv = 0;
	u32 clksel = 0;
	u32 reg_width = PWMDWIDTH, reg_thres = PWMTHRES;
	u32 cnt_period, cnt_duty;
	u32 data_width, thresh;

	mtk_pwm_clk_enable(chip, pwm);

	/* Using resolution in picosecond gets accuracy higher */
	resolution = (u64)NSEC_PER_SEC * 1000;

	/* Calculate resolution based on current clock frequency */
	do_div(resolution, clk_get_rate(clk));

	/* Using resolution to calculate cnt_period which represents
	 * the effective range of the PWM period counter
	 */
	cnt_period = DIV_ROUND_CLOSEST_ULL((u64)period_ns * 1000, resolution);
	while (cnt_period > 8191) {
		/* Using clkdiv to reduce clock frequency and calculate
		 * new resolution based on new clock speed
		 */
		resolution *= 2;
		clkdiv++;

		if (clkdiv > PWM_CLK_DIV_MAX && !clksel) {
			/* Using clksel to divide the pwm source clock by
			 * an additional 1625, and recalculate new clkdiv
			 * and resolution
			 */
			clksel = 1;
			clkdiv = 0;
			resolution = (u64)NSEC_PER_SEC * 1000 * 1625;
			do_div(resolution, clk_get_rate(clk));
		}

		/* Calculate cnt_period based on resolution */
		cnt_period = DIV_ROUND_CLOSEST_ULL((u64)period_ns * 1000,
						   resolution);
	}

	if (clkdiv > PWM_CLK_DIV_MAX) {
		mtk_pwm_clk_disable(chip, pwm);
		dev_err(pc->dev, "period %d not supported\n", period_ns);
		return -EINVAL;
	}

	data_width = period_ns / resolution;
	thresh = duty_ns / resolution;

	if (pc->data->pwm45_fixup && pwm->hwpwm > 2) {
		/*
		 * PWM[4,5] has distinct offset for PWMDWIDTH and PWMTHRES
		 * from the other PWMs on MT7623.
		 */
		reg_width = PWM45DWIDTH_FIXUP;
		reg_thres = PWM45THRES_FIXUP;
	}

	/* Calculate cnt_duty based on resolution */
	cnt_duty = DIV_ROUND_CLOSEST_ULL((u64)duty_ns * 1000, resolution);

	/* The source clock is divided by 2^clkdiv or if the clksel bit
	 * is set by (2^clkdiv*1625)
	 */

	if (clksel)
		mtk_pwm_writel(pc, pwm->hwpwm, PWMCON, BIT(15) | BIT(3) | clkdiv);
	else
		mtk_pwm_writel(pc, pwm->hwpwm, PWMCON, BIT(15) | clkdiv);

	mtk_pwm_writel(pc, pwm->hwpwm, reg_width, cnt_period);
	mtk_pwm_writel(pc, pwm->hwpwm, reg_thres, cnt_duty);

	mtk_pwm_clk_disable(chip, pwm);

	return 0;
}

static int mtk_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_pwm_chip *pc = to_mtk_pwm_chip(chip);
	u32 val;

	mtk_pwm_clk_enable(chip, pwm);

	val = readl(pc->mmio_base + PWM_EN_REG);
	val |= BIT(pwm->hwpwm);
	writel(val, pc->mmio_base + PWM_EN_REG);

	return 0;
}

static void mtk_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_pwm_chip *pc = to_mtk_pwm_chip(chip);
	u32 val;

	val = readl(pc->mmio_base + PWM_EN_REG);
	val &= ~BIT(pwm->hwpwm);
	writel(val, pc->mmio_base + PWM_EN_REG);

	mtk_pwm_clk_disable(chip, pwm);
}

static const struct pwm_ops mtk_pwm_ops = {
	.config = mtk_pwm_config,
	.enable = mtk_pwm_enable,
	.disable = mtk_pwm_disable,
	.owner = THIS_MODULE,
};

static int mtk_pwm_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;
	struct mtk_pwm_chip *pc;
	struct resource *r;
	int ret;
	int i;

	id = of_match_device(mtk_pwm_of_match, &pdev->dev);
	if (!id)
		return -EINVAL;

	pc = devm_kzalloc(&pdev->dev, sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;

	pc->data = id->data;
	pc->dev = &pdev->dev;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pc->mmio_base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(pc->mmio_base))
		return PTR_ERR(pc->mmio_base);

	for (i = 0; i < pc->data->pwm_nums; i++) {
		pc->clk_pwm[i] = devm_clk_get(&pdev->dev, mtk_pwm_clk_name[i]);
		if (IS_ERR(pc->clk_pwm[i])) {
			dev_err(&pdev->dev, "[PWM] clock: %s fail\n", mtk_pwm_clk_name[i]);
			return PTR_ERR(pc->clk_pwm[i]);
		}
	}

	pc->clk_main = devm_clk_get(&pdev->dev, PWM_CLK_NAME_MAIN);
	if (IS_ERR(pc->clk_main))
		return PTR_ERR(pc->clk_main);

	pc->clk_top = devm_clk_get(&pdev->dev, PWM_CLK_NAME_TOP);
	if (IS_ERR(pc->clk_top))
		return PTR_ERR(pc->clk_top);

	pc->chip.dev = &pdev->dev;
	pc->chip.ops = &mtk_pwm_ops;
	pc->chip.base = -1;
	pc->chip.npwm = pc->data->pwm_nums;

	platform_set_drvdata(pdev, pc);

	ret = pwmchip_add(&pc->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int mtk_pwm_remove(struct platform_device *pdev)
{
	struct mtk_pwm_chip *pc = platform_get_drvdata(pdev);

	return pwmchip_remove(&pc->chip);
}

static struct platform_driver mtk_pwm_driver = {
	.driver = {
		.name = "mtk-pwm",
		.of_match_table = mtk_pwm_of_match,
	},
	.probe = mtk_pwm_probe,
	.remove = mtk_pwm_remove,
};
MODULE_DEVICE_TABLE(of, mtk_pwm_of_match);

module_platform_driver(mtk_pwm_driver);

MODULE_AUTHOR("Zhi Mao <zhi.mao@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC PWM driver");
MODULE_LICENSE("GPL v2");
