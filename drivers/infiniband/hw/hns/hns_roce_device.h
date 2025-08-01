/*
 * Copyright (c) 2016 Hisilicon Limited.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _HNS_ROCE_DEVICE_H
#define _HNS_ROCE_DEVICE_H

#include <rdma/ib_verbs.h>
#include <rdma/hns-abi.h>
#include "hns_roce_debugfs.h"

#define PCI_REVISION_ID_HIP08			0x21
#define PCI_REVISION_ID_HIP09			0x30

#define HNS_ROCE_MAX_MSG_LEN			0x80000000

#define HNS_ROCE_IB_MIN_SQ_STRIDE		6

#define BA_BYTE_LEN				8

#define HNS_ROCE_MIN_CQE_NUM			0x40
#define HNS_ROCE_MIN_SRQ_WQE_NUM		1

#define HNS_ROCE_MAX_IRQ_NUM			128

#define HNS_ROCE_SGE_IN_WQE			2
#define HNS_ROCE_SGE_SHIFT			4

#define EQ_ENABLE				1
#define EQ_DISABLE				0

#define HNS_ROCE_CEQ				0
#define HNS_ROCE_AEQ				1

#define HNS_ROCE_CEQE_SIZE 0x4
#define HNS_ROCE_AEQE_SIZE 0x10

#define HNS_ROCE_V3_EQE_SIZE 0x40

#define HNS_ROCE_V2_CQE_SIZE 32
#define HNS_ROCE_V3_CQE_SIZE 64

#define HNS_ROCE_V2_QPC_SZ 256
#define HNS_ROCE_V3_QPC_SZ 512

#define HNS_ROCE_MAX_PORTS			6
#define HNS_ROCE_GID_SIZE			16
#define HNS_ROCE_SGE_SIZE			16
#define HNS_ROCE_DWQE_SIZE			65536

#define HNS_ROCE_HOP_NUM_0			0xff

#define MR_TYPE_MR				0x00
#define MR_TYPE_FRMR				0x01
#define MR_TYPE_DMA				0x03

#define HNS_ROCE_FRMR_MAX_PA			512
#define HNS_ROCE_FRMR_ALIGN_SIZE		128

#define PKEY_ID					0xffff
#define NODE_DESC_SIZE				64
#define DB_REG_OFFSET				0x1000

/* Configure to HW for PAGE_SIZE larger than 4KB */
#define PG_SHIFT_OFFSET				(PAGE_SHIFT - 12)

#define ATOMIC_WR_LEN				8

#define HNS_ROCE_IDX_QUE_ENTRY_SZ		4
#define SRQ_DB_REG				0x230

#define HNS_ROCE_QP_BANK_NUM 8
#define HNS_ROCE_CQ_BANK_NUM 4

#define CQ_BANKID_SHIFT 2
#define CQ_BANKID_MASK GENMASK(1, 0)

#define HNS_ROCE_MAX_CQ_COUNT 0xFFFF
#define HNS_ROCE_MAX_CQ_PERIOD 0xFFFF

enum {
	SERV_TYPE_RC,
	SERV_TYPE_UC,
	SERV_TYPE_RD,
	SERV_TYPE_UD,
	SERV_TYPE_XRC = 5,
};

enum hns_roce_event {
	HNS_ROCE_EVENT_TYPE_PATH_MIG                  = 0x01,
	HNS_ROCE_EVENT_TYPE_PATH_MIG_FAILED           = 0x02,
	HNS_ROCE_EVENT_TYPE_COMM_EST                  = 0x03,
	HNS_ROCE_EVENT_TYPE_SQ_DRAINED                = 0x04,
	HNS_ROCE_EVENT_TYPE_WQ_CATAS_ERROR            = 0x05,
	HNS_ROCE_EVENT_TYPE_INV_REQ_LOCAL_WQ_ERROR    = 0x06,
	HNS_ROCE_EVENT_TYPE_LOCAL_WQ_ACCESS_ERROR     = 0x07,
	HNS_ROCE_EVENT_TYPE_SRQ_LIMIT_REACH           = 0x08,
	HNS_ROCE_EVENT_TYPE_SRQ_LAST_WQE_REACH        = 0x09,
	HNS_ROCE_EVENT_TYPE_SRQ_CATAS_ERROR           = 0x0a,
	HNS_ROCE_EVENT_TYPE_CQ_ACCESS_ERROR           = 0x0b,
	HNS_ROCE_EVENT_TYPE_CQ_OVERFLOW               = 0x0c,
	HNS_ROCE_EVENT_TYPE_CQ_ID_INVALID             = 0x0d,
	HNS_ROCE_EVENT_TYPE_PORT_CHANGE               = 0x0f,
	/* 0x10 and 0x11 is unused in currently application case */
	HNS_ROCE_EVENT_TYPE_DB_OVERFLOW               = 0x12,
	HNS_ROCE_EVENT_TYPE_MB                        = 0x13,
	HNS_ROCE_EVENT_TYPE_FLR			      = 0x15,
	HNS_ROCE_EVENT_TYPE_XRCD_VIOLATION	      = 0x16,
	HNS_ROCE_EVENT_TYPE_INVALID_XRCETH	      = 0x17,
};

enum {
	HNS_ROCE_CAP_FLAG_REREG_MR		= BIT(0),
	HNS_ROCE_CAP_FLAG_ROCE_V1_V2		= BIT(1),
	HNS_ROCE_CAP_FLAG_RQ_INLINE		= BIT(2),
	HNS_ROCE_CAP_FLAG_CQ_RECORD_DB		= BIT(3),
	HNS_ROCE_CAP_FLAG_QP_RECORD_DB		= BIT(4),
	HNS_ROCE_CAP_FLAG_SRQ			= BIT(5),
	HNS_ROCE_CAP_FLAG_XRC			= BIT(6),
	HNS_ROCE_CAP_FLAG_MW			= BIT(7),
	HNS_ROCE_CAP_FLAG_FRMR                  = BIT(8),
	HNS_ROCE_CAP_FLAG_QP_FLOW_CTRL		= BIT(9),
	HNS_ROCE_CAP_FLAG_ATOMIC		= BIT(10),
	HNS_ROCE_CAP_FLAG_DIRECT_WQE		= BIT(12),
	HNS_ROCE_CAP_FLAG_SDI_MODE		= BIT(14),
	HNS_ROCE_CAP_FLAG_STASH			= BIT(17),
	HNS_ROCE_CAP_FLAG_CQE_INLINE		= BIT(19),
	HNS_ROCE_CAP_FLAG_SRQ_RECORD_DB         = BIT(22),
};

#define HNS_ROCE_DB_TYPE_COUNT			2
#define HNS_ROCE_DB_UNIT_SIZE			4

enum {
	HNS_ROCE_DB_PER_PAGE = PAGE_SIZE / 4
};

enum hns_roce_reset_stage {
	HNS_ROCE_STATE_NON_RST,
	HNS_ROCE_STATE_RST_BEF_DOWN,
	HNS_ROCE_STATE_RST_DOWN,
	HNS_ROCE_STATE_RST_UNINIT,
	HNS_ROCE_STATE_RST_INIT,
	HNS_ROCE_STATE_RST_INITED,
};

enum hns_roce_instance_state {
	HNS_ROCE_STATE_NON_INIT,
	HNS_ROCE_STATE_INIT,
	HNS_ROCE_STATE_INITED,
	HNS_ROCE_STATE_UNINIT,
};

enum {
	HNS_ROCE_RST_DIRECT_RETURN		= 0,
};

#define HNS_ROCE_CMD_SUCCESS			1

#define HNS_ROCE_MAX_HOP_NUM			3
/* The minimum page size is 4K for hardware */
#define HNS_HW_PAGE_SHIFT			12
#define HNS_HW_PAGE_SIZE			(1 << HNS_HW_PAGE_SHIFT)

#define HNS_HW_MAX_PAGE_SHIFT			27
#define HNS_HW_MAX_PAGE_SIZE			(1 << HNS_HW_MAX_PAGE_SHIFT)

struct hns_roce_uar {
	u64		pfn;
	unsigned long	index;
	unsigned long	logic_idx;
};

enum hns_roce_mmap_type {
	HNS_ROCE_MMAP_TYPE_DB = 1,
	HNS_ROCE_MMAP_TYPE_DWQE,
};

struct hns_user_mmap_entry {
	struct rdma_user_mmap_entry rdma_entry;
	enum hns_roce_mmap_type mmap_type;
	u64 address;
};

struct hns_roce_ucontext {
	struct ib_ucontext	ibucontext;
	struct hns_roce_uar	uar;
	struct list_head	page_list;
	struct mutex		page_mutex;
	struct hns_user_mmap_entry *db_mmap_entry;
	u32			config;
};

struct hns_roce_pd {
	struct ib_pd		ibpd;
	unsigned long		pdn;
};

struct hns_roce_xrcd {
	struct ib_xrcd ibxrcd;
	u32 xrcdn;
};

struct hns_roce_bitmap {
	/* Bitmap Traversal last a bit which is 1 */
	unsigned long		last;
	unsigned long		top;
	unsigned long		max;
	unsigned long		reserved_top;
	unsigned long		mask;
	spinlock_t		lock;
	unsigned long		*table;
};

struct hns_roce_ida {
	struct ida ida;
	u32 min; /* Lowest ID to allocate.  */
	u32 max; /* Highest ID to allocate. */
};

/* For Hardware Entry Memory */
struct hns_roce_hem_table {
	/* HEM type: 0 = qpc, 1 = mtt, 2 = cqc, 3 = srq, 4 = other */
	u32		type;
	/* HEM array elment num */
	unsigned long	num_hem;
	/* Single obj size */
	unsigned long	obj_size;
	unsigned long	table_chunk_size;
	struct mutex	mutex;
	struct hns_roce_hem **hem;
	u64		**bt_l1;
	dma_addr_t	*bt_l1_dma_addr;
	u64		**bt_l0;
	dma_addr_t	*bt_l0_dma_addr;
};

struct hns_roce_buf_region {
	u32 offset; /* page offset */
	u32 count; /* page count */
	int hopnum; /* addressing hop num */
};

#define HNS_ROCE_MAX_BT_REGION	3
#define HNS_ROCE_MAX_BT_LEVEL	3
struct hns_roce_hem_list {
	struct list_head root_bt;
	/* link all bt dma mem by hop config */
	struct list_head mid_bt[HNS_ROCE_MAX_BT_REGION][HNS_ROCE_MAX_BT_LEVEL];
	struct list_head btm_bt; /* link all bottom bt in @mid_bt */
	dma_addr_t root_ba; /* pointer to the root ba table */
};

enum mtr_type {
	MTR_DEFAULT = 0,
	MTR_PBL,
};

struct hns_roce_buf_attr {
	struct {
		size_t	size;  /* region size */
		int	hopnum; /* multi-hop addressing hop num */
	} region[HNS_ROCE_MAX_BT_REGION];
	unsigned int region_count; /* valid region count */
	unsigned int page_shift;  /* buffer page shift */
	unsigned int user_access; /* umem access flag */
	u64 iova;
	enum mtr_type type;
	bool mtt_only; /* only alloc buffer-required MTT memory */
	bool adaptive; /* adaptive for page_shift and hopnum */
};

struct hns_roce_hem_cfg {
	dma_addr_t	root_ba; /* root BA table's address */
	bool		is_direct; /* addressing without BA table */
	unsigned int	ba_pg_shift; /* BA table page shift */
	unsigned int	buf_pg_shift; /* buffer page shift */
	unsigned int	buf_pg_count;  /* buffer page count */
	struct hns_roce_buf_region region[HNS_ROCE_MAX_BT_REGION];
	unsigned int	region_count;
};

/* memory translate region */
struct hns_roce_mtr {
	struct hns_roce_hem_list hem_list; /* multi-hop addressing resource */
	struct ib_umem		*umem; /* user space buffer */
	struct hns_roce_buf	*kmem; /* kernel space buffer */
	struct hns_roce_hem_cfg  hem_cfg; /* config for hardware addressing */
};

struct hns_roce_mr {
	struct ib_mr		ibmr;
	u64			iova; /* MR's virtual original addr */
	u64			size; /* Address range of MR */
	u32			key; /* Key of MR */
	u32			pd;   /* PD num of MR */
	u32			access; /* Access permission of MR */
	int			enabled; /* MR's active status */
	int			type; /* MR's register type */
	u32			pbl_hop_num; /* multi-hop number */
	struct hns_roce_mtr	pbl_mtr;
	u32			npages;
	dma_addr_t		*page_list;
};

struct hns_roce_mr_table {
	struct hns_roce_ida mtpt_ida;
	struct hns_roce_hem_table	mtpt_table;
};

struct hns_roce_wq {
	u64		*wrid;     /* Work request ID */
	spinlock_t	lock;
	u32		wqe_cnt;  /* WQE num */
	u32		max_gs;
	u32		rsv_sge;
	u32		offset;
	u32		wqe_shift; /* WQE size */
	u32		head;
	u32		tail;
	void __iomem	*db_reg;
	u32		ext_sge_cnt;
};

struct hns_roce_sge {
	unsigned int	sge_cnt; /* SGE num */
	u32		offset;
	u32		sge_shift; /* SGE size */
};

struct hns_roce_buf_list {
	void		*buf;
	dma_addr_t	map;
};

/*
 * %HNS_ROCE_BUF_DIRECT indicates that the all memory must be in a continuous
 * dma address range.
 *
 * %HNS_ROCE_BUF_NOSLEEP indicates that the caller cannot sleep.
 *
 * %HNS_ROCE_BUF_NOFAIL allocation only failed when allocated size is zero, even
 * the allocated size is smaller than the required size.
 */
enum {
	HNS_ROCE_BUF_DIRECT = BIT(0),
	HNS_ROCE_BUF_NOSLEEP = BIT(1),
	HNS_ROCE_BUF_NOFAIL = BIT(2),
};

struct hns_roce_buf {
	struct hns_roce_buf_list	*trunk_list;
	u32				ntrunks;
	u32				npages;
	unsigned int			trunk_shift;
	unsigned int			page_shift;
};

struct hns_roce_db_pgdir {
	struct list_head	list;
	DECLARE_BITMAP(order0, HNS_ROCE_DB_PER_PAGE);
	DECLARE_BITMAP(order1, HNS_ROCE_DB_PER_PAGE / HNS_ROCE_DB_TYPE_COUNT);
	unsigned long		*bits[HNS_ROCE_DB_TYPE_COUNT];
	u32			*page;
	dma_addr_t		db_dma;
};

struct hns_roce_user_db_page {
	struct list_head	list;
	struct ib_umem		*umem;
	unsigned long		user_virt;
	refcount_t		refcount;
};

struct hns_roce_db {
	u32		*db_record;
	union {
		struct hns_roce_db_pgdir *pgdir;
		struct hns_roce_user_db_page *user_page;
	} u;
	dma_addr_t	dma;
	void		*virt_addr;
	unsigned long	index;
	unsigned long	order;
};

struct hns_roce_cq {
	struct ib_cq			ib_cq;
	struct hns_roce_mtr		mtr;
	struct hns_roce_db		db;
	u32				flags;
	spinlock_t			lock;
	u32				cq_depth;
	u32				cons_index;
	u32				*set_ci_db;
	void __iomem			*db_reg;
	int				arm_sn;
	int				cqe_size;
	unsigned long			cqn;
	u32				vector;
	refcount_t			refcount;
	struct completion		free;
	struct list_head		sq_list; /* all qps on this send cq */
	struct list_head		rq_list; /* all qps on this recv cq */
	int				is_armed; /* cq is armed */
	struct list_head		node; /* all armed cqs are on a list */
};

struct hns_roce_idx_que {
	struct hns_roce_mtr		mtr;
	u32				entry_shift;
	unsigned long			*bitmap;
	u32				head;
	u32				tail;
};

struct hns_roce_srq {
	struct ib_srq		ibsrq;
	unsigned long		srqn;
	u32			wqe_cnt;
	int			max_gs;
	u32			rsv_sge;
	u32			wqe_shift;
	u32			cqn;
	u32			xrcdn;
	void __iomem		*db_reg;

	refcount_t		refcount;
	struct completion	free;

	struct hns_roce_mtr	buf_mtr;

	u64		       *wrid;
	struct hns_roce_idx_que idx_que;
	spinlock_t		lock;
	struct mutex		mutex;
	void (*event)(struct hns_roce_srq *srq, enum hns_roce_event event);
	struct hns_roce_db	rdb;
	u32			cap_flags;
};

struct hns_roce_uar_table {
	struct hns_roce_bitmap bitmap;
};

struct hns_roce_bank {
	struct ida ida;
	u32 inuse; /* Number of IDs allocated */
	u32 min; /* Lowest ID to allocate.  */
	u32 max; /* Highest ID to allocate. */
	u32 next; /* Next ID to allocate. */
};

struct hns_roce_qp_table {
	struct hns_roce_hem_table	qp_table;
	struct hns_roce_hem_table	irrl_table;
	struct hns_roce_hem_table	trrl_table;
	struct hns_roce_hem_table	sccc_table;
	struct mutex			scc_mutex;
	struct hns_roce_bank bank[HNS_ROCE_QP_BANK_NUM];
	struct mutex bank_mutex;
	struct xarray			dip_xa;
};

struct hns_roce_cq_table {
	struct xarray			array;
	struct hns_roce_hem_table	table;
	struct hns_roce_bank bank[HNS_ROCE_CQ_BANK_NUM];
	struct mutex			bank_mutex;
};

struct hns_roce_srq_table {
	struct hns_roce_ida		srq_ida;
	struct xarray			xa;
	struct hns_roce_hem_table	table;
};

struct hns_roce_av {
	u8 port;
	u8 gid_index;
	u8 stat_rate;
	u8 hop_limit;
	u32 flowlabel;
	u16 udp_sport;
	u8 sl;
	u8 tclass;
	u8 dgid[HNS_ROCE_GID_SIZE];
	u8 mac[ETH_ALEN];
	u16 vlan_id;
	u8 vlan_en;
};

struct hns_roce_ah {
	struct ib_ah		ibah;
	struct hns_roce_av	av;
};

struct hns_roce_cmd_context {
	struct completion	done;
	int			result;
	int			next;
	u64			out_param;
	u16			token;
	u16			busy;
};

enum hns_roce_cmdq_state {
	HNS_ROCE_CMDQ_STATE_NORMAL,
	HNS_ROCE_CMDQ_STATE_FATAL_ERR,
};

struct hns_roce_cmdq {
	struct dma_pool		*pool;
	struct semaphore	poll_sem;
	/*
	 * Event mode: cmd register mutex protection,
	 * ensure to not exceed max_cmds and user use limit region
	 */
	struct semaphore	event_sem;
	int			max_cmds;
	spinlock_t		context_lock;
	int			free_head;
	struct hns_roce_cmd_context *context;
	/*
	 * Process whether use event mode, init default non-zero
	 * After the event queue of cmd event ready,
	 * can switch into event mode
	 * close device, switch into poll mode(non event mode)
	 */
	u8			use_events;
	enum hns_roce_cmdq_state state;
};

struct hns_roce_cmd_mailbox {
	void		       *buf;
	dma_addr_t		dma;
};

struct hns_roce_mbox_msg {
	u64 in_param;
	u64 out_param;
	u8 cmd;
	u32 tag;
	u16 token;
	u8 event_en;
};

struct hns_roce_dev;

enum {
	HNS_ROCE_FLUSH_FLAG = 0,
	HNS_ROCE_STOP_FLUSH_FLAG = 1,
};

struct hns_roce_work {
	struct hns_roce_dev *hr_dev;
	struct work_struct work;
	int event_type;
	int sub_type;
	u32 queue_num;
};

enum hns_roce_cong_type {
	CONG_TYPE_DCQCN,
	CONG_TYPE_LDCP,
	CONG_TYPE_HC3,
	CONG_TYPE_DIP,
};

struct hns_roce_qp {
	struct ib_qp		ibqp;
	struct hns_roce_wq	rq;
	struct hns_roce_db	rdb;
	struct hns_roce_db	sdb;
	unsigned long		en_flags;
	enum ib_sig_type	sq_signal_bits;
	struct hns_roce_wq	sq;

	struct hns_roce_mtr	mtr;

	u32			buff_size;
	struct mutex		mutex;
	u8			port;
	u8			phy_port;
	u8			sl;
	u8			resp_depth;
	u8			state;
	u32                     atomic_rd_en;
	u32			qkey;
	void			(*event)(struct hns_roce_qp *qp,
					 enum hns_roce_event event_type);
	unsigned long		qpn;

	u32			xrcdn;

	refcount_t		refcount;
	struct completion	free;

	struct hns_roce_sge	sge;
	u32			next_sge;
	enum ib_mtu		path_mtu;
	u32			max_inline_data;
	u8			free_mr_en;

	/* 0: flush needed, 1: unneeded */
	unsigned long		flush_flag;
	struct hns_roce_work	flush_work;
	struct list_head	node; /* all qps are on a list */
	struct list_head	rq_node; /* all recv qps are on a list */
	struct list_head	sq_node; /* all send qps are on a list */
	struct hns_user_mmap_entry *dwqe_mmap_entry;
	u32			config;
	enum hns_roce_cong_type	cong_type;
	u8			tc_mode;
	u8			priority;
	spinlock_t flush_lock;
	struct hns_roce_dip *dip;
};

struct hns_roce_ib_iboe {
	spinlock_t		lock;
	struct net_device      *netdevs[HNS_ROCE_MAX_PORTS];
	struct notifier_block	nb;
	u8			phy_port[HNS_ROCE_MAX_PORTS];
};

struct hns_roce_ceqe {
	__le32	comp;
	__le32	rsv[15];
};

#define CEQE_FIELD_LOC(h, l) FIELD_LOC(struct hns_roce_ceqe, h, l)

#define CEQE_CQN CEQE_FIELD_LOC(23, 0)
#define CEQE_OWNER CEQE_FIELD_LOC(31, 31)

struct hns_roce_aeqe {
	__le32 asyn;
	union {
		struct {
			__le32 num;
			u32 rsv0;
			u32 rsv1;
		} queue_event;

		struct {
			__le64  out_param;
			__le16  token;
			u8	status;
			u8	rsv0;
		} __packed cmd;
	 } event;
	__le32 rsv[12];
};

#define AEQE_FIELD_LOC(h, l) FIELD_LOC(struct hns_roce_aeqe, h, l)

#define AEQE_EVENT_TYPE AEQE_FIELD_LOC(7, 0)
#define AEQE_SUB_TYPE AEQE_FIELD_LOC(15, 8)
#define AEQE_OWNER AEQE_FIELD_LOC(31, 31)
#define AEQE_EVENT_QUEUE_NUM AEQE_FIELD_LOC(55, 32)

struct hns_roce_eq {
	struct hns_roce_dev		*hr_dev;
	void __iomem			*db_reg;

	int				type_flag; /* Aeq:1 ceq:0 */
	int				eqn;
	u32				entries;
	int				eqe_size;
	int				irq;
	u32				cons_index;
	int				over_ignore;
	int				coalesce;
	int				arm_st;
	int				hop_num;
	struct hns_roce_mtr		mtr;
	u16				eq_max_cnt;
	u32				eq_period;
	int				shift;
	int				event_type;
	int				sub_type;
	struct work_struct		work;
};

struct hns_roce_eq_table {
	struct hns_roce_eq	*eq;
};

struct hns_roce_caps {
	u64		fw_ver;
	u8		num_ports;
	int		gid_table_len[HNS_ROCE_MAX_PORTS];
	int		pkey_table_len[HNS_ROCE_MAX_PORTS];
	int		local_ca_ack_delay;
	int		num_uars;
	u32		phy_num_uars;
	u32		max_sq_sg;
	u32		max_sq_inline;
	u32		max_rq_sg;
	u32		rsv0;
	u32		num_qps;
	u32		reserved_qps;
	u32		num_srqs;
	u32		max_wqes;
	u32		max_srq_wrs;
	u32		max_srq_sges;
	u32		max_sq_desc_sz;
	u32		max_rq_desc_sz;
	u32		rsv2;
	int		max_qp_init_rdma;
	int		max_qp_dest_rdma;
	u32		num_cqs;
	u32		max_cqes;
	u32		min_cqes;
	u32		min_wqes;
	u32		reserved_cqs;
	u32		reserved_srqs;
	int		num_aeq_vectors;
	int		num_comp_vectors;
	int		num_other_vectors;
	u32		num_mtpts;
	u32		rsv1;
	u32		num_srqwqe_segs;
	u32		num_idx_segs;
	int		reserved_mrws;
	int		reserved_uars;
	int		num_pds;
	int		reserved_pds;
	u32		num_xrcds;
	u32		reserved_xrcds;
	u32		mtt_entry_sz;
	u32		cqe_sz;
	u32		page_size_cap;
	u32		reserved_lkey;
	int		mtpt_entry_sz;
	int		qpc_sz;
	int		irrl_entry_sz;
	int		trrl_entry_sz;
	int		cqc_entry_sz;
	int		sccc_sz;
	int		qpc_timer_entry_sz;
	int		cqc_timer_entry_sz;
	int		srqc_entry_sz;
	int		idx_entry_sz;
	u32		pbl_ba_pg_sz;
	u32		pbl_buf_pg_sz;
	u32		pbl_hop_num;
	int		aeqe_depth;
	int		ceqe_depth;
	u32		aeqe_size;
	u32		ceqe_size;
	enum ib_mtu	max_mtu;
	u32		qpc_bt_num;
	u32		qpc_timer_bt_num;
	u32		srqc_bt_num;
	u32		cqc_bt_num;
	u32		cqc_timer_bt_num;
	u32		mpt_bt_num;
	u32		eqc_bt_num;
	u32		smac_bt_num;
	u32		sgid_bt_num;
	u32		sccc_bt_num;
	u32		gmv_bt_num;
	u32		qpc_ba_pg_sz;
	u32		qpc_buf_pg_sz;
	u32		qpc_hop_num;
	u32		srqc_ba_pg_sz;
	u32		srqc_buf_pg_sz;
	u32		srqc_hop_num;
	u32		cqc_ba_pg_sz;
	u32		cqc_buf_pg_sz;
	u32		cqc_hop_num;
	u32		mpt_ba_pg_sz;
	u32		mpt_buf_pg_sz;
	u32		mpt_hop_num;
	u32		mtt_ba_pg_sz;
	u32		mtt_buf_pg_sz;
	u32		mtt_hop_num;
	u32		wqe_sq_hop_num;
	u32		wqe_sge_hop_num;
	u32		wqe_rq_hop_num;
	u32		sccc_ba_pg_sz;
	u32		sccc_buf_pg_sz;
	u32		sccc_hop_num;
	u32		qpc_timer_ba_pg_sz;
	u32		qpc_timer_buf_pg_sz;
	u32		qpc_timer_hop_num;
	u32		cqc_timer_ba_pg_sz;
	u32		cqc_timer_buf_pg_sz;
	u32		cqc_timer_hop_num;
	u32		cqe_ba_pg_sz; /* page_size = 4K*(2^cqe_ba_pg_sz) */
	u32		cqe_buf_pg_sz;
	u32		cqe_hop_num;
	u32		srqwqe_ba_pg_sz;
	u32		srqwqe_buf_pg_sz;
	u32		srqwqe_hop_num;
	u32		idx_ba_pg_sz;
	u32		idx_buf_pg_sz;
	u32		idx_hop_num;
	u32		eqe_ba_pg_sz;
	u32		eqe_buf_pg_sz;
	u32		eqe_hop_num;
	u32		gmv_entry_num;
	u32		gmv_entry_sz;
	u32		gmv_ba_pg_sz;
	u32		gmv_buf_pg_sz;
	u32		gmv_hop_num;
	u32		sl_num;
	u32		llm_buf_pg_sz;
	u32		chunk_sz; /* chunk size in non multihop mode */
	u64		flags;
	u16		default_ceq_max_cnt;
	u16		default_ceq_period;
	u16		default_aeq_max_cnt;
	u16		default_aeq_period;
	u16		default_aeq_arm_st;
	u16		default_ceq_arm_st;
	u8		cong_cap;
	enum hns_roce_cong_type default_cong_type;
	u32             max_ack_req_msg_len;
};

enum hns_roce_device_state {
	HNS_ROCE_DEVICE_STATE_INITED,
	HNS_ROCE_DEVICE_STATE_RST_DOWN,
	HNS_ROCE_DEVICE_STATE_UNINIT,
};

enum hns_roce_hw_pkt_stat_index {
	HNS_ROCE_HW_RX_RC_PKT_CNT,
	HNS_ROCE_HW_RX_UC_PKT_CNT,
	HNS_ROCE_HW_RX_UD_PKT_CNT,
	HNS_ROCE_HW_RX_XRC_PKT_CNT,
	HNS_ROCE_HW_RX_PKT_CNT,
	HNS_ROCE_HW_RX_ERR_PKT_CNT,
	HNS_ROCE_HW_RX_CNP_PKT_CNT,
	HNS_ROCE_HW_TX_RC_PKT_CNT,
	HNS_ROCE_HW_TX_UC_PKT_CNT,
	HNS_ROCE_HW_TX_UD_PKT_CNT,
	HNS_ROCE_HW_TX_XRC_PKT_CNT,
	HNS_ROCE_HW_TX_PKT_CNT,
	HNS_ROCE_HW_TX_ERR_PKT_CNT,
	HNS_ROCE_HW_TX_CNP_PKT_CNT,
	HNS_ROCE_HW_TRP_GET_MPT_ERR_PKT_CNT,
	HNS_ROCE_HW_TRP_GET_IRRL_ERR_PKT_CNT,
	HNS_ROCE_HW_ECN_DB_CNT,
	HNS_ROCE_HW_RX_BUF_CNT,
	HNS_ROCE_HW_TRP_RX_SOF_CNT,
	HNS_ROCE_HW_CQ_CQE_CNT,
	HNS_ROCE_HW_CQ_POE_CNT,
	HNS_ROCE_HW_CQ_NOTIFY_CNT,
	HNS_ROCE_HW_CNT_TOTAL
};

enum hns_roce_sw_dfx_stat_index {
	HNS_ROCE_DFX_AEQE_CNT,
	HNS_ROCE_DFX_CEQE_CNT,
	HNS_ROCE_DFX_CMDS_CNT,
	HNS_ROCE_DFX_CMDS_ERR_CNT,
	HNS_ROCE_DFX_MBX_POSTED_CNT,
	HNS_ROCE_DFX_MBX_POLLED_CNT,
	HNS_ROCE_DFX_MBX_EVENT_CNT,
	HNS_ROCE_DFX_QP_CREATE_ERR_CNT,
	HNS_ROCE_DFX_QP_MODIFY_ERR_CNT,
	HNS_ROCE_DFX_CQ_CREATE_ERR_CNT,
	HNS_ROCE_DFX_CQ_MODIFY_ERR_CNT,
	HNS_ROCE_DFX_SRQ_CREATE_ERR_CNT,
	HNS_ROCE_DFX_SRQ_MODIFY_ERR_CNT,
	HNS_ROCE_DFX_XRCD_ALLOC_ERR_CNT,
	HNS_ROCE_DFX_MR_REG_ERR_CNT,
	HNS_ROCE_DFX_MR_REREG_ERR_CNT,
	HNS_ROCE_DFX_AH_CREATE_ERR_CNT,
	HNS_ROCE_DFX_MMAP_ERR_CNT,
	HNS_ROCE_DFX_UCTX_ALLOC_ERR_CNT,
	HNS_ROCE_DFX_CNT_TOTAL
};

struct hns_roce_hw {
	int (*cmq_init)(struct hns_roce_dev *hr_dev);
	void (*cmq_exit)(struct hns_roce_dev *hr_dev);
	int (*hw_profile)(struct hns_roce_dev *hr_dev);
	int (*hw_init)(struct hns_roce_dev *hr_dev);
	void (*hw_exit)(struct hns_roce_dev *hr_dev);
	int (*post_mbox)(struct hns_roce_dev *hr_dev,
			 struct hns_roce_mbox_msg *mbox_msg);
	int (*poll_mbox_done)(struct hns_roce_dev *hr_dev);
	bool (*chk_mbox_avail)(struct hns_roce_dev *hr_dev, bool *is_busy);
	int (*set_gid)(struct hns_roce_dev *hr_dev, int gid_index,
		       const union ib_gid *gid, const struct ib_gid_attr *attr);
	int (*set_mac)(struct hns_roce_dev *hr_dev, u8 phy_port,
		       const u8 *addr);
	int (*write_mtpt)(struct hns_roce_dev *hr_dev, void *mb_buf,
			  struct hns_roce_mr *mr);
	int (*rereg_write_mtpt)(struct hns_roce_dev *hr_dev,
				struct hns_roce_mr *mr, int flags,
				void *mb_buf);
	int (*frmr_write_mtpt)(void *mb_buf, struct hns_roce_mr *mr);
	void (*write_cqc)(struct hns_roce_dev *hr_dev,
			  struct hns_roce_cq *hr_cq, void *mb_buf, u64 *mtts,
			  dma_addr_t dma_handle);
	int (*set_hem)(struct hns_roce_dev *hr_dev,
		       struct hns_roce_hem_table *table, int obj, u32 step_idx);
	int (*clear_hem)(struct hns_roce_dev *hr_dev,
			 struct hns_roce_hem_table *table, int obj,
			 u32 step_idx);
	int (*modify_qp)(struct ib_qp *ibqp, const struct ib_qp_attr *attr,
			 int attr_mask, enum ib_qp_state cur_state,
			 enum ib_qp_state new_state, struct ib_udata *udata);
	int (*qp_flow_control_init)(struct hns_roce_dev *hr_dev,
			 struct hns_roce_qp *hr_qp);
	void (*dereg_mr)(struct hns_roce_dev *hr_dev);
	int (*init_eq)(struct hns_roce_dev *hr_dev);
	void (*cleanup_eq)(struct hns_roce_dev *hr_dev);
	int (*write_srqc)(struct hns_roce_srq *srq, void *mb_buf);
	int (*query_cqc)(struct hns_roce_dev *hr_dev, u32 cqn, void *buffer);
	int (*query_qpc)(struct hns_roce_dev *hr_dev, u32 qpn, void *buffer);
	int (*query_mpt)(struct hns_roce_dev *hr_dev, u32 key, void *buffer);
	int (*query_srqc)(struct hns_roce_dev *hr_dev, u32 srqn, void *buffer);
	int (*query_sccc)(struct hns_roce_dev *hr_dev, u32 qpn, void *buffer);
	int (*query_hw_counter)(struct hns_roce_dev *hr_dev,
				u64 *stats, u32 port, int *hw_counters);
	int (*get_dscp)(struct hns_roce_dev *hr_dev, u8 dscp,
			u8 *tc_mode, u8 *priority);
	const struct ib_device_ops *hns_roce_dev_ops;
	const struct ib_device_ops *hns_roce_dev_srq_ops;
};

struct hns_roce_dev {
	struct ib_device	ib_dev;
	struct pci_dev		*pci_dev;
	struct device		*dev;
	struct hns_roce_uar     priv_uar;
	const char		*irq_names[HNS_ROCE_MAX_IRQ_NUM];
	spinlock_t		sm_lock;
	bool			active;
	bool			is_reset;
	bool			dis_db;
	unsigned long		reset_cnt;
	struct hns_roce_ib_iboe iboe;
	enum hns_roce_device_state state;
	struct list_head	qp_list; /* list of all qps on this dev */
	spinlock_t		qp_list_lock; /* protect qp_list */

	struct list_head        pgdir_list;
	struct mutex            pgdir_mutex;
	int			irq[HNS_ROCE_MAX_IRQ_NUM];
	u8 __iomem		*reg_base;
	void __iomem		*mem_base;
	struct hns_roce_caps	caps;
	struct xarray		qp_table_xa;

	unsigned char	dev_addr[HNS_ROCE_MAX_PORTS][ETH_ALEN];
	u64			sys_image_guid;
	u32                     vendor_id;
	u32                     vendor_part_id;
	u32                     hw_rev;
	void __iomem            *priv_addr;

	struct hns_roce_cmdq	cmd;
	struct hns_roce_ida pd_ida;
	struct hns_roce_ida xrcd_ida;
	struct hns_roce_ida uar_ida;
	struct hns_roce_mr_table  mr_table;
	struct hns_roce_cq_table  cq_table;
	struct hns_roce_srq_table srq_table;
	struct hns_roce_qp_table  qp_table;
	struct hns_roce_eq_table  eq_table;
	struct hns_roce_hem_table  qpc_timer_table;
	struct hns_roce_hem_table  cqc_timer_table;
	/* GMV is the memory area that the driver allocates for the hardware
	 * to store SGID, SMAC and VLAN information.
	 */
	struct hns_roce_hem_table  gmv_table;

	int			cmd_mod;
	int			loop_idc;
	u32			sdb_offset;
	u32			odb_offset;
	const struct hns_roce_hw *hw;
	void			*priv;
	struct workqueue_struct *irq_workq;
	struct work_struct ecc_work;
	u32 func_num;
	u32 is_vf;
	u32 cong_algo_tmpl_id;
	u64 dwqe_page;
	struct hns_roce_dev_debugfs dbgfs;
	atomic64_t *dfx_cnt;
};

enum hns_roce_trace_type {
	TRACE_SQ,
	TRACE_RQ,
	TRACE_SRQ,
};

static inline const char *trace_type_to_str(enum hns_roce_trace_type type)
{
	switch (type) {
	case TRACE_SQ:
		return "SQ";
	case TRACE_RQ:
		return "RQ";
	case TRACE_SRQ:
		return "SRQ";
	default:
		return "UNKNOWN";
	}
}

static inline struct hns_roce_dev *to_hr_dev(struct ib_device *ib_dev)
{
	return container_of(ib_dev, struct hns_roce_dev, ib_dev);
}

static inline struct hns_roce_ucontext
			*to_hr_ucontext(struct ib_ucontext *ibucontext)
{
	return container_of(ibucontext, struct hns_roce_ucontext, ibucontext);
}

static inline struct hns_roce_pd *to_hr_pd(struct ib_pd *ibpd)
{
	return container_of(ibpd, struct hns_roce_pd, ibpd);
}

static inline struct hns_roce_xrcd *to_hr_xrcd(struct ib_xrcd *ibxrcd)
{
	return container_of(ibxrcd, struct hns_roce_xrcd, ibxrcd);
}

static inline struct hns_roce_ah *to_hr_ah(struct ib_ah *ibah)
{
	return container_of(ibah, struct hns_roce_ah, ibah);
}

static inline struct hns_roce_mr *to_hr_mr(struct ib_mr *ibmr)
{
	return container_of(ibmr, struct hns_roce_mr, ibmr);
}

static inline struct hns_roce_qp *to_hr_qp(struct ib_qp *ibqp)
{
	return container_of(ibqp, struct hns_roce_qp, ibqp);
}

static inline struct hns_roce_cq *to_hr_cq(struct ib_cq *ib_cq)
{
	return container_of(ib_cq, struct hns_roce_cq, ib_cq);
}

static inline struct hns_roce_srq *to_hr_srq(struct ib_srq *ibsrq)
{
	return container_of(ibsrq, struct hns_roce_srq, ibsrq);
}

static inline struct hns_user_mmap_entry *
to_hns_mmap(struct rdma_user_mmap_entry *rdma_entry)
{
	return container_of(rdma_entry, struct hns_user_mmap_entry, rdma_entry);
}

static inline void hns_roce_write64_k(__le32 val[2], void __iomem *dest)
{
	writeq(*(u64 *)val, dest);
}

static inline struct hns_roce_qp
	*__hns_roce_qp_lookup(struct hns_roce_dev *hr_dev, u32 qpn)
{
	return xa_load(&hr_dev->qp_table_xa, qpn);
}

static inline void *hns_roce_buf_offset(struct hns_roce_buf *buf,
					unsigned int offset)
{
	return (char *)(buf->trunk_list[offset >> buf->trunk_shift].buf) +
			(offset & ((1 << buf->trunk_shift) - 1));
}

static inline dma_addr_t hns_roce_buf_dma_addr(struct hns_roce_buf *buf,
					       unsigned int offset)
{
	return buf->trunk_list[offset >> buf->trunk_shift].map +
			(offset & ((1 << buf->trunk_shift) - 1));
}

static inline dma_addr_t hns_roce_buf_page(struct hns_roce_buf *buf, u32 idx)
{
	return hns_roce_buf_dma_addr(buf, idx << buf->page_shift);
}

#define hr_hw_page_align(x)		ALIGN(x, 1 << HNS_HW_PAGE_SHIFT)

static inline u64 to_hr_hw_page_addr(u64 addr)
{
	return addr >> HNS_HW_PAGE_SHIFT;
}

static inline u32 to_hr_hw_page_shift(u32 page_shift)
{
	return page_shift - HNS_HW_PAGE_SHIFT;
}

static inline u32 to_hr_hem_hopnum(u32 hopnum, u32 count)
{
	if (count > 0)
		return hopnum == HNS_ROCE_HOP_NUM_0 ? 0 : hopnum;

	return 0;
}

static inline u32 to_hr_hem_entries_size(u32 count, u32 buf_shift)
{
	return hr_hw_page_align(count << buf_shift);
}

static inline u32 to_hr_hem_entries_count(u32 count, u32 buf_shift)
{
	return hr_hw_page_align(count << buf_shift) >> buf_shift;
}

static inline u32 to_hr_hem_entries_shift(u32 count, u32 buf_shift)
{
	if (!count)
		return 0;

	return ilog2(to_hr_hem_entries_count(count, buf_shift));
}

#define DSCP_SHIFT 2

static inline u8 get_tclass(const struct ib_global_route *grh)
{
	return grh->sgid_attr->gid_type == IB_GID_TYPE_ROCE_UDP_ENCAP ?
	       grh->traffic_class >> DSCP_SHIFT : grh->traffic_class;
}

void hns_roce_init_uar_table(struct hns_roce_dev *dev);
int hns_roce_uar_alloc(struct hns_roce_dev *dev, struct hns_roce_uar *uar);

int hns_roce_cmd_init(struct hns_roce_dev *hr_dev);
void hns_roce_cmd_cleanup(struct hns_roce_dev *hr_dev);
void hns_roce_cmd_event(struct hns_roce_dev *hr_dev, u16 token, u8 status,
			u64 out_param);
int hns_roce_cmd_use_events(struct hns_roce_dev *hr_dev);
void hns_roce_cmd_use_polling(struct hns_roce_dev *hr_dev);

/* hns roce hw need current block and next block addr from mtt */
#define MTT_MIN_COUNT	 2
static inline dma_addr_t hns_roce_get_mtr_ba(struct hns_roce_mtr *mtr)
{
	return mtr->hem_cfg.root_ba;
}

int hns_roce_mtr_find(struct hns_roce_dev *hr_dev, struct hns_roce_mtr *mtr,
		      u32 offset, u64 *mtt_buf, int mtt_max);
int hns_roce_mtr_create(struct hns_roce_dev *hr_dev, struct hns_roce_mtr *mtr,
			struct hns_roce_buf_attr *buf_attr,
			unsigned int page_shift, struct ib_udata *udata,
			unsigned long user_addr);
void hns_roce_mtr_destroy(struct hns_roce_dev *hr_dev,
			  struct hns_roce_mtr *mtr);
int hns_roce_mtr_map(struct hns_roce_dev *hr_dev, struct hns_roce_mtr *mtr,
		     dma_addr_t *pages, unsigned int page_cnt);

void hns_roce_init_pd_table(struct hns_roce_dev *hr_dev);
void hns_roce_init_mr_table(struct hns_roce_dev *hr_dev);
void hns_roce_init_cq_table(struct hns_roce_dev *hr_dev);
int hns_roce_init_qp_table(struct hns_roce_dev *hr_dev);
void hns_roce_init_srq_table(struct hns_roce_dev *hr_dev);
void hns_roce_init_xrcd_table(struct hns_roce_dev *hr_dev);

void hns_roce_cleanup_cq_table(struct hns_roce_dev *hr_dev);
void hns_roce_cleanup_qp_table(struct hns_roce_dev *hr_dev);

void hns_roce_cleanup_bitmap(struct hns_roce_dev *hr_dev);

int hns_roce_create_ah(struct ib_ah *ah, struct rdma_ah_init_attr *init_attr,
		       struct ib_udata *udata);
int hns_roce_query_ah(struct ib_ah *ibah, struct rdma_ah_attr *ah_attr);
static inline int hns_roce_destroy_ah(struct ib_ah *ah, u32 flags)
{
	return 0;
}

int hns_roce_alloc_pd(struct ib_pd *pd, struct ib_udata *udata);
int hns_roce_dealloc_pd(struct ib_pd *pd, struct ib_udata *udata);

struct ib_mr *hns_roce_get_dma_mr(struct ib_pd *pd, int acc);
struct ib_mr *hns_roce_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
				   u64 virt_addr, int access_flags,
				   struct ib_dmah *dmah,
				   struct ib_udata *udata);
struct ib_mr *hns_roce_rereg_user_mr(struct ib_mr *mr, int flags, u64 start,
				     u64 length, u64 virt_addr,
				     int mr_access_flags, struct ib_pd *pd,
				     struct ib_udata *udata);
struct ib_mr *hns_roce_alloc_mr(struct ib_pd *pd, enum ib_mr_type mr_type,
				u32 max_num_sg);
int hns_roce_map_mr_sg(struct ib_mr *ibmr, struct scatterlist *sg, int sg_nents,
		       unsigned int *sg_offset);
int hns_roce_dereg_mr(struct ib_mr *ibmr, struct ib_udata *udata);
unsigned long key_to_hw_index(u32 key);

void hns_roce_buf_free(struct hns_roce_dev *hr_dev, struct hns_roce_buf *buf);
struct hns_roce_buf *hns_roce_buf_alloc(struct hns_roce_dev *hr_dev, u32 size,
					u32 page_shift, u32 flags);

int hns_roce_get_kmem_bufs(struct hns_roce_dev *hr_dev, dma_addr_t *bufs,
			   int buf_cnt, struct hns_roce_buf *buf,
			   unsigned int page_shift);
int hns_roce_get_umem_bufs(dma_addr_t *bufs,
			   int buf_cnt, struct ib_umem *umem,
			   unsigned int page_shift);

int hns_roce_create_srq(struct ib_srq *srq,
			struct ib_srq_init_attr *srq_init_attr,
			struct ib_udata *udata);
int hns_roce_destroy_srq(struct ib_srq *ibsrq, struct ib_udata *udata);

int hns_roce_alloc_xrcd(struct ib_xrcd *ib_xrcd, struct ib_udata *udata);
int hns_roce_dealloc_xrcd(struct ib_xrcd *ib_xrcd, struct ib_udata *udata);

int hns_roce_create_qp(struct ib_qp *ib_qp, struct ib_qp_init_attr *init_attr,
		       struct ib_udata *udata);
int hns_roce_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		       int attr_mask, struct ib_udata *udata);
void init_flush_work(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp);
void *hns_roce_get_recv_wqe(struct hns_roce_qp *hr_qp, unsigned int n);
void *hns_roce_get_send_wqe(struct hns_roce_qp *hr_qp, unsigned int n);
void *hns_roce_get_extend_sge(struct hns_roce_qp *hr_qp, unsigned int n);
bool hns_roce_wq_overflow(struct hns_roce_wq *hr_wq, u32 nreq,
			  struct ib_cq *ib_cq);
void hns_roce_lock_cqs(struct hns_roce_cq *send_cq,
		       struct hns_roce_cq *recv_cq);
void hns_roce_unlock_cqs(struct hns_roce_cq *send_cq,
			 struct hns_roce_cq *recv_cq);
void hns_roce_qp_remove(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp);
void hns_roce_qp_destroy(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp,
			 struct ib_udata *udata);
__be32 send_ieth(const struct ib_send_wr *wr);
int to_hr_qp_type(int qp_type);

int hns_roce_create_cq(struct ib_cq *ib_cq, const struct ib_cq_init_attr *attr,
		       struct uverbs_attr_bundle *attrs);

int hns_roce_destroy_cq(struct ib_cq *ib_cq, struct ib_udata *udata);
int hns_roce_db_map_user(struct hns_roce_ucontext *context, unsigned long virt,
			 struct hns_roce_db *db);
void hns_roce_db_unmap_user(struct hns_roce_ucontext *context,
			    struct hns_roce_db *db);
int hns_roce_alloc_db(struct hns_roce_dev *hr_dev, struct hns_roce_db *db,
		      int order);
void hns_roce_free_db(struct hns_roce_dev *hr_dev, struct hns_roce_db *db);

void hns_roce_cq_completion(struct hns_roce_dev *hr_dev, u32 cqn);
void hns_roce_cq_event(struct hns_roce_dev *hr_dev, u32 cqn, int event_type);
void flush_cqe(struct hns_roce_dev *dev, struct hns_roce_qp *qp);
void hns_roce_qp_event(struct hns_roce_dev *hr_dev, u32 qpn, int event_type);
void hns_roce_flush_cqe(struct hns_roce_dev *hr_dev, u32 qpn);
void hns_roce_srq_event(struct hns_roce_dev *hr_dev, u32 srqn, int event_type);
void hns_roce_handle_device_err(struct hns_roce_dev *hr_dev);
int hns_roce_init(struct hns_roce_dev *hr_dev);
void hns_roce_exit(struct hns_roce_dev *hr_dev);
int hns_roce_fill_res_cq_entry(struct sk_buff *msg, struct ib_cq *ib_cq);
int hns_roce_fill_res_cq_entry_raw(struct sk_buff *msg, struct ib_cq *ib_cq);
int hns_roce_fill_res_qp_entry(struct sk_buff *msg, struct ib_qp *ib_qp);
int hns_roce_fill_res_qp_entry_raw(struct sk_buff *msg, struct ib_qp *ib_qp);
int hns_roce_fill_res_mr_entry(struct sk_buff *msg, struct ib_mr *ib_mr);
int hns_roce_fill_res_mr_entry_raw(struct sk_buff *msg, struct ib_mr *ib_mr);
int hns_roce_fill_res_srq_entry(struct sk_buff *msg, struct ib_srq *ib_srq);
int hns_roce_fill_res_srq_entry_raw(struct sk_buff *msg, struct ib_srq *ib_srq);
struct hns_user_mmap_entry *
hns_roce_user_mmap_entry_insert(struct ib_ucontext *ucontext, u64 address,
				size_t length,
				enum hns_roce_mmap_type mmap_type);
bool check_sl_valid(struct hns_roce_dev *hr_dev, u8 sl);

#endif /* _HNS_ROCE_DEVICE_H */
