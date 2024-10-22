// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#include <linux/arm_ffa.h>
#include <linux/errno.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

//#include "../../../../../tee/optee/optee_ffa.h"


#include <linux/version.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <tee_drv.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

#include "soter_private.h"
#include "soter_smc.h"
#include "../tee_private.h"

#define SOTER_SHM_NUM_PRIV_PAGES	1
struct soter_priv *soter_priv;

unsigned long soter_sec_world_id[SOTER_SEC_WORLD_ID_MAX];

struct tee_device *isee_get_teedev(void)
{
	if (soter_priv != NULL)
		return soter_priv->teedev;

	IMSG_ERROR("[%s][%d] soter_priv is NULL!\n", __func__, __LINE__);
	return NULL;
}

static void soter_get_version(struct tee_device *teedev,
				struct tee_ioctl_version_data *vers)
{
	struct tee_ioctl_version_data v = {
		.impl_id = 0x0,
		.impl_caps = 0x0,
		.gen_caps = TEE_GEN_CAP_GP,
	};

	*vers = v;
}

static int soter_open(struct tee_context *ctx)
{
	struct soter_context_data *ctxdata = NULL;
	int retVal = 0;

	ctxdata = kzalloc(sizeof(*ctxdata), GFP_KERNEL);
	if (ctxdata == NULL) {
		IMSG_ERROR("Failed to kzalloc ctxdata!\n");
		return -ENOMEM;
	}

	mutex_init(&ctxdata->mutex);
	INIT_LIST_HEAD(&ctxdata->sess_list);

	ctx->data = ctxdata;

	if (!atomic_read(&is_shm_pool_available)) {
		retVal = wait_for_completion_interruptible(
				&shm_pool_registered);
		if (retVal == -ERESTARTSYS)
			return -EINTR;
	}

	return 0;
}

static void soter_release(struct tee_context *ctx)
{
	struct soter_context_data *ctxdata = ctx->data;
	struct tee_shm *shm;
	struct optee_msg_arg *arg = NULL;
	phys_addr_t parg = 0;
	struct soter_session *sess;
	struct soter_session *sess_tmp;

	if (ctxdata == NULL)
		return;

	shm = isee_shm_alloc(ctx, sizeof(struct optee_msg_arg), TEE_SHM_MAPPED);
	if (!IS_ERR(shm)) {
		arg = isee_shm_get_va(shm, 0);
		/*
		 * If va2pa fails for some reason, we can't call
		 * soter_close_session(), only free the memory. Secure OS
		 * will leak sessions and finally refuse more sessions, but
		 * we will at least let normal world reclaim its memory.
		 */
		if (!IS_ERR(arg))
			isee_shm_va2pa(shm, arg, &parg);
	}

	list_for_each_entry_safe(sess, sess_tmp, &ctxdata->sess_list,
				list_node) {
		list_del(&sess->list_node);
		if (!IS_ERR_OR_NULL(arg)) {
			memset(arg, 0, sizeof(*arg));
			arg->cmd = OPTEE_MSG_CMD_CLOSE_SESSION;
			arg->session = sess->session_id;
			soter_do_call_with_arg(ctx, parg);
		}
		kfree(sess);
	}
	kfree(ctxdata);

	if (!IS_ERR(shm))
		isee_shm_free(shm);

	ctx->data = NULL;
}

void soter_scatterlist_convert(struct scatterlist *s, unsigned long page_link,
				unsigned int length, unsigned int offset)
{
	unsigned long pa = 0;

	pa = (unsigned long)page_to_phys((struct page *)page_link);

	sg_init_marker(s, 1);
	sg_set_page(s, (struct page *)page_link, length, offset);
}


int soter_ffa_shm_register(unsigned long page_link, unsigned int length,
				unsigned int offset, unsigned long *sec_id)
{

	const struct ffa_ops *ffa_ops =  soter_priv->ffa.ffa_ops;
	struct ffa_device *ffa_dev = soter_priv->ffa.ffa_dev;
	struct ffa_mem_region_attributes mem_attr = {
			.receiver = ffa_dev->vm_id,
			.attrs = FFA_MEM_RW,
	};

	struct ffa_mem_ops_args args = {
			.use_txbuf = true,
			.attrs = &mem_attr,
			.nattrs = 1,
	};

	struct scatterlist s;
	int retVal = 0;
	sg_init_table(&s, 1);

	if (soter_priv == NULL) {
		IMSG_ERROR("Failed! soter_priv is NULL\n");
		return -1;
	}

	soter_scatterlist_convert(&s, page_link, length, offset);
	args.sg = &s;

	retVal = ffa_ops->mem_ops->memory_share(&args);
	if (retVal != 0)
		return retVal;

	*sec_id = args.g_handle;

	return 0;
}

#define SOTER_SHM_POOL_SIZE           (PAGE_SIZE * 32)

static struct reserved_mem *teei_find_shm_pool_mblock(void)
{
	struct device_node *mblock_root = NULL;
	struct device_node *pool_node = NULL;
	struct reserved_mem *rmem = NULL;

	mblock_root = of_find_node_by_path("/reserved-memory");
	if (mblock_root == NULL) {
		IMSG_ERROR("TEEI: Failed to get reserved-memory node in DTS\n");
		return NULL;
	}

	pool_node = of_find_compatible_node(mblock_root, NULL,
					"mediatek,security_tee_ree");
	if (pool_node == NULL) {
		IMSG_ERROR("TEEI: Failed to get security_tee_ree in DTS\n");
		return NULL;
	}

	rmem = of_reserved_mem_lookup(pool_node);
	if (rmem == NULL) {
		IMSG_ERROR("TEEI: Failed to find shm pool address\n");
		return NULL;
	}

	return rmem;
}


static struct tee_shm_pool *soter_config_shm_memremap(
				void **memremaped_shm)
{
	struct tee_shm_pool *pool = NULL;
	unsigned long vaddr = 0;
	phys_addr_t paddr = 0;
	size_t size = 0;
	struct tee_shm_pool_mem_info priv_info;
	struct tee_shm_pool_mem_info dmabuf_info;
	void *vpage_addr = NULL;

	reserved_mem = teei_find_shm_pool_mblock();
	if (reserved_mem == NULL) {
		IMSG_ERROR("TEEI: reserved_mem is NULL!\n");
		return NULL;
	}

	paddr = reserved_mem->base;
	size = reserved_mem->size;

	vpage_addr = ioremap_cache(paddr, size);
	if (vpage_addr == NULL) {
		IMSG_ERROR("TEEI: Failed to memremap reserved memory!\n");
		return NULL;
	}

	memset(vpage_addr, 0, size);

	vaddr = (unsigned long)vpage_addr;

	priv_info.vaddr = vaddr;
	priv_info.paddr = paddr;
	priv_info.size = SOTER_SHM_NUM_PRIV_PAGES * PAGE_SIZE;
	dmabuf_info.vaddr = vaddr + SOTER_SHM_NUM_PRIV_PAGES * PAGE_SIZE;
	dmabuf_info.paddr = paddr + SOTER_SHM_NUM_PRIV_PAGES * PAGE_SIZE;
	dmabuf_info.size = size - SOTER_SHM_NUM_PRIV_PAGES * PAGE_SIZE;

	pool = isee_shm_pool_alloc_res_mem(&priv_info, &dmabuf_info);
	if (IS_ERR(pool))
		goto out;

	*memremaped_shm = (void *)vpage_addr;
out:
	return pool;
}

static const struct soter_ffa_ops soter_priv_ffa_ops = {
	.do_call_with_arg = NULL, /* soter_ffa_do_call_with_arg */
};

static struct tee_driver_ops soter_ops = {
	.get_version = soter_get_version,
	.open = soter_open,
	.release = soter_release,
	.open_session = soter_open_session,
	.close_session = soter_close_session,
	.invoke_func = soter_invoke_func,
};

static struct tee_desc soter_desc = {
	.name = "soter-clnt",
	.ops = &soter_ops,
	.owner = THIS_MODULE,
};

static void soter_remove(struct soter_priv *soter)
{
	/*
	 * The device has to be unregistered before we can free the
	 * other resources.
	 */
	isee_device_unregister(soter->teedev);

	isee_shm_pool_free(soter->pool);
	mutex_destroy(&soter->call_queue.mutex);

	kfree(soter);
}

static void teei_ffa_remove(struct ffa_device *ffa_dev)
{
	if (soter_priv)
		soter_remove(soter_priv);

	soter_priv = NULL;
}

static int teei_ffa_probe(struct ffa_device *ffa_dev)
{
	const struct ffa_ops *ffa_ops = NULL;
	struct tee_shm_pool *pool = NULL;
	struct tee_device *teedev = NULL;
	int rc = 0;
	void *memremaped_shm = NULL;

	ffa_ops = ffa_dev->ops;
	if (!ffa_ops) {
		IMSG_ERROR("Failed to get ffa_ops\n");
		return -ENOENT;
	}

	soter_priv = kzalloc(sizeof(*soter_priv), GFP_KERNEL);
	if (!soter_priv) {
		IMSG_ERROR("TEEI: Failed to alloc soter_priv\n");
		return -ENOMEM;
	}

	pool = soter_config_shm_memremap(&memremaped_shm);
	if (IS_ERR(pool)) {
		IMSG_ERROR("TEEI: soter_config_shm_memremap failed, rc = %ld\n",
				PTR_ERR(pool));
		return PTR_ERR(pool);
	}

	soter_priv->pool = pool;
	soter_priv->memremaped_shm = memremaped_shm;
	soter_priv->ops = soter_priv_ffa_ops;
	soter_priv->ffa.ffa_dev = ffa_dev;
	soter_priv->ffa.ffa_ops = ffa_ops;

	teedev = isee_device_alloc(&soter_desc, NULL, pool, soter_priv);
	if (IS_ERR(teedev)) {
		rc = PTR_ERR(teedev);
		IMSG_ERROR("TEEI: isee_device_alloc failed, rc = %d\n", rc);
		goto err;
	}

	soter_priv->teedev = teedev;

	rc = isee_device_register(teedev);
	if (rc != 0) {
		IMSG_ERROR("TEEI: isee_device_register failed, rc = %d\n", rc);
		goto err;
	}

	return rc;
err:
	if (soter_priv != NULL) {
		/*
		 * isee_device_unregister() is safe to call even if the
		 * devices hasn't been registered with
		 * tee_device_register() yet.
		 */
		isee_device_unregister(soter_priv->teedev);
		kfree(soter_priv);
	}

	if (pool)
		isee_shm_pool_free(pool);

	return rc;
}

static const struct ffa_device_id teei_ffa_device_id[] = {
	/* c3ccf08b-7152-a342-58660d8690476904 */
	{ UUID_INIT(0x8bf0ccc3, 0x42a3, 0x5271,
			0x86, 0x0d, 0x66, 0x58, 0x04, 0x69, 0x47, 0x90) },

	{}
};

static struct ffa_driver teei_ffa_driver = {
	.name = "isee_ffa",
	.probe = teei_ffa_probe,
	.remove = teei_ffa_remove,
	.id_table = teei_ffa_device_id,
};

int teei_ffa_abi_register(void)
{
	int retVal = 0;

	if (IS_REACHABLE(CONFIG_ARM_FFA_TRANSPORT))
		retVal = ffa_register(&teei_ffa_driver);
	else
		retVal = -EOPNOTSUPP;

	return retVal;
}

void teei_ffa_abi_unregister(void)
{
	if (IS_REACHABLE(CONFIG_ARM_FFA_TRANSPORT))
		ffa_unregister(&teei_ffa_driver);
}
