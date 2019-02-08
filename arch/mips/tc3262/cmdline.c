#include <linux/init.h>
#include <linux/string.h>

#include <asm/bootinfo.h>

#if defined(CONFIG_RT2880_UART_115200)
#define TTY_UART_CONSOLE	"console=ttyS0,115200n8"
#else
#define TTY_UART_CONSOLE	"console=ttyS0,57600n8"
#endif

#ifdef CONFIG_MIPS_CMDLINE_FROM_BOOTLOADER
extern int prom_argc;
extern int *_prom_argv;

/*
 * YAMON (32-bit PROM) pass arguments and environment as 32-bit pointer.
 * This macro take care of sign extension.
 */
#define prom_argv(index) ((char *)(((int *)(int)_prom_argv)[(index)]))
#endif

extern char arcs_cmdline[COMMAND_LINE_SIZE];

char * __init prom_getcmdline(void)
{
	return &(arcs_cmdline[0]);
}

#ifdef CONFIG_MIPS_CMDLINE_FROM_BOOTLOADER
void __init uboot_cmdline(char *s, size_t size)
{
	int i;

	for (i = 1; i < prom_argc; i++) { /* Skip argv[0] */
		strlcat(s, prom_argv(i), size);

		/* Prevent last space */
		if (i != prom_argc - 1)
			strlcat(s, " ", size);
	}
}
#else
static inline void uboot_cmdline(char *s, size_t size) {}
#endif

static inline void fixup_cmdline(char *s, size_t size)
{
	const char *p = TTY_UART_CONSOLE;

	if (strstr(s, "console="))
		return;

	/* Add space if there are symbols in buffer */
	if (*s)
		strlcat(s, " ", size);
	strlcat(s, p, size);
}

void  __init prom_init_cmdline(void)
{
	uboot_cmdline(arcs_cmdline, sizeof(arcs_cmdline));
	fixup_cmdline(arcs_cmdline, sizeof(arcs_cmdline));
}
