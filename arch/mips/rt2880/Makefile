###############################################################################
#  Jan 2007 Bruce Chang
#
#  Initial Release
#
#
#
###############################################################################

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

obj-y := reset.o init.o memory.o printf.o cmdline.o setup.o time.o

ifdef CONFIG_MIPS_GIC
obj-y += irqchip-gic.o
else
ifdef CONFIG_IRQ_GIC
obj-y += irq-gic.o
else
obj-y += irq.o
endif
endif

obj-$(CONFIG_MIPS_CMP) += malta-amon.o

obj-$(CONFIG_PCI) += pci.o
obj-$(CONFIG_MTD_NAND_MT7621) += dev-nand.o
obj-$(CONFIG_RALINK_TIMER_DFS) += rt_timer_dfs.o
obj-$(CONFIG_RALINK_TIMER_WDG) += rt_timer_wdg.o

ifdef CONFIG_USB_EHCI_HCD_PLATFORM
obj-y += dev-ehci_ohci.o uphy.o
endif

ifdef CONFIG_USB_XHCI_MTK
obj-y += dev-xhci.o uphy.o
endif

ifdef CONFIG_MTK_MMC
obj-y += dev-mmc.o
endif

