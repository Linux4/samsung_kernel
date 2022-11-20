/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * KQ(Kernel Quality) NAD driver implementation
 *	: SeungYoung Lee <seung0.lee@samsung.com>
 */

#ifndef __KQ_NAD_EXYNOS2100_H__
#define __KQ_NAD_EXYNOS2100_H__

#define KQ_NAD_SUPPORT_MAX_BLOCK	64
#define KQ_NAD_SUPPORT_MAX_NAME		16
#define KQ_NAD_MAX_BLOCK_NAME		8
#define KQ_NAD_MAX_DAS_NAME		8

#define KQ_NAD_STR_NAD_PASS "OK_3.1_L"
#define KQ_NAD_STR_NAD_FAIL "NG_3.1_L"
#define KQ_NAD_STR_ACAT_PASS "OK_3.0_L"
#define KQ_NAD_STR_ACAT_FAIL "NG_3.0_L"
#define KQ_NAD_STR_NADX_PASS "OK_3.2_L"
#define KQ_NAD_STR_NADX_FAIL "NG_3.2_L"

#define KQ_NAD_INFORM4_MAGIC 0x1586084c

enum {
	KQ_NAD_BLOCK_START = 0,
	KQ_NAD_BLOCK_DRAM = KQ_NAD_BLOCK_START,
	KQ_NAD_BLOCK_BIG,
	KQ_NAD_BLOCK_MIDD,
	KQ_NAD_BLOCK_LITT,
	KQ_NAD_BLOCK_MIF,
	KQ_NAD_BLOCK_G3D,
	KQ_NAD_BLOCK_INT,
	KQ_NAD_BLOCK_CAM,
	KQ_NAD_BLOCK_FUNC,
	KQ_NAD_BLOCK_CP,
	KQ_NAD_BLOCK_DSU,
	KQ_NAD_BLOCK_NPU,
	KQ_NAD_BLOCK_NONE,

	KQ_NAD_BLOCK_END,
};

static char kq_nad_block_name[KQ_NAD_BLOCK_END][KQ_NAD_MAX_BLOCK_NAME] = {
	"DRAM", "BIG", "MIDD", "LITT", "MIF", "G3D", "INT", "CAM", "FUNC", "CP", "DSU", "NPU", "NONE" };

enum {
	KQ_NAD_DAS_START = 0,
	KQ_NAD_DAS_NONE = KQ_NAD_DAS_START,
	KQ_NAD_DAS_DRAM,
	KQ_NAD_DAS_AP,
	KQ_NAD_DAS_SET,
	KQ_NAD_DAS_CP,

	KQ_NAD_DAS_END,
};

static char kq_nad_das_name[KQ_NAD_DAS_END][KQ_NAD_MAX_DAS_NAME] = {
	"NONE", "DRAM", "AP", "SET", "CP" };

struct kq_nad_block {
	char block[KQ_NAD_SUPPORT_MAX_BLOCK][KQ_NAD_SUPPORT_MAX_NAME];
};

static struct kq_nad_block kq_nad_block_data[] = {
	//DRAM
	{"NONE",        "PATTERN1",     "PATTERN2",   "SELF_WRITE", "SELF_READ",
	"NONE",         "NONE",         "NONE",       "NONE",       "NONE",
	"WRITE",        "READ",         "SMALL_EYE",  "MANUAL",     "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",       "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",       "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",       "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",       "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",       "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",       "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",       "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",       "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",       "NONE",
	"NONE",         "NONE",         "NONE",       "NONE"},
	//BIG
	{"NONE",        "UNZIP",        "C2",         "CACHE",       "Dijkstra",
	"CRYPTO",       "SHA",          "FFT_NEON",   "AES",         "MEMTEST-C",
	"DVFS_MIF",     "UNZIP_DVFS",   "C2_DVFS",    "CACHE_DVFS",  "DIJKSTRA_DVFS",
	"CPU_DVFS",     "MEMTEST_1",    "MEMTEST_B",  "MEMTEST_OPT", "CACHE_L3",
	"ECC_OVERFLOW", "CRYPTO_OCTA",  "VST_UNZIP",  "VST_C2",      "VST_DIJKSTRA",
	"VST_CRYPTO",   "VST_FFT_NEON", "NONE",       "NONE",        "MEMTEST_L2_HF",
	"LZMA",         "BIG_ECC",      "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "HIGH_UNZIP",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE"},
	//MID
	{"NONE",        "UNZIP",        "C2",         "CACHE",       "Dijkstra",
	"CRYPTO",       "SHA",          "FFT_NEON",   "AES",         "MEMTEST-C",
	"DVFS_MIF",     "UNZIP_DVFS",   "C2_DVFS",    "CACHE_DVFS",  "DIJKSTRA_DVFS",
	"CPU_DVFS",     "MEMTEST_1",    "MEMTEST_B",  "MEMTEST_OPT", "CACHE_L3",
	"ECC_OVERFLOW", "CRYPTO_OCTA",  "VST_UNZIP",  "VST_C2",      "VST_DIJKSTRA",
	"VST_CRYPTO",   "VST_FFT_NEON", "NONE",       "NONE",        "MEMTEST_L2_HF",
	"LZMA",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "HIGH_UNZIP",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE"},
	//LITT
	{"NONE",        "UNZIP",        "C2",         "CACHE",       "Dijkstra",
	"CRYPTO",       "SHA",          "FFT_NEON",   "AES",         "MEMTEST-C",
	"DVFS_MIF",     "UNZIP_DVFS",   "C2_DVFS",    "CACHE_DVFS",  "DIJKSTRA_DVFS",
	"CPU_DVFS",     "MEMTEST_1",    "MEMTEST_B",  "MEMTEST_OPT", "CACHE_L3",
	"ECC_OVERFLOW", "CRYPTO_OCTA",  "VST_UNZIP",  "VST_C2",      "VST_DIJKSTRA",
	"VST_CRYPTO",   "VST_FFT_NEON", "NONE",       "NONE",        "MEMTEST_L2_HF",
	"LZMA",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "HIGH_UNZIP",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE"},
	//MIF
	{"NONE",    "MEMTESTER",  "VWM",           "SFR",        "RANDOM_DVFS",
	"MIF_DVFS", "PART_DVFS",  "MIF_DVFS16",    "MIF_DVFS37", "MEMTESTER_MODE4",
	"VREF",     "SED",        "NONE",          "NONE",       "NONE",
	"NONE",     "NONE",       "VST_MEMTESTER", "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",          "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",          "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",          "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",          "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",          "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",          "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",          "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",          "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",          "NONE"},
	//G3D
	{"NONE",      "mTREX",      "mMANHATTAN", "mCARCHASE",  "mHONORKHIGH",
	"mHONORKLOW", "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",       "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",       "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",       "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",       "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",       "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",       "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",       "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",       "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",       "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",       "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",       "NONE",       "NONE",       "NONE"},
	//INT
	{"NONE",    "SSS",        "JPEG",       "MSH",        "USB",
	"G2D",      "G3D",        "MFC0",       "MFC1",       "SLEEP_WAKEUP",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE"},
	//CAM
	{"NONE",     "ABOX",       "DISP",       "IVA",        "MFC",
	"MFC_CACHE", "DPU_HDR",    "NONE",       "NONE",       "NONE",
	"NONE",      "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",      "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",      "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",      "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",      "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",      "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",      "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",      "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",      "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",      "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",      "NONE",       "NONE",       "NONE"},
	//FUNC
	{"NONE",    "LOADING",    "G3D_UNZIP",  "OTP",        "MCT",
	"ADC",      "UFS_MAIN",   "NONE",       "NPU",        "UFS_LB",
	"USB_LB",   "PCIE_LB",    "UFS_CSRS",   "DD_CHECK",   "ON_CHIP",
	"SSP",      "SRAM",       "MCSC",       "G3D_PMU",    "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE"},
	//CP
	{"NONE",      "CP_BOOT",     "DVFS_CPSYS",  "UCPU_UNZIP", "LCPU_UNZIP",
	"LCPU1600",   "DVFS_CPSYS",  "UCPU_UNZIP",  "LCPU_UNZIP", "DVFS_CPSYS",
	"UCPU_UNZIP", "LCPU_UNZIP",  "DVFS_CPSYS",  "UCPU_UNZIP", "LCPU_UNZIP",
	"DVFS_CPSYS", "NONE",        "NONE",        "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",        "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",        "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",        "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",        "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",        "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",        "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",        "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",        "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",        "NONE"},
	//DSU
	{"NONE",        "UNZIP",        "C2",             "CACHE",            "Dijkstra",
	"CRYPTO",       "SHA",          "FFT_NEON",       "AES",              "MEMTEST-C",
	"DVFS_MIF",     "UNZIP_DVFS",   "C2_DVFS",        "CACHE_DVFS",       "DIJKSTRA_DVFS",
	"CPU_DVFS",     "CPU_MEMTEST",  "UNZIP_OCTA",     "C2_OCTA",          "L3",
	"ECC_OVERFLOW", "CRYPTO_OCTA",  "VST_UNZIP",      "VST_C2",           "VST_DIJKSTRA",
	"VST_CRYPTO",   "VST_FFT_NEON", "NONE",           "NONE",             "MEMTEST_L2_HF",
	"LZMA",         "BIG_ECC",      "HIGH_UNZIP",     "SINGLE_UNZIP",     "SPEED_UNZIP",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "HIGH_UNZIP",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE"},
	//NPU
	{"NONE",        "IV3",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE",             "NONE",
	"NONE",         "NONE",         "NONE",           "NONE"},
};

#if IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
#define KQ_NAD_CORRELATION_MPARAM_MAX_LEN (32 * 2 + 1)
#define KQ_NAD_CORRELATION_VALID_MAGIC 0x1234ABCD

/* CL0, CL1, CL2, MIF, DSU, CP, G3D, SCI */
enum {
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CL0,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CL1,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CL2,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_MIF,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_DSU,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CP,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_G3D,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_SCI,

	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_END,
};
#endif //IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)

#if IS_ENABLED(CONFIG_SEC_KQ_BPS_RESULT)
#define KQ_NAD_BPS_MPARAM_MAX_DELIMITER	12
#define KQ_NAD_BPS_MPARAM_MAX_LEN (11 * (KQ_NAD_BPS_MPARAM_MAX_DELIMITER + 1) + KQ_NAD_BPS_MPARAM_MAX_DELIMITER)
#endif //IS_ENABLED(CONFIG_SEC_KQ_BPS_RESULT)

#endif /* __KQ_NAD_EXYNOS2100_H__ */
