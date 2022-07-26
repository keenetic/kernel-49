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
