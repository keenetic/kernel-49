#define pr_fmt(fmt)				KBUILD_MODNAME ": " fmt

#include <linux/smp.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/console.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <asm/tc3162/tc3162.h>

#define TC3162_UART_DRIVER_NAME			KBUILD_MODNAME
#define TC3162_UART_NAME			"ttyS"
#define TC3162_UART_NR_PORTS			(1)
#define TC3162_UART_SIZE			(0x30)
#define TC3162_UART_PORT			(3162)
#define TC3162_UART_TX_BUF_SIZE			UART_XMIT_SIZE
#define TC3162_UART_TX_BUF_STOP_POLL		(1024)
#define TC3162_UART_THREAD_NICE			(-20)

static DECLARE_WAIT_QUEUE_HEAD(tc3162_uart_tx_wq);
static DEFINE_SPINLOCK(tc3162_uart_tx_lock);
static DEFINE_SPINLOCK(tc3162_uart_tx_thread_lock);
static atomic_t tc3162_uart_tx_wr_idx = ATOMIC_INIT(0);
static atomic_t tc3162_uart_tx_rd_idx = ATOMIC_INIT(0);
static atomic_t tc3162_uart_tx_thread_flushing = ATOMIC_INIT(0);
static char tc3162_uart_tx_buf[TC3162_UART_TX_BUF_SIZE];
static struct task_struct *tc3162_uart_tx_thread;
static struct uart_port *tc3162_uart_port;

static inline void tc3162_uart_put(const char ch)
{
	while (!(LSR_INDICATOR & LSR_THRE))
		;
	VPchar(CR_UART_THR) = ch;
}

static inline void tc3162_uart_tx_unbuffered_char(const char ch)
{
	if (ch == '\n')
		tc3162_uart_put('\r');

	tc3162_uart_put(ch);
}

static inline void tc3162_uart_tx_unbuffered(const char *s,
					     unsigned int count)
{
	while (count-- > 0)
		tc3162_uart_tx_unbuffered_char(*s++);
}

static inline unsigned int tc3162_uart_tx_buf_capacity(void)
{
	return sizeof(tc3162_uart_tx_buf) - 1;
}

/* free slots count in the TX buffer */
static inline int tc3162_uart_tx_buf_avail(void)
{
	const int wr_idx = atomic_read(&tc3162_uart_tx_wr_idx);
	const int rd_idx = atomic_read(&tc3162_uart_tx_rd_idx);

	if (rd_idx > wr_idx)
		return rd_idx - wr_idx - 1;

	return sizeof(tc3162_uart_tx_buf) - wr_idx + rd_idx - 1;
}

static inline int tc3162_uart_tx_buf_empty(void)
{
	return  atomic_read(&tc3162_uart_tx_rd_idx) ==
		atomic_read(&tc3162_uart_tx_wr_idx);
}

static inline int tc3162_uart_tx_buf_full(void)
{
	const int wr_idx = atomic_read(&tc3162_uart_tx_wr_idx);
	const int rd_idx = atomic_read(&tc3162_uart_tx_rd_idx);

	return (wr_idx + 1) % sizeof(tc3162_uart_tx_buf) == rd_idx;
}

static inline void tc3162_uart_tx_buf_flush(unsigned int count)
{
	while (count > 0 && !tc3162_uart_tx_buf_empty()) {
		int rd_idx = atomic_read(&tc3162_uart_tx_rd_idx);

		tc3162_uart_tx_unbuffered_char(tc3162_uart_tx_buf[rd_idx]);
		rd_idx = (rd_idx + 1) % sizeof(tc3162_uart_tx_buf);

		/* a single reader does not require a lock */
		atomic_set(&tc3162_uart_tx_rd_idx, rd_idx);
		smp_wmb(); /* notify writers */

		count--;
	}
}

static void tc3162_uart_start_tx(struct uart_port *port);

static inline void tc3162_uart_tx_wakeup(void)
{
	struct uart_port *port;

	rcu_read_lock();
	port = rcu_dereference(tc3162_uart_port);

	if (port) {
		unsigned long flags;

		spin_lock_irqsave(&port->lock, flags);
		tc3162_uart_start_tx(port);
		spin_unlock_irqrestore(&port->lock, flags);
	}

	rcu_read_unlock();
}

static inline int tc3162_uart_thread_proceed(void)
{
	return !tc3162_uart_tx_buf_empty() || kthread_should_stop();
}

static int tc3162_uart_thread_fn(void *data)
{
	set_user_nice(current, TC3162_UART_THREAD_NICE);

	for (;;) {
		unsigned long flags;

		wait_event_interruptible(tc3162_uart_tx_wq,
				         tc3162_uart_thread_proceed());

		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&tc3162_uart_tx_thread_lock, flags);
		atomic_inc(&tc3162_uart_tx_thread_flushing);
		spin_unlock_irqrestore(&tc3162_uart_tx_thread_lock, flags);

		while (!tc3162_uart_tx_buf_empty()) {
			tc3162_uart_tx_buf_flush(TC3162_UART_TX_BUF_STOP_POLL);

			if (kthread_should_stop())
				break;
		}

		atomic_dec(&tc3162_uart_tx_thread_flushing);
		smp_wmb(); /* notify a console write function */

		tc3162_uart_tx_wakeup();
	}

	return 0;
}

static unsigned int tc3162_uart_tx_buf_write(const char *data,
					     unsigned int count)
{
	unsigned int size, avail;
	int rd_idx, wr_idx;

	rd_idx = atomic_read(&tc3162_uart_tx_rd_idx);
	wr_idx = atomic_read(&tc3162_uart_tx_wr_idx);

	if (rd_idx <= wr_idx) {
		avail = sizeof(tc3162_uart_tx_buf) - wr_idx;

		if (rd_idx == 0)
			avail--;

		size = min_t(unsigned int, count, avail);
		memcpy(&tc3162_uart_tx_buf[wr_idx], data, size);
		wr_idx = (wr_idx + size) % sizeof(tc3162_uart_tx_buf);
		count -= size;
		data += size;
	}

	if (count > 0 && rd_idx > wr_idx + 1) {
		avail = rd_idx - (wr_idx + 1);
		size = min_t(unsigned int, count, avail);
		memcpy(&tc3162_uart_tx_buf[wr_idx], data, size);
		wr_idx += size;
		count -= size;
	}

	atomic_set(&tc3162_uart_tx_wr_idx, wr_idx);
	smp_wmb(); /* notify a reader */

	return count;
}

static void tc3162_uart_stop_tx(struct uart_port *port)
{
}

static irqreturn_t tc3162_uart_irq(int irq, void *dev_id)
{
	struct uart_port *port = (struct uart_port *)dev_id;
	struct tty_port *tport;
	const uint8 iir = IIR_INDICATOR;

	if ((iir & IIR_RECEIVED_DATA_AVAILABLE) != IIR_RECEIVED_DATA_AVAILABLE)
		return IRQ_HANDLED;

	tport = &port->state->port;

	while (LSR_INDICATOR & LSR_RECEIVED_DATA_READY) {
		tty_insert_flip_char(tport, VPchar(CR_UART_RBR), TTY_NORMAL);
		port->icount.rx++;
	}

	tty_flip_buffer_push(tport);

	return IRQ_HANDLED;
}

static unsigned int tc3162_uart_tx_empty(struct uart_port *port)
{
	if (tc3162_uart_tx_buf_full())
		return 0;

	return TIOCSER_TEMT;
}

static void tc3162_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static unsigned int tc3162_uart_get_mctrl(struct uart_port *port)
{
	return TIOCM_CTS;
}

void tc3162_uart_send_xchar(struct uart_port *port, char ch)
{
	/* RTS and CTS are not supported, just wake up on XON */
	if (ch == START_CHAR(port->state->port.tty))
		wake_up(&tc3162_uart_tx_wq);

	port->icount.tx++;
}

/* called with disabled interrupts */
static void tc3162_uart_start_tx(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	unsigned int size;

	if (!uart_circ_chars_pending(xmit))
		return;

	spin_lock(&tc3162_uart_tx_lock);

	if (xmit->head < xmit->tail) {
		size = UART_XMIT_SIZE - xmit->tail;
		size -= tc3162_uart_tx_buf_write(&xmit->buf[xmit->tail], size);
		xmit->tail = (xmit->tail + size) & (UART_XMIT_SIZE - 1);
		port->icount.tx += size;
	}

	if (xmit->head > xmit->tail) {
		size = xmit->head - xmit->tail;
		size -= tc3162_uart_tx_buf_write(&xmit->buf[xmit->tail], size);
		xmit->tail += size;
		port->icount.tx += size;
	}

	spin_unlock(&tc3162_uart_tx_lock);

	wake_up(&tc3162_uart_tx_wq);
	uart_write_wakeup(port);
}

static void tc3162_uart_stop_rx(struct uart_port *port)
{
	VPchar(CR_UART_IER) &= ~IER_RECEIVED_DATA_INTERRUPT_ENABLE;
}

static void tc3162_uart_enable_ms(struct uart_port *port)
{
}

static void tc3162_uart_break_ctl(struct uart_port *port, int break_state)
{
}

static int tc3162_uart_startup(struct uart_port *port)
{
	const int ret = request_irq(port->irq, tc3162_uart_irq,
				    0, TC3162_UART_DRIVER_NAME, port);

	if (ret) {
		pr_err("unable to request IRQ#%d\n", port->irq);
		return ret;
	}

	rcu_assign_pointer(tc3162_uart_port, port);
	VPchar(CR_UART_IER) |= IER_RECEIVED_DATA_INTERRUPT_ENABLE;

	return 0;
}

static void tc3162_uart_shutdown(struct uart_port *port)
{
	VPchar(CR_UART_IER) &= ~IER_RECEIVED_DATA_INTERRUPT_ENABLE;
	free_irq(port->irq, port);

	RCU_INIT_POINTER(tc3162_uart_port, NULL);
	synchronize_rcu();
}

static void tc3162_uart_set_termios(struct uart_port *port,
				    struct ktermios *termios,
				    struct ktermios *old)
{
	unsigned long flags;

	termios->c_cflag |= CREAD;

	spin_lock_irqsave(&port->lock, flags);

	uart_update_timeout(port, termios->c_cflag, 115200);
	port->ignore_status_mask = 0;

	spin_unlock_irqrestore(&port->lock, flags);
}

static const char *tc3162_uart_type(struct uart_port *port)
{
	return port->type == TC3162_UART_PORT ? "TC3162" : NULL;
}

static void tc3162_uart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
		port->type = TC3162_UART_PORT;
}

static void tc3162_uart_release_port(struct uart_port *port)
{
	release_mem_region(port->iobase, TC3162_UART_SIZE);
}

static int tc3162_uart_request_port(struct uart_port *port)
{
	if (!request_mem_region(port->iobase, TC3162_UART_SIZE,
				TC3162_UART_DRIVER_NAME))
		return -EBUSY;

	return 0;
}

static const struct uart_ops tc3162_uart_ops = {
	.tx_empty	= tc3162_uart_tx_empty,
	.set_mctrl	= tc3162_uart_set_mctrl,
	.get_mctrl	= tc3162_uart_get_mctrl,
	.stop_tx	= tc3162_uart_stop_tx,
	.start_tx	= tc3162_uart_start_tx,
	.send_xchar	= tc3162_uart_send_xchar,
	.stop_rx	= tc3162_uart_stop_rx,
	.enable_ms	= tc3162_uart_enable_ms,
	.break_ctl	= tc3162_uart_break_ctl,
	.startup	= tc3162_uart_startup,
	.shutdown	= tc3162_uart_shutdown,
	.set_termios	= tc3162_uart_set_termios,
	.type		= tc3162_uart_type,
	.config_port	= tc3162_uart_config_port,
	.release_port	= tc3162_uart_release_port,
	.request_port	= tc3162_uart_request_port
};

/* called with disabled interrupts */
static void tc3162_uart_console_write(struct console *con, const char *s,
				      unsigned int count)
{
	unsigned int cpu;

	spin_lock(&tc3162_uart_tx_lock);

	if (!tc3162_uart_tx_thread) {
		tc3162_uart_tx_unbuffered(s, count);
		spin_unlock(&tc3162_uart_tx_lock);
		return;
	}

	cpu = smp_processor_id();

	while (count > 0) {
		unsigned int remain;

		spin_lock(&tc3162_uart_tx_thread_lock);

		/* tc3162_uart_tx_thread_flushing flag only is not sufficient
		 * since the TX thread may loose CPU during flushing
		 */
		if (!atomic_read(&tc3162_uart_tx_thread_flushing) ||
		    cpu == task_thread_info(tc3162_uart_tx_thread)->cpu) {
			/* the buffer can not be flushed by the TX thread */
			const unsigned long avail = tc3162_uart_tx_buf_avail();

			if (count > avail)
				tc3162_uart_tx_buf_flush(count - avail);

			if (count > tc3162_uart_tx_buf_capacity()) {
				/* write directly to the console */
				const unsigned int unbuffered =
					count - tc3162_uart_tx_buf_capacity();

				tc3162_uart_tx_unbuffered(s, unbuffered);
				s += unbuffered;
				count -= unbuffered;
			}
		}

		spin_unlock(&tc3162_uart_tx_thread_lock);

		remain = tc3162_uart_tx_buf_write(s, count);
		s += count - remain;
		count = remain;

		wake_up(&tc3162_uart_tx_wq);
	}

	spin_unlock(&tc3162_uart_tx_lock);
}

static int tc3162_uart_console_setup(struct console *con, char *options)
{
	return 0;
}

static struct uart_driver tc3162_uart_driver;

static struct console tc3162_uart_console = {
	.name		= TC3162_UART_NAME,
	.write		= tc3162_uart_console_write,
	.device		= uart_console_device,
	.setup		= tc3162_uart_console_setup,
	.flags		= CON_PRINTBUFFER,
	.cflag		= B115200 | CS8 | CREAD,
	.index		= -1,
	.data		= &tc3162_uart_driver
};

static int __init tc3162_uart_console_init(void)
{
	register_console(&tc3162_uart_console);
	return 0;
}

console_initcall(tc3162_uart_console_init);

static struct uart_port tc3162_uart_ports[TC3162_UART_NR_PORTS] = {
	{
		.iobase		= CR_UART_BASE + CR_UART_OFFSET,
		.irq		= UART_INT,
		.uartclk	= 115200,
		.fifosize	= 1,
		.ops		= &tc3162_uart_ops,
		.line		= 0,
		.flags		= ASYNC_BOOT_AUTOCONF
	}
};

static struct uart_driver tc3162_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= TC3162_UART_DRIVER_NAME,
	.dev_name	= TC3162_UART_NAME,
	.major		= TTY_MAJOR,
	.minor		= 64,
	.nr		= TC3162_UART_NR_PORTS
};

static int __init tc3162_uart_init(void)
{
	int ret;
	unsigned long flags;
	struct task_struct *tx_thread;

	pr_info("UART driver for EN751x SoC, buffer size is %lu bytes\n",
		TC3162_UART_TX_BUF_SIZE);

	tx_thread = kthread_create(tc3162_uart_thread_fn,
				   NULL, TC3162_UART_DRIVER_NAME);

	if (IS_ERR(tx_thread)) {
		pr_err("unable to create a console thread\n");
		return PTR_ERR(tx_thread);
	}

	spin_lock_irqsave(&tc3162_uart_tx_lock, flags);
	wake_up_process(tx_thread);
	tc3162_uart_tx_thread = tx_thread;
	spin_unlock_irqrestore(&tc3162_uart_tx_lock, flags);

	ret = uart_register_driver(&tc3162_uart_driver);

	if (ret) {
		pr_err("unable to register a console driver\n");
		goto stop_thread;
	}

	ret = uart_add_one_port(&tc3162_uart_driver, &tc3162_uart_ports[0]);

	if (ret) {
		pr_err("unable to register a console port\n");
		uart_unregister_driver(&tc3162_uart_driver);
		goto stop_thread;
	}

	return 0;

stop_thread:
	spin_lock_irqsave(&tc3162_uart_tx_lock, flags);
	tc3162_uart_tx_thread = NULL;
	spin_unlock_irqrestore(&tc3162_uart_tx_lock, flags);

	kthread_stop(tx_thread);

	return ret;
}

module_init(tc3162_uart_init);

MODULE_AUTHOR("Andrey Zolotarev <a.zolotarev@ndmsystems.com>, "
		"Sergey Korolev <s.korolev@ndmsystems.com>");
MODULE_DESCRIPTION("UART driver for EN751x SoC with buffered output");
MODULE_LICENSE("GPL");
