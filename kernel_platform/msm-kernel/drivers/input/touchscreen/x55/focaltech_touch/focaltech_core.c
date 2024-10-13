/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2019, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*****************************************************************************
*
* File Name: focaltech_core.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: entrance for focaltech ts driver
*
* Version: V1.0
*
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>
#include <linux/of_irq.h>
#include <linux/soc/qcom/panel_event_notifier.h>
#if defined(CONFIG_DRM)
#include <drm/drm_panel.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1     /* Early-suspend level */
#endif
#include "focaltech_core.h"

#if defined(CONFIG_FTS_I2C_TRUSTED_TOUCH)
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include "linux/gunyah/gh_msgq.h"
#include "linux/gunyah/gh_rm_drv.h"
#include <linux/sort.h>
#include <linux/pinctrl/qcom-pinctrl.h>
#endif
/*M55 code for SR-QN6887A-01-356 by tangzhen at 20231007 start*/
#ifndef CONFIG_ARCH_QTI_VM
#include <linux/soc/qcom/battery_charger.h>
#endif// CONFIG_ARCH_QTI_VM
/*M55 code for SR-QN6887A-01-356 by tangzhen at 20231007 end*/

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
/*M55 code for P231106-08987 by yuli at 20231113 start*/
#define FTS_DRIVER_NAME                     "sec_touchscreen"
/*M55 code for P231106-08987 by yuli at 20231113 end*/
#define INTERVAL_READ_REG                   200  /* unit:ms */
#define TIMEOUT_READ_REG                    1000 /* unit:ms */
#if FTS_POWER_SOURCE_CUST_EN
/*M55 code for SR-QN6887A-01-369 by yuli at 20231017 start*/
#define FTS_VTG_MIN_UV                      3300000
/*M55 code for SR-QN6887A-01-369 by yuli at 20231017 end*/
#define FTS_VTG_MAX_UV                      3300000
#define FTS_LOAD_MAX_UA                     30000
#define FTS_LOAD_AVDD_UA                    10000
#define FTS_LOAD_DISABLE_UA                 0
#define FTS_I2C_VTG_MIN_UV                  1800000
#define FTS_I2C_VTG_MAX_UV                  1800000
#endif

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_ts_data *fts_data;

#if defined(CONFIG_DRM)
static struct drm_panel *active_panel;
static void fts_ts_panel_notifier_callback(enum panel_event_notifier_tag tag,
         struct panel_event_notification *event, void *client_data);
#endif

static struct ft_chip_t ctype[] = {
    {0x88, 0x56, 0x52, 0x00, 0x00, 0x00, 0x00, 0x56, 0xB2},
    {0x81, 0x54, 0x52, 0x54, 0x52, 0x00, 0x00, 0x54, 0x5C},
    {0x1C, 0x87, 0x26, 0x87, 0x20, 0x87, 0xA0, 0x00, 0x00},
    {0x90, 0x56, 0x72, 0x00, 0x00, 0x00, 0x00, 0x36, 0xB3},
};

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);
static irqreturn_t fts_irq_handler(int irq, void *data);
static int fts_ts_probe_delayed(struct fts_ts_data *fts_data);
static int fts_ts_enable_reg(struct fts_ts_data *ts_data, bool enable);

static void fts_ts_register_for_panel_events(struct device_node *dp,
                    struct fts_ts_data *ts_data)
{
    const char *touch_type;
    int rc = 0;
    void *cookie = NULL;
    FTS_INFO("+++++");
    rc = of_property_read_string(dp, "focaltech,touch-type",
                        &touch_type);
    if (rc) {
        dev_warn(&fts_data->client->dev,
            "%s: No touch type\n", __func__);
        return;
    }
    if (strcmp(touch_type, "primary")) {
        FTS_ERROR("Invalid touch type");
        return;
    }

    cookie = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,
            PANEL_EVENT_NOTIFIER_CLIENT_PRIMARY_TOUCH, active_panel,
            &fts_ts_panel_notifier_callback, ts_data);
    if (!cookie) {
        FTS_ERROR("Failed to register for panel events");
        return;
    }

    FTS_DEBUG("registered for panel notifications panel: 0x%x\n",
            active_panel);

    ts_data->notifier_cookie = cookie;
    FTS_INFO("-----");
}

#ifdef CONFIG_FTS_I2C_TRUSTED_TOUCH

static void fts_ts_trusted_touch_abort_handler(struct fts_ts_data *fts_data,
                        int error);
static struct gh_acl_desc *fts_ts_vm_get_acl(enum gh_vm_names vm_name)
{
    struct gh_acl_desc *acl_desc;
    gh_vmid_t vmid;

    gh_rm_get_vmid(vm_name, &vmid);
    FTS_INFO("+++++");
    acl_desc = kzalloc(offsetof(struct gh_acl_desc, acl_entries[1]),
            GFP_KERNEL);
    if (!acl_desc)
        return ERR_PTR(ENOMEM);

    acl_desc->n_acl_entries = 1;
    acl_desc->acl_entries[0].vmid = vmid;
    acl_desc->acl_entries[0].perms = GH_RM_ACL_R | GH_RM_ACL_W;
    FTS_INFO("-----");
    return acl_desc;
}

static struct gh_sgl_desc *fts_ts_vm_get_sgl(
                struct trusted_touch_vm_info *vm_info)
{
    struct gh_sgl_desc *sgl_desc;
    int i;
    FTS_INFO("+++++");
    sgl_desc = kzalloc(offsetof(struct gh_sgl_desc,
            sgl_entries[vm_info->iomem_list_size]), GFP_KERNEL);
    if (!sgl_desc)
        return ERR_PTR(ENOMEM);

    sgl_desc->n_sgl_entries = vm_info->iomem_list_size;

    for (i = 0; i < vm_info->iomem_list_size; i++) {
        sgl_desc->sgl_entries[i].ipa_base = vm_info->iomem_bases[i];
        sgl_desc->sgl_entries[i].size = vm_info->iomem_sizes[i];
    }
    FTS_INFO("-----");
    return sgl_desc;
}

static int fts_ts_populate_vm_info_iomem(struct fts_ts_data *fts_data)
{
    int i, gpio, rc = 0;
    int num_regs, num_sizes, num_gpios, list_size;
    struct resource res;
    struct device_node *np = fts_data->dev->of_node;
    struct trusted_touch_vm_info *vm_info = fts_data->vm_info;
    FTS_INFO("+++++");
    num_regs = of_property_count_u32_elems(np, "focaltech,trusted-touch-io-bases");
    if (num_regs < 0) {
        FTS_ERROR("Invalid number of IO regions specified\n");
        return -EINVAL;
    }

    num_sizes = of_property_count_u32_elems(np, "focaltech,trusted-touch-io-sizes");
    if (num_sizes < 0) {
        FTS_ERROR("Invalid number of IO regions specified\n");
        return -EINVAL;
    }

    if (num_regs != num_sizes) {
        FTS_ERROR("IO bases and sizes array lengths mismatch\n");
        return -EINVAL;
    }

    num_gpios = of_gpio_named_count(np, "focaltech,trusted-touch-vm-gpio-list");
    if (num_gpios < 0) {
        dev_warn(fts_data->dev, "Ignoring invalid trusted gpio list: %d\n", num_gpios);
        num_gpios = 0;
    }

    list_size = num_regs + num_gpios;
    vm_info->iomem_list_size = list_size;
    vm_info->iomem_bases = devm_kcalloc(fts_data->dev, list_size, sizeof(*vm_info->iomem_bases),
            GFP_KERNEL);
    if (!vm_info->iomem_bases)
        return -ENOMEM;

    vm_info->iomem_sizes = devm_kcalloc(fts_data->dev, list_size, sizeof(*vm_info->iomem_sizes),
            GFP_KERNEL);
    if (!vm_info->iomem_sizes)
        return -ENOMEM;

    for (i = 0; i < num_gpios; ++i) {
        gpio = of_get_named_gpio(np, "focaltech,trusted-touch-vm-gpio-list", i);
        if (gpio < 0 || !gpio_is_valid(gpio)) {
            FTS_ERROR("Invalid gpio %d at position %d\n", gpio, i);
            return gpio;
        }

        if (!msm_gpio_get_pin_address(gpio, &res)) {
            FTS_ERROR("Failed to retrieve gpio-%d resource\n", gpio);
            return -ENODATA;
        }

        vm_info->iomem_bases[i] = res.start;
        vm_info->iomem_sizes[i] = resource_size(&res);
    }

    rc = of_property_read_u32_array(np, "focaltech,trusted-touch-io-bases",
            &vm_info->iomem_bases[i], list_size - i);
    if (rc) {
        FTS_ERROR("Failed to read trusted touch io bases:%d\n", rc);
        return rc;
    }

    rc = of_property_read_u32_array(np, "focaltech,trusted-touch-io-sizes",
            &vm_info->iomem_sizes[i], list_size - i);
    if (rc) {
        FTS_ERROR("Failed to read trusted touch io sizes:%d\n", rc);
        return rc;
    }
    FTS_INFO("-----");
    return 0;
}

static int fts_ts_populate_vm_info(struct fts_ts_data *fts_data)
{
    int rc;
    struct trusted_touch_vm_info *vm_info;
    struct device_node *np = fts_data->dev->of_node;
    FTS_INFO("+++++");
    vm_info = devm_kzalloc(fts_data->dev, sizeof(struct trusted_touch_vm_info), GFP_KERNEL);
    if (!vm_info)
        return -ENOMEM;

    fts_data->vm_info = vm_info;
    vm_info->vm_name = GH_TRUSTED_VM;
    rc = of_property_read_u32(np, "focaltech,trusted-touch-spi-irq", &vm_info->hw_irq);
    if (rc) {
        FTS_ERROR("Failed to read trusted touch SPI irq:%d", rc);
        return rc;
    }

    rc = fts_ts_populate_vm_info_iomem(fts_data);
    if (rc) {
        FTS_ERROR("Failed to read trusted touch mmio ranges:%d", rc);
        return rc;
    }

    rc = of_property_read_string(np, "focaltech,trusted-touch-type",
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

#ifdef CONFIG_ARCH_QTI_VM
    rc = of_property_read_u32(np, "focaltech,reset-gpio-base", &vm_info->reset_gpio_base);
    if (rc)
        FTS_ERROR("Failed to read reset gpio base:%d", rc);

    rc = of_property_read_u32(np, "focaltech,intr-gpio-base", &vm_info->intr_gpio_base);
    if (rc)
        FTS_ERROR("Failed to read intr gpio base:%d", rc);
#endif
    FTS_INFO("-----");
    return 0;
}

static void fts_ts_destroy_vm_info(struct fts_ts_data *fts_data)
{
    FTS_INFO("+++++");
    kfree(fts_data->vm_info->iomem_sizes);
    kfree(fts_data->vm_info->iomem_bases);
    kfree(fts_data->vm_info);
    FTS_INFO("-----");
}

static void fts_ts_vm_deinit(struct fts_ts_data *fts_data)
{
    FTS_INFO("+++++");
    if (fts_data->vm_info->mem_cookie)
        gh_mem_notifier_unregister(fts_data->vm_info->mem_cookie);
    fts_ts_destroy_vm_info(fts_data);
    FTS_INFO("-----");
}

static int fts_ts_trusted_touch_get_vm_state(struct fts_ts_data *fts_data)
{
    FTS_INFO("+++++");
    return atomic_read(&fts_data->vm_info->vm_state);
    FTS_INFO("-----");
}

static void fts_ts_trusted_touch_set_vm_state(struct fts_ts_data *fts_data,
                            int state)
{
    FTS_INFO("+++++");
    atomic_set(&fts_data->vm_info->vm_state, state);
    FTS_INFO("-----");
}

#ifdef CONFIG_ARCH_QTI_VM
static int fts_ts_vm_mem_release(struct fts_ts_data *fts_data);
static void fts_ts_trusted_touch_tvm_vm_mode_disable(struct fts_ts_data *fts_data);
static void fts_ts_trusted_touch_abort_tvm(struct fts_ts_data *fts_data);
static void fts_ts_trusted_touch_event_notify(struct fts_ts_data *fts_data, int event);

void fts_ts_trusted_touch_tvm_i2c_failure_report(struct fts_ts_data *fts_data)
{
    FTS_ERROR("initiating trusted touch abort due to i2c failure\n");
    fts_ts_trusted_touch_abort_handler(fts_data,
            TRUSTED_TOUCH_EVENT_I2C_FAILURE);
}

static void fts_ts_trusted_touch_reset_gpio_toggle(struct fts_ts_data *fts_data)
{
    void __iomem *base;
    FTS_INFO("+++++");
    if (fts_data->bus_type != BUS_TYPE_I2C)
        return;

    if (!fts_data->vm_info->reset_gpio_base) {
        FTS_ERROR("reset_gpio_base is not valid");
        return;
    }

    base = ioremap(fts_data->vm_info->reset_gpio_base, TOUCH_RESET_GPIO_SIZE);
    writel_relaxed(0x1, base + TOUCH_RESET_GPIO_OFFSET);
    /* wait until toggle to finish*/
    wmb();
    writel_relaxed(0x0, base + TOUCH_RESET_GPIO_OFFSET);
    /* wait until toggle to finish*/
    wmb();
    iounmap(base);
    FTS_INFO("-----");
}

static void fts_trusted_touch_intr_gpio_toggle(struct fts_ts_data *fts_data,
        bool enable)
{
    void __iomem *base;
    u32 val;
    FTS_INFO("+++++");
    if (fts_data->bus_type != BUS_TYPE_I2C)
        return;

    if (!fts_data->vm_info->intr_gpio_base) {
        FTS_ERROR("intr_gpio_base is not valid");
        return;
    }

    base = ioremap(fts_data->vm_info->intr_gpio_base, TOUCH_INTR_GPIO_SIZE);
    val = readl_relaxed(base + TOUCH_INTR_GPIO_OFFSET);
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
    FTS_INFO("-----");
}

static int fts_ts_sgl_cmp(const void *a, const void *b)
{
    struct gh_sgl_entry *left = (struct gh_sgl_entry *)a;
    struct gh_sgl_entry *right = (struct gh_sgl_entry *)b;
    FTS_INFO("-");
    return (left->ipa_base - right->ipa_base);
}

static int fts_ts_vm_compare_sgl_desc(struct gh_sgl_desc *expected,
        struct gh_sgl_desc *received)
{
    int idx;
    FTS_INFO("+++++");
    if (expected->n_sgl_entries != received->n_sgl_entries)
        return -E2BIG;
    sort(received->sgl_entries, received->n_sgl_entries,
            sizeof(received->sgl_entries[0]), fts_ts_sgl_cmp, NULL);
    sort(expected->sgl_entries, expected->n_sgl_entries,
            sizeof(expected->sgl_entries[0]), fts_ts_sgl_cmp, NULL);

    for (idx = 0; idx < expected->n_sgl_entries; idx++) {
        struct gh_sgl_entry *left = &expected->sgl_entries[idx];
        struct gh_sgl_entry *right = &received->sgl_entries[idx];

        if ((left->ipa_base != right->ipa_base) ||
                (left->size != right->size)) {
            FTS_ERROR("sgl mismatch: left_base:%d right base:%d left size:%d right size:%d\n",
                    left->ipa_base, right->ipa_base,
                    left->size, right->size);
            return -EINVAL;
        }
    }
    FTS_INFO("-----");
    return 0;
}

static int fts_ts_vm_handle_vm_hardware(struct fts_ts_data *fts_data)
{
    int rc = 0;
    FTS_INFO("+++++");
    if (atomic_read(&fts_data->delayed_vm_probe_pending)) {
        rc = fts_ts_probe_delayed(fts_data);
        if (rc) {
            FTS_ERROR(" Delayed probe failure on VM!\n");
            return rc;
        }
        atomic_set(&fts_data->delayed_vm_probe_pending, 0);
        return rc;
    }

    fts_irq_enable();
    fts_ts_trusted_touch_set_vm_state(fts_data, TVM_INTERRUPT_ENABLED);
    FTS_INFO("-----");
    return rc;
}

static void fts_ts_trusted_touch_tvm_vm_mode_enable(struct fts_ts_data *fts_data)
{

    struct gh_sgl_desc *sgl_desc, *expected_sgl_desc;
    struct gh_acl_desc *acl_desc;
    struct irq_data *irq_data;
    int rc = 0;
    int irq = 0;
    FTS_INFO("+++++");
    if (fts_ts_trusted_touch_get_vm_state(fts_data) != TVM_ALL_RESOURCES_LENT_NOTIFIED) {
        FTS_ERROR("All lend notifications not received\n");
        fts_ts_trusted_touch_event_notify(fts_data,
                TRUSTED_TOUCH_EVENT_NOTIFICATIONS_PENDING);
        return;
    }

    acl_desc = fts_ts_vm_get_acl(GH_TRUSTED_VM);
    if (IS_ERR(acl_desc)) {
        FTS_ERROR("failed to populated acl data:rc=%d\n",
                PTR_ERR(acl_desc));
        goto accept_fail;
    }

    sgl_desc = gh_rm_mem_accept(fts_data->vm_info->vm_mem_handle,
            GH_RM_MEM_TYPE_IO,
            GH_RM_TRANS_TYPE_LEND,
            GH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS |
            GH_RM_MEM_ACCEPT_VALIDATE_LABEL |
            GH_RM_MEM_ACCEPT_DONE,  TRUSTED_TOUCH_MEM_LABEL,
            acl_desc, NULL, NULL, 0);
    if (IS_ERR_OR_NULL(sgl_desc)) {
        FTS_ERROR("failed to do mem accept :rc=%d\n",
                PTR_ERR(sgl_desc));
        goto acl_fail;
    }
    fts_ts_trusted_touch_set_vm_state(fts_data, TVM_IOMEM_ACCEPTED);

    /* Initiate session on tvm */
    if (fts_data->bus_type == BUS_TYPE_I2C)
        rc = pm_runtime_get_sync(fts_data->client->adapter->dev.parent);
    else
        rc = pm_runtime_get_sync(fts_data->spi->master->dev.parent);

    if (rc < 0) {
        FTS_ERROR("failed to get sync rc:%d\n", rc);
        goto acl_fail;
    }
    fts_ts_trusted_touch_set_vm_state(fts_data, TVM_I2C_SESSION_ACQUIRED);

    expected_sgl_desc = fts_ts_vm_get_sgl(fts_data->vm_info);
    if (fts_ts_vm_compare_sgl_desc(expected_sgl_desc, sgl_desc)) {
        FTS_ERROR("IO sg list does not match\n");
        goto sgl_cmp_fail;
    }

    kfree(expected_sgl_desc);
    kfree(acl_desc);

    irq = gh_irq_accept(fts_data->vm_info->irq_label, -1, IRQ_TYPE_EDGE_RISING);
    fts_trusted_touch_intr_gpio_toggle(fts_data, false);
    if (irq < 0) {
        FTS_ERROR("failed to accept irq\n");
        goto accept_fail;
    }
    fts_ts_trusted_touch_set_vm_state(fts_data, TVM_IRQ_ACCEPTED);


    irq_data = irq_get_irq_data(irq);
    if (!irq_data) {
        FTS_ERROR("Invalid irq data for trusted touch\n");
        goto accept_fail;
    }
    if (!irq_data->hwirq) {
        FTS_ERROR("Invalid irq in irq data\n");
        goto accept_fail;
    }
    if (irq_data->hwirq != fts_data->vm_info->hw_irq) {
        FTS_ERROR("Invalid irq lent\n");
        goto accept_fail;
    }

    pr_debug("irq:returned from accept:%d\n", irq);
    fts_data->irq = irq;

    rc = fts_ts_vm_handle_vm_hardware(fts_data);
    if (rc) {
        FTS_ERROR(" Delayed probe failure on VM!\n");
        goto accept_fail;
    }
    atomic_set(&fts_data->trusted_touch_enabled, 1);
    FTS_INFO("trusted touch enabled\n");
    FTS_INFO("-----");
    return;
sgl_cmp_fail:
    kfree(expected_sgl_desc);
acl_fail:
    kfree(acl_desc);
accept_fail:
    fts_ts_trusted_touch_abort_handler(fts_data,
            TRUSTED_TOUCH_EVENT_ACCEPT_FAILURE);
}
static void fts_ts_vm_irq_on_lend_callback(void *data,
                    unsigned long notif_type,
                    enum gh_irq_label label)
{
    struct fts_ts_data *fts_data = data;

    pr_debug("received irq lend request for label:%d\n", label);
    if (fts_ts_trusted_touch_get_vm_state(fts_data) == TVM_IOMEM_LENT_NOTIFIED)
        fts_ts_trusted_touch_set_vm_state(fts_data, TVM_ALL_RESOURCES_LENT_NOTIFIED);
    else
        fts_ts_trusted_touch_set_vm_state(fts_data, TVM_IRQ_LENT_NOTIFIED);
}

static void fts_ts_vm_mem_on_lend_handler(enum gh_mem_notifier_tag tag,
        unsigned long notif_type, void *entry_data, void *notif_msg)
{
    struct gh_rm_notif_mem_shared_payload *payload;
    struct trusted_touch_vm_info *vm_info;
    struct fts_ts_data *fts_data;
    FTS_INFO("+++++");
    fts_data = (struct fts_ts_data *)entry_data;
    vm_info = fts_data->vm_info;
    if (!vm_info) {
        FTS_ERROR("Invalid vm_info\n");
        return;
    }

    if (notif_type != GH_RM_NOTIF_MEM_SHARED ||
            tag != vm_info->mem_tag) {
        FTS_ERROR("Invalid command passed from rm\n");
        return;
    }

    if (!entry_data || !notif_msg) {
        FTS_ERROR("Invalid entry data passed from rm\n");
        return;
    }


    payload = (struct gh_rm_notif_mem_shared_payload  *)notif_msg;
    if (payload->trans_type != GH_RM_TRANS_TYPE_LEND ||
            payload->label != TRUSTED_TOUCH_MEM_LABEL) {
        FTS_ERROR("Invalid label or transaction type\n");
        return;
    }

    vm_info->vm_mem_handle = payload->mem_handle;
    pr_debug("received mem lend request with handle:%d\n",
        vm_info->vm_mem_handle);
    if (fts_ts_trusted_touch_get_vm_state(fts_data) == TVM_IRQ_LENT_NOTIFIED)
        fts_ts_trusted_touch_set_vm_state(fts_data, TVM_ALL_RESOURCES_LENT_NOTIFIED);
    else
        fts_ts_trusted_touch_set_vm_state(fts_data, TVM_IOMEM_LENT_NOTIFIED);
    FTS_INFO("-----");
}

static int fts_ts_vm_mem_release(struct fts_ts_data *fts_data)
{
    int rc = 0;
    FTS_INFO("+++++");
    if (!fts_data->vm_info->vm_mem_handle) {
        FTS_ERROR("Invalid memory handle\n");
        return -EINVAL;
    }

    rc = gh_rm_mem_release(fts_data->vm_info->vm_mem_handle, 0);
    if (rc)
        FTS_ERROR("VM mem release failed: rc=%d\n", rc);

    rc = gh_rm_mem_notify(fts_data->vm_info->vm_mem_handle,
                GH_RM_MEM_NOTIFY_OWNER_RELEASED,
                fts_data->vm_info->mem_tag, 0);
    if (rc)
        FTS_ERROR("Failed to notify mem release to PVM: rc=%d\n");
    pr_debug("vm mem release succeded\n");

    fts_data->vm_info->vm_mem_handle = 0;
    FTS_INFO("-----");
    return rc;
}

static void fts_ts_trusted_touch_tvm_vm_mode_disable(struct fts_ts_data *fts_data)
{
    int rc = 0;
    FTS_INFO("+++++");
    if (atomic_read(&fts_data->trusted_touch_abort_status)) {
        fts_ts_trusted_touch_abort_tvm(fts_data);
        return;
    }

    /*
     * Acquire the transition lock before disabling the IRQ to avoid the race
     * condition with fts_irq_handler. For SVM, it is acquired here and, for PVM,
     * in fts_ts_trusted_touch_pvm_vm_mode_enable().
     */
    mutex_lock(&fts_data->transition_lock);

    fts_irq_disable();
    fts_ts_trusted_touch_set_vm_state(fts_data, TVM_INTERRUPT_DISABLED);

    rc = gh_irq_release(fts_data->vm_info->irq_label);
    if (rc) {
        FTS_ERROR("Failed to release irq rc:%d\n", rc);
        goto error;
    } else {
        fts_ts_trusted_touch_set_vm_state(fts_data, TVM_IRQ_RELEASED);
    }
    rc = gh_irq_release_notify(fts_data->vm_info->irq_label);
    if (rc)
        FTS_ERROR("Failed to notify release irq rc:%d\n", rc);

    pr_debug("vm irq release succeded\n");

    fts_release_all_finger();

    if (fts_data->bus_type == BUS_TYPE_I2C)
        pm_runtime_put_sync(fts_data->client->adapter->dev.parent);
    else
        pm_runtime_put_sync(fts_data->spi->master->dev.parent);

    fts_ts_trusted_touch_set_vm_state(fts_data, TVM_I2C_SESSION_RELEASED);
    rc = fts_ts_vm_mem_release(fts_data);
    if (rc) {
        FTS_ERROR("Failed to release mem rc:%d\n", rc);
        goto error;
    } else {
        fts_ts_trusted_touch_set_vm_state(fts_data, TVM_IOMEM_RELEASED);
    }
    fts_ts_trusted_touch_set_vm_state(fts_data, TRUSTED_TOUCH_TVM_INIT);
    atomic_set(&fts_data->trusted_touch_enabled, 0);
    mutex_unlock(&fts_data->transition_lock);
    FTS_INFO("trusted touch disabled\n");
    FTS_INFO("-----");
    return;
error:
    mutex_unlock(&fts_data->transition_lock);
    fts_ts_trusted_touch_abort_handler(fts_data,
            TRUSTED_TOUCH_EVENT_RELEASE_FAILURE);
}

int fts_ts_handle_trusted_touch_tvm(struct fts_ts_data *fts_data, int value)
{
    int err = 0;
    FTS_INFO("+++++");
    switch (value) {
    case 0:
        if ((atomic_read(&fts_data->trusted_touch_enabled) == 0) &&
            (atomic_read(&fts_data->trusted_touch_abort_status) == 0)) {
            FTS_ERROR("Trusted touch is already disabled\n");
            break;
        }
        if (atomic_read(&fts_data->trusted_touch_mode) ==
                TRUSTED_TOUCH_VM_MODE) {
            fts_ts_trusted_touch_tvm_vm_mode_disable(fts_data);
        } else {
            FTS_ERROR("Unsupported trusted touch mode\n");
        }
        break;

    case 1:
        if (atomic_read(&fts_data->trusted_touch_enabled)) {
            FTS_ERROR("Trusted touch usecase underway\n");
            err = -EBUSY;
            break;
        }
        if (atomic_read(&fts_data->trusted_touch_mode) ==
                TRUSTED_TOUCH_VM_MODE) {
            fts_ts_trusted_touch_tvm_vm_mode_enable(fts_data);
        } else {
            FTS_ERROR("Unsupported trusted touch mode\n");
        }
        break;

    default:
        FTS_ERROR("unsupported value: %lu\n", value);
        err = -EINVAL;
        break;
    }
    FTS_INFO("-----");
    return err;
}

static void fts_ts_trusted_touch_abort_tvm(struct fts_ts_data *fts_data)
{
    int rc = 0;
    int vm_state = fts_ts_trusted_touch_get_vm_state(fts_data);

    if (vm_state >= TRUSTED_TOUCH_TVM_STATE_MAX) {
        FTS_ERROR("invalid tvm driver state: %d\n", vm_state);
        return;
    }
    FTS_INFO("+++++");
    switch (vm_state) {
    case TVM_INTERRUPT_ENABLED:
        fts_irq_disable();
    case TVM_IRQ_ACCEPTED:
    case TVM_INTERRUPT_DISABLED:
        rc = gh_irq_release(fts_data->vm_info->irq_label);
        if (rc)
            FTS_ERROR("Failed to release irq rc:%d\n", rc);
        rc = gh_irq_release_notify(fts_data->vm_info->irq_label);
        if (rc)
            FTS_ERROR("Failed to notify irq release rc:%d\n", rc);
    case TVM_I2C_SESSION_ACQUIRED:
    case TVM_IOMEM_ACCEPTED:
    case TVM_IRQ_RELEASED:
        fts_release_all_finger();
        if (fts_data->bus_type == BUS_TYPE_I2C)
            pm_runtime_put_sync(fts_data->client->adapter->dev.parent);
        else
            pm_runtime_put_sync(fts_data->spi->master->dev.parent);
    case TVM_I2C_SESSION_RELEASED:
        rc = fts_ts_vm_mem_release(fts_data);
        if (rc)
            FTS_ERROR("Failed to release mem rc:%d\n", rc);
    case TVM_IOMEM_RELEASED:
    case TVM_ALL_RESOURCES_LENT_NOTIFIED:
    case TRUSTED_TOUCH_TVM_INIT:
    case TVM_IRQ_LENT_NOTIFIED:
    case TVM_IOMEM_LENT_NOTIFIED:
        atomic_set(&fts_data->trusted_touch_enabled, 0);
    }

    atomic_set(&fts_data->trusted_touch_abort_status, 0);
    fts_ts_trusted_touch_set_vm_state(fts_data, TRUSTED_TOUCH_TVM_INIT);
    FTS_INFO("-----");
}

#else

static void fts_ts_bus_put(struct fts_ts_data *fts_data);

static void fts_ts_trusted_touch_abort_pvm(struct fts_ts_data *fts_data)
{
    int rc = 0;
    int vm_state = fts_ts_trusted_touch_get_vm_state(fts_data);
    FTS_INFO("+++++");
    if (vm_state >= TRUSTED_TOUCH_PVM_STATE_MAX) {
        FTS_ERROR("Invalid driver state: %d\n", vm_state);
        return;
    }

    switch (vm_state) {
    case PVM_IRQ_RELEASE_NOTIFIED:
    case PVM_ALL_RESOURCES_RELEASE_NOTIFIED:
    case PVM_IRQ_LENT:
    case PVM_IRQ_LENT_NOTIFIED:
        rc = gh_irq_reclaim(fts_data->vm_info->irq_label);
        if (rc)
            FTS_ERROR("failed to reclaim irq on pvm rc:%d\n", rc);
    case PVM_IRQ_RECLAIMED:
    case PVM_IOMEM_LENT:
    case PVM_IOMEM_LENT_NOTIFIED:
    case PVM_IOMEM_RELEASE_NOTIFIED:
        rc = gh_rm_mem_reclaim(fts_data->vm_info->vm_mem_handle, 0);
        if (rc)
            FTS_ERROR("failed to reclaim iomem on pvm rc:%d\n", rc);
        fts_data->vm_info->vm_mem_handle = 0;
    case PVM_IOMEM_RECLAIMED:
    case PVM_INTERRUPT_DISABLED:
        fts_irq_enable();
    case PVM_I2C_RESOURCE_ACQUIRED:
    case PVM_INTERRUPT_ENABLED:
        fts_ts_bus_put(fts_data);
    case TRUSTED_TOUCH_PVM_INIT:
    case PVM_I2C_RESOURCE_RELEASED:
        atomic_set(&fts_data->trusted_touch_enabled, 0);
        atomic_set(&fts_data->trusted_touch_transition, 0);
    }

    atomic_set(&fts_data->trusted_touch_abort_status, 0);

    fts_ts_trusted_touch_set_vm_state(fts_data, TRUSTED_TOUCH_PVM_INIT);
    FTS_INFO("-----");
}

static int fts_ts_clk_prepare_enable(struct fts_ts_data *fts_data)
{
    int ret;
    FTS_INFO("+++++");
    ret = clk_prepare_enable(fts_data->iface_clk);
    if (ret) {
        FTS_ERROR("error on clk_prepare_enable(iface_clk):%d\n", ret);
        return ret;
    }

    ret = clk_prepare_enable(fts_data->core_clk);
    if (ret) {
        clk_disable_unprepare(fts_data->iface_clk);
        FTS_ERROR("error clk_prepare_enable(core_clk):%d\n", ret);
    }
    FTS_INFO("-----");
    return ret;
}

static void fts_ts_clk_disable_unprepare(struct fts_ts_data *fts_data)
{
    FTS_INFO("+++++");
    clk_disable_unprepare(fts_data->core_clk);
    clk_disable_unprepare(fts_data->iface_clk);
    FTS_INFO("-----");
}

static int fts_ts_bus_get(struct fts_ts_data *fts_data)
{
    int rc = 0;
    struct device *dev = NULL;
    FTS_INFO("+++++");
    cancel_work_sync(&fts_data->resume_work);
    reinit_completion(&fts_data->trusted_touch_powerdown);
    fts_ts_enable_reg(fts_data, true);

    if (fts_data->bus_type == BUS_TYPE_I2C)
        dev = fts_data->client->adapter->dev.parent;
    else
        dev = fts_data->spi->master->dev.parent;

    mutex_lock(&fts_data->fts_clk_io_ctrl_mutex);
    rc = pm_runtime_get_sync(dev);
    if (rc >= 0 &&  fts_data->core_clk != NULL &&
                fts_data->iface_clk != NULL) {
        rc = fts_ts_clk_prepare_enable(fts_data);
        if (rc)
            pm_runtime_put_sync(dev);
    }
    FTS_INFO("-----");
    mutex_unlock(&fts_data->fts_clk_io_ctrl_mutex);
    return rc;
}

static void fts_ts_bus_put(struct fts_ts_data *fts_data)
{
    struct device *dev = NULL;
    FTS_INFO("+++++");
    if (fts_data->bus_type == BUS_TYPE_I2C)
        dev = fts_data->client->adapter->dev.parent;
    else
        dev = fts_data->spi->master->dev.parent;

    mutex_lock(&fts_data->fts_clk_io_ctrl_mutex);
    if (fts_data->core_clk != NULL && fts_data->iface_clk != NULL)
        fts_ts_clk_disable_unprepare(fts_data);
    pm_runtime_put_sync(dev);
    mutex_unlock(&fts_data->fts_clk_io_ctrl_mutex);
    complete(&fts_data->trusted_touch_powerdown);
    fts_ts_enable_reg(fts_data, false);
    FTS_INFO("-----");
}

static struct gh_notify_vmid_desc *fts_ts_vm_get_vmid(gh_vmid_t vmid)
{
    struct gh_notify_vmid_desc *vmid_desc;
    FTS_INFO("+++++");
    vmid_desc = kzalloc(offsetof(struct gh_notify_vmid_desc,
                vmid_entries[1]), GFP_KERNEL);
    if (!vmid_desc)
        return ERR_PTR(ENOMEM);

    vmid_desc->n_vmid_entries = 1;
    vmid_desc->vmid_entries[0].vmid = vmid;
    FTS_INFO("-----");
    return vmid_desc;
}

static void fts_trusted_touch_pvm_vm_mode_disable(struct fts_ts_data *fts_data)
{
    int rc = 0;
    FTS_INFO("+++++");
    atomic_set(&fts_data->trusted_touch_transition, 1);

    if (atomic_read(&fts_data->trusted_touch_abort_status)) {
        fts_ts_trusted_touch_abort_pvm(fts_data);
        return;
    }

    if (fts_ts_trusted_touch_get_vm_state(fts_data) != PVM_ALL_RESOURCES_RELEASE_NOTIFIED)
        FTS_ERROR("all release notifications are not received yet\n");

    rc = gh_rm_mem_reclaim(fts_data->vm_info->vm_mem_handle, 0);
    if (rc) {
        FTS_ERROR("Trusted touch VM mem reclaim failed rc:%d\n", rc);
        goto error;
    }
    fts_ts_trusted_touch_set_vm_state(fts_data, PVM_IOMEM_RECLAIMED);
    fts_data->vm_info->vm_mem_handle = 0;
    pr_debug("vm mem reclaim succeded!\n");

    rc = gh_irq_reclaim(fts_data->vm_info->irq_label);
    if (rc) {
        FTS_ERROR("failed to reclaim irq on pvm rc:%d\n", rc);
        goto error;
    }
    fts_ts_trusted_touch_set_vm_state(fts_data, PVM_IRQ_RECLAIMED);
    pr_debug("vm irq reclaim succeded!\n");

    fts_irq_enable();
    fts_ts_trusted_touch_set_vm_state(fts_data, PVM_INTERRUPT_ENABLED);
    fts_ts_bus_put(fts_data);
    atomic_set(&fts_data->trusted_touch_transition, 0);
    fts_ts_trusted_touch_set_vm_state(fts_data, PVM_I2C_RESOURCE_RELEASED);
    fts_ts_trusted_touch_set_vm_state(fts_data, TRUSTED_TOUCH_PVM_INIT);
    atomic_set(&fts_data->trusted_touch_enabled, 0);
    FTS_INFO("trusted touch disabled\n");
    FTS_INFO("-----");
    return;
error:
    fts_ts_trusted_touch_abort_handler(fts_data,
            TRUSTED_TOUCH_EVENT_RECLAIM_FAILURE);
}

static void fts_ts_vm_irq_on_release_callback(void *data,
                    unsigned long notif_type,
                    enum gh_irq_label label)
{
    struct fts_ts_data *fts_data = data;
    FTS_INFO("+++++");
    if (notif_type != GH_RM_NOTIF_VM_IRQ_RELEASED) {
        FTS_ERROR("invalid notification type\n");
        return;
    }

    if (fts_ts_trusted_touch_get_vm_state(fts_data) == PVM_IOMEM_RELEASE_NOTIFIED)
        fts_ts_trusted_touch_set_vm_state(fts_data, PVM_ALL_RESOURCES_RELEASE_NOTIFIED);
    else
        fts_ts_trusted_touch_set_vm_state(fts_data, PVM_IRQ_RELEASE_NOTIFIED);
    FTS_INFO("-----");
}

static void fts_ts_vm_mem_on_release_handler(enum gh_mem_notifier_tag tag,
        unsigned long notif_type, void *entry_data, void *notif_msg)
{
    struct gh_rm_notif_mem_released_payload *release_payload;
    struct trusted_touch_vm_info *vm_info;
    struct fts_ts_data *fts_data;
    FTS_INFO("+++++");
    fts_data = (struct fts_ts_data *)entry_data;
    vm_info = fts_data->vm_info;
    if (!vm_info) {
        FTS_ERROR(" Invalid vm_info\n");
        return;
    }

    if (notif_type != GH_RM_NOTIF_MEM_RELEASED) {
        FTS_ERROR(" Invalid notification type\n");
        return;
    }

    if (tag != vm_info->mem_tag) {
        FTS_ERROR(" Invalid tag\n");
        return;
    }

    if (!entry_data || !notif_msg) {
        FTS_ERROR(" Invalid data or notification message\n");
        return;
    }

    release_payload = (struct gh_rm_notif_mem_released_payload  *)notif_msg;
    if (release_payload->mem_handle != vm_info->vm_mem_handle) {
        FTS_ERROR("Invalid mem handle detected\n");
        return;
    }

    if (fts_ts_trusted_touch_get_vm_state(fts_data) == PVM_IRQ_RELEASE_NOTIFIED)
        fts_ts_trusted_touch_set_vm_state(fts_data, PVM_ALL_RESOURCES_RELEASE_NOTIFIED);
    else
        fts_ts_trusted_touch_set_vm_state(fts_data, PVM_IOMEM_RELEASE_NOTIFIED);
    FTS_INFO("-----");
}

static int fts_ts_vm_mem_lend(struct fts_ts_data *fts_data)
{
    struct gh_acl_desc *acl_desc;
    struct gh_sgl_desc *sgl_desc;
    struct gh_notify_vmid_desc *vmid_desc;
    gh_memparcel_handle_t mem_handle;
    gh_vmid_t trusted_vmid;
    int rc = 0;
    FTS_INFO("+++++");
    acl_desc = fts_ts_vm_get_acl(GH_TRUSTED_VM);
    if (IS_ERR(acl_desc)) {
        FTS_ERROR("Failed to get acl of IO memories for Trusted touch\n");
        PTR_ERR(acl_desc);
        return -EINVAL;
    }

    sgl_desc = fts_ts_vm_get_sgl(fts_data->vm_info);
    if (IS_ERR(sgl_desc)) {
        FTS_ERROR("Failed to get sgl of IO memories for Trusted touch\n");
        PTR_ERR(sgl_desc);
        rc = -EINVAL;
        goto sgl_error;
    }

    rc = gh_rm_mem_lend(GH_RM_MEM_TYPE_IO, 0, TRUSTED_TOUCH_MEM_LABEL,
            acl_desc, sgl_desc, NULL, &mem_handle);
    if (rc) {
        FTS_ERROR("Failed to lend IO memories for Trusted touch rc:%d\n",
                            rc);
        goto error;
    }

    FTS_INFO("vm mem lend succeded\n");

    fts_ts_trusted_touch_set_vm_state(fts_data, PVM_IOMEM_LENT);

    gh_rm_get_vmid(GH_TRUSTED_VM, &trusted_vmid);

    vmid_desc = fts_ts_vm_get_vmid(trusted_vmid);

    rc = gh_rm_mem_notify(mem_handle, GH_RM_MEM_NOTIFY_RECIPIENT_SHARED,
            fts_data->vm_info->mem_tag, vmid_desc);
    if (rc) {
        FTS_ERROR("Failed to notify mem lend to hypervisor rc:%d\n", rc);
        goto vmid_error;
    }

    fts_ts_trusted_touch_set_vm_state(fts_data, PVM_IOMEM_LENT_NOTIFIED);

    fts_data->vm_info->vm_mem_handle = mem_handle;
    FTS_INFO("-----");
vmid_error:
    kfree(vmid_desc);
error:
    kfree(sgl_desc);
sgl_error:
    kfree(acl_desc);

    return rc;
}

static int fts_ts_trusted_touch_pvm_vm_mode_enable(struct fts_ts_data *fts_data)
{
    int rc = 0;
    struct trusted_touch_vm_info *vm_info = fts_data->vm_info;
    FTS_INFO("+++++");
    atomic_set(&fts_data->trusted_touch_transition, 1);
    mutex_lock(&fts_data->transition_lock);

    if (fts_data->suspended) {
        FTS_ERROR("Invalid power state for operation\n");
        atomic_set(&fts_data->trusted_touch_transition, 0);
        rc =  -EPERM;
        goto error;
    }

    /* i2c session start and resource acquire */
    if (fts_ts_bus_get(fts_data) < 0) {
        FTS_ERROR("fts_ts_bus_get failed\n");
        rc = -EIO;
        goto error;
    }

    fts_ts_trusted_touch_set_vm_state(fts_data, PVM_I2C_RESOURCE_ACQUIRED);
    /* flush pending interurpts from FIFO */
    fts_irq_disable();
    fts_ts_trusted_touch_set_vm_state(fts_data, PVM_INTERRUPT_DISABLED);
    fts_release_all_finger();

    rc = fts_ts_vm_mem_lend(fts_data);
    if (rc) {
        FTS_ERROR("Failed to lend memory\n");
        goto abort_handler;
    }
    pr_debug("vm mem lend succeded\n");
    rc = gh_irq_lend_v2(vm_info->irq_label, vm_info->vm_name,
        fts_data->irq, &fts_ts_vm_irq_on_release_callback, fts_data);
    if (rc) {
        FTS_ERROR("Failed to lend irq\n");
        goto abort_handler;
    }

    pr_debug("vm irq lend succeded for irq:%d\n", fts_data->irq);
    fts_ts_trusted_touch_set_vm_state(fts_data, PVM_IRQ_LENT);

    rc = gh_irq_lend_notify(vm_info->irq_label);
    if (rc) {
        FTS_ERROR("Failed to notify irq\n");
        goto abort_handler;
    }
    fts_ts_trusted_touch_set_vm_state(fts_data, PVM_IRQ_LENT_NOTIFIED);

    mutex_unlock(&fts_data->transition_lock);
    atomic_set(&fts_data->trusted_touch_transition, 0);
    atomic_set(&fts_data->trusted_touch_enabled, 1);
    FTS_INFO("trusted touch enabled\n");
    FTS_INFO("-----");
    return rc;

abort_handler:
    fts_ts_trusted_touch_abort_handler(fts_data, TRUSTED_TOUCH_EVENT_LEND_FAILURE);

error:
    mutex_unlock(&fts_data->transition_lock);
    return rc;
}

int fts_ts_handle_trusted_touch_pvm(struct fts_ts_data *fts_data, int value)
{
    int err = 0;
    FTS_INFO("+++++");
    switch (value) {
    case 0:
        if (atomic_read(&fts_data->trusted_touch_enabled) == 0 &&
            (atomic_read(&fts_data->trusted_touch_abort_status) == 0)) {
            FTS_ERROR("Trusted touch is already disabled\n");
            break;
        }
        if (atomic_read(&fts_data->trusted_touch_mode) ==
                TRUSTED_TOUCH_VM_MODE) {
            fts_trusted_touch_pvm_vm_mode_disable(fts_data);
        } else {
            FTS_ERROR("Unsupported trusted touch mode\n");
        }
        break;

    case 1:
        if (atomic_read(&fts_data->trusted_touch_enabled)) {
            FTS_ERROR("Trusted touch usecase underway\n");
            err = -EBUSY;
            break;
        }
        if (atomic_read(&fts_data->trusted_touch_mode) ==
                TRUSTED_TOUCH_VM_MODE) {
            err = fts_ts_trusted_touch_pvm_vm_mode_enable(fts_data);
        } else {
            FTS_ERROR("Unsupported trusted touch mode\n");
        }
        break;

    default:
        FTS_ERROR("unsupported value: %lu\n", value);
        err = -EINVAL;
        break;
    }
    FTS_INFO("-----");
    return err;
}

#endif

static void fts_ts_trusted_touch_event_notify(struct fts_ts_data *fts_data, int event)
{
    FTS_INFO("+++++");
    atomic_set(&fts_data->trusted_touch_event, event);
    sysfs_notify(&fts_data->dev->kobj, NULL, "trusted_touch_event");
    FTS_INFO("-----");
}

static void fts_ts_trusted_touch_abort_handler(struct fts_ts_data *fts_data, int error)
{
    FTS_INFO("-----");
    atomic_set(&fts_data->trusted_touch_abort_status, error);
    FTS_ERROR("TUI session aborted with failure:%d\n", error);
    fts_ts_trusted_touch_event_notify(fts_data, error);
#ifdef CONFIG_ARCH_QTI_VM
    FTS_ERROR("Resetting touch controller\n");
    if (fts_ts_trusted_touch_get_vm_state(fts_data) >= TVM_IOMEM_ACCEPTED &&
            error == TRUSTED_TOUCH_EVENT_I2C_FAILURE) {
        FTS_ERROR("Resetting touch controller\n");
        fts_ts_trusted_touch_reset_gpio_toggle(fts_data);
    }
#endif
    FTS_INFO("-----");
}

static int fts_ts_vm_init(struct fts_ts_data *fts_data)
{
    int rc = 0;
    struct trusted_touch_vm_info *vm_info;
    void *mem_cookie;
    FTS_INFO("+++++");
    rc = fts_ts_populate_vm_info(fts_data);
    if (rc) {
        FTS_ERROR("Cannot setup vm pipeline\n");
        rc = -EINVAL;
        goto fail;
    }

    vm_info = fts_data->vm_info;
#ifdef CONFIG_ARCH_QTI_VM
    mem_cookie = gh_mem_notifier_register(vm_info->mem_tag,
            fts_ts_vm_mem_on_lend_handler, fts_data);
    if (!mem_cookie) {
        FTS_ERROR("Failed to register on lend mem notifier\n");
        rc = -EINVAL;
        goto init_fail;
    }
    vm_info->mem_cookie = mem_cookie;
    rc = gh_irq_wait_for_lend_v2(vm_info->irq_label, GH_PRIMARY_VM,
            &fts_ts_vm_irq_on_lend_callback, fts_data);
    fts_ts_trusted_touch_set_vm_state(fts_data, TRUSTED_TOUCH_TVM_INIT);
#else
    mem_cookie = gh_mem_notifier_register(vm_info->mem_tag,
            fts_ts_vm_mem_on_release_handler, fts_data);
    if (!mem_cookie) {
        FTS_ERROR("Failed to register on release mem notifier\n");
        rc = -EINVAL;
        goto init_fail;
    }
    vm_info->mem_cookie = mem_cookie;
    fts_ts_trusted_touch_set_vm_state(fts_data, TRUSTED_TOUCH_PVM_INIT);
#endif
    FTS_INFO("-----");
    return rc;
init_fail:
    fts_ts_vm_deinit(fts_data);
fail:
    return rc;
}

static void fts_ts_dt_parse_trusted_touch_info(struct fts_ts_data *fts_data)
{
    struct device_node *np = fts_data->dev->of_node;
    int rc = 0;
    const char *selection;
    const char *environment;
    FTS_INFO("+++++");
#ifdef CONFIG_ARCH_QTI_VM
    fts_data->touch_environment = "tvm";
#else
    fts_data->touch_environment = "pvm";
#endif

    rc = of_property_read_string(np, "focaltech,trusted-touch-mode",
                                &selection);
    if (rc) {
        dev_warn(fts_data->dev,
            "%s: No trusted touch mode selection made\n", __func__);
        atomic_set(&fts_data->trusted_touch_mode,
                        TRUSTED_TOUCH_MODE_NONE);
        return;
    }

    if (!strcmp(selection, "vm_mode")) {
        atomic_set(&fts_data->trusted_touch_mode,
                        TRUSTED_TOUCH_VM_MODE);
        FTS_ERROR("Selected trusted touch mode to VM mode\n");
    } else {
        atomic_set(&fts_data->trusted_touch_mode,
                        TRUSTED_TOUCH_MODE_NONE);
        FTS_ERROR("Invalid trusted_touch mode\n");
    }

    rc = of_property_read_string(np, "focaltech,touch-environment",
                        &environment);
    if (rc) {
        dev_warn(fts_data->dev,
            "%s: No trusted touch mode environment\n", __func__);
    }
    fts_data->touch_environment = environment;
    FTS_ERROR("Trusted touch environment:%s\n",
            fts_data->touch_environment);
    FTS_INFO("-----");
}

static void fts_ts_trusted_touch_init(struct fts_ts_data *fts_data)
{
    int rc = 0;
    FTS_INFO("+++++");
    atomic_set(&fts_data->trusted_touch_initialized, 0);
    fts_ts_dt_parse_trusted_touch_info(fts_data);

    if (atomic_read(&fts_data->trusted_touch_mode) ==
                        TRUSTED_TOUCH_MODE_NONE)
        return;

    init_completion(&fts_data->trusted_touch_powerdown);

    /* Get clocks */
    fts_data->core_clk = devm_clk_get(fts_data->dev->parent,
                        "m-ahb");
    if (IS_ERR(fts_data->core_clk)) {
        fts_data->core_clk = NULL;
        dev_warn(fts_data->dev,
                "%s: core_clk is not defined\n", __func__);
    }

    fts_data->iface_clk = devm_clk_get(fts_data->dev->parent,
                        "se-clk");
    if (IS_ERR(fts_data->iface_clk)) {
        fts_data->iface_clk = NULL;
        dev_warn(fts_data->dev,
            "%s: iface_clk is not defined\n", __func__);
    }

    if (atomic_read(&fts_data->trusted_touch_mode) ==
                        TRUSTED_TOUCH_VM_MODE) {
        rc = fts_ts_vm_init(fts_data);
        if (rc)
            FTS_ERROR("Failed to init VM\n");
    }
    atomic_set(&fts_data->trusted_touch_initialized, 1);
    FTS_INFO("-----");
}

#endif

/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_wait_tp_to_valid(void)
{
    int ret = 0;
    int cnt = 0;
    u8 idh = 0;
    u8 idl = 0;
    u8 chip_idh = fts_data->ic_info.ids.chip_idh;
    u8 chip_idl = fts_data->ic_info.ids.chip_idl;

    do {
        ret = fts_read_reg(FTS_REG_CHIP_ID, &idh);
        ret = fts_read_reg(FTS_REG_CHIP_ID2, &idl);
        if ((ret < 0) || (idh != chip_idh) || (idl != chip_idl)) {
            FTS_DEBUG("TP Not Ready,ReadData:0x%02x%02x", idh, idl);
        } else if ((idh == chip_idh) && (idl == chip_idl)) {
            FTS_INFO("TP Ready,Device ID:0x%02x%02x", idh, idl);
            return 0;
        }
        cnt++;
        msleep(INTERVAL_READ_REG);
    } while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

    return -EIO;
}

/*****************************************************************************
*  Name: fts_tp_state_recovery
*  Brief: Need execute this function when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_tp_state_recovery(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();
    /* wait tp stable */
    fts_wait_tp_to_valid();
    /* recover TP charger state 0x8B */
    /* recover TP glove state 0xC0 */
    /* recover TP cover state 0xC1 */
    fts_ex_mode_recovery(ts_data);
    /* recover TP gesture state 0xD0 */
    fts_gesture_recovery(ts_data);
    FTS_FUNC_EXIT();
}

int fts_reset_proc(int hdelayms)
{
    FTS_DEBUG("tp reset");

    /*M55 code for QN6887A-3421 by yuli at 20231220 start*/
    if (gpio_get_value(fts_data->pdata->reset_gpio)) {
        fts_write_reg(0xB6, 0x01);
        msleep(20);
    }
    /*M55 code for QN6887A-3421 by yuli at 20231220 end*/

    gpio_direction_output(fts_data->pdata->reset_gpio, 0);
    msleep(1);
    gpio_direction_output(fts_data->pdata->reset_gpio, 1);
    if (hdelayms) {
        msleep(hdelayms);
    }

    return 0;
}

void fts_irq_disable(void)
{
    unsigned long irqflags;

    FTS_FUNC_ENTER();
    spin_lock_irqsave(&fts_data->irq_lock, irqflags);

    if (!fts_data->irq_disabled) {
#ifdef CONFIG_FTS_I2C_TRUSTED_TOUCH
        if (atomic_read(&fts_data->trusted_touch_transition))
            disable_irq_wake(fts_data->irq);
        else
            disable_irq_nosync(fts_data->irq);
#else
        disable_irq_nosync(fts_data->irq);
#endif
        fts_data->irq_disabled = true;
    }

    spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
    FTS_FUNC_EXIT();
}

void fts_irq_enable(void)
{
    unsigned long irqflags = 0;
    struct irq_desc *desc = irq_to_desc(fts_data->irq);
    FTS_FUNC_ENTER();
    FTS_DEBUG("irq_depth:%d\n", desc->depth);
    spin_lock_irqsave(&fts_data->irq_lock, irqflags);

    if ((desc->depth == 1) || (fts_data->irq_disabled)) {
#ifdef CONFIG_FTS_I2C_TRUSTED_TOUCH
        if (atomic_read(&fts_data->trusted_touch_transition))
            enable_irq_wake(fts_data->irq);
        else
            enable_irq(fts_data->irq);
#else
        enable_irq(fts_data->irq);
#endif
        fts_data->irq_disabled = false;
    }

    spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
    FTS_FUNC_EXIT();
}

void fts_hid2std(void)
{
    int ret = 0;
    u8 buf[3] = {0xEB, 0xAA, 0x09};

    if (fts_data->bus_type != BUS_TYPE_I2C)
        return;

    ret = fts_write(buf, 3);
    if (ret < 0) {
        FTS_ERROR("hid2std cmd write fail");
        return;
    }

    msleep(20);
    buf[0] = buf[1] = buf[2] = 0;
    ret = fts_read(NULL, 0, buf, 3);
    if (ret < 0)
        FTS_ERROR("hid2std cmd read fail");
    else if ((buf[0] == 0xEB) && (buf[1] == 0xAA) && (buf[2] == 0x08))
        FTS_DEBUG("hidi2c change to stdi2c successful");
    else
        FTS_DEBUG("hidi2c change to stdi2c not support or fail");

}

static int fts_get_chip_types(
    struct fts_ts_data *ts_data,
    u8 id_h, u8 id_l, bool fw_valid)
{
    int i = 0;
    u32 ctype_entries = sizeof(ctype) / sizeof(struct ft_chip_t);

    if ((0x0 == id_h) || (0x0 == id_l)) {
        FTS_ERROR("id_h/id_l is 0");
        return -EINVAL;
    }

    FTS_DEBUG("verify id:0x%02x%02x", id_h, id_l);
    for (i = 0; i < ctype_entries; i++) {
        if (VALID == fw_valid) {
            if ((id_h == ctype[i].chip_idh) && (id_l == ctype[i].chip_idl))
                break;
        } else {
            if (((id_h == ctype[i].rom_idh) && (id_l == ctype[i].rom_idl))
                || ((id_h == ctype[i].pb_idh) && (id_l == ctype[i].pb_idl))
                || ((id_h == ctype[i].bl_idh) && (id_l == ctype[i].bl_idl)))
            break;
        }
    }

    if (i >= ctype_entries)
        return -ENODATA;

    ts_data->ic_info.ids = ctype[i];
    return 0;
}

static int fts_read_bootid(struct fts_ts_data *ts_data, u8 *id)
{
    int ret = 0;
    u8 chip_id[2] = { 0 };
    u8 id_cmd[4] = { 0 };
    u32 id_cmd_len = 0;

    id_cmd[0] = FTS_CMD_START1;
    id_cmd[1] = FTS_CMD_START2;
    ret = fts_write(id_cmd, 2);
    if (ret < 0) {
        FTS_ERROR("start cmd write fail");
        return ret;
    }

    msleep(FTS_CMD_START_DELAY);
    id_cmd[0] = FTS_CMD_READ_ID;
    id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
    if (ts_data->ic_info.is_incell)
        id_cmd_len = FTS_CMD_READ_ID_LEN_INCELL;
    else
        id_cmd_len = FTS_CMD_READ_ID_LEN;
    ret = fts_read(id_cmd, id_cmd_len, chip_id, 2);
    if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
        FTS_ERROR("read boot id fail,read:0x%02x%02x", chip_id[0], chip_id[1]);
        return -EIO;
    }

    id[0] = chip_id[0];
    id[1] = chip_id[1];
    return 0;
}

/*****************************************************************************
* Name: fts_get_ic_information
* Brief: read chip id to get ic information, after run the function, driver w-
*        ill know which IC is it.
*        If cant get the ic information, maybe not focaltech's touch IC, need
*        unregister the driver
* Input:
* Output:
* Return: return 0 if get correct ic information, otherwise return error code
*****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int cnt = 0;
    u8 chip_id[2] = { 0 };
    u32 type = ts_data->pdata->type;

    ts_data->ic_info.is_incell = FTS_CHIP_IDC(type);
    ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED(type);

    do {
        ret = fts_read_reg(FTS_REG_CHIP_ID, &chip_id[0]);
        ret = fts_read_reg(FTS_REG_CHIP_ID2, &chip_id[1]);
        if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
            FTS_DEBUG("i2c read invalid, read:0x%02x%02x",
                chip_id[0], chip_id[1]);
        } else {
            ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], VALID);
            if (!ret)
                break;
            else
                FTS_DEBUG("TP not ready, read:0x%02x%02x",
                        chip_id[0], chip_id[1]);
        }

        cnt++;
        msleep(INTERVAL_READ_REG);
    } while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

    if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
        FTS_INFO("fw is invalid, need read boot id");
        if (ts_data->ic_info.hid_supported) {
            fts_hid2std();
        }

        ret = fts_read_bootid(ts_data, &chip_id[0]);
        if (ret <  0) {
            FTS_ERROR("read boot id fail");
            return ret;
        }

        ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], INVALID);
        if (ret < 0) {
            FTS_ERROR("can't get ic informaton");
            return ret;
        }
    }

    FTS_INFO("get ic information, chip id = 0x%02x%02x",
        ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl);

    return 0;
}

/*****************************************************************************
*  Reprot related
*****************************************************************************/
static void fts_show_touch_buffer(u8 *data, int datalen)
{
    int i = 0;
    int count = 0;
    char *tmpbuf = NULL;

    tmpbuf = kzalloc(1024, GFP_KERNEL);
    if (!tmpbuf) {
        FTS_ERROR("tmpbuf zalloc fail");
        return;
    }

    for (i = 0; i < datalen; i++) {
        count += snprintf(tmpbuf + count, 1024 - count, "%02X,", data[i]);
        if (count >= 1024)
            break;
    }
    FTS_DEBUG("point buffer:%s", tmpbuf);

    if (tmpbuf) {
        kfree(tmpbuf);
        tmpbuf = NULL;
    }
}

void fts_release_all_finger(void)
{
    struct input_dev *input_dev = fts_data->input_dev;
#if FTS_MT_PROTOCOL_B_EN
    u32 finger_count = 0;
    u32 max_touches = fts_data->pdata->max_touch_number;
#endif

    FTS_FUNC_ENTER();
    mutex_lock(&fts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
    for (finger_count = 0; finger_count < max_touches; finger_count++) {
        input_mt_slot(input_dev, finger_count);
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
    }
#else
    input_mt_sync(input_dev);
#endif
    input_report_key(input_dev, BTN_TOUCH, 0);
    input_sync(input_dev);

    fts_data->touchs = 0;
    fts_data->key_state = 0;
    mutex_unlock(&fts_data->report_mutex);
    FTS_FUNC_EXIT();
}

/*****************************************************************************
* Name: fts_input_report_key
* Brief: process key events,need report key-event if key enable.
*        if point's coordinate is in (x_dim-50,y_dim-50) ~ (x_dim+50,y_dim+50),
*        need report it to key event.
*        x_dim: parse from dts, means key x_coordinate, dimension:+-50
*        y_dim: parse from dts, means key y_coordinate, dimension:+-50
* Input:
* Output:
* Return: return 0 if it's key event, otherwise return error code
*****************************************************************************/
static int fts_input_report_key(struct fts_ts_data *data, int index)
{
    int i = 0;
    int x = data->events[index].x;
    int y = data->events[index].y;
    int *x_dim = &data->pdata->key_x_coords[0];
    int *y_dim = &data->pdata->key_y_coords[0];

    if (!data->pdata->have_key) {
        return -EINVAL;
    }

    for (i = 0; i < data->pdata->key_number; i++) {
        if ((x >= x_dim[i] - FTS_KEY_DIM) && (x <= x_dim[i] + FTS_KEY_DIM) &&
            (y >= y_dim[i] - FTS_KEY_DIM) && (y <= y_dim[i] + FTS_KEY_DIM)) {
            if (EVENT_DOWN(data->events[index].flag)
                && !(data->key_state & (1 << i))) {
                input_report_key(data->input_dev, data->pdata->keys[i], 1);
                data->key_state |= (1 << i);
                FTS_DEBUG("Key%d(%d,%d) DOWN!", i, x, y);
            } else if (EVENT_UP(data->events[index].flag)
                && (data->key_state & (1 << i))) {
                input_report_key(data->input_dev, data->pdata->keys[i], 0);
                data->key_state &= ~(1 << i);
                FTS_DEBUG("Key%d(%d,%d) Up!", i, x, y);
            }
            return 0;
        }
    }
    return -EINVAL;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *data)
{
    int i = 0;
    int uppoint = 0;
    int touchs = 0;
    bool va_reported = false;
    u32 max_touch_num = data->pdata->max_touch_number;
    struct ts_event *events = data->events;

    for (i = 0; i < data->touch_point; i++) {
        if (fts_input_report_key(data, i) == 0)
            continue;

        va_reported = true;
        input_mt_slot(data->input_dev, events[i].id);

        if (EVENT_DOWN(events[i].flag)) {
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);

#if FTS_REPORT_PRESSURE_EN
            if (events[i].p <= 0) {
                events[i].p = 0x3f;
            }
            input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
            if (events[i].area <= 0) {
                events[i].area = 0x09;
            }
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
            input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);
            /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
            #if FTS_SAMSUNG_SCREEN_SHOT_EN
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MINOR, events[i].minor);
            #endif // FTS_SAMSUNG_SCREEN_SHOT_EN
            /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/

            touchs |= BIT(events[i].id);
            data->touchs |= BIT(events[i].id);

            if ((data->log_level >= 2) ||
                ((1 == data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
                /*M55 code for QN6887A-1242 by yuli at 20231115 start*/
                FTS_INFO("[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
                    events[i].id,
                    events[i].x, events[i].y,
                    events[i].p, events[i].area);
            }
        } else {
            uppoint++;
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
            data->touchs &= ~BIT(events[i].id);
            if (data->log_level >= 1) {
                FTS_INFO("[B]P%d UP!", events[i].id);
            }
        }
    }
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
    #if FTS_SAMSUNG_SCREEN_SHOT_EN
    if (data->palm_flag != 0) {
        input_report_key(data->input_dev, BTN_PALM, data->palm_flag);
        input_sync(data->input_dev);
        input_report_key(data->input_dev, BTN_PALM, 0);
        input_sync(data->input_dev);
    }
    #endif // FTS_SAMSUNG_SCREEN_SHOT_EN
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/

    if (unlikely(data->touchs ^ touchs)) {
        for (i = 0; i < max_touch_num; i++)  {
            if (BIT(i) & (data->touchs ^ touchs)) {
                if (data->log_level >= 1) {
                    FTS_INFO("[B]P%d UP!", i);
                }
            va_reported = true;
            input_mt_slot(data->input_dev, i);
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
            }
        }
    }
    data->touchs = touchs;

    if (va_reported) {
        /* touchs==0, there's no point but key */
        if (EVENT_NO_DOWN(data) || (!touchs)) {
            if (data->log_level >= 1) {
                FTS_INFO("[B]Points All Up!");
            }
            input_report_key(data->input_dev, BTN_TOUCH, 0);
        } else {
            input_report_key(data->input_dev, BTN_TOUCH, 1);
        }
    }

    input_sync(data->input_dev);
    return 0;
}

#else
static int fts_input_report_a(struct fts_ts_data *data)
{
    int i = 0;
    int touchs = 0;
    bool va_reported = false;
    struct ts_event *events = data->events;

    for (i = 0; i < data->touch_point; i++) {
        if (fts_input_report_key(data, i) == 0) {
            continue;
        }

        va_reported = true;
        if (EVENT_DOWN(events[i].flag)) {
            input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, events[i].id);
#if FTS_REPORT_PRESSURE_EN
            if (events[i].p <= 0) {
                events[i].p = 0x3f;
            }
            input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
            if (events[i].area <= 0) {
                events[i].area = 0x09;
            }
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);

            input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

            input_mt_sync(data->input_dev);

            if ((data->log_level >= 2) ||
                ((1 == data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
                FTS_DEBUG("[A]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
                    events[i].id,
                    events[i].x, events[i].y,
                    events[i].p, events[i].area);
            }
            touchs++;
        }
    }

    /* last point down, current no point but key */
    if (data->touchs && !touchs) {
        va_reported = true;
    }
    data->touchs = touchs;

    if (va_reported) {
        if (EVENT_NO_DOWN(data)) {
            if (data->log_level >= 1) {
                FTS_DEBUG("[A]Points All Up!");
            }
            input_report_key(data->input_dev, BTN_TOUCH, 0);
            input_mt_sync(data->input_dev);
        } else {
            input_report_key(data->input_dev, BTN_TOUCH, 1);
        }
    }

    input_sync(data->input_dev);
    return 0;
}
#endif

/*M55 code for QN6887A-371 by yuli at 20231026 start*/
/*M55 code for P231221-04897 by yuli at 20240106 start*/
/*
fod_report_buf[0]:    reserved
fod_report_buf[1]:    fod id  0x25 indicate single tap
fod_report_buf[2]:    reserved
fod_report_buf[3]:    reserved
fod_report_buf[4]:  aod double tap in valid area X high 8bit
fod_report_buf[5]:  aod double tap in valid area X low 8bit
fod_report_buf[6]:  aod double tap in valid area Y high 8bit
fod_report_buf[7]:  aod double tap in valid area Y low 8bit
fod_report_buf[8]:    fod event id long press 0, press 1, release 2, out of fod area 3
*/
/*M55 code for P231221-04897 by yuli at 20240106 end*/
static int fts_read_touchdata(struct fts_ts_data *data)
{
    int ret = 0;
    u8 *buf = data->point_buf;
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
    #if FTS_SAMSUNG_FOD_EN
    u8 *fod_report_buf = data->fod_report_buf;
    u8 cmd[1] = {0};

    /*M55 code for SR-QN6887A-01-369 by yuli at 20231017 start*/
    memset(fod_report_buf, 0xFF, data->fod_report_buf_size);
    /*M55 code for SR-QN6887A-01-369 by yuli at 20231017 end*/
    cmd[0] = 0xE1;
    #endif // FTS_SAMSUNG_FOD_EN
    memset(buf, 0xFF, data->pnt_buf_size);
    buf[0] = 0x01;

    /*M55 code for QN6887A-699 by yuli at 20231117 start*/
    if (data->gesture_mode && data->suspended) {
    /*M55 code for QN6887A-699 by yuli at 20231117 end*/
        #if FTS_SAMSUNG_FOD_EN
        ret = fts_read(cmd, 1, fod_report_buf, data->fod_report_buf_size);
        if (ret < 0) {
            FTS_ERROR("read fod report data failed, ret:%d", ret);
            /*M55 code for QN6887A-576 by yuli at 20231102 start*/
            goto GESTURE_READ;
            /*M55 code for QN6887A-576 by yuli at 20231102 end*/
        }
        /*M55 code for QN6887A-282 by yuli at 20231021 start*/
        if (data->log_level >= 4) {
            fts_show_touch_buffer(fod_report_buf, data->fod_report_buf_size);
        }
        /*M55 code for QN6887A-282 by yuli at 20231021 end*/

        if ((fod_report_buf[8] >= 0 && fod_report_buf[8] <= 3)      //Report fod event
            /*M55 code for P231221-04897 by yuli at 20240106 start*/
            || (fod_report_buf[1] == 0x25)      //Report single_tap
            || (fod_report_buf[1] == 0x27)      //Report  aod double tap in valid area
            /*M55 code for P240111-04531 by yuli at 20240116 start*/
            || (fod_report_buf[1] == 0x22)) {  //Report spay
            FTS_INFO("fod event: 0x%x, single_tap(0x25), aod_double_tap(0x27), spay(0x22) : 0x%x",
                fod_report_buf[8], fod_report_buf[1]);
            /*M55 code for P240111-04531 by yuli at 20240116 end*/
            /*M55 code for P231221-04897 by yuli at 20240106 end*/
            switch (fod_report_buf[8]) {
                case 0:
                    data->ts_data_info.scrub_id = SPONGE_EVENT_TYPE_LONG_PRESS;
                    break;
                case 1:
                    data->ts_data_info.scrub_id = SPONGE_EVENT_TYPE_FOD_PRESS;
                    break;
                case 2:
                    data->ts_data_info.scrub_id = SPONGE_EVENT_TYPE_FOD_RELEASE;
                    break;
                case 3:
                    data->ts_data_info.scrub_id = SPONGE_EVENT_TYPE_FOD_OUT;
                    break;
                default:
                    FTS_INFO("unknown fod event: 0x%x", fod_report_buf[8]);
                    break;
            }

            /*M55 code for P240111-04531 by yuli at 20240116 start*/
            if(fod_report_buf[1] == 0x22) {
                data->ts_data_info.scrub_id = SPONGE_EVENT_TYPE_SPAY;
            }
            /*M55 code for P240111-04531 by yuli at 20240116 end*/

            if(fod_report_buf[1] == 0x25) {
                data->ts_data_info.scrub_id = SPONGE_EVENT_TYPE_SINGLE_TAP;
            }

            /*M55 code for P231221-04897 by yuli at 20240106 start*/
            if(fod_report_buf[1] == 0x27) {
                data->ts_data_info.scrub_id = SPONGE_EVENT_TYPE_AOD_DOUBLETAB;
                data->ts_data_info.scrub_x = (fod_report_buf[4] << 8) + fod_report_buf[5];
                data->ts_data_info.scrub_y = (fod_report_buf[6] << 8) + fod_report_buf[7];
                FTS_INFO("scrub_id: %d, scrub_x:%d, scrub_y:%d", data->ts_data_info.scrub_id,
                    data->ts_data_info.scrub_x, data->ts_data_info.scrub_y);
            }
            /*M55 code for P231221-04897 by yuli at 20240106 end*/

            input_report_key(data->input_dev, KEY_BLACK_UI_GESTURE, 1);
            input_sync(data->input_dev);
            input_report_key(data->input_dev, KEY_BLACK_UI_GESTURE, 0);
            input_sync(data->input_dev);
        }
        #endif // FTS_SAMSUNG_FOD_EN
        /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/

/*M55 code for QN6887A-576 by yuli at 20231102 start*/
GESTURE_READ:
/*M55 code for QN6887A-576 by yuli at 20231102 end*/
        if (0 == fts_gesture_readdata(data, NULL)) {
            FTS_INFO("succuss to get gesture data in irq handler");
            return 1;
        }
    }

    ret = fts_read(buf, 1, buf + 1, data->pnt_buf_size - 1);
    if (ret < 0) {
        FTS_ERROR("read touchdata failed, ret:%d", ret);
        return ret;
    }
    #if FTS_SAMSUNG_FOD_EN
    memcpy(fod_report_buf, buf + 87, data->fod_report_buf_size);

    if (fod_report_buf[8] >= 0 && fod_report_buf[8] <= 3) {      //Report fod event
        FTS_INFO("fod event: 0x%x, single tap event: 0x%x", fod_report_buf[8], fod_report_buf[1]);
        switch (fod_report_buf[8]) {
            case 0:
                data->ts_data_info.scrub_id = SPONGE_EVENT_TYPE_LONG_PRESS;
                break;
            case 1:
                data->ts_data_info.scrub_id = SPONGE_EVENT_TYPE_FOD_PRESS;
                break;
            case 2:
                data->ts_data_info.scrub_id = SPONGE_EVENT_TYPE_FOD_RELEASE;
                break;
            case 3:
                data->ts_data_info.scrub_id = SPONGE_EVENT_TYPE_FOD_OUT;
                break;
            default:
                FTS_INFO("unknown fod event: 0x%x", fod_report_buf[8]);
                break;
        }

        input_report_key(data->input_dev, KEY_BLACK_UI_GESTURE, 1);
        input_sync(data->input_dev);
        input_report_key(data->input_dev, KEY_BLACK_UI_GESTURE, 0);
        input_sync(data->input_dev);
    }
    #endif // FTS_SAMSUNG_FOD_EN

    if (data->log_level >= 3) {
        fts_show_touch_buffer(buf, data->pnt_buf_size);
    }

    return 0;
}

static int fts_read_parse_touchdata(struct fts_ts_data *data)
{
    int ret = 0;
    int i = 0;
    u8 pointid = 0;
    int base = 0;
    struct ts_event *events = data->events;
    int max_touch_num = data->pdata->max_touch_number;
    u8 *buf = data->point_buf;
    int touch_type = 0;
    u8 finger_num = 0;
    u8 event_num = 0;
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
    #if FTS_SAMSUNG_SCREEN_SHOT_EN
    int palm_value;
    #endif // FTS_SAMSUNG_SCREEN_SHOT_EN

    ret = fts_read_touchdata(data);
    if (ret) {
        return ret;
    }

    #if FTS_SAMSUNG_SCREEN_SHOT_EN
    /*M55 code for SR-QN6887A-01-369 by yuli at 20231017 start*/
    palm_value = (buf[3] & 0xF0) >> 4;
    /*M55 code for SR-QN6887A-01-369 by yuli at 20231017 end*/

    if (palm_value != 0) {
        data->palm_flag = 1;
    } else {
        data->palm_flag  = 0;
    }
    #endif // FTS_SAMSUNG_SCREEN_SHOT_EN
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/
    data->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;
    data->touch_point = 0;

    if (data->ic_info.is_incell) {
        if ((data->point_num == 0x0F) && (buf[2] == 0xFF) && (buf[3] == 0xFF)
            && (buf[4] == 0xFF) && (buf[5] == 0xFF) && (buf[6] == 0xFF)) {
            FTS_DEBUG("touch buff is 0xff, need recovery state");
            fts_release_all_finger();
            fts_tp_state_recovery(data);
            return -EIO;
        }
    }

    if (data->point_num > max_touch_num) {
        FTS_INFO("invalid point_num(%d)", data->point_num);
        return -EIO;
    }
    touch_type = ((buf[FTS_TOUCH_POINT_NUM] >> 4) & 0x0F);

    switch (touch_type) {
        case TOUCH_DEFAULT:
            finger_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;
            if (finger_num > max_touch_num) {
                FTS_ERROR("invalid point_num(%d)", finger_num);
                return -EIO;
            }

            for (i = 0; i < max_touch_num; i++) {
                base = FTS_ONE_TCH_LEN * i;
                pointid = (buf[FTS_TOUCH_ID_POS + base]) >> 4;
                if (pointid >= FTS_MAX_ID)
                    break;
                else if (pointid >= max_touch_num) {
                    FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
                    return -EINVAL;
                }

                events[i].id = pointid;
                events[i].flag = buf[FTS_TOUCH_EVENT_POS + base] >> 6;
                events[i].x = ((buf[FTS_TOUCH_X_H_POS + base] & 0x0F) << 8) +
                        (buf[FTS_TOUCH_X_L_POS + base] & 0xFF);
                events[i].y = ((buf[FTS_TOUCH_Y_H_POS + base] & 0x0F) << 8) +
                        (buf[FTS_TOUCH_Y_L_POS + base] & 0xFF);
                events[i].p =  buf[FTS_TOUCH_PRE_POS + base];
                events[i].area = buf[FTS_TOUCH_AREA_POS + base] >> 4;
                if (events[i].p <= 0) events[i].p = 0x3F;
                if (events[i].area <= 0) events[i].area = 0x09;
                events[i].minor = events[i].area;

                event_num++;
                if (EVENT_DOWN(events[i].flag) && (finger_num == 0)) {
                    FTS_INFO("abnormal touch data from fw");
                    return -EIO;
                }
            }

            if (event_num == 0) {
                FTS_INFO("no touch point information(%02x)", buf[2]);
                return -EIO;
            }
            data->touch_point = event_num;

            break;

        case TOUCH_PROTOCOL_v2:
            event_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;
            if (!event_num || (event_num > max_touch_num)) {
                FTS_ERROR("invalid touch event num(%d)", event_num);
                return -EIO;
            }

            data->touch_point = event_num;

            for (i = 0; i < event_num; i++) {
                base = FTS_ONE_TCH_LEN_V2 * i + 2;
                pointid = (buf[FTS_TOUCH_ID_POS + base]) >> 4;
                if (pointid >= max_touch_num) {
                    FTS_ERROR("touch point ID(%d) beyond max_touch_number(%d)",
                              pointid, max_touch_num);
                    return -EINVAL;
                }

                events[i].id = pointid;
                events[i].flag = buf[FTS_TOUCH_EVENT_POS + base] >> 6;

                events[i].x = ((buf[FTS_TOUCH_X_H_POS + base] & 0x0F) << 12) \
                              + ((buf[FTS_TOUCH_X_L_POS + base] & 0xFF) << 4) \
                              + ((buf[FTS_TOUCH_PRE_POS + base] >> 4) & 0x0F);

                events[i].y = ((buf[FTS_TOUCH_Y_H_POS + base] & 0x0F) << 12) \
                              + ((buf[FTS_TOUCH_Y_L_POS + base] & 0xFF) << 4) \
                              + (buf[FTS_TOUCH_PRE_POS + base] & 0x0F);

                // events[i].x = events[i].x * FTS_TOUCH_HIRES_X  / FTS_HI_RES_X_MAX;
                // events[i].y = events[i].y * FTS_TOUCH_HIRES_X / FTS_HI_RES_X_MAX;
                events[i].x = events[i].x / FTS_HI_RES_X_MAX;
                events[i].y = events[i].y  / FTS_HI_RES_X_MAX;
                events[i].area = buf[FTS_TOUCH_AREA_POS + base];
                events[i].minor = buf[FTS_TOUCH_OFF_MINOR + base];
                /*M55 code for QN6887A-150 by yuli at 20231012 start*/
                events[i].p = buf[FTS_TOUCH_OFF_MISC + base];
                /*M55 code for QN6887A-150 by yuli at 20231012 end*/

                if (events[i].area <= 0) events[i].area = 0x09;
                if (events[i].minor <= 0) events[i].minor = 0x09;

            }

            break;

        case TOUCH_IGNORE:
        case TOUCH_ERROR:
            break;

        default:
            FTS_INFO("unknown touch event(%d)", touch_type);
            break;
        }
    /*M55 code for SR-QN6887A-01-391 by hehaoran at 20230922 start*/
    #ifndef CONFIG_ARCH_QTI_VM
    if (!get_tp_enable()) {
        fts_release_all_finger();
        data->touch_point = 0;
        FTS_INFO("get_tp_enable() is false\n");
    }
    #endif//CONFIG_ARCH_QTI_VM
    /*M55 code for SR-QN6887A-01-391 by hehaoran at 20230922 end*/
    return 0;
}

static void fts_irq_read_report(void)
{
    int ret = 0;
    struct fts_ts_data *ts_data = fts_data;

#if FTS_ESDCHECK_EN
    fts_esdcheck_set_intr(1);
#endif

#if FTS_POINT_REPORT_CHECK_EN
    fts_prc_queue_work(ts_data);
#endif

    ret = fts_read_parse_touchdata(ts_data);
    if (ret == 0) {
        mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
        fts_input_report_b(ts_data);
#else
        fts_input_report_a(ts_data);
#endif
        mutex_unlock(&ts_data->report_mutex);
    }

#if FTS_ESDCHECK_EN
    fts_esdcheck_set_intr(0);
#endif
}

static irqreturn_t fts_irq_handler(int irq, void *data)
{
    struct fts_ts_data *fts_data = data;
    /*M55 code for QN6887A-576 by yuli at 20231102 start*/
    #if IS_ENABLED(CONFIG_PM) && FTS_PATCH_COMERR_PM
    int ret = 0;
    if ((fts_data->suspended) && (fts_data->pm_suspend)) {
        ret = wait_for_completion_timeout(
                  &fts_data->pm_completion,
                  msecs_to_jiffies(FTS_TIMEOUT_COMERR_PM));
        if (!ret) {
            FTS_ERROR("Bus don't resume from pm(deep),timeout,skip irq");
            return IRQ_HANDLED;
        }
    }
    #endif // CONFIG_PM && FTS_PATCH_COMERR_PM
    /*M55 code for QN6887A-576 by yuli at 20231102 end*/

    if (!fts_data) {
        FTS_ERROR("%s: Invalid fts_data\n", __func__);
        return IRQ_HANDLED;
    }

    if (!mutex_trylock(&fts_data->transition_lock))
        return IRQ_HANDLED;

#ifdef CONFIG_FTS_I2C_TRUSTED_TOUCH
#ifndef CONFIG_ARCH_QTI_VM
    if (atomic_read(&fts_data->trusted_touch_enabled) == 1) {
        mutex_unlock(&fts_data->transition_lock);
        return IRQ_HANDLED;
    }
#endif
#endif
    fts_irq_read_report();
    mutex_unlock(&fts_data->transition_lock);

    return IRQ_HANDLED;
}

static int fts_irq_registration(struct fts_ts_data *ts_data)
{
    int ret = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;

#ifdef CONFIG_ARCH_QTI_VM
    pdata->irq_gpio_flags = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
    FTS_INFO("irq:%d, flag:%x", ts_data->irq, pdata->irq_gpio_flags);
    ret = request_threaded_irq(ts_data->irq, NULL, fts_irq_handler,
                pdata->irq_gpio_flags,
                FTS_DRIVER_NAME, ts_data);
#else
    ts_data->irq = gpio_to_irq(pdata->irq_gpio);
    pdata->irq_gpio_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
    FTS_INFO("irq:%d, flag:%x", ts_data->irq, pdata->irq_gpio_flags);
    ret = request_threaded_irq(ts_data->irq, NULL, fts_irq_handler,
                pdata->irq_gpio_flags,
                FTS_DRIVER_NAME, ts_data);
#endif
    return ret;
}

static int fts_input_init(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int key_num = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;
    struct input_dev *input_dev;

    FTS_FUNC_ENTER();
    input_dev = input_allocate_device();
    if (!input_dev) {
        FTS_ERROR("Failed to allocate memory for input device");
        return -ENOMEM;
    }

    /* Init and register Input device */
    input_dev->name = FTS_DRIVER_NAME;
    if (ts_data->bus_type == BUS_TYPE_I2C)
        input_dev->id.bustype = BUS_I2C;
    else
        input_dev->id.bustype = BUS_SPI;
    input_dev->dev.parent = ts_data->dev;

    input_set_drvdata(input_dev, ts_data);

    __set_bit(EV_SYN, input_dev->evbit);
    __set_bit(EV_ABS, input_dev->evbit);
    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(BTN_TOUCH, input_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

    if (pdata->have_key) {
        FTS_INFO("set key capabilities");
        for (key_num = 0; key_num < pdata->key_number; key_num++)
            input_set_capability(input_dev, EV_KEY, pdata->keys[key_num]);
    }

#if FTS_MT_PROTOCOL_B_EN
    input_mt_init_slots(input_dev, pdata->max_touch_number, INPUT_MT_DIRECT);
#else
    input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 0x0F, 0, 0);
#endif
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, pdata->x_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, pdata->y_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
#if FTS_REPORT_PRESSURE_EN
    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
    #if FTS_SAMSUNG_SCREEN_SHOT_EN
    set_bit(BTN_PALM, input_dev->keybit);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, 0xFF, 0, 0);
    #endif // FTS_SAMSUNG_SCREEN_SHOT_EN

    #if FTS_SAMSUNG_FOD_EN
    set_bit(KEY_BLACK_UI_GESTURE, input_dev->keybit);
    #endif // FTS_SAMSUNG_FOD_EN
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/

    ret = input_register_device(input_dev);
    if (ret) {
        FTS_ERROR("Input device registration failed");
        input_set_drvdata(input_dev, NULL);
        input_free_device(input_dev);
        input_dev = NULL;
        return ret;
    }

    ts_data->input_dev = input_dev;

    FTS_FUNC_EXIT();
    return 0;
}

static int fts_report_buffer_init(struct fts_ts_data *ts_data)
{
    int point_num = 0;
    int events_num = 0;

    point_num = FTS_MAX_POINTS_SUPPORT;
    ts_data->pnt_buf_size = FTS_TOUCH_DATA_LEN_V2 + 1;
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
    ts_data->point_buf = (u8 *)kzalloc(ts_data->pnt_buf_size, GFP_KERNEL);
    if (!ts_data->point_buf) {
        FTS_ERROR("failed to alloc memory for point buf");
        return -ENOMEM;
    }
    #if FTS_SAMSUNG_FOD_EN
    ts_data->fod_report_buf_size = FTS_FOD_REPORT_DATA_LEN;
    ts_data->fod_report_buf = (u8 *)kzalloc(ts_data->fod_report_buf_size, GFP_KERNEL);
    if (!ts_data->fod_report_buf) {
        FTS_ERROR("failed to alloc memory for fod report buf");
        return -ENOMEM;
    }
    #endif // FTS_SAMSUNG_FOD_EN
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/
    events_num = point_num * sizeof(struct ts_event);
    ts_data->events = (struct ts_event *)kzalloc(events_num, GFP_KERNEL);
    if (!ts_data->events) {
        FTS_ERROR("failed to alloc memory for point events");
        kfree_safe(ts_data->point_buf);
        #if FTS_SAMSUNG_FOD_EN
        kfree_safe(ts_data->fod_report_buf);
        #endif // FTS_SAMSUNG_FOD_EN
        return -ENOMEM;
    }

    return 0;
}

#if FTS_POWER_SOURCE_CUST_EN
/*****************************************************************************
* Power Control
*****************************************************************************/
#if FTS_PINCTRL_EN
static int fts_pinctrl_init(struct fts_ts_data *ts)
{
    int ret = 0;

    ts->pinctrl = devm_pinctrl_get(ts->dev);
    if (IS_ERR_OR_NULL(ts->pinctrl)) {
        FTS_ERROR("Failed to get pinctrl, please check dts");
        ret = PTR_ERR(ts->pinctrl);
        goto err_pinctrl_get;
    }

    ts->pins_active = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_active");
    if (IS_ERR_OR_NULL(ts->pins_active)) {
        FTS_ERROR("Pin state[active] not found");
        ret = PTR_ERR(ts->pins_active);
        goto err_pinctrl_lookup;
    }

    ts->pins_suspend = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_suspend");
    if (IS_ERR_OR_NULL(ts->pins_suspend)) {
        FTS_ERROR("Pin state[suspend] not found");
        ret = PTR_ERR(ts->pins_suspend);
        goto err_pinctrl_lookup;
    }

    ts->pins_release = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_release");
    if (IS_ERR_OR_NULL(ts->pins_release)) {
        FTS_ERROR("Pin state[release] not found");
        ret = PTR_ERR(ts->pins_release);
    }

    return 0;
err_pinctrl_lookup:
    if (ts->pinctrl) {
        devm_pinctrl_put(ts->pinctrl);
    }
err_pinctrl_get:
    ts->pinctrl = NULL;
    ts->pins_release = NULL;
    ts->pins_suspend = NULL;
    ts->pins_active = NULL;
    return ret;
}

static int fts_pinctrl_select_normal(struct fts_ts_data *ts)
{
    int ret = 0;

    if (ts->pinctrl && ts->pins_active) {
        ret = pinctrl_select_state(ts->pinctrl, ts->pins_active);
        if (ret < 0) {
            FTS_ERROR("Set normal pin state error:%d", ret);
        }
    }

    return ret;
}

static int fts_pinctrl_select_suspend(struct fts_ts_data *ts)
{
    int ret = 0;

    if (ts->pinctrl && ts->pins_suspend) {
        ret = pinctrl_select_state(ts->pinctrl, ts->pins_suspend);
        if (ret < 0) {
            FTS_ERROR("Set suspend pin state error:%d", ret);
        }
    }

    return ret;
}

static int fts_pinctrl_select_release(struct fts_ts_data *ts)
{
    int ret = 0;

    if (ts->pinctrl) {
        if (IS_ERR_OR_NULL(ts->pins_release)) {
            devm_pinctrl_put(ts->pinctrl);
            ts->pinctrl = NULL;
        } else {
            ret = pinctrl_select_state(ts->pinctrl, ts->pins_release);
            if (ret < 0)
                FTS_ERROR("Set gesture pin state error:%d", ret);
        }
    }

    return ret;
}
#endif /* FTS_PINCTRL_EN */

static int fts_power_configure(struct fts_ts_data *ts_data, bool enable)
{
    int ret = 0;

    FTS_FUNC_ENTER();

    if (enable) {
        if (regulator_count_voltages(ts_data->vdd) > 0) {
            ret = regulator_set_load(ts_data->vdd, FTS_LOAD_MAX_UA);
            if (ret) {
                FTS_ERROR("vdd regulator set_load failed ret=%d", ret);
                return ret;
            }

            ret = regulator_set_voltage(ts_data->vdd, FTS_VTG_MIN_UV,
                        FTS_VTG_MAX_UV);
            if (ret) {
                FTS_ERROR("vdd regulator set_vtg failed ret=%d", ret);
                goto err_vdd_load;
            }
        }

        if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
            if (regulator_count_voltages(ts_data->vcc_i2c) > 0) {
                ret = regulator_set_load(ts_data->vcc_i2c, FTS_LOAD_AVDD_UA);
                if (ret) {
                    FTS_ERROR("vcc_i2c regulator set_load failed ret=%d", ret);
                    goto err_vdd_load;
                }

                ret = regulator_set_voltage(ts_data->vcc_i2c,
                            FTS_I2C_VTG_MIN_UV,
                            FTS_I2C_VTG_MAX_UV);
                if (ret) {
                    FTS_ERROR("vcc_i2c regulator set_vtg failed,ret=%d", ret);
                    goto err_vcc_load;
                }
            }
        }
    } else {
        if (regulator_count_voltages(ts_data->vdd) > 0) {
            ret = regulator_set_load(ts_data->vdd, FTS_LOAD_DISABLE_UA);
            if (ret) {
                FTS_ERROR("vdd regulator set_load failed ret=%d", ret);
                return ret;
            }
        }

        if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
            if (regulator_count_voltages(ts_data->vcc_i2c) > 0) {
                ret = regulator_set_load(ts_data->vcc_i2c, FTS_LOAD_DISABLE_UA);
                if (ret) {
                    FTS_ERROR("vcc_i2c regulator set_load failed ret=%d", ret);
                    return ret;
                }
            }
        }
    }

    FTS_FUNC_EXIT();
    return ret;

err_vcc_load:
    regulator_set_load(ts_data->vcc_i2c, FTS_LOAD_DISABLE_UA);
err_vdd_load:
    regulator_set_load(ts_data->vdd, FTS_LOAD_DISABLE_UA);
    return ret;
}

static int fts_ts_enable_reg(struct fts_ts_data *ts_data, bool enable)
{
    int ret = 0;

    if (IS_ERR_OR_NULL(ts_data->vdd)) {
        FTS_ERROR("vdd is invalid");
        return -EINVAL;
    }

    if (enable) {
        fts_power_configure(ts_data, true);
        ret = regulator_enable(ts_data->vdd);
        if (ret)
            FTS_ERROR("enable vdd regulator failed,ret=%d", ret);

        if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
            ret = regulator_enable(ts_data->vcc_i2c);
            if (ret)
                FTS_ERROR("enable vcc_i2c regulator failed,ret=%d", ret);
        }
    } else {
        ret = regulator_disable(ts_data->vdd);
        if (ret)
            FTS_ERROR("disable vdd regulator failed,ret=%d", ret);
        if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
            ret = regulator_disable(ts_data->vcc_i2c);
            if (ret)
                FTS_ERROR("disable vcc_i2c regulator failed,ret=%d", ret);
        }
        fts_power_configure(ts_data, false);
    }

    return ret;
}

static int fts_power_source_ctrl(struct fts_ts_data *ts_data, int enable)
{
    int ret = 0;

    if (IS_ERR_OR_NULL(ts_data->vdd)) {
        FTS_ERROR("vdd is invalid");
        return -EINVAL;
    }

    FTS_FUNC_ENTER();
    if (enable) {
        if (ts_data->power_disabled) {
            FTS_DEBUG("regulator enable !");
            gpio_direction_output(ts_data->pdata->reset_gpio, 0);
            msleep(1);
            ret = fts_ts_enable_reg(ts_data, true);
            if (ret)
                FTS_ERROR("Touch reg enable failed\n");
            ts_data->power_disabled = false;
        }
    } else {
        if (!ts_data->power_disabled) {
            FTS_DEBUG("regulator disable !");
            /*M55 code for QN6887A-788 by yuli at 20231109 start*/
            fts_write_reg(0xB6, 0x01);
            usleep_range(20000, 21000);
            gpio_direction_output(ts_data->pdata->reset_gpio, 0);
            usleep_range(2000, 3000);
            /*M55 code for QN6887A-788 by yuli at 20231109 end*/
            ret = fts_ts_enable_reg(ts_data, false);
            if (ret)
                FTS_ERROR("Touch reg disable failed");
            ts_data->power_disabled = true;
        }
    }

    FTS_FUNC_EXIT();
    return ret;
}

/*****************************************************************************
* Name: fts_power_source_init
* Brief: Init regulator power:vdd/vcc_io(if have), generally, no vcc_io
*        vdd---->vdd-supply in dts, kernel will auto add "-supply" to parse
*        Must be call after fts_gpio_configure() execute,because this function
*        will operate reset-gpio which request gpio in fts_gpio_configure()
* Input:
* Output:
* Return: return 0 if init power successfully, otherwise return error code
*****************************************************************************/
static int fts_power_source_init(struct fts_ts_data *ts_data)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    ts_data->vdd = regulator_get(ts_data->dev, "vdd");
    if (IS_ERR_OR_NULL(ts_data->vdd)) {
        ret = PTR_ERR(ts_data->vdd);
        FTS_ERROR("get vdd regulator failed,ret=%d", ret);
        return ret;
    }

    ts_data->vcc_i2c = regulator_get(ts_data->dev, "vcc_i2c");
    if (IS_ERR_OR_NULL(ts_data->vcc_i2c))
        FTS_INFO("get vcc_i2c regulator failed");

#if FTS_PINCTRL_EN
    fts_pinctrl_init(ts_data);
    fts_pinctrl_select_normal(ts_data);
#endif

    ts_data->power_disabled = true;
    ret = fts_power_source_ctrl(ts_data, ENABLE);
    if (ret) {
        FTS_ERROR("fail to enable power(regulator)");
    }

    FTS_FUNC_EXIT();
    return ret;
}

static int fts_power_source_exit(struct fts_ts_data *ts_data)
{
#if FTS_PINCTRL_EN
    fts_pinctrl_select_release(ts_data);
#endif

    fts_power_source_ctrl(ts_data, DISABLE);

    if (!IS_ERR_OR_NULL(ts_data->vdd)) {
        if (regulator_count_voltages(ts_data->vdd) > 0)
            regulator_set_voltage(ts_data->vdd, 0, FTS_VTG_MAX_UV);
        regulator_put(ts_data->vdd);
    }

    if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
        if (regulator_count_voltages(ts_data->vcc_i2c) > 0)
            regulator_set_voltage(ts_data->vcc_i2c, 0, FTS_I2C_VTG_MAX_UV);
        regulator_put(ts_data->vcc_i2c);
    }

    return 0;
}

static int fts_power_source_suspend(struct fts_ts_data *ts_data)
{
    int ret = 0;

#if FTS_PINCTRL_EN
    fts_pinctrl_select_suspend(ts_data);
#endif

    /*M55 code for SR-QN6887A-01-369 by yuli at 20231017 start*/
    #if FTS_POWER_DISABLE
    ret = fts_power_source_ctrl(ts_data, DISABLE);
    if (ret < 0) {
        FTS_ERROR("power off fail, ret=%d", ret);
    }
    #else
    FTS_INFO("No power off");
    #endif // FTS_POWER_DISABLE
    /*M55 code for SR-QN6887A-01-369 by yuli at 20231017 end*/

    return ret;
}

static int fts_power_source_resume(struct fts_ts_data *ts_data)
{
    int ret = 0;

#if FTS_PINCTRL_EN
    fts_pinctrl_select_normal(ts_data);
#endif

    ret = fts_power_source_ctrl(ts_data, ENABLE);
    if (ret < 0) {
        FTS_ERROR("power on fail, ret=%d", ret);
    }

    return ret;
}
#endif /* FTS_POWER_SOURCE_CUST_EN */

static int fts_gpio_configure(struct fts_ts_data *data)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    /* request irq gpio */
    if (gpio_is_valid(data->pdata->irq_gpio)) {
        ret = gpio_request(data->pdata->irq_gpio, "fts_irq_gpio");
        if (ret) {
            FTS_ERROR("[GPIO]irq gpio request failed");
            goto err_irq_gpio_req;
        }

        ret = gpio_direction_input(data->pdata->irq_gpio);
        if (ret) {
            FTS_ERROR("[GPIO]set_direction for irq gpio failed");
            goto err_irq_gpio_dir;
        }
    }

    /* request reset gpio */
    if (gpio_is_valid(data->pdata->reset_gpio)) {
        ret = gpio_request(data->pdata->reset_gpio, "fts_reset_gpio");
        if (ret) {
            FTS_ERROR("[GPIO]reset gpio request failed");
            goto err_irq_gpio_dir;
        }

        /*M55 code for SR-QN6887A-01-369 by yuli at 20231017 start*/
        ret = gpio_direction_output(data->pdata->reset_gpio, 0);
        /*M55 code for SR-QN6887A-01-369 by yuli at 20231017 end*/
        if (ret) {
            FTS_ERROR("[GPIO]set_direction for reset gpio failed");
            goto err_reset_gpio_dir;
        }
    }

    FTS_FUNC_EXIT();
    return 0;

err_reset_gpio_dir:
    if (gpio_is_valid(data->pdata->reset_gpio))
        gpio_free(data->pdata->reset_gpio);
err_irq_gpio_dir:
    if (gpio_is_valid(data->pdata->irq_gpio))
        gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
    FTS_FUNC_EXIT();
    return ret;
}

static int fts_get_dt_coords(struct device *dev, char *name,
                struct fts_ts_platform_data *pdata)
{
    int ret = 0;
    u32 coords[FTS_COORDS_ARR_SIZE] = { 0 };
    struct property *prop;
    struct device_node *np = dev->of_node;
    int coords_size;

    prop = of_find_property(np, name, NULL);
    if (!prop)
        return -EINVAL;
    if (!prop->value)
        return -ENODATA;

    coords_size = prop->length / sizeof(u32);
    if (coords_size != FTS_COORDS_ARR_SIZE) {
        FTS_ERROR("invalid:%s, size:%d", name, coords_size);
        return -EINVAL;
    }

    ret = of_property_read_u32_array(np, name, coords, coords_size);
    if (ret < 0) {
        FTS_ERROR("Unable to read %s, please check dts", name);
        pdata->x_min = FTS_X_MIN_DISPLAY_DEFAULT;
        pdata->y_min = FTS_Y_MIN_DISPLAY_DEFAULT;
        pdata->x_max = FTS_X_MAX_DISPLAY_DEFAULT;
        pdata->y_max = FTS_Y_MAX_DISPLAY_DEFAULT;
        return -ENODATA;
    } else {
        pdata->x_min = coords[0];
        pdata->y_min = coords[1];
        pdata->x_max = coords[2];
        pdata->y_max = coords[3];
    }

    FTS_INFO("display x(%d %d) y(%d %d)", pdata->x_min, pdata->x_max,
        pdata->y_min, pdata->y_max);
    return 0;
}

static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
    int ret = 0;
    struct device_node *np = dev->of_node;
    u32 temp_val = 0;
    const char *gpio_name = "focaltech,reset-gpio";

    FTS_FUNC_ENTER();

    ret = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
    if (ret < 0)
        FTS_ERROR("Unable to get display-coords");

    /* key */
    pdata->have_key = of_property_read_bool(np, "focaltech,have-key");
    if (pdata->have_key) {
        ret = of_property_read_u32(np, "focaltech,key-number", &pdata->key_number);
        if (ret < 0)
            FTS_ERROR("Key number undefined!");

        ret = of_property_read_u32_array(np, "focaltech,keys",
                        pdata->keys, pdata->key_number);
        if (ret < 0)
            FTS_ERROR("Keys undefined!");
        else if (pdata->key_number > FTS_MAX_KEYS)
            pdata->key_number = FTS_MAX_KEYS;

        ret = of_property_read_u32_array(np, "focaltech,key-x-coords",
                        pdata->key_x_coords,
                        pdata->key_number);
        if (ret < 0)
            FTS_ERROR("Key Y Coords undefined!");

        ret = of_property_read_u32_array(np, "focaltech,key-y-coords",
                        pdata->key_y_coords,
                        pdata->key_number);
        if (ret < 0)
            FTS_ERROR("Key X Coords undefined!");

        FTS_INFO("VK Number:%d, key:(%d,%d,%d), "
            "coords:(%d,%d),(%d,%d),(%d,%d)",
            pdata->key_number,
            pdata->keys[0], pdata->keys[1], pdata->keys[2],
            pdata->key_x_coords[0], pdata->key_y_coords[0],
            pdata->key_x_coords[1], pdata->key_y_coords[1],
            pdata->key_x_coords[2], pdata->key_y_coords[2]);
    }

    /* reset, irq gpio info */
/*M55 code for SR-QN6887A-01-391 by hehaoran at 20231008 start*/
#ifndef CONFIG_ARCH_QTI_VM
    if (tp_get_cmdline(EVB0) || tp_get_cmdline(EVB1)) {
        gpio_name = "focaltech,reset-gpio-unofficial";
    } else {
        gpio_name = "focaltech,reset-gpio";
    }
#endif// CONFIG_ARCH_QTI_VM
/*M55 code for SR-QN6887A-01-391 by hehaoran at 20231008 end*/
    pdata->reset_gpio = of_get_named_gpio_flags(np, gpio_name,
            0, &pdata->reset_gpio_flags);
    if (pdata->reset_gpio < 0)
        FTS_ERROR("Unable to get reset_gpio");

    pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio",
            0, &pdata->irq_gpio_flags);
    if (pdata->irq_gpio < 0)
        FTS_ERROR("Unable to get irq_gpio");

    ret = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
    if (ret < 0) {
        FTS_ERROR("Unable to get max-touch-number, please check dts");
        pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
    } else {
        if (temp_val < 2)
            pdata->max_touch_number = 2; /* max_touch_number must >= 2 */
        else if (temp_val > FTS_MAX_POINTS_SUPPORT)
            pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
        else
            pdata->max_touch_number = temp_val;
    }

    FTS_INFO("max touch number:%d, irq gpio:%d, reset gpio:%d",
        pdata->max_touch_number, pdata->irq_gpio, pdata->reset_gpio);

    ret = of_property_read_u32(np, "focaltech,ic-type", &temp_val);
    if (ret < 0)
        pdata->type = _FT3518;
    else
        pdata->type = temp_val;

    FTS_FUNC_EXIT();
    return 0;
}

#if defined(CONFIG_DRM)
static void fts_resume_work(struct work_struct *work)
{
    struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
                    resume_work);

    fts_ts_resume(ts_data->dev);
}

static void fts_ts_panel_notifier_callback(enum panel_event_notifier_tag tag,
         struct panel_event_notification *notification, void *client_data)
{
    struct fts_ts_data *ts_data = client_data;

    if (!notification) {
        FTS_ERROR("Invalid notification\n");
        return;
    }
    /*M55 code for QN6887A-1213 by xiongxiaoliang at 20231114 start*/
    FTS_INFO("Notification type:%d, early_trigger:%d",
            notification->notif_type,
            notification->notif_data.early_trigger);
    /*M55 code for QN6887A-1213 by xiongxiaoliang at 20231114 end*/
    /*M55 code for QN6887A-186 by hehaoran at 20231015 start*/
    if (atomic_read(&fts_data->ito_testing) == 1) {
        FTS_ERROR("ito is testing, can not accept panel notifier!");
        return;
    }
    /*M55 code for QN6887A-186 by hehaoran at 20231015 end*/
    switch (notification->notif_type) {
    case DRM_PANEL_EVENT_UNBLANK:
        /*M55 code for QN6887A-400 by hehaoran at 20231106 start*/
        #ifndef CONFIG_ARCH_QTI_VM
        g_charge_bright_screen = true;
        #endif
        /*M55 code for QN6887A-400 by hehaoran at 20231106 end*/
        if (notification->notif_data.early_trigger)
            FTS_INFO("resume notification pre commit\n");
        else
            queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
        break;
    case DRM_PANEL_EVENT_BLANK:
        /*M55 code for QN6887A-400 by hehaoran at 20231106 start*/
        #ifndef CONFIG_ARCH_QTI_VM
        g_charge_bright_screen = false;
        #endif
        /*M55 code for QN6887A-400 by hehaoran at 20231106 end*/
        if (notification->notif_data.early_trigger) {
            cancel_work_sync(&fts_data->resume_work);
            fts_ts_suspend(ts_data->dev);
        } else {
            FTS_INFO("suspend notification post commit\n");
        }
        break;
    case DRM_PANEL_EVENT_BLANK_LP:
        /*M55 code for QN6887A-1213 by xiongxiaoliang at 20231114 start*/
        #ifndef CONFIG_ARCH_QTI_VM
        g_charge_bright_screen = false;
        #endif
        /*M55 code for QN6887A-1213 by xiongxiaoliang at 20231114 end*/
        FTS_INFO("received lp event\n");
        if (notification->notif_data.early_trigger) {
            /*M55 code for P231221-04897 by yuli at 20240106 start*/
            #ifndef CONFIG_ARCH_QTI_VM
            FTS_INFO("aod_rect_data = %d, %d, %d, %d\n",
                ts_data->ts_data_info.aod_rect_data[0], ts_data->ts_data_info.aod_rect_data[1],
                ts_data->ts_data_info.aod_rect_data[2], ts_data->ts_data_info.aod_rect_data[3]);
            /*M55 code for P240111-04531 by yuli at 20240116 start*/
            if (!((fts_data->spay_en | fts_data->double_tap | fts_data->single_tap | fts_data->fod_enable)
            /*M55 code for P240111-04531 by yuli at 20240116 end*/
            || ((ts_data->ts_data_info.aod_rect_data[0]) || (ts_data->ts_data_info.aod_rect_data[1])
            || (ts_data->ts_data_info.aod_rect_data[2]) || (ts_data->ts_data_info.aod_rect_data[3])))) {
                FTS_INFO("gesture_mode = DISABLE\n");
                fts_data->gesture_mode = DISABLE;
            } else {
                FTS_INFO("gesture_mode = ENABLE\n");
                fts_data->gesture_mode = ENABLE;
            }
            #endif
            /*M55 code for P231221-04897 by yuli at 20240106 end*/
            cancel_work_sync(&fts_data->resume_work);
            fts_ts_suspend(ts_data->dev);
        } else {
            FTS_INFO("suspend notification post commit\n");
        }
        break;
    case DRM_PANEL_EVENT_FPS_CHANGE:
        FTS_DEBUG("shashank:Received fps change old fps:%d new fps:%d\n",
                notification->notif_data.old_fps,
                notification->notif_data.new_fps);
        break;
    default:
        FTS_INFO("notification serviced :%d\n",
                notification->notif_type);
        /*M55 code for QN6887A-1242 by yuli at 20231115 end*/
        break;
    }
}

#elif defined(CONFIG_FB)
static void fts_resume_work(struct work_struct *work)
{
    struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
                    resume_work);

    fts_ts_resume(ts_data->dev);
}

static int fb_notifier_callback(struct notifier_block *self,
                unsigned long event, void *data)
{
    struct fb_event *evdata = data;
    int *blank = NULL;
    struct fts_ts_data *ts_data = container_of(self, struct fts_ts_data,
                    fb_notif);

    if (!(event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK)) {
        FTS_INFO("event(%lu) do not need process\n", event);
        return 0;
    }

    blank = evdata->data;
    FTS_INFO("FB event:%lu,blank:%d", event, *blank);
    switch (*blank) {
    case FB_BLANK_UNBLANK:
        if (FB_EARLY_EVENT_BLANK == event) {
            FTS_INFO("resume: event = %lu, not care\n", event);
        } else if (FB_EVENT_BLANK == event) {
            queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
        }
        break;

    case FB_BLANK_POWERDOWN:
        if (FB_EARLY_EVENT_BLANK == event) {
            cancel_work_sync(&fts_data->resume_work);
            fts_ts_suspend(ts_data->dev);
        } else if (FB_EVENT_BLANK == event) {
            FTS_INFO("suspend: event = %lu, not care\n", event);
        }
        break;

    default:
        FTS_INFO("FB BLANK(%d) do not need process\n", *blank);
        break;
    }

    return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void fts_ts_early_suspend(struct early_suspend *handler)
{
    struct fts_ts_data *ts_data = container_of(handler, struct fts_ts_data,
                    early_suspend);

    fts_ts_suspend(ts_data->dev);
}

static void fts_ts_late_resume(struct early_suspend *handler)
{
    struct fts_ts_data *ts_data = container_of(handler, struct fts_ts_data,
                    early_suspend);

    fts_ts_resume(ts_data->dev);
}
#endif

/*M55 code for SR-QN6887A-01-352 by yuli at 20230923 start*/
/*M55 code for SR-QN6887A-01-391 by hehaoran at 20231008 start*/
fts_upgrade_module fts_module_list[] = {
    {
        .panel_name = "icna3512_visionox",
        .fw_num = MODEL_ICNA3512_FT3683G_VS,
    },
    {
        .panel_name = "icna3512_tianma",
        .fw_num = MODEL_ICNA3512_FT3683G_TM,
    },
};

static int fts_get_tp_module(void)
{
    int i = 0;
    const char * active_panel_name = NULL;

    if (active_panel == NULL) {
            goto not_found;
    }

    active_panel_name = active_panel->dev->of_node->name;
    FTS_INFO("active_panel_name = %s!\n",active_panel_name);
    for (i = 0; i < ARRAY_SIZE(fts_module_list); i++) {
        if (strstr(active_panel_name,fts_module_list[i].panel_name)) {
            return fts_module_list[i].fw_num;
        }
    }

not_found:
    FTS_INFO("Not found tp module !!\n");
    return MODEL_DEFAULT;
}
/*M55 code for SR-QN6887A-01-391 by hehaoran at 20231008 end*/
static void fts_update_module_info(void)
{
    int module = fts_get_tp_module();

    FTS_INFO("start");
    switch (module) {
        case MODEL_ICNA3512_FT3683G_VS:
            fts_data->fw_name = "focaltech_ts_fw_visionox.bin";
            fts_data->ito_file = "focaltech_ts_ito_visionox.ini";
            fts_data->module_name = "icna3512_ft3683g_visionox";
            break;
        case MODEL_ICNA3512_FT3683G_TM:
            fts_data->fw_name = "focaltech_ts_fw_tianma.bin";
            fts_data->ito_file = "focaltech_ts_ito_tianma.ini";
            fts_data->module_name = "icna3512_ft3683g_tianma";
            break;
        default:
            fts_data->fw_name = "unknown";
            fts_data->ito_file = "unknown";
            fts_data->module_name = "unknown";
            break;
    }

    FTS_INFO("fw_name = %s, ito_file = %s, module_name = %s",
        fts_data->fw_name, fts_data->ito_file, fts_data->module_name);
    FTS_INFO("end");
}
/*M55 code for SR-QN6887A-01-352 by yuli at 20230923 end*/

static int fts_ts_probe_delayed(struct fts_ts_data *fts_data)
{
    int ret = 0;

/* Avoid setting up hardware for TVM during probe */
#ifdef CONFIG_FTS_I2C_TRUSTED_TOUCH
#ifdef CONFIG_ARCH_QTI_VM
    if (!atomic_read(&fts_data->delayed_vm_probe_pending)) {
        atomic_set(&fts_data->delayed_vm_probe_pending, 1);
        return 0;
    }
    goto tvm_setup;
#endif
#endif
    ret = fts_gpio_configure(fts_data);
    if (ret) {
        FTS_ERROR("configure the gpios fail");
        goto err_gpio_config;
    }

#if FTS_POWER_SOURCE_CUST_EN
    ret = fts_power_source_init(fts_data);
    if (ret) {
        FTS_ERROR("fail to get power(regulator)");
        goto err_power_init;
    }
#endif

    fts_reset_proc(200);

    ret = fts_get_ic_information(fts_data);
    if (ret) {
        FTS_ERROR("not focal IC, unregister driver");
        goto err_irq_req;
    }

    /*M55 code for SR-QN6887A-01-352 by yuli at 20230923 start*/
    #if FTS_TEST_EN
    ret = fts_test_init(fts_data);
    if (ret) {
        FTS_ERROR("init host test fail");
    }
    #endif // FTS_TEST_EN
    /*M55 code for SR-QN6887A-01-352 by yuli at 20230923 end*/

#ifdef CONFIG_ARCH_QTI_VM
tvm_setup:
#endif
    ret = fts_irq_registration(fts_data);
    if (ret) {
        FTS_ERROR("request irq failed");
#ifdef CONFIG_ARCH_QTI_VM
        return ret;
#endif
        goto err_irq_req;
    }

#ifdef CONFIG_ARCH_QTI_VM
    return ret;
#endif

    ret = fts_fwupg_init(fts_data);
    if (ret)
        FTS_ERROR("init fw upgrade fail");

    return 0;

err_irq_req:
    if (gpio_is_valid(fts_data->pdata->reset_gpio))
        gpio_free(fts_data->pdata->reset_gpio);
    if (gpio_is_valid(fts_data->pdata->irq_gpio))
        gpio_free(fts_data->pdata->irq_gpio);
#if FTS_POWER_SOURCE_CUST_EN
err_power_init:
    fts_power_source_exit(fts_data);
#endif
err_gpio_config:
    return ret;
}

/*M55 code for SR-QN6887A-01-356 by tangzhen at 20231007 start*/
#ifndef CONFIG_ARCH_QTI_VM
static void fts_update_charger_state(bool mode)
{
    if (fts_data == NULL) {
        return;
    }

    if (fts_data->charger_mode == true) {
        fts_ex_mode_switch(MODE_CHARGER, ENABLE);
        FTS_INFO("Charger Mode:%s\n", fts_data->charger_mode ? "On" : "Off");
    } else if (fts_data->charger_mode == false) {
        fts_ex_mode_switch(MODE_CHARGER, DISABLE);
        FTS_INFO("Charger Mode:%s\n", fts_data->charger_mode ? "On" : "Off");
    }
}
static void fts_usb_workqueue(struct work_struct *work)
{
    if (NULL == work) {
        FTS_ERROR("%s:  parameter work are null!\n", __func__);
        return;
    }

    if (fts_data->suspended == false) {
        FTS_INFO("enter usb_workqueue\n");
        fts_update_charger_state(fts_data->charger_mode);
    }
}
static int fts_usb_notifier_callback(struct notifier_block *this,unsigned long val, void *v)
{
    /*M55 code for QN6887A-1147 by yuli at 20231113 start*/
    if (atomic_read(&fts_data->trusted_touch_enabled) == 1) {
        FTS_INFO("trusted_touch_enabled = 1, Unable to USB notification");
        return 0;
    }
    /*M55 code for QN6887A-1147 by yuli at 20231113 end*/

    if (fts_data == NULL) {
        return -EFAULT;
    }

    switch (val) {
        case CHARGER_NOTIFY_STOP_CHARGING:
            fts_data->charger_mode = false;
            break;
        case CHARGER_NOTIFY_START_CHARGING:
            fts_data->charger_mode = true;
            break;
        default:
            FTS_ERROR("unknown");
            break;
    }

    schedule_work(&fts_data->tp_usb_work_queue);

    return 0;
}
#endif// CONFIG_ARCH_QTI_VM
/*M55 code for SR-QN6887A-01-356 by tangzhen at 20231007 end*/

static int fts_ts_probe_entry(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int pdata_size = sizeof(struct fts_ts_platform_data);

    FTS_FUNC_ENTER();
    FTS_INFO("%s", FTS_DRIVER_VERSION);
    ts_data->pdata = kzalloc(pdata_size, GFP_KERNEL);
    if (!ts_data->pdata) {
        FTS_ERROR("allocate memory for platform_data fail");
        return -ENOMEM;
    }

    if (ts_data->dev->of_node) {
        ret = fts_parse_dt(ts_data->dev, ts_data->pdata);
        if (ret)
            FTS_ERROR("device-tree parse fail");
    } else {
        if (ts_data->dev->platform_data) {
            memcpy(ts_data->pdata, ts_data->dev->platform_data, pdata_size);
        } else {
            FTS_ERROR("platform_data is null");
            return -ENODEV;
        }
    }

    ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
    if (!ts_data->ts_workqueue) {
        FTS_ERROR("create fts workqueue fail");
    }

    spin_lock_init(&ts_data->irq_lock);
    mutex_init(&ts_data->report_mutex);
    mutex_init(&ts_data->bus_lock);
    mutex_init(&ts_data->transition_lock);

    /* Init communication interface */
    ret = fts_bus_init(ts_data);
    if (ret) {
        FTS_ERROR("bus initialize fail");
        goto err_bus_init;
    }

    ret = fts_input_init(ts_data);
    if (ret) {
        FTS_ERROR("input initialize fail");
        goto err_input_init;
    }

    ret = fts_report_buffer_init(ts_data);
    if (ret) {
        FTS_ERROR("report buffer init fail");
        goto err_report_buffer;
    }

    ret = fts_create_apk_debug_channel(ts_data);
    if (ret) {
        FTS_ERROR("create apk debug node fail");
    }

    ret = fts_create_sysfs(ts_data);
    if (ret) {
        FTS_ERROR("create sysfs node fail");
    }

#if FTS_POINT_REPORT_CHECK_EN
    ret = fts_point_report_check_init(ts_data);
    if (ret) {
        FTS_ERROR("init point report check fail");
    }
#endif

    ret = fts_ex_mode_init(ts_data);
    if (ret) {
        FTS_ERROR("init glove/cover/charger fail");
    }

    ret = fts_gesture_init(ts_data);
    if (ret) {
        FTS_ERROR("init gesture fail");
    }

#if FTS_ESDCHECK_EN
    ret = fts_esdcheck_init(ts_data);
    if (ret) {
        FTS_ERROR("init esd check fail");
    }
#endif
    /*M55 code for QN6887A-186 by hehaoran at 20231015 start*/
    atomic_set(&fts_data->ito_testing, 0);//init
    /*M55 code for QN6887A-186  by hehaoran at 20231015 end*/
    /*M55 code for SR-QN6887A-01-352 by yuli at 20230923 start*/
    fts_update_module_info();
    /*M55 code for SR-QN6887A-01-352 by yuli at 20230923 end*/

#ifdef CONFIG_FTS_I2C_TRUSTED_TOUCH
    fts_ts_trusted_touch_init(ts_data);
    mutex_init(&(ts_data->fts_clk_io_ctrl_mutex));
#endif

#ifndef CONFIG_ARCH_QTI_VM
    if (ts_data->pdata->type == _FT8726) {
        atomic_set(&ts_data->delayed_vm_probe_pending, 1);
        ts_data->suspended = true;
    } else {
        ret = fts_ts_probe_delayed(ts_data);
        if (ret) {
            FTS_ERROR("Failed to enable resources\n");
            goto err_probe_delayed;
        }
    }
#else
    ret = fts_ts_probe_delayed(ts_data);
    if (ret) {
        FTS_ERROR("Failed to enable resources\n");
        goto err_probe_delayed;
    }
#endif

/*M55 code for QN6887A-349 by tangzhen at 20231026 start*/
#ifndef CONFIG_ARCH_QTI_VM
    INIT_WORK(&ts_data->tp_usb_work_queue, fts_usb_workqueue);
    ts_data->tp_usb_notify.notifier_call = fts_usb_notifier_callback;
    ret = register_usb_check_notifier(&ts_data->tp_usb_notify);
    if (ret < 0) {
        FTS_ERROR("register_usb_check_notifier:error:%d", ret);
        cancel_work_sync(&ts_data->tp_usb_work_queue);
    } else {
        FTS_ERROR("register_usb_check_notifier:sucess:%d", ret);
    }
#endif// CONFIG_ARCH_QTI_VM
/*M55 code for QN6887A-349 by tangzhen at 20231026 end*/

#if defined(CONFIG_DRM)
    if (ts_data->ts_workqueue)
        INIT_WORK(&ts_data->resume_work, fts_resume_work);

    if(!fts_data->touch_environment) {
        ret = of_property_read_string(ts_data->dev->of_node, "focaltech,touch-environment",
                        &fts_data->touch_environment);
        if(ret) {
            FTS_ERROR("No  focaltech,touch-environment");
            fts_data->touch_environment = "pvm";
        }
    }

    if (!strcmp(fts_data->touch_environment, "pvm"))
        fts_ts_register_for_panel_events(ts_data->dev->of_node, ts_data);
#elif defined(CONFIG_FB)
    if (ts_data->ts_workqueue) {
        INIT_WORK(&ts_data->resume_work, fts_resume_work);
    }
    ts_data->fb_notif.notifier_call = fb_notifier_callback;
    ret = fb_register_client(&ts_data->fb_notif);
    if (ret) {
        FTS_ERROR("[FB]Unable to register fb_notifier: %d", ret);
    }
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    ts_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + FTS_SUSPEND_LEVEL;
    ts_data->early_suspend.suspend = fts_ts_early_suspend;
    ts_data->early_suspend.resume = fts_ts_late_resume;
    register_early_suspend(&ts_data->early_suspend);
#endif

    /*M55 code for QN6887A-576 by yuli at 20231102 start*/
    #if IS_ENABLED(CONFIG_PM) && FTS_PATCH_COMERR_PM
    init_completion(&ts_data->pm_completion);
    ts_data->pm_suspend = false;
    #endif // CONFIG_PM && FTS_PATCH_COMERR_PM
    /*M55 code for QN6887A-576 by yuli at 20231102 end*/

    FTS_FUNC_EXIT();
    return 0;

err_probe_delayed:
    kfree_safe(ts_data->point_buf);
    #if FTS_SAMSUNG_FOD_EN
    kfree_safe(ts_data->fod_report_buf);
    #endif // FTS_SAMSUNG_FOD_EN
    kfree_safe(ts_data->events);
err_report_buffer:
    input_unregister_device(ts_data->input_dev);
err_input_init:
    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);
err_bus_init:
    kfree_safe(ts_data->bus_tx_buf);
    kfree_safe(ts_data->bus_rx_buf);
    kfree_safe(ts_data->pdata);

    FTS_FUNC_EXIT();
    return ret;
}

static int fts_ts_remove_entry(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();

#if FTS_POINT_REPORT_CHECK_EN
    fts_point_report_check_exit(ts_data);
#endif

    fts_release_apk_debug_channel(ts_data);
    fts_remove_sysfs(ts_data);
    fts_ex_mode_exit(ts_data);

    fts_fwupg_exit(ts_data);

#if FTS_TEST_EN
    fts_test_exit(ts_data);
#endif

#if FTS_ESDCHECK_EN
    fts_esdcheck_exit(ts_data);
#endif

    fts_gesture_exit(ts_data);
    fts_bus_exit(ts_data);

    free_irq(ts_data->irq, ts_data);
    input_unregister_device(ts_data->input_dev);

    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);

#if defined(CONFIG_DRM)
    if (active_panel && ts_data->notifier_cookie)
        panel_event_notifier_unregister(ts_data->notifier_cookie);

#elif defined(CONFIG_FB)
    if (fb_unregister_client(&ts_data->fb_notif))
        FTS_ERROR("Error occurred while unregistering fb_notifier.");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    unregister_early_suspend(&ts_data->early_suspend);
#endif

    /*M55 code for SR-QN6887A-01-356 by tangzhen at 20231007 start*/
#ifndef CONFIG_ARCH_QTI_VM
    cancel_work_sync(&ts_data->tp_usb_work_queue);
    unregister_usb_check_notifier(&ts_data->tp_usb_notify);
#endif// CONFIG_ARCH_QTI_VM
    /*M55 code for SR-QN6887A-01-356 by tangzhen at 20231007 end*/

    if (gpio_is_valid(ts_data->pdata->reset_gpio))
        gpio_free(ts_data->pdata->reset_gpio);

    if (gpio_is_valid(ts_data->pdata->irq_gpio))
        gpio_free(ts_data->pdata->irq_gpio);

#if FTS_POWER_SOURCE_CUST_EN
    fts_power_source_exit(ts_data);
#endif

    kfree_safe(ts_data->point_buf);
    #if FTS_SAMSUNG_FOD_EN
    kfree_safe(ts_data->fod_report_buf);
    #endif // FTS_SAMSUNG_FOD_EN
    kfree_safe(ts_data->events);

    kfree_safe(ts_data->pdata);
    kfree_safe(ts_data);

    FTS_FUNC_EXIT();

    return 0;
}
/*M55 code for QN6887A-371 by yuli at 20231026 end*/

static int fts_ts_suspend(struct device *dev)
{
    int ret = 0;
    struct fts_ts_data *ts_data = fts_data;
    /*M55 code for P231221-04897 by yuli at 20240106 start*/
    u8 id = 0;
    int i = 0;

    FTS_FUNC_ENTER();

/*M55 code for P240111-04942 by hehaoran at 20240118 start*/
#ifdef CONFIG_FTS_I2C_TRUSTED_TOUCH
    if (atomic_read(&fts_data->trusted_touch_transition)
            || atomic_read(&fts_data->trusted_touch_enabled)) {
        wait_for_completion_interruptible(
            &fts_data->trusted_touch_powerdown);
        usleep_range(1000, 1001);
    }
#endif
/*M55 code for P240111-04942 by hehaoran at 20240118 end*/

    /*M55 code for QN6887A-4750 by yuli at 20240112 start*/
    if (ts_data->suspended && ts_data->gesture_mode && !(ts_data->gesture_suspend)) {
        FTS_INFO("sleep mode to gesture(suspend)");
        mutex_lock(&ts_data->transition_lock);
        fts_reset_proc(150);
        /*M55 code for QN6887A-4750 by yuli at 20240112 end*/
        for (i = 0; i < 20; i++) {
            fts_read_reg(0xA3, &id);
            if (id == 0x56) {
                break;
            }
            msleep(10);
        }
        if (i >= 20) {
            FTS_ERROR("wait tp fw valid timeout");
        }
        ts_data->suspended = false;
        if (ts_data->irq_disabled) {
            fts_irq_enable();
        }
        mutex_unlock(&ts_data->transition_lock);
    }
    /*M55 code for P231221-04897 by yuli at 20240106 end*/

    if (ts_data->suspended) {
        FTS_INFO("Already in suspend state");
        return 0;
    }

    if (ts_data->fw_loading) {
        FTS_INFO("fw upgrade in process, can't suspend");
        return 0;
    }

    mutex_lock(&ts_data->transition_lock);

#if FTS_ESDCHECK_EN
    fts_esdcheck_suspend();
#endif

    if (ts_data->gesture_mode) {
        fts_gesture_suspend(ts_data);
    } else {
        fts_irq_disable();

        FTS_INFO("make TP enter into sleep mode");
        ret = fts_write_reg(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP);
        if (ret < 0)
            FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);

        if (!ts_data->ic_info.is_incell) {
#if FTS_POWER_SOURCE_CUST_EN
            ret = fts_power_source_suspend(ts_data);
            if (ret < 0) {
                FTS_ERROR("power enter suspend fail");
            }
#endif
        } else {
#if FTS_PINCTRL_EN
            fts_pinctrl_select_suspend(ts_data);
#endif
            gpio_direction_output(ts_data->pdata->reset_gpio, 0);
        }
        /*M55 code for QN6887A-4750 by yuli at 20240112 start*/
        ts_data->gesture_suspend = false;
        /*M55 code for QN6887A-4750 by yuli at 20240112 end*/
    }

    fts_release_all_finger();
    ts_data->suspended = true;
    mutex_unlock(&ts_data->transition_lock);
    FTS_FUNC_EXIT();
    return 0;
}

static int fts_ts_resume(struct device *dev)
{
    struct fts_ts_data *ts_data = fts_data;
    int ret = 0;
    /*M55 code for QN6887A-699 by yuli at 20231117 start*/
    u8 id = 0;
    int i = 0;
    /*M55 code for QN6887A-699 by yuli at 20231117 end*/

    FTS_FUNC_ENTER();
    if (!ts_data->suspended) {
        FTS_DEBUG("Already in awake state");
        return 0;
    }

    /*M55 code for QN6887A-3421 by yuli at 20231220 start*/
    #ifndef CONFIG_ARCH_QTI_VM
    if (!get_tp_enable() && ts_data->fod_enable) {
        FTS_INFO("fod: on, tp_enable is false, skip resume");
        return 0;
    }
    #endif //CONFIG_ARCH_QTI_VM
    /*M55 code for QN6887A-3421 by yuli at 20231220 end*/
#ifdef CONFIG_FTS_I2C_TRUSTED_TOUCH

    if (atomic_read(&ts_data->trusted_touch_transition))
        wait_for_completion_interruptible(
            &ts_data->trusted_touch_powerdown);
#endif

    if (ts_data->pdata->type == _FT3683G &&
            atomic_read(&ts_data->delayed_vm_probe_pending)) {
        ret = fts_ts_probe_delayed(ts_data);
        if (ret) {
            FTS_ERROR("Failed to enable resources\n");
            return ret;
        }
        ts_data->suspended = false;
        atomic_set(&ts_data->delayed_vm_probe_pending, 0);
        return ret;
    }

    mutex_lock(&ts_data->transition_lock);

    fts_release_all_finger();

    if (!ts_data->ic_info.is_incell) {
#if FTS_POWER_SOURCE_CUST_EN
        fts_power_source_resume(ts_data);
#endif
    } else {
#if FTS_PINCTRL_EN
        fts_pinctrl_select_normal(ts_data);
#endif
    }

    /*M55 code for QN6887A-699 by yuli at 20231117 start*/
    fts_reset_proc(50);

    for (i = 0; i < 20; i++) {
        fts_read_reg(0xA3, &id);
        if (id == 0x56) {
            break;
        }
        msleep(10);
    }
    if (i >= 20) {
        FTS_ERROR("wait tp fw valid timeout");
    }
    /*M55 code for QN6887A-699 by yuli at 20231117 end*/
    fts_ex_mode_recovery(ts_data);

#if FTS_ESDCHECK_EN
    fts_esdcheck_resume();
#endif

    if (ts_data->gesture_mode) {
        fts_gesture_resume(ts_data);
    }

    /*M55 code for SR-QN6887A-01-443|369 by yuli at 20231016 start*/
    FTS_INFO("FW version = 0x%02x", fts_data->firmware_ver);
    /*M55 code for SR-QN6887A-01-443|369 by yuli at 20231016 end*/
    fts_irq_enable();
    ts_data->suspended = false;
    mutex_unlock(&ts_data->transition_lock);
    FTS_FUNC_EXIT();
    return 0;
}

/*****************************************************************************
* TP Driver
*****************************************************************************/
/*M55 code for SR-QN6887A-01-391 by hehaoran at 20231008 start*/
static int fts_ts_check_dt(struct device_node *np)
{
    int i = 0;
    int count = 0;
    struct device_node *node = NULL;
    struct drm_panel *panel = NULL;
    FTS_INFO("+++++");
    count = of_count_phandle_with_args(np, "panel", NULL);
    if (count <= 0) {
        return 0;
    }

    for (i = 0; i < count; i++) {
        node = of_parse_phandle(np, "panel", i);
        panel = of_drm_find_panel(node);
        of_node_put(node);
        if (!IS_ERR(panel)) {
            active_panel = panel;
            return 0;
        }
    }
    FTS_INFO("-----");
    return PTR_ERR(panel);
}

static int fts_ts_check_default_tp(struct device_node *dt, const char *prop)
{
    const char **active_tp = NULL;
    int count, tmp, score = 0;
    const char *active = NULL;
    int ret, i = 0;

    FTS_INFO("+++++");
    count = of_property_count_strings(dt->parent, prop);
    if (count <= 0 || count > 3) {
        return -ENODEV;
    }

    active_tp = kcalloc(count, sizeof(char *),  GFP_KERNEL);
    if (!active_tp) {
        FTS_ERROR("FTS alloc failed\n");
        return -ENOMEM;
    }

    ret = of_property_read_string_array(dt->parent, prop,  active_tp, count);
    if (ret < 0) {
        FTS_ERROR("fail to read %s %d\n", prop, ret);
        ret = -ENODEV;
        goto out;
    }

    for (i = 0; i < count; i++) {
        active = active_tp[i];
        if (active != NULL) {
            tmp = of_device_is_compatible(dt, active);
            if (tmp > 0) {
                score++;
            }
        }
    }

    if (score <= 0) {
        FTS_INFO("not match this driver\n");
        ret = -ENODEV;
        goto out;
    }
    ret = 0;
out:
    FTS_INFO("-----");
    kfree(active_tp);
    return ret;
}
/*M55 code for SR-QN6887A-01-391 by hehaoran at 20231008 end*/
/*M55 code for SR-QN6887A-01-391 by hehaoran at 20231008 start*/
#ifndef CONFIG_ARCH_QTI_VM
/*M55 code for SR-QN6887A-01-391 by hehaoran at 20231008 end*/
bool tp_get_cmdline(const char *name)
{
    const char *bootparams = NULL;
    bool result = false;
    struct device_node *np = of_find_node_by_path("/chosen");
    if (!np) {
        FTS_ERROR("%s: failed to find node", __func__);
        return false;
    }

    if (of_property_read_string(np, "bootargs", &bootparams)) {
        FTS_ERROR("%s: failed to get bootargs property", __func__);
        of_node_put(np); // Free the node reference
        return false;
    }

    result = strnstr(bootparams, name, strlen(bootparams)) ? true : false;
    if (result) {
        FTS_INFO("Successfully matched '%s' in cmdline", name);
    } else {
        FTS_INFO("No '%s' in cmdline\n", name);
    }

    of_node_put(np); // Free the node reference
    return result;
}
/*M55 code for SR-QN6887A-01-391 by hehaoran at 20230922 start*/

/*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
void fts_gesture_wakeup_enable(void)
{
    FTS_INFO("enable double tap");
    mutex_lock(&fts_data->input_dev->mutex);
    fts_data->double_tap = ENABLE;
    fts_data->gesture_mode = ENABLE;
    mutex_unlock(&fts_data->input_dev->mutex);
}

/*M55 code for SR-QN6887A-01-431 by yuli at 20231012 start*/
void fts_gesture_wakeup_disable(void)
{
    u8 val = 0;

    FTS_INFO("disable double tap");
    mutex_lock(&fts_data->input_dev->mutex);
    fts_data->double_tap = DISABLE;
    /*M55 code for P240111-04531 by yuli at 20240116 start*/
    val = (fts_data->spay_en << 2) | (fts_data->double_tap << 4) | (fts_data->single_tap << 5);
    /*M55 code for P240111-04531 by yuli at 20240116 end*/
    /*M55 code for QN6887A-282 by yuli at 20231021 start*/
    #if FTS_SAMSUNG_FOD_EN
    val = (val | fts_data->fod_enable);
    #endif // FTS_SAMSUNG_FOD_EN
    if (!val) {
        /*M55 code for QN6887A-4750 by yuli at 20240112 start*/
        if (!(fts_data->ts_data_info.aod_rect_data[0] || fts_data->ts_data_info.aod_rect_data[1]
            || fts_data->ts_data_info.aod_rect_data[2] || fts_data->ts_data_info.aod_rect_data[3])) {
            fts_data->gesture_mode = DISABLE;
        }
        /*M55 code for QN6887A-4750 by yuli at 20240112 end*/
    }
    mutex_unlock(&fts_data->input_dev->mutex);
    FTS_INFO("gesture_mode = %d", (fts_data->gesture_mode ? ENABLE : DISABLE));
    /*M55 code for QN6887A-282 by yuli at 20231021 end*/
}

void tp_singletap_enable(void)
{
    FTS_INFO("enable single tap");
    mutex_lock(&fts_data->input_dev->mutex);
    fts_data->single_tap = ENABLE;
    fts_data->gesture_mode = ENABLE;
    mutex_unlock(&fts_data->input_dev->mutex);
}

void tp_singletap_disable(void)
{
    u8 val = 0;

    FTS_INFO("disable single tap");
    mutex_lock(&fts_data->input_dev->mutex);
    fts_data->single_tap = DISABLE;
    /*M55 code for P240111-04531 by yuli at 20240116 start*/
    val = (fts_data->spay_en << 2) | (fts_data->double_tap << 4) | (fts_data->single_tap << 5);
    /*M55 code for P240111-04531 by yuli at 20240116 end*/
    /*M55 code for QN6887A-282 by yuli at 20231021 start*/
    #if FTS_SAMSUNG_FOD_EN
    val = (val | fts_data->fod_enable);
    #endif // FTS_SAMSUNG_FOD_EN
    if (!val) {
        /*M55 code for QN6887A-4750 by yuli at 20240112 start*/
        if (!(fts_data->ts_data_info.aod_rect_data[0] || fts_data->ts_data_info.aod_rect_data[1]
            || fts_data->ts_data_info.aod_rect_data[2] || fts_data->ts_data_info.aod_rect_data[3])) {
            fts_data->gesture_mode = DISABLE;
        }
        /*M55 code for QN6887A-4750 by yuli at 20240112 end*/
    }
    mutex_unlock(&fts_data->input_dev->mutex);
    FTS_INFO("gesture_mode = %d", (fts_data->gesture_mode ? ENABLE : DISABLE));
    /*M55 code for QN6887A-282 by yuli at 20231021 end*/
}
/*M55 code for SR-QN6887A-01-431 by yuli at 20231012 end*/

/*M55 code for QN6887A-282 by yuli at 20231021 start*/
#if FTS_SAMSUNG_FOD_EN
void tp_fod_enabled(u8 value)
{
    int val = 0;

    FTS_INFO("value = 0x%x, fod %s", value, (value ? "enable" : "disable"));
    mutex_lock(&fts_data->input_dev->mutex);
    fts_data->fod_enable = value;
    fts_fod_enable(value);
    /*M55 code for P240111-04531 by yuli at 20240116 start*/
    val = fts_data->spay_en | (fts_data->double_tap | fts_data->single_tap | fts_data->fod_enable);
    /*M55 code for P240111-04531 by yuli at 20240116 end*/
    if (value) {
        fts_data->gesture_mode = ENABLE;
    } else if (!val) {
        /*M55 code for QN6887A-4750 by yuli at 20240112 start*/
        if (!(fts_data->ts_data_info.aod_rect_data[0] || fts_data->ts_data_info.aod_rect_data[1]
            || fts_data->ts_data_info.aod_rect_data[2] || fts_data->ts_data_info.aod_rect_data[3])) {
            fts_data->gesture_mode = DISABLE;
        }
        /*M55 code for QN6887A-4750 by yuli at 20240112 end*/
    }
    mutex_unlock(&fts_data->input_dev->mutex);
    FTS_INFO("gesture_mode = %d", (fts_data->gesture_mode ? ENABLE : DISABLE));
}

/*
TX total: 16
RX total: 37
 */
void  tp_get_fod_info(void)
{
    u8 data[3] = {16, 14, 28};

    FTS_INFO("++++");
    fts_data->ts_data_info.fod_info_tx = data[0];
    fts_data->ts_data_info.fod_info_rx = data[1];
    fts_data->ts_data_info.fod_info_bytes = data[2];
    FTS_INFO("TX: %d, RX: %d, size: %d", fts_data->ts_data_info.fod_info_tx,
        fts_data->ts_data_info.fod_info_rx, fts_data->ts_data_info.fod_info_bytes);
    FTS_INFO("------");
}

void  tp_get_fod_pos(void)
{
    u8 data[28] = {0};

    FTS_INFO("++++");
    fts_read_fod_pos(data);
    memcpy(fts_data->ts_data_info.fod_pos, data, sizeof(data));
    FTS_INFO("------");
}

int  tp_set_fod_rect(u16 fod_rect_data[4])
{
    u8 data[8] = {0};
    int i = 0, ret = 0;

    FTS_INFO("++++");
    FTS_INFO("l:%d, t:%d, r:%d, b:%d",fod_rect_data[0],fod_rect_data[1], fod_rect_data[2], fod_rect_data[3]);
    if ((fod_rect_data[0] < fts_data->pdata->x_min) || (fod_rect_data[0] > fts_data->pdata->x_max)
        || (fod_rect_data[1] < fts_data->pdata->y_min) || (fod_rect_data[1] > fts_data->pdata->y_max)
        || (fod_rect_data[2] < fts_data->pdata->x_min) || (fod_rect_data[2] > fts_data->pdata->x_max)
        || (fod_rect_data[3] < fts_data->pdata->y_min) || (fod_rect_data[3] > fts_data->pdata->y_max)) {
        FTS_INFO("There is illegal input!");
        return -EINVAL;
    }

    for (i = 0; i < 4; i++) {
        data[i * 2] = (fod_rect_data[i] >> 8) & 0xFF; //high 8bit
        data[i * 2 + 1] = fod_rect_data[i] & 0xFF; //low 8bit
        FTS_INFO("data[%d][%d] = 0x%02x%02x", i * 2, i * 2 + 1, data[i * 2], data[i * 2 +1]);
    }

    ret = fts_set_fod_rect(data);
    if (ret) {
        return ret;
    }
    FTS_INFO("------");
    return 0;
}
#endif // FTS_SAMSUNG_FOD_EN
/*M55 code for QN6887A-282 by yuli at 20231021 end*/

/*M55 code for P231221-04897 by yuli at 20240106 start*/
int  tp_set_aod_rect(u16 aod_rect_data[4])
{
    u8 data[8] = {0};
    int i = 0, ret = 0;
    /*M55 code for P240111-04531 by yuli at 20240116 start*/
    int val = fts_data->spay_en | fts_data->double_tap | fts_data->single_tap | fts_data->fod_enable;
    /*M55 code for P240111-04531 by yuli at 20240116 end*/

    FTS_INFO("++++");
    FTS_INFO("w:%d, h:%d, x:%d, y:%d",aod_rect_data[0],aod_rect_data[1], aod_rect_data[2], aod_rect_data[3]);
    if ((aod_rect_data[0] < 0) || (aod_rect_data[0] > fts_data->pdata->x_max)
        || (aod_rect_data[1] < 0) || (aod_rect_data[1] > fts_data->pdata->y_max)
        || (aod_rect_data[2] < 0) || (aod_rect_data[2] + aod_rect_data[0] > fts_data->pdata->x_max)
        || (aod_rect_data[3] < 0) || (aod_rect_data[3] + aod_rect_data[1] > fts_data->pdata->y_max)) {
        FTS_INFO("There is illegal input!");
        return -EINVAL;
    }

    if (!(aod_rect_data[0] || aod_rect_data[1] || aod_rect_data[2] || aod_rect_data[3])) {
        FTS_INFO("Close aod double tap!, val = %d\n", val);
        if (!val) {
            fts_data->gesture_mode = DISABLE;
        }
    } else {
        fts_data->gesture_mode = ENABLE;
    }
    FTS_INFO("gesture_mode = %d", (fts_data->gesture_mode ? ENABLE : DISABLE));

    for (i = 0; i < 4; i++) {
        data[i * 2] = (aod_rect_data[i] >> 8) & 0xFF; //high 8bit
        data[i * 2 + 1] = aod_rect_data[i] & 0xFF; //low 8bit
        FTS_INFO("data[%d][%d] = 0x%02x%02x", i * 2, i * 2 + 1, data[i * 2], data[i * 2 +1]);
    }

    ret = fts_set_aod_rect(data);
    FTS_INFO("------");
    return 0;
}
/*M55 code for P231221-04897 by yuli at 20240106 end*/

/*M55 code for QN6887A-3421 by yuli at 20231220 start*/
void fts_try_resume_work(void)
{
    queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
}
/*M55 code for QN6887A-3421 by yuli at 20231220 end*/

/*M55 code for P240111-04531 by yuli at 20240116 start*/
void tp_spay_enable(void)
{
    FTS_INFO("enable spay_en");
    mutex_lock(&fts_data->input_dev->mutex);
    fts_data->spay_en = ENABLE;
    fts_data->gesture_mode = ENABLE;
    mutex_unlock(&fts_data->input_dev->mutex);
}

void tp_spay_disable(void)
{
    u8 val = 0;

    FTS_INFO("disable spay_en");
    mutex_lock(&fts_data->input_dev->mutex);
    fts_data->spay_en = DISABLE;
    val = (fts_data->spay_en << 2) | (fts_data->double_tap << 4) | (fts_data->single_tap << 5);
    #if FTS_SAMSUNG_FOD_EN
    val = (val | fts_data->fod_enable);
    #endif // FTS_SAMSUNG_FOD_EN
    if (!val) {
        if (!(fts_data->ts_data_info.aod_rect_data[0] || fts_data->ts_data_info.aod_rect_data[1]
            || fts_data->ts_data_info.aod_rect_data[2] || fts_data->ts_data_info.aod_rect_data[3])) {
            fts_data->gesture_mode = DISABLE;
        }
    }
    mutex_unlock(&fts_data->input_dev->mutex);
    FTS_INFO("gesture_mode = %d", (fts_data->gesture_mode ? ENABLE : DISABLE));
}
/*M55 code for P240111-04531 by yuli at 20240116 end*/

static void tp_feature_interface(void)
{
    int ret = 0;
    struct lcm_info *gs_fts_lcm_info = NULL;

    FTS_INFO("++++\n");
    gs_fts_lcm_info = kzalloc(sizeof(*gs_fts_lcm_info), GFP_KERNEL);
    if (gs_fts_lcm_info == NULL) {
        ret = -EPERM;
        kfree(gs_fts_lcm_info);
    }

    /*M55 code for SR-QN6887A-01-352 by yuli at 20230923 start*/
    gs_fts_lcm_info->module_name = fts_data->module_name;
    gs_fts_lcm_info->fw_version = &fts_data->firmware_ver;
    /*M55 code for SR-QN6887A-01-352 by yuli at 20230923 end*/

    gs_fts_lcm_info->sec = fts_data->sec;
    gs_fts_lcm_info->gesture_wakeup_enable = fts_gesture_wakeup_enable;
    gs_fts_lcm_info->gesture_wakeup_disable = fts_gesture_wakeup_disable;
    gs_fts_lcm_info->single_tap_enable = tp_singletap_enable;
    gs_fts_lcm_info->single_tap_disable = tp_singletap_disable;
    gs_fts_lcm_info->ito_test = fts_ito_test;
    /*M55 code for QN6887A-282 by yuli at 20231021 start*/
    #if FTS_SAMSUNG_FOD_EN
    gs_fts_lcm_info->fod_enable = tp_fod_enabled;
    gs_fts_lcm_info->ts_data_info = &(fts_data->ts_data_info);
    gs_fts_lcm_info->get_fod_info = tp_get_fod_info;
    gs_fts_lcm_info->get_fod_pos = tp_get_fod_pos;
    gs_fts_lcm_info->set_fod_rect = tp_set_fod_rect;
    #endif // FTS_SAMSUNG_FOD_EN
    /*M55 code for QN6887A-282 by yuli at 20231021 end*/
    /*M55 code for P231221-04897 by yuli at 20240106 start*/
    gs_fts_lcm_info->set_aod_rect = tp_set_aod_rect;
    /*M55 code for P231221-04897 by yuli at 20240106 end*/
    /*M55 code for QN6887A-3421 by yuli at 20231220 start*/
    gs_fts_lcm_info->tp_resume_work = fts_try_resume_work;
    /*M55 code for QN6887A-3421 by yuli at 20231220 end*/
    /*M55 code for P240111-04531 by yuli at 20240116 start*/
    gs_fts_lcm_info->spay_enable = tp_spay_enable;
    gs_fts_lcm_info->spay_disable = tp_spay_disable;
    /*M55 code for P240111-04531 by yuli at 20240116 end*/
    tp_common_init(gs_fts_lcm_info);
    FTS_INFO("module_name=%s", gs_fts_lcm_info->module_name);
    FTS_INFO("------\n");
}
/*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/
#endif//CONFIG_ARCH_QTI_VM
/*M55 code for SR-QN6887A-01-391 by hehaoran at 20230922 end*/
static int fts_ts_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    struct fts_ts_data *ts_data = NULL;
    struct device_node *dp = client->dev.of_node;

    FTS_INFO("Touch Screen(I2C BUS) driver prboe...");
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        FTS_ERROR("I2C not supported");
        return -ENODEV;
    }

    ret = fts_ts_check_dt(dp);
    if (ret == -EPROBE_DEFER)
        return ret;

    if (ret) {
        if (!fts_ts_check_default_tp(dp, "qcom,i2c-touch-active"))
            ret = -EPROBE_DEFER;
        else
            ret = -ENODEV;

        return ret;
    }

    /* malloc memory for global struct variable */
    ts_data = (struct fts_ts_data *)kzalloc(sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data) {
        FTS_ERROR("allocate memory for fts_data fail");
        return -ENOMEM;
    }

    fts_data = ts_data;
    ts_data->client = client;
    ts_data->dev = &client->dev;
    ts_data->log_level = 1;
    ts_data->fw_is_running = 0;
    ts_data->bus_type = BUS_TYPE_I2C;
    i2c_set_clientdata(client, ts_data);

    ret = fts_ts_probe_entry(ts_data);
    if (ret) {
        FTS_ERROR("Touch Screen(I2C BUS) driver probe fail");
        kfree_safe(ts_data);
        return ret;
    }
    /*M55 code for SR-QN6887A-01-391 by hehaoran at 20230922 start*/
    #ifndef CONFIG_ARCH_QTI_VM
    tp_feature_interface();
    #endif//CONFIG_ARCH_QTI_VM
    /*M55 code for SR-QN6887A-01-391 by hehaoran at 20230922 end*/
    FTS_INFO("Touch Screen(I2C BUS) driver prboe successfully");
    return 0;
}

static int fts_ts_i2c_remove(struct i2c_client *client)
{
    return fts_ts_remove_entry(i2c_get_clientdata(client));
}

/*M55 code for QN6887A-788 by yuli at 20231109 start*/
static void fts_ts_i2c_shutdown(struct i2c_client *client)
{
    int ret = 0;
    struct fts_ts_data *ts_data = i2c_get_clientdata(client);
    /*M55 code for QN6887A-1147 by yuli at 20231113 start*/
    if (atomic_read(&ts_data->trusted_touch_enabled) == 1) {
        FTS_INFO("trusted_touch_enabled = 1, PVM cannot use I2C, GPIO and power control");
        return;
    }
    /*M55 code for QN6887A-1147 by yuli at 20231113 end*/
    FTS_INFO("enter");
    ret = fts_power_source_ctrl(ts_data, DISABLE);
    if (ret < 0) {
        FTS_ERROR("power off fail, ret=%d", ret);
    }
    FTS_INFO("exit");
}
/*M55 code for QN6887A-788 by yuli at 20231109 end*/

static const struct i2c_device_id fts_ts_i2c_id[] = {
    {FTS_DRIVER_NAME, 0},
    {},
};
static const struct of_device_id fts_dt_match[] = {
    {.compatible = "focaltech,fts_ts", },
    {},
};

/*M55 code for QN6887A-576 by yuli at 20231102 start*/
#if IS_ENABLED(CONFIG_PM) && FTS_PATCH_COMERR_PM
static int fts_pm_suspend(struct device *dev)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    FTS_INFO("system enters into pm_suspend");
    ts_data->pm_suspend = true;
    reinit_completion(&ts_data->pm_completion);
    return 0;
}

static int fts_pm_resume(struct device *dev)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    FTS_INFO("system resumes from pm_suspend");
    ts_data->pm_suspend = false;
    complete(&ts_data->pm_completion);
    return 0;
}

static const struct dev_pm_ops fts_dev_pm_ops = {
    .suspend = fts_pm_suspend,
    .resume = fts_pm_resume,
};
#endif // CONFIG_PM && FTS_PATCH_COMERR_PM

MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct i2c_driver fts_ts_i2c_driver = {
    .probe = fts_ts_i2c_probe,
    .remove = fts_ts_i2c_remove,
    /*M55 code for QN6887A-788 by yuli at 20231109 start*/
    .shutdown = fts_ts_i2c_shutdown,
    /*M55 code for QN6887A-788 by yuli at 20231109 end*/
    .driver = {
        .name = FTS_DRIVER_NAME,
        .owner = THIS_MODULE,
        #if IS_ENABLED(CONFIG_PM) && FTS_PATCH_COMERR_PM
        .pm = &fts_dev_pm_ops,
        #endif // CONFIG_PM && FTS_PATCH_COMERR_PM
        .of_match_table = of_match_ptr(fts_dt_match),
    },
    .id_table = fts_ts_i2c_id,
};
/*M55 code for QN6887A-576 by yuli at 20231102 end*/

static int __init fts_ts_i2c_init(void)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    ret = i2c_add_driver(&fts_ts_i2c_driver);
    if (ret != 0)
        FTS_ERROR("Focaltech touch screen driver init failed!");

    FTS_FUNC_EXIT();
    return ret;
}

static void __exit fts_ts_i2c_exit(void)
{
    i2c_del_driver(&fts_ts_i2c_driver);
}

static int fts_ts_spi_probe(struct spi_device *spi)
{
    int ret = 0;
    struct fts_ts_data *ts_data = NULL;
    struct device_node *dp = spi->dev.of_node;

    FTS_INFO("Touch Screen(SPI BUS) driver prboe...");

    ret = fts_ts_check_dt(dp);
    if (ret == -EPROBE_DEFER)
        return ret;

    if (ret) {
        if (!fts_ts_check_default_tp(dp, "qcom,spi-touch-active"))
            ret = -EPROBE_DEFER;
        else
            ret = -ENODEV;

        return ret;
    }

    spi->mode = SPI_MODE_0;
    spi->bits_per_word = 8;
    ret = spi_setup(spi);
    if (ret) {
        FTS_ERROR("spi setup fail");
        return ret;
    }

    /* malloc memory for global struct variable */
    ts_data = kzalloc(sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data) {
        FTS_ERROR("allocate memory for fts_data fail");
        return -ENOMEM;
    }

    fts_data = ts_data;
    ts_data->spi = spi;
    ts_data->dev = &spi->dev;
    ts_data->log_level = 1;

    ts_data->bus_type = BUS_TYPE_SPI_V2;
    spi_set_drvdata(spi, ts_data);

    ret = fts_ts_probe_entry(ts_data);
    if (ret) {
        FTS_ERROR("Touch Screen(SPI BUS) driver probe fail");
        kfree_safe(ts_data);
        return ret;
    }

    FTS_INFO("Touch Screen(SPI BUS) driver prboe successfully");
    return 0;
}

static int fts_ts_spi_remove(struct spi_device *spi)
{
    return fts_ts_remove_entry(spi_get_drvdata(spi));
}

static const struct spi_device_id fts_ts_spi_id[] = {
    {FTS_DRIVER_NAME, 0},
    {},
};

static struct spi_driver fts_ts_spi_driver = {
    .probe = fts_ts_spi_probe,
    .remove = fts_ts_spi_remove,
    .driver = {
        .name = FTS_DRIVER_NAME,
        .owner = THIS_MODULE,
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
        .pm = &fts_dev_pm_ops,
#endif
        .of_match_table = of_match_ptr(fts_dt_match),
    },
    .id_table = fts_ts_spi_id,
};

static int __init fts_ts_spi_init(void)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    ret = spi_register_driver(&fts_ts_spi_driver);
    if (ret != 0)
        FTS_ERROR("Focaltech touch screen driver init failed!");

    FTS_FUNC_EXIT();
    return ret;
}

static void __exit fts_ts_spi_exit(void)
{
    spi_unregister_driver(&fts_ts_spi_driver);
}

static int __init fts_ts_init(void)
{
    int ret = 0;

    /* if (!tp_get_cmdline("touch.load=1")) {
        FTS_INFO("No need to load touch!");
        return ret;
    } */

    /*M55 code for SR-QN6887A-01-391 by hehaoran at 20231008 start*/
    /*M55 code for  SR-QN6887A-01-387 by yuli at 20230923 start*/
    #ifndef CONFIG_ARCH_QTI_VM
    if (!tp_get_cmdline("icna3512")) {
        FTS_INFO("panel match failed, no need to load TP!");
        return ret;
    }
    #endif//CONFIG_ARCH_QTI_VM
    /*M55 code for  SR-QN6887A-01-387 by yuli at 20230923 end*/

    ret = fts_ts_i2c_init();
    if (ret) {
        FTS_ERROR("Focaltech I2C driver init failed!");
    } else {
        return ret;
    }
    /*M55 code for SR-QN6887A-01-391 by hehaoran at 20231008 end*/
    ret = fts_ts_spi_init();
    if (ret)
        FTS_ERROR("Focaltech SPI driver init failed!");

    return ret;
}

static void __exit fts_ts_exit(void)
{
    fts_ts_i2c_exit();
    fts_ts_spi_exit();
}

late_initcall(fts_ts_init);
module_exit(fts_ts_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
/*M55 code for SR-QN6887A-01-352 by yuli at 20230923 start*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_IMPORT_NS(ANDROID_GKI_VFS_EXPORT_ONLY);
#endif
/*M55 code for SR-QN6887A-01-352 by yuli at 20230923 end*/
