/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_H
#define _UFS_MEDIATEK_H

#include "linux/spinlock_types.h"
#include <linux/bitops.h>
#include <linux/pm_qos.h>
#include <linux/of_device.h>
#include <linux/cdev.h>
#include <ufs/ufs.h>
#include <ufs/ufshci.h>
#include <ufs/ufshcd.h>
#include "ufs-mediatek-rpmb.h"

/*
 * MCQ define and struct
 */
#define UFSHCD_MAX_Q_NR 8
#define MTK_MCQ_INVALID_IRQ	0xFFFF

#define UFSHCD_ENABLE_MTK_MCQ_INTRS	\
				(UTP_TASK_REQ_COMPL | UFSHCD_ERROR_MASK)

/* REG_UFS_MMIO_OPT_CTRL_0 160h */
#define EHS_EN                  0x1
#define PFM_IMPV                0x2
#define MCQ_MULTI_INTR_EN       0x4
#define MCQ_CMB_INTR_EN         0x8
#define MCQ_AH8                 0x10
#define MON_EN			0x20

#define MCQ_INTR_EN_MSK         (MCQ_MULTI_INTR_EN | MCQ_CMB_INTR_EN)

/*
 * Vendor specific UFSHCI Registers
 */
#define REG_UFS_XOUFS_CTRL          0x140
#define REG_UFS_REFCLK_CTRL         0x144
#define REG_UFS_MMIO_OPT_CTRL_0     0x160
#define REG_UFS_EXTREG              0x2100
#define REG_UFS_MPHYCTRL            0x2200
#define REG_UFS_MTK_IP_VER          0x2240
#define REG_UFS_REJECT_MON          0x22AC
#define REG_UFS_AH8E_MON            0x22B0
#define REG_UFS_AH8X_MON            0x22B4
#define REG_UFS_DEBUG_SEL           0x22C0
#define REG_UFS_PROBE               0x22C8
#define REG_UFS_DEBUG_SEL_B0        0x22D0
#define REG_UFS_DEBUG_SEL_B1        0x22D4
#define REG_UFS_DEBUG_SEL_B2        0x22D8
#define REG_UFS_DEBUG_SEL_B3        0x22DC
#define REG_UFS_SQ_DBR_DBG          0x22FC
#define REG_UFS_ACT_STS             0x2304

#define REG_UFS_MTK_SQD             0x2800
#define REG_UFS_MTK_SQIS            0x2814
#define REG_UFS_MTK_CQD             0x281C
#define REG_UFS_MTK_CQIS            0x2824
#define REF_UFS_MTK_CQ7_IACR        0x297C

#define REG_UFS_MCQ_STRIDE          0x30

/*
 * Should define in unipro.h
 */
#define PA_TXHSG1SYNCLENGTH	0x1552
#define PA_TXHSG2SYNCLENGTH	0x1554
#define PA_TXHSG3SYNCLENGTH	0x1556
#define PA_TXHSG4SYNCLENGTH	0x15D0
#define PA_TXHSG5SYNCLENGTH	0x15D6

/*
 * Ref-clk control
 *
 * Values for register REG_UFS_REFCLK_CTRL
 */
#define REFCLK_RELEASE              0x0
#define REFCLK_REQUEST              BIT(0)
#define REFCLK_ACK                  BIT(1)

#define REFCLK_REQ_TIMEOUT_US       3000
#define REFCLK_DEFAULT_WAIT_US      32

/*
 * Other attributes
 */
#define VS_DEBUGCLOCKENABLE         0xD0A1
#define VS_SAVEPOWERCONTROL         0xD0A6
#define VS_UNIPROPOWERDOWNCONTROL   0xD0A8

/*
 * Vendor specific link state
 */
enum {
	VS_LINK_DISABLED            = 0,
	VS_LINK_DOWN                = 1,
	VS_LINK_UP                  = 2,
	VS_LINK_HIBERN8             = 3,
	VS_LINK_LOST                = 4,
	VS_LINK_CFG                 = 5,
};

/*
 * Vendor specific host controller state
 */
enum {
	VS_HCE_RESET                = 0,
	VS_HCE_BASE                 = 1,
	VS_HCE_OOCPR_WAIT           = 2,
	VS_HCE_DME_RESET            = 3,
	VS_HCE_MIDDLE               = 4,
	VS_HCE_DME_ENABLE           = 5,
	VS_HCE_DEFAULTS             = 6,
	VS_HIB_IDLEEN               = 7,
	VS_HIB_ENTER                = 8,
	VS_HIB_ENTER_CONF           = 9,
	VS_HIB_MIDDLE               = 10,
	VS_HIB_WAITTIMER            = 11,
	VS_HIB_EXIT_CONF            = 12,
	VS_HIB_EXIT                 = 13,
};

/*
 * VS_DEBUGCLOCKENABLE
 */
enum {
	TX_SYMBOL_CLK_REQ_FORCE = 5,
};

/*
 * VS_SAVEPOWERCONTROL
 */
enum {
	RX_SYMBOL_CLK_GATE_EN   = 0,
	SYS_CLK_GATE_EN         = 2,
	TX_CLK_GATE_EN          = 3,
};

/*
 * Host capability
 */
enum ufs_mtk_host_caps {
	UFS_MTK_CAP_BOOST_CRYPT_ENGINE         = 1 << 0,
	UFS_MTK_CAP_VA09_PWR_CTRL              = 1 << 1,
	UFS_MTK_CAP_DISABLE_AH8                = 1 << 2,
	UFS_MTK_CAP_BROKEN_VCC                 = 1 << 3,

	/* Override UFS_MTK_CAP_BROKEN_VCC's behavior to
	 * allow vccqx upstream to enter LPM
	 */
	UFS_MTK_CAP_ALLOW_VCCQX_LPM            = 1 << 5,
	UFS_MTK_CAP_PMC_VIA_FASTAUTO           = 1 << 6,
	UFS_MTK_CAP_TX_SKEW_FIX                = 1 << 7,
	UFS_MTK_CAP_DISABLE_MCQ                = 1 << 8,
	/* Control MTCMOS with RTFF */
	UFS_MTK_CAP_RTFF_MTCMOS                = 1 << 9,

	UFS_MTK_CAP_MCQ_BROKEN_RTC             = 1 << 10,
	UFS_MTK_CAP_MPHY_DUMP                  = 1 << 11,
	UFS_MTK_CAP_BYPASS_VCCQX_LPM           = 1 << 12,
};

struct ufs_mtk_crypt_cfg {
	struct regulator *reg_vcore;
	struct clk *clk_crypt_perf;
	struct clk *clk_crypt_mux;
	struct clk *clk_crypt_lp;
	int vcore_volt;
};

struct ufs_mtk_clk {
	struct ufs_clk_info *ufs_sel_clki; // mux
	struct ufs_clk_info *ufs_sel_max_clki; // max src
	struct ufs_clk_info *ufs_sel_min_clki; // min src
	struct ufs_clk_info *ufs_fde_clki; // mux
	struct ufs_clk_info *ufs_fde_max_clki; // max src
	struct ufs_clk_info *ufs_fde_min_clki; // min src
	struct regulator *reg_vcore;
	int vcore_volt;
};

struct ufs_mtk_hw_ver {
	u8 step;
	u8 minor;
	u8 major;
};

#if IS_ENABLED(CONFIG_UFS_MEDIATEK_INTERNAL)
struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};
#endif

enum rpmb_key_state {
	RPMB_KEY_ST_UNKNOWN = 0x1,
	RPMB_KEY_ST_PROGRAMMED,
	RPMB_KEY_ST_NOT_PROGRAMMED,
};

struct tag_ufs {
	enum rpmb_key_state rpmb_r2_kst;  /* RPMB Region 2 Key State*/
};

struct ufs_mtk_mcq_intr_info {
	struct ufs_hba *hba;
	u32 irq;
	u8 qid;
};

struct ufs_mtk_host {
	struct phy *mphy;
	void __iomem *mphy_base;
	struct regulator *reg_va09;
	struct reset_control *hci_reset;
	struct reset_control *unipro_reset;
	struct reset_control *crypto_reset;
	struct reset_control *mphy_reset;
	struct ufs_hba *hba;
	struct ufs_mtk_crypt_cfg *crypt;
	struct ufs_mtk_hw_ver hw_ver;
	enum ufs_mtk_host_caps caps;
	bool mphy_powered_on;
	bool unipro_lpm;
	bool ref_clk_enabled;
	bool clk_scale_up;
	bool clk_scale_forbid;
	atomic_t clkscale_control;
	atomic_t skip_btag;
	atomic_t dbg_tp_unregister;
	u16 ref_clk_ungating_wait_us;
	u16 ref_clk_gating_wait_us;
	u32 ip_ver;
	u32 desired_ahit;
	u32 max_gear;
	struct ufs_mtk_clk mclk;
	bool pm_qos_init;
	struct pm_qos_request pm_qos_req;
	bool qos_allowed;
	bool qos_enabled;
	bool boot_device;

	struct completion luns_added;
	struct delayed_work delay_eh_work;
	struct workqueue_struct *delay_eh_workq;
	struct wakeup_source *ufs_wake_lock;

	struct delayed_work phy_dmp_work;
	struct workqueue_struct *phy_dmp_workq;

#ifdef CONFIG_PM_SLEEP
	struct delayed_work rq_dwork;
	struct workqueue_struct *rq_workq;
#endif

#ifdef CONFIG_MTK_UFS_DEBUG_BUILD
	/* clk debug */
	struct notifier_block clk_notifier;
#endif
	struct device *phy_dev;

	/* RPMB */
	struct semaphore rpmb_sem;
	struct scsi_device *sdev_rpmb;
	struct cdev ise_rpmb_cdev;
	struct device *rpmb_bsg_dev;
	struct request_queue	*rpmb_bsg_queue;
	spinlock_t purge_lock;
	struct timer_list purge_timer;
	bool purge_active;

	bool mcq_set_intr;
	bool is_mcq_intr_enabled;
	int mcq_nr_intr;
	struct ufs_mtk_mcq_intr_info mcq_intr_info[UFSHCD_MAX_Q_NR];
	struct tag_ufs *atag;

	bool skip_flush;
};

#define UFSHCD_MAX_TAG	256


/*
 *  IOCTL opcode for ufs queries has the following opcode after
 *  SCSI_IOCTL_GET_PCI
 */
#define UFS_IOCTL_QUERY			0x5388

/* MTK delay of autosuspend: 500 ms */
#define MTK_RPM_AUTOSUSPEND_DELAY_MS 500

/**
 * struct ufs_ioctl_query_data - used to transfer data to and from user via
 * ioctl
 * @opcode: type of data to query (descriptor/attribute/flag)
 * @idn: id of the data structure
 * @buf_size: number of allocated bytes/data size on return
 * @buffer: data location
 *
 * Received: buffer and buf_size (available space for transferred data)
 * Submitted: opcode, idn, length, buf_size
 */
struct ufs_ioctl_query_data {
	/*
	 * User should select one of the opcode defined in "enum query_opcode".
	 * Please check include/uapi/scsi/ufs/ufs.h for the definition of it.
	 * Note that only UPIU_QUERY_OPCODE_READ_DESC,
	 * UPIU_QUERY_OPCODE_READ_ATTR & UPIU_QUERY_OPCODE_READ_FLAG are
	 * supported as of now. All other query_opcode would be considered
	 * invalid.
	 * As of now only read query operations are supported.
	 */
	__u32 opcode;
	/*
	 * User should select one of the idn from "enum flag_idn" or "enum
	 * attr_idn" or "enum desc_idn" based on whether opcode above is
	 * attribute, flag or descriptor.
	 * Please check include/uapi/scsi/ufs/ufs.h for the definition of it.
	 */
	__u8 idn;
	__u8 idx;
	/*
	 * User should specify the size of the buffer (buffer[0] below) where
	 * it wants to read the query data (attribute/flag/descriptor).
	 * As we might end up reading less data then what is specified in
	 * buf_size. So we are updating buf_size to what exactly we have read.
	 */
	__u16 buf_size;
	/*
	 * placeholder for the start of the data buffer where kernel will copy
	 * the query data (attribute/flag/descriptor) read from the UFS device
	 * Note:
	 * For Read/Write Attribute you will have to allocate 4 bytes
	 * For Read/Write Flag you will have to allocate 1 byte
	 */
	__u8 *buffer;
};

enum {
	BOOTDEV_SDMMC = 1,
	BOOTDEV_UFS   = 2
};

/* UFS MTK ip version value */
enum {
	IP_VER_NONE   = 0x0,
	IP_VER_MT6878 = 0x10420200,
	IP_VER_MT6897 = 0x10440000,
	IP_VER_MT6989 = 0x10450000
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

enum {
	CLK_SCALE_FREE_RUN,
	CLK_FORCE_SCALE_DOWN,
	CLK_FORCE_SCALE_UP,
	CLK_FORCE_SCALE_G1
};

void ufs_mtk_dynamic_clock_scaling(struct ufs_hba *hba, int mode);

/**
 * ufshcd_upiu_wlun_to_scsi_wlun - maps UPIU W-LUN id to SCSI W-LUN ID
 * @upiu_wlun_id: UPIU W-LUN id
 *
 * Returns SCSI W-LUN id
 */
static inline u16 ufshcd_upiu_wlun_to_scsi_wlun(u8 upiu_wlun_id)
{
	return (upiu_wlun_id & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE;
}

static inline const void *ufs_mtk_get_boot_property(struct device_node *np,
	const char *name, int *lenp)
{
	struct device_node *boot_node = NULL;

	boot_node = of_parse_phandle(np, "bootmode", 0);
	if (!boot_node)
		return NULL;
	return of_get_property(boot_node, name, lenp);
}

#endif /* !_UFS_MEDIATEK_H */
