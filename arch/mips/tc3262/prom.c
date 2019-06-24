#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <asm/bootinfo.h>
#include <asm/cacheflush.h>
#include <asm/traps.h>
#include <asm/io.h>

#ifdef CONFIG_MIPS_MT_SMP
#include <asm/smp-ops.h>
#include <asm/mips-cm.h>
#include <asm/mips-cpc.h>
#include <asm/tc3162/launch.h>
#endif

#include <asm/tc3162/prom.h>
#include <asm/tc3162/rt_mmap.h>
#include <asm/tc3162/tc3162.h>

unsigned int surfboard_sysclk;
unsigned int tc_mips_cpu_freq;

int soc_power_status;
EXPORT_SYMBOL(soc_power_status);

#ifdef CONFIG_MIPS_CMDLINE_FROM_BOOTLOADER
int prom_argc;
int *_prom_argv, *_prom_envp;

/*
 * YAMON (32-bit PROM) pass arguments and environment as 32-bit pointer.
 * This macro take care of sign extension, if running in 64-bit mode.
 */
#define prom_envp(index) ((char *)(((int *)(int)_prom_envp)[(index)]))
#endif

char *prom_getenv(char *envname)
{
#ifdef CONFIG_MIPS_CMDLINE_FROM_BOOTLOADER
	/*
	 * Return a pointer to the given environment variable.
	 * In 64-bit mode: we're using 64-bit pointers, but all pointers
	 * in the PROM structures are only 32-bit, so we need some
	 * workarounds, if we are running in 64-bit mode.
	 */
	int i, index=0;
	char *p, *q;

	i = strlen(envname);
	while (prom_envp(index)) {
		p = (char*) KSEG0ADDR(prom_envp(index));
		if(!strncmp(envname, p, i)) {
			q = strchr(p, '=');
			if (q)
				q++;
			return q;
		}
		index++;
	}
#endif
	return NULL;
}

const char *get_system_type(void)
{
#ifdef CONFIG_ECONET_EN7516
	if (isEN7516G)
		return "EcoNet EN7516G SoC";
#endif
#ifdef CONFIG_ECONET_EN7527
	if (isEN7527G)
		return "EcoNet EN7527G SoC";
	if (isEN7527H)
		return "EcoNet EN7527H SoC";
#endif
#ifdef CONFIG_ECONET_EN7512
	if (isEN7513G)
		return "EcoNet EN7513G SoC";
	if (isEN7513)
		return "EcoNet EN7513 SoC";
	if (isEN7512)
		return "EcoNet EN7512 SoC";
#endif
	return "EcoNet SoC";
}

static inline void prom_show_pstat(void)
{
	char *s;

	s = prom_getenv("pstat");
	if (!s)
		return;

	soc_power_status = (int)simple_strtoul(s, NULL, 0);

	switch (soc_power_status) {
	case 3:
		printk(KERN_WARNING "SoC power status: %s\n", "Watchdog reset occured");
		break;
	case 2:
		printk(KERN_INFO "SoC power status: %s\n", "Soft reset occured");
		break;
	case 1:
		printk(KERN_INFO "SoC power status: %s\n", "Hard reset occured");
		break;
	}
}

static inline void tc_mips_setup(void)
{
#ifdef CONFIG_MIPS_TC3262_34K
	/* enable 34K external sync */
#ifndef CONFIG_SMP
	unsigned int oconfig7 = read_c0_config7();
	unsigned int nconfig7 = oconfig7;

	nconfig7 |= (1 << 8);
	if (oconfig7 != nconfig7) {
		__asm__ __volatile("sync");
		write_c0_config7(nconfig7);
		ehb();
	}
#else
	strcat(arcs_cmdline, " es=1");
#endif
#endif
}

#if defined(CONFIG_MIPS_CMP) || \
    defined(CONFIG_MIPS_CPS)
phys_addr_t mips_cpc_default_phys_base(void)
{
	return RALINK_CPC_BASE;
}

static inline void prom_init_cm(void)
{
	/* early detection of CMP support */
	mips_cm_probe();
	mips_cpc_probe();

#ifdef CONFIG_MIPS_MT_SMP
	if (register_cps_smp_ops() == 0)
		return;
	if (register_cmp_smp_ops() == 0)
		return;
	if (register_vsmp_smp_ops() == 0)
		return;
#endif
}

bool plat_cpu_core_present(int core)
{
	struct cpulaunch *launch = (struct cpulaunch *)CPU_LAUNCH_BASE;

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

static inline void tc_uart_setup(void)
{
	unsigned int div_x, div_y, word;

	/* Set FIFO controo enable, reset RFIFO, TFIFO, 16550 mode, watermark=0x00 (1 byte) */
	VPchar(CR_HSUART_FCR) = UART_FCR | UART_WATERMARK;

	/* Set modem control to 0 */
	VPchar(CR_HSUART_MCR) = UART_MCR;

	/* Disable IRDA, Disable Power Saving Mode, RTS , CTS flow control */
	VPchar(CR_HSUART_MISCC) = UART_MISCC;

	/* Access the baudrate divider */
	VPchar(CR_HSUART_LCR) = UART_BRD_ACCESS;

	div_y = UART_XYD_Y;
#if defined(CONFIG_RT2880_UART_115200)
	div_x = 59904;	// Baud rate 115200
#else
	div_x = 29952;	// Baud rate 57600
#endif
	word = (div_x << 16) | div_y;
	VPint(CR_HSUART_XYD) = word;

	/* Set Baud Rate Divisor to 1*16 */
	VPchar(CR_HSUART_BRDL) = UART_BRDL_20M;
	VPchar(CR_HSUART_BRDH) = UART_BRDH_20M;

	/* Set DLAB = 0, clength = 8, stop = 1, no parity check */
	VPchar(CR_HSUART_LCR) = UART_LCR;
}

static inline void tc_ahb_setup(void)
{
	/* setup bus timeout value */
	VPint(CR_AHB_AACS) = 0xffff;
}

static inline void tc_usb_setup(void)
{
#if !IS_ENABLED(CONFIG_USB)
	/* disable both ports UPHY */
	VPint(RALINK_XHCI_UPHY_BASE + 0x081C) = 0xC0241580;
	VPint(RALINK_XHCI_UPHY_BASE + 0x101C) = 0xC0241580;
#endif
}

static inline void tc_dmt_setup(void)
{
	/* assert DMT reset */
	VPint(CR_AHB_DMTCR) = 0x1;

#ifndef CONFIG_TC3162_ADSL
	udelay(100);

	/* disable DMT clock to power save */
	VPint(CR_AHB_DMTCR) = 0x3;
#endif
}

static inline void tc_fe_setup(void)
{
	unsigned int reg_val;

	reg_val = VPint(CR_AHB_BASE + 0x834);

	/* check GSW not in reset state */
	if (!(reg_val & (1U << 23))) {
		/* disable GSW P6 link */
		VPint(RALINK_ETH_SW_BASE + 0x3600) = 0x8000;
	}

	/* assert FE, QDMA2, QDMA1 reset */
	reg_val |=  ((1U << 21) | (1U << 2) | (1U << 1));
	VPint(CR_AHB_BASE + 0x834) = reg_val;
	udelay(100);

	/* de-assert FE, QDMA2, QDMA1 reset */
	reg_val &= ~((1U << 21) | (1U << 2) | (1U << 1));
	VPint(CR_AHB_BASE + 0x834) = reg_val;

	/* wait FE & PPE init done */
	udelay(1000);

	/* disable PPE engine by default */
	reg_val = VPint(CR_MAC_BASE + 0x0E00);
	reg_val &= ~(1U << 0);
	VPint(CR_MAC_BASE + 0x0E00) = reg_val;
}

#define VECTORSPACING 0x100	/* for EI/VI mode */

void __init mips_nmi_setup(void)
{
	void *base;
	extern char except_vec_nmi;

	base = cpu_has_veic ?
		(void *)(ebase + 0x200 + VECTORSPACING * 64) :
		(void *)(ebase + 0x380);

	printk(KERN_INFO "NMI base is %08X\n", (unsigned int)base);

	/*
	 * Fill the NMI_Handler address in a register, which is a R/W register
	 * start.S will read it, then jump to NMI_Handler address
	 */
	VPint(0xbfb00244) = (unsigned int)base;

	memcpy(base, &except_vec_nmi, 0x80);
	flush_icache_range((unsigned long)base, (unsigned long)base + 0x80);
}

static inline void cpu_dma_round_robin(int mode)
{
	unsigned int reg_arb;

	reg_arb = VPint(ARB_CFG);

	if (mode == ENABLE)
		reg_arb |= ROUND_ROBIN_ENABLE;
	else
		reg_arb &= ROUND_ROBIN_DISBALE;

	VPint(ARB_CFG) = reg_arb;
}

void __init prom_init(void)
{
	unsigned long memsize;
	unsigned int bus_freq, cpu_freq, cpu_ratio;
	const char *ram_type;

	/* Test supported hardware */
#if defined(CONFIG_ECONET_EN7512)
	if (!isEN751221)
#elif defined(CONFIG_ECONET_EN7516) || \
      defined(CONFIG_ECONET_EN7527)
	if (!isEN751627)
#endif
		BUG();

#ifdef CONFIG_MIPS_CMDLINE_FROM_BOOTLOADER
	prom_argc = fw_arg0;
	_prom_argv = (int*) fw_arg1;
	_prom_envp = (int*) fw_arg2;
#endif

	set_io_port_base(KSEG1);

	prom_init_cmdline();

	tc_mips_setup();
	tc_uart_setup();
	tc_ahb_setup();
	tc_usb_setup();
	tc_dmt_setup();
	tc_fe_setup();

	ram_type = (VPint(CR_DMC_BASE + 0xE4) & (1 << 7)) ? "DDR3" : "DDR2";
	memsize = (GET_DRAM_SIZE) << 20;
	cpu_ratio = 4;

	if (memsize > 0x1c000000) {
		/* 1. Normal region 0..448MB */
		add_memory_region(0x20000, 0x1c000000 - 0x20000, BOOT_MEM_RAM);
#ifdef CONFIG_HIGHMEM
		/* 2. Highmem region 0x40000000..0x44000000 */
		add_memory_region(EN75XX_HIGHMEM_START, (memsize - 0x1c000000),
			BOOT_MEM_RAM);
#endif
	} else
		add_memory_region(0x20000, memsize - 0x20000, BOOT_MEM_RAM);

	bus_freq = SYS_HCLK;
	cpu_freq = bus_freq * cpu_ratio;
	if (cpu_freq % 2)
		cpu_freq += 1;

	surfboard_sysclk = bus_freq * 1000 * 1000;
	tc_mips_cpu_freq = cpu_freq * 1000 * 1000;

	printk(KERN_INFO "%s: RAM: %s %luMB\n",
		get_system_type(),
		ram_type,
		memsize / 1024 / 1024);

	printk(KERN_INFO "CPU/SYS frequency: %u/%u MHz\n",
		cpu_freq,
		bus_freq);

	board_nmi_handler_setup = mips_nmi_setup;
	cpu_dma_round_robin(ENABLE);

#if defined(CONFIG_MIPS_CMP) || \
    defined(CONFIG_MIPS_CPS)
	prom_init_cm();
#elif defined(CONFIG_MIPS_MT_SMP)
	register_vsmp_smp_ops();
#endif
	prom_show_pstat();
}

void __init prom_free_prom_memory(void)
{
	/* We do not have any memory to free */
}
