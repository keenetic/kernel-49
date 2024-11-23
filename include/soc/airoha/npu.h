#ifndef _NPU_H_
#define _NPU_H_

struct npuMboxInfo;

void reserve_memblocks_npu(void);

u32  hwf_get_buf_addr(u8 id);
u32  hwf_get_buf_size(u8 id);

u32  npu_wifi_offload_get_pkt_buf_addr(void);
u32  npu_wifi_offload_get_ba_node_addr(void);
u32  npu_wifi_offload_get_tx_pkt_buf_addr(void);

u8   npu_wifi_offload_get_force_to_cpu_flag(void);
void npu_wifi_offload_set_force_to_cpu_flag(u8 isForceToCpu);

struct device *get_npu_dev(void);
int  get_npu_irq(int index);
u32  get_npu_hostadpt_reg(u32 hadap_ofs);
void set_npu_hostadpt_reg(u32 hadap_ofs, u32 val);
void set_npu_sram_power_save(u32 reg, u32 val);

int  npu_load_firmware(const u8 *buf, u32 buf_size, int buf_type);

void boot_npu_all_cores(bool uart_on);
void host_set_npu_core_on_off(u8 core, bool core_on);

int  host_notify_npuMbox(struct npuMboxInfo *mi);

#endif /* _NPU_H_ */
