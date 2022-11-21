/*
 * include/linux/sec_smem.h
 *
 * COPYRIGHT(C) 2016-2017 Samsung Electronics Co., Ltd. All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SEC_SMEM_H_
#define _SEC_SMEM_H_

typedef struct {
	uint64_t magic;
	uint64_t version;
} sec_smem_header_t;

/* For SMEM_ID_VENDOR0 */
#define SMEM_VEN0_MAGIC 0x304E4556304E4556
#define SMEM_VEN0_VER 2

typedef struct{
    uint64_t afe_rx_good;
    uint64_t afe_rx_mute;
    uint64_t afe_tx_good;
    uint64_t afe_tx_mute;
} smem_afe_log_t;

typedef struct{
    uint64_t reserved[5];
} smem_afe_ext_t;

typedef struct {
	uint32_t ddr_vendor;
	uint32_t reserved;
	smem_afe_log_t afe_log;
} sec_smem_id_vendor0_v1_t;

typedef struct {
	sec_smem_header_t header;
	uint32_t ddr_vendor;
	uint32_t reserved;
	smem_afe_log_t afe_log;
	smem_afe_ext_t afe_ext;
} sec_smem_id_vendor0_v2_t;

/* For SMEM_ID_VENDOR1 */
#define SMEM_VEN1_MAGIC 0x314E4556314E4556
#define SMEM_VEN1_VER 4

typedef struct {
	uint64_t hw_rev;
} sec_smem_id_vendor1_v1_t;

typedef struct {
	uint64_t hw_rev;
	uint64_t ap_suspended;
} sec_smem_id_vendor1_v2_t;

#define NUM_CH 2
#define NUM_CS 2
#define NUM_DQ_PCH 4

typedef struct {
	uint8_t tDQSCK[NUM_CH][NUM_CS][NUM_DQ_PCH];
} ddr_rcw_t;

typedef struct {
	uint8_t coarse_cdc[NUM_CH][NUM_CS][NUM_DQ_PCH];
	uint8_t fine_cdc[NUM_CH][NUM_CS][NUM_DQ_PCH];
} ddr_wr_dqdqs_t;

typedef struct {
	uint32_t version;
	ddr_rcw_t rcw;
	ddr_wr_dqdqs_t wr_dqdqs;
} ddr_train_t;

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

typedef struct {
	void *clk __attribute__((aligned(8)));
	void *apc_cpr __attribute__((aligned(8)));
} smem_apps_stat_t;

typedef struct {
	void *vreg __attribute__((aligned(8)));
} smem_vreg_stat_t;

typedef struct {
	sec_smem_header_t header;
	sec_smem_id_vendor1_v2_t ven1_v2;
	smem_ddr_stat_t ddr_stat;
	void * ap_health __attribute__((aligned(8)));
	smem_apps_stat_t apps_stat;
	smem_vreg_stat_t vreg_stat;
	ddr_train_t ddr_training;
} sec_smem_id_vendor1_v4_t;

#ifdef CONFIG_SEC_BSP
extern char* get_ddr_vendor_name(void);
extern uint32_t get_ddr_DSF_version(void);
extern uint8_t get_ddr_rcw_tDQSCK(uint32_t ch, uint32_t cs, uint32_t dq);
extern uint8_t get_ddr_wr_coarseCDC(uint32_t ch, uint32_t cs, uint32_t dq);
extern uint8_t get_ddr_wr_fineCDC(uint32_t ch, uint32_t cs, uint32_t dq);
#endif

#endif // _SEC_SMEM_H_
