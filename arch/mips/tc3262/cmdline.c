#include <linux/init.h>
#include <linux/string.h>
#include <linux/module.h>

#include <asm/fw/fw.h>

#if defined(CONFIG_RT2880_UART_115200)
#define TTY_UART_CONSOLE	"console=ttyS0,115200n8"
#else
#define TTY_UART_CONSOLE	"console=ttyS0,57600n8"
#endif

int env_dual_image;
EXPORT_SYMBOL(env_dual_image);

static inline void fixup_env(void)
{
	const char *s = fw_getenv("dual_image");

	if (s && strcmp(s, "0") == 0)
		env_dual_image = 0;
	else
		env_dual_image = 1;
}

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
	fw_init_cmdline();
	fixup_cmdline(arcs_cmdline, sizeof(arcs_cmdline));
	fixup_env();
}
