/*
 * fs/sdcardfs/main.c
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd
 *   Authors: Daeho Jeong, Woojoong Lee, Seunghwan Hyun,
 *               Sunghwan Yun, Sungjong Seo
 *
 * This program has been developed as a stackable file system based on
 * the WrapFS which written by
 *
 * Copyright (c) 1998-2011 Erez Zadok
 * Copyright (c) 2009     Shrikar Archak
 * Copyright (c) 2003-2011 Stony Brook University
 * Copyright (c) 2003-2011 The Research Foundation of SUNY
 *
 * This file is dual licensed.  It may be redistributed and/or modified
 * under the terms of the Apache 2.0 License OR version 2 of the GNU
 * General Public License.
 */

#include "sdcardfs.h"
#include <linux/fscrypt.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs_parser.h>

enum {
	Opt_fsuid,
	Opt_fsgid,
	Opt_gid,
	Opt_debug,
	Opt_mask,
	Opt_multiuser,
	Opt_userid,
	Opt_reserved_mb,
	Opt_gid_derivation,
	Opt_default_normal,
	Opt_nocache,
	Opt_unshared_obb,
	Opt_err,
};

static const struct fs_parameter_spec sdcardfs_fs_parameters[] = {
	fsparam_u32	("fsuid",		Opt_fsuid),
	fsparam_u32	("fsgid",		Opt_fsgid),
	fsparam_u32	("gid",			Opt_gid),
	fsparam_u32	("mask",		Opt_mask),
	fsparam_u32	("userid",		Opt_userid),
	fsparam_u32	("reserved_mb",		Opt_reserved_mb),
	fsparam_flag	("multiuser",		Opt_multiuser),
	fsparam_flag	("derive_gid",		Opt_gid_derivation),
	fsparam_flag	("default_normal",	Opt_default_normal),
	fsparam_flag	("nocache",		Opt_nocache),
	fsparam_flag	("unshared_obb",	Opt_unshared_obb),
	{}
};

static int sdcardfs_parse_param(struct fs_context *fc,
		struct fs_parameter *param)
{
	struct fs_parse_result result;
	struct sdcardfs_fs_context *ctx = fc->fs_private;
	int opt;
	struct sdcardfs_mount_options *opts = &ctx->opts;
	struct sdcardfs_vfsmount_options *vfsopts = &ctx->vfsmnt_opts;

	opt = fs_parse(fc, sdcardfs_fs_parameters, param, &result);
	if (opt < 0)
		return opt;

	if (fc->purpose == FS_CONTEXT_FOR_RECONFIGURE) {
		if (!fc->oldapi)
			return invalfc(fc, "only do_remount supported");

		switch (opt) {
		case Opt_gid:
		case Opt_mask:
			break;
		default:
			return invalfc(fc, "%s is not allowed in reconfigure",
					param->key);
		}
	}

	switch (opt) {
	case Opt_fsuid:
		opts->fs_low_uid = result.uint_32;
		break;
	case Opt_fsgid:
		opts->fs_low_gid = result.uint_32;
		break;
	case Opt_gid:
		vfsopts->gid = result.uint_32;
		break;
	case Opt_userid:
		opts->fs_user_id = result.uint_32;
		break;
	case Opt_mask:
		vfsopts->mask = result.uint_32;
		break;
	case Opt_multiuser:
		opts->multiuser = true;
		break;
	case Opt_reserved_mb:
		opts->reserved_mb = result.uint_32;
		break;
	case Opt_gid_derivation:
		opts->gid_derivation = true;
		break;
	case Opt_default_normal:
		opts->default_normal = true;
		break;
	case Opt_nocache:
		opts->nocache = true;
		break;
	case Opt_unshared_obb:
		opts->unshared_obb = true;
		break;
	/* unknown option */
	default:
		return invalfc(fc, "Unknown option %s", param->key);
	}

	return 0;
}

int sdcardfs_parse_monolithic(struct fs_context *fc, void *data)
{
	errorfc(fc, "%s", (char *) data);
	return generic_parse_monolithic(fc, data);
}

DEFINE_MUTEX(sdcardfs_super_list_lock);
EXPORT_SYMBOL_GPL(sdcardfs_super_list_lock);
LIST_HEAD(sdcardfs_super_list);
EXPORT_SYMBOL_GPL(sdcardfs_super_list);

/*
 * There is no need to lock the sdcardfs_super_info's rwsem as there is no
 * way anyone can have a reference to the superblock at this point in time.
 */
static int sdcardfs_fill_super(struct super_block *sb, struct fs_context *fc)
{
	int err = 0;
	struct super_block *lower_sb;
	struct path lower_path;
	struct sdcardfs_sb_info *sb_info;
	struct inode *inode;
	struct sdcardfs_fs_context *ctx = fc->fs_private;

	errorfc(fc, "version 2.0 linux 5.10");
	errorfc(fc, "dev_name -> %s", fc->source);

	/* parse lower path */
	err = kern_path(fc->source, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
			&lower_path);
	if (err) {
		pr_err("sdcardfs: error accessing lower directory '%s'\n",
				fc->source);
		goto out;
	}

	/* allocate superblock private data */
	sb->s_fs_info = kzalloc(sizeof(struct sdcardfs_sb_info), GFP_KERNEL);
	if (!SDCARDFS_SB(sb)) {
		pr_crit("sdcardfs: fill_super: out of memory\n");
		err = -ENOMEM;
		goto out_free;
	}

	sb_info = sb->s_fs_info;
	sb_info->options = ctx->opts;
	sb_info->vfsmnt_opt = ctx->vfsmnt_opts;

	/* set the lower superblock field of upper superblock */
	lower_sb = lower_path.dentry->d_sb;
	atomic_inc(&lower_sb->s_active);
	sdcardfs_set_lower_super(sb, lower_sb);

	sb->s_stack_depth = lower_sb->s_stack_depth + 1;
	if (sb->s_stack_depth > FILESYSTEM_MAX_STACK_DEPTH) {
		pr_err("sdcardfs: maximum fs stacking depth exceeded\n");
		err = -EINVAL;
		goto out_sput;
	}

	/* inherit maxbytes from lower file system */
	sb->s_maxbytes = lower_sb->s_maxbytes;

	/*
	 * Our c/m/atime granularity is 1 ns because we may stack on file
	 * systems whose granularity is as good.
	 */
	sb->s_time_gran = 1;

	sb->s_magic = SDCARDFS_SUPER_MAGIC;
	sb->s_op = &sdcardfs_sops;

	/* get a new inode and allocate our root dentry */
	inode = sdcardfs_iget(sb, d_inode(lower_path.dentry), 0);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_sput;
	}
	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		err = -ENOMEM;
		goto out_sput;
	}
	d_set_d_op(sb->s_root, &sdcardfs_ci_dops);

	/* link the upper and lower dentries */
	sb->s_root->d_fsdata = NULL;
	err = new_dentry_private_data(sb->s_root);
	if (err)
		goto out_freeroot;

	/* set the lower dentries for s_root */
	sdcardfs_set_lower_path(sb->s_root, &lower_path);

	/*
	 * No need to call interpose because we already have a positive
	 * dentry, which was instantiated by d_make_root.  Just need to
	 * d_rehash it.
	 */
	d_rehash(sb->s_root);

	/* setup permission policy */
	sb_info->obbpath_s = kzalloc(PATH_MAX, GFP_KERNEL);
	mutex_lock(&sdcardfs_super_list_lock);
	if (sb_info->options.multiuser) {
		setup_derived_state(d_inode(sb->s_root), PERM_PRE_ROOT,
				sb_info->options.fs_user_id, AID_ROOT);
		snprintf(sb_info->obbpath_s, PATH_MAX, "%s/obb", fc->source);
	} else {
		setup_derived_state(d_inode(sb->s_root), PERM_ROOT,
				sb_info->options.fs_user_id, AID_ROOT);
		snprintf(sb_info->obbpath_s, PATH_MAX, "%s/Android/obb", fc->source);
	}
	fixup_tmp_permissions(d_inode(sb->s_root));
	sb_info->sb = sb;
	list_add(&sb_info->list, &sdcardfs_super_list);
	mutex_unlock(&sdcardfs_super_list_lock);

	sb_info->fscrypt_nb.notifier_call = sdcardfs_on_fscrypt_key_removed;
	fscrypt_register_key_removal_notifier(&sb_info->fscrypt_nb);

	pr_info("sdcardfs: mounted on top of %s type %s\n",
			fc->source, lower_sb->s_type->name);
	goto out; /* all is well */

	/* no longer needed: free_dentry_private_data(sb->s_root); */
out_freeroot:
	dput(sb->s_root);
	sb->s_root = NULL;
out_sput:
	/* drop refs we took earlier */
	atomic_dec(&lower_sb->s_active);
	kfree(SDCARDFS_SB(sb));
	sb->s_fs_info = NULL;
out_free:
	path_put(&lower_path);

out:
	return err;
}

struct sdcardfs_mount_private {
	const char *dev_name;
	void *raw_data;
};

static struct sdcardfs_vfsmount_options *__sdcardfs_alloc_mnt_data(void)
{
	struct sdcardfs_vfsmount_options *vfsmnt_opts =
		kzalloc(sizeof(struct sdcardfs_vfsmount_options), GFP_KERNEL);
	return vfsmnt_opts ?: NULL;
}

void *sdcardfs_alloc_mnt_data(struct fs_context *fc)
{
	struct sdcardfs_vfsmount_options *vfsmnt_opts;
	struct sdcardfs_fs_context *ctx = fc->fs_private;

	vfsmnt_opts = __sdcardfs_alloc_mnt_data();
	if (!vfsmnt_opts)
		return NULL;

	*vfsmnt_opts = ctx->vfsmnt_opts;
	return vfsmnt_opts;
}

void sdcardfs_kill_sb(struct super_block *sb)
{
	struct sdcardfs_sb_info *sbi;

	if (sb->s_magic == SDCARDFS_SUPER_MAGIC && sb->s_fs_info) {
		sbi = SDCARDFS_SB(sb);

		fscrypt_unregister_key_removal_notifier(&sbi->fscrypt_nb);

		mutex_lock(&sdcardfs_super_list_lock);
		list_del(&sbi->list);
		mutex_unlock(&sdcardfs_super_list_lock);
	}
	kill_anon_super(sb);
}

static void sdcardfs_free_fc(struct fs_context *fc)
{
	struct sdcardfs_fs_context *ctx = fc->fs_private;

	if (ctx) {
		if (ctx->path)
			path_put(ctx->path);
		kfree(ctx);
	}
}

static int sdcardfs_get_tree(struct fs_context *fc)
{
	return get_tree_nodev(fc, sdcardfs_fill_super);
}

static int sdcardfs_reconfigure(struct fs_context *fc)
{
	struct sdcardfs_fs_context *ctx = fc->fs_private;
	struct vfsmount *m = ctx->path->mnt;

	*((struct sdcardfs_vfsmount_options *) m->data) = ctx->vfsmnt_opts;
	do_propagate_remount(m);

	return 0;
}

static const struct fs_context_operations sdcardfs_context_ops = {
	.free			= sdcardfs_free_fc,
	.parse_param		= sdcardfs_parse_param,
	.parse_monolithic	= sdcardfs_parse_monolithic,
	.reconfigure		= sdcardfs_reconfigure,
	.get_tree		= sdcardfs_get_tree,
};

static void *sdcardfs_clone_mnt_data(void *data)
{
	struct sdcardfs_vfsmount_options *opt =
		kmalloc(sizeof(struct sdcardfs_vfsmount_options), GFP_KERNEL);
	struct sdcardfs_vfsmount_options *old = data;

	if (!opt)
		return NULL;
	opt->gid = old->gid;
	opt->mask = old->mask;
	return opt;
}

static void sdcardfs_copy_mnt_data(void *data, void *newdata)
{
	struct sdcardfs_vfsmount_options *old = data;
	struct sdcardfs_vfsmount_options *new = newdata;

	old->gid = new->gid;
	old->mask = new->mask;
}

static const struct vfsmount_data_operations sdcardfs_vfsmnt_ops = {
	.alloc	= sdcardfs_alloc_mnt_data,
	.clone	= sdcardfs_clone_mnt_data,
	.copy 	= sdcardfs_copy_mnt_data,
};

static void sdcardfs_init_default_mount_options(struct fs_context *fc)
{
	struct sdcardfs_fs_context *ctx = fc->fs_private;
	struct sdcardfs_mount_options *opts = &ctx->opts;
	struct sdcardfs_vfsmount_options *vfsopts = &ctx->vfsmnt_opts;

	if (fc->purpose != FS_CONTEXT_FOR_MOUNT)
		return;

	opts->fs_low_uid = AID_MEDIA_RW;
	opts->fs_low_gid = AID_MEDIA_RW;
	vfsopts->mask = 0;
	opts->multiuser = false;
	opts->fs_user_id = 0;
	vfsopts->gid = 0;
	/* by default, 0MB is reserved */
	opts->reserved_mb = 0;
	/* by default, gid derivation is off */
	opts->gid_derivation = false;
	opts->default_normal = false;
	opts->nocache = false;
	opts->unshared_obb = false;
}

static int sdcardfs_init_fs_context(struct fs_context *fc)
{
	struct sdcardfs_fs_context *ctx;
	struct path *path = (struct path *) fc->fs_private;

	ctx = kzalloc(sizeof(struct sdcardfs_fs_context), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	if (path) {
		path_get(path);
		ctx->path = path;
	}

	fc->fs_private = ctx;
	fc->ops = &sdcardfs_context_ops;
	fc->vfsmnt_ops = &sdcardfs_vfsmnt_ops;

	sdcardfs_init_default_mount_options(fc);

	return 0;
}

static struct file_system_type sdcardfs_fs_type = {
	.owner			= THIS_MODULE,
	.name			= SDCARDFS_NAME,
	.init_fs_context	= sdcardfs_init_fs_context,
	.kill_sb		= sdcardfs_kill_sb,
	.fs_flags		= 0,
};
MODULE_ALIAS_FS(SDCARDFS_NAME);

static int __init init_sdcardfs_fs(void)
{
	int err;

	pr_info("Registering sdcardfs " SDCARDFS_VERSION "\n");

	err = sdcardfs_init_inode_cache();
	if (err)
		goto out;
	err = sdcardfs_init_dentry_cache();
	if (err)
		goto out;
	err = packagelist_init();
	if (err)
		goto out;
	err = register_filesystem(&sdcardfs_fs_type);
out:
	if (err) {
		sdcardfs_destroy_inode_cache();
		sdcardfs_destroy_dentry_cache();
		packagelist_exit();
	}
	return err;
}

static void __exit exit_sdcardfs_fs(void)
{
	sdcardfs_destroy_inode_cache();
	sdcardfs_destroy_dentry_cache();
	packagelist_exit();
	unregister_filesystem(&sdcardfs_fs_type);
	pr_info("Completed sdcardfs module unload\n");
}

/* Original wrapfs authors */
MODULE_AUTHOR("Erez Zadok, Filesystems and Storage Lab, Stony Brook University (http://www.fsl.cs.sunysb.edu/)");

/* Original sdcardfs authors */
MODULE_AUTHOR("Woojoong Lee, Daeho Jeong, Kitae Lee, Yeongjin Gil System Memory Lab., Samsung Electronics");

/* Current maintainer */
MODULE_AUTHOR("Daniel Rosenberg, Google");
MODULE_DESCRIPTION("Sdcardfs " SDCARDFS_VERSION);
MODULE_LICENSE("GPL");

module_init(init_sdcardfs_fs);
module_exit(exit_sdcardfs_fs);
