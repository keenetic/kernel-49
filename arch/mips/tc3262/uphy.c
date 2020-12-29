#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/tc3162/rt_mmap.h>
#include <asm/tc3162/tc3162.h>

#define ADDR_SIFSLV_BASE		RALINK_XHCI_UPHY_BASE
#define ADDR_SIFSLV_FMREG_BASE		(ADDR_SIFSLV_BASE + 0x0100)
#define ADDR_SIFSLV_IPPC_BASE		(ADDR_SIFSLV_BASE + 0x0700)
#define ADDR_SIFSLV_PHYD_BASE		(ADDR_SIFSLV_BASE + 0x0900)
#define ADDR_SIFSLV_PHYD_B2_BASE	(ADDR_SIFSLV_BASE + 0x0a00)
#define ADDR_SIFSLV_PHYA_BASE		(ADDR_SIFSLV_BASE + 0x0b00)
#define ADDR_SIFSLV_PHYA_DA_BASE	(ADDR_SIFSLV_BASE + 0x0c00)
#define ADDR_U2_PHY_P0_BASE		(ADDR_SIFSLV_BASE + 0x0800)
#define ADDR_U2_PHY_P1_BASE		(ADDR_SIFSLV_BASE + 0x1000)

#define U2_SR_COEFF			28
#define REF_CK				20

#define REG_SIFSLV_FMREG_FMCR0		(ADDR_SIFSLV_FMREG_BASE + 0x00)
#define REG_SIFSLV_FMREG_FMCR1		(ADDR_SIFSLV_FMREG_BASE + 0x04)
#define REG_SIFSLV_FMREG_FMCR2		(ADDR_SIFSLV_FMREG_BASE + 0x08)
#define REG_SIFSLV_FMREG_FMMONR0	(ADDR_SIFSLV_FMREG_BASE + 0x0C)
#define REG_SIFSLV_FMREG_FMMONR1	(ADDR_SIFSLV_FMREG_BASE + 0x10)

/* SIFSLV_FMREG_FMCR0 */
#define RG_LOCKTH			(0xf << 28)
#define RG_MONCLK_SEL			(0x3 << 26)
#define RG_FM_MODE			(0x1 << 25)
#define RG_FREQDET_EN			(0x1 << 24)
#define RG_CYCLECNT			(0x00ffffff)

/* SIFSLV_FMREG_FMMONR1 */
#define RG_MONCLK_SEL_3			(0x1 << 9)
#define RG_FRCK_EN			(0x1 << 8)
#define USBPLL_LOCK			(0x1 << 1)
#define USB_FM_VLD			(0x1 << 0)

#define OFS_U2_PHY_AC0			0x00
#define OFS_U2_PHY_AC1			0x04
#define OFS_U2_PHY_AC2			0x08
#define OFS_U2_PHY_ACR0			0x10
#define OFS_U2_PHY_ACR1			0x14
#define OFS_U2_PHY_ACR2			0x18
#define OFS_U2_PHY_ACR3			0x1C
#define OFS_U2_PHY_ACR4			0x20
#define OFS_U2_PHY_AMON0		0x24
#define OFS_U2_PHY_DCR0			0x60
#define OFS_U2_PHY_DCR1			0x64
#define OFS_U2_PHY_DTM0			0x68
#define OFS_U2_PHY_DTM1			0x6C

/* U2_PHY_ACR0 */
#define RG_USB20_ICUSB_EN		(0x1 << 24)
#define RG_USB20_HSTX_SRCAL_EN		(0x1 << 23)
#define RG_USB20_HSTX_SRCTRL		(0x7 << 16)
#define RG_USB20_LS_CR			(0x7 << 12)
#define RG_USB20_FS_CR			(0x7 << 8)
#define RG_USB20_LS_SR			(0x7 << 4)
#define RG_USB20_FS_SR			(0x7 << 0)

/* SSUSB_IP_PW_CTRL0 */
#define REG_SSUSB_IP_PW_CTRL0		(ADDR_SIFSLV_IPPC_BASE + 0x0)
#define SSUSB_IP_SW_RST			(1 << 0)

/* SSUSB_IP_PW_CTRL1 */
#define REG_SSUSB_IP_PW_CTRL1		(ADDR_SIFSLV_IPPC_BASE + 0x4)
#define SSUSB_IP_PDN			(1 << 0)

/* SSUSB_IP_PW_STS1 */
#define REG_SSUSB_IP_PW_STS1		(ADDR_SIFSLV_IPPC_BASE + 0x10)
#define SSUSB_STS1_SYSPLL_STABLE	(1 << 0)
#define SSUSB_STS1_REF_RST		(1 << 8)
#define SSUSB_STS1_SYS125_RST		(1 << 10)
#define SSUSB_STS1_XHCI_RST		(1 << 11)
#define SSUSB_STS1_U3_MAC_RST		(1 << 16)

/* SSUSB_U3_CTRL */
#define REG_SSUSB_U3_CTRL(p)		(ADDR_SIFSLV_IPPC_BASE + 0x30 + (p * 0x08))
#define SSUSB_U3_PORT_DIS		(1 << 0)
#define SSUSB_U3_PORT_PDN		(1 << 1)
#define SSUSB_U3_PORT_HOST_SEL		(1 << 2)
#define SSUSB_U3_PORT_CKBG_EN		(1 << 3)
#define SSUSB_U3_PORT_MAC_RST		(1 << 4)
#define SSUSB_U3_PORT_PHYD_RST		(1 << 5)

/* SSUSB_U2_CTRL */
#define REG_SSUSB_U2_CTRL(p)		(ADDR_SIFSLV_IPPC_BASE + 0x50 + (p * 0x08))
#define SSUSB_U2_PORT_DIS		(1 << 0)
#define SSUSB_U2_PORT_PDN		(1 << 1)
#define SSUSB_U2_PORT_HOST_SEL		(1 << 2)
#define SSUSB_U2_PORT_CKBG_EN		(1 << 3)
#define SSUSB_U2_PORT_MAC_RST		(1 << 4)
#define SSUSB_U2_PORT_PHYD_RST		(1 << 5)

/* SSUSB_IP_CAP */
#define REG_SSUSB_IP_CAP		(ADDR_SIFSLV_IPPC_BASE + 0x024)
#define SSUSB_U3_PORT_NUM(p)		(p & 0xff)
#define SSUSB_U2_PORT_NUM(p)		((p >> 8) & 0xff)

/* SSUSB_IP_SPAR0 */
#define REG_SSUSB_IP_SPAR0		(ADDR_SIFSLV_IPPC_BASE + 0x0c8)

static atomic_t uphy_init_instance = ATOMIC_INIT(0);

static inline u32
uphy_read32(u32 reg_addr)
{
	return sysRegRead(reg_addr);
}

static inline void
uphy_write32(u32 reg_addr, u32 reg_data)
{
	sysRegWrite(reg_addr, reg_data);
}

static void
uphy_write8(u32 reg_addr, u32 reg_data)
{
	u32 reg_sfl = (reg_addr % 4) * 8;
	u32 reg_a32 = reg_addr & 0xfffffffc;
	u32 reg_msk = 0xff << reg_sfl;
	u32 reg_tmp = uphy_read32(reg_a32);

	reg_tmp &= ~reg_msk;
	reg_tmp |= ((reg_data << reg_sfl) & reg_msk);

	uphy_write32(reg_a32, reg_tmp);
}

static void
u2_slew_rate_calibration(int port_id, u32 u2_phy_reg_base)
{
	u32 reg_val, i;
	u32 u4Tmp, u4FmOut = 0;

	/* enable HS TX SR calibration */
	reg_val = uphy_read32(u2_phy_reg_base + OFS_U2_PHY_ACR0);
	reg_val |= RG_USB20_HSTX_SRCAL_EN;
	uphy_write32(u2_phy_reg_base + OFS_U2_PHY_ACR0, reg_val);
	msleep(1);

	/* enable free run clock */
	reg_val = uphy_read32(REG_SIFSLV_FMREG_FMMONR1);
	reg_val |= RG_FRCK_EN;
	uphy_write32(REG_SIFSLV_FMREG_FMMONR1, reg_val);

	/* setting MONCLK_SEL 0x0/0x1 for port0/port1 */
	reg_val = uphy_read32(REG_SIFSLV_FMREG_FMCR0);
	reg_val &= ~RG_MONCLK_SEL;
	reg_val |= (port_id << 26);
	uphy_write32(REG_SIFSLV_FMREG_FMCR0, reg_val);

	/* setting cyclecnt = 400 */
	reg_val = uphy_read32(REG_SIFSLV_FMREG_FMCR0);
	reg_val &= ~RG_CYCLECNT;
	reg_val |= 0x400;
	uphy_write32(REG_SIFSLV_FMREG_FMCR0, reg_val);

	/* enable frequency meter */
	reg_val = uphy_read32(REG_SIFSLV_FMREG_FMCR0);
	reg_val |= RG_FREQDET_EN;
	uphy_write32(REG_SIFSLV_FMREG_FMCR0, reg_val);

	/* wait for FM detection done, set 10ms timeout */
	for (i = 0; i < 10; i++) {
		/* read FM_OUT */
		u4FmOut = uphy_read32(REG_SIFSLV_FMREG_FMMONR0);

		/* check if FM detection done */
		if (u4FmOut != 0)
			break;

		msleep(1);
	}

	/* disable frequency meter */
	reg_val = uphy_read32(REG_SIFSLV_FMREG_FMCR0);
	reg_val &= ~RG_FREQDET_EN;
	uphy_write32(REG_SIFSLV_FMREG_FMCR0, reg_val);

	/* disable free run clock */
	reg_val = uphy_read32(REG_SIFSLV_FMREG_FMMONR1);
	reg_val &= ~RG_FRCK_EN;
	uphy_write32(REG_SIFSLV_FMREG_FMMONR1, reg_val);

	/* disable HS TX SR calibration */
	reg_val = uphy_read32(u2_phy_reg_base + OFS_U2_PHY_ACR0);
	reg_val &= ~RG_USB20_HSTX_SRCAL_EN;
	uphy_write32(u2_phy_reg_base + OFS_U2_PHY_ACR0, reg_val);
	msleep(1);

	/* update RG_USB20_HSTX_SRCTRL */
	reg_val = uphy_read32(u2_phy_reg_base + OFS_U2_PHY_ACR0);
	reg_val &= ~RG_USB20_HSTX_SRCTRL;
	if (u4FmOut != 0) {
		/* set reg = (1024/FM_OUT) * 20 * 0.028 (round to the nearest digits) */
		u4Tmp = (((1024 * REF_CK * U2_SR_COEFF) / u4FmOut) + 500) / 1000;
		reg_val |= ((u4Tmp & 0x07) << 16);
		printk(KERN_INFO "U2PHY P%d set SRCTRL %s value: %d\n", port_id, "calibration", u4Tmp);
	} else {
		reg_val |= (0x4 << 16);
		printk(KERN_INFO "U2PHY P%d set SRCTRL %s value: %d\n", port_id, "default", 4);
	}
	uphy_write32(u2_phy_reg_base + OFS_U2_PHY_ACR0, reg_val);
}

static void
u2_phy_init(u32 u2_phy_reg_base)
{
	u32 reg_val;

	/* set SW PLL Stable mode to 1 for U2 LPM device remote wakeup */
	reg_val = uphy_read32(u2_phy_reg_base + OFS_U2_PHY_DCR1);
	reg_val &= ~(0x3 << 18);
	reg_val |=  (0x1 << 18);
	uphy_write32(u2_phy_reg_base + OFS_U2_PHY_DCR1, reg_val);
}

static inline void
uphy_setup_25mhz_xtal(void)
{
	uphy_write8(ADDR_SIFSLV_PHYA_DA_BASE + 0x1c, 0x18);
	uphy_write8(ADDR_SIFSLV_PHYA_DA_BASE + 0x1d, 0x18);
	uphy_write8(ADDR_SIFSLV_PHYA_DA_BASE + 0x1f, 0x18);
	uphy_write32(ADDR_SIFSLV_PHYA_DA_BASE + 0x24, 0x18000000);
	uphy_write32(ADDR_SIFSLV_PHYA_DA_BASE + 0x28, 0x18000000);
	uphy_write32(ADDR_SIFSLV_PHYA_DA_BASE + 0x30, 0x18000000);
	uphy_write32(ADDR_SIFSLV_PHYA_DA_BASE + 0x38, 0x004a004a);
	uphy_write8(ADDR_SIFSLV_PHYA_DA_BASE + 0x3e, 0x4a);
	uphy_write8(ADDR_SIFSLV_PHYA_DA_BASE + 0x3f, 0x0);
	uphy_write8(ADDR_SIFSLV_PHYA_DA_BASE + 0x42, 0x48);
	uphy_write8(ADDR_SIFSLV_PHYA_DA_BASE + 0x43, 0x0);
	uphy_write8(ADDR_SIFSLV_PHYA_DA_BASE + 0x44, 0x48);
	uphy_write8(ADDR_SIFSLV_PHYA_DA_BASE + 0x45, 0x0);
	uphy_write8(ADDR_SIFSLV_PHYA_DA_BASE + 0x48, 0x48);
	uphy_write8(ADDR_SIFSLV_PHYA_DA_BASE + 0x49, 0x0);

	uphy_write8(ADDR_SIFSLV_PHYA_BASE + 0x24, 0x90);
	uphy_write8(ADDR_SIFSLV_PHYA_BASE + 0x25, 0x1);
	uphy_write32(ADDR_SIFSLV_PHYA_BASE + 0x10, 0x1c000000);
	uphy_write8(ADDR_SIFSLV_PHYA_BASE + 0x0b, 0xe);
}

void uphy_init(void)
{
	u32 reg_val;

	if (atomic_inc_return(&uphy_init_instance) != 1)
		return;

#if defined(CONFIG_ECONET_EN7512)

	/* patch TxDetRx Timing for E1, from DR 20160421, Biker_20160516 */
	reg_val = uphy_read32(ADDR_SIFSLV_PHYD_B2_BASE + 0x28);
	reg_val &= ~(0x1ff << 9);
	reg_val |=  (0x010 << 9);
	uphy_write32(ADDR_SIFSLV_PHYD_B2_BASE + 0x28, reg_val);

	reg_val = uphy_read32(ADDR_SIFSLV_PHYD_B2_BASE + 0x2c);
	reg_val &= ~0x1ff;
	reg_val |=  0x010;
	uphy_write32(ADDR_SIFSLV_PHYD_B2_BASE + 0x2c, reg_val);

	/* patch LFPS Filter Threshold for E1, from DR 20160421, Biker_20160516 */
	reg_val = uphy_read32(ADDR_SIFSLV_PHYD_BASE + 0x0c);
	reg_val &= ~(0x3f << 16);
	reg_val |=  (0x34 << 16);
	uphy_write32(ADDR_SIFSLV_PHYD_BASE + 0x0c, reg_val);

	/* configure for XTAL 25MHz */
	if (VPint(CR_AHB_HWCONF) & 0x01)
		uphy_setup_25mhz_xtal();

	if (isEN7513 || isEN7513G) {
		printk(KERN_INFO "%s USB PHY config\n", "EN7513 (BGA)");

		uphy_write32(ADDR_U2_PHY_P0_BASE + 0x1c, 0xC0240008); /* enable port 0 */
		uphy_write32(ADDR_U2_PHY_P1_BASE + 0x1c, 0xC0240000); /* enable port 1 */
	} else if (isEN7512) {
		printk(KERN_INFO "%s USB PHY config\n", "EN7512 (QFP)");

		uphy_write32(ADDR_U2_PHY_P0_BASE + 0x1c, 0xC0241580); /* disable port 0 */
		uphy_write32(ADDR_U2_PHY_P1_BASE + 0x1c, 0xC0240000); /* enable port 1 */
	}

#elif defined(CONFIG_ECONET_EN7516) || \
      defined(CONFIG_ECONET_EN7527) || \
      defined(CONFIG_ECONET_EN7528)

	/* configure for XTAL 25MHz */
	reg_val = VPint(CR_AHB_HWCONF);
	if (!(reg_val & 0x40000))
		uphy_setup_25mhz_xtal();

	uphy_write32(ADDR_U2_PHY_P0_BASE + 0x1c, 0xC0240008); /* enable port 0 */
	uphy_write32(ADDR_U2_PHY_P1_BASE + 0x1c, 0xC0240000); /* enable port 1 */

	printk(KERN_INFO "%s USB PHY config\n", "EN7516/EN7561");

#endif

	/* doorbell handling */
	uphy_write32(REG_SSUSB_IP_SPAR0, 0x1);

	/* init UPHY */
	u2_phy_init(ADDR_U2_PHY_P0_BASE);
	u2_phy_init(ADDR_U2_PHY_P1_BASE);

	/* calibrate UPHY */
	u2_slew_rate_calibration(0, ADDR_U2_PHY_P0_BASE);
	u2_slew_rate_calibration(1, ADDR_U2_PHY_P1_BASE);
}
