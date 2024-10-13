/*
 *  Copyright (C) 2018 Samsung Electronics Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SDP_CRYPTO_H_
#define SDP_CRYPTO_H_

#include <crypto/aead.h>
#include <linux/crypto.h>
#include <linux/init.h>
#include "../fscrypt_private.h"
#include "../fscrypt_knox_private.h"
#include <linux/fscrypto_sdp_cache.h>
#include "fscrypto_sdp_xattr_private.h"
#include <sdp/dek_common.h>

#define ROUND_UPX(i, x) (((i)+((x)-1))&~((x)-1))
#define SDP_CRYPTO_RNG_SEED_SIZE 16

/* Definitions for AEAD */
#define AEAD_IV_LEN 12
#define AEAD_AAD_LEN 16
#define AEAD_AUTH_LEN 16
#define AEAD_D32_PACK_DATA_LEN 32
#define AEAD_D64_PACK_DATA_LEN 64
#define AEAD_D32_PACK_TOTAL_LEN (AEAD_IV_LEN + AEAD_D32_PACK_DATA_LEN + AEAD_AUTH_LEN)
#define AEAD_D64_PACK_TOTAL_LEN (AEAD_IV_LEN + AEAD_D64_PACK_DATA_LEN + AEAD_AUTH_LEN)
#define AEAD_DATA_PACK_MAX_LEN AEAD_D64_PACK_TOTAL_LEN

struct __aead_data_32_pack {
	unsigned char iv[AEAD_IV_LEN];
	unsigned char data[AEAD_D32_PACK_DATA_LEN];
	unsigned char auth[AEAD_AUTH_LEN];
};

struct __aead_data_64_pack {
	unsigned char iv[AEAD_IV_LEN];
	unsigned char data[AEAD_D64_PACK_DATA_LEN];
	unsigned char auth[AEAD_AUTH_LEN];
};

/* Default Definitions for AES-GCM crypto */
typedef struct __aead_data_32_pack gcm_pack32;
typedef struct __aead_data_64_pack gcm_pack64;
typedef struct __gcm_pack {
	u32 type;
	u8 *iv;
	u8 *data;
	u8 *auth;
} gcm_pack;

#define SDP_CRYPTO_GCM_PACK32 0x01
#define SDP_CRYPTO_GCM_PACK64 0x02
#define CONV_TYPE_TO_DLEN(x) (x == SDP_CRYPTO_GCM_PACK32 ? \
		AEAD_D32_PACK_DATA_LEN : x == SDP_CRYPTO_GCM_PACK64 ? \
		AEAD_D64_PACK_DATA_LEN : 0)
#define CONV_TYPE_TO_PLEN(x) (x == SDP_CRYPTO_GCM_PACK32 ? \
		AEAD_D32_PACK_TOTAL_LEN : x == SDP_CRYPTO_GCM_PACK64 ? \
		AEAD_D64_PACK_TOTAL_LEN : 0)
#define CONV_DLEN_TO_TYPE(x) (x == AEAD_D32_PACK_DATA_LEN ? \
		SDP_CRYPTO_GCM_PACK32 : x == AEAD_D64_PACK_DATA_LEN ? \
		SDP_CRYPTO_GCM_PACK64 : 0)
#define CONV_PLEN_TO_TYPE(x) (x == AEAD_D32_PACK_TOTAL_LEN ? \
		SDP_CRYPTO_GCM_PACK32 : x == AEAD_D64_PACK_TOTAL_LEN ? \
		SDP_CRYPTO_GCM_PACK64 : 0)
#define SDP_CRYPTO_GCM_MAX_PLEN AEAD_DATA_PACK_MAX_LEN

#define SDP_CRYPTO_GCM_IV_LEN AEAD_IV_LEN
#define SDP_CRYPTO_GCM_AAD_LEN AEAD_AAD_LEN
#define SDP_CRYPTO_GCM_AUTH_LEN AEAD_AUTH_LEN
#define SDP_CRYPTO_GCM_DATA_LEN AEAD_D64_PACK_DATA_LEN
#define SDP_CRYPTO_GCM_DEFAULT_AAD "PROTECTED_BY_SDP" // Explicitly 16 bytes following SDP_CRYPTO_GCM_AAD_LEN
#define SDP_CRYPTO_GCM_DEFAULT_KEY_LEN 32

/* Definitions for Nonce */
#define MAX_EN_BUF_LEN AEAD_D32_PACK_TOTAL_LEN
#define SDP_CRYPTO_NEK_LEN SDP_CRYPTO_GCM_DEFAULT_KEY_LEN
#define SDP_CRYPTO_NEK_DRV_LABEL "NONCE_ENC_KEY"
#define SDP_CRYPTO_NEK_DRV_CONTEXT "NONCE_FOR_FEK"

#define SDP_CRYPTO_SHA512_OUTPUT_SIZE 64

struct sdp_info {
	u32 sdp_flags;
	u32 engine_id;
	dek_t sdp_dek;
	u8 sdp_en_buf[MAX_EN_BUF_LEN];
	spinlock_t sdp_flag_lock;
};

// Essential material for set_sensitive operation
typedef struct _sdp_ess_material {
	struct inode *inode;
	struct fscrypt_key key;
} sdp_ess_material;

#define SDP_DEK_SDP_ENABLED             0x00100000
#define SDP_DEK_IS_SENSITIVE            0x00200000
#define SDP_DEK_IS_UNINITIALIZED        0x00400000
//#define SDP_DEK_MULTI_ENGINE            0x00400000
#define SDP_DEK_TO_SET_SENSITIVE        0x00800000
#define SDP_DEK_TO_CONVERT_KEY_TYPE     0x01000000
//#define SDP_DEK_DECRYPTED_FEK_SET       0x02000000
//#define SDP_DEK_IS_EMPTY_CTFM_SET       0x04000000
//#define SDP_DEK_TO_CLEAR_NONCE          0x08000000
//#define SDP_DEK_TO_CLEAR_CACHE          0x10000000
#define SDP_USE_HKDF_EXPANDED_KEY       0x08000000
#define SDP_USE_PER_FILE_KEY            0x10000000
#define SDP_IS_CHAMBER_DIR              0x20000000
#define SDP_IS_DIRECTORY                0x40000000
#define SDP_IS_INO_CACHED	            0x80000000
#define SDP_IS_CLEARING_ONGOING         0x00010000
#define SDP_IS_FILE_IO_ONGOING          0x00020000
#ifdef CONFIG_SDP_KEY_DUMP
#define SDP_IS_TRACED                   0x00000001
#endif

struct fscrypt_sdp_context {
	//Store knox_flags to fscrypt_context, not in this context.
	__u32 engine_id;
	__u32 sdp_dek_type;
	__u32 sdp_dek_len;
	__u8 sdp_dek_buf[DEK_MAXLEN];
	__u8 sdp_en_buf[MAX_EN_BUF_LEN];
} __attribute__((__packed__));

extern int dek_is_locked(int engine_id);
extern int dek_encrypt_dek_efs(int engine_id, dek_t *plainDek, dek_t *encDek);
extern int dek_decrypt_dek_efs(int engine_id, dek_t *encDek, dek_t *plainDek);
extern int dek_encrypt_fek(unsigned char *master_key, unsigned int master_key_len,
					unsigned char *fek, unsigned int fek_len,
					unsigned char *efek, unsigned int *efek_len);
extern int dek_decrypt_fek(unsigned char *master_key, unsigned int master_key_len,
					unsigned char *efek, unsigned int efek_len,
					unsigned char *fek, unsigned int *fek_len);
extern int fscrypt_sdp_get_engine_id(struct inode *inode);

//Exclusively masking the shared flags
#define FSCRYPT_SDP_PARSE_FLAG_SDP_ONLY(flag) (flag & FSCRYPT_KNOX_FLG_SDP_MASK)
#define FSCRYPT_SDP_PARSE_FLAG_OUT_OF_SDP(flag) (flag & ~FSCRYPT_KNOX_FLG_SDP_MASK)
#ifdef CONFIG_SDP_KEY_DUMP
#define FSCRYPT_SDP_PARSE_FLAG_SDP_TRACE_ONLY(flag) (flag & FSCRYPT_KNOX_FLG_SDP_TRACE_MASK)
#endif // End of CONFIG_SDP_KEY_DUMP

#ifdef CONFIG_FSCRYPT_SDP
//#include "fscrypto_sdp_private.h"
#include <sdp/fs_request.h>
#endif

/* Declarations for Open APIs*/
int sdp_crypto_generate_key(void *raw_key, int nbytes);
int sdp_crypto_hash_sha512(const u8 *data, u32 data_len, u8 *hashed);
int sdp_crypto_aes_gcm_encrypt(struct crypto_aead *tfm,
					u8 *data, size_t data_len, u8 *auth, u8 *iv);
int sdp_crypto_aes_gcm_decrypt(struct crypto_aead *tfm,
					u8 *data, size_t data_len, u8 *auth, u8 *iv);
int sdp_crypto_aes_gcm_encrypt_pack(struct crypto_aead *tfm, gcm_pack *pack);
int sdp_crypto_aes_gcm_decrypt_pack(struct crypto_aead *tfm, gcm_pack *pack);
struct crypto_aead *sdp_crypto_aes_gcm_key_setup(const u8 key[], size_t key_len);
void sdp_crypto_aes_gcm_key_free(struct crypto_aead *tfm);
int __init sdp_crypto_init(void);
void __exit sdp_crypto_exit(void);

void dump_file_key_hex(const char *tag, uint8_t *data, unsigned int data_len);
int fscrypt_sdp_dump_file_key(struct inode *inode);
int fscrypt_sdp_set_sdp_policy(struct inode *inode, int engine_id);
int fscrypt_sdp_set_sensitive(struct inode *inode, int engine_id, struct fscrypt_key *key);
int fscrypt_sdp_set_protected(struct inode *inode, int engine_id);
int fscrypt_sdp_add_chamber_directory(int engine_id, struct inode *inode);
int fscrypt_sdp_remove_chamber_directory(struct inode *inode);
int fscrypt_sdp_get_engine_id(struct inode *inode);
int fscrypt_sdp_inherit_info(struct inode *parent, struct inode *child, u32 *sdp_flags, struct sdp_info *sdpInfo);
void fscrypt_sdp_finalize_tasks(struct inode *inode);
struct sdp_info *fscrypt_sdp_alloc_sdp_info(void);
void fscrypt_sdp_put_sdp_info(struct sdp_info *ci_sdp_info);
bool fscrypt_sdp_init_sdp_info_cachep(void);
void fscrypt_sdp_release_sdp_info_cachep(void);
int fscrypt_sdp_derive_dek(struct fscrypt_info *crypt_info,
						unsigned char *derived_key,
						unsigned int derived_keysize);
int fscrypt_sdp_derive_uninitialized_dek(struct fscrypt_info *crypt_info,
						unsigned char *derived_key,
						unsigned int derived_keysize);
int fscrypt_sdp_derive_fekey(struct inode *inode,
						struct fscrypt_info *crypt_info,
						struct fscrypt_key *fek);
int fscrypt_sdp_derive_fek(struct inode *inode,
						struct fscrypt_info *crypt_info,
						unsigned char *fek, unsigned int fek_len);
int fscrypt_sdp_store_fek(struct inode *inode,
						struct fscrypt_info *crypt_info,
						unsigned char *fek, unsigned int fek_len);
int fscrypt_sdp_is_classified(struct fscrypt_info *crypt_info);
int fscrypt_sdp_is_uninitialized(struct fscrypt_info *crypt_info);
int fscrypt_sdp_use_hkdf_expanded_key(struct fscrypt_info *crypt_info);
int fscrypt_sdp_use_pfk(struct fscrypt_info *crypt_info);
int fscrypt_sdp_is_native(struct fscrypt_info *crypt_info);
int fscrypt_sdp_is_sensitive(struct fscrypt_info *crypt_info);
int fscrypt_sdp_is_to_sensitive(struct fscrypt_info *crypt_info);
void fscrypt_sdp_update_conv_status(struct fscrypt_info *crypt_info);

#ifdef CONFIG_SDP_KEY_DUMP
int fscrypt_sdp_trace_file(struct inode *inode);
int fscrypt_sdp_is_traced_file(struct fscrypt_info *crypt_info);
#endif // End of CONFIG_SDP_KEY_DUMP

#if defined(CONFIG_FSCRYPT_SDP) || defined(CONFIG_DDAR)
struct ext_fscrypt_info {
	struct fscrypt_info fscrypt_info;

#ifdef CONFIG_DDAR
	struct dd_info *ci_dd_info;
#endif

#ifdef CONFIG_FSCRYPT_SDP
	struct sdp_info *ci_sdp_info;
#endif
};

static inline struct ext_fscrypt_info *GET_EXT_CI(struct fscrypt_info *ci)
{
	return container_of(ci, struct ext_fscrypt_info, fscrypt_info);
}
#endif

#ifdef CONFIG_FSCRYPT_SDP
static inline bool fscrypt_sdp_protected(const u32 knox_flags)
{
	if (knox_flags & FSCRYPT_KNOX_FLG_SDP_MASK)
		return true;

	return false;
}

static inline int fscrypt_set_knox_sdp_flags(union fscrypt_context *ctx_u,
						struct fscrypt_info *crypt_info)
{
	struct ext_fscrypt_info *ext_crypt_info;

	if (!crypt_info)
		return 0;

	ext_crypt_info = GET_EXT_CI(crypt_info);
	if (!ext_crypt_info->ci_sdp_info)
		return 0;

	switch (ctx_u->version) {
	case FSCRYPT_CONTEXT_V1: {
		struct fscrypt_context_v1 *ctx = &ctx_u->v1;

		ctx->knox_flags = ext_crypt_info->ci_sdp_info->sdp_flags;
		return 0;
	}
	case FSCRYPT_CONTEXT_V2: {
		struct fscrypt_context_v2 *ctx = &ctx_u->v2;

		ctx->knox_flags = ext_crypt_info->ci_sdp_info->sdp_flags;
		return 0;
	}
	}
	/* unreachable */
	return -EINVAL;
}
#endif

#ifdef CONFIG_DDAR
static inline bool fscrypt_ddar_protected(const u32 knox_flags)
{
	if (knox_flags & FSCRYPT_KNOX_FLG_DDAR_ENABLED)
		return true;

	return false;
}

static inline int fscrypt_set_knox_ddar_flags(union fscrypt_context *ctx_u,
						struct fscrypt_info *crypt_info)
{
	struct ext_fscrypt_info *ext_crypt_info;

	if (!crypt_info)
		return 0;

	ext_crypt_info = GET_EXT_CI(crypt_info);
	if (!ext_crypt_info->ci_dd_info)
		return 0;

	switch (ctx_u->version) {
	case FSCRYPT_CONTEXT_V1: {
		struct fscrypt_context_v1 *ctx = &ctx_u->v1;

		ctx->knox_flags |= ((ext_crypt_info->ci_dd_info->policy.flags
				<< FSCRYPT_KNOX_FLG_DDAR_SHIFT) & FSCRYPT_KNOX_FLG_DDAR_MASK);
		return 0;
	}
	case FSCRYPT_CONTEXT_V2: {
		struct fscrypt_context_v2 *ctx = &ctx_u->v2;

		ctx->knox_flags |= ((ext_crypt_info->ci_dd_info->policy.flags
				<< FSCRYPT_KNOX_FLG_DDAR_SHIFT) & FSCRYPT_KNOX_FLG_DDAR_MASK);
		return 0;
	}
	}
	/* unreachable */
	return -EINVAL;
}
#endif

#if defined(CONFIG_FSCRYPT_SDP) || defined(CONFIG_DDAR)
static inline struct fscrypt_info *fscrypt_has_dar_info(struct inode *parent)
{
	struct fscrypt_info *ci = fscrypt_get_info(parent);
	struct ext_fscrypt_info *ext_ci;

	if (ci) {
		ext_ci = GET_EXT_CI(ci);
#ifdef CONFIG_FSCRYPT_SDP
		if (ext_ci->ci_sdp_info)
			return ci;
#endif
#ifdef CONFIG_DDAR
		if (ext_ci->ci_dd_info)
			return ci;
#endif
	}
	return NULL;
}

static inline bool fscrypt_has_knox_flags(const union fscrypt_context *ctx_u)
{
	switch (ctx_u->version) {
	case FSCRYPT_CONTEXT_V1: {
		const struct fscrypt_context_v1 *ctx = &ctx_u->v1;

		return (ctx->knox_flags != 0) ? true : false;
	}
	case FSCRYPT_CONTEXT_V2: {
		const struct fscrypt_context_v2 *ctx = &ctx_u->v2;

		return (ctx->knox_flags != 0) ? true : false;
	}
	}
	return false;
}

static inline u32 fscrypt_knox_flags_from_context(const union fscrypt_context *ctx_u)
{
	switch (ctx_u->version) {
	case FSCRYPT_CONTEXT_V1: {
		const struct fscrypt_context_v1 *ctx = &ctx_u->v1;

		return ctx->knox_flags;
	}
	case FSCRYPT_CONTEXT_V2: {
		const struct fscrypt_context_v2 *ctx = &ctx_u->v2;

		return ctx->knox_flags;
	}
	}
	return 0;
}
#endif

#ifdef CONFIG_DDAR
extern int fscrypt_dd_decrypt_page(struct inode *inode, struct page *page);
extern int fscrypt_dd_encrypted(struct bio *bio);
extern int fscrypt_dd_encrypted_inode(const struct inode *inode);
extern int fscrypt_dd_is_traced_inode(const struct inode *inode);
extern void fscrypt_dd_trace_inode(const struct inode *inode);
extern long fscrypt_dd_get_ino(struct bio *bio);
extern long fscrypt_dd_ioctl(unsigned int cmd, unsigned long *arg, struct inode *inode);
extern int fscrypt_dd_submit_bio(struct inode *inode, struct bio *bio);
extern int fscrypt_dd_may_submit_bio(struct bio *bio);
extern struct inode *fscrypt_bio_get_inode(const struct bio *bio);
extern bool fscrypt_dd_can_merge_bio(struct bio *bio, struct address_space *mapping);
#endif

#ifdef CONFIG_FSCRYPT_SDP
int fscrypt_get_encryption_key(
		struct fscrypt_info *crypt_info,
		struct fscrypt_key *key);
int fscrypt_get_encryption_key_classified(
		struct fscrypt_info *crypt_info,
		struct fscrypt_key *key);
int fscrypt_get_encryption_kek(
		struct fscrypt_info *crypt_info,
		struct fscrypt_key *kek);
#endif

#endif /* SDP_CRYPTO_H_ */
