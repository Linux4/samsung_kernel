#include <linux/delay.h>
#include "csi_driver.h"
#include "csi_access.h"
#include "csi_log.h"
#include <mach/hardware.h>
#include <mach/sci.h>


static int csi_core_initialized = 0;

static const struct csi_pclk_cfg csi_pclk_setting[CSI_PCLK_CFG_COUNTER] = {
	{80,    90,   0x00, 4},
	{90,   100, 0x10, 4},
	{100, 110, 0x20, 4},
	{110, 130, 0x01, 6},
	{130, 140, 0x11, 6},
	{140, 150, 0x21, 6},
	{150, 170, 0x02, 9},
	{170, 180, 0x12, 9},
	{180, 200, 0x22, 9},
	{200, 220, 0x03, 10},
	{220, 240, 0x13, 10},
	{240, 250, 0x23, 10},
	{250, 270, 0x04, 13},
	{270, 300, 0x14, 13},
	{300, 330, 0x05, 17},
	{330, 360, 0x15, 17},
	{360, 400, 0x25, 17},
	{400, 450, 0x06, 25},
	{450, 500, 0x16, 26},
	{500, 550, 0x07, 28},
	{550, 600, 0x17, 28},
	{600, 650, 0x08, 34},
	{650, 700, 0x18, 37},
	{700, 750, 0x09, 38},
	{750, 800, 0x19, 38},
	{800, 850, 0x29, 38},
	{850, 900, 0x39, 38},
	{900, 950, 0x0A, 52},
	{950, 1000, 0x1A, 52}
};

#if defined(CONFIG_ARCH_SCX30G)
static void dpy_ab_clr(void)
{
	sci_glb_clr(SPRD_MMAHB_BASE + 0x000C, 0x1F);
}

static void dpy_a_enable(void)
{
	sci_glb_clr(SPRD_MMAHB_BASE + 0x000C, 0x07);
	sci_glb_set(SPRD_MMAHB_BASE + 0x000C, 0x0C);
}

static void dpy_b_enable(void)
{
	sci_glb_clr(SPRD_MMAHB_BASE + 0x000C, 0x07);
	sci_glb_set(SPRD_MMAHB_BASE + 0x000C, 0x0F);
}

static void dpy_ab_sync(void)
{
	sci_glb_clr(SPRD_MMAHB_BASE + 0x000C, 0x07);
}
#endif

static void dphy_write(u8 test_code, u8 test_data, u8* test_out)
{
	u32 temp = 0xffffff00;

	csi_core_write_part(PHY_TST_CRTL1, 0, PHY_TESTEN, 1); //phy_testen = 0
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0,1, PHY_TESTCLK, 1); //phy_testclk = 1
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL1, test_code, PHY_TESTDIN, PHY_TESTDIN_W); //phy_testdin
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL1, 1, PHY_TESTEN, 1);//phy_testen = 1
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0, 0, PHY_TESTCLK, 1);//phy_testclk = 0
	udelay(1);
	temp = csi_core_read_part(PHY_TST_CRTL1, PHY_TESTDOUT,PHY_TESTDOUT_W);
	*test_out = (u8)temp;
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL1, 0, PHY_TESTEN, 1); //phy_testen = 0
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL1, test_data, PHY_TESTDIN, PHY_TESTDIN_W);//phy_testdin
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0,1, PHY_TESTCLK, 1);//phy_testclk = 1
	udelay(1);
}

static void dphy_cfg_start(void)
{
	csi_core_write_part(PHY_TST_CRTL1, 0, PHY_TESTEN, 1); //phy_testen = 0
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0,1, PHY_TESTCLK, 1); //phy_testclk = 1
	udelay(1);
}

static void dphy_cfg_done(void)
{
	csi_core_write_part(PHY_TST_CRTL1, 0, PHY_TESTEN, 1); //phy_testen = 0
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0,1, PHY_TESTCLK, 1); //phy_testclk = 1
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL1, 0, PHY_TESTDIN, PHY_TESTDIN_W);//phy_testdin
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL1, 1, PHY_TESTEN, 1);//phy_testen = 1
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0, 0, PHY_TESTCLK, 1);//phy_testclk = 0
	udelay(1);
}

static void csi_get_pclk_cfg(u32 pclk, struct csi_pclk_cfg *csi_pclk_cfg_ptr)
{
	u32 i = 0;
	int rtn = -1;

	for (i =0 ; i < CSI_PCLK_CFG_COUNTER; i++) {
		if ( pclk >= csi_pclk_setting[i].pclk_start && pclk < csi_pclk_setting[i].pclk_end) {
			csi_pclk_cfg_ptr->hsfreqrange = csi_pclk_setting[i].hsfreqrange;
			csi_pclk_cfg_ptr->hsrxthssettle = csi_pclk_setting[i].hsrxthssettle;
			rtn = 0;
			break;
		}
	}

	if (rtn) {
		csi_pclk_cfg_ptr->hsfreqrange = csi_pclk_setting[CSI_PCLK_CFG_COUNTER -2].hsfreqrange;
		csi_pclk_cfg_ptr->hsrxthssettle = csi_pclk_setting[CSI_PCLK_CFG_COUNTER -2].hsrxthssettle;
	}
}

static void dphy_init_common(u32 pclk, u32 phy_id, u32 rx_mode)
{
	u8 temp = 0;
	struct csi_pclk_cfg csi_pclk_cfg_val = {0, 0, 0, 0};

	csi_core_write_part(PHY_SHUTDOWNZ,  0, 0, 1);
	csi_core_write_part(DPHY_RSTZ, 0, 0, 1);
	csi_core_write_part(PHY_TST_CRTL0, 1, PHY_TESTCLR, 1);
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0, 0, PHY_TESTCLR, 1);
	udelay(1);
	dphy_cfg_start();

	csi_get_pclk_cfg(pclk, &csi_pclk_cfg_val);

#if defined(CONFIG_ARCH_SCX30G)
	if (0x03 == phy_id) {
		if (0x00 == rx_mode) {
			dphy_write(0x34, 0xA0, &temp);
		} else {
			dphy_write(0x34, 0x14, &temp);
		}
	} else {
		dphy_write(0x34, 0x14, &temp);
	}
#else
	dphy_write(0x34, 0x14, &temp);
#endif

	dphy_write(0x44, (((csi_pclk_cfg_val.hsfreqrange & 0x3F) << 1) & 0x7E), &temp);
	dphy_write(0x75, (0x80 | (csi_pclk_cfg_val.hsrxthssettle & 0x7F)), &temp);
	dphy_write(0x54, 0x14, &temp);
	dphy_write(0x64, 0x14, &temp);
	dphy_write(0x74, 0x14, &temp);

	dphy_cfg_done();
}

void dphy_init(u32 pclk, u32 phy_id)
{
#if defined(CONFIG_ARCH_SCX30G)
	dpy_ab_clr();

	if (phy_id & 0x01) {
		dpy_a_enable();
		dphy_init_common(pclk, phy_id, 0);
	}

	if (phy_id & 0x02) {
		dpy_b_enable();
		dphy_init_common(pclk, phy_id, 1);
	}

	if (0x03 == (phy_id & 0x03)) {
		dpy_ab_sync();
	}
#else
	dphy_init_common(pclk, phy_id, 1);
#endif
}

u8 csi_init(u32 base_address)
{
    csi_error_t e = SUCCESS;
    do
    {
        if (csi_core_initialized == 0)
        {
            access_init((u32*)base_address);
            if (csi_core_read(VERSION) == (u32)(CURRENT_VERSION))
            {
                csi_core_initialized = 1;
                break;
            }
            else
            {
                LOG_ERROR("Driver not compatible with core");
                e = ERR_NOT_COMPATIBLE;
                break;
            }
        }
        else
        {
            LOG_ERROR("driver already initialised");
            e = ERR_ALREADY_INIT;
            break;
        }

    } while(0);
    return e;
}

u8 csi_close()
{
    csi_shut_down_phy(1);
    csi_reset_controller();
    csi_core_initialized = 0;
    return SUCCESS;
}

u8 csi_get_on_lanes()
{
    return (csi_core_read_part(N_LANES, 0, 2) + 1);
}

u8 csi_set_on_lanes(u8 lanes)
{
	return csi_core_write_part(N_LANES, (lanes - 1), 0, 2);
}

u8 csi_shut_down_phy(u8 shutdown)
{
    LOG_DEBUG2("shutdown", shutdown);
    /*  active low - bit 0 */
    return csi_core_write_part(PHY_SHUTDOWNZ, shutdown? 0: 1, 0, 1);
}

u8 csi_reset_phy()
{
    /*  active low - bit 0 */
    int retVal = 0xffff;
    retVal = csi_core_write_part(DPHY_RSTZ, 0, 0, 1);
    switch (retVal)
    {
    case SUCCESS:
        return csi_core_write_part(DPHY_RSTZ, 1, 0, 1);
        break;
    case ERR_NOT_INIT:
        LOG_ERROR("Driver not initialized");
        return retVal;
        break;
    default:
        LOG_ERROR("Undefined error");
        return ERR_UNDEFINED;
        break;
    }
}

u8 csi_reset_controller()
{
    /*  active low - bit 0 */
    int retVal = 0xffff;
    retVal = csi_core_write_part(CSI2_RESETN, 0, 0, 1);
    switch (retVal)
    {
    case SUCCESS:
        return csi_core_write_part(CSI2_RESETN, 1, 0, 1);
        break;
    case ERR_NOT_INIT:
        LOG_ERROR("Driver not initialized");
        return retVal;
        break;
    default:
        LOG_ERROR("Undefined error");
        return ERR_UNDEFINED;
        break;
    }
}

csi_lane_state_t csi_lane_module_state(u8 lane)
{
    if (lane < csi_core_read_part(N_LANES, 0, 2) + 1)
    {
        if (csi_core_read_part(PHY_STATE, lane, 1))
        {
            return CSI_LANE_ULTRA_LOW_POWER;
        }
        else if (csi_core_read_part(PHY_STATE, (lane + 4), 1))
        {
            return CSI_LANE_STOP;
        }
        return CSI_LANE_ON;
    }
    else
    {
        LOG_WARNING("Lane switched off");
        return CSI_LANE_OFF;
    }
}

csi_lane_state_t csi_clk_state()
{
    if (!csi_core_read_part(PHY_STATE, 9, 1))
    {
        return CSI_LANE_ULTRA_LOW_POWER;
    }
    else if (csi_core_read_part(PHY_STATE, 10, 1))
    {
        return CSI_LANE_STOP;
    }
    else if (csi_core_read_part(PHY_STATE, 8, 1))
    {
        return CSI_LANE_HIGH_SPEED;
    }
    return CSI_LANE_ON;
}

u8 csi_payload_bypass(u8 on)
{
    return csi_core_write_part(PHY_STATE, on? 1: 0, 11, 1);
}

u8 csi_register_line_event(u8 virtual_channel_no, csi_data_type_t data_type, u8 offset)
{
    u8 id = 0;
    csi_registers_t reg_offset = 0;
    LOG_TRACE();
    if ((virtual_channel_no > 4) || (offset > 8))
    {
        return ERR_OUT_OF_BOUND;
    }
    id = (virtual_channel_no << 6) | data_type;

    reg_offset = ((offset / 4) == 1) ? DATA_IDS_2: DATA_IDS_1;

    return csi_core_write_part(reg_offset, id, (offset * 8), 8);
}
u8 csi_unregister_line_event(u8 offset)
{
    csi_registers_t reg_offset = 0;
    LOG_TRACE();
    if (offset > 8)
    {
        return ERR_OUT_OF_BOUND;
    }
    reg_offset = ((offset / 4) == 1) ? DATA_IDS_2: DATA_IDS_1;
    return csi_core_write_part(reg_offset, 0x00, (offset * 8), 8);
}

u8 csi_get_registered_line_event(u8 offset)
{
    csi_registers_t reg_offset = 0;
    LOG_TRACE();
    if (offset > 8)
    {
        return ERR_OUT_OF_BOUND;
    }
    reg_offset = ((offset / 4) == 1) ? DATA_IDS_2: DATA_IDS_1;
    return (u8)csi_core_read_part(reg_offset, (offset * 8), 8);
}

u8 csi_event_disable(u32 mask, u8 err_reg_no)
{
    switch (err_reg_no)
    {
        case 1:
            return csi_core_write(MASK1, mask | csi_core_read(MASK1));
        case 2:
            return csi_core_write(MASK2, mask | csi_core_read(MASK2));
        default:
            return ERR_OUT_OF_BOUND;
    }
}

u8 csi_event_enable(u32 mask, u8 err_reg_no)
{
    switch (err_reg_no)
    {
        case 1:
            return csi_core_write(MASK1, (~mask) & csi_core_read(MASK1));
        case 2:
            return csi_core_write(MASK2, (~mask) & csi_core_read(MASK2));
        default:
            return ERR_OUT_OF_BOUND;
    }
}
u32 csi_event_get_source(u8 err_reg_no)
{
    switch (err_reg_no)
    {
        case 1:
            return csi_core_read(ERR1);
        case 2:
            return csi_core_read(ERR2);
        default:
            return ERR_OUT_OF_BOUND;
    }
}

/*  register methods */
u32 csi_core_read(csi_registers_t address)
{
    return access_read(address>>2);
}

u8 csi_core_write(csi_registers_t address, u32 data)
{
    if (csi_core_initialized == 0)
    {
        LOG_ERROR("driver not initialised");
        return ERR_NOT_INIT;
    }
    LOG_DEBUG3("write", data, address>>2);
    access_write(data, address>>2);
    return SUCCESS;
}

u8 csi_core_write_part(csi_registers_t address, u32 data, u8 shift, u8 width)
{
    u32 mask = (1 << width) - 1;
    u32 temp = csi_core_read(address);
    temp &= ~(mask << shift);
    temp |= (data & mask) << shift;
    return csi_core_write(address, temp);
}

u32 csi_core_read_part(csi_registers_t address, u8 shift, u8 width)
{
    return (csi_core_read(address) >> shift) & ((1 << width) - 1);
}

