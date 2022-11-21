/*
 * Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ufshcd.h"
#include "ufs_quirks.h"

#define SERIAL_NUM_SIZE 7
#define TOSHIBA_SERIAL_NUM_SIZE 10

static struct ufs_card_fix ufs_fixups[] = {
	/* UFS cards deviations table */
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL, UFS_DEVICE_NO_VCCQ),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_NO_FASTAUTO),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G9C8KBADG",
		UFS_DEVICE_QUIRK_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G9D8KBADG",
		UFS_DEVICE_QUIRK_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGAF8G9T43BAIR",
		UFS_DEVICE_QUIRK_SUPPORT_QUERY_FATAL_MODE),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_HYNIX, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_SAVECONFIGTIME),
	UFS_FIX(UFS_VENDOR_HYNIX, "hB8aL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_HYNIX, "hC8aL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_HYNIX, "hD8aL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_HYNIX, "hC8aM1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_HYNIX, "h08aM1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_HYNIX, "hC8GL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_HYNIX, "hC8HL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),

	END_FIX
};

/*
 * UN policy
 * 20 digits : manid + mandate + serial number
 * 		(sec, hynix : 7byte hex, toshiba : 6byte + 00, ascii)
 */
void ufs_set_sec_unique_number(struct ufs_hba *hba, u8 *str_desc_buf, u8 *desc_buf)
{
	u8 manid;
	u8 snum_buf[UFS_UN_MAX_DIGITS];

	manid = hba->manufacturer_id & 0xFF;
	memset(hba->unique_number, 0, sizeof(hba->unique_number));
	memset(snum_buf, 0, sizeof(snum_buf));

#if defined(CONFIG_UFS_UN_20DIGITS)
	memcpy(snum_buf, str_desc_buf + QUERY_DESC_HDR_SIZE, SERIAL_NUM_SIZE);

	sprintf(hba->unique_number, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		manid,
		desc_buf[DEVICE_DESC_PARAM_MANF_DATE], desc_buf[DEVICE_DESC_PARAM_MANF_DATE+1],
		snum_buf[0], snum_buf[1], snum_buf[2], snum_buf[3], snum_buf[4], snum_buf[5], snum_buf[6]);

	/* Null terminate the unique number string */
	hba->unique_number[UFS_UN_20_DIGITS] = '\0';
#endif
	hba->manufacturer_date =
		desc_buf[DEVICE_DESC_PARAM_MANF_DATE] << 8 | desc_buf[DEVICE_DESC_PARAM_MANF_DATE+1];
}

static int ufs_get_device_info(struct ufs_hba *hba,
				struct ufs_card_info *card_data)
{
	int err;
	u8 model_index;
	u8 serial_num_index;
	u8 str_desc_buf[QUERY_DESC_STRING_MAX_SIZE + 1];
	u8 desc_buf[QUERY_DESC_DEVICE_MAX_SIZE];
	u8 health_buf[QUERY_DESC_HEALTH_MAX_SIZE];
	bool ascii_type;

	err = ufshcd_read_device_desc(hba, desc_buf,
					QUERY_DESC_DEVICE_MAX_SIZE);
	if (err)
		goto out;

	/*
	 * getting vendor (manufacturerID) and Bank Index in big endian
	 * format
	 */
	card_data->wmanufacturerid = desc_buf[DEVICE_DESC_PARAM_MANF_ID] << 8 |
				     desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];

	hba->manufacturer_id = card_data->wmanufacturerid;

	err = ufshcd_read_health_desc(hba, health_buf,
			QUERY_DESC_HEALTH_MAX_SIZE);
	if (err)
		printk("%s: DEVICE_HEALTH desc read fail, err  = %d\n", __FUNCTION__, err);

	/* getting Life Time at Device Health DESC*/
	card_data->lifetime = health_buf[HEALTH_DEVICE_DESC_PARAM_LIFETIMEA];

	hba->lifetime = card_data->lifetime;


	model_index = desc_buf[DEVICE_DESC_PARAM_PRDCT_NAME];

	memset(str_desc_buf, 0, QUERY_DESC_STRING_MAX_SIZE);
	err = ufshcd_read_string_desc(hba, model_index, str_desc_buf,
			QUERY_DESC_STRING_MAX_SIZE, ASCII_STD);
	if (err)
		goto out;

	str_desc_buf[QUERY_DESC_STRING_MAX_SIZE] = '\0';
	strlcpy(card_data->model, (str_desc_buf + QUERY_DESC_HDR_SIZE),
			min_t(u8, str_desc_buf[QUERY_DESC_LENGTH_OFFSET],
				MAX_MODEL_LEN));
	/* Null terminate the model string */
	card_data->model[MAX_MODEL_LEN] = '\0';

	serial_num_index = desc_buf[DEVICE_DESC_PARAM_SN];
	memset(str_desc_buf, 0, QUERY_DESC_STRING_MAX_SIZE);

	/*spec is unicode but sec use hex data*/
	ascii_type = UTF16_STD;

	err = ufshcd_read_string_desc(hba, serial_num_index, str_desc_buf,
			QUERY_DESC_STRING_MAX_SIZE, ascii_type);

	if (err)
		goto out;
	str_desc_buf[QUERY_DESC_STRING_MAX_SIZE] = '\0';

	ufs_set_sec_unique_number(hba, str_desc_buf, desc_buf);

out:
	return err;
}

void ufs_advertise_fixup_device(struct ufs_hba *hba)
{
	int err;
	struct ufs_card_fix *f;
	struct ufs_card_info card_data;

	card_data.wmanufacturerid = 0;
	card_data.model = kmalloc(MAX_MODEL_LEN + 1, GFP_KERNEL);
	if (!card_data.model)
		goto out;

	/* get device data*/
	err = ufs_get_device_info(hba, &card_data);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting device info\n", __func__);
		goto out;
	}

	for (f = ufs_fixups; f->quirk; f++) {
		/* if same wmanufacturerid */
		if (((f->card.wmanufacturerid == card_data.wmanufacturerid) ||
		     (f->card.wmanufacturerid == UFS_ANY_VENDOR)) &&
		    /* and same model */
		    (STR_PRFX_EQUAL(f->card.model, card_data.model) ||
		     !strcmp(f->card.model, UFS_ANY_MODEL)))
			/* update quirks */
			hba->dev_quirks |= f->quirk;
	}
out:
	kfree(card_data.model);
}
