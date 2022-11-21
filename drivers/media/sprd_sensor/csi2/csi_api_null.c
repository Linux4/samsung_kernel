#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>


#include "csi_api.h"
#include "csi_log.h"

void csi_api_event1_handler(int irq, void *handle);
void csi_api_event2_handler(int irq, void *handle);
int csi_api_malloc(void **handle)
{
	return ERR_NOT_COMPATIBLE;
}

void csi_api_free(void *handle)
{
}
u8 csi_api_init(u32 bps_per_lane, u32 phy_id)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_start(void *handle)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_close(void *handle, u32 phy_id)
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
u8 csi_api_register_event(void *handle, csi_event_t event, u8 vc_lane, handler_t handler)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_unregister_event(void *handle, csi_event_t event, u8 vc_lane)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_register_line_event(void *handle, u8 vc, csi_data_type_t data_type, csi_line_event_t line_event, handler_t handler)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_unregister_line_event(void *handle, u8 vc, csi_data_type_t data_type, csi_line_event_t line_event)
{
	return ERR_NOT_COMPATIBLE;
}
void csi_api_event1_handler(int irq, void *handle)
{
	return ERR_NOT_COMPATIBLE;
}
void csi_api_event2_handler(int irq, void *handle)
{
	return ERR_NOT_COMPATIBLE;
}
u8 csi_api_unregister_all_events(void *handle)
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

int csi_reg_isr(void *handle, csi2_isr_func user_func, void* user_data)
{
	unsigned long                flag;
	struct csi_context *csi_handle = handle;
	if (NULL == handle) {
		printk("handle null\n");
		return ERR_UNDEFINED;
	}

	spin_lock_irqsave(&csi_handle->csi2_lock, flag);
	csi_handle->isr_cb = user_func;
	csi_handle->u_data = user_data;
	spin_unlock_irqrestore(&csi_handle->csi2_lock, flag);
	return 0;
}

int csi_read_registers(u32* reg_buf, u32 *buf_len)
{
	u32                *reg_addr = (u32*)CSI2_BASE;

	if (NULL == reg_buf || NULL == buf_len || 0 != (*buf_len % 4)) {
		return -1;
	}

	while (buf_len != 0 && (u32)reg_addr < (CSI2_BASE + SPRD_CSI2_REG_SIZE)) {
		*reg_buf++ = *(volatile u32*)reg_addr++;
		*buf_len -= 4;
	}

	*buf_len = SPRD_CSI2_REG_SIZE;
	return 0;
}
