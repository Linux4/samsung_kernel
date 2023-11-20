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

#define PIN_STATUS_ACTIVE 1
#define PIN_STATUS_INACTIVE 0


#define DRIVER_NAME "sdhci_hal"

#define DBG(f, x...) 	pr_debug(DRIVER_NAME " [%s()]: " f, __func__,## x)

#define MAX_MIPC_WAIT_TIMEOUT     2000

static unsigned int cp_to_ap_req_irq;


static wait_queue_head_t   s_gpio_cp_ready_wq;

extern void process_modem_packet(unsigned long data);

int cp2ap_req(void)
{
        DBG("[SDIO_IPC] cp_to_ap_req=%d\r\n",gpio_get_value(GPIO_CP_TO_AP_RDY));
        return (PIN_STATUS_ACTIVE == gpio_get_value(GPIO_CP_TO_AP_RDY));
}

void ap2cp_req_enable(void)
{
        DBG("SDIO ap2cp_req_enable AP_TO_CP_RTS is 1\r\n");
        gpio_set_value(GPIO_AP_TO_CP_RTS, PIN_STATUS_ACTIVE);
}

void ap2cp_req_disable(void)
{
        DBG("SDIO ap2cp_req_disable AP_TO_CP_RTS is 0\r\n");
        gpio_set_value(GPIO_AP_TO_CP_RTS, PIN_STATUS_INACTIVE);
}

extern  u32  wake_up_mipc_rx_thread(u32  need_to_rx_data);
extern void set_cp_awake(bool awake);


static irqreturn_t cp_to_ap_req_handle(int irq, void *handle)
{
        int cur = cp2ap_req();

        if(cur) {
                irq_set_irq_type(cp_to_ap_req_irq,  IRQ_TYPE_LEVEL_LOW);
        } else  {
                irq_set_irq_type(cp_to_ap_req_irq,  IRQ_TYPE_LEVEL_HIGH);
        }

        DBG("[SDIO_IPC]cp_to_ap_req_handle:%d\r\n ", cur);

        if(cur) {
                set_cp_awake(true);
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

        cp_to_ap_req_irq = gpio_to_irq(GPIO_CP_TO_AP_RDY);
        if (cp_to_ap_req_irq < 0)
                return -1;
        ret = request_threaded_irq(cp_to_ap_req_irq, cp_to_ap_req_handle,
                                   NULL, IRQF_NO_SUSPEND|IRQF_DISABLED|IRQF_TRIGGER_HIGH,
                                   "cp_to_ap_req_irq", NULL);
        if (ret) {
                DBG("lee :cannot alloc cp_to_ap_req_irq, err %d\r\r\n", ret);
                return ret;
        }
        enable_irq_wake(cp_to_ap_req_irq);
        if(cp2ap_req()) {
            printk("[SDIO_IPC]initial check call cp_to_ap_req_handle()\r\n ");
            cp_to_ap_req_handle(0, NULL);
        }
        return 0;

}

void sdhci_hal_gpio_exit(void)
{
        DBG("sdhci_hal gpio exit\r\n");

        gpio_free(GPIO_AP_TO_CP_RTS);
        gpio_free(GPIO_CP_TO_AP_RDY);

        free_irq(cp_to_ap_req_irq, NULL);
}

//MODULE_AUTHOR("Bin.Xu<bin.xu@spreadtrum.com>");
//MODULE_DESCRIPTION("GPIO driver about mux sdio");
//MODULE_LICENSE("GPL");
