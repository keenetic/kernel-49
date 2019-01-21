#ifndef __XFRM_HWCRYPTO__
#define __XFRM_HWCRYPTO__

#define HWCRYPTO_OK		1
#define HWCRYPTO_NOMEM		0x80

extern void (*eip93Adapter_free)(unsigned int spi);

extern atomic_t esp_mtk_hardware;

#endif /* __XFRM_HWCRYPTO__ */
