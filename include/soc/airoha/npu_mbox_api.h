#ifndef _NPU_MBOX_API_H_
#define _NPU_MBOX_API_H_

//#define MBOX_DBG_ON		(1)
//#define MBOX_API_TEST		(1)

#define MAX_MBOX_FUNC		(8)
#define MBOX2HOST_IDX		(8)

#define MASK_CORE_ID		(0xf)
#define MASK_RET_STATUS		(0x7)
#define MASK_FUNC_ID		(0xf)

union mboxArg {
	struct {
		u16 block_mode	 : 1;
		u16 done_bit	 : 1;
		u16 ret_status	 : 3;
		u16 static_buff	 : 1;
		u16 reserved	 : 5;
		u16 func_id	 : 4;
	} bits;
	u16 hWord;
};

enum npuCoreId {
	CORE0 = 0,
	CORE1,
	CORE2,
	CORE3,
	CORE4,
	CORE5,
	CORE6,
	CORE7
};

enum npuMboxFuncId {
	MFUNC_WIFI = 0,
	MFUNC_TUNNEL,
	MFUNC_NOTIFY,
	MFUNC_DBA,
	MFUNC_TR471,
	MFUNC_MAX_CASE
};

typedef int (*h_mbox_cb_t) (unsigned long va, u16 info);

struct npuMboxFlags {
	u32 isBlockingMode	 : 1;
	u32 isStaticBuffInit	 : 1;
	u32 isStaticBuffDel	 : 1;
	u32 resv		 : 29;
};

struct npuMboxInfo {
	enum npuCoreId core_id;
	enum npuMboxFuncId func_id;
	unsigned long va;
	u32 pa;
	u16 len;
	struct npuMboxFlags flags;
	u32 blockTimeout;	/* unit: 100us */
	h_mbox_cb_t cb;
};

#endif /* _NPU_MBOX_API_H_ */
