/**************************************************************************
 *
 *  BRIEF MODULE DESCRIPTION
 *     init setup for Ralink RT2880 solution
 *
 *  Copyright 2007 Ralink Inc. (bruce_chang@ralinktech.com.tw)
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 **************************************************************************
 * May 2007 Bruce Chang
 *
 * Initial Release
 *
 *
 *
 **************************************************************************
 */

#include <linux/init.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/serial_8250.h>
#include <linux/delay.h>
#include <asm/fw/fw.h>
#include <asm/io.h>

#if defined(CONFIG_RALINK_MT7621)
#include <asm/smp-ops.h>
#include <asm/mips-cm.h>
#include <asm/mips-cpc.h>
#include <asm/mips-boards/launch.h>
#endif

#include <asm/rt2880/prom.h>
#include <asm/rt2880/generic.h>
#include <asm/rt2880/surfboard.h>
#include <asm/rt2880/surfboardint.h>
#include <asm/rt2880/rt_mmap.h>
#include <asm/rt2880/rt_serial.h>
#include <asm/rt2880/eureka_ep430.h>

#define UART_BAUDRATE		CONFIG_RALINK_UART_BRATE

#define RALINK_CLKCFG1		*(volatile u32 *)(RALINK_SYSCTL_BASE + 0x30)
#define RALINK_RSTCTRL		*(volatile u32 *)(RALINK_SYSCTL_BASE + 0x34)
#define RALINK_RSTSTAT		*(volatile u32 *)(RALINK_SYSCTL_BASE + 0x38)
#define RALINK_GPIOMODE		*(volatile u32 *)(RALINK_SYSCTL_BASE + 0x60)

u32 mips_cpu_feq;
u32 surfboard_sysclk;
u32 ralink_asic_rev_id;

static inline void prom_show_pstat(void)
{
	unsigned long status = fw_getenvl("pstat");

	switch (status) {
	default:
		pr_err("SoC power status: unknown (%08lx)\n", status);
		break;
	case 3:
		pr_warn("SoC power status: hardware watchdog reset\n");
		break;
	case 2:
		pr_info("SoC power status: software reset\n");
		break;
	case 1:
		pr_info("SoC power status: power-on reset\n");
		break;
	}
}

#if defined(CONFIG_RALINK_MT7621)

phys_addr_t mips_cpc_default_phys_base(void)
{
	return RALINK_CPC_BASE;
}

static inline void prom_init_cm(void)
{
	/* early detection of CMP support */
	mips_cm_probe();
	mips_cpc_probe();

	if (mips_cm_numiocu()) {
		/* Palmbus CM region */
		write_gcr_reg0_base(CM_GCR_REG0_BASE_VALUE);
		write_gcr_reg0_mask((CM_GCR_REG0_MASK_VALUE << 16) | CM_GCR_REGn_MASK_CMTGT_IOCU0);
#ifdef CONFIG_PCI
		/* PCIe CM region */
		write_gcr_reg1_base(CM_GCR_REG1_BASE_VALUE);
		write_gcr_reg1_mask((CM_GCR_REG1_MASK_VALUE << 16) | CM_GCR_REGn_MASK_CMTGT_IOCU0);
#endif
		__sync();
	}

	if (register_cps_smp_ops() == 0)
		return;
	if (register_cmp_smp_ops() == 0)
		return;
	if (register_vsmp_smp_ops() == 0)
		return;
}

bool plat_cpu_core_present(int core)
{
	struct cpulaunch *launch = (struct cpulaunch *)CKSEG0ADDR(CPULAUNCH);

	if (!core)
		return true;

	launch += core * 2; /* 2 VPEs per core */
	if (!(launch->flags & LAUNCH_FREADY))
		return false;

	if (launch->flags & (LAUNCH_FGO | LAUNCH_FGONE))
		return false;

	return true;
}
#endif

static inline void prom_init_pcie(void)
{
#if defined(CONFIG_RALINK_MT7628)
#if defined(CONFIG_PCI)
	/* PERST_GPIO_MODE = 0 */
	RALINK_GPIOMODE &= ~(0x01 << 16);

	/* assert PERST */
	RALINK_PCI_PCICFG_ADDR |= (1 << 1);
	udelay(100);
#endif

	/* assert PCIe RC RST */
	RALINK_RSTCTRL |=  RALINK_PCIE0_RST;

	/* disable PCIe clock */
	RALINK_CLKCFG1 &= ~RALINK_PCIE0_CLK_EN;

#if !defined(CONFIG_PCI)
	/* set  PCIe PHY to 1.3mA for power saving */
	(*((volatile u32 *)(RALINK_PCIEPHY_P0_CTL_OFFSET))) = 0x10;
#endif
#elif defined(CONFIG_RALINK_MT7621)
	u32 val = 0;
#if !defined(CONFIG_PCI) || !defined(CONFIG_PCIE_PORT0)
	val |= RALINK_PCIE0_RST;
#endif
#if !defined(CONFIG_PCI) || !defined(CONFIG_PCIE_PORT1)
	val |= RALINK_PCIE1_RST;
#endif
#if !defined(CONFIG_PCI) || !defined(CONFIG_PCIE_PORT2)
	val |= RALINK_PCIE2_RST;
#endif
	if (val) {
		/* deassert PCIe RC RST for disabled ports */
		if ((ralink_asic_rev_id & 0xFFFF) == 0x0101)
			RALINK_RSTCTRL &= ~(val);
		else
			RALINK_RSTCTRL |=  (val);
		udelay(100);
	}
#if !defined(CONFIG_PCI) || !(defined(CONFIG_PCIE_PORT0) || defined(CONFIG_PCIE_PORT1))
	/* set PCIe PHY P0P1 to 1.3mA for power saving */
	(*((volatile u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET))) = 0x10;
#endif
#if !defined(CONFIG_PCI) || !defined(CONFIG_PCIE_PORT2)
	/* set PCIe PHY P2 to 1.3mA for power saving */
	(*((volatile u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET))) = 0x10;
#endif
#if !defined(CONFIG_PCI)
	/* assert PCIe RC RST for disabled ports */
	if ((ralink_asic_rev_id & 0xFFFF) == 0x0101)
		RALINK_RSTCTRL |=  (val);
	else
		RALINK_RSTCTRL &= ~(val);
	/* disable PCIe clock for disabled ports */
	RALINK_CLKCFG1 &= ~val;
#endif
#endif
}

static inline void prom_init_usb(void)
{
#if defined(CONFIG_RALINK_MT7628)
	u32 reg;

	/* raise USB reset */
	reg = *(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x34));
	reg |= (RALINK_UDEV_RST | RALINK_UHST_RST);
	*(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x34))= reg;

	/* disable PHY0/1 clock */
	reg = *(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x30));
	reg &= ~(RALINK_UPHY0_CLK_EN | RALINK_UPHY1_CLK_EN);
	*(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x30)) = reg;
#endif
}

static inline void prom_init_sdxc(void)
{
#if defined(CONFIG_RALINK_MT7621) || defined(CONFIG_RALINK_MT7628)
	u32 reg;

	/* raise SDXC reset */
	reg = *(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x34));
	reg |= (RALINK_SDXC_RST);
	*(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x34))= reg;

	/* disable SDXC clock */
	reg = *(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x30));
	reg &= ~(RALINK_SDXC_CLK_EN);
	*(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x30)) = reg;
#endif
}

static inline void prom_init_nand(void)
{
#if !defined(CONFIG_MTD_NAND_MT7621)
#if defined(CONFIG_RALINK_MT7621)
	u32 reg;

	/* raise NAND reset */
	reg = *(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x34));
	reg |= (RALINK_NAND_RST);
	*(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x34))= reg;

	/* disable NAND clock */
	reg = *(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x30));
	reg &= ~(RALINK_NAND_CLK_EN);
	*(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x30)) = reg;
#endif
#endif
}

static inline void prom_init_spdif(void)
{
#if !defined(CONFIG_RALINK_SPDIF)
#if defined(CONFIG_RALINK_MT7621)
	u32 reg;

	/* raise SPDIF reset */
	reg = *(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x34));
	reg |= (RALINK_SPDIF_RST);
	*(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x34))= reg;

	/* disable SPDIF clock */
	reg = *(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x30));
	reg &= ~(RALINK_SPDIF_CLK_EN);
	*(volatile u32 *)KSEG1ADDR((RALINK_SYSCTL_BASE + 0x30)) = reg;
#endif
#endif
}

static void prom_init_sysclk(void)
{
	const char *vendor_name, *ram_type = "DDR2";
	char asic_id[8];
	int xtal = 40;
	u32 reg, ocp_freq;
	u8  clk_sel;
#if defined(CONFIG_MT7621_ASIC) || defined(CONFIG_MT7628_ASIC)
	u8  clk_sel2;
#endif
#if defined(CONFIG_RALINK_MT7621)
	u32 cpu_fdiv = 0;
	u32 cpu_ffrac = 0;
	u32 fbdiv = 0;
#endif

	reg = (*(volatile u32 *)(RALINK_SYSCTL_BASE + 0x00));
	memcpy(asic_id, &reg, 4);
	reg = (*(volatile u32 *)(RALINK_SYSCTL_BASE + 0x04));
	memcpy(asic_id+4, &reg, 4);
	asic_id[6] = '\0';
	asic_id[7] = '\0';

	ralink_asic_rev_id = (*(volatile u32 *)(RALINK_SYSCTL_BASE + 0x0c));

#if defined(CONFIG_RALINK_MT7621)
	/* CORE_NUM [17:17], 0: Single Core (S), 1: Dual Core (A) */
	if (ralink_asic_rev_id & (1UL<<17))
		asic_id[6] = 'A';
	else
		asic_id[6] = 'S';
#elif defined(CONFIG_RALINK_MT7628)
	/* Detect MT7688 via FUSE EE_CFG bit 20 */
	reg = (*(volatile u32 *)(RALINK_SYSCTL_BASE + 0x08));
	if (reg & (1UL<<20))
		asic_id[4] = '8';

	/* PKG_ID [16:16], 0: DRQFN-120 (KN), 1: DRQFN-156 (AN) */
	if (ralink_asic_rev_id & (1UL<<16))
		asic_id[6] = 'A';
	else
		asic_id[6] = 'K';
#endif

	reg = (*((volatile u32 *)(RALINK_SYSCTL_BASE + 0x10)));

#if defined(CONFIG_MT7621_ASIC)
	clk_sel = 0;	/* GPLL (500MHz) */
	clk_sel2 = (reg>>4) & 0x03;
	reg = (reg >> 6) & 0x7;
	if (reg >= 6)
		xtal = 25;
	else if (reg <= 2)
		xtal = 20;
	reg = (*((volatile u32 *)(RALINK_SYSCTL_BASE + 0x2C)));
	if (reg & (0x1UL << 30))
		clk_sel = 1;	/* CPU PLL */
#elif defined(CONFIG_MT7628_ASIC)
	clk_sel = 0;		/* CPU PLL (580/575MHz) */
	clk_sel2 = reg & 0x01;
	if (!(reg & (1UL<<6)))
		xtal = 25;
	reg = (*((volatile u32 *)(RALINK_SYSCTL_BASE + 0x2C)));
	if (reg & (0x1UL << 1))
		clk_sel = 1;	/* BBP PLL (480MHz) */
#else
#error Please Choice System Type
#endif

	switch(clk_sel) {
#if defined(CONFIG_RALINK_MT7621)
	case 0: /* GPLL (500MHz) */
		reg = (*(volatile u32 *)(RALINK_SYSCTL_BASE + 0x44));
		cpu_fdiv = ((reg >> 8) & 0x1F);
		cpu_ffrac = (reg & 0x1F);
		mips_cpu_feq = (500 * cpu_ffrac / cpu_fdiv) * 1000 * 1000;
		break;
	case 1: /* CPU PLL */
		reg = (*(volatile u32 *)(RALINK_MEMCTRL_BASE + 0x648));
		fbdiv = ((reg >> 4) & 0x7F) + 1;
		if (xtal == 25)
			mips_cpu_feq = 25 * fbdiv * 1000 * 1000;	/* 25Mhz Xtal */
		else
			mips_cpu_feq = 20 * fbdiv * 1000 * 1000;	/* 20/40Mhz Xtal */
		break;
#elif defined(CONFIG_RALINK_MT7628)
	case 0:
		if (xtal == 25)
			mips_cpu_feq = 575 * 1000 * 1000;	/* 25MHZ Xtal */
		else
			mips_cpu_feq = 580 * 1000 * 1000;	/* 40MHz Xtal */
		break;
	case 1:
		mips_cpu_feq = (480*1000*1000);
		break;
#else
#error Please Choice Chip Type
#endif
	}

#if defined(CONFIG_RALINK_MT7621)
	if (clk_sel2 & 0x01)
		ram_type = "DDR2";
	else
		ram_type = "DDR3";
	if (clk_sel2 & 0x02)
		ocp_freq = mips_cpu_feq/4;	/* OCP_RATIO 1:4 */
	else
		ocp_freq = mips_cpu_feq/3;	/* OCP_RATIO 1:3 */
	surfboard_sysclk = mips_cpu_feq/4;
#elif defined(CONFIG_RALINK_MT7628)
	surfboard_sysclk = mips_cpu_feq/3;
	if (clk_sel2)
		ram_type = "DDR1";
	else
		ram_type = "DDR2";
	/* set CPU ratio for sleep mode (USB OCP must be >= 30MHz) */
	reg = (*((volatile u32 *)(RALINK_SYSCTL_BASE + 0x440)));
	reg &= ~0x0f0f;
	reg |=  0x0606;	/* CPU ratio 1/6 for sleep mode (OCP: 575/6/3 = 31 MHz) */
	(*((volatile u32 *)(RALINK_SYSCTL_BASE + 0x440))) = reg;
	udelay(10);

	/* disable request preemption */
	reg = (*((volatile u32 *)(RALINK_RBUS_MATRIXCTL_BASE + 0x0)));
	reg &= ~0x04000000;
	(*((volatile u32 *)(RALINK_RBUS_MATRIXCTL_BASE + 0x0))) = reg;

	/* MIPS reset apply to Andes */
	RALINK_RSTSTAT |= (1U << 9);
#else
	surfboard_sysclk = mips_cpu_feq/3;
#endif

#if !defined(CONFIG_RALINK_MT7621)
	ocp_freq = surfboard_sysclk;
#endif

	vendor_name = "MediaTek";

	printk(KERN_INFO "%s SoC: %s, RevID: %04X, RAM: %s, XTAL: %dMHz\n",
		vendor_name,
		asic_id,
		ralink_asic_rev_id & 0xffff,
		ram_type,
		xtal
	);

	printk(KERN_INFO "CPU/OCP/SYS frequency: %d/%d/%d MHz\n",
		mips_cpu_feq / 1000 / 1000,
		ocp_freq / 1000 / 1000,
		surfboard_sysclk / 1000 / 1000
	);
}

/*
** This function sets up the local prom_rs_table used only for the fake console
** console (mainly prom_printf for debug display and no input processing)
** and also sets up the global rs_table used for the actual serial console.
** To get the correct baud_base value, prom_init_sysclk() must be called before
** this function is called.
*/

static void __init prom_init_serial_port(void)
{
	unsigned int uartclk;
	unsigned int clock_divisor;
	struct uart_port serial_req[2];

	/*
	 * baud rate = system clock freq / (CLKDIV * 16)
	 * CLKDIV=system clock freq/16/baud rate
	 */
#if defined(CONFIG_RALINK_MT7621)
	uartclk = 50000000;
#else
	uartclk = 40000000;
#endif
	clock_divisor = (uartclk / SURFBOARD_BAUD_DIV / UART_BAUDRATE);

	memset(serial_req, 0, sizeof(serial_req));

	// fixed 8n1
	IER(RALINK_SYSCTL_BASE + 0xC00) = 0;
	LCR(RALINK_SYSCTL_BASE + 0xC00) = RT2880_UART_LCR_WLEN8;
	DLL(RALINK_SYSCTL_BASE + 0xC00) = (clock_divisor & 0xFF);
	DLM(RALINK_SYSCTL_BASE + 0xC00) = (clock_divisor >> 8) & 0xFF;
	FCR(RALINK_SYSCTL_BASE + 0xC00) = 7; /* reset TX and RX */

	// fixed 8n1
	IER(RALINK_SYSCTL_BASE + 0xD00) = 0;
	LCR(RALINK_SYSCTL_BASE + 0xD00) = RT2880_UART_LCR_WLEN8;
	DLL(RALINK_SYSCTL_BASE + 0xD00) = (clock_divisor & 0xFF);
	DLM(RALINK_SYSCTL_BASE + 0xD00) = (clock_divisor >> 8) & 0xFF;
	FCR(RALINK_SYSCTL_BASE + 0xD00) = 7; /* reset TX and RX */

	serial_req[0].line       = 0;
	serial_req[0].type       = PORT_16550A;
	serial_req[0].iotype     = UPIO_MEM32;
	serial_req[0].irq        = SURFBOARDINT_UART1;
	serial_req[0].flags      = UPF_FIXED_TYPE | UPF_SKIP_TEST | UPF_BOOT_AUTOCONF;
	serial_req[0].uartclk    = uartclk;
	serial_req[0].regshift   = 2;
	serial_req[0].mapbase    = RALINK_UART_LITE_BASE;
	serial_req[0].membase    = ioremap_nocache(serial_req[0].mapbase, 0x0100);
	serial_req[0].mapsize    = 0x0100;
	early_serial_setup(&serial_req[0]);

#if !defined(CONFIG_RALINK_GPIOMODE_UARTF) && (CONFIG_SERIAL_8250_NR_UARTS > 1)
	serial_req[1].line       = 1;
	serial_req[1].type       = PORT_16550A;
	serial_req[1].iotype     = UPIO_MEM32;
	serial_req[1].irq        = SURFBOARDINT_UART2;
	serial_req[1].flags      = UPF_FIXED_TYPE | UPF_SKIP_TEST | UPF_BOOT_AUTOCONF;
	serial_req[1].uartclk    = uartclk;
	serial_req[1].regshift   = 2;
	serial_req[1].mapbase    = RALINK_UART_BASE;
	serial_req[1].membase    = ioremap_nocache(serial_req[1].mapbase, 0x0100);
	serial_req[1].mapsize    = 0x0100;
	early_serial_setup(&serial_req[1]);
#endif
}

int __init prom_get_ttysnum(void)
{
	char *argptr;
	int ttys_num = 0;  /* default UART Lite */

	/* get ttys_num to use with the fake console/prom_printf */
	argptr = fw_getcmdline();

	if ((argptr = strstr(argptr, "console=ttyS")) != NULL) {
		argptr += strlen("console=ttyS");
		if (argptr[0] == '0')		/* ttyS0 */
			ttys_num = 0;		/* happens to be rs_table[0] */
		else if (argptr[0] == '1')	/* ttyS1 */
			ttys_num = 1;		/* happens to be rs_table[1] */
	}

	return (ttys_num);
}

void __init prom_init(void)
{
	set_io_port_base(KSEG1);

	/* remove all wired TLB entries */
	write_c0_wired(0);

	prom_init_cmdline();
	prom_init_printf(prom_get_ttysnum());
	prom_init_sysclk();
	prom_init_serial_port();	/* Needed for Serial Console */
	prom_init_usb();		/* USB power saving */
	prom_init_pcie();		/* PCIe power saving */
	prom_init_sdxc();		/* SDXC power saving */
	prom_init_nand();		/* NAND power saving */
	prom_init_spdif();		/* SPDIF power saving */
	prom_meminit();
#if defined(CONFIG_RALINK_MT7621)
	prom_init_cm();
#endif

	prom_show_pstat();

	prom_printf("Linux kernel started.\n");
}

