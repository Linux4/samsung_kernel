#ifndef _NPU_CONFIG_H_
#define _NPU_CONFIG_H_

/* Timeout - ns unit (ms * 1000000) */
#define NPU_TIMEOUT_HARD_PROTODRV_COMPLETED	(5000 * 1000000L)	/* PROCESSING to COMPLETED */
#define NPU_TIMEOUT_HARD_PROTODRV_PROCESSING	(1000 * 1000000L)	/* REQUESTED to PROCESSING */

/*
 * =================================================================================================
 * CONFIG - GLOBAL OPTIONS
 * =================================================================================================
 */


#define NPU_MAX_BUFFER			16
#define NPU_MAX_PLANE			3

#define NPU_MAX_GRAPH			32
#define NPU_MAX_FRAME			NPU_MAX_BUFFER

/* Device identification */
#define NPU_MINOR			10
#define NPU_VERTEX_NAME			"npu"
/* Number of message id of mailbox -> Valid message ID will be 0 ~ MAX_MSG_ID_CNT-1 */
#define NPU_MAX_MSG_ID_CNT		32

/* Queue between Session manager and Network/Frame manager */
/* Maximum number of elements can be hold by frame request queue */
#define BUFFER_Q_LIST_SIZE		(1 << 10)

/* Maximum number of elements can be hold by network management request queue */
#define NCP_MGMT_LIST_SIZE		(1 << 8)

/* Interface between Network/Frame anager and npu-interface */
/* Use same size for request and response */
#define NW_Q_SIZE			(1 << 10)
#define FRAME_Q_SIZE			(1 << 10)

/* Log Keeper settings */
/* Size of buffer for firmware log keeper */
#define NPU_FW_LOG_KEEP_SIZE		(4096 * 1024)

/* npu_queue and vb_queue constants */
/* Maximum number of index allowed for mapping */
#define NPU_MAX_BUFFER			16

#define NPU_MAILBOX_DEFAULT_TID         0

/* NCP version used for this SoC */
#define NCP_VERSION			21

/* Mailbox version used for this SoC */
#define MAILBOX_VERSION			7

#ifdef CONFIG_NPU_HARDWARE
/* ioremap area definition of NPU IP */
/* TODO: Those constants shall be read from DT later */

#define NPU_IOMEM_TCUSRAM_START         0x16700000
#define NPU_IOMEM_TCUSRAM_END           0x16720000
#define NPU_IOMEM_IDPSRAM_START         0x16100000
#define NPU_IOMEM_IDPSRAM_END           0x16200000
#define NPU_IOMEM_SFR_DNC_START         0x16200000 //CMU_DNC
#define NPU_IOMEM_SFR_DNC_END           0x16300000
#define NPU_IOMEM_SFR_NPU0_START        0x10A00000 //CMU_NPU0
#define NPU_IOMEM_SFR_NPU0_END          0x10B00000
#define NPU_IOMEM_SFR_NPU1_START        0x10B00000 //CMU_NPU1
#define NPU_IOMEM_SFR_NPU1_END          0x10C00000

#define NPU_IOMEM_PMU_NPU_START         0x10E62480
#define NPU_IOMEM_PMU_NPU_END           0x10E62580
//added newly;;need to add in new structure
#define NPU_IOMEM_PMU_DNC_START         0x10E62580
#define NPU_IOMEM_PMU_DNC_END           0x10E62600

#define NPU_IOMEM_PMU_NPU_CPU_START     0x10E62f80
#define NPU_IOMEM_PMU_NPU_CPU_END       0x10E63000

#define NPU_IOMEM_BAAW_NPU_START        0x11DA0000
#define NPU_IOMEM_BAAW_NPU_END          0x1A350040

#define NPU_IOMEM_MBOX_SFR_START        0x165C0000
#define NPU_IOMEM_MBOX_SFR_END          0x165C017C

#define NPU_IOMEM_PWM_START             0x1640C000
#define NPU_IOMEM_PWM_END               0x1641C000

//doubtful; check again
#define NPU_SFR_DSPC_START              0x16420010
#define NPU_SFR_DSPC_END                0x16440C40

#define DNC_SYSREG_START                0x16220000
#define DNC_SYSREG_END                  0x16220500

#define NPU_FW_DRAM_SIZE		0x00080000
#define NPU_FW_DRAM_DADDR		0x50000000

#define NPU_FW_LOG_SIZE			0x200000
#define NPU_FW_LOG_DADDR		0x50300000

#define NPU_FW_EARLY_BOOT_SIZE	0x00020000
#define NPU_FW_EARLY_BOOT_DADDR	0x500E0000

#define NPU_FW_UNIT_TEST_BUF_SIZE	0x200000
#define NPU_FW_UNIT_TEST_BUF_DADDR	0x50100000

#endif

// #define NPU_CM7_RELEASE_HACK

/* Offset of PWM conunter used for profiling */
#define TCNTO0_OFF                      0x0014
/* Definition for ZEBU */
#ifdef CONFIG_NPU_HARDWARE
//#define FORCE_HWACG_DISABLE
/* #define FORCE_WDT_DISABLE */
//#define ENABLE_PWR_ON
#define CA5_SECURE_RELEASE


#ifdef CONFIG_NPU_ZEBU_EMULATION
#define NPU_CM7_RELEASE_HACK
#define T32_GROUP_TRACE_SUPPORT
#endif  /* CONFIG_NPU_ZEBU_EMULATION */
#endif

#ifdef CONFIG_NPU_HARDWARE
/* Clear TCU/IDP SRAM area before firmware loading */
//#define CLEAR_SRAM_ON_FIRMWARE_LOADING

/* Start address of firmware address space */
#define NPU_FW_BASE_ADDR                0
#endif

/* Skip sending PURGE command(Trigerred by STREAM_OFF) to NPU H/W */

/*
 * Mailbox size configuration
 * Please refer the mailbox.h for detail
 */
#define K_SIZE	(1024)
#define NPU_MAILBOX_HDR_SECTION_LEN	(4 * K_SIZE)
#define NPU_MAILBOX_SIZE		(32 * K_SIZE)
#define NPU_MAILBOX_BASE		0x80000

/*
 * Delay insearted before streamoff and power down sequence on EMERGENCY.
 * It is required to give the H/W finish its DMA transfer.
 */
#define POWER_DOWN_DELAY_ON_EMERGENCY	(300u)

/* Stream off delay should be longer than timeout of firmware response */
#define STREAMOFF_DELAY_ON_EMERGENCY	(12000u)

#endif /* _NPU_CONFIG_H_ */
