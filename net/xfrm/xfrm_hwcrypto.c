#include <linux/module.h>
#include <asm/atomic.h>

void (*eip93Adapter_free)(unsigned int spi) = NULL;
EXPORT_SYMBOL(eip93Adapter_free);

atomic_t esp_mtk_hardware = ATOMIC_INIT(0);
EXPORT_SYMBOL(esp_mtk_hardware);
