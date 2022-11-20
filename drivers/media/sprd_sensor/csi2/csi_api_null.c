#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/globalregs.h>
#include <mach/sci.h>

#include "csi_api.h"
#include "csi_log.h"


static DEFINE_SPINLOCK(csi2_lock);

static u32            g_csi2_irq = 0x12000034;
static handler_t      csi_api_event_registry[MAX_EVENT] = {NULL};
static csi2_isr_func  isr_cb = NULL;
static void           *u_data = NULL;

void csi_api_event1_handler(void *param);
void csi_api_event2_handler(void *param);

u8 csi_api_init(u32 pclk, u32 phy_id)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_start(void)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_close(u32 phy_id)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_set_on_lanes(u8 no_of_lanes)
{
	return ERR_NOT_COMPATIBLE;
}

u8 csi_api_get_on_lanes() 
{
	return ERR_NOT_COMPATIBLE;
}
csi_lane_state_t csi_api_get_clk_state()
{
	return ERR_NOT_COMPATIBLE;
}
csi_lane_state_t csi_api_get_lane_state(u8 lane)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_register_event(csi_event_t event, u8 vc_lane, handler_t handler)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_unregister_event(csi_event_t event, u8 vc_lane)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_register_line_event(u8 vc, csi_data_type_t data_type, csi_line_event_t line_event, handler_t handler)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_unregister_line_event(u8 vc, csi_data_type_t data_type, csi_line_event_t line_event)
{
	return ERR_NOT_COMPATIBLE;
}
void csi_api_event1_handler(void *param)
{
	return ERR_NOT_COMPATIBLE;
}
void csi_api_event2_handler(void *param)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_unregister_all_events()
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_shut_down_phy(u8 shutdown)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_reset_phy()
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_reset_controller()
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_core_write(csi_registers_t address, u32 data)
{
	return ERR_NOT_COMPATIBLE;
}
u32 csi_api_core_read(csi_registers_t address)
{
	return ERR_NOT_COMPATIBLE;
}

int csi_reg_isr(csi2_isr_func user_func, void* user_data)
{
	unsigned long                flag;

	spin_lock_irqsave(&csi2_lock, flag);
	isr_cb = user_func;
	u_data = user_data;
	spin_unlock_irqrestore(&csi2_lock, flag);
	return 0;
}

int csi_read_registers(u32* reg_buf, u32 *buf_len)
{
	u32                *reg_addr = (u32*)SPRD_CSI2_BASE;

	if (NULL == reg_buf || NULL == buf_len || 0 != (*buf_len % 4)) {
		return -1;
	}

	while (buf_len != 0 && (u32)reg_addr < (SPRD_CSI2_BASE + SPRD_CSI2_REG_SIZE)) {
		*reg_buf++ = *(volatile u32*)reg_addr++;
		*buf_len -= 4;
	}

	*buf_len = SPRD_CSI2_REG_SIZE;
	return 0;
}
