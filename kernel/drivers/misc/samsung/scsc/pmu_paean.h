#ifndef PMU_PRIVATE_H
#define PMU_PRIVATE_H

#ifdef FEEDIRAM_MODE
#define PMU_RAMRP_BASE (0x0000000c)
#else
#define PMU_RAMRP_BASE (0x00000000)
#endif

#define RAMRP_WLAN_SCOREBOARD (256)
/* Mailbox buffers between host and device for request/response
 * exchange.
 */
typedef struct {
	uint32_t from_host;
	uint32_t to_host;
} __attribute__((packed, aligned(4))) ramrp_mbox_t;

typedef struct {
	uint64_t start_addr_0;
	uint64_t end_addr_0;
	uint64_t start_addr_1;
	uint64_t end_addr_1;
} pcie_atu_config_t;

typedef struct
{
	pcie_atu_config_t pcie_config;
	ramrp_mbox_t pmu_mbox;
	ramrp_mbox_t pcie_mbox;
	ramrp_mbox_t error_mbox;
	uint32_t wlan_scoreboard[RAMRP_WLAN_SCOREBOARD];
} __attribute__ ((packed, aligned(4))) ramrp_hostif_t;

#define RAMRP_HOSTIF_SIZE (sizeof(ramrp_hostif_t))
#define RAMRP_HOSTIF_OFFSET(ofs)                                               \
	((PMU_RAMRP_BASE + (offsetof(ramrp_hostif_t, ofs))))

#define RAMRP_HOSTIF_PMU_PCIE_CONFIG_PTR                                       \
	RAMRP_HOSTIF_OFFSET(pcie_config)

#define RAMRP_HOSTIF_PMU_PCIE_CONFIG_SCOREBOARD                                \
	RAMRP_HOSTIF_OFFSET(wlan_scoreboard[0])

#define RAMRP_HOSTIF_PMU_MBOX_FROM_HOST_PTR                                    \
	RAMRP_HOSTIF_OFFSET(pmu_mbox.from_host)
#define RAMRP_HOSTIF_PCIE_MBOX_FROM_HOST_PTR                                   \
	RAMRP_HOSTIF_OFFSET(pcie_mbox.from_host)
#define RAMRP_HOSTIF_ERROR_MBOX_FROM_HOST_PTR                                   \
	RAMRP_HOSTIF_OFFSET(error_mbox.from_host)

#define RAMRP_HOSTIF_PMU_MBOX_TO_HOST_PTR                                      \
	RAMRP_HOSTIF_OFFSET(pmu_mbox.to_host)
#define RAMRP_HOSTIF_PCIE_MBOX_TO_HOST_PTR                                     \
	RAMRP_HOSTIF_OFFSET(pcie_mbox.to_host)
#define RAMRP_HOSTIF_ERROR_MBOX_TO_HOST_PTR                                     \
	RAMRP_HOSTIF_OFFSET(error_mbox.to_host)

/* 32 different interrupts can be supported
 * from host to PMU.
 *
 * Add any new PMU interrupts from host here.
 */
typedef enum {
	PMU_INT_FROM_HOST_SS_BOOT_START_MASK	= 0x00000001,
	PMU_INT_FROM_HOST_PMU_MBOX_REQ_MASK	= 0x00000002,
	PMU_INT_FROM_HOST_WAKE_WLAN_MASK	= 0x00000004,
	PMU_INT_FROM_HOST_WAKE_WPAN_MASK	= 0x00000008,
	PMU_INT_FROM_HOST_PCIE_MBOX_REQ_MASK	= 0x00000010,
	PMU_INT_FROM_HOST_MAX_MASK
} pmu_int_from_host_mask_t;

typedef enum
{
    PMU_MBOX_PMU   = 29,
    PMU_MBOX_PCIE  = 30,
    PMU_MBOX_ERROR = 31
} pmu_mbox_t;

typedef enum
{
    PMU_INT_TO_HOST_PMU_MBOX_RESP_MASK   = (0x1 << PMU_MBOX_PMU),
    PMU_INT_TO_HOST_PCIE_MBOX_RESP_MASK  = (0x1 << PMU_MBOX_PCIE),
    PMU_INT_TO_HOST_ERROR_MBOX_RESP_MASK = (0x1 << PMU_MBOX_ERROR)
} pmu_int_to_host_mask_t;

#define PMU_INT_TO_HOST_MASK(mbox) (0x1 << (mbox))
#define RAMRP_HOSTIF_MBOX_TO_HOST_PTR(mbox) (((mbox) == PMU_MBOX_PMU) ? RAMRP_HOSTIF_PMU_MBOX_TO_HOST_PTR : (((mbox) == PMU_MBOX_PCIE) ? RAMRP_HOSTIF_PCIE_MBOX_TO_HOST_PTR : RAMRP_HOSTIF_ERROR_MBOX_TO_HOST_PTR))
#define RAMRP_HOSTIF_MBOX_FROM_HOST_PTR(mbox) (((mbox) == PMU_MBOX_PMU) ? RAMRP_HOSTIF_PMU_MBOX_FROM_HOST_PTR : (((mbox) == PMU_MBOX_PCIE) ? RAMRP_HOSTIF_PCIE_MBOX_FROM_HOST_PTR : RAMRP_HOSTIF_ERROR_MBOX_FROM_HOST_PTR))

/* Second Stage boot status sent back to PMU
 * via rhif.pmu_mbox.to_host
 */
typedef enum {
	PMU_SS_BOOT_STATUS_SUCCESS = 0x0,
	PMU_SS_BOOT_STATUS_FAILURE
} pmu_ss_boot_status_t;

#endif /* PMU_PRIVATE_H */
