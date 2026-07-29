#ifndef __DEV_I2C_H__
#define __DEV_I2C_H__

#define MTK_I2C_DRV_NAME	"i2c-mt7621"

struct mtk_i2c_platform_data {
	u32 bus_freq;
	u32 clk_src;
};

#endif /* __DEV_I2C_H__ */
