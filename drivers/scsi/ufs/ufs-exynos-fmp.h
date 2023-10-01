#ifndef _UFS_EXYNOS_FMP_H_
#define _UFS_EXYNOS_FMP_H_

#define FMP_DRV_VERSION "5.0.0"

/* FMP IDs (second or third parameter to FMP/SMU Ctrls) */
#define FMP_EMBEDDED			0
#define FMP_UFSCARD			1
#define FMP_SDCARD			2

/* SMU commands (second parameter to SMC_CMD_SMU) */
#define SMU_INIT			0
#define SMU_SET				1
#define SMU_ABORT			2

/* Fourth parameter to SMC_CMD_FMP_SECURITY */
#define CFG_DESCTYPE_0			0
#define CFG_DESCTYPE_3			3

/* Fourth parameter to SMC_CMD_FMP_KW_MODE */
#define WAP0_NS				(1 << 8)
#define UNWRAP_EN			(0 << 4)
#define UNWRAP_BYPASS			(1 << 4)
#define SWKEY_MODE			0
#define SECUREKEY_MODE			1

/* Fourth parameter to SMC_CMD_FMP_KW_INDATASWAP */
#define RAW_FILEKEY_WORDSWAP            (1 << 22)
#define RAW_TWEAKKEY_WORDSWAP           (1 << 21)
#define PBK_WORDSWAP                    (1 << 20)
#define IV_WORDSWAP                     (1 << 19)
#define TAG_WORDSWAP                    (1 << 18)
#define SECURE_FILEKEY_WORDSWAP         (1 << 17)
#define SECURE_TWEAKKEY_WORDSWAP        (1 << 16)
#define RAW_FILEKEY_BYTESWAP            (1 << 6)
#define RAW_TWEAKKEY_BYTESWAP           (1 << 5)
#define PBK_BYTESWAP                    (1 << 4)
#define IV_BYTESWAP                     (1 << 3)
#define TAG_BYTESWAP                    (1 << 2)
#define SECURE_FILEKEY_BYTESWAP         (1 << 1)
#define SECURE_TWEAKKEY_BYTESWAP        (1 << 0)
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

/* Number of keyslots */
#define NUM_KEYSLOTS	16

#define AES_256_XTS_KEY_SIZE		64
#define SECRET_SIZE			32
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

struct exynos_fmp {
	u8 cfge_en;
};

/* FMP register map  */
#define FMP_KW_SECUREKEY	0x8000
#define FMP_KW_TAG		0x8040
#define FMP_KW_CONTROL		0xA008
#define FMP_KW_KEYVALID		0xA00C
#define FMP_KW_TAGABORT		0xA010
#define FMP_KW_PBKABORT		0xA014
#define FMP_SELFTESTSTAT	0x84

#define FMP_KW_SECUREKEY_OFFSET	0x80
#define FMP_KW_TAG_OFFSET	0x80

/* UFSHCD register map  */
#define HCI_VS_BASE_GAP	0x1100
#define CRYPTOCFG		0x2040
#define CRYPTOCFG_OFFSET	0x80
#define CRYPTOCFG_GFGE_EN	(1 << 31)


#endif /* _UFS_EXYNOS_FMP_H_ */
