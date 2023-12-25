#ifndef _WARP_WIFI_COMMON_H_
#define _WARP_WIFI_COMMON_H_

/* default SER status */
#define WIFI_ERR_RECOV_NONE 			0x10
#define WIFI_ERR_RECOV_STOP_IDLE 		0
#define WIFI_ERR_RECOV_STOP_PDMA0 		1
#define WIFI_ERR_RECOV_RESET_PDMA0 		2
#define WIFI_ERR_RECOV_STOP_IDLE_DONE 		3
#define WIFI_ETH_ERR_STOP_WED_RX_TRAFFIC	4
#define WIFI_ETH_ERR_START_WED_RX_TRAFFIC	5

enum {
	BUS_TYPE_PCIE,
	BUS_TYPE_AXI,
	BUS_TYPE_MAX
};

enum {
	WIFI_FREE_IRQ = 0,
	WIFI_REQUEST_IRQ = 1,
};

enum {
	WIFI_HW_CAP_PAO,
	WIFI_HW_CAP_RRO,
};

struct warp_slot_info {
	u8 wed_id;
	u8 wdma_rx_port;
	u8 wdma_tx_port;
	u8 hw_tx_en;
};

struct wlan_tx_info {
	void *pkt;
	u16 wcid;
	u8 bssidx;
	u8 ringidx;
	u32 usr_info;
};

struct wlan_rx_info {
	void *pkt;
	u16 ppe_entry;
	u8 csrn;
};

struct wo_cmd_rxcnt_t {
	u16 wlan_idx;
	u16 tid;
	u32 rx_pkt_cnt;
	u32 rx_byte_cnt;
	u32 rx_err_cnt;
	u32 rx_drop_cnt;
};

struct ring_ctrl {
	u32 base;
	u32 cnt;
	u32 cidx;
	u32 didx;
	u32 lens;
	dma_addr_t cb_alloc_pa;
};

struct rro_ctrl {
	u16 max_win_sz;
	u32 ack_sn_cr;
	u32 particular_se_id;
	dma_addr_t particular_se_base;
	u8 se_group_num;
	dma_addr_t se_base[128];
};

struct wifi_hw {
	void *priv;
	void *hif_dev;
	u8 non_coherent_dma_addr_size;
	u8 coherent_dma_addr_size;
	u32 *p_int_mask;
	u32 chip_id;
	u32 tx_ring_num;
	u32 tx_token_nums;
	u32 tx_buf_nums;
	u32 sw_tx_token_nums;
	u32 hw_rx_token_num;
	u32 rx_page_nums;
	unsigned long dma_offset;
	u32 int_sta;
	u32 int_mask;
	u32 tx_dma_glo_cfg;
	u32 ring_offset;
	u32 txd_size;
	u32 fbuf_size;
	u32 tx_pkt_size;
	u32 tx_ring_size;
	u32 rx_ring_size;
	u32 int_ser;
	u32 int_ser_value;
	struct ring_ctrl tx[2];
	struct ring_ctrl event;

	u32 rx_dma_glo_cfg;
	u32 rx_ring_num;
	u32 rx_data_ring_size;
	u32 rxd_size;
	u32 rx_pkt_size;
	u16 rx_page_size;
	u32 max_rxd_size;
	u32 sign_base_cr;
	u8 rx_rro_data_ring_num;
	u8 rx_rro_page_ring_num;
	u8 rx_rro_ind_cmd_ring_num;
	u8 tx_free_done_ver;
	struct ring_ctrl rx[2];
	struct ring_ctrl rx_rro[2];
	struct ring_ctrl rx_rro_pg[3];
	struct ring_ctrl rx_rro_ind_cmd;
	struct rro_ctrl rro_ctl;

	unsigned long wpdma_base;
	unsigned long base_phy_addr;
	unsigned long base_addr;

	u8 wfdma_tx_done_trig0_bit;
	u8 wfdma_tx_done_trig1_bit;
	u8 wfdma_tx_done_free_notify_trig_bit;
	u8 wfdma_rx_done_trig0_bit;
	u8 wfdma_rx_done_trig0_enable;
	u8 wfdma_rx_done_trig1_bit;
	u8 wfdma_rx_done_trig1_enable;
	u8 wfdma_rro_rx_done_trig0_bit;
	u8 wfdma_rro_rx_done_trig1_bit;
	u8 wfdma_rro_rx_pg_trig0_bit;
	u8 wfdma_rro_rx_pg_trig1_bit;
	u8 wfdma_rro_rx_pg_trig2_bit;

	u32 hif_type;
	u32 irq;
	u32 hw_cap;
	u32 max_amsdu_len;
	u32 max_wcid_nums;
	u32 wed_v1_compatible_en_addr;
	u32 wed_v1_compatible_en_msk;
	u32 wed_v1_compatible_tx0_addr;
	u32 wed_v1_compatible_tx0_msk;
	u32 wed_v1_compatible_tx0_id;
	u32 wed_v1_compatible_tx1_addr;
	u32 wed_v1_compatible_tx1_msk;
	u32 wed_v1_compatible_tx1_id;
	u32 wed_v1_compatible_rx1_addr;
	u32 wed_v1_compatible_rx1_msk;
	u32 wed_v1_compatible_rx1_id;

	u8 max_amsdu_nums;
	u8 mac_ver;
	u8 src;
	bool msi_enable;
	bool dbdc_mode;
	bool rm_vlan;
	bool hdtr_mode;
};

struct wifi_ops {
	void (*config_atc)(void *priv_data, bool enable);
	void (*swap_irq)(void *priv_data, u32 irq);
	void (*fbuf_init)(u8 *fbuf, u32 pkt_pa, u32 tkid);
	void (*txinfo_wrapper)(u8 *tx_info, struct wlan_tx_info *info);
	void (*txinfo_set_drop)(u8 *tx_info);
	bool (*hw_tx_allow)(u8 *tx_info);
	void (*tx_ring_info_dump)(void *priv_data, u8 ring_id, u32 idx);
	void (*warp_ver_notify)(void *priv_data, u8 ver, u8 warp_sub_ver, u8 warp_branch, int warp_hw_caps);
	u32 (*token_rx_dmad_init)(void *priv_data, void *pkt, unsigned long alloc_size,
				  void *alloc_va, dma_addr_t alloc_pa);
	int (*token_rx_dmad_lookup)(void *priv_data, u32 tkn_rx_id, void **pkt,
				    void **alloc_va, dma_addr_t *alloc_pa);
	void (*rxinfo_wrapper)(u8 *rx_info, struct wlan_rx_info *info);
	void (*do_wifi_reset)(void *priv_data);
	void (*update_wo_rxcnt)(void *priv_data, struct wo_cmd_rxcnt_t *wo_rxcnt, u8 num);
	void (*request_irq)(void *priv_data, u32 irq);
	void (*free_irq)(void *priv_data, u32 irq);
	void (*hb_check_notify)(void *priv_data);
	void (*fbuf_v1_init)(u8 *fbuf, dma_addr_t pkt_pa, u32 tkid, u8 src);
};

#endif /*_WARP_WIFI_COMMON_H_*/
