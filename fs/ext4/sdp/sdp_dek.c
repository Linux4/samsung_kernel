/*
 * sdp_dek.c
 *
 */
#include <linux/kthread.h>
#include <linux/pagemap.h>
#include <asm/unaligned.h>
#include "../ext4.h"
#include "../xattr.h"

#include <sdp/fs_handler.h>
#if defined(CONFIG_EXT4CRYPT_SDP) || defined(CONFIG_DDAR)
#include "../fscrypt_knox_private.h"
#endif

#ifdef CONFIG_EXT4CRYPT_SDP
#include "fscrypto_sdp_dek_private.h"
#endif

static int __fscrypt_get_sdp_context(struct inode *inode, struct ext4_crypt_info *crypt_info);
static int __fscrypt_get_sdp_dek(struct ext4_crypt_info *crypt_info, unsigned char *fe_key);
static int __fscrypt_set_sdp_dek(struct ext4_crypt_info *crypt_info, unsigned char *fe_key, int fe_key_len);
static int __fscrypt_sdp_finish_set_sensitive(struct inode *inode,
		struct ext4_encryption_context *ctx,
		struct ext4_crypt_info *crypt_info, unsigned char *fe_key, int keysize);

static struct kmem_cache *sdp_info_cachep;

inline struct sdp_info *fscrypt_sdp_alloc_sdp_info(void)
{
	struct sdp_info *ci_sdp_info;

	ci_sdp_info = kmem_cache_alloc(sdp_info_cachep, GFP_NOFS);
	if (!ci_sdp_info) {
		DEK_LOGE("Failed to alloc sdp info!!\n");
		return NULL;
	}
	ci_sdp_info->sdp_flags = 0;
	spin_lock_init(&ci_sdp_info->sdp_flag_lock);

	return ci_sdp_info;
}

int fscrypt_sdp_set_sensitive(int engine_id, struct inode *inode)
{
	struct ext4_crypt_info *ci = ext4_encryption_info(inode);
	struct fscrypt_sdp_context sdp_ctx;
	struct ext4_encryption_context ctx;
	int rc = 0;
	int is_dir = 0;

	if (!ci->ci_sdp_info) {
		struct sdp_info *ci_sdp_info = fscrypt_sdp_alloc_sdp_info();
		if (!ci_sdp_info) {
			return -ENOMEM;
		}

		if (cmpxchg(&ci->ci_sdp_info, NULL, ci_sdp_info) != NULL) {
			fscrypt_sdp_put_sdp_info(ci_sdp_info);
		}
	}

	ci->ci_sdp_info->engine_id = engine_id;
	if (S_ISDIR(inode->i_mode)) {
		ci->ci_sdp_info->sdp_flags |= SDP_DEK_IS_SENSITIVE;
		is_dir = 1;
	} else if (S_ISREG(inode->i_mode)) {
		ci->ci_sdp_info->sdp_flags |= SDP_DEK_TO_SET_SENSITIVE;
	}

	sdp_ctx.engine_id = engine_id;
	sdp_ctx.sdp_dek_type = DEK_TYPE_PLAIN;
	sdp_ctx.sdp_dek_len = DEK_MAXLEN;
	memset(sdp_ctx.sdp_dek_buf, 0, DEK_MAXLEN);

	rc = fscrypt_sdp_set_context(inode, &sdp_ctx, sizeof(sdp_ctx));

	if (rc) {
		DEK_LOGE(KERN_ERR
			   "%s: Failed to set sensitive flag (err:%d)\n", __func__, rc);
		return rc;
	}

	rc = ext4_xattr_get(inode, EXT4_XATTR_INDEX_ENCRYPTION,
			EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
			&ctx, sizeof(ctx));
	if (rc < 0) {
		DEK_LOGE(KERN_ERR
			   "%s: Failed to get fscrypt ctx (err:%d)\n", __func__, rc);
		return rc;
	}

	if (!is_dir) {
		//run setsensitive with nonce from ctx
		rc = __fscrypt_sdp_finish_set_sensitive(inode, &ctx, ci, ctx.nonce, sizeof(ctx.nonce));
	} else {
		ctx.knox_flags = FSCRYPT_SDP_PARSE_FLAG_OUT_OF_SDP(ctx.knox_flags) | SDP_DEK_IS_SENSITIVE;
		inode_lock(inode);
		rc = ext4_xattr_set(inode, EXT4_XATTR_INDEX_ENCRYPTION,
				EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
				&ctx, sizeof(ctx), 0);
		inode_unlock(inode);
	}

	return rc;
}

int fscrypt_sdp_set_protected(struct inode *inode)
{
	struct ext4_crypt_info *ci = ext4_encryption_info(inode);
	struct fscrypt_sdp_context sdp_ctx;
	struct ext4_encryption_context ctx;
	int rc = 0;

	if (!ci || !ci->ci_sdp_info)
		return -1;

	if (!(ci->ci_sdp_info->sdp_flags & SDP_DEK_IS_SENSITIVE)) {
		DEK_LOGD("%s: already in protected now.\n", __func__);
		return 0;
	}

	if (dek_is_locked(ci->ci_sdp_info->engine_id)) {
		DEK_LOGE("%s: Failed to set protected it's locked state now..\n", __func__);
		return -1;
	}

	rc = ext4_xattr_get(inode, EXT4_XATTR_INDEX_ENCRYPTION,
			EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
			&ctx, sizeof(ctx));
	if (rc < 0) {
		DEK_LOGE(KERN_ERR
			   "%s: Failed to get fscrypt ctx (err:%d)\n", __func__, rc);
		return rc;
	}

	if (!S_ISDIR(inode->i_mode)) {
		//Get nonce encrypted by SDP key
		rc = __fscrypt_get_sdp_dek(ci, ctx.nonce);
		if (rc) {
			DEK_LOGE(KERN_ERR
					   "%s: Failed to get dek (err:%d)\n", __func__, rc);
			return rc;
		}
	}

	ctx.knox_flags = FSCRYPT_SDP_PARSE_FLAG_OUT_OF_SDP(ctx.knox_flags);
	inode_lock(inode);
	rc = ext4_xattr_set(inode, EXT4_XATTR_INDEX_ENCRYPTION,
			EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
			&ctx, sizeof(ctx), 0);
	inode_unlock(inode);
	if (rc) {
		DEK_LOGE("%s: Failed to set ext4 context for sdp (err:%d)\n", __func__, rc);
		return rc;
	}

	//Unset SDP context
	ci->ci_sdp_info->sdp_flags = FSCRYPT_SDP_PARSE_FLAG_OUT_OF_SDP(ci->ci_sdp_info->sdp_flags);
	memzero_explicit(sdp_ctx.sdp_dek_buf, DEK_MAXLEN);
	rc = fscrypt_sdp_set_context(inode, &sdp_ctx, sizeof(sdp_ctx));

	if (rc) {
		DEK_LOGE("%s: Failed to set sdp context (err:%d)\n", __func__, rc);
		return rc;
	}

	fscrypt_sdp_cache_remove_inode_num(inode);
	mapping_clear_sensitive(inode->i_mapping);

	return 0;
}

int fscrypt_sdp_add_chamber_directory(int engine_id, struct inode *inode)
{
	struct ext4_crypt_info *ci = ext4_encryption_info(inode);
	struct fscrypt_sdp_context sdp_ctx;
	struct ext4_encryption_context ctx;
	int rc = 0;

	rc = ext4_xattr_get(inode, EXT4_XATTR_INDEX_ENCRYPTION,
			EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
			&ctx, sizeof(ctx));
	if (rc < 0) {
		DEK_LOGE(KERN_ERR
			   "%s: Failed to get fscrypt ctx (err:%d)\n", __func__, rc);
		return rc;
	}

	if (!ci->ci_sdp_info) {
		struct sdp_info *ci_sdp_info = fscrypt_sdp_alloc_sdp_info();
		if (!ci_sdp_info) {
			return -ENOMEM;
		}

		if (cmpxchg(&ci->ci_sdp_info, NULL, ci_sdp_info) != NULL) {
			DEK_LOGD("Need to put info\n");
			fscrypt_sdp_put_sdp_info(ci_sdp_info);
		}
	}

	ci->ci_sdp_info->sdp_flags |= SDP_IS_CHAMBER_DIR;

	if (!(ci->ci_sdp_info->sdp_flags & SDP_DEK_IS_SENSITIVE)) {
		ci->ci_sdp_info->sdp_flags |= SDP_DEK_IS_SENSITIVE;
		ci->ci_sdp_info->engine_id = engine_id;
		sdp_ctx.sdp_dek_type = DEK_TYPE_PLAIN;
		sdp_ctx.sdp_dek_len = DEK_MAXLEN;
		memset(sdp_ctx.sdp_dek_buf, 0, DEK_MAXLEN);
	} else {
		sdp_ctx.sdp_dek_type = ci->ci_sdp_info->sdp_dek.type;
		sdp_ctx.sdp_dek_len = ci->ci_sdp_info->sdp_dek.len;
		memcpy(sdp_ctx.sdp_dek_buf, ci->ci_sdp_info->sdp_dek.buf, ci->ci_sdp_info->sdp_dek.len);
	}
	sdp_ctx.engine_id = ci->ci_sdp_info->engine_id;

	rc = fscrypt_sdp_set_context(inode, &sdp_ctx, sizeof(sdp_ctx));

	if (rc) {
		DEK_LOGE("%s: Failed to add chamber dir.. (err:%d)\n", __func__, rc);
		return rc;
	}

	ctx.knox_flags = ci->ci_sdp_info->sdp_flags | FSCRYPT_SDP_PARSE_FLAG_OUT_OF_SDP(ctx.knox_flags);
	inode_lock(inode);
	rc = ext4_xattr_set(inode, EXT4_XATTR_INDEX_ENCRYPTION,
			EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
			&ctx, sizeof(ctx), 0);
	inode_unlock(inode);
	if (rc) {
		DEK_LOGE("%s: Failed to set ext4 context for sdp (err:%d)\n", __func__, rc);
	}

	return rc;
}

int fscrypt_sdp_remove_chamber_directory(struct inode *inode)
{
	struct ext4_crypt_info *ci = ext4_encryption_info(inode);
	struct fscrypt_sdp_context sdp_ctx;
	struct ext4_encryption_context ctx;
	int rc = 0;

	rc = ext4_xattr_get(inode, EXT4_XATTR_INDEX_ENCRYPTION,
			EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
			&ctx, sizeof(ctx));
	if (rc < 0) {
		DEK_LOGE(KERN_ERR
			   "%s: Failed to get fscrypt ctx (err:%d)\n", __func__, rc);
		return rc;
	}

	if (!ci->ci_sdp_info)
		return -EINVAL;

	ci->ci_sdp_info->sdp_flags = 0;

	sdp_ctx.engine_id = ci->ci_sdp_info->engine_id;
	sdp_ctx.sdp_dek_type = ci->ci_sdp_info->sdp_dek.type;
	sdp_ctx.sdp_dek_len = ci->ci_sdp_info->sdp_dek.len;
	memset(sdp_ctx.sdp_dek_buf, 0, DEK_MAXLEN);

	rc = fscrypt_sdp_set_context(inode, &sdp_ctx, sizeof(sdp_ctx));

	if (rc) {
		DEK_LOGE("%s: Failed to remove chamber dir.. (err:%d)\n", __func__, rc);
		return rc;
	}

	ctx.knox_flags = FSCRYPT_SDP_PARSE_FLAG_OUT_OF_SDP(ctx.knox_flags);
	inode_lock(inode);
	rc = ext4_xattr_set(inode, EXT4_XATTR_INDEX_ENCRYPTION,
			EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
			&ctx, sizeof(ctx), 0);
	inode_unlock(inode);
	if (rc) {
		DEK_LOGE("%s: Failed to set ext4 context for sdp (err:%d)\n", __func__, rc);
	}

	return rc;
}

inline int __fscrypt_get_sdp_dek(struct ext4_crypt_info *crypt_info, unsigned char *fe_key)
{
	int res = 0;
	dek_t *DEK;

	DEK = kmalloc(sizeof(dek_t), GFP_NOFS);
	if (!DEK) {
		return -ENOMEM;
	}

	res = dek_decrypt_dek_efs(crypt_info->ci_sdp_info->engine_id, &crypt_info->ci_sdp_info->sdp_dek, DEK);
	if (res < 0) {
		res = -ENOKEY;
		goto out;
	}

	if (DEK->len > FS_MAX_KEY_SIZE) {
		res = -EINVAL;
		goto out;
	}
	memcpy(fe_key, DEK->buf, DEK->len);

out:
	memset(DEK->buf, 0, DEK_MAXLEN);
	kzfree(DEK);
	return res;
}

inline int __fscrypt_set_sdp_dek(struct ext4_crypt_info *crypt_info,
		unsigned char *fe_key, int fe_key_len)
{
	/*
	 *
	 * __ext4_set_sdp_dek(crypt_info->engine_id, crypt_info->ci_raw_key,
				ext4_encryption_key_size(mode), &crypt_info->sdp_dek);
	 */
	int res;
	dek_t *DEK;

	DEK = kmalloc(sizeof(dek_t), GFP_NOFS);
	if (!DEK) {
		return -ENOMEM;
	}

	DEK->type = DEK_TYPE_PLAIN;
	DEK->len = fe_key_len;
	memcpy(DEK->buf, fe_key, fe_key_len);

	res = dek_encrypt_dek_efs(crypt_info->ci_sdp_info->engine_id, DEK, &crypt_info->ci_sdp_info->sdp_dek);

	memset(DEK->buf, 0, DEK_MAXLEN);
	kzfree(DEK);

	return res;
}

inline int __fscrypt_get_sdp_context(struct inode *inode, struct ext4_crypt_info *crypt_info)
{
	int res = 0;
	struct fscrypt_sdp_context sdp_ctx;

	res = fscrypt_sdp_get_context(inode, &sdp_ctx, sizeof(sdp_ctx));

	if (res == sizeof(sdp_ctx)) {
		crypt_info->ci_sdp_info->engine_id = sdp_ctx.engine_id;
		crypt_info->ci_sdp_info->sdp_dek.type = sdp_ctx.sdp_dek_type;
		crypt_info->ci_sdp_info->sdp_dek.len = sdp_ctx.sdp_dek_len;
		DEK_LOGD("sensitive flags = %x, engid = %d\n", crypt_info->ci_sdp_info->sdp_flags, sdp_ctx.engine_id);
		memcpy(crypt_info->ci_sdp_info->sdp_dek.buf, sdp_ctx.sdp_dek_buf,
				sizeof(crypt_info->ci_sdp_info->sdp_dek.buf));

		if (S_ISDIR(inode->i_mode))
			crypt_info->ci_sdp_info->sdp_flags |= SDP_IS_DIRECTORY;

		res = 0;
	} else {
		res = -EINVAL;
	}

	return res;
}

static inline void __fscrypt_sdp_set_inode_sensitive(struct inode *inode)
{
	fscrypt_sdp_cache_add_inode_num(inode);
	mapping_set_sensitive(inode->i_mapping);
}

inline int __fscrypt_sdp_finish_set_sensitive(struct inode *inode,
		struct ext4_encryption_context *ctx,
		struct ext4_crypt_info *crypt_info, unsigned char *decrypted_key, int keysize) {
	int res = 0;
	struct fscrypt_sdp_context sdp_ctx;

	if (crypt_info->ci_sdp_info->sdp_flags & SDP_DEK_TO_SET_SENSITIVE ||
			crypt_info->ci_sdp_info->sdp_flags & SDP_DEK_TO_CONVERT_KEY_TYPE) {
		DEK_LOGD("sensitive SDP_DEK_TO_SET_SENSITIVE\n");
		//It's a new sensitive file, let's make sdp dek!
		res = __fscrypt_set_sdp_dek(crypt_info, decrypted_key, keysize);
		if (res) {
			DEK_LOGE("sensitive SDP_DEK_TO_SET_SENSITIVE error res = %d\n", res);
			return res;
		}

		crypt_info->ci_sdp_info->sdp_flags &= ~(SDP_DEK_TO_SET_SENSITIVE);
		crypt_info->ci_sdp_info->sdp_flags &= ~(SDP_DEK_TO_CONVERT_KEY_TYPE);
		crypt_info->ci_sdp_info->sdp_flags |= SDP_DEK_IS_SENSITIVE;
		sdp_ctx.engine_id = crypt_info->ci_sdp_info->engine_id;
		sdp_ctx.sdp_dek_type = crypt_info->ci_sdp_info->sdp_dek.type;
		sdp_ctx.sdp_dek_len = crypt_info->ci_sdp_info->sdp_dek.len;
		memcpy(sdp_ctx.sdp_dek_buf, crypt_info->ci_sdp_info->sdp_dek.buf,
				crypt_info->ci_sdp_info->sdp_dek.len);

		res = fscrypt_sdp_set_context(inode, &sdp_ctx, sizeof(sdp_ctx));
		if (res) {
			DEK_LOGE("sensitive SDP_DEK_TO_SET_SENSITIVE setxattr error res = %d\n", res);
			return res;
		}

		ctx->knox_flags = (FSCRYPT_SDP_PARSE_FLAG_OUT_OF_SDP(ctx->knox_flags) | SDP_DEK_IS_SENSITIVE);
		memzero_explicit(ctx->nonce, EXT4_KEY_DERIVATION_NONCE_SIZE);
		res = ext4_xattr_set(inode, EXT4_XATTR_INDEX_ENCRYPTION,
				EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
				ctx, sizeof(*ctx), 0);
		if (res) {
			DEK_LOGE("Failed to set cleared context for sdp (err:%d)\n", res);
			return res;
		}
		DEK_LOGD("sensitive SDP_DEK_TO_SET_SENSITIVE finished!!\n");
	}

	if (crypt_info->ci_sdp_info->sdp_flags & SDP_DEK_IS_SENSITIVE &&
			!(crypt_info->ci_sdp_info->sdp_flags & SDP_IS_DIRECTORY)) {
		__fscrypt_sdp_set_inode_sensitive(inode);
#ifdef CONFIG_SDP_KEY_DUMP
		if (get_sdp_sysfs_key_dump()) {
			key_dump(decrypted_key, keysize);
		}
#endif
	}

	return res;
}

int fscrypt_sdp_get_engine_id(struct inode *inode)
{
	struct ext4_crypt_info *crypt_info;

	crypt_info = ext4_encryption_info(inode);
	if (!crypt_info || !crypt_info->ci_sdp_info)
		return -1;

	return crypt_info->ci_sdp_info->engine_id;
}

typedef enum {
	SDP_THREAD_SET_SENSITIVE,
	SDP_THREAD_KEY_CONVERT
} sdp_thread_type;

inline int __fscrypt_sdp_thread_set_sensitive(void *arg)
{
	struct inode *inode = arg;

	if (inode) {
		struct ext4_crypt_info *ci = ext4_encryption_info(inode);

		if (ci && ci->ci_sdp_info) {
			fscrypt_sdp_set_sensitive(ci->ci_sdp_info->engine_id, inode);
		}
		iput(inode);
	}

	return 0;
}

inline int __fscrypt_sdp_thread_convert_sdp_key(void *arg)
{
	struct inode *inode = arg;

	if (inode) {
		struct ext4_crypt_info *ci = ext4_encryption_info(inode);

		if (ci && ci->ci_sdp_info) {
			int rc = 0;
			struct ext4_encryption_context ctx;

			rc = ext4_xattr_get(inode, EXT4_XATTR_INDEX_ENCRYPTION,
					EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
					&ctx, sizeof(ctx));
			if (rc < 0) {
				DEK_LOGE(KERN_ERR
					   "%s: Failed to get fscrypt ctx (err:%d)\n", __func__, rc);
				return 0;
			}

			rc = __fscrypt_get_sdp_dek(ci, ctx.nonce);
			if (rc) {
				DEK_LOGE("sensitive SDP_DEK_IS_SENSITIVE error rc = %d\n", rc);
				return 0;
			}

			__fscrypt_sdp_finish_set_sensitive(inode, &ctx, ci, ctx.nonce, sizeof(ctx.nonce));
			memzero_explicit(ctx.nonce, sizeof(ctx.nonce));
		}
		iput(inode);
	}

	return 0;
}

inline void fscrypt_sdp_run_thread(struct inode *inode, sdp_thread_type thread_type)
{
	struct task_struct *task = NULL;

	if (thread_type == SDP_THREAD_SET_SENSITIVE) {
		if (igrab(inode))
			task = kthread_run(__fscrypt_sdp_thread_set_sensitive, inode, "__fscrypt_sdp_thread_set_sensitive");
	}
	else if (thread_type == SDP_THREAD_KEY_CONVERT) {
		if (igrab(inode))
			task = kthread_run(__fscrypt_sdp_thread_convert_sdp_key, inode, "__fscrypt_sdp_thread_convert_sdp_key");
	}
	else
		return;

	if (IS_ERR(task)) {
		DEK_LOGE("unable to create kernel thread fscrypt_sdp_run_thread res:%ld\n",
				PTR_ERR(task));
	}
}

int fscrypt_sdp_get_key_if_sensitive(struct inode *inode, struct ext4_crypt_info *crypt_info, unsigned char *decrypted_key)
{
	int res = 0;

	res = __fscrypt_get_sdp_context(inode, crypt_info);
	if (!res
			&& (crypt_info->ci_sdp_info->sdp_flags & SDP_DEK_IS_SENSITIVE)
			&& !(crypt_info->ci_sdp_info->sdp_flags & SDP_IS_DIRECTORY)) {
		res = __fscrypt_get_sdp_dek(crypt_info, decrypted_key);
		if (res) {
			DEK_LOGE("sensitive SDP_DEK_IS_SENSITIVE error res = %d\n", res);
		} else {
			DEK_LOGD("Success to get FEK by SDP..\n");
			// Asym to sym key migration
			if (crypt_info->ci_sdp_info->sdp_dek.type != DEK_TYPE_AES_ENC) {
				DEK_LOGD("Asym file, type = %d\n", crypt_info->ci_sdp_info->sdp_dek.type);
				crypt_info->ci_sdp_info->sdp_flags |= SDP_DEK_TO_CONVERT_KEY_TYPE;
			}
		}
	}

	return res;
}

int fscrypt_sdp_test_and_inherit_context(struct inode *parent, struct inode *child, struct ext4_encryption_context *ctx)
{
	int res = 0;
	struct ext4_crypt_info *ci;

	ci = ext4_encryption_info(parent);
	if (ci && ci->ci_sdp_info &&
			ci->ci_sdp_info->sdp_flags & SDP_DEK_IS_SENSITIVE) {
		struct fscrypt_sdp_context sdp_ctx;

		DEK_LOGD("parent->i_crypt_info->sdp_flags: %x\n", ci->ci_sdp_info->sdp_flags);

		ctx->knox_flags = FSCRYPT_SDP_PARSE_FLAG_OUT_OF_SDP(ctx->knox_flags) | ci->ci_sdp_info->sdp_flags;
		if (!S_ISDIR(child->i_mode)) {
			ctx->knox_flags &= ~SDP_DEK_IS_SENSITIVE;
			ctx->knox_flags &= ~SDP_IS_DIRECTORY;
			ctx->knox_flags |= SDP_DEK_TO_SET_SENSITIVE;
		}
		ctx->knox_flags &= ~SDP_IS_CHAMBER_DIR;
		sdp_ctx.engine_id = ci->ci_sdp_info->engine_id;
		sdp_ctx.sdp_dek_type = DEK_TYPE_PLAIN;
		sdp_ctx.sdp_dek_len = DEK_MAXLEN;
		memset(sdp_ctx.sdp_dek_buf, 0, DEK_MAXLEN);

		DEK_LOGD("Inherited ctx->knox_flags: %x\n", ctx->knox_flags);

		res = fscrypt_sdp_set_context_nolock(child, &sdp_ctx, sizeof(sdp_ctx));
	}

	return res;
}

void fscrypt_sdp_finalize_tasks(struct inode *inode)
{
	struct ext4_crypt_info *ci = ext4_encryption_info(inode);//This pointer has been loaded by get_encryption_info completely

	if (ci && ci->ci_sdp_info &&
			(ci->ci_sdp_info->sdp_flags & FSCRYPT_KNOX_FLG_SDP_MASK)) {
		if (ci->ci_sdp_info->sdp_flags & SDP_DEK_IS_SENSITIVE &&
				!(ci->ci_sdp_info->sdp_flags & SDP_IS_DIRECTORY)) {
			__fscrypt_sdp_set_inode_sensitive(inode);
		}

		if (ci->ci_sdp_info->sdp_flags & SDP_DEK_TO_SET_SENSITIVE) {//Case for newly inherited child inode (Not sensitive yet)
			DEK_LOGD("Run set sensitive thread\n");
			fscrypt_sdp_run_thread(inode, SDP_THREAD_SET_SENSITIVE);
		} else if (ci->ci_sdp_info->sdp_flags & SDP_DEK_TO_CONVERT_KEY_TYPE) {//Case for converting from asym to sym (Already sensitive)
			DEK_LOGD("Run key convert thread\n");
			fscrypt_sdp_run_thread(inode, SDP_THREAD_KEY_CONVERT);
		}
	}
}

void fscrypt_sdp_put_sdp_info(struct sdp_info *ci_sdp_info)
{
	if (ci_sdp_info) {
		kmem_cache_free(sdp_info_cachep, ci_sdp_info);
	}
}

bool fscrypt_sdp_init_sdp_info_cachep(void)
{
	sdp_info_cachep = KMEM_CACHE(sdp_info, SLAB_RECLAIM_ACCOUNT);
	if (!sdp_info_cachep)
		return false;
	return true;
}

void fscrypt_sdp_release_sdp_info_cachep(void)
{
	kmem_cache_destroy(sdp_info_cachep);
}
