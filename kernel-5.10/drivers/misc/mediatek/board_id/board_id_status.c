#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <asm/uaccess.h>
#include <linux/printk.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

/* 0 = pre board. 1 = official board ;*/
int g_board_id_status = 0;
EXPORT_SYMBOL(g_board_id_status);

module_param(g_board_id_status, int, 0444);
MODULE_PARM_DESC(g_board_id_status, "show board id status");

static int odm_board_id_pin[3]={};
static int id_pin_status[3]={};
static int board_id_parse_dt(struct device *dev)
{
    struct device_node *np = NULL;

    if (!dev) {
        return -ENODEV;
    }

    np = dev->of_node;
    odm_board_id_pin[0] = of_get_named_gpio(np, "board_id_num1", 0);

    if (!gpio_is_valid(odm_board_id_pin[0])) {
        pr_err("Invalid GPIO, odm_board_id_pin[0]:%d",
            odm_board_id_pin[0]);
        return -EINVAL;
    }
    id_pin_status[0] = gpio_get_value(odm_board_id_pin[0]);
    pr_notice("[board_id]  board_id [%d]status=%d...\n",odm_board_id_pin[0],id_pin_status[0]);

    odm_board_id_pin[1] = of_get_named_gpio(np, "board_id_num2", 0);

    if (!gpio_is_valid(odm_board_id_pin[1])) {
        pr_err("Invalid GPIO, odm_board_id_pin[1]:%d",
            odm_board_id_pin[1]);
        return -EINVAL;
    }
    id_pin_status[1] = gpio_get_value(odm_board_id_pin[1]);
    pr_notice("[board_id]  board_id [%d]status=%d...\n",odm_board_id_pin[1],id_pin_status[1]);

    odm_board_id_pin[2] = of_get_named_gpio(np, "board_id_num3", 0);

    if (!gpio_is_valid(odm_board_id_pin[2])) {
        pr_err("Invalid GPIO, odm_board_id_pin[2]:%d",
            odm_board_id_pin[2]);
        return -EINVAL;
    }
    id_pin_status[2] = gpio_get_value(odm_board_id_pin[2]);
    pr_notice("[board_id]  board_id [%d]status=%d...\n",odm_board_id_pin[2],id_pin_status[2]);

    return 0;
}

static int board_id_probe(struct platform_device *pdev)
{
    int rc = 0;

    pr_info("[%s]enter\n", __func__);

    if (pdev->dev.of_node) {
        rc = board_id_parse_dt(&pdev->dev);
        if (rc < 0) {
            pr_err("board_id_parse_dt fail...\n");
            return -EINVAL;
        }
    }
    /*Tab A9 code for SR-AX6739A-01-309 by xiongxiaoliang at 20230504 start*/
    g_board_id_status = id_pin_status[0] | (id_pin_status[1] << 1) | (id_pin_status[2] << 2);
    /*Tab A9 code for SR-AX6739A-01-309 by xiongxiaoliang at 20230504 end*/
    if (g_board_id_status) {
        pr_info("[%s]hw is official board ...\n", __func__);
    } else {
        pr_info("[%s] is pre board...\n", __func__);
    }

    return 0;
}

static int board_id_remove(struct platform_device *pdev)
{
    pr_info("board id driver remove...\n");

    return 0;
}

static const struct of_device_id board_id_detpin_dt_match[] = {
    {.compatible = "odm,board-id-pin"},
    {},
};
MODULE_DEVICE_TABLE(of, board_id_detpin_dt_match);

static struct platform_driver board_id_driver = {
    .driver = {
        .name = "board_id_pin",
        .owner = THIS_MODULE,
        .of_match_table = board_id_detpin_dt_match,
    },
    .probe = board_id_probe,
    .remove = board_id_remove,
};

static int __init board_id_init(void)
{
    int rc = 0;

    pr_info("board id driver init...\n");
    rc = platform_driver_register(&board_id_driver);
    if (rc) {
        pr_err("%s: Failed to register board id driver\n",
                __func__);
        return rc;
    }

    return 0;
}

static void __exit board_id_exit(void)
{
    pr_info("board-id driver exit...\n");
    platform_driver_unregister(&board_id_driver);
}

module_init(board_id_init)
module_exit(board_id_exit)
MODULE_DESCRIPTION("add function show board id status");
MODULE_LICENSE("GPL v2");