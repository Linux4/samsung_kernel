// SPDX-License-Identifier: GPL-2.0
/*
 * gxy_glink.c
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/soc/qcom/pmic_glink.h>
#include <linux/power/gxy_psy_sysfs.h>
/* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 start */
#include "gpio_afc.h"
/* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 end */
/*M55 code for P240122-04615 by xiongxiaoliang at 20240123 start*/
#include "gxy_battery_ttf.h"
/*M55 code for P240122-04615 by xiongxiaoliang at 20240123 end*/


#define GXY_GLINK_MSG_NAME              "gxy_glink"
#define GXY_GLINK_MSG_OWNER_ID          32777
#define GXY_GLINK_MSG_TYPE_REQ_RESP     1
#define GXY_GLINK_MSG_TYPE_NOTIFY       2

#define GXY_GLINK_BATTERY_OPCODE_GET    0x10011
#define GXY_GLINK_BATTERY_OPCODE_SET    0x10012

#define GXY_GLINK_USB_OPCODE_GET        0x10013
#define GXY_GLINK_USB_OPCODE_SET        0x10014

#define GXY_GLINK_MSG_OPCODE_NOTIFY     0x10010

#define GXY_GLINK_MSG_WAIT_MS           1000
/*M55 code for QN6887A-1117 by shixuanxuan at 20231113 start*/
static bool g_gxy_afc_init_done = false;
/*M55 code for QN6887A-1117 by shixuanxuan at 20231113 end*/
/*M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 start*/
static bool g_gxy_otg_online;
/*M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 end*/
/*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 start*/
static bool g_gxy_analog_earphone_online = 0;
bool get_analog_online(void)
{
    return g_gxy_analog_earphone_online;
}
EXPORT_SYMBOL(get_analog_online);
/*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 end*/
/*M55 code for QN6887A-66 by shixuanxuan at 20230917 start*/
static bool g_gxy_chg_pump_state = 0;
bool get_chg_pump_run_state(void)
{
    return g_gxy_chg_pump_state;
}
EXPORT_SYMBOL(get_chg_pump_run_state);
/*M55 code for QN6887A-66 by shixuanxuan at 20230917 end*/
/* tx message data for get operation */
struct gxy_glink_req_msg {
    struct pmic_glink_hdr hdr;
    u32 prop_id;
    u32 value;
};

struct gxy_glink_resp_msg {
    struct pmic_glink_hdr hdr;
    u32 prop_id;
    u32 value;
    u32 ret_code;
};

/* rx message data for notify operation */
struct gxy_glink_notify_rx_msg {
    struct pmic_glink_hdr hdr;
    u32 notify_type;
    u32 notify_value;
};

struct gxy_psy_state {
    u32 *prop;
    u32 opcode_get;
    u32 opcode_set;
    u32 prop_count;
};

enum gxy_psy_type {
    GXY_PSY_TYPE_BATTERY,
    GXY_PSY_TYPE_USB,
    GXY_PSY_TYPE_MAX,
};

struct gxy_glink_dev {
    struct device *dev;
    struct pmic_glink_client *client;
    struct mutex send_lock;
    struct completion msg_ack;
    struct gxy_psy_state psy_list[GXY_PSY_TYPE_MAX];
    /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230913 start */
    bool afc_first_detect;
    struct delayed_work afc_notify_attach_delayed_work;
    struct work_struct afc_notify_detach_work;
    /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230913 end */
};

static struct gxy_glink_dev *g_gxy_glink_dev = NULL;
struct gxy_usb g_gxy_usb;
/*M55 code for P240122-04615 by xiongxiaoliang at 20240123 start*/
EXPORT_SYMBOL(g_gxy_usb);
/*M55 code for P240122-04615 by xiongxiaoliang at 20240123 end*/

static int gxy_glink_send_message(struct gxy_glink_dev *gxy_dev, void *data, int len)
{
    int ret = 0;

    mutex_lock(&gxy_dev->send_lock);
    reinit_completion(&gxy_dev->msg_ack);

    ret = pmic_glink_write(gxy_dev->client, data, len);
    if (ret) {
        mutex_unlock(&gxy_dev->send_lock);
        GXY_PSY_ERR("send message error\n");
        return ret;
    }

    /* wait for to receive data when message send success */
    ret = wait_for_completion_timeout(&gxy_dev->msg_ack,
        msecs_to_jiffies(GXY_GLINK_MSG_WAIT_MS));
    if (!ret) {
        mutex_unlock(&gxy_dev->send_lock);
        GXY_PSY_ERR("send message timeout\n");
        return -ETIMEDOUT;
    }

    mutex_unlock(&gxy_dev->send_lock);
    return 0;
}

static int gxy_glink_get_property(struct gxy_glink_dev *gxy_dev,
                                  struct gxy_psy_state *pst, u32 prop_id)
{
    struct gxy_glink_req_msg msg = { { 0 } };

    /* prepare header */
    msg.hdr.owner = GXY_GLINK_MSG_OWNER_ID;
    msg.hdr.type = GXY_GLINK_MSG_TYPE_REQ_RESP;
    msg.hdr.opcode = pst->opcode_get;
    /* prepare data */
    msg.prop_id = prop_id;

    return gxy_glink_send_message(gxy_dev, &msg, sizeof(msg));
}

static int gxy_glink_set_property(struct gxy_glink_dev *gxy_dev,
                                  struct gxy_psy_state *pst,
                                  u32 prop_id, u32 value)
{
    struct gxy_glink_req_msg msg = { { 0 } };

    /* prepare header */
    msg.hdr.owner = GXY_GLINK_MSG_OWNER_ID;
    msg.hdr.type = GXY_GLINK_MSG_TYPE_REQ_RESP;
    msg.hdr.opcode = pst->opcode_set;
    /* prepare data */
    msg.prop_id = prop_id;
    msg.value = value;

    return gxy_glink_send_message(gxy_dev, &msg, sizeof(msg));
}

/*-------------------- External extension function start--------------*/
int gxy_battery_get_prop(u32 prop_id)
{
    struct gxy_glink_dev *gxy_dev = g_gxy_glink_dev;
    struct gxy_psy_state *pst = &gxy_dev->psy_list[GXY_PSY_TYPE_BATTERY];
    int ret = 0;

    if (gxy_dev != NULL) {
        ret = gxy_glink_get_property(gxy_dev, pst, prop_id);
        if (ret < 0) {
            return ret;
        }
        return pst->prop[prop_id];
    }
    return -ENODATA;
}
EXPORT_SYMBOL(gxy_battery_get_prop);

int gxy_battery_set_prop(u32 prop_id, u32 value)
{
    struct gxy_glink_dev *gxy_dev = g_gxy_glink_dev;
    struct gxy_psy_state *pst = &gxy_dev->psy_list[GXY_PSY_TYPE_BATTERY];
    int ret = 0;

    if (gxy_dev != NULL) {
        ret = gxy_glink_set_property(gxy_dev, pst, prop_id, value);
        if (ret < 0) {
            return ret;
        }
        return ret;
    }
    return -ENODATA;
}
EXPORT_SYMBOL(gxy_battery_set_prop);

/*M55 code for P231125-00024 by xiongxiaoliang at 20231211 start*/
int gxy_usb_get_prop(u32 prop_id)
{
    switch (prop_id) {
        case TYPEC_CC_ORIENT:
            return g_gxy_usb.cc_orient;
        case PD_MAX_POWER:
            return g_gxy_usb.pd_maxpower_status;
        default:
            GXY_PSY_ERR("Unknown porp id: %u\n", prop_id);
            break;
    }

    return -ENODATA;
}
EXPORT_SYMBOL(gxy_usb_get_prop);
/*M55 code for P231125-00024 by xiongxiaoliang at 20231211 end*/

/*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 start*/
int gxy_otg_get_prop(u32 prop_id)
{
    return g_gxy_otg_online;
}
EXPORT_SYMBOL(gxy_otg_get_prop);
/*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 end*/

int gxy_usb_set_prop(u32 prop_id)
{
    struct gxy_glink_dev *gxy_dev = g_gxy_glink_dev;
    //struct gxy_psy_state *pst = &gxy_dev->psy_list[GXY_PSY_TYPE_USB];

    if (gxy_dev != NULL) {
        //oem
    }
    return -ENODATA;
}
EXPORT_SYMBOL(gxy_usb_set_prop);
/*-------------------- External extension function end--------------*/
static bool gxy_validate_message(struct gxy_glink_dev *gxy_dev,
    struct gxy_glink_resp_msg *resp_msg, size_t len)
{
    if (len != sizeof(*resp_msg)) {
        GXY_PSY_ERR("Incorrect response length %zu for opcode %#x\n", len,
            resp_msg->hdr.opcode);
        return false;
    }

    if (resp_msg->ret_code) {
        GXY_PSY_ERR("Error in response for opcode %#x prop_id %u, rc=%d\n",
            resp_msg->hdr.opcode, resp_msg->prop_id, (int)resp_msg->ret_code);
        return false;
    }

    return true;
}

static void gxy_glink_handle_message(struct gxy_glink_dev *gxy_dev,
                                     void *data, size_t len)
{
    struct gxy_glink_resp_msg *msg = data;
    struct gxy_psy_state *pst = NULL;
    bool ack_ok = false;

    if (!gxy_validate_message(gxy_dev, msg, len)) {
        return;
    }

    switch (msg->hdr.opcode) {
        case GXY_GLINK_BATTERY_OPCODE_GET:
            pst = &gxy_dev->psy_list[GXY_PSY_TYPE_BATTERY];
            pst->prop[msg->prop_id] = msg->value;
            ack_ok = true;
            break;
        case GXY_GLINK_BATTERY_OPCODE_SET:
            ack_ok = true;
            break;
        case GXY_GLINK_USB_OPCODE_GET:
            pst = &gxy_dev->psy_list[GXY_PSY_TYPE_USB];
            pst->prop[msg->prop_id] = msg->value;
            ack_ok = true;
            break;
        case GXY_GLINK_USB_OPCODE_SET:
            ack_ok = true;
            break;
        default:
            GXY_PSY_ERR("Unknown opcode: %u\n", msg->hdr.opcode);
            break;
    }

    if (ack_ok) {
        complete(&gxy_dev->msg_ack);
    }
}

/* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230913 start */
#define AFC_FIRST_ATTACH_DELAY_TIME 6000  //used for afc first detect
#define AFC_ATTACH_DELAY_TIME 1500

static void afc_notifier_attach_delayed_work(struct work_struct *work)
{
    int afc_first_check = 0;

    if (g_gxy_glink_dev->afc_first_detect) {
        afc_first_check = afc_hal_check_first_attach();
        if (afc_first_check == AFC_HW_READY) {
            afc_notifier_call(EVT_ATTACH);
        } else {
            pr_info("afc first attach but not DCP, skip afc detection\n");
        }
        g_gxy_glink_dev->afc_first_detect = false;
    } else {
        afc_notifier_call(EVT_ATTACH);
    }
}

static void afc_notifier_detach_work(struct work_struct *work)
{
    afc_notifier_call(EVT_DETACH);
    if (g_gxy_glink_dev->afc_first_detect) {
        g_gxy_glink_dev->afc_first_detect = false;
    }
}
/* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230913 end */

static void gxy_glink_handle_notify(struct gxy_glink_dev *gxy_dev,
                                    void *data, size_t len)
{
    struct gxy_glink_notify_rx_msg *notify_msg = data;
    u32 notification = 0;
    /*M55 code for P240110-02462 by xiongxiaoliang at 20240111 start*/
    static bool pd_event_flag = false;
    /*M55 code for P240110-02462 by xiongxiaoliang at 20240111 end*/

    if (len != sizeof(*notify_msg)) {
        GXY_PSY_ERR("invalid msg len: %u!=%u\n", len, sizeof(*notify_msg));
        return;
    }

    notification = notify_msg->notify_type;
    switch (notification) {
        case GXY_TYPEC_CC_EVENT:
            g_gxy_usb.cc_orient = notify_msg->notify_value;
            /*M55 code for P240110-02462|P240122-04615 by xiongxiaoliang at 20240223 start*/
            if (g_gxy_usb.cc_orient != 0) {
                if (pd_event_flag != true) {
                    g_gxy_usb.pd_maxpower_status = 0;
                }
                gxy_ttf_work_start();
            } else {
                pd_event_flag = false;
                gxy_ttf_work_cancel();
            }
            /*M55 code for P240110-02462|P240122-04615 by xiongxiaoliang at 20240223 end*/
            break;
        /*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 start*/
        case GXY_OTG_EVENT:
            g_gxy_otg_online = notify_msg->notify_value;
            GXY_PSY_INFO("GXY_OTG_EVENT: otg oline = %d\n", g_gxy_otg_online);
            break;
        /*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 end*/
        /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230913 start */
        case GXY_AFC_ATTACH_EVENT:
            /*M55 code for QN6887A-1117 by shixuanxuan at 20231113 start*/
            if (!g_gxy_afc_init_done) {
                GXY_PSY_INFO("notify msg: AFC attach/detach event, but afc not init done. return\n");
                break;
            }
            /*M55 code for QN6887A-1117 by shixuanxuan at 20231113 end*/
            if (!(notify_msg->notify_value)) {
                GXY_PSY_INFO("notify msg: AFC attach event\n");
                if (g_gxy_glink_dev->afc_first_detect) {
                    g_gxy_glink_dev->afc_first_detect = false;
                } else {
                    schedule_delayed_work(&gxy_dev->afc_notify_attach_delayed_work, msecs_to_jiffies(AFC_ATTACH_DELAY_TIME));
                }
                cancel_work_sync(&gxy_dev->afc_notify_detach_work);
            } else {
                GXY_PSY_INFO("notify msg: AFC detach event\n");
                schedule_work(&gxy_dev->afc_notify_detach_work);
                cancel_delayed_work(&gxy_dev->afc_notify_attach_delayed_work);
            }
            break;
        /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230913 end */
        /*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 start*/
        case GXY_ANALOG_EARPHONE_ATTACH_EVENT:
            if (!(notify_msg->notify_value)) {
                g_gxy_analog_earphone_online = true;
                GXY_PSY_INFO("Analog earphone attached \n");
            } else {
                g_gxy_analog_earphone_online = false;
                GXY_PSY_INFO("Analog earphone detached \n");
            }
            break;
        /*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 end*/
        /*M55 code for QN6887A-66 by shixuanxuan at 20230917 start*/
        case GXY_CP_RUN_EVENT:
            if (notify_msg->notify_value) {
                g_gxy_chg_pump_state = true;
                GXY_PSY_INFO("Charge pump runing \n");
            } else {
                g_gxy_chg_pump_state = false;
                GXY_PSY_INFO("Charge pump exit\n");
            }
            break;
        /*M55 code for QN6887A-66 by shixuanxuan at 20230917 end*/
        /*M55 code for P231125-00024 by xiongxiaoliang at 20231211 start*/
        case GXY_PD_MAX_POWER_EVENT:
            /*M55 code for P240110-02462 by xiongxiaoliang at 20240111 start*/
            pd_event_flag = true;
            /*M55 code for P240110-02462 by xiongxiaoliang at 20240111 end*/
            g_gxy_usb.pd_maxpower_status = notify_msg->notify_value;
            break;
        /*M55 code for P231125-00024 by xiongxiaoliang at 20231211 end*/
        default:
            break;
    }

    GXY_PSY_INFO("notify msg: notify_type=%u notify_value=%u\n", notify_msg->notify_type, notify_msg->notify_value);
}

static int gxy_glink_msg_callback(void *dev_data, void *data, size_t len)
{
    struct pmic_glink_hdr *hdr = data;
    struct gxy_glink_dev *gxy_dev = dev_data;

    if (!gxy_dev || !hdr)
        return -ENODEV;

    GXY_PSY_ERR("msg_callback: owner=%u type=%u opcode=%u len=%zu\n",
        hdr->owner, hdr->type, hdr->opcode, len);

    if (hdr->owner != GXY_GLINK_MSG_OWNER_ID) {
        GXY_PSY_ERR("invalid msg owner: %u\n", hdr->owner);
        return -EINVAL;
    }

    if (hdr->opcode == GXY_GLINK_MSG_OPCODE_NOTIFY) {
        gxy_glink_handle_notify(gxy_dev, data, len);
    } else {
        gxy_glink_handle_message(gxy_dev, data, len);
    }

    return 0;
}

// callback function to notify pmic glink state in the event of a subsystem restart
// or a protection domain restart.
static void gxy_glink_state_callback(void *dev_data, enum pmic_glink_state state)
{
    struct gxy_glink_dev *gxy_dev = dev_data;

    if (!gxy_dev) {
        return;
    }

    GXY_PSY_INFO("state_callback: state=%d\n", state);
}

static int gxy_glink_probe(struct platform_device *pdev)
{
    struct gxy_glink_dev *gxy_dev = NULL;
    struct pmic_glink_client_data client_data = { 0 };
    int ret = -EINVAL;
    int i = 0;

    GXY_PSY_INFO("probe start\n");
    if (!pdev || !pdev->dev.of_node)
        return -ENODEV;

    gxy_dev = devm_kzalloc(&pdev->dev, sizeof(*gxy_dev), GFP_KERNEL);
    if (!gxy_dev)
        return -ENOMEM;

    gxy_dev->psy_list[GXY_PSY_TYPE_BATTERY].opcode_get = GXY_GLINK_BATTERY_OPCODE_GET;
    gxy_dev->psy_list[GXY_PSY_TYPE_BATTERY].opcode_set = GXY_GLINK_BATTERY_OPCODE_SET;
    gxy_dev->psy_list[GXY_PSY_TYPE_BATTERY].prop_count = GXY_BATT_PROP_MAX;
    gxy_dev->psy_list[GXY_PSY_TYPE_USB].opcode_get = GXY_GLINK_USB_OPCODE_GET;
    gxy_dev->psy_list[GXY_PSY_TYPE_USB].opcode_set = GXY_GLINK_USB_OPCODE_SET;
    gxy_dev->psy_list[GXY_PSY_TYPE_USB].prop_count = GXY_USB_PROP_MAX;

    for (i = 0; i < GXY_PSY_TYPE_MAX; i++) {
        gxy_dev->psy_list[i].prop = devm_kcalloc(&pdev->dev, gxy_dev->psy_list[i].prop_count,
            sizeof(u32), GFP_KERNEL);
        if (!gxy_dev->psy_list[i].prop) {
            return -ENOMEM;
        }
    }

    gxy_dev->dev = &pdev->dev;

    mutex_init(&gxy_dev->send_lock);
    init_completion(&gxy_dev->msg_ack);

    client_data.id = GXY_GLINK_MSG_OWNER_ID;
    client_data.name = GXY_GLINK_MSG_NAME;
    client_data.msg_cb = gxy_glink_msg_callback;
    client_data.priv = gxy_dev;
    client_data.state_cb = gxy_glink_state_callback;
    gxy_dev->client = pmic_glink_register_client(gxy_dev->dev, &client_data);
    if (IS_ERR(gxy_dev->client)) {
        GXY_PSY_ERR("glink register fail\n");
        ret = -EPROBE_DEFER;
        goto fail_free_mem;
    }

    platform_set_drvdata(pdev, gxy_dev);
    g_gxy_glink_dev = gxy_dev;
    /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230913 start */
    /* AFC probe*/
    gpio_afc_init(pdev);
    gxy_dev->afc_first_detect = true;
    INIT_DELAYED_WORK(&gxy_dev->afc_notify_attach_delayed_work, afc_notifier_attach_delayed_work);
    INIT_WORK(&gxy_dev->afc_notify_detach_work, afc_notifier_detach_work);
    schedule_delayed_work(&gxy_dev->afc_notify_attach_delayed_work, msecs_to_jiffies(AFC_FIRST_ATTACH_DELAY_TIME));
    /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230913 end */
    /*M55 code for QN6887A-1117 by shixuanxuan at 20231113 start*/
    g_gxy_afc_init_done = true;
    /*M55 code for QN6887A-1117 by shixuanxuan at 20231113 end*/
    /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 start*/
    gxy_ttf_init();
    /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 end*/
    GXY_PSY_INFO("probe ok\n");

    return 0;

fail_free_mem:
    mutex_destroy(&gxy_dev->send_lock);
    return ret;
}

static int gxy_glink_remove(struct platform_device *pdev)
{
    struct gxy_glink_dev *gxy_dev = platform_get_drvdata(pdev);

    if (!gxy_dev) {
        return -ENODEV;
    }

    /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 start */
    cancel_delayed_work(&gxy_dev->afc_notify_attach_delayed_work);
    cancel_work_sync(&gxy_dev->afc_notify_detach_work);
    /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 end */
    mutex_destroy(&gxy_dev->send_lock);
    pmic_glink_unregister_client(gxy_dev->client);
    g_gxy_glink_dev = NULL;
    GXY_PSY_INFO("remove\n");
    return 0;
}

static const struct of_device_id gxy_glink_match_table[] = {
    {.compatible = "qcom,gxy_glink",},
    {},
};

static struct platform_driver gxy_glink_driver = {
    .probe = gxy_glink_probe,
    .remove = gxy_glink_remove,
    .driver = {
        .name = "qcom,gxy_glink",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(gxy_glink_match_table),
    },
};

int __init gxy_glink_init(void)
{
    GXY_PSY_INFO("gxy_glink_init\n");
    return platform_driver_register(&gxy_glink_driver);
}

void __exit gxy_glink_exit(void)
{
    platform_driver_unregister(&gxy_glink_driver);
}

module_init(gxy_glink_init);
module_exit(gxy_glink_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("gxy glink module driver");
MODULE_AUTHOR("gxy_oem");
