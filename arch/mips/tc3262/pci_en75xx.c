#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <asm/pci.h>
#include <asm/io.h>
#include <asm/irq.h>

#ifdef CONFIG_PCI
#include <asm/tc3162/rt_mmap.h>
#include <asm/tc3162/surfboardint.h>
#include <asm/tc3162/tc3162.h>

#define ECONET_RSTCTRL_REG		(RALINK_SYSCTL_BASE + 0x834)
#define ECONET_PCIEC_REG		(RALINK_SYSCTL_BASE + 0x088)

#define ECONET_PCIE_ERR_IRQ_EN 		(RALINK_PCI_BASE + 0x0040)
#define ECONET_PCIE_LINKUP		(RALINK_PCI_BASE + 0x0050)
#define ECONET_PCIE0_MAC_BASE		(RALINK_PCI_BASE + 0x1000)
#define ECONET_PCIE1_MAC_BASE		(RALINK_PCI_BASE + 0x3000)

#define ECONET_PCI_MM_MAP_BASE		0x20000000
#define ECONET_PCI_IO_MAP_BASE		0x1F600000

static int pcie_link_status;
static DEFINE_SPINLOCK(asic_pcr_lock);

static u32 en75xx_get_rc_port(u32 busn, u32 slot)
{
	u32 rc = 2;

	if (busn == 0 && slot < 2)
		rc = slot;
	else if (busn == 1 && slot == 0 && (pcie_link_status & 0x1))
		rc = 0;
	else if (busn == 2 && slot == 0 && (pcie_link_status & 0x2))
		rc = 1;

	return rc;
}

static int
en75xx_write_config_word(u32 busn, u32 slot, u32 func, u32 reg, u32 value)
{
	u32 val, rc, mac_offset;

	rc = en75xx_get_rc_port(busn, slot);
	if (rc == 0)
		mac_offset = ECONET_PCIE0_MAC_BASE;
	else if (rc == 1)
		mac_offset = ECONET_PCIE1_MAC_BASE;
	else
		return -1;

	/* fmt=2 | type=4 | length=1 */
	val = (2 << 29) | (4 << 24) | 1;

	/* write TLP header offset 0..3 */
	sysRegWrite(mac_offset + 0x0460, val);

	/* write requester ID */
	val = (rc << 19) | 0x070f;

	/* write TLP header offset 4..7 */
	sysRegWrite(mac_offset + 0x0464, val);

	val = (busn << 24) | (slot << 19) | (func << 16) | (reg & 0xffc);

	/* write TLP header offset 8..11 */
	sysRegWrite(mac_offset + 0x0468, val);

	/* write TLP data */
	sysRegWrite(mac_offset + 0x0470, value);

	/* start TLP request */
	sysRegWrite(mac_offset + 0x0488, 0x1);

	udelay(10);

	/* polling TLP request status (max 10ms) */
	val = 1000;
	while ((val--) > 0) {
		/* TLP request finished or timeout */
		if ((sysRegRead(mac_offset + 0x0488) & 0x1) == 0)
			break;
		udelay(10);
	}

	if (val == 0) {
		printk(KERN_ERR "PCR %s err: bus = %d, dev = %d, func = %d, reg = 0x%08X\n",
			"write", busn, slot, func, reg);
		return -1;
	}

	return 0;
}

static u32
en75xx_read_config_word(u32 busn, u32 slot, u32 func, u32 reg)
{
	u32 val, rc, mac_offset;

	rc = en75xx_get_rc_port(busn, slot);
	if (rc == 0)
		mac_offset = ECONET_PCIE0_MAC_BASE;
	else if (rc == 1)
		mac_offset = ECONET_PCIE1_MAC_BASE;
	else
		return 0xffffffff;

	/* initialize the data reg */
	sysRegWrite(mac_offset + 0x048c, 0xffffffff);

	/* fmt=0 | type=4 | length=1 */
	val = (4 << 24) | 1;

	/* write TLP header offset 0..3 */
	sysRegWrite(mac_offset + 0x0460, val);

	/* write requester ID */
	val = (rc << 19) | 0x070f;

	/* write TLP header offset 4..7 */
	sysRegWrite(mac_offset + 0x0464, val);

	val = (busn << 24) | (slot << 19) | (func << 16) | (reg & 0xffc);

	/* write TLP header offset 8..11 */
	sysRegWrite(mac_offset + 0x0468, val);

	/* start TLP requuest */
	sysRegWrite(mac_offset + 0x0488, 0x1);

	udelay(10);

	/* polling TLP request status (max 10ms) */
	val = 1000;
	while ((val--) > 0) {
		/* TLP request finished or timeout */
		if ((sysRegRead(mac_offset + 0x0488) & 0x1) == 0)
			break;
		udelay(10);
	}

	if (val == 0) {
		printk(KERN_ERR "PCR %s err: bus = %d, dev = %d, func = %d, reg = 0x%08X\n",
			"read", busn, slot, func, reg);
		return 0xffffffff;
	}

	/* return the data from data reg */
	return sysRegRead(mac_offset + 0x048c);
}

static int
en75xx_pci_config_read(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *val)
{
	u32 busn = bus->number;
	u32 slot = PCI_SLOT(devfn);
	u32 func = PCI_FUNC(devfn);
	u32 shift, reg, tmp;
	unsigned long flags;

	reg = (u32)where;

	spin_lock_irqsave(&asic_pcr_lock, flags);

	tmp = en75xx_read_config_word(busn, slot, func, reg);

	spin_unlock_irqrestore(&asic_pcr_lock, flags);

	switch (size) {
	case 1:
		shift = (reg & 0x3) << 3;
		*val = (tmp >> shift) & 0xff;
		break;
	case 2:
		shift = (reg & 0x2) << 3;
		*val = (tmp >> shift) & 0xffff;
		break;
	default:
		*val = tmp;
		break;
	}

	return PCIBIOS_SUCCESSFUL;
}

static int
en75xx_pci_config_write(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 val)
{
	u32 busn = bus->number;
	u32 slot = PCI_SLOT(devfn);
	u32 func = PCI_FUNC(devfn);
	u32 shift, reg, tmp;
	unsigned long flags;

	reg = (u32)where;

	spin_lock_irqsave(&asic_pcr_lock, flags);

	switch (size) {
	case 1:
		tmp = en75xx_read_config_word(busn, slot, func, reg);
		shift = (reg & 0x3) << 3;
		tmp &= ~(0xff << shift);
		tmp |= ((val & 0xff) << shift);
		break;
	case 2:
		tmp = en75xx_read_config_word(busn, slot, func, reg);
		shift = (reg & 0x2) << 3;
		tmp &= ~(0xffff << shift);
		tmp |= ((val & 0xffff) << shift);
		break;
	default:
		tmp = val;
		break;
	}

	en75xx_write_config_word(busn, slot, func, reg, tmp);

	spin_unlock_irqrestore(&asic_pcr_lock, flags);

	return PCIBIOS_SUCCESSFUL;
}

/*
 *  General-purpose PCI functions.
 */

struct pci_ops en75xx_pci_ops = {
	.read		 = en75xx_pci_config_read,
	.write		 = en75xx_pci_config_write,
};

static struct resource en75xx_res_pci_mem1 = {
	.name		 = "PCI MEM1",
	.start		 = ECONET_PCI_MM_MAP_BASE,
	.end		 = ECONET_PCI_MM_MAP_BASE + 0x0fffffff,
	.flags		 = IORESOURCE_MEM,
};

static struct resource en75xx_res_pci_io1 = {
	.name		 = "PCI I/O1",
	.start		 = ECONET_PCI_IO_MAP_BASE,
	.end		 = ECONET_PCI_IO_MAP_BASE + 0xffff,
	.flags		 = IORESOURCE_IO,
};

struct pci_controller en75xx_pci_controller = {
	.pci_ops	 = &en75xx_pci_ops,
	.mem_resource	 = &en75xx_res_pci_mem1,
	.io_resource	 = &en75xx_res_pci_io1,
	.mem_offset	 = 0x00000000UL,
	.io_offset	 = 0x00000000UL,
};

#if defined(CONFIG_ECONET_EN7516) || defined(CONFIG_PCIE_PORT1)
static u32 en75xx_pcie_get_pos(u32 busn, u32 slot)
{
	u32 val, pos;

	val = en75xx_read_config_word(busn, slot, 0, 0x34);
	pos = val & 0xff;

	while (pos && pos != 0xff) {
		val = en75xx_read_config_word(busn, slot, 0, pos);
		if ((val & 0xff) == 0x10)
			return pos;

		pos = (val >> 0x08) & 0xff;
	}

	return 0;
}
#endif

static inline void en75xx_pcie_rc0_retrain(void)
{
#ifdef CONFIG_ECONET_EN7516
	u32 pos, ppos;
	u32 linkcap, plinkcap, plinksta[2];
	unsigned long flags;

	spin_lock_irqsave(&asic_pcr_lock, flags);

	ppos = en75xx_pcie_get_pos(0, 0);
	pos  = en75xx_pcie_get_pos(1, 0);

	if (pos < 0x40 || ppos < 0x40)
		goto exit_unlock;

	plinkcap = en75xx_read_config_word(0, 0, 0, ppos + 0x0c);
	linkcap  = en75xx_read_config_word(1, 0, 0,  pos + 0x0c);

	if ((linkcap & 0x0f) == 1 || (plinkcap & 0x0f) == 1)
		goto exit_unlock;

	plinksta[0] = en75xx_read_config_word(0, 0, 0, ppos + 0x10);
	if (((plinksta[0] >> 16) & 0x0f) == (plinkcap & 0x0f))
		goto exit_unlock;

	/* train link Gen1 -> Gen2 */
	plinksta[0] = en75xx_read_config_word(0, 0, 0, ppos + 0x10);
	plinksta[0] |= 0x20;
	en75xx_write_config_word(0, 0, 0, ppos + 0x10, plinksta[0]);

	spin_unlock_irqrestore(&asic_pcr_lock, flags);

	msleep(250);

	spin_lock_irqsave(&asic_pcr_lock, flags);
	plinksta[1] = en75xx_read_config_word(0, 0, 0, ppos + 0x10);

	printk(KERN_INFO "PCIe RC0 link trained: %x -> %x\n",
		((plinksta[0] >> 16) & 0x0f), ((plinksta[1] >> 16) & 0x0f));

exit_unlock:
	spin_unlock_irqrestore(&asic_pcr_lock, flags);
#endif
}

#ifdef CONFIG_PCIE_PORT1
static inline void en75xx_pcie_rc1_retrain(void)
{
	u32 pos, ppos;
	u32 linkcap, plinkcap, plinksta[2];
	unsigned long flags;

	spin_lock_irqsave(&asic_pcr_lock, flags);

	ppos = en75xx_pcie_get_pos(0, 1);
	pos  = en75xx_pcie_get_pos(2, 0);

	if (pos < 0x40 || ppos < 0x40)
		goto exit_unlock;

	plinkcap = en75xx_read_config_word(0, 1, 0, ppos + 0x0c);
	linkcap  = en75xx_read_config_word(2, 0, 0,  pos + 0x0c);

	if ((linkcap & 0x0f) == 1 || (plinkcap & 0x0f) == 1)
		goto exit_unlock;

	plinksta[0] = en75xx_read_config_word(0, 1, 0, ppos + 0x10);
	if (((plinksta[0] >> 16) & 0x0f) == (plinkcap & 0x0f))
		goto exit_unlock;

	/* train link Gen1 -> Gen2 */
	plinksta[0] = en75xx_read_config_word(0, 1, 0, ppos + 0x10);
	plinksta[0] |= 0x20;
	en75xx_write_config_word(0, 1, 0, ppos + 0x10, plinksta[0]);

	spin_unlock_irqrestore(&asic_pcr_lock, flags);

	msleep(250);

	spin_lock_irqsave(&asic_pcr_lock, flags);
	plinksta[1] = en75xx_read_config_word(0, 1, 0, ppos + 0x10);

	printk(KERN_INFO "PCIe RC1 link trained: %x -> %x\n",
		((plinksta[0] >> 16) & 0x0f), ((plinksta[1] >> 16) & 0x0f));

exit_unlock:
	spin_unlock_irqrestore(&asic_pcr_lock, flags);
}
#endif

int pcibios_plat_dev_init(struct pci_dev *dev)
{
	u32 busn = dev->bus->number;
	u32 slot = PCI_SLOT(dev->devfn);
	u32 i, val, tmp;

	/* P2P bridge */
	if (busn == 0) {
		/* set CLS */
		pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE,
			(L1_CACHE_BYTES >> 2));
	}

	if (busn == 0 && slot == 0 && (pcie_link_status & 0x1)) {
		val = 0;
		pci_read_config_dword(dev, 0x20, &val);
		tmp = (val & 0xffff) << 16;
		val = (val & 0xffff0000) + 0x100000;
		val = val - tmp;

		i = 0;
		while (i < 32) {
			if ((1u << i) >= val)
				break;
			i++;
		}

		/* config RC0 to EP addr window */
		sysRegWrite(ECONET_PCIE0_MAC_BASE + 0x438, tmp | i);
		mdelay(1);

		/* enable EP to RC0 access */
		sysRegWrite(ECONET_PCIE0_MAC_BASE + 0x448, 0x80);

		en75xx_pcie_rc0_retrain();
	}

#ifdef CONFIG_PCIE_PORT1
	if (busn == 0 && slot == 1 && (pcie_link_status & 0x2)) {
		val = 0;
		pci_read_config_dword(dev, 0x20, &val);
		tmp = (val & 0xffff) << 16;
		val = (val & 0xffff0000) + 0x100000;
		val = val - tmp;

		i = 0;
		while (i < 32) {
			if ((1 << i) >= val)
				break;
			i++;
		}

		/* config RC1 to EP addr window */
		sysRegWrite(ECONET_PCIE1_MAC_BASE + 0x438, tmp | i);
		mdelay(1);

		/* enable EP to RC1 access */
		sysRegWrite(ECONET_PCIE1_MAC_BASE + 0x448, 0x80);

		en75xx_pcie_rc1_retrain();
	}
#endif

	return 0;
}

int pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	int pci_irq = 0;

	if (slot == 0x0)
		pci_irq = SURFBOARDINT_PCIE0;
	else if (slot == 0x1)
		pci_irq = SURFBOARDINT_PCIE1;

	return pci_irq;
}

static inline void en75xx_pcie_init_phy(void)
{
#ifdef CONFIG_ECONET_EN7512
	u32 reg_val;

	/* LCDDS_CLK_PH_INV */
	reg_val = sysRegRead(RALINK_PCI_PHY0_BASE + 0x04a0);
	reg_val |= (0x1 << 5);
	sysRegWrite(RALINK_PCI_PHY0_BASE + 0x04a0, reg_val);
	mdelay(1);

	/* Patch TxDetRx Timing for 7512 E1, from DR 20160421, Biker_20160516 */
	reg_val = sysRegRead(RALINK_PCI_PHY1_BASE + 0x0a28);
	reg_val &= ~(0x1ff << 9);
	reg_val |=  (0x010 << 9);
	sysRegWrite(RALINK_PCI_PHY1_BASE + 0x0a28, reg_val);
	mdelay(1);

	reg_val = sysRegRead(RALINK_PCI_PHY1_BASE + 0x0a2c);
	reg_val &= ~0x1ff;
	reg_val |=  0x010;
	sysRegWrite(RALINK_PCI_PHY1_BASE + 0x0a2c, reg_val);
	mdelay(1);

#ifdef CONFIG_PCIE_PORT1
	if (isEN7512)
#endif
	{
		/* disable gen2 port PHY for COC test */
		reg_val = sysRegRead(RALINK_PCI_PHY1_BASE + 0x030c);
		reg_val |= (1u << 31);	/* IP_SW_RESET */
		sysRegWrite(RALINK_PCI_PHY1_BASE + 0x030c, reg_val);
		mdelay(1);
	}
#endif
}

int __init init_en75xx_pci(void)
{
	u32 i, reg_val;

	/* reset PCIe devices by pulse low */
	reg_val = sysRegRead(ECONET_PCIEC_REG);
	reg_val &= ~((1 << 29) | (1 << 26));
	sysRegWrite(ECONET_PCIEC_REG, reg_val);
	mdelay(1);

	en75xx_pcie_init_phy();

	/* enabled PCIe RC1 clock */
	reg_val = sysRegRead(ECONET_PCIEC_REG);
	reg_val |= (1 << 22);
	sysRegWrite(ECONET_PCIEC_REG, reg_val);
	mdelay(1);

	/* assert PCIe RC0/RC1/HB reset signal */
	reg_val = sysRegRead(ECONET_RSTCTRL_REG);
	reg_val |=  (RALINK_PCIE0_RST | RALINK_PCIE1_RST | RALINK_PCIEHB_RST);
	sysRegWrite(ECONET_RSTCTRL_REG, reg_val);
	mdelay(10);

	/* de-assert PCIe RC0/RC1/HB reset signal */
	reg_val &= ~(RALINK_PCIE0_RST | RALINK_PCIE1_RST | RALINK_PCIEHB_RST);
	sysRegWrite(ECONET_RSTCTRL_REG, reg_val);
	mdelay(10);

	/* release PCIe devices reset */
	reg_val = sysRegRead(ECONET_PCIEC_REG);
	reg_val |= ((1 << 29) | (1 << 26));
	sysRegWrite(ECONET_PCIEC_REG, reg_val);

	/* wait before detect card in slots */
	msleep(200);
	for (i = 0; i < 20; i++) {
		pcie_link_status = (sysRegRead(ECONET_PCIE_LINKUP) >> 1) & 0x3;

		if ((pcie_link_status & 0x1)
#ifdef CONFIG_PCIE_PORT1
		 && (pcie_link_status & 0x2)
#endif
		    )
			break;
		msleep(20);
	}

	if ((pcie_link_status & 0x1) == 0) {
		printk(KERN_WARNING "PCIe RC%d no card, disable PHY\n", 0);

		/* disable gen1 port PHY */
		reg_val = sysRegRead(RALINK_PCI_PHY0_BASE);
		reg_val &= ~(1 << 5); /* rg_pe1_phy_en */
		reg_val |=  (1 << 4); /* rg_pe1_frc_phy_en */
		sysRegWrite(RALINK_PCI_PHY0_BASE, reg_val);
		mdelay(1);
	}

#ifdef CONFIG_PCIE_PORT1
	if ((pcie_link_status & 0x2) == 0) {
		printk(KERN_WARNING "PCIe RC%d no card, disable PHY\n", 1);

		/* disable gen2 port PHY */
		reg_val = sysRegRead(RALINK_PCI_PHY1_BASE + 0x030c);
		reg_val |= (1u << 31);	/* IP_SW_RESET */
		sysRegWrite(RALINK_PCI_PHY1_BASE + 0x030c, reg_val);
		mdelay(1);
	}
#endif

	if (!pcie_link_status)
		return 0;

	/* change class to pci-pci class */
	sysRegWrite(ECONET_PCIE0_MAC_BASE + 0x104, 0x06040001);
	sysRegWrite(ECONET_PCIE1_MAC_BASE + 0x104, 0x06040001);
	mdelay(1);

	/* set host mode */
	sysRegWrite(ECONET_PCIE0_MAC_BASE, 0x00804201);
	sysRegWrite(ECONET_PCIE1_MAC_BASE, 0x00804201);
	mdelay(1);

	/* disable MSI interrupt */
	reg_val = sysRegRead(ECONET_PCIE0_MAC_BASE + 0x11c);
	reg_val &= ~(1 << 5);
	sysRegWrite(ECONET_PCIE0_MAC_BASE + 0x11c, reg_val);

	reg_val = sysRegRead(ECONET_PCIE1_MAC_BASE + 0x11c);
	reg_val &= ~(1 << 5);
	sysRegWrite(ECONET_PCIE1_MAC_BASE + 0x11c, reg_val);

	/* setup PCIe port0 MAC */
	if (pcie_link_status & 0x1) {
		/* enable interrupt */
		reg_val = sysRegRead(ECONET_PCIE0_MAC_BASE + 0x420);
		reg_val &= ~(1 << 16);
		sysRegWrite(ECONET_PCIE0_MAC_BASE + 0x420, reg_val);

		/* enable error interrupt */
		reg_val = sysRegRead(ECONET_PCIE_ERR_IRQ_EN);
		reg_val |= 0x03;
		sysRegWrite(ECONET_PCIE_ERR_IRQ_EN, reg_val);
	}

#ifdef CONFIG_PCIE_PORT1
	/* setup PCIe port1 MAC */
	if (pcie_link_status & 0x2) {
		/* enable interrupt */
		reg_val = sysRegRead(ECONET_PCIE1_MAC_BASE + 0x420);
		reg_val &= ~(1 << 16);
		sysRegWrite(ECONET_PCIE1_MAC_BASE + 0x420, reg_val);

		/* enable error interrupt */
		reg_val = sysRegRead(ECONET_PCIE_ERR_IRQ_EN);
		reg_val |= (0x03 << 2);
		sysRegWrite(ECONET_PCIE_ERR_IRQ_EN, reg_val);
	}
#endif

	en75xx_pci_controller.io_map_base = mips_io_port_base;

	register_pci_controller(&en75xx_pci_controller);

	return 0;
}

arch_initcall(init_en75xx_pci);

#endif /* CONFIG_PCI */
