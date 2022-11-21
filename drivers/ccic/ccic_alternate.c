/*
 * driver/ccic/ccic_alternate.c - S2MM005 USB CCIC Alternate mode driver
 * 
 * Copyright (C) 2016 Samsung Electronics
 * Author: Wookwang Lee <wookwang.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/ccic/s2mm005.h>
#include <linux/ccic/s2mm005_ext.h>
#include <linux/ccic/ccic_alternate.h>
/* switch device header */
#if defined(CONFIG_SWITCH)
#include <linux/switch.h>
#endif /* CONFIG_SWITCH */
////////////////////////////////////////////////////////////////////////////////
// s2mm005_cc.c called s2mm005_alternate.c
////////////////////////////////////////////////////////////////////////////////
#if defined(CONFIG_CCIC_ALTERNATE_MODE)
#if defined(CONFIG_SWITCH)
static struct switch_dev switch_dock = {
	.name = "ccic_dock",
};
#endif /* CONFIG_SWITCH */

static char VDM_MSG_IRQ_State_Print[7][40] =
{
    {"bFLAG_Vdm_Reserve_b0"},
    {"bFLAG_Vdm_Discover_ID"},
    {"bFLAG_Vdm_Discover_SVIDs"},
    {"bFLAG_Vdm_Discover_MODEs"},
    {"bFLAG_Vdm_Enter_Mode"},
    {"bFLAG_Vdm_Exit_Mode"},
    {"bFLAG_Vdm_Attention"},
};
////////////////////////////////////////////////////////////////////////////////
// Alternate mode processing
////////////////////////////////////////////////////////////////////////////////
int ccic_register_switch_device(int mode)
{
#ifdef CONFIG_SWITCH
	int ret = 0;
	if (mode) {
		ret = switch_dev_register(&switch_dock);
		if (ret < 0) {
			pr_err("%s: Failed to register dock switch(%d)\n",
				__func__, ret);
			return -ENODEV;
		}
	} else {
		switch_dev_unregister(&switch_dock);
	}
#endif /* CONFIG_SWITCH */
	return 0;
}

static void ccic_send_dock_intent(int type)
{
	pr_info("%s: CCIC dock type(%d)\n", __func__, type);
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_dock, type);
#endif /* CONFIG_SWITCH */
}

void acc_detach_check(struct work_struct *wk)
{
	struct delayed_work *delay_work =
		container_of(wk, struct delayed_work, work);
	struct s2mm005_data *usbpd_data =
		container_of(delay_work, struct s2mm005_data, acc_detach_work);
	pr_info("%s: usbpd_data->pd_state : %d\n", __func__, usbpd_data->pd_state);
	if (usbpd_data->pd_state == State_PE_Initial_detach) {
		ccic_send_dock_intent(CCIC_DOCK_DETACHED);
		usbpd_data->acc_type = CCIC_DOCK_DETACHED;
	}
}

static int process_check_accessory(void * data)
{
	struct s2mm005_data *usbpd_data = data;

	// detect Gear VR
	if (usbpd_data->Vendor_ID == 0x04E8 && usbpd_data->Product_ID >= 0xA500 && usbpd_data->Product_ID <= 0xA505) {
		pr_info("%s : Samsung Gear VR connected.\n", __func__);
		if (usbpd_data->acc_type == CCIC_DOCK_DETACHED) {
			ccic_send_dock_intent(CCIC_DOCK_HMT);
			usbpd_data->acc_type = CCIC_DOCK_HMT;
		}
		return 1;
	}
	return 0;
}

static void process_discover_identity(void * data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_RX_DIS_ID;
	uint8_t ReadMSG[32] = {0,};
	int ret = 0;
	
	// Message Type Definition
	U_DATA_MSG_ID_HEADER_Type     *DATA_MSG_ID = (U_DATA_MSG_ID_HEADER_Type *)&ReadMSG[8];
	U_PRODUCT_VDO_Type			  *DATA_MSG_PRODUCT = (U_PRODUCT_VDO_Type *)&ReadMSG[16];
	
	ret = s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s has i2c error.\n", __func__);
		return;
	}

	usbpd_data->Vendor_ID = DATA_MSG_ID->BITS.USB_Vendor_ID;
	usbpd_data->Product_ID = DATA_MSG_PRODUCT->BITS.Product_ID;

	dev_info(&i2c->dev, "%s Vendor_ID : 0x%X, Product_ID : 0x%X\n", __func__, usbpd_data->Vendor_ID, usbpd_data->Product_ID);
	if (process_check_accessory(usbpd_data))
		dev_info(&i2c->dev, "%s : Samsung Accessory Connected.\n", __func__);
}

static void process_discover_svids(void * data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_RX_DIS_SVID;
	uint8_t ReadMSG[32] = {0,};
	int ret = 0;

	// Message Type Definition
	U_VDO1_Type 				  *DATA_MSG_VDO1 = (U_VDO1_Type *)&ReadMSG[8];

	ret = s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s has i2c error.\n", __func__);
		return;
	}

	usbpd_data->SVID_0 = DATA_MSG_VDO1->BITS.SVID_0;
	usbpd_data->SVID_1 = DATA_MSG_VDO1->BITS.SVID_1;

	dev_info(&i2c->dev, "%s SVID_0 : 0x%X, SVID_1 : 0x%X\n", __func__, usbpd_data->SVID_0, usbpd_data->SVID_1);
}

static void process_discover_modes(void * data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_RX_MODE;
	uint8_t ReadMSG[32] = {0,};
	int ret = 0;

	ret = s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s has i2c error.\n", __func__);
		return;
	}

	dev_info(&i2c->dev, "%s\n", __func__);
}

static void process_enter_mode(void * data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_RX_ENTER_MODE;
	uint8_t ReadMSG[32] = {0,};
	int ret = 0;

	// Message Type Definition
	U_DATA_MSG_VDM_HEADER_Type	  *DATA_MSG_VDM = (U_DATA_MSG_VDM_HEADER_Type *)&ReadMSG[4];
	
	ret = s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s has i2c error.\n", __func__);
		return;
	}
	if (DATA_MSG_VDM->BITS.VDM_command_type == 1)
		dev_info(&i2c->dev, "%s : EnterMode ACK.\n", __func__);
	else
		dev_info(&i2c->dev, "%s : EnterMode NAK.\n", __func__);
}

static void process_alternate_mode(void * data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint32_t mode = usbpd_data->alternate_state;

	if (mode) {
		dev_info(&i2c->dev, "%s, mode : 0x%x\n", __func__, mode);

		if (mode & VDM_DISCOVER_ID)
			process_discover_identity(usbpd_data);
		if (mode & VDM_DISCOVER_SVIDS)
			process_discover_svids(usbpd_data);
		if (mode & VDM_DISCOVER_MODES)
			process_discover_modes(usbpd_data);
		if (mode & VDM_ENTER_MODE)
			process_enter_mode(usbpd_data);
		usbpd_data->alternate_state = 0;
	}
}

void send_alternate_message(void * data, int cmd)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_VDM_MSG_REQ;
	u8 mode = (u8)cmd;
	dev_info(&i2c->dev, "%s : %s\n",__func__, &VDM_MSG_IRQ_State_Print[cmd][0]);
	s2mm005_write_byte(i2c, REG_ADD, &mode, 1);
}

void receive_alternate_message(void * data, VDM_MSG_IRQ_STATUS_Type *VDM_MSG_IRQ_State)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;

	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Discover_ID) {
		dev_info(&i2c->dev, "%s : %s\n",__func__, &VDM_MSG_IRQ_State_Print[1][0]);
		usbpd_data->alternate_state |= VDM_DISCOVER_ID;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Discover_SVIDs) {
		dev_info(&i2c->dev, "%s : %s\n",__func__, &VDM_MSG_IRQ_State_Print[2][0]);
		usbpd_data->alternate_state |= VDM_DISCOVER_SVIDS;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Discover_MODEs) {
		dev_info(&i2c->dev, "%s : %s\n",__func__, &VDM_MSG_IRQ_State_Print[3][0]);
		usbpd_data->alternate_state |= VDM_DISCOVER_MODES;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Enter_Mode) {
		dev_info(&i2c->dev, "%s : %s\n",__func__, &VDM_MSG_IRQ_State_Print[4][0]);
		usbpd_data->alternate_state |= VDM_ENTER_MODE;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Exit_Mode) {
		dev_info(&i2c->dev, "%s : %s\n",__func__, &VDM_MSG_IRQ_State_Print[5][0]);
		usbpd_data->alternate_state |= VDM_EXIT_MODE;
	}
	process_alternate_mode(usbpd_data);
}
#endif
