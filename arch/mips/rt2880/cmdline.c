/**************************************************************************
 *
 *  BRIEF MODULE DESCRIPTION
 *     cmdline parsing for Ralink RT2880 solution
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
