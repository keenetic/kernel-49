#ifndef _WIFI_MAIL_TYPE_H_
#define _WIFI_MAIL_TYPE_H_

#define INTERFACE_2G		0
#define INTERFACE_5G		1

#define MAX_SSID_PER_BAND	8

#define MAX_PER_ENTRY		128

enum {
	WIFI_MAIL_FUNCTION_SET_WAIT_PCIE_ADDR = 0,
	WIFI_MAIL_FUNCTION_SET_WAIT_DESC,
	WIFI_MAIL_FUNCTION_SET_WAIT_NPU_INIT_DONE,
	WIFI_MAIL_FUNCTION_SET_WAIT_TRAN_TO_CPU,
	WIFI_MAIL_FUNCTION_SET_WAIT_BA_WIN_SIZE,
	WIFI_MAIL_FUNCTION_SET_WAIT_DRIVER_MODEL,
	WIFI_MAIL_FUNCTION_SET_WAIT_DEL_STA,
	WIFI_MAIL_FUNCTION_SET_WAIT_DRAM_BA_NODE_ADDR,
	WIFI_MAIL_FUNCTION_SET_WAIT_PKT_BUF_ADDR,
	WIFI_MAIL_FUNCTION_SET_WAIT_IS_TEST_NOBA,
	WIFI_MAIL_FUNCTION_SET_WAIT_FLUSHONE_TIMEOUT,
	WIFI_MAIL_FUNCTION_SET_WAIT_FLUSHALL_TIMEOUT,
	WIFI_MAIL_FUNCTION_SET_WAIT_IS_FORCE_TO_CPU,
	WIFI_MAIL_FUNCTION_SET_WAIT_PCIE_STATE,
	WIFI_MAIL_FUNCTION_SET_WAIT_PCIE_PORT_TYPE,
	WIFI_MAIL_FUNCTION_SET_WAIT_ERROR_RETRY_TIMES,
	WIFI_MAIL_FUNCTION_SET_WAIT_BAR_INFO,
	WIFI_MAIL_FUNCTION_SET_WAIT_FAST_FLAG,
	WIFI_MAIL_FUNCTION_SET_WAIT_NPU_BAND0_ONCPU,
	WIFI_MAIL_FUNCTION_SET_WAIT_TX_RING_PCIE_ADDR,
	WIFI_MAIL_FUNCTION_SET_WAIT_TX_DESC_HW_BASE,
	WIFI_MAIL_FUNCTION_SET_WAIT_TX_BUF_SPACE_HW_BASE,
	WIFI_MAIL_FUNCTION_SET_WAIT_RX_RING_FOR_TXDONE_HW_BASE,
	WIFI_MAIL_FUNCTION_SET_WAIT_TX_PKT_BUF_ADDR,
	WIFI_MAIL_FUNCTION_SET_WAIT_INODE_TXRX_REG_ADDR,
	WIFI_MAIL_FUNCTION_SET_WAIT_INODE_DEBUG_FLAG,
	WIFI_MAIL_FUNCTION_SET_WAIT_INODE_HW_CFG_INFO,
	WIFI_MAIL_FUNCTION_SET_WAIT_INODE_STOP_ACTION,
	WIFI_MAIL_FUNCTION_SET_WAIT_INODE_PCIE_SWAP,
	WIFI_MAIL_FUNCTION_SET_WAIT_RATELIMIT_CTRL,
	WIFI_MAIL_FUNCTION_SET_WAIT_MAX_NUM
};

enum {
	WIFI_MAIL_FUNCTION_SET_NO_WAIT_BASE = 0,
	WIFI_MAIL_FUNCTION_SET_NO_WAIT_MAX_NUM
};

enum {
	WIFI_MAIL_FUNCTION_GET_WAIT_NPU_INFO = 0,
	WIFI_MAIL_FUNCTION_GET_WAIT_LAST_RATE,
	WIFI_MAIL_FUNCTION_GET_WAIT_COUNTER,
	WIFI_MAIL_FUNCTION_GET_WAIT_DBG_COUNTER,
	WIFI_MAIL_FUNCTION_GET_WAIT_RXDESC_BASE,
	WIFI_MAIL_FUNCTION_GET_WAIT_WCID_DBG_COUNTER,
	WIFI_MAIL_FUNCTION_GET_WAIT_DMA_ADDR,
	WIFI_MAIL_FUNCTION_GET_WAIT_RING_SIZE,
	WIFI_MAIL_FUNCTION_GET_WAIT_MAX_NUM
};

enum {
	WIFI_MAIL_FUNCTION_GET_NO_WAIT_BASE = 0,
	WIFI_MAIL_FUNCTION_GET_NO_WAIT_MAX_NUM
};

enum {
	SET_WAIT = 1,
	SET_NO_WAIT,
	GET_WAIT,
	GET_NO_WAIT,
	OP_MAX_NUM
};

struct npu_info {
	u32 info;
};

struct last_rate {
	u32 per_sta;
	u32 total;
};

struct counter {
	u32 txCount;
	u64 rxCount2G[MAX_SSID_PER_BAND];
	u64 rxCount5G[MAX_SSID_PER_BAND];
	u64 rxByteCount2G[MAX_SSID_PER_BAND];
	u64 rxByteCount5G[MAX_SSID_PER_BAND];
	u8  omacIdx5G[MAX_SSID_PER_BAND];
	u8  omacIdx2G[MAX_SSID_PER_BAND];
	u64 rxApcliCount2G;
	u64 rxApcliCount5G;
	u64 rxApcliByteCount2G;
	u64 rxApcliByteCount5G;
	u64 rx_pkts_entry[2][MAX_PER_ENTRY];
	u64 rx_bytes_entry[2][MAX_PER_ENTRY];
};

struct ratelimit_ctrl {
	u32 band_idx;
	u32 bssid_idx;
	u32 ctrl;
};

struct dbg_counter {
	u32 reg_count;
};

struct inode_txrx_addr {
	u32 dir;
	u32 in_counter_addr;
	u32 out_status_addr;
	u32 out_counter_addr;
};

struct inode_cfg_info {
	u8 ep_mask;
	u8 vap_mask;
};

enum {
	MT7915_A = 0,
	MT7915_D,
	MT7916_D,
	D_BOTTOM_
};

enum {
	P0 = 0,
	P1,
	P0_P0,
	P0_P1
};

struct wifi_mail_data {
	u32 interface_id:4;
	u32 func_type:4;
	u32 func_id;
	union {
		u32 pcie_addr;
		u32 desc;
		u32 npu_init_done;
		u32 tran2cpu;
		u32 bawin_size;
		u32 bar_info;
		u32 driver_model;
		u32 dram_banode_addr;
		struct npu_info ni;
		struct last_rate lr;
		struct counter count;
		struct ratelimit_ctrl rl_ctrl;
		struct dbg_counter dbg_count;
		u32 wcid;
		u32 force2cpu_flag;
		u32 band0_oncpu_flag;
		u32 pkt_buf_addr;
		u32 test_noba_flag;
		u32 flushone_timeout;
		u32 flushall_timeout;
		u8 fast_flag;
		u32 pcie_port_type;
		u32 retry_times;
		u32 phy_addr;
		struct inode_txrx_addr txrx_addr;
		struct inode_cfg_info cfg_info;
		u32 dir;
		u32 dma_addr;
	} wifi_mail_private;
};

#endif /* _WIFI_MAIL_TYPE_H_ */
