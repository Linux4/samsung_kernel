/* drivers/input/sec_input/stm/nvt_core.c
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "nvt_dev.h"
#include "nvt_reg.h"

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#if IS_ENABLED(CONFIG_GH_RM_DRV)
static void nvt_ts_trusted_touch_abort_handler(struct nvt_ts_data *ts,
						int error);
static struct gh_acl_desc *nvt_ts_vm_get_acl(enum gh_vm_names vm_name)
{
	struct gh_acl_desc *acl_desc;
	gh_vmid_t vmid;

	gh_rm_get_vmid(vm_name, &vmid);

	acl_desc = kzalloc(offsetof(struct gh_acl_desc, acl_entries[1]),
			GFP_KERNEL);
	if (!acl_desc)
		return ERR_PTR(ENOMEM);

	acl_desc->n_acl_entries = 1;
	acl_desc->acl_entries[0].vmid = vmid;
	acl_desc->acl_entries[0].perms = GH_RM_ACL_R | GH_RM_ACL_W;

	return acl_desc;
}

static struct gh_sgl_desc *nvt_ts_vm_get_sgl(
				struct trusted_touch_vm_info *vm_info)
{
	struct gh_sgl_desc *sgl_desc;
	int i;

	sgl_desc = kzalloc(offsetof(struct gh_sgl_desc,
			sgl_entries[vm_info->iomem_list_size]), GFP_KERNEL);
	if (!sgl_desc)
		return ERR_PTR(ENOMEM);

	sgl_desc->n_sgl_entries = vm_info->iomem_list_size;

	for (i = 0; i < vm_info->iomem_list_size; i++) {
		sgl_desc->sgl_entries[i].ipa_base = vm_info->iomem_bases[i];
		sgl_desc->sgl_entries[i].size = vm_info->iomem_sizes[i];
	}

	return sgl_desc;
}

static int nvt_ts_populate_vm_info(struct nvt_ts_data *ts)
{
	int rc = 0;
	struct trusted_touch_vm_info *vm_info;
	struct device_node *np = ts->client->dev.of_node;
	int num_regs, num_sizes = 0;

	vm_info = kzalloc(sizeof(struct trusted_touch_vm_info), GFP_KERNEL);
	if (!vm_info) {
		rc = -ENOMEM;
		goto error;
	}

	ts->vm_info = vm_info;
	vm_info->vm_name = GH_TRUSTED_VM;
	rc = of_property_read_u32(np, "trusted-touch-spi-irq", &vm_info->hw_irq);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to read trusted touch SPI irq:%d\n", rc);
		goto vm_error;
	}
	num_regs = of_property_count_u32_elems(np, "trusted-touch-io-bases");
	if (num_regs < 0) {
		input_err(true, &ts->client->dev, "Invalid number of IO regions specified\n");
		rc = -EINVAL;
		goto vm_error;
	}

	num_sizes = of_property_count_u32_elems(np, "trusted-touch-io-sizes");
	if (num_sizes < 0) {
		input_err(true, &ts->client->dev, "Invalid number of IO regions specified\n");
		rc = -EINVAL;
		goto vm_error;
	}

	if (num_regs != num_sizes) {
		input_err(true, &ts->client->dev, "IO bases and sizes doe not match\n");
		rc = -EINVAL;
		goto vm_error;
	}

	vm_info->iomem_list_size = num_regs;

	vm_info->iomem_bases = kcalloc(num_regs, sizeof(*vm_info->iomem_bases), GFP_KERNEL);
	if (!vm_info->iomem_bases) {
		rc = -ENOMEM;
		goto vm_error;
	}

	rc = of_property_read_string(np, "trusted-touch-type",
						&vm_info->trusted_touch_type);
	if (rc) {
		pr_warn("%s: No trusted touch type selection made\n", __func__);
		vm_info->mem_tag = GH_MEM_NOTIFIER_TAG_TOUCH_PRIMARY;
		vm_info->irq_label = GH_IRQ_LABEL_TRUSTED_TOUCH_PRIMARY;
		rc = 0;
	} else if (!strcmp(vm_info->trusted_touch_type, "primary")) {
		vm_info->mem_tag = GH_MEM_NOTIFIER_TAG_TOUCH_PRIMARY;
		vm_info->irq_label = GH_IRQ_LABEL_TRUSTED_TOUCH_PRIMARY;
	} else if (!strcmp(vm_info->trusted_touch_type, "secondary")) {
		vm_info->mem_tag = GH_MEM_NOTIFIER_TAG_TOUCH_SECONDARY;
		vm_info->irq_label = GH_IRQ_LABEL_TRUSTED_TOUCH_SECONDARY;
	}

	rc = of_property_read_string(np, "trusted-touch-type",
						&vm_info->trusted_touch_type);
	if (rc)
		pr_warn("%s: No trusted touch type selection made\n", __func__);

	rc = of_property_read_u32_array(np, "trusted-touch-io-bases",
			vm_info->iomem_bases, vm_info->iomem_list_size);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to read trusted touch io bases:%d\n", rc);
		goto io_bases_error;
	}

	vm_info->iomem_sizes = kzalloc(
			sizeof(*vm_info->iomem_sizes) * num_sizes, GFP_KERNEL);
	if (!vm_info->iomem_sizes) {
		rc = -ENOMEM;
		goto io_bases_error;
	}

	rc = of_property_read_u32_array(np, "trusted-touch-io-sizes",
			vm_info->iomem_sizes, vm_info->iomem_list_size);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to read trusted touch io sizes:%d\n", rc);
		goto io_sizes_error;
	}
	return rc;

io_sizes_error:
	kfree(vm_info->iomem_sizes);
io_bases_error:
	kfree(vm_info->iomem_bases);
vm_error:
	kfree(vm_info);
error:
	return rc;
}

static void nvt_ts_destroy_vm_info(struct nvt_ts_data *ts)
{
	kfree(ts->vm_info->iomem_sizes);
	kfree(ts->vm_info->iomem_bases);
	kfree(ts->vm_info);
}

static void nvt_ts_vm_deinit(struct nvt_ts_data *ts)
{
	if (ts->vm_info->mem_cookie)
		gh_mem_notifier_unregister(ts->vm_info->mem_cookie);
	nvt_ts_destroy_vm_info(ts);
}

#if IS_ENABLED(CONFIG_ARCH_QTI_VM)
static int nvt_ts_vm_mem_release(struct nvt_ts_data *ts);
static void nvt_ts_trusted_touch_tvm_vm_mode_disable(struct nvt_ts_data *ts);
static void nvt_ts_trusted_touch_abort_tvm(struct nvt_ts_data *ts);
static void nvt_ts_trusted_touch_event_notify(struct nvt_ts_data *ts, int event);

void nvt_ts_trusted_touch_tvm_i2c_failure_report(struct nvt_ts_data *ts)
{
	input_err(true, &ts->client->dev, "initiating trusted touch abort due to i2c failure\n");
	nvt_ts_trusted_touch_abort_handler(ts,
			TRUSTED_TOUCH_EVENT_I2C_FAILURE);
}

static void nvt_trusted_touch_intr_gpio_toggle(struct nvt_ts_data *ts,
		bool enable)
{
	void __iomem *base;
	u32 val = 0;

	base = ioremap(TOUCH_INTR_GPIO_BASE, TOUCH_INTR_GPIO_SIZE);
	if (enable) {
		val |= BIT(0);
		writel_relaxed(val, base + TOUCH_INTR_GPIO_OFFSET);
		/* wait until toggle to finish*/
		wmb();
	} else {
		val &= ~BIT(0);
		writel_relaxed(val, base + TOUCH_INTR_GPIO_OFFSET);
		/* wait until toggle to finish*/
		wmb();
	}
	iounmap(base);
}

static int nvt_ts_trusted_touch_get_tvm_driver_state(struct nvt_ts_data *ts)
{
	return atomic_read(&ts->vm_info->vm_state);
}

static void nvt_ts_trusted_touch_set_tvm_driver_state(struct nvt_ts_data *ts,
						int state)
{
	atomic_set(&ts->vm_info->vm_state, state);
}

static int nvt_ts_sgl_cmp(const void *a, const void *b)
{
	struct gh_sgl_entry *left = (struct gh_sgl_entry *)a;
	struct gh_sgl_entry *right = (struct gh_sgl_entry *)b;

	return (left->ipa_base - right->ipa_base);
}

static int nvt_ts_vm_compare_sgl_desc(struct gh_sgl_desc *expected,
		struct gh_sgl_desc *received)
{
	int idx;

	if (expected->n_sgl_entries != received->n_sgl_entries)
		return -E2BIG;
	sort(received->sgl_entries, received->n_sgl_entries,
			sizeof(received->sgl_entries[0]), nvt_ts_sgl_cmp, NULL);
	sort(expected->sgl_entries, expected->n_sgl_entries,
			sizeof(expected->sgl_entries[0]), nvt_ts_sgl_cmp, NULL);

	for (idx = 0; idx < expected->n_sgl_entries; idx++) {
		struct gh_sgl_entry *left = &expected->sgl_entries[idx];
		struct gh_sgl_entry *right = &received->sgl_entries[idx];

		if ((left->ipa_base != right->ipa_base) ||
				(left->size != right->size)) {
			pr_err("%s: sgl mismatch: left_base:%lld right base:%lld left size:%lld right size:%lld\n",
					__func__, left->ipa_base, right->ipa_base,
					left->size, right->size);
			return -EINVAL;
		}
	}
	return 0;
}

static int nvt_ts_vm_handle_vm_hardware(struct nvt_ts_data *ts)
{
	int rc = 0;

	enable_irq(ts->irq);
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TVM_INTERRUPT_ENABLED);
	return rc;
}

static void nvt_ts_trusted_touch_tvm_vm_mode_enable(struct nvt_ts_data *ts)
{

	struct gh_sgl_desc *sgl_desc, *expected_sgl_desc;
	struct gh_acl_desc *acl_desc;
	struct irq_data *irq_data;
	int rc = 0;
	int irq = 0;

	if (nvt_ts_trusted_touch_get_tvm_driver_state(ts) !=
					TVM_ALL_RESOURCES_LENT_NOTIFIED) {
		input_err(true, &ts->client->dev, "All lend notifications not received\n");
		nvt_ts_trusted_touch_event_notify(ts,
				TRUSTED_TOUCH_EVENT_NOTIFICATIONS_PENDING);
		return;
	}

	acl_desc = nvt_ts_vm_get_acl(GH_TRUSTED_VM);
	if (IS_ERR(acl_desc)) {
		input_err(true, &ts->client->dev, "failed to populated acl data:rc=%ld\n",
				PTR_ERR(acl_desc));
		goto accept_fail;
	}

	sgl_desc = gh_rm_mem_accept(ts->vm_info->vm_mem_handle,
			GH_RM_MEM_TYPE_IO,
			GH_RM_TRANS_TYPE_LEND,
			GH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS |
			GH_RM_MEM_ACCEPT_VALIDATE_LABEL |
			GH_RM_MEM_ACCEPT_DONE,  TRUSTED_TOUCH_MEM_LABEL,
			acl_desc, NULL, NULL, 0);
	if (IS_ERR_OR_NULL(sgl_desc)) {
		input_err(true, &ts->client->dev, "failed to do mem accept :rc=%ld\n",
				PTR_ERR(sgl_desc));
		goto acl_fail;
	}
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TVM_IOMEM_ACCEPTED);

	/* Initiate session on tvm */
	rc = pm_runtime_get_sync(ts->client->controller->dev.parent);

	if (rc < 0) {
		input_err(true, &ts->client->dev, "failed to get sync rc:%d\n", rc);
		goto acl_fail;
	}
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TVM_I2C_SESSION_ACQUIRED);

	expected_sgl_desc = nvt_ts_vm_get_sgl(ts->vm_info);
	if (nvt_ts_vm_compare_sgl_desc(expected_sgl_desc, sgl_desc)) {
		input_err(true, &ts->client->dev, "IO sg list does not match\n");
		goto sgl_cmp_fail;
	}

	kfree(expected_sgl_desc);
	kfree(acl_desc);

	irq = gh_irq_accept(ts->vm_info->irq_label, -1, IRQ_TYPE_EDGE_RISING);
	nvt_trusted_touch_intr_gpio_toggle(ts, false);
	if (irq < 0) {
		input_err(true, &ts->client->dev, "failed to accept irq\n");
		goto accept_fail;
	}
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TVM_IRQ_ACCEPTED);


	irq_data = irq_get_irq_data(irq);
	if (!irq_data) {
		input_err(true, &ts->client->dev, "Invalid irq data for trusted touch\n");
		goto accept_fail;
	}
	if (!irq_data->hwirq) {
		input_err(true, &ts->client->dev, "Invalid irq in irq data\n");
		goto accept_fail;
	}
	if (irq_data->hwirq != ts->vm_info->hw_irq) {
		input_err(true, &ts->client->dev, "Invalid irq lent\n");
		goto accept_fail;
	}

	input_err(true, &ts->client->dev, "irq:returned from accept:%d\n", irq);
	ts->irq = irq;

	rc = nvt_ts_vm_handle_vm_hardware(ts);
	if (rc) {
		input_err(true, &ts->client->dev, " Delayed probe failure on VM!\n");
		goto accept_fail;
	}
	atomic_set(&ts->trusted_touch_enabled, 1);
	input_err(true, &ts->client->dev, " trusted touch enabled\n");

	return;
sgl_cmp_fail:
	kfree(expected_sgl_desc);
acl_fail:
	kfree(acl_desc);
accept_fail:
	nvt_ts_trusted_touch_abort_handler(ts,
			TRUSTED_TOUCH_EVENT_ACCEPT_FAILURE);
}
static void nvt_ts_vm_irq_on_lend_callback(void *data,
					unsigned long notif_type,
					enum gh_irq_label label)
{
	struct nvt_ts_data *ts = data;

	input_err(true, &ts->client->dev, "received irq lend request for label:%d\n", label);
	if (nvt_ts_trusted_touch_get_tvm_driver_state(ts) ==
		TVM_IOMEM_LENT_NOTIFIED) {
		nvt_ts_trusted_touch_set_tvm_driver_state(ts,
		TVM_ALL_RESOURCES_LENT_NOTIFIED);
	} else {
		nvt_ts_trusted_touch_set_tvm_driver_state(ts,
			TVM_IRQ_LENT_NOTIFIED);
	}
}

static void nvt_ts_vm_mem_on_lend_handler(enum gh_mem_notifier_tag tag,
		unsigned long notif_type, void *entry_data, void *notif_msg)
{
	struct gh_rm_notif_mem_shared_payload *payload;
	struct trusted_touch_vm_info *vm_info;
	struct nvt_ts_data *ts;

	ts = (struct nvt_ts_data *)entry_data;
	vm_info = ts->vm_info;
	if (!vm_info) {
		input_err(true, &ts->client->dev, "Invalid vm_info\n");
		return;
	}

	if (notif_type != GH_RM_NOTIF_MEM_SHARED ||
			tag != vm_info->mem_tag) {
		input_err(true, &ts->client->dev, "Invalid command passed from rm\n");
		return;
	}

	if (!entry_data || !notif_msg) {
		input_err(true, &ts->client->dev, "Invalid entry data passed from rm\n");
		return;
	}


	payload = (struct gh_rm_notif_mem_shared_payload  *)notif_msg;
	if (payload->trans_type != GH_RM_TRANS_TYPE_LEND ||
			payload->label != TRUSTED_TOUCH_MEM_LABEL) {
		input_err(true, &ts->client->dev, "Invalid label or transaction type\n");
		return;
	}

	vm_info->vm_mem_handle = payload->mem_handle;
	input_err(true, &ts->client->dev, "received mem lend request with handle:%d\n",
		vm_info->vm_mem_handle);
	if (nvt_ts_trusted_touch_get_tvm_driver_state(ts) ==
		TVM_IRQ_LENT_NOTIFIED) {
		nvt_ts_trusted_touch_set_tvm_driver_state(ts,
			TVM_ALL_RESOURCES_LENT_NOTIFIED);
	} else {
		nvt_ts_trusted_touch_set_tvm_driver_state(ts,
		TVM_IOMEM_LENT_NOTIFIED);
	}
}

static int nvt_ts_vm_mem_release(struct nvt_ts_data *ts)
{
	int rc = 0;

	if (!ts->vm_info->vm_mem_handle) {
		input_err(true, &ts->client->dev, "Invalid memory handle\n");
		return -EINVAL;
	}

	rc = gh_rm_mem_release(ts->vm_info->vm_mem_handle, 0);
	if (rc)
		input_err(true, &ts->client->dev, "VM mem release failed: rc=%d\n", rc);

	rc = gh_rm_mem_notify(ts->vm_info->vm_mem_handle,
				GH_RM_MEM_NOTIFY_OWNER_RELEASED,
				ts->vm_info->mem_tag, 0);
	if (rc)
		input_err(true, &ts->client->dev, "Failed to notify mem release to PVM: rc=%d\n", rc);
	input_err(true, &ts->client->dev, "vm mem release succeded\n");

	ts->vm_info->vm_mem_handle = 0;
	return rc;
}

static void nvt_ts_trusted_touch_tvm_vm_mode_disable(struct nvt_ts_data *ts)
{
	int rc = 0;

	atomic_set(&ts->trusted_touch_transition, 1);

	if (atomic_read(&ts->trusted_touch_abort_status)) {
		nvt_ts_trusted_touch_abort_tvm(ts);
		return;
	}

	disable_irq(ts->irq);

	nvt_ts_trusted_touch_set_tvm_driver_state(ts,
				TVM_INTERRUPT_DISABLED);

	rc = gh_irq_release(ts->vm_info->irq_label);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to release irq rc:%d\n", rc);
		goto error;
	} else {
		nvt_ts_trusted_touch_set_tvm_driver_state(ts,
					TVM_IRQ_RELEASED);
	}
	rc = gh_irq_release_notify(ts->vm_info->irq_label);
	if (rc)
		input_err(true, &ts->client->dev, "Failed to notify release irq rc:%d\n", rc);

	input_err(true, &ts->client->dev, "vm irq release succeded\n");

	nvt_ts_release_all_finger(ts);

	pm_runtime_put_sync(ts->client->controller->dev.parent);

	nvt_ts_trusted_touch_set_tvm_driver_state(ts,
					TVM_I2C_SESSION_RELEASED);
	rc = nvt_ts_vm_mem_release(ts);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to release mem rc:%d\n", rc);
		goto error;
	} else {
		nvt_ts_trusted_touch_set_tvm_driver_state(ts,
					TVM_IOMEM_RELEASED);
	}
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TRUSTED_TOUCH_TVM_INIT);
	atomic_set(&ts->trusted_touch_transition, 0);
	atomic_set(&ts->trusted_touch_enabled, 0);
	input_err(true, &ts->client->dev, " trusted touch disabled\n");
	return;
error:
	nvt_ts_trusted_touch_abort_handler(ts,
			TRUSTED_TOUCH_EVENT_RELEASE_FAILURE);
}

int nvt_ts_handle_trusted_touch_tvm(struct nvt_ts_data *ts, int value)
{
	int err = 0;

	switch (value) {
	case 0:
		if ((atomic_read(&ts->trusted_touch_enabled) == 0) &&
			(atomic_read(&ts->trusted_touch_abort_status) == 0)) {
			input_err(true, &ts->client->dev, "Trusted touch is already disabled\n");
			break;
		}
		if (atomic_read(&ts->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			nvt_ts_trusted_touch_tvm_vm_mode_disable(ts);
		} else {
			input_err(true, &ts->client->dev, "Unsupported trusted touch mode\n");
		}
		break;

	case 1:
		if (atomic_read(&ts->trusted_touch_enabled)) {
			input_err(true, &ts->client->dev, "Trusted touch usecase underway\n");
			err = -EBUSY;
			break;
		}
		if (atomic_read(&ts->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			nvt_ts_trusted_touch_tvm_vm_mode_enable(ts);
		} else {
			input_err(true, &ts->client->dev, "Unsupported trusted touch mode\n");
		}
		break;

	default:
		input_err(true, &ts->client->dev, "unsupported value: %d\n", value);
		err = -EINVAL;
		break;
	}

	return err;
}

static void nvt_ts_trusted_touch_abort_tvm(struct nvt_ts_data *ts)
{
	int rc = 0;
	int vm_state = nvt_ts_trusted_touch_get_tvm_driver_state(ts);

	if (vm_state >= TRUSTED_TOUCH_TVM_STATE_MAX) {
		input_err(true, &ts->client->dev, "invalid tvm driver state: %d\n", vm_state);
		return;
	}

	switch (vm_state) {
	case TVM_INTERRUPT_ENABLED:
		disable_irq(ts->irq);
	case TVM_IRQ_ACCEPTED:
	case TVM_INTERRUPT_DISABLED:
		rc = gh_irq_release(ts->vm_info->irq_label);
		if (rc)
			input_err(true, &ts->client->dev, "Failed to release irq rc:%d\n", rc);
		rc = gh_irq_release_notify(ts->vm_info->irq_label);
		if (rc)
			input_err(true, &ts->client->dev, "Failed to notify irq release rc:%d\n", rc);
	case TVM_I2C_SESSION_ACQUIRED:
	case TVM_IOMEM_ACCEPTED:
	case TVM_IRQ_RELEASED:
		nvt_ts_release_all_finger(ts);
		pm_runtime_put_sync(ts->client->controller->dev.parent);

	case TVM_I2C_SESSION_RELEASED:
		rc = nvt_ts_vm_mem_release(ts);
		if (rc)
			input_err(true, &ts->client->dev, "Failed to release mem rc:%d\n", rc);
	case TVM_IOMEM_RELEASED:
	case TVM_ALL_RESOURCES_LENT_NOTIFIED:
	case TRUSTED_TOUCH_TVM_INIT:
	case TVM_IRQ_LENT_NOTIFIED:
	case TVM_IOMEM_LENT_NOTIFIED:
		atomic_set(&ts->trusted_touch_enabled, 0);
	}

	atomic_set(&ts->trusted_touch_abort_status, 0);
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TRUSTED_TOUCH_TVM_INIT);
}

#else

static void nvt_ts_bus_put(struct nvt_ts_data *ts);

static int nvt_ts_trusted_touch_get_pvm_driver_state(struct nvt_ts_data *ts)
{
	return atomic_read(&ts->vm_info->vm_state);
}

static void nvt_ts_trusted_touch_set_pvm_driver_state(struct nvt_ts_data *ts,
							int state)
{
	atomic_set(&ts->vm_info->vm_state, state);
}

static void nvt_ts_trusted_touch_abort_pvm(struct nvt_ts_data *ts)
{
	int rc = 0;
	int vm_state = nvt_ts_trusted_touch_get_pvm_driver_state(ts);

	if (vm_state >= TRUSTED_TOUCH_PVM_STATE_MAX) {
		input_err(true, &ts->client->dev, "Invalid driver state: %d\n", vm_state);
		return;
	}

	switch (vm_state) {
	case PVM_IRQ_RELEASE_NOTIFIED:
	case PVM_ALL_RESOURCES_RELEASE_NOTIFIED:
	case PVM_IRQ_LENT:
	case PVM_IRQ_LENT_NOTIFIED:
		rc = gh_irq_reclaim(ts->vm_info->irq_label);
		if (rc)
			input_err(true, &ts->client->dev, "failed to reclaim irq on pvm rc:%d\n", rc);
	case PVM_IRQ_RECLAIMED:
	case PVM_IOMEM_LENT:
	case PVM_IOMEM_LENT_NOTIFIED:
	case PVM_IOMEM_RELEASE_NOTIFIED:
		rc = gh_rm_mem_reclaim(ts->vm_info->vm_mem_handle, 0);
		if (rc)
			input_err(true, &ts->client->dev, "failed to reclaim iomem on pvm rc:%d\n", rc);
		ts->vm_info->vm_mem_handle = 0;
	case PVM_IOMEM_RECLAIMED:
	case PVM_INTERRUPT_DISABLED:
		enable_irq(ts->irq);
	case PVM_I2C_RESOURCE_ACQUIRED:
	case PVM_INTERRUPT_ENABLED:
		nvt_ts_bus_put(ts);
	case TRUSTED_TOUCH_PVM_INIT:
	case PVM_I2C_RESOURCE_RELEASED:
		atomic_set(&ts->trusted_touch_enabled, 0);
		atomic_set(&ts->trusted_touch_transition, 0);
	}

	atomic_set(&ts->trusted_touch_abort_status, 0);

	nvt_ts_trusted_touch_set_pvm_driver_state(ts, TRUSTED_TOUCH_PVM_INIT);
}

static int nvt_ts_clk_prepare_enable(struct nvt_ts_data *ts)
{
	int ret;

	ret = clk_prepare_enable(ts->iface_clk);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: error on clk_prepare_enable(iface_clk): %d\n", __func__, ret);
		return ret;
	}

	ret = clk_prepare_enable(ts->core_clk);
	if (ret) {
		clk_disable_unprepare(ts->iface_clk);
		input_err(true, &ts->client->dev, "%s: error clk_prepare_enable(core_clk): %d\n", __func__, ret);
	}
	return ret;
}

static void nvt_ts_clk_disable_unprepare(struct nvt_ts_data *ts)
{
	clk_disable_unprepare(ts->core_clk);
	clk_disable_unprepare(ts->iface_clk);
}

static int nvt_ts_bus_get(struct nvt_ts_data *ts)
{
	int rc = 0;
	struct device *dev = NULL;

	reinit_completion(&ts->trusted_touch_powerdown);

	dev = ts->client->controller->dev.parent;

	mutex_lock(&ts->clk_io_ctrl_mutex);
	rc = pm_runtime_get_sync(dev);
	if (rc >= 0 &&  ts->core_clk != NULL &&
				ts->iface_clk != NULL) {
		rc = nvt_ts_clk_prepare_enable(ts);
		if (rc)
			pm_runtime_put_sync(dev);
	}

	mutex_unlock(&ts->clk_io_ctrl_mutex);
	return rc;
}

static void nvt_ts_bus_put(struct nvt_ts_data *ts)
{
	struct device *dev = NULL;

	dev = ts->client->controller->dev.parent;

	mutex_lock(&ts->clk_io_ctrl_mutex);
	if (ts->core_clk != NULL && ts->iface_clk != NULL)
		nvt_ts_clk_disable_unprepare(ts);
	pm_runtime_put_sync(dev);
	mutex_unlock(&ts->clk_io_ctrl_mutex);
	complete(&ts->trusted_touch_powerdown);
}

static struct gh_notify_vmid_desc *nvt_ts_vm_get_vmid(gh_vmid_t vmid)
{
	struct gh_notify_vmid_desc *vmid_desc;

	vmid_desc = kzalloc(offsetof(struct gh_notify_vmid_desc,
				vmid_entries[1]), GFP_KERNEL);
	if (!vmid_desc)
		return ERR_PTR(ENOMEM);

	vmid_desc->n_vmid_entries = 1;
	vmid_desc->vmid_entries[0].vmid = vmid;
	return vmid_desc;
}

static void nvt_trusted_touch_pvm_vm_mode_disable(struct nvt_ts_data *ts)
{
	int rc = 0;

	atomic_set(&ts->trusted_touch_transition, 1);

	if (atomic_read(&ts->trusted_touch_abort_status)) {
		nvt_ts_trusted_touch_abort_pvm(ts);
		return;
	}

	if (nvt_ts_trusted_touch_get_pvm_driver_state(ts) !=
					PVM_ALL_RESOURCES_RELEASE_NOTIFIED)
		input_err(true, &ts->client->dev, "all release notifications are not received yet\n");

	rc = gh_rm_mem_reclaim(ts->vm_info->vm_mem_handle, 0);
	if (rc) {
		input_err(true, &ts->client->dev, "Trusted touch VM mem reclaim failed rc:%d\n", rc);
		goto error;
	}
	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IOMEM_RECLAIMED);
	ts->vm_info->vm_mem_handle = 0;
	input_err(true, &ts->client->dev, "vm mem reclaim succeded!\n");

	rc = gh_irq_reclaim(ts->vm_info->irq_label);
	if (rc) {
		input_err(true, &ts->client->dev, "failed to reclaim irq on pvm rc:%d\n", rc);
		goto error;
	}
	nvt_ts_trusted_touch_set_pvm_driver_state(ts,
				PVM_IRQ_RECLAIMED);
	input_err(true, &ts->client->dev, "vm irq reclaim succeded!\n");
	enable_irq(ts->irq);

	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_INTERRUPT_ENABLED);
	nvt_ts_bus_put(ts);
	atomic_set(&ts->trusted_touch_transition, 0);
	nvt_ts_trusted_touch_set_pvm_driver_state(ts,
						PVM_I2C_RESOURCE_RELEASED);
	nvt_ts_trusted_touch_set_pvm_driver_state(ts,
						TRUSTED_TOUCH_PVM_INIT);
	atomic_set(&ts->trusted_touch_enabled, 0);
	input_err(true, &ts->client->dev, " trusted touch disabled\n");
	return;
error:
	nvt_ts_trusted_touch_abort_handler(ts,
			TRUSTED_TOUCH_EVENT_RECLAIM_FAILURE);
}

static void nvt_ts_vm_irq_on_release_callback(void *data,
					unsigned long notif_type,
					enum gh_irq_label label)
{
	struct nvt_ts_data *ts = data;

	if (notif_type != GH_RM_NOTIF_VM_IRQ_RELEASED) {
		input_err(true, &ts->client->dev, "invalid notification type\n");
		return;
	}

	if (nvt_ts_trusted_touch_get_pvm_driver_state(ts) == PVM_IOMEM_RELEASE_NOTIFIED) {
		nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_ALL_RESOURCES_RELEASE_NOTIFIED);
	} else {
		nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IRQ_RELEASE_NOTIFIED);
	}
}

static void nvt_ts_vm_mem_on_release_handler(enum gh_mem_notifier_tag tag,
		unsigned long notif_type, void *entry_data, void *notif_msg)
{
	struct gh_rm_notif_mem_released_payload *release_payload;
	struct trusted_touch_vm_info *vm_info;
	struct nvt_ts_data *ts;

	ts = (struct nvt_ts_data *)entry_data;
	vm_info = ts->vm_info;
	if (!vm_info) {
		input_err(true, &ts->client->dev, " Invalid vm_info\n");
		return;
	}

	if (notif_type != GH_RM_NOTIF_MEM_RELEASED) {
		input_err(true, &ts->client->dev, " Invalid notification type\n");
		return;
	}

	if (tag != vm_info->mem_tag) {
		input_err(true, &ts->client->dev, " Invalid tag\n");
		return;
	}

	if (!entry_data || !notif_msg) {
		input_err(true, &ts->client->dev, " Invalid data or notification message\n");
		return;
	}

	release_payload = (struct gh_rm_notif_mem_released_payload  *)notif_msg;
	if (release_payload->mem_handle != vm_info->vm_mem_handle) {
		input_err(true, &ts->client->dev, "Invalid mem handle detected\n");
		return;
	}

	if (nvt_ts_trusted_touch_get_pvm_driver_state(ts) ==
				PVM_IRQ_RELEASE_NOTIFIED) {
		nvt_ts_trusted_touch_set_pvm_driver_state(ts,
			PVM_ALL_RESOURCES_RELEASE_NOTIFIED);
	} else {
		nvt_ts_trusted_touch_set_pvm_driver_state(ts,
			PVM_IOMEM_RELEASE_NOTIFIED);
	}
}

static int nvt_ts_vm_mem_lend(struct nvt_ts_data *ts)
{
	struct gh_acl_desc *acl_desc;
	struct gh_sgl_desc *sgl_desc;
	struct gh_notify_vmid_desc *vmid_desc;
	gh_memparcel_handle_t mem_handle;
	gh_vmid_t trusted_vmid;
	int rc = 0;

	acl_desc = nvt_ts_vm_get_acl(GH_TRUSTED_VM);
	if (IS_ERR(acl_desc)) {
		input_err(true, &ts->client->dev, "Failed to get acl of IO memories for Trusted touch\n");
		PTR_ERR(acl_desc);
		return -EINVAL;
	}

	sgl_desc = nvt_ts_vm_get_sgl(ts->vm_info);
	if (IS_ERR(sgl_desc)) {
		input_err(true, &ts->client->dev, "Failed to get sgl of IO memories for Trusted touch\n");
		PTR_ERR(sgl_desc);
		rc = -EINVAL;
		goto sgl_error;
	}

	rc = gh_rm_mem_lend(GH_RM_MEM_TYPE_IO, 0, TRUSTED_TOUCH_MEM_LABEL,
			acl_desc, sgl_desc, NULL, &mem_handle);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to lend IO memories for Trusted touch rc:%d\n",
							rc);
		goto error;
	}

	input_err(true, &ts->client->dev, " vm mem lend succeded\n");

	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IOMEM_LENT);

	gh_rm_get_vmid(GH_TRUSTED_VM, &trusted_vmid);

	vmid_desc = nvt_ts_vm_get_vmid(trusted_vmid);

	rc = gh_rm_mem_notify(mem_handle, GH_RM_MEM_NOTIFY_RECIPIENT_SHARED,
			ts->vm_info->mem_tag, vmid_desc);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to notify mem lend to hypervisor rc:%d\n", rc);
		goto vmid_error;
	}

	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IOMEM_LENT_NOTIFIED);

	ts->vm_info->vm_mem_handle = mem_handle;
vmid_error:
	kfree(vmid_desc);
error:
	kfree(sgl_desc);
sgl_error:
	kfree(acl_desc);

	return rc;
}

static int nvt_ts_trusted_touch_pvm_vm_mode_enable(struct nvt_ts_data *ts)
{
	int rc = 0;
	struct trusted_touch_vm_info *vm_info = ts->vm_info;

	atomic_set(&ts->trusted_touch_transition, 1);
	mutex_lock(&ts->transition_lock);

	if (ts->plat_data->enabled == false) {
		input_err(true, &ts->client->dev, "Invalid power state for operation\n");
		atomic_set(&ts->trusted_touch_transition, 0);
		rc =  -EPERM;
		goto error;
	}

	/* i2c session start and resource acquire */
	if (nvt_ts_bus_get(ts) < 0) {
		input_err(true, &ts->client->dev, "nvt_ts_bus_get failed\n");
		rc = -EIO;
		goto error;
	}

	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_I2C_RESOURCE_ACQUIRED);
	/* flush pending interurpts from FIFO */
	disable_irq(ts->irq);
	atomic_set(&ts->trusted_touch_enabled, 1);
	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_INTERRUPT_DISABLED);
	nvt_ts_release_all_finger(ts);

	rc = nvt_ts_vm_mem_lend(ts);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to lend memory\n");
		goto abort_handler;
	}
	input_err(true, &ts->client->dev, "vm mem lend succeded\n");
	rc = gh_irq_lend_v2(vm_info->irq_label, vm_info->vm_name,
		ts->irq, &nvt_ts_vm_irq_on_release_callback, ts);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to lend irq\n");
		goto abort_handler;
	}

	input_err(true, &ts->client->dev, "vm irq lend succeded for irq:%d\n", ts->irq);
	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IRQ_LENT);

	rc = gh_irq_lend_notify(vm_info->irq_label);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to notify irq\n");
		goto abort_handler;
	}
	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IRQ_LENT_NOTIFIED);

	mutex_unlock(&ts->transition_lock);
	atomic_set(&ts->trusted_touch_transition, 0);
	input_err(true, &ts->client->dev, " trusted touch enabled\n");
	return rc;

abort_handler:
	atomic_set(&ts->trusted_touch_enabled, 0);
	nvt_ts_trusted_touch_abort_handler(ts, TRUSTED_TOUCH_EVENT_LEND_FAILURE);

error:
	mutex_unlock(&ts->transition_lock);
	return rc;
}

int nvt_ts_handle_trusted_touch_pvm(struct nvt_ts_data *ts, int value)
{
	int err = 0;

	switch (value) {
	case 0:
		if (atomic_read(&ts->trusted_touch_enabled) == 0 &&
			(atomic_read(&ts->trusted_touch_abort_status) == 0)) {
			input_err(true, &ts->client->dev, "Trusted touch is already disabled\n");
			break;
		}
		if (atomic_read(&ts->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			nvt_trusted_touch_pvm_vm_mode_disable(ts);
		} else {
			input_err(true, &ts->client->dev, "Unsupported trusted touch mode\n");
		}
		break;

	case 1:
		if (atomic_read(&ts->trusted_touch_enabled)) {
			input_err(true, &ts->client->dev, "Trusted touch usecase underway\n");
			err = -EBUSY;
			break;
		}
		if (atomic_read(&ts->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			err = nvt_ts_trusted_touch_pvm_vm_mode_enable(ts);
		} else {
			input_err(true, &ts->client->dev, "Unsupported trusted touch mode\n");
		}
		break;

	default:
		input_err(true, &ts->client->dev, "unsupported value: %d\n", value);
		err = -EINVAL;
		break;
	}
	return err;
}

#endif

static void nvt_ts_trusted_touch_event_notify(struct nvt_ts_data *ts, int event)
{
	atomic_set(&ts->trusted_touch_event, event);
	sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "trusted_touch_event");
}

static void nvt_ts_trusted_touch_abort_handler(struct nvt_ts_data *ts, int error)
{
	atomic_set(&ts->trusted_touch_abort_status, error);
	input_err(true, &ts->client->dev, "TUI session aborted with failure:%d\n", error);
	nvt_ts_trusted_touch_event_notify(ts, error);
#if IS_ENABLED(CONFIG_ARCH_QTI_VM)
	input_err(true, &ts->client->dev, "Resetting touch controller\n");
	if (nvt_ts_trusted_touch_get_tvm_driver_state(ts) >= TVM_IOMEM_ACCEPTED
			&& error == TRUSTED_TOUCH_EVENT_I2C_FAILURE) {
		input_err(true, &ts->client->dev, "Resetting touch controller ??????????????\n");

	}
#endif
}

static int nvt_ts_vm_init(struct nvt_ts_data *ts)
{
	int rc = 0;
	struct trusted_touch_vm_info *vm_info;
	void *mem_cookie;

	rc = nvt_ts_populate_vm_info(ts);
	if (rc) {
		input_err(true, &ts->client->dev, "Cannot setup vm pipeline\n");
		rc = -EINVAL;
		goto fail;
	}

	vm_info = ts->vm_info;
#if IS_ENABLED(CONFIG_ARCH_QTI_VM)
	mem_cookie = gh_mem_notifier_register(vm_info->mem_tag,
			nvt_ts_vm_mem_on_lend_handler, ts);
	if (!mem_cookie) {
		input_err(true, &ts->client->dev, "Failed to register on lend mem notifier\n");
		rc = -EINVAL;
		goto init_fail;
	}
	vm_info->mem_cookie = mem_cookie;
	rc = gh_irq_wait_for_lend_v2(vm_info->irq_label, GH_PRIMARY_VM,
			&nvt_ts_vm_irq_on_lend_callback, ts);
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TRUSTED_TOUCH_TVM_INIT);
#else
	mem_cookie = gh_mem_notifier_register(vm_info->mem_tag,
			nvt_ts_vm_mem_on_release_handler, ts);
	if (!mem_cookie) {
		input_err(true, &ts->client->dev, "Failed to register on release mem notifier\n");
		rc = -EINVAL;
		goto init_fail;
	}
	vm_info->mem_cookie = mem_cookie;
	nvt_ts_trusted_touch_set_pvm_driver_state(ts, TRUSTED_TOUCH_PVM_INIT);
#endif
	return rc;
init_fail:
	nvt_ts_vm_deinit(ts);
fail:
	return rc;
}

static void nvt_ts_dt_parse_trusted_touch_info(struct nvt_ts_data *ts)
{
	struct device_node *np = ts->client->dev.of_node;
	int rc = 0;
	const char *selection;
	const char *environment;

	rc = of_property_read_string(np, "trusted-touch-mode",
								&selection);
	if (rc) {
		input_err(true, &ts->client->dev, "%s: No trusted touch mode selection made\n", __func__);
		atomic_set(&ts->trusted_touch_mode,
						TRUSTED_TOUCH_MODE_NONE);
		return;
	}

	if (!strcmp(selection, "vm_mode")) {
		atomic_set(&ts->trusted_touch_mode,
						TRUSTED_TOUCH_VM_MODE);
		input_err(true, &ts->client->dev, "Selected trusted touch mode to VM mode\n");
	} else {
		atomic_set(&ts->trusted_touch_mode,
						TRUSTED_TOUCH_MODE_NONE);
		input_err(true, &ts->client->dev, "Invalid trusted_touch mode\n");
	}

	rc = of_property_read_string(np, "touch-environment",
						&environment);
	if (rc) {
		input_err(true, &ts->client->dev, "%s: No trusted touch mode environment\n", __func__);
	}
	ts->touch_environment = environment;
	input_err(true, &ts->client->dev, "Trusted touch environment:%s\n",
			ts->touch_environment);
}

static void nvt_ts_trusted_touch_init(struct nvt_ts_data *ts)
{
	int rc = 0;

	atomic_set(&ts->trusted_touch_initialized, 0);
	nvt_ts_dt_parse_trusted_touch_info(ts);

	if (atomic_read(&ts->trusted_touch_mode) ==
						TRUSTED_TOUCH_MODE_NONE)
		return;

	init_completion(&ts->trusted_touch_powerdown);

	/* Get clocks */
	ts->core_clk = devm_clk_get(ts->client->controller->dev.parent,
						"m-ahb");
	if (IS_ERR(ts->core_clk)) {
		ts->core_clk = NULL;
		input_err(true, &ts->client->dev, "%s: core_clk is not defined\n", __func__);
	}

	ts->iface_clk = devm_clk_get(ts->client->controller->dev.parent,
						"se-clk");
	if (IS_ERR(ts->iface_clk)) {
		ts->iface_clk = NULL;
		input_err(true, &ts->client->dev, "%s: iface_clk is not defined\n", __func__);
	}

	if (atomic_read(&ts->trusted_touch_mode) ==
						TRUSTED_TOUCH_VM_MODE) {
		rc = nvt_ts_vm_init(ts);
		if (rc)
			input_err(true, &ts->client->dev, "Failed to init VM\n");
	}
	atomic_set(&ts->trusted_touch_initialized, 1);
}

static ssize_t trusted_touch_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	input_err(true, &ts->client->dev, "%s\n", __func__);
	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->trusted_touch_enabled));
}

static ssize_t trusted_touch_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);
	unsigned long value;
	int ret;

	input_err(true, &ts->client->dev, "%s\n", __func__);
	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0)
		return ret;

	if (!atomic_read(&ts->trusted_touch_initialized))
		return -EIO;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	if (value == 1)
		sec_input_notify(&ts->nvt_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif

	input_info(true, &ts->client->dev, "%s: value: %ld\n", __func__, value);
#if IS_ENABLED(CONFIG_ARCH_QTI_VM)
	ret = nvt_ts_handle_trusted_touch_tvm(ts, value);
#else
	ret = nvt_ts_handle_trusted_touch_pvm(ts, value);
#endif
	if (ret) {
		input_err(true, &ts->client->dev, "Failed to handle touch vm\n");
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->nvt_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	if (value == 0)
		sec_input_notify(&ts->nvt_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif

	return count;
}

static ssize_t trusted_touch_event_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	input_err(true, &ts->client->dev, "%s\n", __func__);
	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->trusted_touch_event));
}

static ssize_t trusted_touch_event_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);
	unsigned long value;
	int ret;

	input_err(true, &ts->client->dev, "%s\n", __func__);
	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0)
		return ret;

	if (!atomic_read(&ts->trusted_touch_initialized))
		return -EIO;

	input_info(true, &ts->client->dev, "%s: value: %ld\n", __func__, value);

	atomic_set(&ts->trusted_touch_event, value);

	return count;
}

static ssize_t trusted_touch_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	input_err(true, &ts->client->dev, "%s\n", __func__);
	return scnprintf(buf, PAGE_SIZE, "%s", ts->vm_info->trusted_touch_type);
}
#endif
static irqreturn_t secure_filter_interrupt(struct nvt_ts_data *ts)
{
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		if (atomic_cmpxchg(&ts->secure_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		} else {
			input_info(true, &ts->client->dev, "%s: pending irq:%d\n",
					__func__, (int)atomic_read(&ts->secure_pending_irqs));
		}

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

/**
 * Sysfs attr group for secure touch & interrupt handler for Secure world.
 * @atomic : syncronization for secure_enabled
 * @pm_runtime : set rpm_resume or rpm_ilde
 */
static ssize_t secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->secure_enabled));
}

static ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *addr, const char *buf, size_t count)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	unsigned long data;

	if (count > 2) {
		input_err(true, &ts->client->dev,
				"%s: cmd length is over (%s,%d)!!\n",
				__func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	ret = kstrtoul(buf, 10, &data);
	if (ret != 0) {
		input_err(true, &ts->client->dev, "%s: failed to read:%d\n",
				__func__, ret);
		return -EINVAL;
	}

	if (data == 1) {
		/* Enable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
			input_err(true, &ts->client->dev, "%s: already enabled\n", __func__);
			return -EBUSY;
		}

		sec_delay(200);
		
		/* syncronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		disable_irq(ts->client->irq);

		/* Release All Finger */
		nvt_ts_release_all_finger(ts);

		if (nvt_pm_runtime_get_sync(ts) < 0) {
			enable_irq(ts->client->irq);
			input_err(true, &ts->client->dev, "%s: failed to get pm_runtime\n", __func__);
			return -EIO;
		}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->nvt_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif
		reinit_completion(&ts->secure_powerdown);
		reinit_completion(&ts->secure_interrupt);

		atomic_set(&ts->secure_enabled, 1);
		atomic_set(&ts->secure_pending_irqs, 0);

		enable_irq(ts->client->irq);

		input_info(true, &ts->client->dev, "%s: secure touch enable\n", __func__);
	} else if (data == 0) {
		/* Disable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
			input_err(true, &ts->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		sec_delay(200);

		nvt_pm_runtime_put_sync(ts);
		atomic_set(&ts->secure_enabled, 0);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		sec_delay(10);

		nvt_ts_irq_thread(ts->client->irq, ts);
		complete(&ts->secure_interrupt);
		complete(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: secure touch disable\n", __func__);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->nvt_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
	} else {
		input_err(true, &ts->client->dev, "%s: unsupport value:%ld\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

static ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
		input_err(true, &ts->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, -1, 0) == -1) {
		input_err(true, &ts->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, 1, 0) == 1) {
		val = 1;
		input_err(true, &ts->client->dev, "%s: pending irq is %d\n",
				__func__, atomic_read(&ts->secure_pending_irqs));
	}

	complete(&ts->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "%u", val);
}

static ssize_t secure_ownership_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1");
}

static int secure_touch_init(struct nvt_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	init_completion(&ts->secure_interrupt);
	init_completion(&ts->secure_powerdown);

	return 0;
}

static void secure_touch_stop(struct nvt_ts_data *ts, bool stop)
{
	if (atomic_read(&ts->secure_enabled)) {
		atomic_set(&ts->secure_pending_irqs, -1);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		if (stop)
			wait_for_completion_interruptible(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: %d\n", __func__, stop);
	}
}

#if IS_ENABLED(CONFIG_GH_RM_DRV)
static DEVICE_ATTR(trusted_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
		trusted_touch_enable_show, trusted_touch_enable_store);
static DEVICE_ATTR(trusted_touch_event, (S_IRUGO | S_IWUSR | S_IWGRP),
		trusted_touch_event_show, trusted_touch_event_store);
static DEVICE_ATTR(trusted_touch_type, S_IRUGO,
		trusted_touch_type_show, NULL);
#endif
static DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
		secure_touch_enable_show, secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO, secure_touch_show, NULL);
static DEVICE_ATTR(secure_ownership, S_IRUGO, secure_ownership_show, NULL);
static struct attribute *secure_attr[] = {
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	&dev_attr_trusted_touch_enable.attr,
	&dev_attr_trusted_touch_event.attr,
	&dev_attr_trusted_touch_type.attr,
#endif
	&dev_attr_secure_touch_enable.attr,
	&dev_attr_secure_touch.attr,
	&dev_attr_secure_ownership.attr,
	NULL,
};

static struct attribute_group secure_attr_group = {
	.attrs = secure_attr,
};
#endif

static int nvt_fw_recovery(char *point_data)
{
	int i = 0;
	int detected = true;

	/* check pattern */
	for (i = 1; i < 7; i++) {
		if (point_data[i] != 0x77) {
			detected = false;
			break;
		}
	}

	return detected;
}

#if NVT_TOUCH_WDT_RECOVERY
static bool nvt_wdt_fw_recovery(u8 *point_data)
{
	int recovery_cnt_max = 10;
	bool recovery_enable = false;
	int i = 0;
	static int recovery_cnt = 0;

	recovery_cnt++;

	/* check pattern */
	for (i = 1; i < 7; i++) {
		if ((point_data[i] != 0xFD) && (point_data[i] != 0xFE)) {
			recovery_cnt = 0;
			break;
		}
	}

	if (recovery_cnt > recovery_cnt_max) {
		recovery_enable = true;
		recovery_cnt = 0;
	}

	return recovery_enable;
}

int nvt_check_fw_reset_state(struct nvt_ts_data *ts, RST_COMPLETE_STATE check_reset_state)
{
	char buf[8] = {0};
	int ret = 0;
	int retry = 0;
	int retry_max = (check_reset_state == RESET_STATE_INIT) ? 10 : 50;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE);

	while (1) {
		//---read reset state---
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		ts->nvt_ts_read(ts, &buf[0], 1, &buf[1], 5);

		if ((buf[1] >= check_reset_state) && (buf[1] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if(unlikely(retry > retry_max)) {
			input_err(true, &ts->client->dev,"error, retry=%d, buf[1]=0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
				retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}

		sec_delay(10);
	}

	if (!ret)
		input_info(true, &ts->client->dev, "%s : retry=%d, buf[1] = %x\n", __func__, retry, buf[1]);

	return ret;
}

#endif	/* #if NVT_TOUCH_WDT_RECOVERY */

void nvt_ts_proximity_report(struct nvt_ts_data *ts, uint8_t *data)
{
	struct nvt_ts_event_proximity *p_event_proximity;
	u8 status = 0;

	p_event_proximity = (struct nvt_ts_event_proximity *)&data[1];

	/* check data page */
	if (p_event_proximity->data_page != FUNCPAGE_PROXIMITY) {
		input_info(true, &ts->client->dev,"proximity data page %d is invalid\n", p_event_proximity->data_page);
		return;
	}

	status = p_event_proximity->status;

	input_info(true, &ts->client->dev,"proximity->status = %d\n", status);

	input_info(true, &ts->client->dev, "%s hover : %d\n", __func__, status);
	ts->hover_event = status;
	input_report_abs(ts->plat_data->input_dev_proximity, ABS_MT_CUSTOM, status);
	input_sync(ts->plat_data->input_dev_proximity);
}

void nvt_ts_wakeup_gesture_report(struct nvt_ts_data *ts, char *data)
{
	int keycode = 0;
	char func_id = data[3];

//	input_info(true, &ts->client->dev, "gesture_id = %d\n", func_id);

	if (func_id == 15) {
		input_info(true, &ts->client->dev,"Gesture : Double Click.\n");
		keycode = KEY_WAKEUP;
	} else if (func_id == 21) {
		input_info(true, &ts->client->dev,"Gesture : Slide UP.\n");
		keycode = KEY_BLACK_UI_GESTURE;
		ts->scrub_id = SPONGE_EVENT_TYPE_SPAY;
	} else if (func_id >= 12) {
		input_err(true, &ts->client->dev, "invalid gesture event (%02X %02X %02X %02X %02X %02X)",
			data[0], data[1], data[2],
			data[3], data[4], data[5]);
		keycode = KEY_WAKEUP;
	} else {
		input_err(true, &ts->client->dev, "invalid gesture event (%02X %02X %02X %02X %02X %02X)",
			data[0], data[1], data[2],
			data[3], data[4], data[5]);
		return;
	}

	if (keycode > 0) {
		input_report_key(ts->plat_data->input_dev, keycode, 1);
		input_sync(ts->plat_data->input_dev);
		input_report_key(ts->plat_data->input_dev, keycode, 0);
		input_sync(ts->plat_data->input_dev);
	}
}

#if SEC_FW_STATUS
#define FW_STATUS_OFFSET	(1 + 0x3C)	// 1 is for read dummy byte, total 2 bytes length for fw status

void nvt_ts_ic_status(struct nvt_ts_data *ts, uint8_t *point_data)
{
	u8 fw_status = point_data[FW_STATUS_OFFSET];
	u8 fw_status_changed = 0;
	char print_buff[800] = { 0 };
	char tmp_buff[100] = { 0 };

	fw_status_changed = fw_status ^ ts->fw_status_record;
	ts->fw_status_record = fw_status;

	if (fw_status_changed & FW_STATUS_WATER_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Water flag %s)\n", __func__, (fw_status & FW_STATUS_WATER_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Water flag %s, ", (fw_status & FW_STATUS_WATER_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_PALM_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Palm flag %s)\n", __func__, (fw_status & FW_STATUS_PALM_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Palm flag %s, ", (fw_status & FW_STATUS_PALM_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_HOPPING_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Hopping flag %s)\n", __func__, (fw_status & FW_STATUS_HOPPING_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Hopping flag %s, ", (fw_status & FW_STATUS_HOPPING_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_BENDING_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Bending flag %s)\n", __func__, (fw_status & FW_STATUS_BENDING_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Bending flag %s, ", (fw_status & FW_STATUS_BENDING_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_GLOVE_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Glove flag %s)\n", __func__, (fw_status & FW_STATUS_GLOVE_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Glove flag %s, ", (fw_status & FW_STATUS_GLOVE_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_GND_UNSTABLE) {
//			input_info(true, &ts->client->dev, "%s: FW status (GND unstable %s)\n", __func__, (fw_status & FW_STATUS_GND_UNSTABLE) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "GND unstable %s, ", (fw_status & FW_STATUS_GND_UNSTABLE) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_TA_PIN) {
//			input_info(true, &ts->client->dev, "%s: FW status (TA pin %s)\n", __func__, (fw_status & FW_STATUS_TA_PIN) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "TA pin %s, ", (fw_status & FW_STATUS_TA_PIN) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}
	if (print_buff[0])
		input_info(true, &ts->client->dev, "%s: FW status %s\n", __func__, print_buff);

	switch (point_data[FW_STATUS_OFFSET + 1] & FW_STATUS_REK_STATUS) {
	case 0:
		// Do nothing...
		break;
	case 1:
		input_err(true, &ts->client->dev, "%s: FW status (1D reK)\n", __func__);
		break;
	case 2:
		input_err(true, &ts->client->dev, "%s: FW status (2D RC reK)\n", __func__);
		break;
	case 3:
		input_err(true, &ts->client->dev, "%s: FW status (2D raw check reK)\n", __func__);
		break;
	default:
		input_err(true, &ts->client->dev, "%s: FW status (invalid reK status : %02x%02x)\n",
					__func__, point_data[FW_STATUS_OFFSET], point_data[FW_STATUS_OFFSET + 1]);
	}
}
#endif

#define NVT_TS_LOCATION_DETECT_SIZE	6

static int nvt_ts_regulator_init(struct nvt_ts_data *ts)
{

	if (ts->nplat_data->name_lcd_rst) {
		ts->regulator_lcd_rst = regulator_get(NULL, ts->nplat_data->name_lcd_rst);
		if (IS_ERR(ts->regulator_lcd_rst)) {
			input_err(true, &ts->client->dev, "%s: Failed to get regulator_lcd_rst regulator.", __func__);
		}
	}

	if (ts->nplat_data->name_lcd_vddi) {
		ts->regulator_lcd_vddi = regulator_get(NULL, ts->nplat_data->name_lcd_vddi);
		if (IS_ERR(ts->regulator_lcd_vddi)) {
			input_err(true, &ts->client->dev, "%s: Failed to get regulator_lcd_vddi regulator.", __func__);
		}
	}

	if (ts->nplat_data->name_lcd_bl_en) {
		ts->regulator_lcd_bl_en = regulator_get(NULL, ts->nplat_data->name_lcd_bl_en);
		if (IS_ERR(ts->regulator_lcd_bl_en)) {
			input_err(true, &ts->client->dev, "%s: Failed to get regulator_lcd_bl_en regulator.", __func__);
		}
	}

	if (ts->nplat_data->name_lcd_vsp) {
		ts->regulator_lcd_vsp = regulator_get(NULL, ts->nplat_data->name_lcd_vsp);
		if (IS_ERR(ts->regulator_lcd_vsp)) {
			input_err(true, &ts->client->dev, "%s: Failed to get regulator_lcd_vsp regulator.", __func__);
		}
	}

	if (ts->nplat_data->name_lcd_vsn) {
		ts->regulator_lcd_vsn = regulator_get(NULL, ts->nplat_data->name_lcd_vsn);
		if (IS_ERR(ts->regulator_lcd_vsn)) {
			input_err(true, &ts->client->dev, "%s: Failed to get regulator_lcd_vsn regulator.", __func__);
		}
	}

	return 0;
}

static void nvt_ts_coord_parsing(struct nvt_ts_data *ts, struct nvt_ts_event_coord *p_event_coord, u8 *point_data, u8 t_id, int i, int press)
{
	int w_major = 0, w_minor = 0, p = 0;
	int status = 0;

	ts->plat_data->coord[t_id].id = t_id;
	if (press == SEC_TS_COORDINATE_ACTION_PRESS) {
		ts->plat_data->coord[t_id].action = SEC_TS_COORDINATE_ACTION_PRESS;
	} else if (press == SEC_TS_COORDINATE_ACTION_MOVE) {
		ts->plat_data->coord[t_id].action = SEC_TS_COORDINATE_ACTION_MOVE;
	} else {
		ts->plat_data->coord[t_id].action = SEC_TS_COORDINATE_ACTION_RELEASE;
		return;
	}

		// width major
	w_major = p_event_coord->w_major;
	if (!w_major)
		w_major = 1;

	// width minor
	w_minor = point_data[i + 99];
	if (!w_minor)
		w_minor = 1;

	if (i < 2) {
		p = (p_event_coord->pressure_7_0) + (point_data[i + 62] << 8);
		if (p > TOUCH_FORCE_NUM)
			p = TOUCH_FORCE_NUM;
	} else {
		p = (p_event_coord->pressure_7_0);
	}

	if (!p)
		p = 1;

	status = p_event_coord->status;

	ts->plat_data->coord[t_id].x = (u32)(p_event_coord->x_11_4 << 4) + (u32)(p_event_coord->x_3_0);
	ts->plat_data->coord[t_id].y = (u32)(p_event_coord->y_11_4 << 4) + (u32)(p_event_coord->y_3_0);
	ts->plat_data->coord[t_id].z = p;
	if ((status == FINGER_ENTER) || (status == FINGER_MOVING))
		ts->plat_data->coord[t_id].ttype = NVT_TS_TOUCHTYPE_NORMAL;
	else if (status == GLOVE_TOUCH)
		ts->plat_data->coord[t_id].ttype = NVT_TS_TOUCHTYPE_GLOVE;

	ts->plat_data->coord[t_id].major = w_major;
	ts->plat_data->coord[t_id].minor = w_minor;
}

static void nvt_ts_coordinate_event(struct nvt_ts_data *ts, u8 *point_data)
{
	struct nvt_ts_event_coord *p_event_coord;
	int i = 0;
	int finger_cnt = 0;
	int id = 0;
	int press_id[TOUCH_MAX_FINGER_NUM] = {0};
	int status = 0;

	for (i = 0, finger_cnt = 0; i < ts->nplat_data->max_touch_num; i++) {
		p_event_coord = (struct nvt_ts_event_coord *)&point_data[1 + 6 * i];
		id = p_event_coord->id;
		if (!id || (id > ts->nplat_data->max_touch_num))
			continue;
		id = id - 1;
		status = p_event_coord->status;
		if ((status == FINGER_ENTER) || (status == FINGER_MOVING) || (status == GLOVE_TOUCH)) {
			press_id[id] = ts->press[id] = true;
			ts->plat_data->prev_coord[id] = ts->plat_data->coord[id];
			if (ts->press[id] && !ts->p_press[id]) {
				nvt_ts_coord_parsing(ts, p_event_coord, point_data, id, i, SEC_TS_COORDINATE_ACTION_PRESS);
				sec_input_coord_event(&ts->client->dev, id);
				ts->p_press[id] = ts->press[id];
			} else if (ts->press[id] == ts->p_press[id]) {
				nvt_ts_coord_parsing(ts, p_event_coord, point_data, id, i, SEC_TS_COORDINATE_ACTION_MOVE);
				sec_input_coord_event(&ts->client->dev, id);
			} else {
				input_info(true, &ts->client->dev, "%s: id:%d, press:%d, p_press %d\n", __func__, id, ts->press[id], ts->p_press[id]);
			}
		}
	}

	for (i = 0; i < ts->nplat_data->max_touch_num; i++) {
		if (!press_id[i] && ts->p_press[i]) {
			ts->plat_data->prev_coord[i] = ts->plat_data->coord[i];
			ts->press[i] = false;
			nvt_ts_coord_parsing(ts, p_event_coord, point_data, i, i, SEC_TS_COORDINATE_ACTION_RELEASE);
			sec_input_coord_event(&ts->client->dev, i);
			ts->p_press[i] = false;
		}
	}

}

irqreturn_t nvt_ts_irq_thread(int irq, void *ptr)
{
	struct nvt_ts_data *ts = (struct nvt_ts_data *)ptr;
	int ret = -1;
	u8 point_data[POINT_DATA_LEN + 1 + DUMMY_BYTES] = {0};

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (secure_filter_interrupt(ts) == IRQ_HANDLED) {
		wait_for_completion_interruptible_timeout(&ts->secure_interrupt,
				msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, &ts->client->dev,
				"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return IRQ_HANDLED;
#endif

	ret = sec_input_handler_start(&ts->client->dev);
	if (ret < 0)
		return IRQ_HANDLED;
	mutex_lock(&ts->lock);

	ret = ts->nvt_ts_read(ts, &point_data[0], 1, &point_data[1], POINT_DATA_LEN);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s:  CTP_SPI_READ failed.(%d)\n", __func__, ret);
		goto XFER_ERROR;
	}
/*
	//--- dump SPI buf ---
	for (i = 0; i < 10; i++) {
		printk("%02X %02X %02X %02X %02X %02X  ",
			point_data[1+i*6], point_data[2+i*6], point_data[3+i*6], point_data[4+i*6], point_data[5+i*6], point_data[6+i*6]);
	}
	printk("\n");
*/

#if NVT_TOUCH_WDT_RECOVERY
   /* ESD protect by WDT */
	if (nvt_wdt_fw_recovery(point_data)) {
		input_err(true, &ts->client->dev,"Recover for fw reset, %02X\n", point_data[1]);
		nvt_update_firmware(ts, ts->plat_data->firmware_name);
		if (nvt_check_fw_reset_state(ts, RESET_STATE_REK)) {
			input_err(true, &ts->client->dev, "%s: Check FW state failed after FW reset recovery\n", __func__);
		} else {
			nvt_ts_mode_restore(ts);
		}

		goto XFER_ERROR;
	}
#endif /* #if NVT_TOUCH_WDT_RECOVERY */

	/* ESD protect by FW handshake */
	if (nvt_fw_recovery(point_data)) {
		goto XFER_ERROR;
	}

#if POINT_DATA_CHECKSUM
   if (POINT_DATA_LEN >= POINT_DATA_CHECKSUM_LEN) {
       ret = nvt_ts_point_data_checksum(ts, point_data, POINT_DATA_CHECKSUM_LEN);
       if (ret) {
           goto XFER_ERROR;
       }
   }
#endif /* POINT_DATA_CHECKSUM */

	if (ts->plat_data->support_ear_detect) {
		if (((point_data[1] >> 3) == DATA_PROTOCOL)
				&& (point_data[2] == FUNCPAGE_PROXIMITY)) {
			nvt_ts_proximity_report(ts, point_data);
			mutex_unlock(&ts->lock);
			return IRQ_HANDLED;
		}
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		if (point_data[2] == FUNCPAGE_GESTURE)
			nvt_ts_wakeup_gesture_report(ts, point_data);
		else
			input_err(true, &ts->client->dev, "invalid lp event (%02X %02X %02X %02X %02X %02X)",
				point_data[0], point_data[1], point_data[2],
				point_data[3], point_data[4], point_data[5]);
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;
	}

#if SEC_FW_STATUS
	nvt_ts_ic_status(ts, point_data);
#endif
	nvt_ts_coordinate_event(ts, point_data);

XFER_ERROR:

	mutex_unlock(&ts->lock);

	return IRQ_HANDLED;
}

int nvt_ts_lcd_reset_ctrl(struct nvt_ts_data *ts, bool on)
{
	int retval;
	static bool enabled;

	if (enabled == on)
		return 0;

	if (ts->nplat_data->name_lcd_rst == NULL) {
		input_err(true, &ts->client->dev, "%s: name_lcd_rst is null\n", __func__);
		return 0;
	}

	if (on) {
		retval = regulator_enable(ts->regulator_lcd_rst);
		if (retval) {
			input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_rst: %d\n", __func__, retval);
			return retval;
		}

	} else {
		regulator_disable(ts->regulator_lcd_rst);
	}

	enabled = on;

	input_info(true, &ts->client->dev, "%s %d done\n", __func__, on);

	return 0;
}

int nvt_ts_lcd_power_ctrl(struct nvt_ts_data *ts, bool on)
{
	int retval;
	static bool enabled;

	if (enabled == on)
		return 0;

	if (on) {
		if (ts->nplat_data->name_lcd_vddi) {
			retval = regulator_enable(ts->regulator_lcd_vddi);
			if (retval) {
				input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_vddi: %d\n", __func__, retval);
				return retval;
			}
		}

		if (ts->nplat_data->name_lcd_bl_en) {
			retval = regulator_enable(ts->regulator_lcd_bl_en);
			if (retval) {
				input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_bl_en: %d\n", __func__, retval);
				return retval;
			}
		}

		if (ts->nplat_data->name_lcd_vsp) {
			retval = regulator_enable(ts->regulator_lcd_vsp);
			if (retval) {
				input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_vsp: %d\n", __func__, retval);
				return retval;
			}
		}
		if (ts->nplat_data->name_lcd_vsn) {
			retval = regulator_enable(ts->regulator_lcd_vsn);
			if (retval) {
				input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_vsn: %d\n", __func__, retval);
				return retval;
			}
		}
	} else {
		if (ts->nplat_data->name_lcd_vddi)
			regulator_disable(ts->regulator_lcd_vddi);
		if (ts->nplat_data->name_lcd_bl_en)
			regulator_disable(ts->regulator_lcd_bl_en);
		if (ts->nplat_data->name_lcd_vsp)
			regulator_disable(ts->regulator_lcd_vsp);
		if (ts->nplat_data->name_lcd_vsn)
			regulator_disable(ts->regulator_lcd_vsn);
	}

	enabled = on;

	input_info(true, &ts->client->dev, "%s %d done\n", __func__, on);

	return 0;
}


void nvt_ts_early_resume(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	input_info(true, &ts->client->dev, "%s : start(%d)\n", __func__, ts->plat_data->power_state);

	cancel_delayed_work_sync(&ts->work_read_info);

	if (ts->plat_data->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return;
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 0);
#endif
	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		if (device_may_wakeup(&ts->client->dev))
			disable_irq_wake(ts->client->irq);

		if (ts->nplat_data->support_nvt_lpwg_dump) {
			input_info(true, &ts->client->dev, "%s : read lpgw logs start\n", __func__);
			nvt_ts_lpwg_dump(ts);
			input_info(true, &ts->client->dev, "%s : read lpgw logs end\n", __func__);
		}

		disable_irq(ts->irq);

		mutex_lock(&ts->lock);
		nvt_ts_lcd_reset_ctrl(ts, false);
		mutex_unlock(&ts->lock);
	}
}


int nvt_ts_input_resume(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);
	u8 lpwg_dump[5] = {0x7, 0x0, 0x0, 0x0, 0x0};

	mutex_lock(&ts->lock);

	ts->lcd_esd_recovery = 0;
	cancel_delayed_work_sync(&ts->work_read_info);

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 0);
#endif

	if (ts->plat_data->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return 0;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		nvt_ts_lcd_power_ctrl(ts, false);
		if (ts->nplat_data->support_nvt_lpwg_dump) {
			nvt_ts_lpwg_dump_buf_write(ts, lpwg_dump);
		}

	} else {
		ts->plat_data->pinctrl_configure(&ts->client->dev, true);
	}

	ts->plat_data->prox_power_off = 0;
	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;

	if (nvt_update_firmware(ts, ts->plat_data->firmware_name)) {
		input_err(true, &ts->client->dev,"download firmware failed, ignore check fw state\n");
	} else {
		nvt_check_fw_reset_state(ts, RESET_STATE_REK);
	}

	nvt_ts_mode_restore(ts);

	if (ts->plat_data->ed_enable == 0 && ts->ed_reset_flag) {
		input_info(true, &ts->client->dev, "%s : set ed on & off\n", __func__);
		if (set_ear_detect(ts, 1, false)) {
			input_err(true, &ts->client->dev, "%s : Fail to set set_ear_detect on\n", __func__);
		}
		if (set_ear_detect(ts, 0, false)) {
			input_err(true, &ts->client->dev, "%s : Fail to set set_ear_detect off\n", __func__);
		}
	}
	ts->ed_reset_flag = false;

	if (ts->plat_data->ed_enable) {
		if (set_ear_detect(ts, ts->plat_data->ed_enable, false)) {
			input_err(true, &ts->client->dev, "%s : Fail to set set_ear_detect\n", __func__);
		}
	}

	mutex_unlock(&ts->lock);

	enable_irq(ts->client->irq);

	cancel_delayed_work(&ts->work_print_info);
	ts->plat_data->print_info_cnt_open = 0;
	ts->plat_data->print_info_cnt_release = 0;
	schedule_work(&ts->work_print_info.work);

	input_info(true, &ts->client->dev, "%s : end\n", __func__);

	return 0;
}

void nvt_ts_input_suspend(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);
	char buf[4] = {0};
	int enter_force_ed_mode = 0;

	u8 lpwg_dump[5] = {0x3, 0x0, 0x0, 0x0, 0x0};

	cancel_delayed_work_sync(&ts->work_read_info);

	if (ts->plat_data->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF || ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		input_info(true, &ts->client->dev, "Touch is already suspend(%d)\n", ts->plat_data->power_state);
		return;
	}

	input_info(true, &ts->client->dev, "%s : ed:%d, lp:%d, prox:%d, test:%d, prox_in_aot:%d\n",
					__func__, ts->plat_data->ed_enable,
					ts->plat_data->lowpower_mode, ts->plat_data->prox_power_off,
					ts->lcdoff_test, ts->prox_in_aot);

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 1);
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	if (atomic_read(&ts->trusted_touch_enabled)) {
		input_info(true, &ts->client->dev, "%s wait for disabling trusted touch\n", __func__);
		wait_for_completion_interruptible(&ts->secure_powerdown);
	}
#endif
#endif

	if (ts->plat_data->lowpower_mode && (ts->plat_data->prox_power_off || ts->plat_data->ed_enable))
		enter_force_ed_mode = 1;	// for ed

	if ((!ts->plat_data->prox_power_off && ts->plat_data->ed_enable) && ts->prox_in_aot)
		enter_force_ed_mode = 0;	// for prox in aot feature

	if ((ts->plat_data->lowpower_mode || ts->lcdoff_test) && enter_force_ed_mode == 0) {
		disable_irq(ts->irq);
		nvt_ts_lcd_power_ctrl(ts, true);
		nvt_ts_lcd_reset_ctrl(ts, true);
		mutex_lock(&ts->lock);
		ts->plat_data->power_state = SEC_INPUT_STATE_LPM;

		/* LPWG enter */
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = LPWG_ENTER;
		ts->nvt_ts_write(ts, buf, 2, NULL, 0);
		mutex_unlock(&ts->lock);

		enable_irq(ts->irq);
		enable_irq_wake(ts->client->irq);

		input_info(true, &ts->client->dev, "%s: lp mode(%d)\n", __func__, ts->plat_data->lowpower_mode);

	} else if (ts->plat_data->ed_enable) {

		nvt_ts_lcd_power_ctrl(ts, true);
		nvt_ts_lcd_reset_ctrl(ts, true);
		mutex_lock(&ts->lock);
		ts->plat_data->power_state = SEC_INPUT_STATE_LPM;
		mutex_unlock(&ts->lock);

		enable_irq_wake(ts->client->irq);

		input_info(true, &ts->client->dev, "%s: ed mode(%d)\n", __func__, ts->plat_data->ed_enable);

	} else {
		disable_irq(ts->irq);
		mutex_lock(&ts->lock);
		ts->plat_data->pinctrl_configure(&ts->client->dev, false);
		ts->plat_data->power_state = SEC_INPUT_STATE_POWER_OFF;
		mutex_unlock(&ts->lock);
		input_info(true, &ts->client->dev, "%s: power off %d\n", __func__, ts->plat_data->power_state);
	}

	if (ts->plat_data->prox_power_off) {
		input_info(true, &ts->client->dev, "%s : cancel touch\n", __func__);
		input_report_key(ts->plat_data->input_dev, KEY_INT_CANCEL, 1);
		input_sync(ts->plat_data->input_dev);
		input_report_key(ts->plat_data->input_dev, KEY_INT_CANCEL, 0);
		input_sync(ts->plat_data->input_dev);
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		if (ts->nplat_data->support_nvt_lpwg_dump) {
			nvt_ts_lpwg_dump_buf_write(ts, lpwg_dump);
		}
	}

	/* release all touches */
	nvt_ts_release_all_finger(ts);

	msleep(50);

	cancel_delayed_work(&ts->work_print_info);
	sec_input_print_info(&ts->client->dev, NULL);

	input_info(true, &ts->client->dev, "%s : end\n", __func__);

	return;
}

static int nvt_ts_hw_init(struct nvt_ts_data *ts)
{
	int ret = 0;

	ts->plat_data->pinctrl_configure(&ts->client->dev, true);

	nvt_ts_regulator_init(ts);

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;
#if IS_ENABLED(CONFIG_TOUCHSCREEN_NVT_SPI)
	nvt_ts_set_spi_mode(ts);
#endif

	nvt_eng_reset(ts);

	sec_delay(10);

	ret = nvt_ts_check_chip_ver_trim(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "chip is not identified\n");
		return -EINVAL;
	}

	ret = nvt_ts_fw_update_on_probe(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "nvt_ts_fw_update_on_probe failed. ret(%d)\n", ret);
		return -EINVAL;
	}

 // need to change dt
 	if (ts->nplat_data->support_nvt_touch_proc) {
		ret = nvt_flash_proc_init(ts);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: nvt flash proc init failed rc = %d\n", __func__, ret);
			return ret;
		}
 	}

	if (ts->nplat_data->support_nvt_touch_ext_proc) {
		ret = nvt_extra_proc_init(ts);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: nvt extra flash proc init failed rc = %d\n", __func__, ret);
			return ret;
		}
	}

	if (ts->nplat_data->support_nvt_lpwg_dump) {
		nvt_ts_lpwg_dump_buf_init(ts);
	}

	return ret;
}

static int nvt_ts_parse_dt(struct device *dev, struct nvt_ts_platdata *platdata)
{
	struct device_node *np = dev->of_node;
	int ret = 0;
	int tmp[3];
	int lcd_id1_gpio = 0, lcd_id2_gpio = 0, lcd_id3_gpio = 0, dt_lcdtype;
	int fw_name_cnt;
	int lcdtype_cnt;
	int fw_sel_idx = 0;
	int lcdtype = 0;

	lcdtype = sec_input_get_lcd_id(dev);
	if (lcdtype < 0) {
		input_err(true, dev, "lcd is not attached\n");
		return -ENODEV;
	}

	fw_name_cnt = of_property_count_strings(np, "novatek,fw_name");

	if (fw_name_cnt == 0) {
		input_err(true, dev, "%s: no fw_name in DT\n", __func__);
		return -EINVAL;

	} else if (fw_name_cnt == 1) {
		ret = of_property_read_u32(np, "novatek,lcdtype", &dt_lcdtype);
		if (ret < 0) {
			input_err(true, dev, "%s: failed to read novatek,lcdtype\n", __func__);

		} else {
			input_info(true, dev, "%s: fw_name_cnt(1), ap lcdtype=0x%06X & dt lcdtype=0x%06X\n",
								__func__, lcdtype, dt_lcdtype);
			if (lcdtype != dt_lcdtype) {
				input_err(true, dev, "%s: panel mismatched, unload driver\n", __func__);
				return -EINVAL;
			}
		}
	} else {

		lcd_id1_gpio = of_get_named_gpio(np, "novatek,lcdid1-gpio", 0);
		if (gpio_is_valid(lcd_id1_gpio))
			input_info(true, dev, "%s: lcd id1_gpio %d(%d)\n", __func__, lcd_id1_gpio, gpio_get_value(lcd_id1_gpio));
		else {
			input_err(true, dev, "%s: Failed to get novatek,lcdid1-gpio\n", __func__);
			return -EINVAL;
		}

		lcd_id2_gpio = of_get_named_gpio(np, "novatek,lcdid2-gpio", 0);
		if (gpio_is_valid(lcd_id2_gpio))
			input_info(true, dev, "%s: lcd id2_gpio %d(%d)\n", __func__, lcd_id2_gpio, gpio_get_value(lcd_id2_gpio));
		else {
			input_err(true, dev, "%s: Failed to get novatek,lcdid2-gpio\n", __func__);
			return -EINVAL;
		}

		/* support lcd id3 */
		lcd_id3_gpio = of_get_named_gpio(np, "novatek,lcdid3-gpio", 0);
		if (gpio_is_valid(lcd_id3_gpio)) {
			input_info(true, dev, "%s: lcd id3_gpio %d(%d)\n", __func__, lcd_id3_gpio, gpio_get_value(lcd_id3_gpio));
			fw_sel_idx = (gpio_get_value(lcd_id3_gpio) << 2) | (gpio_get_value(lcd_id2_gpio) << 1) | gpio_get_value(lcd_id1_gpio);

		} else {
			input_err(true, dev, "%s: Failed to get novatek,lcdid3-gpio and use #1 &#2 id\n", __func__);
			fw_sel_idx = (gpio_get_value(lcd_id2_gpio) << 1) | gpio_get_value(lcd_id1_gpio);
		}

		lcdtype_cnt = of_property_count_u32_elems(np, "novatek,lcdtype");
		input_info(true, dev, "%s: fw_name_cnt(%d) & lcdtype_cnt(%d) & fw_sel_idx(%d)\n",
					__func__, fw_name_cnt, lcdtype_cnt, fw_sel_idx);

		if (lcdtype_cnt <= 0 || fw_name_cnt <= 0 || lcdtype_cnt <= fw_sel_idx || fw_name_cnt <= fw_sel_idx) {
			input_err(true, dev, "%s: abnormal lcdtype & fw name count, fw_sel_idx(%d)\n", __func__, fw_sel_idx);
			return -EINVAL;
		}
		of_property_read_u32_index(np, "novatek,lcdtype", fw_sel_idx, &dt_lcdtype);
		input_info(true, dev, "%s: lcd id(%d), ap lcdtype=0x%06X & dt lcdtype=0x%06X\n",
						__func__, fw_sel_idx, lcdtype, dt_lcdtype);
	}

	if (of_property_read_u32_index(np, "novatek,resume_lp_delay", fw_sel_idx, &platdata->resume_lp_delay)) {
		input_err(true, dev, "%s: fail to get resume_lp_delay & set default 15 ms\n", __func__);
		platdata->resume_lp_delay = 15;
	}
	input_info(true, dev, "%s: resume_lp_delay(%d ms)\n", __func__, platdata->resume_lp_delay);

	of_property_read_string_index(np, "novatek,fw_name_mp", fw_sel_idx, &platdata->firmware_name_mp);
	if (platdata->firmware_name_mp == NULL || strlen(platdata->firmware_name_mp) == 0) {
		input_err(true, dev, "%s: Failed to get mp fw name\n", __func__);
		return -EINVAL;
	} else {
		input_info(true, dev, "%s: fw name(%s)\n", __func__, platdata->firmware_name_mp);
	}

	/* lcd regulator */
	if (of_property_read_string(np, "novatek,name_lcd_rst", &platdata->name_lcd_rst)) {
		input_err(true, dev, "%s: Failed to get name_lcd_rst property\n", __func__);
		platdata->name_lcd_rst = NULL;
	}

	if (of_property_read_string(np, "novatek,name_lcd_vddi", &platdata->name_lcd_vddi)) {
		input_err(true, dev, "%s: Failed to get name_lcd_vddi property\n", __func__);
		platdata->name_lcd_vddi = NULL;
	}

	if (of_property_read_string(np, "novatek,name_lcd_bl_en", &platdata->name_lcd_bl_en)) {
		input_err(true, dev, "%s: Failed to get name_lcd_bl_en property\n", __func__);
		platdata->name_lcd_bl_en = NULL;
	}

	if (of_property_read_string(np, "novatek,name_lcd_vsp", &platdata->name_lcd_vsp)) {
		input_err(true, dev, "%s: Failed to get name_lcd_vsp name property\n", __func__);
		platdata->name_lcd_vsp = NULL;
	}

	if (of_property_read_string(np, "novatek,name_lcd_vsn", &platdata->name_lcd_vsn)) {
		input_err(true, dev, "%s: Failed to get name_lcd_vsn name property\n", __func__);
		platdata->name_lcd_vsn = NULL;
	}

	
	ret = of_property_read_u32(np, "novatek,swrst-n8-addr", &platdata->swrst_n8_addr);
	if (ret) {
		input_err(true, dev, "%s: error reading novatek,swrst-n8-addr. ret=%d\n", __func__, ret);
		return -ENXIO;
	} else {
		input_info(true, dev, "%s: SWRST_N8_ADDR=0x%06X\n", __func__, platdata->swrst_n8_addr);
	}

	ret = of_property_read_u32(np, "novatek,spi-rd-fast-addr", &platdata->spi_rd_fast_addr);
	if (ret) {
		input_info(true, dev, "%s: not support novatek,spi-rd-fast-addr\n", __func__);
		platdata->spi_rd_fast_addr = 0;
//		ret = ERR_PTR(-ENXIO);
	} else {
		input_info(true, dev, "%s: SPI_RD_FAST_ADDR=0x%06X\n", __func__, platdata->spi_rd_fast_addr);
	}

	ret = of_property_read_u32_array(np, "novatek,max_touch_num", tmp, 1);
	if (!ret) {
		platdata->max_touch_num = tmp[0];
	} else {
		platdata->max_touch_num = TOUCH_MAX_FINGER_NUM;
	}

	platdata->prox_lp_scan_enabled = of_property_read_bool(np, "novatek,prox_lp_scan_enabled");
	input_info(true, dev, "%s: Prox LP Scan enabled %s\n",
				__func__, platdata->prox_lp_scan_enabled ? "ON" : "OFF");

	platdata->enable_glove_mode = of_property_read_bool(np, "novatek,enable_glove_mode");
	input_info(true, dev, "%s: glove enabled %s\n",
				__func__, platdata->enable_glove_mode ? "ON" : "OFF");

	platdata->support_nvt_touch_proc = of_property_read_bool(np, "novatek,support_nvt_touch_proc");
	input_info(true, dev, "%s: support_nvt_touch_proc %s\n",
				__func__, platdata->support_nvt_touch_proc ? "ON" : "OFF");
	
	platdata->support_nvt_touch_ext_proc = of_property_read_bool(np, "novatek,support_nvt_touch_ext_proc");
		input_info(true, dev, "%s: support_nvt_touch_ext_proc %s\n",
					__func__, platdata->support_nvt_touch_ext_proc ? "ON" : "OFF");

	platdata->support_nvt_lpwg_dump = of_property_read_bool(np, "novatek,support_nvt_lpwg_dump");
		input_info(true, dev, "%s: support_nvt_lpwg_dump %s\n",
					__func__, platdata->support_nvt_lpwg_dump ? "ON" : "OFF");

	return 0;

}

static int nvt_ts_init(struct nvt_ts_data *ts)
{
	int ret = 0;

	if (ts->client->dev.of_node) {
		ret = sec_input_parse_dt(&ts->client->dev);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: Failed to parse dt\n", __func__);
			goto error_allocate_mem;
		}

		ret = nvt_ts_parse_dt(&ts->client->dev, ts->nplat_data);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: Failed to parse dt\n", __func__);
			goto error_allocate_mem;
		}
		//added incell dtsi
	} else {
		ts->plat_data = ts->client->dev.platform_data;
		if (!ts->plat_data) {
			ret = -ENOMEM;
			input_err(true, &ts->client->dev, "%s: No platform data found\n", __func__);
			goto error_allocate_pdata;
		}
	}

	ts->plat_data->pinctrl = devm_pinctrl_get(&ts->client->dev);
	if (IS_ERR(ts->plat_data->pinctrl))
		input_err(true, &ts->client->dev, "%s: could not get pinctrl\n", __func__);

	ts->client->irq = gpio_to_irq(ts->plat_data->irq_gpio);

	ts->irq = ts->client->irq;

	ts->plat_data->pinctrl_configure = sec_input_pinctrl_configure;

	ptsp = &ts->client->dev;

	INIT_DELAYED_WORK(&ts->work_print_info, nvt_ts_print_info_work);
	INIT_DELAYED_WORK(&ts->work_read_info, nvt_read_info_work);

	mutex_init(&ts->read_write_lock);
	mutex_init(&ts->lock);

	ts->plat_data->sec_ws = wakeup_source_register(&ts->client->dev, "tsp");
	device_init_wakeup(&ts->client->dev, true);

	init_completion(&ts->plat_data->resume_done);
	complete_all(&ts->plat_data->resume_done);

	ret = sec_input_device_register(&ts->client->dev, ts);
	if (ret) {
		input_err(true, &ts->client->dev, "failed to register input device, %d\n", ret);
		goto err_register_input_device;
	}

	ret = nvt_ts_fn_init(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: fail to init fn\n", __func__);
		goto err_fn_init;
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (sysfs_create_group(&ts->plat_data->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &ts->client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(ts);

	sec_secure_touch_register(ts, ts->plat_data->ss_touch_num, &ts->plat_data->input_dev->dev.kobj);

#if IS_ENABLED(CONFIG_GH_RM_DRV)
	nvt_ts_trusted_touch_init(ts);
	mutex_init(&ts->clk_io_ctrl_mutex);
	mutex_init(&ts->transition_lock);
#endif
#endif
	input_info(true, &ts->client->dev, "%s: init resource\n", __func__);

	return 0;

err_fn_init:
err_register_input_device:
	wakeup_source_unregister(ts->plat_data->sec_ws);
	regulator_put(ts->plat_data->dvdd);
	regulator_put(ts->plat_data->avdd);
error_allocate_pdata:
error_allocate_mem:
	input_err(true, &ts->client->dev, "%s: failed(%d)\n", __func__, ret);
	input_log_fix();
	return ret;
}

void nvt_ts_release(struct nvt_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_unregister(&ts->vbus_nb);
#endif

	cancel_delayed_work_sync(&ts->work_print_info);
	cancel_delayed_work_sync(&ts->work_read_info);

	nvt_ts_sec_fn_remove(ts);

	
	if (ts->nplat_data->support_nvt_touch_ext_proc)
		nvt_extra_proc_deinit(ts);

	if (ts->nplat_data->support_nvt_touch_proc)
		nvt_flash_proc_deinit(ts);

	device_init_wakeup(&ts->client->dev, false);
	wakeup_source_unregister(ts->plat_data->sec_ws);

	ts->plat_data->lowpower_mode = false;

	if (ts->nplat_data->name_lcd_rst)
		regulator_put(ts->regulator_lcd_rst);
	if (ts->nplat_data->name_lcd_vddi)
		regulator_put(ts->regulator_lcd_vddi);
	if (ts->nplat_data->name_lcd_bl_en)
		regulator_put(ts->regulator_lcd_bl_en);
	if (ts->nplat_data->name_lcd_vsp)
		regulator_put(ts->regulator_lcd_vsp);
	if (ts->nplat_data->name_lcd_vsn)
		regulator_put(ts->regulator_lcd_vsn);

}


int nvt_ts_probe(struct nvt_ts_data *ts)
{
	int ret = 0;
#if IS_MODULE(CONFIG_TOUCHSCREEN_NVT) || IS_MODULE(CONFIG_TOUCHSCREEN_NVT_SPI)
	static int deferred_flag = 0;

	if (!deferred_flag) {
		deferred_flag = 1;
		input_info(true, &ts->client->dev, "deferred_flag boot %s\n", __func__);
		return -EPROBE_DEFER;
	}
#endif
	input_info(true, &ts->client->dev, "%s\n", __func__);

	ret = nvt_ts_init(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to init resource\n", __func__);
		return ret;
	}

	ret = nvt_ts_hw_init(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to init hw\n", __func__);
		nvt_ts_release(ts);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: request_irq = %d 0x%4x\n", __func__, ts->client->irq, ts->plat_data->irq_flag);
	ret = request_threaded_irq(ts->client->irq, NULL, nvt_ts_irq_thread,
			ts->plat_data->irq_flag, NVT_TS_SPI_NAME, ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Unable to request threaded irq\n", __func__);
		nvt_ts_release(ts);
		return ret;
	}

	ts->plat_data->enabled = true;

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_register(&ts->vbus_nb, nvt_ts_vbus_notification,
						VBUS_NOTIFY_DEV_CHARGER);
#endif
	
#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
	ts->nb.priority = 1;
	ts->nb.notifier_call = nvt_notifier_call;
	ss_panel_notifier_register(&ts->nb);
#elif defined(EXYNOS_DISPLAY_INPUT_NOTIFIER)
	ts->nb.priority = 1;
	ts->nb.notifier_call = nvt_notifier_call;
	panel_notifier_register(&ts->nb);
#endif

	nvt_ts_mode_read(ts);
	input_info(true, &ts->client->dev, "%s: %s prox(1) & AOT\n",
				__func__, ts->prox_in_aot ? "Support" : "Not support");

	input_err(true, &ts->client->dev, "%s: done\n", __func__);
	input_log_fix();

	if (!ts->plat_data->shutdown_called)
		schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(50));

	return 0;
}

int nvt_ts_remove(struct nvt_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP) && IS_ENABLED(CONFIG_TOUCHSCREEN_nvt_SPI)
	nvt_ts_tool_proc_remove();
#endif
	mutex_lock(&ts->lock);
	ts->plat_data->shutdown_called = true;
	mutex_unlock(&ts->lock);

	disable_irq_nosync(ts->client->irq);
	free_irq(ts->client->irq, ts);

	nvt_ts_release(ts);

	return 0;
}

void nvt_ts_shutdown(struct nvt_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	nvt_ts_remove(ts);
}


#if IS_ENABLED(CONFIG_PM)
int nvt_ts_pm_suspend(struct nvt_ts_data *ts)
{
	reinit_completion(&ts->plat_data->resume_done);

	return 0;
}

int nvt_ts_pm_resume(struct nvt_ts_data *ts)
{
	complete_all(&ts->plat_data->resume_done);

	return 0;
}
#endif

MODULE_LICENSE("GPL");
