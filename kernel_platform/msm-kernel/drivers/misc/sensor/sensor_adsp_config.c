#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/of_device.h>

#define SENSOR_NUM 2
#define ADDR_SHIFT 12
int g_sensor_flag[SENSOR_NUM] = {0};

struct class *sensor_class = NULL;
struct device *tp_info = NULL;

int g_borad_id    = 1;
char g_tp_rcv[3]  = {0};
char *g_panel_rcv = NULL;

enum board_vendor_info {
    TP_BOARD_FIRST = 1,
    TP_BOARD_SECOND,
};

struct tp_vendor_info {
    enum board_vendor_info board_info;
    char *tp_name;
};

void identify_tp_info(void)
{
    int i = 0;
    struct device_node *chosen = NULL;
    const char *panel_name = NULL;

    struct tp_vendor_info tp_info_list[] = {
        {TP_BOARD_FIRST,   "80"},
        {TP_BOARD_FIRST,   "12"},
        {TP_BOARD_SECOND,  "22"},
    };

    chosen = of_find_node_by_name(NULL, "chosen");
    if (NULL == chosen) {
        pr_info("adsp_config identify bootargs error");
    } else {
        of_property_read_string(chosen, "bootargs", &panel_name);

        if (NULL != strstr(panel_name, "PanelID=")) {
            g_panel_rcv = strstr(panel_name, "PanelID=");
            strncpy(g_tp_rcv, g_panel_rcv + ADDR_SHIFT, 2);
            pr_info("%s adsp_config panel_rcv= %s, tp_rcv= %s\n", __func__, g_panel_rcv, g_tp_rcv);

            for (i = 0; i < sizeof(tp_info_list)/sizeof(tp_info_list[0]); i++) {
                if (0 == strcmp(g_tp_rcv, tp_info_list[i].tp_name)) {
                    g_borad_id = tp_info_list[i].board_info;
                    pr_info("adsp_config board_id %d, tp_name:%s", g_borad_id, g_tp_rcv);
                    return;
                }
            }
        }
    }
    pr_info("adsp_config not find, tp_name:%s", g_tp_rcv);
    g_borad_id = 1;
    return;
}

static ssize_t tp_info_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    pr_info("%s:adsp_config g_borad_id = %d\n", __func__, g_borad_id);
    return snprintf(buf, PAGE_SIZE, "%d\n", g_borad_id);
}

static DEVICE_ATTR(tp_info, 0444, tp_info_show, NULL);

static struct device_attribute *sensor_attrs[] = {
    &dev_attr_tp_info,
    NULL,
};

static int __init sensor_adsp_init(void)
{
    int i = 0;

    pr_info("%s\n", __func__);

    sensor_class = class_create(THIS_MODULE, "sensor");

    if (IS_ERR(sensor_class)) {
        pr_err("%s: adsp_config create sensor_class is failed.\n", __func__);
        return PTR_ERR(sensor_class);
    }

    sensor_class->dev_uevent = NULL;

    //create tp_info
    tp_info = device_create(sensor_class, NULL, 0, NULL, "sensor");

    if (IS_ERR(tp_info)) {
        pr_err("%s: adsp_config device_create failed!\n", __func__);
        class_destroy(sensor_class);
        return PTR_ERR(tp_info);
    }

    for (i = 0; sensor_attrs[i] != NULL; i++) {
        g_sensor_flag[i] = device_create_file(tp_info, sensor_attrs[i]);

        if (g_sensor_flag[i]) {
            pr_err("%s: fail device_create_file (tpinfo, sensor_attrs[%d])\n", __func__, i);
            device_destroy(sensor_class, 0);
            class_destroy(sensor_class);
            return g_sensor_flag[i];
        }
    }

    identify_tp_info();

    return 0;
}

static void __exit sensor_adsp_exit(void)
{
    int i = 0;

    for (i = 0; sensor_attrs[i] != NULL; i++) {
        if (!g_sensor_flag[i]) {
            device_remove_file(tp_info, &dev_attr_tp_info);
            device_destroy(sensor_class, 0);
            class_destroy(sensor_class);
        }
    }
    sensor_class = NULL;
}

module_init(sensor_adsp_init);
module_exit(sensor_adsp_exit);

MODULE_DESCRIPTION("sensor adsp config");
MODULE_LICENSE("GPL");