/*
 * ZyXEL Keenetic Plus DSL driver based on the Davicom DM96xx driver.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/stddef.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/crc32.h>
#include <linux/if_vlan.h>
#include <linux/usb/usbnet.h>
#include <linux/slab.h>

/* control requests */
#define DM_READ_REGS	0x00
#define DM_WRITE_REGS	0x01
#define DM_READ_MEMS	0x02
#define DM_WRITE_REG	0x03
#define DM_WRITE_MEMS	0x05
#define DM_WRITE_MEM	0x07

/* registers */
#define DM_NET_CTRL	0x00
#define DM_NET_STATUS	0x01
#define DM_RX_CTRL	0x05
#define DM_FLOW_CTRL	0x0a
#define DM_SHARED_CTRL	0x0b
#define DM_SHARED_ADDR	0x0c
#define DM_SHARED_DATA	0x0d	/* low + high */
#define DM_WAKEUP_CTRL	0x0f
#define DM_PHY_ADDR	0x10	/* 6 bytes */
#define DM_MCAST_ADDR	0x16	/* 8 bytes */
#define DM_GPR_CTRL	0x1e
#define DM_GPR_DATA	0x1f

#define DM_TX_CRC_CTRL	0x31
#define DM_RX_CRC_CTRL	0x32
#define DM_USB_CTRL	0xf4

#define DM_PHY_SPEC_CFG	20
#define DM_TXRX_M	0x5c

#define DM_MAX_MCAST	64
#define DM_MCAST_SIZE	8
#define DM_EEPROM_LEN	128
#define DM_TX_OVERHEAD	2	/* 2 byte header */
#define DM_RX_OVERHEAD	8	/* 4 byte header + 4 byte crc tail */
#define DM_TIMEOUT	1000

#define DM_9620_PHY_ID	1	/* read phy register */

#define DM_LINKEN	(1 << 5)
#define DM_MAGICEN	(1 << 3)
#define DM_LINKST	(1 << 2)
#define DM_MAGICST	(1 << 0)

#define DM_KPDSL_OFFS	0x80

struct kpdsl_info {
	char servicetag[0x20];
	char servicehost[0x30];
	char servicepass[0x20];
	char ndmhwid[0x10];
	char _reserved[0x10];
	char checksum[0x22];
};

static inline int
dm_read(struct usbnet *dev, u8 reg, u16 length, void *data)
{
	int err;

	err = usbnet_read_cmd(dev, DM_READ_REGS,
			      USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			      0, reg, data, length);

	if (err != length && err >= 0)
		err = -EINVAL;

	return err;
}

static inline int
dm_read_reg(struct usbnet *dev, u8 reg, u8 *value)
{
	return dm_read(dev, reg, 1, value);
}

static inline int
dm_write(struct usbnet *dev, u8 reg, u16 length, void *data)
{
	int err;

	err = usbnet_write_cmd(dev, DM_WRITE_REGS,
			       USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			       0, reg, data, length);

	if (err >= 0 && err < length)
		err = -EINVAL;

	return err;
}

static inline int dm_write_reg(struct usbnet *dev, u8 reg, u8 value)
{
	return usbnet_write_cmd(dev, DM_WRITE_REG,
				USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
				value, reg, NULL, 0);
}

static inline void
dm_write_async(struct usbnet *dev, u8 reg, u16 length, void *data)
{
	usbnet_write_cmd_async(dev, DM_WRITE_REGS,
			       USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			       0, reg, data, length);
}

static inline void dm_write_reg_async(struct usbnet *dev, u8 reg, u8 value)
{
	usbnet_write_cmd_async(dev, DM_WRITE_REG,
			       USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			       value, reg, NULL, 0);
}

static int dm_read_shared_word(struct usbnet *dev, int phy, u8 reg, __le16 *value)
{
	int ret, i;

	mutex_lock(&dev->phy_mutex);

	dm_write_reg(dev, DM_SHARED_ADDR, phy ? (reg | 0x40) : reg);
	dm_write_reg(dev, DM_SHARED_CTRL, phy ? 0xc : 0x4);

	for (i = 0; i < DM_TIMEOUT; i++) {
		u8 tmp = 0;

		udelay(1);
		ret = dm_read_reg(dev, DM_SHARED_CTRL, &tmp);
		if (ret < 0)
			goto out;

		/* ready */
		if ((tmp & 1) == 0)
			break;
	}

	if (i == DM_TIMEOUT) {
		netdev_err(dev->net, "%s read timed out\n",
			   phy ? "PHY" : "EEPROM");
		ret = -EIO;
		goto out;
	}

	dm_write_reg(dev, DM_SHARED_CTRL, 0x0);
	ret = dm_read(dev, DM_SHARED_DATA, 2, value);

	netdev_dbg(dev->net, "read shared %d 0x%02x returned 0x%04x, %d\n",
		   phy, reg, *value, ret);

out:
	mutex_unlock(&dev->phy_mutex);
	return ret;
}

static int dm_write_shared_word(struct usbnet *dev, int phy, u8 reg, __le16 value)
{
	int ret, i;

	mutex_lock(&dev->phy_mutex);

	ret = dm_write(dev, DM_SHARED_DATA, 2, &value);
	if (ret < 0)
		goto out;

	dm_write_reg(dev, DM_SHARED_ADDR, phy ? (reg | 0x40) : reg);

	if (!phy)
		dm_write_reg(dev, DM_SHARED_CTRL, 0x10);

	dm_write_reg(dev, DM_SHARED_CTRL, phy ? 0x0a : 0x12);
	dm_write_reg(dev, DM_SHARED_CTRL, 0x10);

	for (i = 0; i < DM_TIMEOUT; i++) {
		u8 tmp = 0;

		udelay(1);
		ret = dm_read_reg(dev, DM_SHARED_CTRL, &tmp);
		if (ret < 0)
			goto out;

		/* ready */
		if ((tmp & 1) == 0)
			break;
	}

	if (i == DM_TIMEOUT) {
		netdev_err(dev->net, "%s write timed out\n",
			   phy ? "PHY" : "EEPROM");
		ret = -EIO;
		goto out;
	}

	dm_write_reg(dev, DM_SHARED_CTRL, 0x0);

out:
	mutex_unlock(&dev->phy_mutex);
	return ret;
}

static int dm_read_eeprom_word(struct usbnet *dev, u8 offset, void *value)
{
	return dm_read_shared_word(dev, 0, offset, value);
}

static int kpdsl_get_eeprom_len(struct net_device *dev)
{
	return DM_EEPROM_LEN;
}

static int kpdsl_get_eeprom(struct net_device *net,
			    struct ethtool_eeprom *eeprom, u8 *data)
{
	struct usbnet *dev = netdev_priv(net);
	__le16 *ebuf = (__le16 *)data;
	int i;

	/* access is 16bit */
	if ((eeprom->offset % 2) || (eeprom->len % 2))
		return -EINVAL;

	for (i = 0; i < eeprom->len / 2; i++) {
		if (dm_read_eeprom_word(dev, eeprom->offset / 2 + i,
					&ebuf[i]) < 0)
			return -EINVAL;
	}

	return 0;
}

static int kpdsl_mdio_read(struct net_device *netdev, int phy_id, int loc)
{
	struct usbnet *dev = netdev_priv(netdev);
	u8 val;
	__le16 res;

	/* since REG DM_NET_CTRL/DM_NET_STATUS is the final result,
	 * no matter internal PHY or EXT MII */
	dm_read_reg(dev, DM_NET_CTRL, &val);

	if (val & 0x80) { /* EXT MII */
		if (loc == MII_BMCR) {
			u16 ret = 0;

			if (val & 0x08) /* duplex mode */
				ret |= BMCR_FULLDPLX;

			dm_read_reg(dev, DM_NET_STATUS, &val);

			if (!(val & 0x80)) /* speed 10/100 */
				ret |= BMCR_SPEED100;

			return ret;
		}

		if (loc == MII_BMSR) {
			const u16 ret =
				BMSR_ERCAP       |
				BMSR_ANEGCAPABLE |
				BMSR_10HALF      |
				BMSR_10FULL      |
				BMSR_100HALF     |
				BMSR_100FULL;

			dm_read_reg(dev, DM_NET_STATUS, &val);

			if (val & 0x40) /* link status */
				return ret | BMSR_LSTATUS;

			return ret;
		}

		if (loc == MII_ADVERTISE)
			return ADVERTISE_CSMA      |
			       ADVERTISE_10HALF    |
			       ADVERTISE_10FULL    |
			       ADVERTISE_100HALF   |
			       ADVERTISE_100FULL   |
			       ADVERTISE_PAUSE_CAP;

		if (loc == MII_LPA) /* link partner ability */
			return LPA_10HALF    |
			       LPA_10FULL    |
			       LPA_100HALF   |
			       LPA_100FULL   |
			       LPA_PAUSE_CAP |
			       LPA_LPACK;
	}

	dm_read_shared_word(dev, phy_id, loc, &res);

	netdev_dbg(dev->net,
		   "kpdsl_mdio_read() phy_id=0x%02x, loc=0x%02x, returns=0x%04x\n",
		   phy_id, loc, le16_to_cpu(res));

	return le16_to_cpu(res);
}

static void kpdsl_mdio_write(struct net_device *netdev, int phy_id, int loc,
			     int val)
{
	struct usbnet *dev = netdev_priv(netdev);
	__le16 res = cpu_to_le16(val);
	int mdio_val;

	netdev_dbg(dev->net, "kpdsl_mdio_write() phy_id=0x%02x, loc=0x%02x, val=0x%04x\n",
		   phy_id, loc, val);

	dm_write_shared_word(dev, phy_id, loc, res);
	mdelay(1);
	mdio_val = kpdsl_mdio_read(netdev, phy_id, loc);
}

static u32 kpdsl_get_link(struct net_device *net)
{
	struct usbnet *dev = netdev_priv(net);

	return mii_link_ok(&dev->mii);
}

static int kpdsl_do_ioctl(struct net_device *net, struct ifreq *rq, int cmd)
{
	struct usbnet *dev = netdev_priv(net);

	/* MT2311 GPIO (SMI_D) low */
	if (cmd == SIOCDEVPRIVATE + 0)
		return dm_write_reg(dev, DM_GPR_DATA, 0);

	/* MT2311 GPIO (SMI_D) high */
	if (cmd == SIOCDEVPRIVATE + 1)
		return dm_write_reg(dev, DM_GPR_DATA, 1 << 3);

	if (cmd == SIOCDEVPRIVATE + 4) {
		struct kpdsl_info *info;
		struct ethtool_eeprom eeprom = {0};

		info = kmalloc(sizeof(*info), GFP_TEMPORARY);
		if (info == NULL)
			return -ENOMEM;

		eeprom.offset = DM_KPDSL_OFFS;
		eeprom.len = sizeof(*info);

		if (kpdsl_get_eeprom(net, &eeprom, (u8 *)info) < 0) {
			kfree(info);
			return -EIO;
		}

		if (copy_to_user(rq->ifr_data, info, sizeof(*info))) {
			kfree(info);
			return -EFAULT;
		}

		kfree(info);
		return 0;
	}

	return generic_mii_ioctl(&dev->mii, if_mii(rq), cmd, NULL);
}

static void
kpdsl_get_wol(struct net_device *net, struct ethtool_wolinfo *wolinfo)
{
	struct usbnet *dev = netdev_priv(net);
	u8 opt;

	if (dm_read_reg(dev, DM_WAKEUP_CTRL, &opt) < 0) {
		wolinfo->supported = 0;
		wolinfo->wolopts = 0;
		return;
	}

	wolinfo->supported = WAKE_PHY | WAKE_MAGIC;
	wolinfo->wolopts = 0;

	if (opt & DM_LINKEN)
		wolinfo->wolopts |= WAKE_PHY;

	if (opt & DM_MAGICEN)
		wolinfo->wolopts |= WAKE_MAGIC;
}

static int
kpdsl_set_wol(struct net_device *net, struct ethtool_wolinfo *wolinfo)
{
	struct usbnet *dev = netdev_priv(net);
	u8 opt = 0;

	if (wolinfo->wolopts & WAKE_PHY)
		opt |= DM_LINKEN;

	if (wolinfo->wolopts & WAKE_MAGIC)
		opt |= DM_MAGICEN;

	dm_write_reg(dev, DM_NET_CTRL, 0x48); /* enable WAKEEN */

	return dm_write_reg(dev, DM_WAKEUP_CTRL, opt);
}

static const struct ethtool_ops kpdsl_ethtool_ops = {
	.get_drvinfo	= usbnet_get_drvinfo,
	.get_link	= kpdsl_get_link,
	.get_msglevel	= usbnet_get_msglevel,
	.set_msglevel	= usbnet_set_msglevel,
	.get_eeprom_len	= kpdsl_get_eeprom_len,
	.get_eeprom	= kpdsl_get_eeprom,
	.get_settings	= usbnet_get_settings,
	.set_settings	= usbnet_set_settings,
	.nway_reset	= usbnet_nway_reset,
	.get_wol	= kpdsl_get_wol,
	.set_wol	= kpdsl_set_wol
};

static void kpdsl_set_multicast(struct net_device *net)
{
	struct usbnet *dev = netdev_priv(net);
	/* We use the 20 byte dev->data for our 8 byte filter buffer
	 * to avoid allocating memory that is tricky to free later */
	u8 *hashes = (u8 *)&dev->data;
	u8 rx_ctl = 0x31;

	/* RUNT: pass RX packets < 64 bytes */
	rx_ctl |= 0x04;

	memset(hashes, 0x00, DM_MCAST_SIZE);
	hashes[DM_MCAST_SIZE - 1] |= 0x80;	/* broadcast address */

	if (net->flags & IFF_PROMISC) {
		rx_ctl |= 0x02;
	} else if (net->flags & IFF_ALLMULTI ||
		   netdev_mc_count(net) > DM_MAX_MCAST) {
		rx_ctl |= 0x08;
	} else if (!netdev_mc_empty(net)) {
		struct netdev_hw_addr *ha;

		netdev_for_each_mc_addr(ha, net) {
			u32 crc = crc32_le(~0, ha->addr, ETH_ALEN) & 0x3f;
			hashes[crc >> 3] |= 1 << (crc & 0x7);
		}
	}

	dm_write_async(dev, DM_MCAST_ADDR, DM_MCAST_SIZE, hashes);
	dm_write_reg_async(dev, DM_RX_CTRL, rx_ctl);
}

static inline void __kpdsl_set_mac_address(struct usbnet *dev)
{
	dm_write_async(dev, DM_PHY_ADDR, ETH_ALEN, dev->net->dev_addr);
}

static int kpdsl_set_mac_address(struct net_device *net, void *p)
{
	struct usbnet *dev = netdev_priv(net);

	memcpy(net->dev_addr, p, net->addr_len);
	__kpdsl_set_mac_address(dev);

	return 0;
}

static int kpdsl_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < 68 || new_mtu > ETH_DATA_LEN)
		return -EINVAL;

	return usbnet_change_mtu(dev, new_mtu);
}

static const struct net_device_ops kpdsl_netdev_ops = {
	.ndo_open		= usbnet_open,
	.ndo_stop		= usbnet_stop,
	.ndo_start_xmit		= usbnet_start_xmit,
	.ndo_tx_timeout		= usbnet_tx_timeout,
	.ndo_change_mtu		= kpdsl_change_mtu,
	.ndo_get_stats64	= usbnet_get_stats64,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_do_ioctl		= kpdsl_do_ioctl,
	.ndo_set_rx_mode	= kpdsl_set_multicast,
	.ndo_set_mac_address	= kpdsl_set_mac_address
};

static int kpdsl_bind(struct usbnet *dev, struct usb_interface *intf)
{
	int ret, mdio_val;
	u8 val;

	ret = usbnet_get_endpoints(dev, intf);
	if (ret)
		goto out;

	dev->net->netdev_ops = &kpdsl_netdev_ops;
	dev->net->ethtool_ops = &kpdsl_ethtool_ops;
	dev->net->hard_header_len += DM_TX_OVERHEAD;
	dev->hard_mtu = dev->net->mtu + dev->net->hard_header_len;
	dev->rx_urb_size = ETH_DATA_LEN   +	/* maximum ethernet MTU */
			   VLAN_ETH_HLEN  +	/* ethernet header + VLAN */
			   VLAN_HLEN      +	/* 2nd VLAN */
			   DM_RX_OVERHEAD;	/* usbnet RX fixup */

	dev->mii.dev = dev->net;
	dev->mii.mdio_read = kpdsl_mdio_read;
	dev->mii.mdio_write = kpdsl_mdio_write;
	dev->mii.phy_id_mask = 0x1f;
	dev->mii.reg_num_mask = 0x1f;
	dev->mii.phy_id = DM_9620_PHY_ID;

	/* reset */
	dm_write_reg(dev, DM_NET_CTRL, 1);
	udelay(20);

	/* enable "MAC layer" flow control, TX pause packet enable */
	dm_write_reg(dev, DM_FLOW_CTRL, 0x29);

	/* enable "PHY layer" flow control support (phy register 0x04 bit 10) */
	mdio_val = kpdsl_mdio_read(dev->net, dev->mii.phy_id, 0x04);
	kpdsl_mdio_write(dev->net, dev->mii.phy_id, 0x04, mdio_val | 0x400);

	/* enable auto link while plug in RJ45, Hank July 20, 2009 */
	dm_write_reg(dev, DM_USB_CTRL, 0x20);

	/* read MAC */
	if (dm_read(dev, DM_PHY_ADDR, ETH_ALEN, dev->net->dev_addr) < 0) {
		netdev_err(dev->net, "error reading MAC address\n");
		ret = -ENODEV;
		goto out;
	}

	/* need to check the chipset version */
	dm_read_reg(dev, DM_TXRX_M, &val);

	if (val == 0x02) {
		dm_read_reg(dev, 0x3f, &val);
		dm_write_reg(dev, 0x3f, val | 0x80);
	}

	/* power up phy */
	/* enable MT2311 reset line control via DM96xx GPIO[3] */
	dm_write_reg(dev, DM_GPR_CTRL, 1 << 3);
	dm_write_reg(dev, DM_GPR_DATA, 0);

	/* init RX checksum control */
	dm_write_reg(dev, DM_RX_CRC_CTRL, 2);

	/* receive broadcast packets */
	kpdsl_set_multicast(dev->net);

	kpdsl_mdio_write(dev->net, dev->mii.phy_id, MII_BMCR, BMCR_RESET);

	/* fix compatibility issue (10M power control) */
	kpdsl_mdio_write(dev->net, dev->mii.phy_id, DM_PHY_SPEC_CFG, 0x800);
	mdio_val = kpdsl_mdio_read(dev->net, dev->mii.phy_id, DM_PHY_SPEC_CFG);
	kpdsl_mdio_write(dev->net, dev->mii.phy_id, MII_ADVERTISE,
			 ADVERTISE_ALL | ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP);
	mii_nway_restart(&dev->mii);

out:
	return ret;
}

static int kpdsl_rx_fixup(struct usbnet *dev, struct sk_buff *skb)
{
	u8 status;
	int len;

	/* 9620 format:
	   b1: rx status
	   b2: packet length (incl crc) low
	   b3: packet length (incl crc) high
	   b4..bn-4: packet data
	   bn-3..bn: ethernet crc
	 */

	if (unlikely(skb->len < DM_RX_OVERHEAD)) {
		netdev_err(dev->net, "unexpected tiny RX frame\n");
		return 0;
	}

	status = skb->data[1];
	len = (skb->data[2] | (skb->data[3] << 8)) - 4;

	/* bit[7] RUNT packets - accept it (thanks to Ian from Metanoia) */
	if (unlikely(status & 0x3f)) {
		if (status & 0x01)
			dev->net->stats.rx_fifo_errors++;
		if (status & 0x02)
			dev->net->stats.rx_crc_errors++;
		if (status & 0x04)
			dev->net->stats.rx_frame_errors++;
		if (status & 0x10)
			dev->net->stats.rx_length_errors++;
		if (status & 0x20)
			dev->net->stats.rx_missed_errors++;
		return 0;
	}

	/* drop RUNT packets < 14 bytes */
	if (unlikely(len < ETH_HLEN)) {
		dev->net->stats.rx_length_errors++;
		return 0;
	}

	skb_pull(skb, 4);
	skb_trim(skb, len);

	return 1;
}

static struct sk_buff *kpdsl_tx_fixup(struct usbnet *dev, struct sk_buff *skb,
				      gfp_t flags)
{
	int len, pad;

	/* format:
	   b0: packet length low
	   b1: packet length high
	   b2..bn-1: packet data
	*/

	len = skb->len + DM_TX_OVERHEAD;

	/* workaround for dm962x errata with tx fifo getting out of
	 * sync if a USB bulk transfer retry happens right after a
	 * packet with odd / maxpacket length by adding up to 3 bytes
	 * padding.
	 */
	while ((len & 1) || !(len % dev->maxpacket))
		len++;

	len -= DM_TX_OVERHEAD; /* hw header doesn't count as part of length */
	pad = len - skb->len;

	if (skb_headroom(skb) < DM_TX_OVERHEAD || skb_tailroom(skb) < pad) {
		struct sk_buff *skb2;

		skb2 = skb_copy_expand(skb, DM_TX_OVERHEAD, pad, flags);
		dev_kfree_skb_any(skb);
		skb = skb2;
		if (!skb)
			return NULL;
	}

	__skb_push(skb, DM_TX_OVERHEAD);

	if (pad) {
		memset(skb->data + skb->len, 0, pad);
		__skb_put(skb, pad);
	}

	skb->data[0] = len;
	skb->data[1] = len >> 8;

	return skb;
}

static void kpdsl_status(struct usbnet *dev, struct urb *urb)
{
	int link;
	u8 *buf;

	/* format:
	   b0: net status
	   b1: tx status 1
	   b2: tx status 2
	   b3: rx status
	   b4: rx overflow
	   b5: rx count
	   b6: tx count
	   b7: gpr
	*/

	if (urb->actual_length < 8)
		return;

	buf = urb->transfer_buffer;

	link = !!(buf[0] & 0x40);
	if (netif_carrier_ok(dev->net) != link) {
		usbnet_link_change(dev, link, 1);
		netdev_dbg(dev->net, "kpdsl_status() link status is: %d\n",
			   link);
	}
}

static int kpdsl_link_reset(struct usbnet *dev)
{
	struct ethtool_cmd ecmd = { .cmd = ETHTOOL_GSET };

	mii_check_media(&dev->mii, 1, 1);
	mii_ethtool_gset(&dev->mii, &ecmd);

	/* hank add */
	kpdsl_mdio_write(dev->net, dev->mii.phy_id, DM_PHY_SPEC_CFG, 0x800);

	netdev_dbg(dev->net, "link_reset() speed: %u duplex: %d\n",
		   ethtool_cmd_speed(&ecmd), ecmd.duplex);

	return 0;
}

static const struct driver_info kplusdsl_info = {
	.description    = "ZyXEL Keenetic Plus DSL",
	.flags		= FLAG_ETHER | FLAG_LINK_INTR,
	.bind		= kpdsl_bind,
	.rx_fixup	= kpdsl_rx_fixup,
	.tx_fixup	= kpdsl_tx_fixup,
	.status		= kpdsl_status,
	.link_reset	= kpdsl_link_reset,
	.reset		= kpdsl_link_reset
};

static const struct usb_device_id products[] = {
	{
	 USB_DEVICE(0x0586, 0x3427),	/* ZyXEL Keenetic Plus DSL */
	 .driver_info = (unsigned long)&kplusdsl_info,
	},
	{},				/* END */
};

MODULE_DEVICE_TABLE(usb, products);

static struct usb_driver kpdsl_driver = {
	.name			   = "kpdsl",
	.id_table		   = products,
	.probe			   = usbnet_probe,
	.disconnect		   = usbnet_disconnect,
	.suspend		   = usbnet_suspend,
	.resume			   = usbnet_resume,
	.disable_hub_initiated_lpm = 1,
};

module_usb_driver(kpdsl_driver);

MODULE_AUTHOR("Sergey Korolev <s.korolev@ndmsystems.com>");
MODULE_DESCRIPTION("ZyXEL Keenetic Plus DSL");
MODULE_LICENSE("GPL");
