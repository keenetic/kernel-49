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
rt_nand-objs += en75xx_nand/spi_nand_flash.o
endif
