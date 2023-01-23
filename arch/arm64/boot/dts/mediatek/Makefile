ifeq ($(CONFIG_MACH_MT7622),y)
dtb-$(CONFIG_ARCH_MEDIATEK) += mt7622-ac2600rfb1.dtb
else
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6755-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6795-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt8173-evb.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb
