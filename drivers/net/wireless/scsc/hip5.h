/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __HIP5_H__
#define __HIP5_H__

/**
 * This header file is the public HIP5 interface, which will be accessible by
 * Wi-Fi service driver components.
 *
 * All struct and internal HIP functions shall be moved to a private header
 * file.
 */

#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/skbuff.h>
#include <scsc/scsc_mifram.h>
#include <scsc/scsc_mx.h>
#ifdef CONFIG_SCSC_WLAN_ANDROID
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif
#endif
#include "mbulk.h"

/* Shared memory Layout
 *
 * |-------------------------| CONFIG
 * |    CONFIG  + Queues     |
 * |       ---------         |
 * |          MIB            |
 * |-------------------------| TX Pool
 * |         TX DAT          |
 * |       ---------         |
 * |         TX CTL          |
 * |-------------------------| RX Pool
 * |          RX             |
 * |-------------------------|
 */

/**** OFFSET SHOULD BE 4096 BYTES ALIGNED ***/
/*** CONFIG POOL ***/
#define HIP5_WLAN_CONFIG_OFFSET	0x00000
#define HIP5_WLAN_CONFIG_SIZE	0x80000 /* 512 kB */
/*** MIB POOL ***/
#define HIP5_WLAN_MIB_OFFSET	(HIP5_WLAN_CONFIG_OFFSET +  HIP5_WLAN_CONFIG_SIZE)
#define HIP5_WLAN_MIB_SIZE	0x08000 /* 32 kB */
/*** TX POOL ***/
#define HIP5_WLAN_TX_OFFSET	(HIP5_WLAN_MIB_OFFSET + HIP5_WLAN_MIB_SIZE)
/*** TX POOL - DAT POOL ***/
#define HIP5_WLAN_TX_DAT_OFFSET	HIP5_WLAN_TX_OFFSET
#define HIP5_WLAN_TX_DAT_SIZE	0x200000 /* 2 MB */
/*** TX POOL - CTL POOL ***/
#define HIP5_WLAN_TX_CTL_OFFSET	(HIP5_WLAN_TX_DAT_OFFSET + HIP5_WLAN_TX_DAT_SIZE)
#define HIP5_WLAN_TX_CTL_SIZE	0x10000 /*  64 kB */
#define HIP5_WLAN_TX_SIZE	(HIP5_WLAN_TX_DAT_SIZE + HIP5_WLAN_TX_CTL_SIZE)
/*** RX POOL ***/
#define HIP5_WLAN_RX_OFFSET	(HIP5_WLAN_TX_CTL_OFFSET +  HIP5_WLAN_TX_CTL_SIZE)
#define HIP5_WLAN_RX_SIZE	0x100000 /* 1 MB */

/*** TOTAL : CONFIG POOL + TX POOL + RX POOL ***/
#define HIP5_WLAN_TOTAL_MEM	(HIP5_WLAN_CONFIG_SIZE + HIP5_WLAN_MIB_SIZE + \
				 HIP5_WLAN_TX_SIZE + HIP5_WLAN_RX_SIZE) /* 2 MB + 104 KB*/

#define HIP5_DAT_MBULK_SIZE	(2 * 1024)
#define HIP5_DAT_SLOTS		(HIP5_WLAN_TX_DAT_SIZE / HIP5_DAT_MBULK_SIZE)
#define HIP5_CTL_MBULK_SIZE	(2 * 1024)
#define HIP5_CTL_SLOTS		(HIP5_WLAN_TX_CTL_SIZE / HIP5_CTL_MBULK_SIZE)

/* external */
#define SLSI_HIP_TX_DATA_SLOTS_NUM HIP5_DAT_SLOTS

#define MIF_HIP_CFG_Q_NUM       24

#define MIF_NO_IRQ		0xff

enum hip5_hip_q_conf {
	/* UCPU0 FH */
	HIP5_MIF_Q_FH_CTRL = 0,
    HIP5_MIF_Q_FH_PRI0,
    HIP5_MIF_Q_FH_SKB0,
    HIP5_MIF_Q_FH_RFBC,
    HIP5_MIF_Q_FH_RFBD0,

    /* UCPU0 TH */
    HIP5_MIF_Q_TH_CTRL,
    HIP5_MIF_Q_TH_DAT0,
    HIP5_MIF_Q_TH_RFBC,
    HIP5_MIF_Q_TH_RFBD0,

    /* UCPU1 FH */
    HIP5_MIF_Q_FH_PRI1,
    HIP5_MIF_Q_FH_SKB1,
    HIP5_MIF_Q_FH_RFBD1,
    HIP5_MIF_Q_TH_DAT1,
    HIP5_MIF_Q_TH_RFBD1,

    /* FH DATA */
    HIP5_MIF_Q_FH_DAT0,
    HIP5_MIF_Q_FH_DAT1,
    HIP5_MIF_Q_FH_DAT2,
    HIP5_MIF_Q_FH_DAT3,
    HIP5_MIF_Q_FH_DAT4,
    HIP5_MIF_Q_FH_DAT5,
    HIP5_MIF_Q_FH_DAT6,
    HIP5_MIF_Q_FH_DAT7,
    HIP5_MIF_Q_FH_DAT8,
    HIP5_MIF_Q_FH_DAT9
};

struct hip5_mif_q {
	u8  q_type;
	u16 q_len;
	u16 q_idx_sz;
	u16 q_entry_sz;
	u8  int_n;
	u32 q_loc;
	u8  ucpu;
	u8  vif;
} __packed;

struct hip5_hip_config_version_1 {
	/* Host owned */
	u32 magic_number;       /* 0xcaba0401 */
	u16 hip_config_ver;     /* Version of this configuration structure = 2*/
	u16 config_len;         /* Size of this configuration structure */

	/* FW owned */
	u32 compat_flag;         /* flag of the expected driver's behaviours */

	u16 sap_mlme_ver;        /* Fapi SAP_MLME version*/
	u16 sap_ma_ver;          /* Fapi SAP_MA version */
	u16 sap_debug_ver;       /* Fapi SAP_DEBUG version */
	u16 sap_test_ver;        /* Fapi SAP_TEST version */

	u32 fw_build_id;         /* Firmware Build Id */
	u32 fw_patch_id;         /* Firmware Patch Id */

	u8  unidat_req_headroom; /* Headroom the host shall reserve in mbulk for MA-UNITDATA.REQ signal */
	u8  unidat_req_tailroom; /* Tailroom the host shall reserve in mbulk for MA-UNITDATA.REQ signal */
	u8  bulk_buffer_align;   /* 4 */

	/* Host owned */
	u8  host_cache_line;    /* 64 */

	u32 host_buf_loc;       /* location of the host buffer in MIF_ADDR */
	u32 host_buf_sz;        /* in byte, size of the host buffer */
	u32 fw_buf_loc;         /* location of the firmware buffer in MIF_ADDR */
	u32 fw_buf_sz;          /* in byte, size of the firmware buffer */
	u32 mib_loc;            /* MIB location in MIF_ADDR */
	u32 mib_sz;             /* MIB size */
	u32 log_config_loc;     /* Logging Configuration Location in MIF_ADDR */
	u32 log_config_sz;      /* Logging Configuration Size in MIF_ADDR */
	u32 scbrd_loc;          /* Scoreboard locatin in MIF_ADDR */

	u16 q_num;              /* 24 */
	struct hip5_mif_q q_cfg[MIF_HIP_CFG_Q_NUM];
} __packed;

struct hip5_hip_config_version_2 {
	/* Host owned */
	u32 magic_number;       /* 0xcaba0401 */
	u16 hip_config_ver;     /* Version of this configuration structure = 2*/
	u16 config_len;         /* Size of this configuration structure */

	/* FW owned */
	u32 compat_flag;         /* flag of the expected driver's behaviours */

	u16 sap_mlme_ver;        /* Fapi SAP_MLME version*/
	u16 sap_ma_ver;          /* Fapi SAP_MA version */
	u16 sap_debug_ver;       /* Fapi SAP_DEBUG version */
	u16 sap_test_ver;        /* Fapi SAP_TEST version */

	u32 fw_build_id;         /* Firmware Build Id */
	u32 fw_patch_id;         /* Firmware Patch Id */

	u8  unidat_req_headroom; /* Headroom the host shall reserve in mbulk for MA-UNITDATA.REQ signal */
	u8  unidat_req_tailroom; /* Tailroom the host shall reserve in mbulk for MA-UNITDATA.REQ signal */
	u8  bulk_buffer_align;   /* 4 */

	/* Host owned */
	u8  host_cache_line;    /* 64 */

	u32 host_buf_loc;       /* location of the host buffer in MIF_ADDR */
	u32 host_buf_sz;        /* in byte, size of the host buffer */
	u32 fw_buf_loc;         /* location of the firmware buffer in MIF_ADDR */
	u32 fw_buf_sz;          /* in byte, size of the firmware buffer */
	u32 mib_loc;            /* MIB location in MIF_ADDR */
	u32 mib_sz;             /* MIB size */
	u32 log_config_loc;     /* Logging Configuration Location in MIF_ADDR */
	u32 log_config_sz;      /* Logging Configuration Size in MIF_ADDR */

	u8 mif_fh_int_n;		/* MIF from-host interrupt bit position for all HIP queue */
	u8 reserved1[3];

	u8 mif_th_int_n[6];		/* MIF to-host interrupt bit positions for each HIP queue */
	u8 reserved2[2];

	u32 scbrd_loc;          /* Scoreboard locatin in MIF_ADDR */

	u16 q_num;              /* 24 */
	struct hip5_mif_q q_cfg[MIF_HIP_CFG_Q_NUM];
} __packed;

struct hip5_hip_init {
	/* Host owned */
	u32 magic_number;       /* 0xcaaa0400 */
	/* FW owned */
	u32 conf_hip5_ver;
	/* Host owned */
	u32 version_a_ref;      /* Location of Config structure A (old) */
	u32 version_b_ref;      /* Location of Config structure B (new) */
} __packed;

#define MAX_NUM 1024

/* HIP entry chain flags */
#define HIP5_HIP_ENTRY_CHAIN_HEAD                 (1 << 0)
#define HIP5_HIP_ENTRY_CHAIN_CONTINUE             (1 << 1)
#define HIP5_HIP_ENTRY_CHAIN_END                  (1 << 2)

struct hip5_mbulk_bd
{
    u32 buf_addr;    /* Buffer address
                      * LSB 0: from Shared DRAM without shifting,
                      * LSB 1: (address & ~0x1) << 8
                      */
    u16 buf_sz;      /* buffer size in byte */
    u16 len;         /* valid data length */
    u16 offset;      /* start offset of data */
    u8  flag;        /* Chained, types of memory */
    u8  extra;
} __packed; /* 12 bytes */

struct hip5_hip_entry {
	u16 data_len;	/* total length of data in multiple HIP entries */
	u8 num_bd: 4;	/* number of BD in the HIP entry */
	u8 flag: 4;		/* Bit0: Chain HEAD,
					 * Bit1: Chain continue, Bit2: End of chain
				     */
	u8 sig_len;	 	/* 0 or signal length defined in FAPI,
					 * 0: signal omitted
					 */
	struct hip5_mbulk_bd bd[2];
	union
	{
		struct hip5_mbulk_bd bdx[3];
		u8 fapi_sig[34];
	};
} __packed __aligned(64);

struct hip5_hip_q_tlv {
	struct hip5_hip_entry array[MAX_NUM];
	u16  idx_read;      /* To keep track */
	u16  idx_write;     /* To keep track */
	u16  total;
} __aligned(64);

struct hip5_hip_q {
	u32 array[MAX_NUM];
	u16  idx_read;
	u16  idx_write;
	u16  total;
} __aligned(64);

struct hip5_hip_control {
	struct hip5_hip_init             init;
	struct hip5_hip_config_version_1 config_v5 __aligned(32);
	struct hip5_hip_config_version_1 config_v4 __aligned(32);
	u32                              scoreboard[256] __aligned(64);
	struct hip5_hip_q                q[14] __aligned(64);
	struct hip5_hip_q_tlv            q_tlv[4] __aligned(64);
} __aligned(4096);

struct slsi_hip;
#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
struct bh_struct;
#endif

struct hip5_tx_skb_entry {
	bool in_use;
	u32 colour;
	u32 skb_dma_addr;
	struct sk_buff *skb;
};

/* This struct is private to the HIP implementation */
struct hip_priv {
	spinlock_t                   napi_cpu_lock;

	struct bh_struct            *bh_dat;
	struct bh_struct            *bh_ctl;
	struct bh_struct            *bh_rfb;

	/* Interrupts cache */
	u32                          intr_from_host_ctrl;
	u32                          intr_to_host_ctrl;
	u32                          intr_to_host_ctrl_fb;
	u32                          intr_from_host_data;
	u32                          intr_to_host_data1;
	u32                          intr_to_host_data2;

	/* For workqueue */
	struct slsi_hip             *hip;

	/* Pool for data frames*/
	u8                           host_pool_id_dat;
	/* Pool for ctl frames*/
	u8                           host_pool_id_ctl;

	/* rx cycle lock */
	spinlock_t                   rx_lock;
	/* tx cycle lock */
	spinlock_t                   tx_lock;

	/* Scoreboard update spinlock */
	rwlock_t                     rw_scoreboard;

#ifdef CONFIG_SCSC_WLAN_ANDROID
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct scsc_wake_lock             wake_lock_tx;
	struct scsc_wake_lock             wake_lock_ctrl;
	struct scsc_wake_lock             wake_lock_data;
#else

	struct wake_lock             wake_lock_tx;
	struct wake_lock             wake_lock_ctrl;
	struct wake_lock             wake_lock_data;
#endif
#endif

	/* Control the hip instance deinit */
	atomic_t                     closing;
	atomic_t                     in_tx;
	atomic_t                     in_rx;
	atomic_t                     in_suspend;
	u32                          storm_count;

	/*minor*/
	u32                          minor;
	u8                           unidat_req_headroom; /* Headroom the host shall reserve in mbulk for MA-UNITDATA.REQ signal */
	u8                           unidat_req_tailroom; /* Tailroom the host shall reserve in mbulk for MA-UNITDATA.REQ signal */
	u32                          version; /* Version of the running FW */

	void                         *scbrd_base; /* Scbrd_base pointer */
	__iomem void                 *scbrd_ramrp_base; /* Scbrd_base pointer in RAMRP */

#ifndef CONFIG_SCSC_WLAN_TX_API
	/* Global domain Q control*/
	atomic_t                     gactive;
	atomic_t                     gmod;
	atomic_t                     gcod;
	int                          saturated;
	int                          guard;
	/* Global domain Q spinlock */
	spinlock_t                   gbot_lock;
#endif
#ifdef CONFIG_SCSC_QOS
	/* PM QoS control */
	struct work_struct           pm_qos_work;
	/* PM QoS control spinlock */
	spinlock_t                   pm_qos_lock;
	u8                           pm_qos_state;
#endif
	/* Collection artificats */
	void                         *mib_collect;
	u16                          mib_sz;
	/* Mutex to protect hcf file collection if a tear down is triggered */
	struct mutex                 in_collection;

	struct workqueue_struct      *hip_workq;
	struct hip5_tx_skb_entry     tx_skb_table[MAX_NUM];
};

struct scsc_service;

/* Macros for accessing information stored in the hip_config struct */
#define scsc_wifi_get_hip_config_version_4_u8(buff_ptr, member) le16_to_cpu((((struct hip5_hip_config_version_1 *)(buff_ptr))->member))
#define scsc_wifi_get_hip_config_version_4_u16(buff_ptr, member) le16_to_cpu((((struct hip5_hip_config_version_1 *)(buff_ptr))->member))
#define scsc_wifi_get_hip_config_version_4_u32(buff_ptr, member) le32_to_cpu((((struct hip5_hip_config_version_1 *)(buff_ptr))->member))
#define scsc_wifi_get_hip_config_version_5_u8(buff_ptr, member) le16_to_cpu((((struct hip5_hip_config_version_2 *)(buff_ptr))->member))
#define scsc_wifi_get_hip_config_version_5_u16(buff_ptr, member) le16_to_cpu((((struct hip5_hip_config_version_2 *)(buff_ptr))->member))
#define scsc_wifi_get_hip_config_version_5_u32(buff_ptr, member) le32_to_cpu((((struct hip5_hip_config_version_2 *)(buff_ptr))->member))
#define scsc_wifi_get_hip_config_u8(buff_ptr, member, ver) le16_to_cpu((((struct hip5_hip_config_version_1 *)(buff_ptr->config_v##ver))->member))
#define scsc_wifi_get_hip_config_u16(buff_ptr, member, ver) le16_to_cpu((((struct hip5_hip_config_version_##ver *)(buff_ptr->config_v##ver))->member))
#define scsc_wifi_get_hip_config_u32(buff_ptr, member, ver) le32_to_cpu((((struct hip5_hip_config_version_##ver *)(buff_ptr->config_v##ver))->member))
#define scsc_wifi_get_hip_config_version(buff_ptr) le32_to_cpu((((struct hip5_hip_init *)(buff_ptr))->conf_hip5_ver))

#endif /* __HIP5_H__ */
