#
# Makefile for the bus drivers.
#

# Interconnect bus drivers for ARM platforms
obj-$(CONFIG_ARM_CCI)		+= arm-cci.o
obj-$(CONFIG_ARM_CCN)		+= arm-ccn.o

obj-$(CONFIG_BRCMSTB_GISB_ARB)	+= brcmstb_gisb.o
obj-$(CONFIG_IMX_WEIM)		+= imx-weim.o
obj-$(CONFIG_MIPS_CDMM)		+= mips_cdmm.o
obj-$(CONFIG_MVEBU_MBUS) 	+= mvebu-mbus.o
obj-$(CONFIG_MTK_SUBSYS_IOC)	+= mtk-subsys-ioc.o
obj-$(CONFIG_ARCH_AIROHA)	+= airoha-pbus.o airoha-rbus.o mtk-rbus.o
obj-$(CONFIG_ARCH_MEDIATEK)	+= mtk-rbus.o

# Interconnect bus driver for OMAP SoCs.
obj-$(CONFIG_OMAP_INTERCONNECT)	+= omap_l3_smx.o omap_l3_noc.o

obj-$(CONFIG_OMAP_OCP2SCP)	+= omap-ocp2scp.o
obj-$(CONFIG_QCOM_EBI2)		+= qcom-ebi2.o
obj-$(CONFIG_SUNXI_RSB)		+= sunxi-rsb.o
obj-$(CONFIG_SIMPLE_PM_BUS)	+= simple-pm-bus.o
obj-$(CONFIG_TEGRA_ACONNECT)	+= tegra-aconnect.o
obj-$(CONFIG_UNIPHIER_SYSTEM_BUS)	+= uniphier-system-bus.o
obj-$(CONFIG_VEXPRESS_CONFIG)	+= vexpress-config.o
