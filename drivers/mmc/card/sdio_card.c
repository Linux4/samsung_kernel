/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/mmc.h>
#include <linux/pm_runtime.h>

#ifdef CONFIG_PM_RUNTIME

#define DRV_NAME "sprd-sdio-card"

static int sprd_sdio_card_probe(struct mmc_card *card) {
    struct mmc_host *host = card->host;
    if (mmc_card_sdio(card)) {
        if(host->caps & MMC_CAP_POWER_OFF_CARD) {
            pm_runtime_no_callbacks(&card->dev); // avoid default sdio bus runtime calling
            return 0;
        }
    }
    return -EINVAL;
}

static void sprd_sdio_card_remove(struct mmc_card *card) {
}

static struct mmc_driver sprd_sdio_card_driver = {
    .drv.name  = DRV_NAME,
    .probe         = sprd_sdio_card_probe,
    .remove      = sprd_sdio_card_remove,
};

static int __init sprd_sdio_card_init(void) {
    return mmc_register_driver(&sprd_sdio_card_driver);
}

static void __exit sprd_sdio_card_exit(void) {
    mmc_unregister_driver(&sprd_sdio_card_driver);
}

subsys_initcall(sprd_sdio_card_init);
module_exit(sprd_sdio_card_exit);

#endif
