// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2023 Airoha

/*
 *  thermal.c - Generic Thermal Management Sysfs support.
 *  Copyright (C) 2023 Airoha
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/thermal.h>
#include <linux/reset.h>
#include <linux/types.h>

/* AN7552 */
#define AN7552_NUM_ZONES	2
#define AN7552_NUM_SENSORS	2

// SCU base: 0x1fa20000
#define AN7552_PLLRG_PROTECT	0x268
#define AN7552_PROTECT_KEY	0x80

// TADC
#define AN7552_MUX_TADC		0x2e4
#define AN7552_DOUT_TADC	0x2f0

// TADC_CPU
#define AN7552_MUX_TADC_CPU	0x2f4
#define AN7552_DOUT_TADC_CPU	0x2fc

#define AN7552_TADC_RST_OFST	0x4
#define AN7552_TADC_SET_MASK	0xf3ef
#define AN7552_TADC_MUX_OFST	0x1
#define AN7552_TADC_MUX_MASK	0x7

/* AN7581 */
#define AN7581_NUM_ZONES	1
#define AN7581_NUM_SENSORS	1

// SCU base: 0x1fa20000
#define AN7581_PLLRG_PROTECT	0x268
#define AN7581_PROTECT_KEY	0x12

// TADC
#define AN7581_MUX_TADC		0x2ec
#define AN7581_DOUT_TADC	0x2f8

// TADC_CPU
#define AN7581_MUX_TADC_CPU	0x2fc
#define AN7581_DOUT_TADC_CPU	0x308

#define AN7581_TADC_RST_OFST	0x4
#define AN7581_TADC_SET_MASK	0xf3ef
#define AN7581_TADC_MUX_OFST	0x1
#define AN7581_TADC_MUX_MASK	0x07

/* AN7583 */
#define AN7583_NUM_ZONES	1
#define AN7583_NUM_SENSORS	3

// SCU base: 0x1fa20000
#define AN7583_PLLRG_PROTECT	0x268
#define AN7583_PROTECT_KEY	0x80

// TADC
#define AN7583_MUX_SENSOR	0x2a0
#define AN7583_MUX_TADC		0x2e4
#define AN7583_DOUT_TADC	0x2f0

#define AN7583_TADC_RST_OFST	0x4
#define AN7583_TADC_SET_MASK	0x1ffff
#define AN7583_TADC_MUX_OFST	0x1
#define AN7583_TADC_MUX_MASK	0x7
#define AN7583_LOD_ADJ_OFST	0x2
#define AN7583_LOD_ADJ_MASK	0x3

#define THERMAL_DRIVER_VERSION	"1.0.240605"

#define MAX_NUM_ZONES		4

enum {
	VTS1,
	VTS2,
	VTS3,
	VTS4,
	VTS5,
	MAX_NUM_VTS,
};

/* AN7552 */
static const int an7552_SLOPE_X100_DEFAULT[AN7552_NUM_SENSORS] = { 5565, 6059, };
static const int an7552_initTemperature_CPK_x10[AN7552_NUM_SENSORS] = { 300, 300, };
static const int an7552_initTemperature_NONK_x10[AN7552_NUM_SENSORS] = { 550, 550, };
static const int an7552_BIAS[AN7552_NUM_SENSORS] = { 0, 0, };
static const int an7552_bank_data[AN7552_NUM_SENSORS] = { VTS1, VTS2 };

/* AN7581 */
static const int an7581_SLOPE_7581_X100_DIO_DEFAULT[AN7581_NUM_SENSORS] = { 5710, };
static const int an7581_SLOPE_7581_X100_DIO_AVS[AN7581_NUM_SENSORS] = { 5645, };
static const int an7581_initTemperature_CPK_x10[AN7581_NUM_SENSORS] = { 300, };
static const int an7581_initTemperature_FTK_x10[AN7581_NUM_SENSORS] = { 620, };
static const int an7581_initTemperature_NONK_x10[AN7581_NUM_SENSORS] = { 550, };
static const int an7581_BIAS[AN7581_NUM_SENSORS] = { 0, };
static const int an7581_bank_data[AN7581_NUM_SENSORS] = { VTS1 };

/* AN7583 */
static const int an7583_SLOPE_X100_DEFAULT[AN7583_NUM_SENSORS] = { 7440, 7620, 8390, };
static const int an7583_bank_data[AN7583_NUM_SENSORS] = { 0, 5, 6 };
static const int an7583_dbg_coeff[AN7583_NUM_SENSORS] = { 973, 995, 1035, };
static const int an7583_dbg_offset[AN7583_NUM_SENSORS] = { 294, 298, 344, };

enum AN7583_TADC_MUX_SEL {
	AN7583_BGP_TEMP_SENSOR = 0,
	AN7583_GBE_TEMP_SENSOR = 5,
	AN7583_CPU_TEMP_SENSOR = 6,
};

enum AN7583_DIO_MUX_SEL {
	AN7583_D0_TADC = 0,
	AN7583_Zero_TADC = 1,
	AN7583_D1_TADC = 2,
};

struct airoha_thermal;

struct airoha_thermal_bank {
	struct airoha_thermal *at;
	int id;
};

struct thermal_bank_cfg {
	const int *sensors;
	u32 num_sensors;
};

struct airoha_thermal_data {
	void (*init)(struct airoha_thermal *at);
	int (*get_temp)(struct airoha_thermal_bank *bank);
	struct thermal_bank_cfg bank_data[MAX_NUM_ZONES];
	u32 DUMMY_REG_OSFT;
	u32 DUMMY_REG_1_OSFT;
	u32 DUMMY_REG_2_OSFT;
	s32 num_banks;
	s32 num_sensors;
};

struct airoha_thermal {
	struct device *dev;
	void __iomem *scu_base;
	void __iomem *ptp_base;
	const u32 *SLOPE_X100;
	const u32 *bias;
	const u32 *initTemperature_x10;
	const u32 *dbg_coef;
	u32 offset[MAX_NUM_VTS];
	u32 thermal_efuse_valid;
	struct mutex lock;
	const struct airoha_thermal_data *conf;
	struct airoha_thermal_bank banks[MAX_NUM_ZONES];
};

static inline u32 get_ptp_dummy(struct airoha_thermal *at)
{
	return readl(at->ptp_base + at->conf->DUMMY_REG_OSFT);
}

static inline u32 get_ptp_dummy1(struct airoha_thermal *at)
{
	return readl(at->ptp_base + at->conf->DUMMY_REG_1_OSFT);
}

static inline u32 get_ptp_dummy2(struct airoha_thermal *at)
{
	return readl(at->ptp_base + at->conf->DUMMY_REG_2_OSFT);
}

static void _TADC_reset(struct airoha_thermal *at, u32 tadc_reg, u32 tadc_ofs)
{
	u32 reg;

	reg = readl(at->scu_base + tadc_reg);
	reg &= ~(1u << tadc_ofs);
	writel(reg, at->scu_base + tadc_reg);

	reg = readl(at->scu_base + tadc_reg);
	reg |= (1u << tadc_ofs);
	writel(reg, at->scu_base + tadc_reg);
}

static void _TADC_init(struct airoha_thermal *at, u32 tadc_reg, u32 mask, u32 data)
{
	u32 reg;

	reg = readl(at->scu_base + tadc_reg);
	reg &= ~mask;
	reg |=  data;
	writel(reg, at->scu_base + tadc_reg);
}

/* AN7552 */
static inline void an7552_TADC_init(struct airoha_thermal *at)
{
	u32 tmp_key;

	/* enable write protect reg */
	tmp_key = readl(at->scu_base + AN7552_PLLRG_PROTECT);
	writel(AN7552_PROTECT_KEY, at->scu_base + AN7552_PLLRG_PROTECT);

	/* reset TADC, [4] -> 0, [4] -> 1 */
	_TADC_reset(at, AN7552_MUX_TADC, AN7552_TADC_RST_OFST);

	/* set TADC initional mode */
	_TADC_init(at, AN7552_MUX_TADC, AN7552_TADC_SET_MASK, 0x33be);

	/* reset TADC_CPU, [4] -> 0, [4] -> 1 */
	_TADC_reset(at, AN7552_MUX_TADC_CPU, AN7552_TADC_RST_OFST);

	/* set TADC_CPU initional mode */
	_TADC_init(at, AN7552_MUX_TADC_CPU, AN7552_TADC_SET_MASK, 0x33be);

	writel(tmp_key, at->scu_base + AN7552_PLLRG_PROTECT);
}

static void an7552_TADC_mode(struct airoha_thermal *at, bool tadc0_tcpu,
			     u8 res0_diode, u8 delay_cnt)
{
	u32 addr = AN7552_MUX_TADC;
	u32 reg, tmp_key;

	if (tadc0_tcpu)
		addr = AN7552_MUX_TADC_CPU;

	/* enable write protect reg */
	tmp_key = readl(at->scu_base + AN7552_PLLRG_PROTECT);
	writel(AN7552_PROTECT_KEY, at->scu_base + AN7552_PLLRG_PROTECT);

	reg = readl(at->scu_base + addr);
	reg &= ~(AN7552_TADC_MUX_MASK << AN7552_TADC_MUX_OFST);
	if (res0_diode == 1)
		reg |= (0x7 << AN7552_TADC_MUX_OFST);
	else if (res0_diode == 2)
		reg |= (0x6 << AN7552_TADC_MUX_OFST);
	writel(reg, at->scu_base + addr);

	writel(tmp_key, at->scu_base + AN7552_PLLRG_PROTECT);

	msleep(delay_cnt);
}

static u32 an7552_TADC_out(struct airoha_thermal *at, bool tadc0_tcpu)
{
	u32 addr = AN7552_DOUT_TADC;

	if (tadc0_tcpu)
		addr = AN7552_DOUT_TADC_CPU;

	return readl(at->scu_base + addr);
}

static void an7552_thermal_init(struct airoha_thermal *at)
{
	u32 offset_array[AN7552_NUM_SENSORS] = {0, 0};
	const u32 *target_offset = offset_array;
	u32 offset_efuse = get_ptp_dummy(at);
	u32 i;

	an7552_TADC_init(at);

	at->thermal_efuse_valid = (offset_efuse == 0) ? 0 : 1;

	an7552_TADC_mode(at, false, 1, 10);

	at->SLOPE_X100 = an7552_SLOPE_X100_DEFAULT;

	if (at->thermal_efuse_valid) {
		offset_array[0] = (offset_efuse & 0x0000ffff);
		offset_array[1] = (offset_efuse & 0xffff0000) >> 16;
		at->initTemperature_x10 = an7552_initTemperature_CPK_x10;
	} else {
		offset_array[0] = an7552_TADC_out(at, false);
		an7552_TADC_mode(at, false, 2, 10);
		offset_array[1] = an7552_TADC_out(at, false);
		an7552_TADC_mode(at, false, 1, 10);
		at->initTemperature_x10 = an7552_initTemperature_NONK_x10;
	}

	at->bias = an7552_BIAS;

	for (i = 0; i < AN7552_NUM_SENSORS; i++)
		at->offset[i] = target_offset[i] + at->bias[i];

	dev_info(at->dev, "AN7552 init, IC is %scalibrated, check code_30: %d, slope: %d",
		 at->thermal_efuse_valid ? "" : "not ",
		 at->offset[0], at->SLOPE_X100[0]);
}

static int an7552_thermal_get_temp(struct airoha_thermal_bank *bank)
{
	struct airoha_thermal *at = bank->at;
	u32 sensor_index = at->conf->bank_data->sensors[bank->id];
	u32 ADC_code_30 = at->offset[sensor_index];
	s32 SLOPE_X100 = (s32)at->SLOPE_X100[sensor_index];
	int initTemperature_x10 = at->initTemperature_x10[sensor_index];
	int temp_x10;
	int min, max, avgTemp, temp_ADC;
	u32 getIndex;

	an7552_TADC_mode(at, false, 1 + sensor_index, 10);

	min = max = avgTemp = temp_ADC = an7552_TADC_out(at, false);

	for (getIndex = 1; getIndex < 6; getIndex++) {
		temp_ADC = an7552_TADC_out(at, false);
		avgTemp += temp_ADC;

		if (temp_ADC > max)
			max = temp_ADC;
		else if (temp_ADC < min)
			min = temp_ADC;
	}

	avgTemp = avgTemp - max - min;
	avgTemp = avgTemp >> 2;
	temp_x10 = 1000 * ((signed int)(avgTemp - ADC_code_30)) / SLOPE_X100;
	temp_x10 += initTemperature_x10;

	return temp_x10 * 100;
}

struct airoha_thermal_data an7552_thermal_data ={
	.init = an7552_thermal_init,
	.get_temp = an7552_thermal_get_temp,
	.num_banks = AN7552_NUM_ZONES,
	.num_sensors = AN7552_NUM_SENSORS,
	.DUMMY_REG_OSFT = 0xf20,
	.DUMMY_REG_1_OSFT = 0xf24,
	.DUMMY_REG_2_OSFT = 0xf28,
	.bank_data = {
		{
			.num_sensors = 1,
			.sensors = an7552_bank_data,
		},
	},
};

/* AN7581 */
static inline void an7581_TADC_init(struct airoha_thermal *at)
{
	u32 tmp_key;

	/* enable write protect reg */
	tmp_key = readl(at->scu_base + AN7581_PLLRG_PROTECT);
	writel(AN7581_PROTECT_KEY, at->scu_base + AN7581_PLLRG_PROTECT);

	/* reset TADC, [4] -> 0, [4] -> 1 */
	_TADC_reset(at, AN7581_MUX_TADC, AN7581_TADC_RST_OFST);

	/* set TADC mode */
	_TADC_init(at, AN7581_MUX_TADC, AN7581_TADC_SET_MASK, 0x0390);

	/* reset TADC_CPU, [4] -> 0, [4] -> 1 */
	_TADC_reset(at, AN7581_MUX_TADC_CPU, AN7581_TADC_RST_OFST);

	/* set TADC_CPU mode */
	_TADC_init(at, AN7581_MUX_TADC_CPU, AN7581_TADC_SET_MASK, 0x0390);

	writel(tmp_key, at->scu_base + AN7581_PLLRG_PROTECT);
}

static void an7581_TADC_mode(struct airoha_thermal *at, bool tadc0_tcpu,
			     bool res0_diode, u8 delay_cnt)
{
	u32 addr = AN7581_MUX_TADC;
	u32 reg, tmp_key;

	if (tadc0_tcpu)
		addr = AN7581_MUX_TADC_CPU;

	/* enable write protect reg */
	tmp_key = readl(at->scu_base + AN7581_PLLRG_PROTECT);
	writel(AN7581_PROTECT_KEY, at->scu_base + AN7581_PLLRG_PROTECT);

	reg = readl(at->scu_base + addr);
	reg &= ~(AN7581_TADC_MUX_MASK << AN7581_TADC_MUX_OFST);
	if (res0_diode)
		reg |= (0x7 << AN7581_TADC_MUX_OFST);
	writel(reg, at->scu_base + addr);

	writel(tmp_key, at->scu_base + AN7581_PLLRG_PROTECT);

	msleep(delay_cnt);
}

static u32 an7581_TADC_out(struct airoha_thermal *at, bool tadc0_tcpu)
{
	u32 addr = AN7581_DOUT_TADC;

	if (tadc0_tcpu)
		addr = AN7581_DOUT_TADC_CPU;

	return readl(at->scu_base + addr);
}

static void an7581_thermal_init(struct airoha_thermal *at)
{
	u32 offset_array[AN7581_NUM_SENSORS] = {0};
	const u32 *target_offset = offset_array;
	u32 offset_efuse = get_ptp_dummy(at);
	u32 i;

	an7581_TADC_init(at);

	at->thermal_efuse_valid = (offset_efuse == 0) ? 0 : 1;

	if (at->thermal_efuse_valid) {
		offset_array[0] = (offset_efuse & 0xffff0000) >> 16;

		if (get_ptp_dummy2(at) == 0) {
			at->SLOPE_X100 = an7581_SLOPE_7581_X100_DIO_AVS;
			at->initTemperature_x10 = an7581_initTemperature_CPK_x10;
		} else {
			at->SLOPE_X100 = an7581_SLOPE_7581_X100_DIO_DEFAULT;
			at->initTemperature_x10 = an7581_initTemperature_FTK_x10;
		}
	} else {
		an7581_TADC_mode(at, false, true, 10);
		offset_array[0] = an7581_TADC_out(at, false);
		at->SLOPE_X100 = an7581_SLOPE_7581_X100_DIO_DEFAULT;
		at->initTemperature_x10 = an7581_initTemperature_NONK_x10;
	}

	at->bias = an7581_BIAS;

	for (i = 0; i < AN7581_NUM_SENSORS; i++)
		at->offset[i] = target_offset[i] + at->bias[i];

	dev_info(at->dev, "AN7581 init, IC is %scalibrated, check code_30: %d, slope: %d",
		 at->thermal_efuse_valid ? "" : "not ",
		 at->offset[0], at->SLOPE_X100[0]);
}

static int an7581_thermal_get_temp(struct airoha_thermal_bank *bank)
{
	struct airoha_thermal *at = bank->at;
	u32 sensor_index = at->conf->bank_data->sensors[bank->id];
	u32 ADC_code_30 = at->offset[sensor_index];
	s32 SLOPE_X100 = (s32)at->SLOPE_X100[sensor_index];
	int initTemperature_x10 = at->initTemperature_x10[sensor_index];
	int temp_x10;
	int min, max, avgTemp, temp_ADC;
	u32 getIndex;

	an7581_TADC_mode(at, false, true, 10);

	min = max = avgTemp = temp_ADC = an7581_TADC_out(at, false);

	for (getIndex = 1; getIndex < 6; getIndex++) {
		temp_ADC = an7581_TADC_out(at, false);
		avgTemp += temp_ADC;

		if (temp_ADC > max)
			max = temp_ADC;
		else if (temp_ADC < min)
			min = temp_ADC;
	}

	avgTemp = avgTemp - max - min;
	avgTemp = avgTemp >> 2;
	temp_x10 = 1000 * ((signed int)(avgTemp - ADC_code_30)) / SLOPE_X100;
	temp_x10 += initTemperature_x10;

	return temp_x10 * 100;
}

struct airoha_thermal_data an7581_thermal_data = {
	.init = an7581_thermal_init,
	.get_temp = an7581_thermal_get_temp,
	.num_banks = AN7581_NUM_ZONES,
	.num_sensors = AN7581_NUM_SENSORS,
	.DUMMY_REG_OSFT = 0xf20,
	.DUMMY_REG_1_OSFT = 0xf24,
	.DUMMY_REG_2_OSFT = 0xf28,
	.bank_data = {
		{
			.num_sensors = 1,
			.sensors = an7581_bank_data,
		},
	},
};

/* AN7583 */
static inline void an7583_TADC_init(struct airoha_thermal *at)
{
	u32 tmp_key;

	/* enable write protect reg */
	tmp_key = readl(at->scu_base + AN7583_PLLRG_PROTECT);
	writel(AN7583_PROTECT_KEY, at->scu_base + AN7583_PLLRG_PROTECT);

	/* reset TADC, [4] -> 0, [4] -> 1 */
	_TADC_reset(at, AN7583_MUX_TADC, AN7583_TADC_RST_OFST);

	/* set TADC mode */
	_TADC_init(at, AN7583_MUX_TADC, AN7583_TADC_SET_MASK, 0x33bc);

	writel(tmp_key, at->scu_base + AN7583_PLLRG_PROTECT);
}

static void an7583_TADC_mode(struct airoha_thermal *at, u8 adc_sel,
			     u8 sens_sel, u8 delay_cnt)
{
	u32 reg, mux_sens, mux_tadc;
	u32 tmp_key;
	bool need_change_mux_tadc = false;
	bool need_change_mux_sens = false;

	switch (adc_sel) {
	case AN7583_GBE_TEMP_SENSOR:
		mux_tadc = 0x5;
		break;
	case AN7583_CPU_TEMP_SENSOR:
		mux_tadc = 0x6;
		break;
	default:
		mux_tadc = 0x0;
		break;
	}

	switch (sens_sel) {
	case AN7583_Zero_TADC:
		mux_sens = 0x1;
		break;
	case AN7583_D1_TADC:
		mux_sens = 0x2;
		break;
	default:
		mux_sens = 0x0;
		break;
	}

	reg = readl(at->scu_base + AN7583_MUX_SENSOR) >> AN7583_LOD_ADJ_OFST;
	reg &= AN7583_LOD_ADJ_MASK;

	if (reg != mux_sens)
		need_change_mux_sens = true;

	reg = readl(at->scu_base + AN7583_MUX_TADC) >> AN7583_TADC_MUX_OFST;
	reg &= AN7583_TADC_MUX_MASK;

	if (reg != mux_tadc)
		need_change_mux_tadc = true;

	if (!need_change_mux_tadc && !need_change_mux_sens)
		return;

	/* enable write protect reg */
	tmp_key = readl(at->scu_base + AN7583_PLLRG_PROTECT);
	writel(AN7583_PROTECT_KEY, at->scu_base + AN7583_PLLRG_PROTECT);

	if (need_change_mux_sens) {
		reg = readl(at->scu_base + AN7583_MUX_SENSOR);
		reg &= ~(AN7583_LOD_ADJ_MASK << AN7583_LOD_ADJ_OFST);
		reg |=  (mux_sens << AN7583_LOD_ADJ_OFST);
		writel(reg, at->scu_base + AN7583_MUX_SENSOR);
	}

	if (need_change_mux_tadc) {
		reg = readl(at->scu_base + AN7583_MUX_TADC);
		reg &= ~(AN7583_TADC_MUX_MASK << AN7583_TADC_MUX_OFST);
		reg |=  (mux_tadc << AN7583_TADC_MUX_OFST);
		writel(reg, at->scu_base + AN7583_MUX_TADC);
	}

	writel(tmp_key, at->scu_base + AN7583_PLLRG_PROTECT);

	msleep(delay_cnt);
}

static u32 an7583_TADC_out(struct airoha_thermal *at)
{
	return readl(at->scu_base + AN7583_DOUT_TADC);
}

static void an7583_thermal_init(struct airoha_thermal *at)
{
	u32 i;

	at->thermal_efuse_valid = 0;

	an7583_TADC_init(at);
	an7583_TADC_mode(at, AN7583_CPU_TEMP_SENSOR, AN7583_D1_TADC, 10);

	at->dbg_coef = an7583_dbg_coeff;
	at->SLOPE_X100 = an7583_SLOPE_X100_DEFAULT;

	for (i = 0; i < AN7583_NUM_SENSORS; i++)
		at->offset[i] = an7583_dbg_offset[i];

	dev_info(at->dev, "AN7583 init, check BGP coeffs: %d, %d, %d\n",
		 at->dbg_coef[0], at->SLOPE_X100[0], at->offset[0]);
}

static int an7583_thermal_get_temp(struct airoha_thermal_bank *bank)
{
	struct airoha_thermal *at = bank->at;
	u32 sensor_index = at->conf->bank_data->sensors[bank->id];
	int BG_COEF_x100, Tgain, Toffset, tadc_Mux;
	int deltaD, zero, d1, d0;
	int dbg, temp_x10;

	if (sensor_index == AN7583_BGP_TEMP_SENSOR)
		tadc_Mux = sensor_index;
	else
		tadc_Mux = sensor_index + 4; // 1->5, 2->6

	BG_COEF_x100 = at->dbg_coef[sensor_index];
	Tgain = (s32)at->SLOPE_X100[sensor_index];
	Toffset = at->offset[sensor_index];

	mutex_lock(&at->lock);
	an7583_TADC_mode(at, tadc_Mux, AN7583_Zero_TADC, 10);
	zero = an7583_TADC_out(at);
	an7583_TADC_mode(at, tadc_Mux, AN7583_D0_TADC, 10);
	d0 = an7583_TADC_out(at);
	an7583_TADC_mode(at, tadc_Mux, AN7583_D1_TADC, 10);
	d1 = an7583_TADC_out(at);
	mutex_unlock(&at->lock);

	deltaD = (d1 - d0);

	dbg = (deltaD * BG_COEF_x100) / 100 + (zero - d1);
	temp_x10 = (Tgain * deltaD * 10) / dbg - Toffset * 10;

	return temp_x10 * 100;
}

struct airoha_thermal_data an7583_thermal_data = {
	.init = an7583_thermal_init,
	.get_temp = an7583_thermal_get_temp,
	.num_banks = AN7583_NUM_ZONES,
	.num_sensors = AN7583_NUM_SENSORS,
	.DUMMY_REG_OSFT = 0xf20,
	.DUMMY_REG_1_OSFT = 0xf24,
	.DUMMY_REG_2_OSFT = 0xf28,
	.bank_data = {
		{
			.num_sensors = 1,
			.sensors = an7583_bank_data,
		},
	},
};

static const struct of_device_id airoha_thermal_phy_match[] = {
	{ .compatible = "airoha,an7552-thermal_phy",
	  .data =  (void *)&an7552_thermal_data },
	{ .compatible = "airoha,an7581-thermal_phy",
	  .data =  (void *)&an7581_thermal_data },
	{ .compatible = "airoha,an7583-thermal_phy",
	  .data =  (void *)&an7583_thermal_data },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, airoha_thermal_phy_match);

static int airoha_read_temp(void *data, int *temperature)
{
	struct airoha_thermal *at = data;
	int tempmax = INT_MIN;
	int i;

	for (i = 0; i < at->conf->num_banks; i++) {
		struct airoha_thermal_bank *bank = &at->banks[i];

		tempmax = max(tempmax, at->conf->get_temp(bank));
	}

	*temperature = tempmax;

	return 0;
}

static const struct thermal_zone_of_device_ops airoha_thermal_ops = {
	.get_temp = airoha_read_temp,
};

static void airoha_thermal_init_bank(struct airoha_thermal *at, int num)
{
	struct airoha_thermal_bank *bank = &at->banks[num];

	bank->id = num;
	bank->at = at;
}

static int airoha_thermal_probe(struct platform_device *pdev)
{
	struct airoha_thermal *at;
	struct thermal_zone_device *tzdev;
	struct device_node *ns;
	struct resource *res, _res;
	int i, ret;

	at = devm_kzalloc(&pdev->dev, sizeof(*at), GFP_KERNEL);
	if (!at)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	at->ptp_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(at->ptp_base))
		return PTR_ERR(at->ptp_base);

	ns = of_parse_phandle(pdev->dev.of_node, "airoha,scu", 0);
	if (!ns) {
		dev_err(&pdev->dev, "SCU node not found\n");
		return -ENODEV;
	}

	/* map chip SCU */
	res = &_res;
	ret = of_address_to_resource(ns, 1, res);
	of_node_put(ns);

	if (ret)
		return ret;

	at->scu_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!at->scu_base)
		return -ENOMEM;

	mutex_init(&at->lock);

	at->dev = &pdev->dev;
	at->conf = of_device_get_match_data(&pdev->dev);
	at->conf->init(at);

	for (i = 0; i < at->conf->num_banks; i++)
		airoha_thermal_init_bank(at, i);

	platform_set_drvdata(pdev, at);

	tzdev = devm_thermal_zone_of_sensor_register(&pdev->dev, 0, at,
						     &airoha_thermal_ops);
	if (IS_ERR(tzdev))
		return PTR_ERR(tzdev);

	return 0;
}

static struct platform_driver airoha_thermal_phy_driver = {
	.probe = airoha_thermal_probe,
	.driver = {
		.name = "airoha-thermal",
		.of_match_table = airoha_thermal_phy_match,
	},
};

module_platform_driver(airoha_thermal_phy_driver);

MODULE_AUTHOR("Elijah Yu <elijah.yu@airoha.com>");
MODULE_DESCRIPTION("Airoha thermal driver");
MODULE_LICENSE("GPL v2");
