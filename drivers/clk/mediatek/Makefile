obj-$(CONFIG_COMMON_CLK_MEDIATEK) += clk-mtk.o clk-pll.o clk-gate.o clk-apmixed.o clk-cpumux.o
obj-$(CONFIG_RESET_CONTROLLER) += reset.o
obj-$(CONFIG_COMMON_CLK_MT2701) += clk-mt2701.o
obj-$(CONFIG_COMMON_CLK_MT7622) += clk-mt7622.o clkchk-mt7622.o
obj-$(CONFIG_COMMON_CLK_MT8135) += clk-mt8135.o
obj-$(CONFIG_COMMON_CLK_MT8173) += clk-mt8173.o
obj-y += clk-bringup.o

ifeq ($(CONFIG_DEBUG_FS),y)
#obj-$(CONFIG_COMMON_CLK_MEDIATEK) += clkdbg.o
#obj-$(CONFIG_COMMON_CLK_MT7622) += clkdbg-mt7622.o
endif
