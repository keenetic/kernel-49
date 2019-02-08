#ifndef __DEV_XHCI_H__
#define __DEV_XHCI_H__

#define XHCI_MTK_DRV_NAME	"xhci-mtk"

struct xhci_mtk_pdata {
	void (*uphy_init)(void);

	bool	usb3_lpm_capable;
};

#endif /* __DEV_XHCI_H__ */
