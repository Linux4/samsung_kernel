/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __APDU_R3P0_H__
#define __APDU_R3P0_H__

/* apdu channel reg offset */
#define APDU_TEE_OFFSET	0x0
#define APDU_REE_OFFSET	0x100
#define APDU_CP0_OFFSET	0x200
#define APDU_CP1_OFFSET	0x300

#define APDU_STATUS0		(APDU_REE_OFFSET + 0x0)
#define APDU_STATUS1		(APDU_REE_OFFSET + 0x4)
#define APDU_WATER_MARK		(APDU_REE_OFFSET + 0x8)
#define APDU_INT_EN		(APDU_REE_OFFSET + 0xc)
#define APDU_INT_RAW		(APDU_REE_OFFSET + 0x10)
#define APDU_INT_MASK		(APDU_REE_OFFSET + 0x14)
#define APDU_INT_CLR		(APDU_REE_OFFSET + 0x18)
#define APDU_CNT_CLR		(APDU_REE_OFFSET + 0x1c)
#define APDU_TX_FIFO		(APDU_REE_OFFSET + 0x20)
#define APDU_RX_FIFO		(APDU_REE_OFFSET + 0x60)
#define APDU_INF_INT_EN		(APDU_REE_OFFSET + 0xb0)
#define APDU_INF_INT_RAW	(APDU_REE_OFFSET + 0xb4)
#define APDU_INF_INT_MASK	(APDU_REE_OFFSET + 0xb8)
#define APDU_INF_INT_CLR	(APDU_REE_OFFSET + 0xbc)

#define APDU_FIFO_TX_POINT_OFFSET	(8)
#define APDU_FIFO_RX_OFFSET		(16)
#define APDU_FIFO_RX_POINT_OFFSET	(24)
#define APDU_FIFO_LEN_MASK		GENMASK(7, 0)
#define APDU_CNT_LEN_MASK		GENMASK(15, 0)
#define APDU_TX_THRESHOLD		(0x40)
#define APDU_INT_DEFAULT_EN		(0x1)
#define APDU_INF_INT_DEFAULT_EN		(0x0)

#define APDU_INT_BITS	(BIT(9) | BIT(8) | BIT(5) | BIT(4) | BIT(3) \
				| BIT(2) | BIT(1) | BIT(0))
#define APDU_INT_RX_EMPTY_TO_NOEMPTY	BIT(0)
#define APDU_INT_MED_WR_DONE		BIT(8)
#define APDU_INT_MED_ERR		BIT(9)

#define APDU_INF_INT_BITS		(0xffffffff)
#define APDU_INF_INT_GET_ATR		BIT(31)
#define APDU_INF_INT_ISESYS_INT_FAIL	BIT(30)
#define APDU_INF_INT_APDU_NO_SEC	BIT(29)
#define APDU_INF_INT_HARD_FAULT_STATUS	GENMASK(18, 14)
#define APDU_INF_INT_SELF_CHECK_STATUS	GENMASK(13, 10)
#define APDU_INF_INT_ATTACK		GENMASK(9, 0)
#define APDU_INF_INT_FAULT	(APDU_INF_INT_ATTACK | \
				APDU_INF_INT_SELF_CHECK_STATUS | \
				APDU_INF_INT_HARD_FAULT_STATUS | \
				APDU_INF_INT_APDU_NO_SEC | \
				APDU_INF_INT_ISESYS_INT_FAIL)

#define APDU_DRIVER_NAME	"apdu"

#define APDU_POWER_ON_CHECK_TIMES	(10)
#define APDU_POWER_ON_CHECK_TIMES2	(800)

/* packet was alinged with 4 byte should not exceed max size + pad byte */
#define APDU_TX_MAX_SIZE	(44 * 1024 + 4)
#define APDU_RX_MAX_SIZE	(5120 + 4)
#define APDU_ATR_DATA_MAX_SIZE	(32)
#define MED_INFO_MAX_BLOCK	(5)
/* 2--sizeof med_origin_info_t in words */
#define APDU_MED_INFO_BLK_SIZE	(2)
#define APDU_MED_INFO_SIZE	(MED_INFO_MAX_BLOCK * APDU_MED_INFO_BLK_SIZE)
/* 8--sizeof med_parse_info_t in words */
#define APDU_MED_PARSE_BLK_SZ	(8)
#define APDU_MED_INFO_PARSE_SZ	(MED_INFO_MAX_BLOCK * APDU_MED_PARSE_BLK_SZ)
/* save (ISE_ATTACK_BUFFER_SIZE - 1) attack status, 1 for message header */
#define ISE_ATTACK_BUFFER_SIZE	(33)
#define APDU_FIFO_LENGTH	(128)
#define APDU_FIFO_SIZE		(APDU_FIFO_LENGTH * 4)
#define APDU_MAGIC_NUM		(0x55AA)
#define APDU_MAGIC_MASK(x)	(((x) >> 16) & 0xffff)
#define APDU_DATA_LEN_MASK(x)	((x) & 0xffff)

#define APDU_RESET			_IO('U', 0)
#define APDU_CLR_FIFO			_IO('U', 1)
#define APDU_SET_WATER			_IO('U', 2)
#define APDU_CHECK_CLR_FIFO_DONE	_IO('U', 3)
#define APDU_CHECK_MED_ERROR_STATUS	_IO('U', 4)
#define APDU_CHECK_FAULT_STATUS		_IO('U', 5)
#define APDU_GET_ATR_INF		_IO('U', 6)
#define APDU_SET_MED_HIGH_ADDR		_IO('U', 7)
#define APDU_MED_REWRITE_INFO_PARSE	_IO('U', 8)
#define APDU_NORMAL_PWR_ON_CFG		_IO('U', 9)
#define APDU_ENTER_APDU_LOOP		_IO('U', 10)
#define APDU_FAULT_INT_RESOLVE_DONE	_IO('U', 11)
#define APDU_NORMAL_POWER_ON_ISE	_IO('U', 12)
#define APDU_SOFT_RESET_ISE		_IO('U', 13)
#define APDU_ION_MAP_MEDDDR_IN_KERNEL	_IO('U', 14)
#define APDU_ION_UNMAP_MEDDDR_IN_KERNEL	_IO('U', 15)
#define APDU_POWEROFF_ISE_AON_DOMAIN	_IO('U', 16)
#define APDU_SOFT_RESET_ISE_AON_DOMAIN	_IO('U', 17)
#define APDU_DUMP_MEDDDR_COUNTER_DATA	_IO('U', 18)
#define APDU_SAVE_MEDDDR_COUNTER_DATA	_IO('U', 19)
#define APDU_LOAD_MEDDDR_DATA_IN_KERNEL	_IO('U', 20)
#define APDU_SAVE_MEDDDR_DATA_IN_KERNEL	_IO('U', 21)
#define APDU_SET_CURRENT_SLOT_IN_KERNEL	_IO('U', 22)
#define APDU_ISE_STATUS_CHECK		_IO('U', 23)
#define APDU_SEND_MED_HIGH_ADDR		_IO('U', 24)
#define APDU_SEND_MED_RESERVED_SIZE	_IO('U', 25)
#define APDU_NORMAL_POWER_DOWN_ISE	_IO('U', 26)

#define ISE_BUSY_STATUS		(0x60)
#define AP_WAIT_TIMES		(20)
#define DIV_CEILING(a, b)	(((a) + ((b) - 1)) / (b))

#define APDU_NETLINK		(30)
#define APDU_USER_PORT		(100)
#define DDR_BASE		(0x80000000)
#define MED_ISE_MAX_SIZE	(0x1C00000)
#define MED_DDR_MIN_SIZE	(8 * 1024 * 1024)
#define MED_DDR_MAX_SIZE	(64 * 1024 * 1024)
/* ise just send med offset and length info */
#define ISE_MED_BASE_ADDR	(0x0)

#define MESSAGE_HEADER_MED_INFO	(0x5AA55AA5)
#define MESSAGE_HEADER_FAULT	(0x6BB66BB6)
#define MESSAGE_HEADER_MED_ERR	(0x7CC77CC7)

#define POWER_SWITCH_HARD	(0x55AA)
#define POWER_SWITCH_SOFT	(0x6B6B)
#define POWER_COLD_BOOT		(0x5A5A)

/* MAX_WAIT_TIME milliseconds --ISE system hangs up and exits */
#define MAX_WAIT_TIME		(5000)

#define APDU_PD_CHECK_TIMEOUT	(100)
#define APDU_WR_PRE_TIMEOUT	(300)
#define APDU_WR_PRE_TIMEOUT_2	(1000)
#define APDU_WR_PRE_TIMEOUT_3	(2000)
#define APDU_WR_RD_TIMEOUT	(200)

/* data in big endian, 0x9000 */
#define ISE_APDU_ANSWER_SUCCESS	(0x0090)

#define ISE_PD_PWR_ON		(0xAA)
#define ISE_PD_PWR_DOWN		(0x55)

/* AXI 35bit addressing */
#define MED_LOW_ADDRESSING	GENMASK_ULL(25, 0)
#define MED_HIGH_ADDRESSING	GENMASK_ULL(34, 26)

#define ISE_PD_MAGIC_NUM	(0xDEAD8810)

#define MEDDDR_ISEDATA_OFFSET_BASE_ADDRESS	(0x200000)
#define MEDDDR_COUNTER_AREA_MAX_SIZE		(0x600) //0x24
#define MEDDDR_COUNTER_LV1_AREA_MAX_SIZE	(0x600)
#define ISEDATA_DEV_PATH	"dev/block/by-name/isedata"

struct sprd_apdu_pub_ise_cfg_t {
	struct regmap *pub_reg_base;
	u32 pub_ise_reg_offset;
	u32 pub_ise_bit_offset;
	u64 med_hi_addr;
	u64 med_size;
	u64 med_ise_size;
};

struct sprd_apdu_pd_ise_config_t {
	/* keep the same at dts reserved-memory */
	const char *ise_compatible;
	struct regmap *ise_pd_reg_base;
	struct regmap *ise_aon_reg_base;
	struct regmap *ise_aon_clk_reg_base;
	u32 ise_pd_magic_num;
	/* check ise power on status reg */
	long (*ise_pd_status_check)(void *apdu_dev);
	/* ise cold power on function (ise first time power on) */
	long (*ise_cold_power_on)(void *apdu_dev);
	/* ise full power down function (phone shutdown) */
	long (*ise_full_power_down)(void *apdu_dev);
	/*
	 * reset ise aon domain and power down domain
	 * (ensure ise cannot reading med ddr)
	 */
	long (*ise_hard_reset)(void *apdu_dev);
	/* reset ise power down domain (ise status not limit) */
	long (*ise_soft_reset)(void *apdu_dev);
	/* hard reset signal set high */
	long (*ise_hard_reset_set)(void *apdu_dev);
	/* hard reset signal release low */
	long (*ise_hard_reset_clr)(void *apdu_dev);
};

struct sprd_apdu_ise_fault_status_t {
	u32 *ise_fault_buf;
	u32 ise_fault_ptr;
	u32 ise_fault_status;
	/* ensure user space have slove last fault status */
	u32 ise_fault_allow_to_send_flag;
};

struct sprd_apdu_med_rewrite_t {
	/* ise med rewrite origin format info */
	u32 *med_rewrite;
	/* indicate that ise sending med rewrite info */
	u32 med_wr_done;
	/*
	 * med rewrite info sending to user space
	 * or med rewrite data writing to flash
	 */
	u32 med_data_processing;
};

struct sprd_apdu_med_error_t {
	/* indicate that ise sending med error status */
	u32 med_error_status;
};

struct sprd_apdu_atr_t {
	u32 *atr;
	/* indicate that ise sending atr info */
	u32 atr_rcv_status;
	u32 ise_gp_status[ISE_ATTACK_BUFFER_SIZE];
	u32 gp_index;
};

struct sprd_apdu_device {
	struct device *dev;
	struct miscdevice misc;
	wait_queue_head_t read_wq;
	int rx_done;
	void *tx_buf;
	void *rx_buf;

	/* synchronize access to our device file */
	struct mutex mutex;
	void __iomem *base;
	int irq;

	/* for exchange data between DDR and FLASH */
	void *medddr_address;
	int slot;

	struct sprd_apdu_pub_ise_cfg_t pub_ise_cfg;
	struct sprd_apdu_pd_ise_config_t *pd_ise;
	struct sprd_apdu_ise_fault_status_t ise_fault;
	struct sprd_apdu_med_rewrite_t med_rewr;
	struct sprd_apdu_med_error_t med_err;
	struct sprd_apdu_atr_t ise_atr;
};

/* origin data info from ISE */
struct med_origin_info_t {
	u32 med_data_offset;
	u32 med_data_len;
};

struct med_parse_info_t {
	u32 ap_side_data_offset;
	u32 ap_side_data_length;
	u32 level1_rng_offset;
	u32 level1_rng_length;
	u32 level2_rng_offset;
	u32 level2_rng_length;
	u32 level3_rng_offset;
	u32 level3_rng_length;
};

extern struct net init_net;

long med_rewrite_post_process_check(struct sprd_apdu_device *apdu);
long sprd_apdu_normal_pd_ise_req(struct sprd_apdu_device *apdu);
long sprd_apdu_power_on_check(struct sprd_apdu_device *apdu, u32 times);
void sprd_apdu_normal_pd_sync_cnt_lv1(struct sprd_apdu_device *apdu);

#endif

