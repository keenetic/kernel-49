#include <linux/atomic.h>

atomic_t xhci_uas_enable = ATOMIC_INIT(0);
EXPORT_SYMBOL(xhci_uas_enable);
