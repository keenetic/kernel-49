obj-$(CONFIG_MTD_RALINK)	+= ralink-flash.o
obj-$(CONFIG_MTD_SPI_RALINK)	+= rt_spi.o
obj-$(CONFIG_MTD_NAND_MT7621)	+= rt_nand.o
obj-$(CONFIG_MTD_SPINAND_ECONET)+= rt_nand.o

rt_spi-objs = ralink_spi_bbu.o

ifeq ($(CONFIG_RALINK_MT7621),y)
rt_nand-objs = mt7621_nand.o
endif

ifeq ($(CONFIG_MTD_SPINAND_ECONET),y)
EXTRA_CFLAGS += -DTCSUPPORT_NAND_BMT
rt_nand-objs += en75xx_nand/bmt.o
rt_nand-objs += en75xx_nand/spi_controller.o
ifneq ($(CONFIG_ECONET_EN7512),y)
EXTRA_CFLAGS += -DTCSUPPORT_SPI_CONTROLLER_ECC
EXTRA_CFLAGS += -DTCSUPPORT_SPI_NAND_FLASH_ECC_DMA
rt_nand-objs += en75xx_nand/spi_nfi.o
rt_nand-objs += en75xx_nand/spi_ecc.o
endif
rt_nand-objs += en75xx_nand/spi_nand_flash.o
rt_nand-objs += en75xx_nand/spi_nand_flash_table.o
endif
