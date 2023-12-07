#ifndef _SEC_SMEM_H_
#define _SEC_SMEM_H_

typedef struct {
	uint64_t magic;
	uint64_t version;
} sec_smem_header_t;

/* For SMEM_ID_VENDOR0 */
/**
 * struct smem_sec_hw_info_type - for SMEM_ID_VENDOR0
 * @afe_rx/tx_good :	Request from CP system team
 * @afe_rx/tx_mute :	Request from CP system team
 */
struct smem_afe_log_type {
    uint64_t afe_rx_good;
    uint64_t afe_rx_mute;
    uint64_t afe_tx_good;
    uint64_t afe_tx_mute;
};

struct sec_smem_id_vendor0_type {
	uint32_t ddr_vendor;
	uint32_t reserved;
	struct smem_afe_log_type afe_log;
};


/* For SMEM_ID_VENDOR1 */
#define SMEM_VEN1_MAGIC 0x314E4556314E4556
#define SMEM_VEN1_VER 3

typedef struct {
	uint64_t hw_rev;
} sec_smem_id_vendor1_v1_t;

typedef struct {
	uint64_t hw_rev;
	uint64_t ap_suspended;
} sec_smem_id_vendor1_v2_t;

typedef struct {
	uint64_t num;
	void * nIndex __attribute__((aligned(8)));
	void * DDRLogs __attribute__((aligned(8)));
	void * DDR_STRUCT __attribute__((aligned(8)));
} smem_ddr_stat_t;

typedef struct {
	sec_smem_header_t header;
	sec_smem_id_vendor1_v2_t ven1_v2;
	smem_ddr_stat_t ddr_stat;
} sec_smem_id_vendor1_v3_t;

#endif // _SEC_SMEM_H_
