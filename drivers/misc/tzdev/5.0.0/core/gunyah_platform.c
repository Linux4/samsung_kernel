/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/io.h>
#include <linux/sizes.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/smp.h>
#include <linux/gunyah/gh_dbl.h>
#include <linux/gunyah/gh_mem_notifier.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <soc/qcom/secure_buffer.h>

#include "tzdev_internal.h"
#include "core/gunyah_platform.h"
#include "core/kthread_pool.h"
#include "core/log.h"
#include "core/tvm_shmem.h"

#define NW_SYSCALL_DBL_MASK	(1 << 12)
#define PING_DBL_MASK		(1 << 12)

#define VALID_FLAG	UL(1 << 0)
#define TVM_READY	UL(1 << 0)
#define TVM_INIT_FAIL	UL(1 << 1)
#define RESP_READY	UL(1 << 2)

struct pvm_data {
	uint64_t flags;

	uint64_t sys_no;
	uint64_t p1;
	uint64_t p2;
	uint64_t p3;
	uint64_t p4;
	uint64_t p5;
	uint64_t p6;
};

struct tvm_data {
	uint64_t flags;

	uint64_t a0;
	uint64_t a1;
	uint64_t a2;
	uint64_t a3;
};

/* Currently, only one CPU is supported */
struct iwd_buffer {
	struct tvm_data tvm[NR_SW_CPU_IDS];
	struct pvm_data pvm[NR_SW_CPU_IDS];
};

struct gh_data {
	struct device *dev;

	struct resource res;
	struct iwd_buffer *buffer;

	u32 label;
	u32 ping_label;
	u32 memparcel;
	u32 peer_name;
	struct notifier_block rm_nb;

	void *tx_dbl;
	void *rx_dbl;
	void *rx_ping_dbl;
};

static struct gh_data gh_data;
static struct work_struct init_work;
static DECLARE_COMPLETION(nw_syscall_compl);
static DEFINE_SPINLOCK(flags_lock);
static DEFINE_MUTEX(smc_lock); /* Currently, only one SMC at the same time is supported */

static inline struct tvm_data *get_tvm_data(void)
{
	return &gh_data.buffer->tvm[0];
}

static inline struct pvm_data *get_pvm_data(void)
{
	return &gh_data.buffer->pvm[0];
}

int gunyah_get_pvm_vmid(void)
{
	gh_vmid_t pvm_vmid;

	gh_rm_get_vmid(GH_PRIMARY_VM, &pvm_vmid);
	return (int)pvm_vmid;
}

int gunyah_get_tvm_vmid(void)
{
	gh_vmid_t tvm_vmid;

	gh_rm_get_vmid(GH_OEM_VM, &tvm_vmid);
	return (int)tvm_vmid;
}

static struct gh_sgl_desc *get_sgl_desc(struct sg_table *sgt)
{
	int i;
	struct scatterlist *s;
	struct gh_sgl_desc *sgl;

	sgl = kzalloc(offsetof(struct gh_sgl_desc, sgl_entries[sgt->nents]), GFP_KERNEL);
	if (!sgl) {
		log_error(tzdev_platform, "Failed to allocate memory for SGL\n");
		return ERR_PTR(-ENOMEM);
	}

	sgl->n_sgl_entries = sgt->nents;
	for_each_sg(sgt->sgl, s, sgt->nents, i) {
		sgl->sgl_entries[i].ipa_base = sg_phys(s);
		sgl->sgl_entries[i].size = s->length;
		log_debug(tzdev_platform, "sgl[%d]: phys: %#llx; len: %u\n",
				i, sg_phys(s), s->length);
	}

	return sgl;
}

static struct gh_acl_desc *get_acl_desc(void)
{
	struct gh_acl_desc *acl;

	acl = kzalloc(offsetof(struct gh_acl_desc, acl_entries[2]), GFP_KERNEL);
	if (!acl) {
		log_error(tzdev_platform, "Failed to allocate memory for ACL\n");
		return ERR_PTR(-ENOMEM);
	}

	acl->n_acl_entries = 2;

	acl->acl_entries[0].vmid = gunyah_get_pvm_vmid();
	acl->acl_entries[0].perms = GH_RM_ACL_R | GH_RM_ACL_W;

	acl->acl_entries[1].vmid = gunyah_get_tvm_vmid();
	acl->acl_entries[1].perms = GH_RM_ACL_R | GH_RM_ACL_W;

	return acl;
}

int gunyah_share_memory(struct sg_table *sgt, uint32_t *mem_handle)
{
	int ret;
	struct gh_acl_desc *acl;
	struct gh_sgl_desc *sgl;

	if (!sgt || !sgt->sgl || !sgt->nents) {
		log_error(tzdev_platform, "Invalid arguments\n");
		return -EINVAL;
	}

	acl = get_acl_desc();
	if (IS_ERR(acl)) {
		log_error(tzdev_platform, "Failed to allocate memory for ACL\n");
		return PTR_ERR(acl);
	}

	sgl = get_sgl_desc(sgt);
	if (IS_ERR(sgl)) {
		log_error(tzdev_platform, "Failed to allocate memory for SG list\n");
		kfree(acl);
		return PTR_ERR(sgl);
	}

	ret = gh_rm_mem_share(GH_RM_MEM_TYPE_NORMAL, 0, 0, acl, sgl, NULL, mem_handle);
	if (ret) {
		log_error(tzdev_platform, "Failed to get mem handle: %d\n", ret);
	}

	kfree(acl);
	kfree(sgl);

	return ret;
}

int gunyah_mem_notify(uint32_t mem_handle)
{
	int ret;
	struct gh_notify_vmid_desc *vmid_desc;

	vmid_desc = kzalloc(offsetof(struct gh_notify_vmid_desc, vmid_entries[1]), GFP_KERNEL);
	if (!vmid_desc) {
		log_error(tzdev_platform, "Failed to get vmid desc: %d\n", PTR_ERR(vmid_desc));
		return -ENOMEM;
	}

	vmid_desc->n_vmid_entries = 1;
	vmid_desc->vmid_entries[0].vmid = gunyah_get_tvm_vmid();

	ret = gh_rm_mem_notify(mem_handle, GH_RM_MEM_NOTIFY_RECIPIENT_SHARED,
			GH_MEM_NOTIFIER_TAG_TEEGRIS, vmid_desc);
	if (ret) {
		log_error(tzdev_platform, "Failed to notify TVM about memory sharing: %d\n", ret);
	}

	kfree(vmid_desc);

	return ret;
}

static int tzdev_gunyah_kick(struct tzdev_smc_data *data)
{
	int ret;
	gh_dbl_flags_t dbl_mask = NW_SYSCALL_DBL_MASK;
	struct tvm_data *tvm = get_tvm_data();
	struct pvm_data *pvm = get_pvm_data();

	mutex_lock(&smc_lock);
	pvm->flags |= VALID_FLAG;
	pvm->sys_no = data->args[0] & 0xffff;
	pvm->p1 = data->args[1];
	pvm->p2 = data->args[2];
	pvm->p3 = data->args[3];
	pvm->p4 = data->args[4];
	pvm->p5 = data->args[5];
	pvm->p6 = data->args[6];

	reinit_completion(&nw_syscall_compl);

	ret = gh_dbl_send(gh_data.tx_dbl, &dbl_mask, 0);
	if (ret) {
		log_error(tzdev_platform, "failed to raise doorbell %d\n", ret);
		return ret;
	}

	/* Wait for response from TVM */
	wait_for_completion(&nw_syscall_compl);

	data->args[0] = tvm->a0;
	data->args[1] = tvm->a1;
	data->args[2] = tvm->a2;
	data->args[3] = tvm->a3;
	mutex_unlock(&smc_lock);

	return 0;
}

static void gunyah_shmem_notifier(enum gh_mem_notifier_tag tag, unsigned long notif_type,
		void *entry_data, void *notif_msg)
{
	struct gh_rm_notif_mem_accepted_payload *payload =
		(struct gh_rm_notif_mem_accepted_payload *)notif_msg;
	(void)entry_data;

	if (!payload || tag != GH_MEM_NOTIFIER_TAG_TEEGRIS) {
		log_error(tzdev_platform, "Wrong data in notifier\n");
	}

	if (notif_type == GH_RM_NOTIF_MEM_ACCEPTED) {
		tvm_handle_accepted(payload->mem_handle);
	} else if (notif_type == GH_RM_NOTIF_MEM_RELEASED) {
		tvm_handle_released(payload->mem_handle);
	} else {
		log_error(tzdev_platform, "Wrong notifier type: %#lx\n", notif_type);
	}
}

static void gh_tzdev_init(struct work_struct *work)
{
	int ret;
	void *cookie;

	ret = gh_dbl_set_mask(gh_data.rx_dbl, NW_SYSCALL_DBL_MASK, NW_SYSCALL_DBL_MASK, 0);
	if (ret)
		log_error(tzdev_platform, "Failed to set masks for RX doorbell\n");

	ret = gh_dbl_set_mask(gh_data.rx_ping_dbl, PING_DBL_MASK, PING_DBL_MASK, 0);
	if (ret)
		log_error(tzdev_platform, "Failed to set masks for ping RX doorbell\n");

	cookie = gh_mem_notifier_register(GH_MEM_NOTIFIER_TAG_TEEGRIS, gunyah_shmem_notifier, NULL);
	if (IS_ERR(cookie)) {
		log_error(tzdev_platform, "Failed to register gunyah memory notifier, error = %d\n",
				PTR_ERR(cookie));
		return;
	}

	ret = tzdev_run_init_sequence();
	if (ret) {
		log_error(tzdev_platform, "tzdev initialization failed\n");
		return;
	}

	log_debug(tzdev_platform, "tzdev initialization done.\n");
}

static void gh_tzdev_cb(int irq, void *data)
{
	struct tvm_data *tvm = get_tvm_data();

	spin_lock(&flags_lock);
	if (tvm->flags & TVM_READY) {
		tvm->flags &= ~TVM_READY;
		log_debug(tzdev_platform, "TVM_READY\n");
		INIT_WORK(&init_work, gh_tzdev_init);
		schedule_work(&init_work);
	} else if (tvm->flags & RESP_READY) {
		tvm->flags &= ~RESP_READY;
		log_debug(tzdev_platform, "RESP_READY\n");
		complete(&nw_syscall_compl);
	} else {
		log_error(tzdev_platform, "Failed to initialize TVM\n");
	}
	spin_unlock(&flags_lock);
}

static void gh_tzdev_ping(int irq, void *data)
{
	log_debug(tzdev_platform, "Ping from TVM\n");
	tz_kthread_pool_cmd_send();
}

static int gh_tzdev_share_mem(gh_vmid_t self, gh_vmid_t peer)
{
	u32 src_vmlist[1] = {self};
	int dst_vmlist[2] = {self, peer};
	int dst_perms[2] = {PERM_READ | PERM_WRITE, PERM_READ | PERM_WRITE};
	struct gh_acl_desc *acl;
	struct gh_sgl_desc *sgl;
	int ret;

	ret = hyp_assign_phys(gh_data.res.start, resource_size(&gh_data.res),
				src_vmlist, 1,
				dst_vmlist, dst_perms, 2);

	if (ret) {
		log_error(tzdev_platform, "hyp_assign_phys failed addr=%x size=%u err=%d\n",
				gh_data.res.start,
				resource_size(&gh_data.res), ret);
		return ret;
	}

	acl = kzalloc(offsetof(struct gh_acl_desc, acl_entries[2]), GFP_KERNEL);
	if (!acl)
		return -ENOMEM;

	sgl = kzalloc(offsetof(struct gh_sgl_desc, sgl_entries[1]), GFP_KERNEL);
	if (!sgl) {
		kfree(acl);
		return -ENOMEM;
	}

	acl->n_acl_entries = 2;
	acl->acl_entries[0].vmid = (u16)self;
	acl->acl_entries[0].perms = GH_RM_ACL_R | GH_RM_ACL_W;
	acl->acl_entries[1].vmid = (u16)peer;
	acl->acl_entries[1].perms = GH_RM_ACL_R | GH_RM_ACL_W;

	sgl->n_sgl_entries = 1;
	sgl->sgl_entries[0].ipa_base = gh_data.res.start;
	sgl->sgl_entries[0].size = resource_size(&gh_data.res);

	ret = gh_rm_mem_share(GH_RM_MEM_TYPE_NORMAL,
						0, gh_data.label,
						acl, sgl, NULL, &gh_data.memparcel);

	if (ret)
		log_error(tzdev_platform, "gh_rm_mem_share failed ret = %d\n", ret);

	kfree(acl);
	kfree(sgl);

	return ret;
}

static int gh_tzdev_rm_cb(struct notifier_block *nb, unsigned long cmd,
			    void *data)
{
	struct gh_rm_notif_vm_status_payload *vm_status_payload;
	gh_vmid_t peer_vmid;
	gh_vmid_t self_vmid;

	if (cmd != GH_RM_NOTIF_VM_STATUS)
		return NOTIFY_DONE;

	vm_status_payload = data;
	if (vm_status_payload->vm_status != GH_RM_VM_STATUS_READY)
		return NOTIFY_DONE;
	if (gh_rm_get_vmid(gh_data.peer_name, &peer_vmid))
		return NOTIFY_DONE;
	if (gh_rm_get_vmid(GH_PRIMARY_VM, &self_vmid))
		return NOTIFY_DONE;
	if (peer_vmid != vm_status_payload->vmid)
		return NOTIFY_DONE;

	if (gh_tzdev_share_mem(self_vmid, peer_vmid))
		log_error(tzdev_platform, "failed to share memory\n");

	return NOTIFY_DONE;
}

static int setup_iwd_buffer(void)
{
	struct device *dev = gh_data.dev;
	struct device_node *np;
	int ret;

	np = of_parse_phandle(dev->of_node, "shared-buffer", 0);
	if (!np) {
		log_error(tzdev_platform, "Can't parse shared-buffer node\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(np, 0, &gh_data.res);
	of_node_put(np);
	if (ret) {
		log_error(tzdev_platform, "of_address_to_resource failed\n");
		return -EINVAL;
	}

	gh_data.buffer = devm_ioremap_resource(dev, &gh_data.res);
	if (!gh_data.buffer) {
		log_error(tzdev_platform, "devm_ioremap_nocache failed\n");
		return -ENXIO;
	}

	memset(gh_data.buffer, 0, sizeof(struct iwd_buffer));

	return 0;
}

static int tzdev_gunyah_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node = pdev->dev.of_node;

	ret = of_property_read_u32(node, "gunyah-label", &gh_data.label);
	if (ret) {
		log_error(tzdev_platform, "failed to read label info %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(node, "gunyah-ping-label", &gh_data.ping_label);
	if (ret) {
		log_error(tzdev_platform, "failed to read ping label info %d\n", ret);
		return ret;
	}

	gh_data.tx_dbl = gh_dbl_tx_register(gh_data.label);
	if (IS_ERR_OR_NULL(gh_data.tx_dbl)) {
		ret = PTR_ERR(gh_data.tx_dbl);
		log_error(tzdev_platform, "failed to get gunyah tx dbl %d\n", ret);
		return ret;
	}

	gh_data.rx_dbl = gh_dbl_rx_register(gh_data.label, gh_tzdev_cb, NULL);
	if (IS_ERR_OR_NULL(gh_data.rx_dbl)) {
		ret = PTR_ERR(gh_data.rx_dbl);
		log_error(tzdev_platform, "failed to get gunyah rx dbl %d\n", ret);
		goto tx_dbl_unregister;
	}

	gh_data.rx_ping_dbl = gh_dbl_rx_register(gh_data.ping_label, gh_tzdev_ping, NULL);
	if (IS_ERR_OR_NULL(gh_data.rx_ping_dbl)) {
		ret = PTR_ERR(gh_data.rx_ping_dbl);
		log_error(tzdev_platform, "failed to get ping rx dbl %d\n", ret);
		goto rx_dbl_unregister;
	}

	gh_data.dev = &pdev->dev;

	ret = setup_iwd_buffer();
	if (ret) {
		log_error(tzdev_platform, "failed to map memory %d\n", ret);
		goto ping_dbl_unregister;
	}

	ret = of_property_read_u32(node, "peer-name", &gh_data.peer_name);
	if (ret)
		gh_data.peer_name = GH_SELF_VM;

	gh_data.rm_nb.notifier_call = gh_tzdev_rm_cb;
	gh_data.rm_nb.priority = INT_MAX;
	gh_rm_register_notifier(&gh_data.rm_nb);

	return 0;

ping_dbl_unregister:
	gh_dbl_rx_unregister(gh_data.rx_ping_dbl);
	gh_data.rx_ping_dbl = NULL;

rx_dbl_unregister:
	gh_dbl_rx_unregister(gh_data.rx_dbl);
	gh_data.rx_dbl = NULL;

tx_dbl_unregister:
	gh_dbl_tx_unregister(gh_data.tx_dbl);
	gh_data.tx_dbl = NULL;

	return ret;
}

static int tzdev_gunyah_remove(struct platform_device *pdev)
{
	gh_dbl_tx_unregister(gh_data.tx_dbl);
	gh_dbl_rx_unregister(gh_data.rx_dbl);

	tzdev_run_fini_sequence();

	return 0;
}

static const struct of_device_id tzdev_gunyah_match_table[] = {
	{ .compatible = "samsung,tzdev-gunyah" },
	{}
};

static struct platform_driver tzdev_gunyah_driver = {
	.probe = tzdev_gunyah_probe,
	.remove = tzdev_gunyah_remove,

	.driver = {
		.name = "tzdev_gunyah",
		.owner = THIS_MODULE,
		.of_match_table = tzdev_gunyah_match_table,
	 },
};

int tzdev_platform_register(void)
{
	return platform_driver_register(&tzdev_gunyah_driver);
}

int tzdev_platform_smc_call(struct tzdev_smc_data *data)
{
	return tzdev_gunyah_kick(data);
}

void tzdev_platform_unregister(void)
{
	platform_driver_unregister(&tzdev_gunyah_driver);
}

void *tzdev_platform_open(void)
{
	return NULL;
}

void tzdev_platform_release(void *data)
{
}

long tzdev_platform_ioctl(void *data, unsigned int cmd, unsigned long arg)
{
	(void) data;
	(void) cmd;
	(void) arg;

	return -ENOTTY;
}
