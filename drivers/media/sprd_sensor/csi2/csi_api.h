/*
 * csi_driver
 * 12/05/2010
 * Synopsys Inc. PT02
 */

#ifndef CSI_API_H_
#define CSI_API_H_

#include "csi_types.h"
#include "csi_driver.h"
#include <linux/spinlock.h>

#define SPRD_CSI2_REG_SIZE             0x40

typedef int (*csi2_isr_func)(u32 err_id, u32 err_status, void* u_data);

/* enumerators index the handler array rather than masks */
typedef enum
{
    ERR_PHY_TX_START = 0,
    ERR_FRAME_BOUNDARY_MATCH = 4,
    ERR_FRAME_SEQUENCE = 8,
    ERR_CRC_DURING_FRAME = 12,
    ERR_LINE_CRC = 16,
    ERR_DOUBLE_ECC = 20,
    ERR_PHY_ESCAPE_ENTRY = 21,
    ERR_ECC_SINGLE = 25,
    ERR_UNSUPPORTED_DATA_TYPE = 29,
    MAX_EVENT = 49
} csi_event_t;
typedef enum
{
    ERR_LINE_BOUNDARY_MATCH = 33,
    ERR_LINE_SEQUENCE = 41
} csi_line_event_t;
typedef enum
{
    ERROR_VC_LANE_OUT_OF_BOUND =    0xCA,
    ERROR_EVENT_TYPE_INVALID = 0xC9,
    ERROR_DATA_TYPE_INVALID = 0xC8,
    ERROR_SLOTS_FULL = 0xC7,
    ERROR_ALREADY_REGISTERED = 0xC6,
    ERROR_NOT_REGISTERED = 0xC5
} csi_api_error_t;

struct csi_context {
	u32                g_csi2_irq;
	csi2_isr_func      isr_cb;
	void              *u_data;
	spinlock_t         csi2_lock;
	handler_t          csi_api_event_registry[MAX_EVENT];
};

int csi_api_malloc(void **handle);
void csi_api_free(void *handle);
u8 csi_api_init(u32 bps_per_lane, u32 phy_id);
u8 csi_api_start(void *handle);
//u8 csi_api_start(u32 base_address);
u8 csi_api_close(void *handle, u32 phy_id);
u8 csi_api_set_on_lanes(u8 no_of_lanes);
u8 csi_api_get_on_lanes(void);
csi_lane_state_t csi_api_get_clk_state(void);
csi_lane_state_t csi_api_get_lane_state(u8 lane);
u8 csi_api_register_line_event(void *handle, u8 vc, csi_data_type_t data_type, csi_line_event_t line_event, handler_t handler);
u8 csi_api_unregister_line_event(void *handle, u8 vc, csi_data_type_t data_type, csi_line_event_t line_event);
u8 csi_api_register_event(void *handle, csi_event_t event, u8 vc_lane, handler_t handler);
u8 csi_api_unregister_event(void *handle, csi_event_t event, u8 vc_lane);
u8 csi_api_unregister_all_events(void *handle);
u8 csi_api_shut_down_phy(u8 shutdown);
u8 csi_api_reset_phy(void);
u8 csi_api_reset_controller(void);
u8 csi_api_core_write(csi_registers_t address, u32 data);
u32 csi_api_core_read(csi_registers_t address);
int csi_reg_isr(void *handle, csi2_isr_func user_func, void* user_data);
int csi_read_registers(uint32_t* reg_buf, uint32_t *buf_len);

#endif /* CSI_API_H_ */

