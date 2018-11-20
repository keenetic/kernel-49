/*
    Module Name:
    hwnat_ioctl.h

    Abstract:

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
    Name        Date            Modification logs
    Steven Liu  2006-10-06      Initial version
*/

#ifndef __HW_NAT_IOCTL_H__
#define __HW_NAT_IOCTL_H__

#define HW_NAT_DUMP_CACHE_ENTRY		(0x02)
#define HW_NAT_DUMP_ENTRY		(0x03)
#define HW_NAT_GET_ALL_ENTRIES 		(0x04)
#define HW_NAT_BIND_ENTRY		(0x05)
#define HW_NAT_UNBIND_ENTRY		(0x06)
#define HW_NAT_INVALID_ENTRY		(0x07)
#define HW_NAT_DEBUG			(0x08)

#define HW_NAT_GET_AC_CNT		(0x09)
#define HW_NAT_MCAST_INS		(0x20)
#define HW_NAT_MCAST_DEL		(0x21)
#define HW_NAT_MCAST_DUMP		(0x22)
#define HW_NAT_DROP_ENTRY		(0x36)

#define HW_NAT_BIND_THRESHOLD		(0x16)
#define HW_NAT_MAX_ENTRY_LMT		(0x17)
#define HW_NAT_RULE_SIZE		(0x18)
#define HW_NAT_KA_INTERVAL		(0x19)
#define HW_NAT_UB_LIFETIME		(0x1A)
#define HW_NAT_BIND_LIFETIME		(0x1B)
#define HW_NAT_BIND_DIRECTION		(0x1C)
#define HW_NAT_VLAN_ID			(0x1D)

#define HW_NAT_DEVNAME			"hwnat0"
#define HW_NAT_MAJOR			(220)

enum hwnat_status {
	HWNAT_SUCCESS = 0,
	HWNAT_FAIL = 1,
	HWNAT_ENTRY_NOT_FOUND = 2
};

struct hwnat_tuple {
	unsigned short hash_index;
	unsigned char pkt_type;

	/* egress layer2 */
	unsigned char dmac[6];
	unsigned char smac[6];
	unsigned short vlan1;
	unsigned short vlan2;
	unsigned short pppoe_id;

	/* ingress layer3 */
	unsigned int ing_sipv4;
	unsigned int ing_dipv4;

	unsigned int ing_sipv6_0;
	unsigned int ing_sipv6_1;
	unsigned int ing_sipv6_2;
	unsigned int ing_sipv6_3;

	unsigned int ing_dipv6_0;
	unsigned int ing_dipv6_1;
	unsigned int ing_dipv6_2;
	unsigned int ing_dipv6_3;

	/* egress layer3 */
	unsigned int eg_sipv4;
	unsigned int eg_dipv4;

	unsigned int eg_sipv6_0;
	unsigned int eg_sipv6_1;
	unsigned int eg_sipv6_2;
	unsigned int eg_sipv6_3;

	unsigned int eg_dipv6_0;
	unsigned int eg_dipv6_1;
	unsigned int eg_dipv6_2;
	unsigned int eg_dipv6_3;

	unsigned char prot;

	// ingress layer4
	unsigned short ing_sp;
	unsigned short ing_dp;

	// egress layer4
	unsigned short eg_sp;
	unsigned short eg_dp;

	unsigned char	ipv6_flowlabel;
	enum hwnat_status result;
};

struct hwnat_args {
	unsigned int debug:3;
	unsigned int entry_state:2;	/* invalid=0, unbind=1, bind=2, fin=3 */
	enum hwnat_status result;
	unsigned int entry_num:16;
	unsigned int num_of_entries:16;
	struct hwnat_tuple entries[0];
};

/* hnat config */
struct hwnat_config_args {
	unsigned int bind_threshold:16;
	unsigned int foe_full_lmt:14;
	unsigned int foe_half_lmt:14;
	unsigned int foe_qut_lmt:14;
	unsigned int pre_acl:9;
	unsigned int pre_meter:9;
	unsigned int pre_ac:9;
	unsigned int post_meter:9;
	unsigned int post_ac:9;
	unsigned int foe_tcp_ka:8;	/*unit 4 sec */
	unsigned int foe_udp_ka:8;	/*unit 4 sec */
	unsigned int foe_unb_dlta:8;	/*unit 1 sec */
	unsigned int foe_tcp_dlta:16;	/*unit 1 sec */
	unsigned int foe_udp_dlta:16;	/*unit 1 sec */
	unsigned int foe_fin_dlta:16;	/*unit 1 sec */
	unsigned int wan_vid:16;
	unsigned int lan_vid:16;
	unsigned int bind_dir:2;	/* 0=upstream, 1=downstream, 2=bi-direction */
	enum hwnat_status result;
};

struct hwnat_ac_args {
	unsigned int ag_index;
	unsigned int ag_pkt_cnt;
	unsigned long long ag_byte_cnt;
	enum hwnat_status result;
};

struct hwnat_mcast_args {
	unsigned int mc_vid:16;
	unsigned int mc_px_en:4;
	unsigned int valid:1;
	unsigned int rev2:3;
	unsigned int mc_px_qos_en:4;
	unsigned int mc_qos_qid:4;
	unsigned char dst_mac[6];
};

#ifdef __KERNEL__
/*
 * EXPORT FUNCTION
 */
int PpeRegIoctlHandler(void);
void PpeUnRegIoctlHandler(void);

int PpeSetBindThreshold(uint32_t threshold);
int PpeSetMaxEntryLimit(uint32_t full, uint32_t half, uint32_t qurt);
int PpeSetKaInterval(uint8_t tcp_ka, uint8_t udp_ka);
int PpeSetUnbindLifeTime(uint8_t lifetime);
int PpeSetBindLifetime(uint16_t tcp_fin, uint16_t udp_life, uint16_t fin_life);
#endif /* __KERNEL__ */

#endif
