/*
 *  Copyright (C) 2017 Samsung Electronics Co., Ltd.
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

#ifndef _FSCRYPTO_SDP_DEK_H
#define _FSCRYPTO_SDP_DEK_H

#ifdef CONFIG_EXT4CRYPT_SDP
struct sdp_info {
	u32 sdp_flags;
	u32 engine_id;
	dek_t sdp_dek;
	spinlock_t sdp_flag_lock;
};

int fscrypt_sdp_set_sensitive(int engine_id, struct inode *inode);
int fscrypt_sdp_set_protected(struct inode *inode);
int fscrypt_sdp_add_chamber_directory(int engine_id, struct inode *inode);
int fscrypt_sdp_remove_chamber_directory(struct inode *inode);

int fscrypt_sdp_get_key_if_sensitive(struct inode *inode, struct ext4_crypt_info *crypt_info, unsigned char *decrypted_key);

int fscrypt_sdp_get_engine_id(struct inode *inode);
int fscrypt_sdp_test_and_inherit_context(struct inode *parent, struct inode *child, struct ext4_encryption_context *ctx);
void fscrypt_sdp_finalize_tasks(struct inode *inode);
struct sdp_info *fscrypt_sdp_alloc_sdp_info(void);
void fscrypt_sdp_put_sdp_info(struct sdp_info *ci_sdp_info);
bool fscrypt_sdp_init_sdp_info_cachep(void);
void fscrypt_sdp_release_sdp_info_cachep(void);
#endif

#endif	/* _FSCRYPTO_SDP_DEK_H */
