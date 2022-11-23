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


#include <linux/delay.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/bitops.h>
#include <mach/board.h>
#include "ipc_gpio.h"
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>

#define SDHCI_RESEND_SUPPORT

#define DRIVER_NAME "sdhci_hal"

#define DBG(f, x...) 	pr_debug(DRIVER_NAME " [%s()]: " f, __func__,## x)

#define MAX_MIPC_WAIT_TIMEOUT     2000

static unsigned int cp_to_ap_rdy_irq;
static unsigned int cp_to_ap_rts_irq;


static wait_queue_head_t   s_gpio_cp_ready_wq;

extern void process_modem_packet(unsigned long data);

int cp2ap_rdy(void)
{
        DBG("SDIO :cp2ap_rdy=%d\r\n",gpio_get_value(GPIO_CP_TO_AP_RDY));
        return gpio_get_value(GPIO_CP_TO_AP_RDY);
}

void ap2cp_rts_enable(void)
{
        printk("SDIO ap2cp_rts_enable AP_TO_CP_RTS is 1\r\n");
        gpio_set_value(GPIO_AP_TO_CP_RTS, 1);
}

void ap2cp_rts_disable(void)
{
        printk("SDIO ap2cp_rts_disable AP_TO_CP_RTS is 0\r\n");
        gpio_set_value(GPIO_AP_TO_CP_RTS, 0);
}

extern  u32  wake_up_mipc_rx_thread(u32  need_to_rx_data);
extern void set_cp_awake(bool awake);


static irqreturn_t cp_to_ap_rdy_handle(int irq, void *handle)
{
        printk/*DBG*/("[SDIO_IPC]cp_to_ap_rdy_handle:%d\r\n ", cp2ap_rdy());

        set_cp_awake(true);
        if(!cp2ap_rdy()) {
                wake_up_mipc_rx_thread(1);
        }
        return IRQ_HANDLED;
}

/*****************************************************************************\
 *                                                                           *
 * Driver init/exit                                                          *
 *                                                                           *
\*****************************************************************************/

int sdhci_hal_gpio_init(void)
{
        int ret;

        DBG("sdhci_hal_gpio init \n");

        init_waitqueue_head(&s_gpio_cp_ready_wq);

        //config ap_to_cp_rts
        ret = gpio_request(GPIO_AP_TO_CP_RTS, "ap_to_cp_rts");
        if (ret) {
                DBG("Cannot request GPIO %d\r\n", GPIO_AP_TO_CP_RTS);
                gpio_free(GPIO_AP_TO_CP_RTS);
                return ret;
        }
        gpio_direction_output(GPIO_AP_TO_CP_RTS, 1); //use new sdio dma option, default value change to 1
        gpio_export(GPIO_AP_TO_CP_RTS,  1);


        //config cp_out_rdy
        ret = gpio_request(GPIO_CP_TO_AP_RDY, "cp_out_rdy");
        if (ret) {
                DBG("Cannot request GPIO %d\r\n",GPIO_CP_TO_AP_RDY);
                gpio_free(GPIO_CP_TO_AP_RDY);
                return ret;
        }
        gpio_direction_input(GPIO_CP_TO_AP_RDY);
        gpio_export(GPIO_CP_TO_AP_RDY,  1);

        return 0;
}

int sdhci_hal_gpio_irq_init(void)
{
        int ret;

        DBG("sdhci_hal_gpio init \n");

        cp_to_ap_rdy_irq = gpio_to_irq(GPIO_CP_TO_AP_RDY);
        if (cp_to_ap_rdy_irq < 0)
                return -1;
        ret = request_threaded_irq(cp_to_ap_rdy_irq, cp_to_ap_rdy_handle,
                                   NULL, IRQF_DISABLED|IRQF_TRIGGER_FALLING, "cp_to_ap_rdy_irq", NULL);
        if (ret) {
                DBG("lee :cannot alloc cp_to_ap_rdy_irq, err %d\r\r\n", ret);
                return ret;
        }

        return 0;

}

void sdhci_hal_gpio_exit(void)
{
        DBG("sdhci_hal gpio exit\r\n");

        gpio_free(GPIO_AP_TO_CP_RTS);
        gpio_free(GPIO_CP_TO_AP_RDY);

        free_irq(cp_to_ap_rdy_irq, NULL);
        free_irq(cp_to_ap_rts_irq, NULL);
}

//MODULE_AUTHOR("Bin.Xu<bin.xu@spreadtrum.com>");
//MODULE_DESCRIPTION("GPIO driver about mux sdio");
//MODULE_LICENSE("GPL");
