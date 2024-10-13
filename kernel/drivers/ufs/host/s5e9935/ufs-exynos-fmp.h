#ifndef _UFS_EXYNOS_FMP_H_
#define _UFS_EXYNOS_FMP_H_

#define FMP_DRV_VERSION "6.0.0"

/* FMP IDs (second or third parameter to FMP/SMU Ctrls) */
#define FMP_EMBEDDED			0
#define FMP_UFSCARD			1
#define FMP_SDCARD			2

/* SMU commands (second parameter to SMC_CMD_SMU) */
#define SMU_INIT			0
#define SMU_SET				1
#define SMU_ABORT			2

/* Fourth parameter to SMC_CMD_FMP */
#define HW_KEYS_IN_KEYSLOT		0
#define KEYS_IN_KEYSLOT			1
#define KEYS_IN_PRDT			2

#ifdef CONFIG_HW_KEYS_IN_CUSTOM_KEYSLOT
#define FMP_MODE			HW_KEYS_IN_KEYSLOT
#endif
#ifdef CONFIG_KEYS_IN_CUSTOM_KEYSLOT
#define FMP_MODE			KEYS_IN_KEYSLOT
#endif
#ifdef CONFIG_KEYS_IN_PRDT
#define FMP_MODE			KEYS_IN_PRDT
#endif

/* FMP selftest status */
#define FMP_SELF_TEST_DONE		(1 << 0)
#define FMP_ST_READ_ABORT		(1 << 16)
#define FMP_ST_WRITE_ABORT		(1 << 17)
#define FMP_SELF_TEST_FAIL		(FMP_ST_READ_ABORT | FMP_ST_WRITE_ABORT)

enum fmp_crypto_algo_mode {
	FMP_BYPASS_MODE = 0,
	FMP_ALGO_MODE_AES_CBC = 1,
	FMP_ALGO_MODE_AES_XTS = 2,
};

#define FMP_DATA_UNIT_SIZE 4096

#define AES_256_XTS_KEY_SIZE		64
#define AES_GCM_TAG_SIZE		16
#define AES_256_XTS_TWK_OFFSET		8
#define HW_WRAPPED_KEY_TAG_OFFSET	16
#define HW_WRAPPED_KEY_SECRET_OFFSET	80

#ifdef CONFIG_KEYS_IN_PRDT

struct fmp_sg_entry {
	/* The first four fields correspond to those of ufshcd_sg_entry. */
	__le32 des0;
	__le32 des1;
	__le32 des2;
	/*
	 * The algorithm and key length are configured in the high bits of des3,
	 * whose low bits already contain ufshcd_sg_entry::size.
	 */
	__le32 des3;
#define FKL			(1 << 26)
#define SET_KEYLEN(ent, v)	((ent)->des3 |= cpu_to_le32(v))
#define SET_FAS(ent, v)		((ent)->des3 |= cpu_to_le32((v) << 28))

	/* The IV with all bytes reversed */
	__be32 file_iv[4];

	/*
	 * The key with all bytes reversed.  For XTS, the two halves of the key
	 * are given separately and are byte-reversed separately.
	 */
	__be32 file_enckey[8];
	__be32 file_twkey[8];

	/* Not used */
	__be32 disk_iv[4];
	__le32 reserved[4];
};
#endif

/* CRYPTOCFG - CFGE_CAPIDX_DUSIZE */
union exynos_ufs_crypto_cfg_entry {
	__le32 reg_val;
	struct {
		u8 data_unit_size;
		u8 crypto_cap_idx;
		u8 reserved_1;
		u8 config_enable;
	};
};

/* FMP register map  */
#define FMP_KW_SECUREKEY	0x8000
#define FMP_KW_TAG		0x8040
#define FMP_KW_CONTROL		0xA008
#define FMP_KW_KEYVALID		0xA00C
#define FMP_KW_TAGABORT		0xA010
#define FMP_KW_PBKABORT		0xA014
#define FMP_SELFTESTSTAT0	0x84
#define FMP_SELFTESTSTAT1	FMP_SELFTESTSTAT0

#define FMP_KW_SECUREKEY_OFFSET	0x80
#define FMP_KW_TAG_OFFSET	0x80

/* UFSHCD register map  */
#define CRYPTOCFG		0x2040
#define CRYPTOCFG_OFFSET	0x80

#endif /* _UFS_EXYNOS_FMP_H_ */
