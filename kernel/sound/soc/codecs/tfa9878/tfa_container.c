/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "inc/tfa_container.h"
#include "inc/tfa.h"
#include "inc/tfa98xx_tfafieldnames.h"
#include "inc/tfa_internal.h"
#include "inc/config.h"

/* defines */
#define MODULE_BIQUADFILTERBANK 2
#define BIQUAD_COEFF_SIZE       6

#define TSEL_OFFSET	(1 * 3)
#define TEMP_OFFSET	((1 + 2) * 3)
static void tfa_overwrite_temp(struct tfa_device *tfa, char *data_buf);

/* module globals */
static uint8_t gresp_address; /* in case of setting with option */

/*
 * check the container file
 */
enum tfa_error tfa_load_cnt(void *cnt, int length)
{
	struct tfa_container *cntbuf = (struct tfa_container *)cnt;

	if (length > TFA_MAX_CNT_LENGTH) {
		pr_err("incorrect length\n");
		return tfa_error_container;
	}

	if (HDR(cntbuf->id[0], cntbuf->id[1]) == 0) {
		pr_err("header is 0\n");
		return tfa_error_container;
	}

	if ((HDR(cntbuf->id[0], cntbuf->id[1])) != params_hdr) {
		pr_err("wrong header type: 0x%02x 0x%02x\n",
			cntbuf->id[0], cntbuf->id[1]);
		return tfa_error_container;
	}

	if (cntbuf->size == 0) {
		pr_err("data size is 0\n");
		return tfa_error_container;
	}

	/* check CRC */
	if (tfa_cont_crc_check_container(cntbuf)) {
		pr_err("CRC error\n");
		return tfa_error_container;
	}

	/* check sub version level */
	if ((cntbuf->subversion[1] != TFA_PM_SUBVERSION)
		&& (cntbuf->subversion[0] != '0')) {
		pr_err("container sub-version not supported: %c%c\n",
			cntbuf->subversion[0], cntbuf->subversion[1]);
		return tfa_error_container;
	}

	return tfa_error_ok;
}

/*
 * Dump the contents of the file header
 */
void tfa_cont_show_header(struct tfa_header *hdr)
{
	char _id[2];

	pr_debug("File header\n");

	_id[1] = hdr->id >> 8;
	_id[0] = hdr->id & 0xff;
	pr_debug("\tid:%.2s version:%.2s subversion:%.2s\n", _id,
		hdr->version, hdr->subversion);
	pr_debug("\tsize:%d CRC:0x%08x\n", hdr->size, hdr->crc);
	pr_debug("\tcustomer:%.8s application:%.8s type:%.8s\n",
		hdr->customer, hdr->application, hdr->type);
}

/*
 * return device list dsc from index
 */
struct tfa_device_list *tfa_cont_get_dev_list
(struct tfa_container *cont, int dev_idx)
{
	uint8_t *base = (uint8_t *)cont;

	if (cont == NULL)
		return NULL;

	if ((dev_idx < 0) || (dev_idx >= cont->ndev))
		return NULL;

	if (cont->index[dev_idx].type != dsc_device)
		return NULL;

	base += cont->index[dev_idx].offset;
	return (struct tfa_device_list *)base;
}

/*
 * get the Nth profile for the Nth device
 */
struct tfa_profile_list *tfa_cont_get_dev_prof_list
(struct tfa_container *cont, int dev_idx, int prof_idx)
{
	struct tfa_device_list *dev;
	int idx, hit;
	uint8_t *base = (uint8_t *)cont;

	dev = tfa_cont_get_dev_list(cont, dev_idx);
	if (dev) {
		for (idx = 0, hit = 0; idx < dev->length; idx++) {
			if (dev->list[idx].type == dsc_profile) {
				if (prof_idx == hit++)
					return (struct tfa_profile_list *)
						(dev->list[idx].offset + base);
			}
		}
	}

	return NULL;
}

/*
 * get the number of profiles for the Nth device
 */
int tfa_cnt_get_dev_nprof(struct tfa_device *tfa)
{
	struct tfa_device_list *dev;
	int idx, nprof = 0;

	if (tfa->cnt == NULL)
		return 0;

	if ((tfa->dev_idx < 0) || (tfa->dev_idx >= tfa->cnt->ndev))
		return 0;

	dev = tfa_cont_get_dev_list(tfa->cnt, tfa->dev_idx);
	if (dev) {
		for (idx = 0; idx < dev->length; idx++) {
			if (dev->list[idx].type == dsc_profile)
				nprof++;
		}
	}

	return nprof;
}

/*
 * get the Nth lifedata for the Nth device
 */
struct tfa_livedata_list *tfa_cont_get_dev_livedata_list
(struct tfa_container *cont, int dev_idx, int lifedata_idx)
{
	struct tfa_device_list *dev;
	int idx, hit;
	uint8_t *base = (uint8_t *)cont;

	dev = tfa_cont_get_dev_list(cont, dev_idx);
	if (dev) {
		for (idx = 0, hit = 0; idx < dev->length; idx++) {
			if (dev->list[idx].type == dsc_livedata) {
				if (lifedata_idx == hit++)
					return (struct tfa_livedata_list *)
						(dev->list[idx].offset + base);
			}
		}
	}

	return NULL;
}

/*
 * Get the max volume step associated with Nth profile for the Nth device
 */
int tfa_cont_get_max_vstep(struct tfa_device *tfa, int prof_idx)
{
	struct tfa_volume_step_max2_file *vp3;
	int vstep_count = 0;

	vp3 = (struct tfa_volume_step_max2_file *)
		tfa_cont_get_file_data(tfa, prof_idx, volstep_hdr);
	if (vp3)
		vstep_count = vp3->nr_of_vsteps;

	return vstep_count;
}

/*
 * Get the file contents associated with the device or profile
 * Search within the device tree, if not found, search within the profile
 * tree. There can only be one type of file within profile or device.
 */
struct tfa_file_dsc *tfa_cont_get_file_data(struct tfa_device *tfa,
	int prof_idx, enum tfa_header_type type)
{
	struct tfa_device_list *dev;
	struct tfa_profile_list *prof;
	struct tfa_file_dsc *file;
	struct tfa_header *hdr;
	unsigned int i;

	if (tfa->cnt == NULL) {
		pr_err("invalid pointer to container file\n");
		return NULL;
	}

	dev = tfa_cont_get_dev_list(tfa->cnt, tfa->dev_idx);
	if (dev == NULL) {
		pr_err("invalid pointer to container file device list\n");
		return NULL;
	}

	/* process the device list until a file type is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_file) {
			file = (struct tfa_file_dsc *)
				(dev->list[i].offset + (uint8_t *)tfa->cnt);
			if (file != NULL) {
				hdr = (struct tfa_header *)file->data;
				/* check for file type */
				if (hdr->id == type) {
					/* pr_debug("%s: file found of type "
					 *  "%d in device %s\n",
					 *  __func__, type,
					 * tfa_cont_device_name(tfa->cnt,
					 * tfa->dev_idx));
					 */
					return (struct tfa_file_dsc *)
						&file->data;
				}
			}
		}
	}

	/* File not found in device tree.
	 * So, look in the profile list until the file type is encountered
	 */
	prof = tfa_cont_get_dev_prof_list(tfa->cnt, tfa->dev_idx, prof_idx);
	if (prof == NULL) {
		pr_err("invalid pointer to container file profile list\n");
		return NULL;
	}

	for (i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dsc_file) {
			file = (struct tfa_file_dsc *)
				(prof->list[i].offset + (uint8_t *)tfa->cnt);
			if (file != NULL) {
				hdr = (struct tfa_header *)file->data;
				if (hdr != NULL) {
					/* check for file type */
					if (hdr->id == type) {
						/* pr_debug("%s: file found of "
						 *  "type %d in profile %s\n",
						 *  __func__, type,
						 * tfa_cont_profile_name
						 *  (tfa->cnt, tfa->dev_idx,
						 *  prof_idx));
						 */
						return (struct tfa_file_dsc *)
							&file->data;
					}
				}
			}
		}
	}

	if (tfa->verbose)
		pr_debug("%s: no file found of type %d\n", __func__, type);

	return NULL;
}

enum tfa98xx_error tfa_cont_fw_api_check(struct tfa_device *tfa,
	char *hdrstr)
{
	int i;

	pr_info("%s: Expected FW API ver: %d.%d.%d.%d, Msg File ver: %d.%d.%d.%d\n",
		__func__,
		tfa->fw_itf_ver[0], tfa->fw_itf_ver[1],
		tfa->fw_itf_ver[2], tfa->fw_itf_ver[3],
		hdrstr[4], hdrstr[5], hdrstr[6], hdrstr[7]);

	for (i = 0; i < 4; i++) {
		/* SB5.0 and greater */
		if (tfa->fw_itf_ver[1] >= TFA_API_SBFW_10_00_00_SMALL_M)
			if (i == 2) /* skipped the 3rd field, update field */
				continue;
		if (tfa->fw_itf_ver[i] != hdrstr[i + 4])
			/* +4 to skip "APIV" in msg file */
			return TFA98XX_ERROR_BAD_PARAMETER;
	}

	return TFA98XX_ERROR_OK;
}

static int tfa_cont_is_config_loaded(struct tfa_device *tfa)
{
	if (tfa->ext_dsp != 1)
		return 0; /* unrelated */

	/* check if config is loaded at the first device:
	 * to write files only once
	 */

	if (tfa_count_status_flag(tfa, TFA_SET_DEVICE) > 1
		|| tfa_count_status_flag(tfa, TFA_SET_CONFIG) > 0) {
		pr_debug("%s: skip secondary device (%d)\n",
			__func__, tfa->dev_idx);
		return 1;
	}

	return 0;
}

static void tfa_overwrite_temp(struct tfa_device *tfa, char *data_buf)
{
	int channel, temp_index = TEMP_OFFSET;

	if ((data_buf[1] != (0x80 | MODULE_FRAMEWORK))
		|| data_buf[2] != FW_PAR_ID_SET_CHIP_TEMP_SELECTOR
		|| tfa->temp == 0xffff)
		return;

	pr_info("%s: temp_select - %s\n", __func__,
		(data_buf[TSEL_OFFSET + 2]) ? "external" : "internal");
	if (data_buf[TSEL_OFFSET + 2] == 0)
		return;

	/* write temp stored in driver: SetChipTempSelect */
	/* set index by skipping command and two parameters */
	pr_info("%s: check temp in msg 0x%02x%02x%02x, @ 0x%02x\n",
		__func__, data_buf[TEMP_OFFSET],
		data_buf[TEMP_OFFSET + 1],
		data_buf[TEMP_OFFSET + 2],
		temp_index);

	for (channel = 0; channel < MAX_CHANNELS; channel++) {
		temp_index = TEMP_OFFSET + channel * 3;
		data_buf[temp_index]
			= (char)((tfa->temp & 0xff0000) >> 16);
		data_buf[temp_index + 1]
			= (char)((tfa->temp & 0x00ff00) >> 8);
		data_buf[temp_index + 2]
			= (char)(tfa->temp & 0x0000ff);
	}

	pr_info("%s: set temp from driver 0x%02x%02x%02x\n",
		__func__, data_buf[TEMP_OFFSET],
		data_buf[TEMP_OFFSET + 1],
		data_buf[TEMP_OFFSET + 2]);
}

/*
 * write a parameter file to the device
 * The VstepIndex and VstepMsgIndex are only used to write
 * a specific msg from the vstep file.
 */
enum tfa98xx_error tfa_cont_write_file(struct tfa_device *tfa,
	struct tfa_file_dsc *file, int vstep_idx, int vstep_msg_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_header *hdr = (struct tfa_header *)file->data;
	enum tfa_header_type type;
	int size;
	char sub_ver_string[8] = {0};
	uint16_t subversion = 0;
	int kerr;
	char *data_buf;
	struct tfa_device *ntfa;
	int i;

	if (tfa_cont_is_config_loaded(tfa))
		return err;

	if (tfa->verbose)
		tfa_cont_show_header(hdr);

	type = (enum tfa_header_type)hdr->id;
	if ((type == msg_hdr)
		|| ((type == volstep_hdr) && (tfa->tfa_family == 2))) {
		sub_ver_string[0] = hdr->subversion[0];
		sub_ver_string[1] = hdr->subversion[1];
		sub_ver_string[2] = '\0';

		kerr = kstrtou16(sub_ver_string, 16, &subversion);
		if (kerr < 0)
			pr_err("%s: error in readaing subversion\n", __func__);

		if ((subversion > 0)
			&& (((hdr->customer[0]) == 'A')
			&& ((hdr->customer[1]) == 'P')
			&& ((hdr->customer[2]) == 'I')
			&& ((hdr->customer[3]) == 'V'))) {
			pr_debug("%s: msg subversion 0x%x, custom v%d.%d.%d.%d\n",
				__func__, subversion,
				hdr->customer[4],
				hdr->customer[5],
				hdr->customer[6],
				hdr->customer[7]);

			if (tfa->fw_itf_ver[0] == 0xff) {
				err = tfa_get_fw_api_version(tfa,
					(unsigned char *)&tfa->fw_itf_ver[0]);
				if (err) {
					pr_debug("[%s] cannot get FW API ver, error = %d\n",
						__func__, err);
					err = TFA98XX_ERROR_OK;
				} else {
					for (i = 0; i < tfa->dev_count; i++) {
						ntfa = tfa98xx_get_tfa_device_from_index(i);

						if (ntfa == NULL)
							continue;
						if (ntfa->dev_idx == tfa->dev_idx)
							continue;

						memcpy(&ntfa->fw_itf_ver[0],
							&tfa->fw_itf_ver[0], 4);
					}
				}
			} else {
				pr_debug("%s: checked - itf v%d.%d.%d.%d\n",
					__func__,
					tfa->fw_itf_ver[0],
					tfa->fw_itf_ver[1],
					tfa->fw_itf_ver[2],
					tfa->fw_itf_ver[3]);
			}
		}
	}

	switch (type) {
	case msg_hdr: /* generic DSP message */
		size = hdr->size - sizeof(struct tfa_msg_file);
		data_buf = (char *)((struct tfa_msg_file *)hdr)->data;

		tfa_overwrite_temp(tfa, data_buf);

		err = dsp_msg(tfa, size,
			(const char *)((struct tfa_msg_file *)hdr)->data);

		/* Reset bypass if writing msg files */
		if (err == TFA98XX_ERROR_OK)
			tfa->is_bypass = 0;
		break;
	case speaker_hdr:
		if (tfa->tfa_family == 2) {
			/* Remove header and xml_id */
			size = hdr->size - sizeof(struct tfa_spk_header)
				- sizeof(struct tfa_fw_ver);

			err = dsp_msg(tfa, size,
				(const char *)
				(((struct tfa_speaker_file *)hdr)->data
				+ (sizeof(struct tfa_fw_ver))));
		}
		break;
	case equalizer_hdr:
		err = tfa_cont_write_filterbank(tfa,
			((struct tfa_equalizer_file *)hdr)->filter);
		break;
	case patch_hdr:
		size = hdr->size - sizeof(struct tfa_patch_file);
		/* total length */
		err = tfa_dsp_patch(tfa, size,
			(const unsigned char *)
			((struct tfa_patch_file *)hdr)->data);
		break;
	case info_hdr:
		/* Ignore */
		break;
	default:
		pr_err("Header is of unknown type: 0x%x\n", type);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	return err;
}

/*
 * get the 1st of this dsc type this devicelist
 */
static struct tfa_desc_ptr *tfa_cnt_get_dsc
(struct tfa_container *cnt, enum tfa_descriptor_type type, int dev_idx)
{
	struct tfa_device_list *dev = tfa_cont_device(cnt, dev_idx);
	struct tfa_desc_ptr *_this;
	int i;

	if (!dev)
		return NULL;

	/* process the list until a the type is encountered */
	for (i = 0; i < dev->length; i++)
		if (dev->list[i].type == (uint32_t)type) {
			_this = (struct tfa_desc_ptr *)
				(dev->list[i].offset + (uint8_t *)cnt);
			return _this;
		}

	return NULL;
}

/*
 * get the device type from the patch in this devicelist
 * - find the patch file for this devidx
 * - return the devid from the patch or 0 if not found
 */
int tfa_cont_get_devid(struct tfa_container *cnt, int dev_idx)
{
	struct tfa_patch_file *patchfile;
	struct tfa_desc_ptr *patchdsc;
	uint8_t *patchheader;
	unsigned short devid, checkaddress;
	int checkvalue;

	/* fall through to try the patch */
	patchdsc = tfa_cnt_get_dsc(cnt, dsc_patch, dev_idx);
	if (!patchdsc) /* no patch for this device, assume non-i2c */
		return 0;

	patchdsc += 2; /* first the filename dsc and filesize, so skip them */
	patchfile = (struct tfa_patch_file *)patchdsc;

	patchheader = patchfile->data;

	checkaddress = (patchheader[1] << 8) + patchheader[2];
	checkvalue = (patchheader[3] << 16)
		+ (patchheader[4] << 8) + patchheader[5];

	devid = patchheader[0];

	if (checkaddress == 0xFFFF
		&& checkvalue != 0xFFFFFF && checkvalue != 0)
		devid = patchheader[5] << 8
			| patchheader[0]; /* full revid */

	return devid;
}

/*
 * get the firmware version from the patch in this devicelist
 */
int tfa_cnt_get_patch_version(struct tfa_device *tfa)
{
	struct tfa_patch_file *patchfile;
	struct tfa_desc_ptr *patchdsc;
	uint8_t *data;
	int size, version;

	if (tfa->cnt == NULL)
		return TFA_ERROR;

	patchdsc = tfa_cnt_get_dsc(tfa->cnt, dsc_patch, tfa->dev_idx);
	patchdsc += 2; /* first the filename dsc and filesize, so skip them */
	patchfile = (struct tfa_patch_file *)patchdsc;

	size = patchfile->hdr.size - sizeof(struct tfa_patch_file);
	data = patchfile->data;

	version = (data[size - 3] << 16)
		+ (data[size - 2] << 8) + data[size - 1];

	return version;
}

/*
 * get the responder for the device if it exists
 */
enum tfa98xx_error tfa_cont_get_resp(struct tfa_device *tfa,
	uint8_t *resp_addr)
{
	struct tfa_device_list *dev = NULL;

	/* Make sure the cnt file is loaded */
	if (tfa->cnt != NULL)
		dev = tfa_cont_device(tfa->cnt, tfa->dev_idx);

	if (dev == NULL) {
		/* Check if responder argument is used! */
		if (gresp_address == 0)
			return TFA98XX_ERROR_BAD_PARAMETER;

		*resp_addr = gresp_address;

		return TFA98XX_ERROR_OK;
	}

	*resp_addr = dev->dev;

	return TFA98XX_ERROR_OK;
}

/* without cnt, we can always have used the responder argument */
void tfa_cont_set_resp(uint8_t resp_addr)
{
	gresp_address = resp_addr;
}

/*
 * lookup responder and return device index
 */
int tfa_cont_get_idx(struct tfa_device *tfa)
{
	struct tfa_device_list *dev = NULL;
	int i;

	for (i = 0; i < tfa->cnt->ndev; i++) {
		dev = tfa_cont_device(tfa->cnt, i);
		if (dev->dev == tfa->resp_address)
			break;
	}

	if (i == tfa->cnt->ndev)
		return TFA_ERROR;

	return i;
}

int tfa_cont_get_idx_tfadsp(struct tfa_device *tfa, int value)
{
	struct tfa_device_list *dev = NULL;
	int i;

	for (i = 0; i < tfa->cnt->ndev; i++) {
		dev = tfa_cont_device(tfa->cnt, i);
		if (dev->dev == value)
			return i;
	}

	return TFA_NOT_FOUND;
}

/*
 * write a bit field
 */
enum tfa98xx_error tfa_run_write_bitfield(struct tfa_device *tfa,
	struct tfa_bitfield bf)
{
	enum tfa98xx_error error;
	uint16_t value;
	union {
		uint16_t field;
		struct tfa_bf_enum bf_enum;
	} bf_uni;

	value = bf.value;
	bf_uni.field = bf.field;
	error = tfa->dev_ops.set_bitfield(tfa, bf_uni.field, value);

	return error;
}

/*
 * read a bit field
 */
enum tfa98xx_error tfa_run_read_bitfield(struct tfa_device *tfa,
	struct tfa_bitfield *bf)
{
	enum tfa98xx_error error;
	union {
		uint16_t field;
		struct tfa_bf_enum bf_enum;
	} bf_uni;
	uint16_t regvalue, msk;

	bf_uni.field = bf->field;

	error = reg_read(tfa,
		(unsigned char)(bf_uni.bf_enum.address), &regvalue);
	if (error)
		return error;

	msk = ((1 << (bf_uni.bf_enum.len + 1)) - 1) << bf_uni.bf_enum.pos;

	regvalue &= msk;
	bf->value = regvalue >> bf_uni.bf_enum.pos;

	return error;
}

/*
 * write the register based on the input address, value and mask
 * only the part that is masked will be updated
 */
static enum tfa98xx_error tfa_run_write_register
(struct tfa_device *tfa, struct tfa_reg_patch *reg)
{
	enum tfa98xx_error error;
	uint16_t value, newvalue;

	if (tfa->verbose)
		pr_debug("register: 0x%02x=0x%04x (msk=0x%04x)\n",
			reg->address, reg->value, reg->mask);

	error = reg_read(tfa, reg->address, &value);
	if (error)
		return error;

	value &= ~reg->mask;
	newvalue = reg->value & reg->mask;

	value |= newvalue;
	error = reg_write(tfa, reg->address, value);

	return error;
}

/* write reg and bitfield items in the devicelist to the target */
enum tfa98xx_error tfa_cont_write_regs_dev(struct tfa_device *tfa)
{
	struct tfa_device_list *dev
		= tfa_cont_device(tfa->cnt, tfa->dev_idx);
	struct tfa_bitfield *bit_f;
	int i;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	if (!dev)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* process the list until a patch, file of profile is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_patch
			|| dev->list[i].type == dsc_file
			|| dev->list[i].type == dsc_profile)
			break;

		if (dev->list[i].type == dsc_bit_field) {
			bit_f = (struct tfa_bitfield *)
				(dev->list[i].offset + (uint8_t *)tfa->cnt);
			err = tfa_run_write_bitfield(tfa, *bit_f);
		}
		if (dev->list[i].type == dsc_register)
			err = tfa_run_write_register(tfa,
				(struct tfa_reg_patch *)
				(dev->list[i].offset + (char *)tfa->cnt));

		if (err)
			break;
	}

	return err;
}

/* write reg and bitfield items in the profilelist the target */
enum tfa98xx_error tfa_cont_write_regs_prof(struct tfa_device *tfa,
	int prof_idx)
{
	struct tfa_profile_list *prof
		= tfa_cont_get_dev_prof_list(tfa->cnt, tfa->dev_idx, prof_idx);
	struct tfa_bitfield *bitf;
	unsigned int i;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	if (!prof)
		return TFA98XX_ERROR_BAD_PARAMETER;

	if (tfa->verbose)
		pr_debug("----- profile: %s (%d) -----\n",
			tfa_cont_get_string(tfa->cnt, &prof->name), prof_idx);

	/* process the list
	 * until the end of the profile or the default section
	 */
	for (i = 0; i < prof->length; i++) {
		/* only to write the values before the default section
		 * when we switch profile
		 */
		if (prof->list[i].type == dsc_default)
			break;

		if (prof->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(prof->list[i].offset + (uint8_t *)tfa->cnt);
			err = tfa_run_write_bitfield(tfa, *bitf);
		}
		if (prof->list[i].type == dsc_register)
			err = tfa_run_write_register(tfa,
				(struct tfa_reg_patch *)(prof->list[i].offset
				+ (char *)tfa->cnt));
		if (err)
			break;
	}
	return err;
}

/*
 * Create a buffer which can be used to send to the dsp.
 */
static void create_dsp_buffer_msg(struct tfa_device *tfa,
	struct tfa_msg *msg, char *buffer, int *size)
{
	int i, nr = 0;

	(void)tfa;

	/* Copy cmd_id. Remember that the cmd_id is reversed */
	buffer[nr++] = msg->cmd_id[2];
	buffer[nr++] = msg->cmd_id[1];
	buffer[nr++] = msg->cmd_id[0];

	/* Copy the data to the buffer */
	for (i = 0; i < msg->msg_size; i++) {
		buffer[nr++] = (uint8_t)((msg->data[i] >> 16) & 0xffff);
		buffer[nr++] = (uint8_t)((msg->data[i] >> 8) & 0xff);
		buffer[nr++] = (uint8_t)(msg->data[i] & 0xff);
	}

	*size = nr;
}

/* write all param files in the devicelist to the target */
enum tfa98xx_error tfa_cont_write_files(struct tfa_device *tfa)
{
	struct tfa_device_list *dev = NULL;
	int dev_idx_files = 0;
	struct tfa_file_dsc *file;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	char buffer[(MEMTRACK_MAX_WORDS * 3) + 3] = {0};
	/* every word requires 3 bytes, and 3 is the msg */
	int i, size = 0;

	dev_idx_files = (tfa->dev_tfadsp == -1)
		? tfa->dev_idx : tfa->dev_tfadsp;
	dev = tfa_cont_device(tfa->cnt, dev_idx_files);

	if (!dev)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* process the list and write all files */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_file) {
			file = (struct tfa_file_dsc *)
				(dev->list[i].offset + (uint8_t *)tfa->cnt);
			if (tfa_cont_write_file
				(tfa, file, 0, TFA_MAX_VSTEP_MSG_MARKER))
				return TFA98XX_ERROR_BAD_PARAMETER;
		}

		if (dev->list[i].type == dsc_set_input_select
			|| dev->list[i].type == dsc_set_output_select
			|| dev->list[i].type == dsc_set_program_config
			|| dev->list[i].type == dsc_set_lag_w
			|| dev->list[i].type == dsc_set_gains
			|| dev->list[i].type == dsc_set_vbat_factors
			|| dev->list[i].type == dsc_set_senses_cal
			|| dev->list[i].type == dsc_set_senses_delay
			|| dev->list[i].type == dsc_set_mb_drc
			|| dev->list[i].type == dsc_set_fw_use_case
			|| dev->list[i].type == dsc_set_vddp_config) {
			if (tfa_cont_is_config_loaded(tfa))
				continue;

			create_dsp_buffer_msg(tfa, (struct tfa_msg *)
				(dev->list[i].offset + (char *)tfa->cnt),
				buffer, &size);

			if (tfa->verbose) {
				pr_debug("command: %s=0x%02x%02x%02x\n",
					tfa_cont_get_command_string
					(dev->list[i].type),
					(unsigned char)buffer[0],
					(unsigned char)buffer[1],
					(unsigned char)buffer[2]);
			}

			err = dsp_msg(tfa, size, buffer);
		}

		if (dev->list[i].type == dsc_cmd) {
			if (tfa_cont_is_config_loaded(tfa))
				continue;

			size = *(uint16_t *)
				(dev->list[i].offset + (char *)tfa->cnt);

			err = dsp_msg(tfa, size,
				dev->list[i].offset + 2 + (char *)tfa->cnt);

			if (tfa->verbose) {
				const char *cmd_id = dev->list[i].offset
					+ 2 + (char *)tfa->cnt;

				pr_debug("Writing cmd=0x%02x%02x%02x\n",
					(uint8_t)cmd_id[0],
					(uint8_t)cmd_id[1],
					(uint8_t)cmd_id[2]);
			}
		}
		if (err != TFA98XX_ERROR_OK)
			break;

		if (err != TFA98XX_ERROR_OK)
			break;
	}

	return err;
}

/*
 * write all param files in the profilelist to the target
 * this is used during startup when maybe ACS is set
 */
enum tfa98xx_error tfa_cont_write_files_prof(struct tfa_device *tfa,
	int prof_idx, int vstep_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_profile_list *prof = NULL;
	int dev_idx_files = 0;
	char buffer[(MEMTRACK_MAX_WORDS * 3) + 3] = {0};
	/* every word requires 3 bytes, and 3 is the msg */
	unsigned int i;
	struct tfa_file_dsc *file;
	struct tfa_patch_file *patchfile;
	int size;

	dev_idx_files = (tfa->dev_tfadsp == -1)
		? tfa->dev_idx : tfa->dev_tfadsp;
	prof = tfa_cont_get_dev_prof_list(tfa->cnt,
		dev_idx_files, prof_idx);

	if (!prof)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* process the list and write all files */
	for (i = 0; i < prof->length; i++) {
		switch (prof->list[i].type) {
		case dsc_file:
			file = (struct tfa_file_dsc *)
				(prof->list[i].offset + (uint8_t *)tfa->cnt);
			err = tfa_cont_write_file(tfa, file,
				vstep_idx, TFA_MAX_VSTEP_MSG_MARKER);
			break;
		case dsc_patch:
			file = (struct tfa_file_dsc *)
				(prof->list[i].offset + (uint8_t *)tfa->cnt);
			patchfile = (struct tfa_patch_file *)&file->data;
			if (tfa->verbose)
				tfa_cont_show_header(&patchfile->hdr);

			/* size is total length */
			size = patchfile->hdr.size
				- sizeof(struct tfa_patch_file);
			err = tfa_dsp_patch(tfa, size,
				(const unsigned char *)patchfile->data);
			break;
		case dsc_set_input_select:
		case dsc_set_output_select:
		case dsc_set_program_config:
		case dsc_set_lag_w:
		case dsc_set_gains:
		case dsc_set_vbat_factors:
		case dsc_set_senses_cal:
		case dsc_set_senses_delay:
		case dsc_set_mb_drc:
		case dsc_set_fw_use_case:
		case dsc_set_vddp_config:
			if (tfa_cont_is_config_loaded(tfa))
				break;

			create_dsp_buffer_msg(tfa,
				(struct tfa_msg *)(prof->list[i].offset
				+ (uint8_t *)tfa->cnt), buffer, &size);

			if (tfa->verbose)
				pr_debug("command: %s=0x%02x%02x%02x\n",
					tfa_cont_get_command_string
					(prof->list[i].type),
					(unsigned char)buffer[0],
					(unsigned char)buffer[1],
					(unsigned char)buffer[2]);

			err = dsp_msg(tfa, size, buffer);

			/* Reset bypass if writing msg files */
			if (err == TFA98XX_ERROR_OK)
				tfa->is_bypass = 0;
			break;
		case dsc_cmd: /* to change set-commands per profile */
			if (tfa_cont_is_config_loaded(tfa))
				break;

			size = *(uint16_t *)
				(prof->list[i].offset
				+ (char *)tfa->cnt);
			err = dsp_msg(tfa, size,
				prof->list[i].offset
				+ 2 + (char *)tfa->cnt);

			if (tfa->verbose) {
				const char *cmd_id = (prof->list[i].offset
					+ 2 + (char *)tfa->cnt);
				pr_debug("Writing cmd=0x%02x%02x%02x\n",
					(uint8_t)cmd_id[0],
					(uint8_t)cmd_id[1],
					(uint8_t)cmd_id[2]);
			}

			/* Reset bypass if writing msg files */
			if (err == TFA98XX_ERROR_OK)
				tfa->is_bypass = 0;
			break;
		default:
			/* ignore any other type */
			break;
		}
	}

	return err;
}

static enum tfa98xx_error tfa_cont_write_item
(struct tfa_device *tfa, struct tfa_desc_ptr *dsc)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_reg_patch *reg;
	struct tfa_mode *cas;
	struct tfa_bitfield *bitf;

	/* When no DSP should only write to HW registers. */
	if (tfa->ext_dsp == 0
		&& !(dsc->type == dsc_bit_field
		|| dsc->type == dsc_register)) {
		return TFA98XX_ERROR_OK;
	}

	switch (dsc->type) {
	case dsc_default:
	case dsc_device: /* ignore */
	case dsc_profile: /* profile list */
		break;
	case dsc_register: /* register patch */
		reg = (struct tfa_reg_patch *)
			(dsc->offset + (uint8_t *)tfa->cnt);
		/* pr_debug("$0x%2x=0x%02x,0x%02x\n",
		 * reg->address, reg->mask, reg->value);
		 */
		return tfa_run_write_register(tfa, reg);
	case dsc_string: /* ascii: zero terminated string */
		pr_debug(";string: %s\n",
			tfa_cont_get_string(tfa->cnt, dsc));
		break;
	case dsc_file: /* filename + file contents */
	case dsc_patch:
		break;
	case dsc_mode:
		cas = (struct tfa_mode *)
			(dsc->offset + (uint8_t *)tfa->cnt);
		if (cas->value == TFA98XX_MODE_RCV)
			tfa98xx_select_mode(tfa, TFA98XX_MODE_RCV);
		else
			tfa98xx_select_mode(tfa, TFA98XX_MODE_NORMAL);
		break;
	case dsc_bit_field:
		bitf = (struct tfa_bitfield *)
			(dsc->offset + (uint8_t *)tfa->cnt);
		return tfa_run_write_bitfield(tfa, *bitf);
	default:
		/* ignore any other type */
		break;
	}

	return err;
}

static unsigned int tfa98xx_sr_from_field(unsigned int field)
{
	switch (field) {
	case 0:
		return 8000;
	case 1:
		return 11025;
	case 2:
		return 12000;
	case 3:
		return 16000;
	case 4:
		return 22050;
	case 5:
		return 24000;
	case 6:
		return 32000;
	case 7:
		return 44100;
	case 8:
		return 48000;
	default:
		return 0;
	}
}

unsigned int tfa98xx_get_profile_sr(struct tfa_device *tfa,
	unsigned int prof_idx)
{
	struct tfa_bitfield *bitf;
	unsigned int i;
	struct tfa_device_list *dev;
	struct tfa_profile_list *prof;
	int fs_profile = -1;

	dev = tfa_cont_device(tfa->cnt, tfa->dev_idx);
	if (!dev)
		return 0;

	if (prof_idx == -1) { /* refer to prof in other device */
		if (tfa->profile != -1)
			prof_idx = tfa->profile;
	}
	prof = tfa_cont_get_dev_prof_list(tfa->cnt, tfa->dev_idx, prof_idx);
	if (!prof)
		return 0;

	/* Check profile fields first */
	for (i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dsc_default)
			break;

		/* check for profile settingd (AUDFS) */
		if (prof->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(prof->list[i].offset + (uint8_t *)tfa->cnt);
			if (bitf->field == TFA_FAM(tfa, AUDFS)) {
				fs_profile = bitf->value;
				break;
			}
		}
	}

	if (tfa->verbose)
		pr_debug("%s - profile fs: 0x%x = %dHz (%d - %d)\n",
			__func__, fs_profile,
			tfa98xx_sr_from_field(fs_profile),
			tfa->dev_idx, prof_idx);

	if (fs_profile != -1)
		return tfa98xx_sr_from_field(fs_profile);

	/* Check for container default setting */
	/* process the list until a patch, file of profile is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_patch
			|| dev->list[i].type == dsc_file
			|| dev->list[i].type == dsc_profile)
			break;

		if (dev->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(dev->list[i].offset + (uint8_t *)tfa->cnt);
			if (bitf->field == TFA_FAM(tfa, AUDFS)) {
				fs_profile = bitf->value;
				break;
			}
		}
		/* Ignore register case */
	}

	if (tfa->verbose)
		pr_debug("%s - default fs: 0x%x = %dHz (%d - %d)\n",
			__func__, fs_profile,
			tfa98xx_sr_from_field(fs_profile),
			tfa->dev_idx, prof_idx);

	if (fs_profile != -1)
		return tfa98xx_sr_from_field(fs_profile);

	return 48000; /* default of HW */
}

unsigned int tfa98xx_get_cnt_bitfield(struct tfa_device *tfa,
	uint16_t bitfield)
{
	struct tfa_bitfield *bitf;
	unsigned int i;
	struct tfa_device_list *dev;
	struct tfa_profile_list *prof;
	int value = -1;

	/* bypass case in Max2 */
	if (tfa->tfa_family != 2)
		return 0;

	if (tfa->verbose)
		pr_debug("%s: dev %d, prof %d, bitfield 0x%04x\n",
			__func__, tfa->dev_idx,
			tfa->profile, bitfield);

	dev = tfa_cont_device(tfa->cnt, tfa->dev_idx);
	if (!dev)
		return 0;

	prof = tfa_cont_get_dev_prof_list(tfa->cnt,
		tfa->dev_idx, tfa->profile);
	if (prof) {
		/* Check profile fields first */
		for (i = 0; i < prof->length; i++) {
			if (prof->list[i].type == dsc_default)
				break;

			/* check for profile setting */
			if (prof->list[i].type == dsc_bit_field) {
				bitf = (struct tfa_bitfield *)
					(prof->list[i].offset
					+ (uint8_t *)tfa->cnt);
				if (bitf->field == bitfield)
					value = bitf->value;
				if (value != -1)
					break;
			}
		}

		if (tfa->verbose)
			pr_debug("%s - profile value: 0x%x (%d - %d)\n",
				__func__, value,
				tfa->dev_idx, tfa->profile);
		if (value != -1)
			return value;
	}

	/* Check for container default setting */
	/* process the list until a patch, file of profile is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dsc_patch
			|| dev->list[i].type == dsc_file
			|| dev->list[i].type == dsc_profile)
			break;

		if (dev->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(dev->list[i].offset + (uint8_t *)tfa->cnt);
			if (bitf->field == bitfield)
				value = bitf->value;
			if (value != -1)
				break;
		}
		/* Ignore register case */
	}

	if (tfa->verbose)
		pr_debug("%s - default value: 0x%x (%d - %d)\n",
			__func__, value,
			tfa->dev_idx, tfa->profile);
	if (value != -1)
		return value;

	value = tfa_get_bf(tfa, bitfield);

	if (tfa->verbose)
		pr_debug("%s - current value: 0x%x\n",
			__func__, value);

	return value;
}

static enum tfa98xx_error get_sample_rate_info(struct tfa_device *tfa,
	struct tfa_profile_list *prof, struct tfa_profile_list *previous_prof,
	int fs_previous_profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_bitfield *bitf;
	unsigned int i;
	int fs_default_profile = 8;	/* default is 48kHz */
	int fs_next_profile = 8;	/* default is 48kHz */

	/* ---------- default settings previous profile ---------- */
	for (i = 0; i < previous_prof->length; i++) {
		/* Search for the default section */
		if (i == 0) {
			while (previous_prof->list[i].type != dsc_default
				&& i < previous_prof->length) {
				i++;
			}
			i++;
		}

		/* Only if we found the default section search for AUDFS */
		if (i < previous_prof->length) {
			if (previous_prof->list[i].type == dsc_bit_field) {
				bitf = (struct tfa_bitfield *)
					(previous_prof->list[i].offset
					+ (uint8_t *)tfa->cnt);
				if (bitf->field == TFA_FAM(tfa, AUDFS)) {
					fs_default_profile = bitf->value;
					break;
				}
			}
		}
	}

	/* ---------- settings next profile ---------- */
	for (i = 0; i < prof->length; i++) {
		/* only to write the values before default section */
		if (prof->list[i].type == dsc_default)
			break;
		/* search for AUDFS */
		if (prof->list[i].type == dsc_bit_field) {
			bitf = (struct tfa_bitfield *)
				(prof->list[i].offset + (uint8_t *)tfa->cnt);
			if (bitf->field == TFA_FAM(tfa, AUDFS)) {
				fs_next_profile = bitf->value;
				break;
			}
		}
	}

	/* Enable if needed for debugging!
	 * if (tfa98xx_cnt_verbose) {
	 *  pr_debug("sample rate from the previous profile: %d\n",
	 *   fs_previous_profile);
	 *  pr_debug("sample rate in the default section: %d\n",
	 *   fs_default_profile);
	 *  pr_debug("sample rate for the next profile: %d\n",
	 *   fs_next_profile);
	 * }
	 */

	if (fs_next_profile != fs_default_profile) {
		if (tfa->verbose)
			pr_debug("Writing delay tables for AUDFS=%d\n",
				fs_next_profile);

		/* If AUDFS from the next profile is not the same as
		 * AUDFS from the default we need to write new delay tables
		 */
		err = tfa98xx_dsp_write_tables(tfa, fs_next_profile);
	} else if (fs_default_profile != fs_previous_profile) {
		if (tfa->verbose)
			pr_debug("Writing delay tables for AUDFS=%d\n",
				fs_default_profile);

		/* But if we do not have a new AUDFS in the next profile
		 * and AUDFS from the default profile is not the same
		 * as AUDFS from the previous profile
		 * we also need to write new delay tables
		 */
		err = tfa98xx_dsp_write_tables(tfa, fs_default_profile);
	}

	return err;
}

/*
 * process all items in the profilelist
 * NOTE an error return during processing will leave the device muted
 */
enum tfa98xx_error tfa_cont_write_profile(struct tfa_device *tfa,
	int prof_idx, int vstep_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int previous_prof_idx = tfa_dev_get_swprof(tfa);
	struct tfa_profile_list *prof = NULL;
	struct tfa_profile_list *previous_prof = NULL;
	struct tfa_profile_list *prof_tfadsp = NULL;
	struct tfa_profile_list *previous_prof_tfadsp = NULL;
	char buffer[(MEMTRACK_MAX_WORDS * 3) + 3] = {0};
	/* every word requires 3 or 4 bytes, and 3 or 4 is the msg */
	unsigned int i, k = 0, j = 0;
	struct tfa_file_dsc *file;
	int size = 0, fs_previous_profile = 8; /* default fs is 48kHz */
	int ready, tries = 0;

	prof = tfa_cont_get_dev_prof_list(tfa->cnt,
		tfa->dev_idx, prof_idx);
	previous_prof = tfa_cont_get_dev_prof_list(tfa->cnt,
		tfa->dev_idx, previous_prof_idx);

	if (!prof || !previous_prof) {
		pr_err("Error trying to get the (previous) swprofile\n");
		err = TFA98XX_ERROR_BAD_PARAMETER;
		goto tfa_cont_write_profile_error_exit;
	}

	if (tfa->dev_tfadsp != -1) {
		prof_tfadsp = tfa_cont_get_dev_prof_list(tfa->cnt,
			tfa->dev_tfadsp, prof_idx);
		previous_prof_tfadsp = tfa_cont_get_dev_prof_list(tfa->cnt,
			tfa->dev_tfadsp, previous_prof_idx);
	} else {
		prof_tfadsp = prof;
		previous_prof_tfadsp = previous_prof;
	}

	if (!prof_tfadsp || !previous_prof_tfadsp) {
		pr_err("Error trying to get the (previous) swprofile for tfadsp\n");
		err = TFA98XX_ERROR_BAD_PARAMETER;
		goto tfa_cont_write_profile_error_exit;
	}

	if (tfa->verbose)
		pr_info("device:%s profile:%s vstep:%d\n",
			tfa_cont_device_name(tfa->cnt, tfa->dev_idx),
			tfa_cont_profile_name(tfa->cnt, tfa->dev_idx,
				prof_idx),
			vstep_idx);

	/* Get current sample rate before we start switching */
	fs_previous_profile = TFA_GET_BF(tfa, AUDFS);

	/* set all bitfield settings */
	/* First set all default settings */
	if (tfa->verbose)
		pr_debug("---------- default settings profile: %s (%d) ----------\n",
			tfa_cont_get_string(tfa->cnt,
				&previous_prof->name), previous_prof_idx);

	err = tfa_show_current_state(tfa);

	/* Loop profile length */
	for (i = 0; i < previous_prof->length; i++) {
		/* Search for the default section */
		if (i == 0) {
			while (previous_prof->list[i].type != dsc_default
				&& i < previous_prof->length)
				i++;
			i++;
		}

		/* Only if we found the default section try writing the items */
		if (i < previous_prof->length) {
			if (tfa_cont_write_item(tfa,
				&previous_prof->list[i])
				!= TFA98XX_ERROR_OK) {
				pr_err("%s: Error in writing default items!\n",
					__func__);
				err = TFA98XX_ERROR_BAD_PARAMETER;
				goto tfa_cont_write_profile_error_exit;
			}
		}
	}

	if (tfa->verbose)
		pr_debug("---------- new settings profile: %s (%d) ----------\n",
			tfa_cont_get_string(tfa->cnt,
			&prof->name), prof_idx);

	/* set new settings */
	for (i = 0; i < prof->length; i++) {
		/* only to write the values before default section
		 * when we switch profile
		 */
		if (prof->list[i].type == dsc_default)
			break;

		/* process and write all non-file items */
		switch (prof->list[i].type) {
		case dsc_file:
		case dsc_patch:
		case dsc_set_input_select:
		case dsc_set_output_select:
		case dsc_set_program_config:
		case dsc_set_lag_w:
		case dsc_set_gains:
		case dsc_set_vbat_factors:
		case dsc_set_senses_cal:
		case dsc_set_senses_delay:
		case dsc_set_mb_drc:
		case dsc_set_fw_use_case:
		case dsc_set_vddp_config:
		case dsc_cmd:
		case dsc_filter:
			/* Skip files / commands and continue */
			/* i = prof->length; */
			break;
		default:
			/* Remember where we currently are with writing items*/
			j = i;

			err = tfa_cont_write_item(tfa, &prof->list[i]);
			if (err != TFA98XX_ERROR_OK) {
				pr_err("%s: Error in writing items!\n",
					__func__);
				err = TFA98XX_ERROR_BAD_PARAMETER;
				goto tfa_cont_write_profile_error_exit;
			}
			break;
		}
	}

	if (tfa_cont_is_standby_profile(tfa, prof_idx)) {
		pr_info("%s: Keep power down without writing files, in standby profile!\n",
			__func__);

		err = tfa98xx_powerdown(tfa, 1);
		if (err)
			goto tfa_cont_write_profile_error_exit;

		/* Wait until we are in PLL powerdown */
		tries = 0;
		do {
			err = tfa98xx_dsp_system_stable(tfa, &ready);
			if (!ready)
				break;

			/* wait 10ms to avoid busload */
			msleep_interruptible(10);
			tries++;
		} while (tries <= TFA98XX_WAITPOWERUP_NTRIES);

		if (tries > TFA98XX_WAITPOWERUP_NTRIES) {
			pr_debug("Wait for PLL powerdown timed out!\n");
			err = TFA98XX_ERROR_STATE_TIMED_OUT;
			goto tfa_cont_write_profile_error_exit;
		}

		err = tfa_show_current_state(tfa);
		goto tfa_cont_write_profile_error_exit;
	}

	/* Check if there are sample rate changes */
	err = get_sample_rate_info(tfa, prof,
		previous_prof, fs_previous_profile);
	if (err) {
		pr_err("%s: error in getting sr\n", __func__);
		goto tfa_cont_write_profile_error_exit;
	}

	/* Write files from previous profile (default section)
	 * Should only be used for the patch & trap patch (file)
	 */
	if ((tfa->ext_dsp != 0) && (tfa->tfa_family == 2)) {
		for (i = 0; i < previous_prof_tfadsp->length; i++) {
			char type;

			/* Search for the default section */
			if (i == 0) {
				while (previous_prof_tfadsp->list[i].type
					!= dsc_default
					&& i < previous_prof_tfadsp->length) {
					i++;
				}
				i++;
			}

			/*
			 * Only if we found the default section
			 * try writing the file
			 */
			if (i >= previous_prof_tfadsp->length)
				break;

			type = previous_prof_tfadsp->list[i].type;
			if (type != dsc_file && type != dsc_patch)
				continue;

			/* Only write this once */
			if (tfa->verbose && k == 0) {
				pr_debug("---------- files default profile: %s (%d) ----------\n",
					tfa_cont_get_string
					(tfa->cnt, &previous_prof_tfadsp->name),
					prof_idx);
				k++;
			}
			file = (struct tfa_file_dsc *)
				(previous_prof_tfadsp->list[i].offset
				+ (uint8_t *)tfa->cnt);
			err = tfa_cont_write_file(tfa,
				file, vstep_idx,
				TFA_MAX_VSTEP_MSG_MARKER);
		}

		if (tfa->verbose)
			pr_debug("---------- files new profile: %s (%d) ----------\n",
				tfa_cont_get_string(tfa->cnt,
				&prof_tfadsp->name), prof_idx);
	}

	if (tfa->dev_tfadsp != -1)
		j = 0; /* reset to begin from the head */

	/* write everything
	 * until end or the default section starts
	 */
	/* Start where we currenly left */
	for (i = j; i < prof_tfadsp->length; i++) {
		/* only to write the values
		 * before the default section when we switch profile
		 */
		if (prof_tfadsp->list[i].type == dsc_default)
			break;

		switch (prof_tfadsp->list[i].type) {
		case dsc_file:
		case dsc_patch:
			/* For tiberius stereo 1 device does not have a dsp! */
			if (tfa->ext_dsp == 0)
				break;
			if (tfa_cont_is_config_loaded(tfa))
				break;

			file = (struct tfa_file_dsc *)
				(prof_tfadsp->list[i].offset
				+ (uint8_t *)tfa->cnt);
			err = tfa_cont_write_file(tfa, file,
				vstep_idx, TFA_MAX_VSTEP_MSG_MARKER);
			break;
		case dsc_set_input_select:
		case dsc_set_output_select:
		case dsc_set_program_config:
		case dsc_set_lag_w:
		case dsc_set_gains:
		case dsc_set_vbat_factors:
		case dsc_set_senses_cal:
		case dsc_set_senses_delay:
		case dsc_set_mb_drc:
		case dsc_set_fw_use_case:
		case dsc_set_vddp_config:
			/* For tiberius device 1 has no dsp! */
			if (tfa->ext_dsp == 0)
				break;
			if (tfa_cont_is_config_loaded(tfa))
				break;

			create_dsp_buffer_msg(tfa,
				(struct tfa_msg *)
				(prof_tfadsp->list[i].offset
				+ (char *)tfa->cnt),
				buffer, &size);
			err = dsp_msg(tfa, size, buffer);

			if (tfa->verbose)
				pr_debug("command: %s=0x%02x%02x%02x\n",
					tfa_cont_get_command_string
					(prof_tfadsp->list[i].type),
					(unsigned char)buffer[0],
					(unsigned char)buffer[1],
					(unsigned char)buffer[2]);

			/* Reset bypass if writing msg files */
			if (err == TFA98XX_ERROR_OK)
				tfa->is_bypass = 0;
			break;
		case dsc_cmd:
			/* For tiberius device 1 has no dsp! */
			if (tfa->ext_dsp == 0)
				break;
			if (tfa_cont_is_config_loaded(tfa))
				break;

			size = *(uint16_t *)
				(prof_tfadsp->list[i].offset
				+ (char *)tfa->cnt);
			err = dsp_msg(tfa, size,
				prof_tfadsp->list[i].offset
				+ 2 + (char *)tfa->cnt);

			if (tfa->verbose) {
				const char *cmd_id
					= prof_tfadsp->list[i].offset
					+ 2 + (char *)tfa->cnt;
				pr_debug("Writing cmd=0x%02x%02x%02x\n",
					(uint8_t)cmd_id[0],
					(uint8_t)cmd_id[1],
					(uint8_t)cmd_id[2]);
			}

			/* Reset bypass if writing msg files */
			if (err == TFA98XX_ERROR_OK)
				tfa->is_bypass = 0;
			break;
		default:
			/*
			 * Already have written in non-tfadsp device:
			 * err = tfa_cont_write_item(tfa,
			 *		&prof_tfadsp->list[i]);
			 */
			break;
		}

		if (err != TFA98XX_ERROR_OK) {
			pr_err("%s: Error in writing new files and more!\n",
				__func__);
			goto tfa_cont_write_profile_error_exit;
		}
	}

	tfa_dev_set_swprof(tfa, (unsigned short)prof_idx);

	/* put SetRe25C message to indicate all messages are sent */
	if (tfa->ext_dsp == 1)
		err = tfa_set_calibration_values_once(tfa);

tfa_cont_write_profile_error_exit:
	return err;
}

char *tfa_cont_get_string(struct tfa_container *cnt, struct tfa_desc_ptr *dsc)
{
	if (dsc->type != dsc_string)
		return "Undefined string";

	return dsc->offset + (char *)cnt;
}

char *tfa_cont_get_command_string(uint32_t type)
{
	if (type == dsc_set_input_select)
		return "SetInputSelector";
	else if (type == dsc_set_output_select)
		return "SetOutputSelector";
	else if (type == dsc_set_program_config)
		return "SetProgramConfig";
	else if (type == dsc_set_lag_w)
		return "SetLagW";
	else if (type == dsc_set_gains)
		return "SetGains";
	else if (type == dsc_set_vbat_factors)
		return "SetvBatFactors";
	else if (type == dsc_set_senses_cal)
		return "SetSensesCal";
	else if (type == dsc_set_senses_delay)
		return "SetSensesDelay";
	else if (type == dsc_set_mb_drc)
		return "SetMBDrc";
	else if (type == dsc_set_fw_use_case)
		return "SetFwkUseCase";
	else if (type == dsc_set_vddp_config)
		return "SetVddpConfig";
	else if (type == dsc_filter)
		return "filter";
	else
		return "Undefined string";
}

/*
 * Get the name of the device at a certain index in the container file
 * return device name
 */
char *tfa_cont_device_name(struct tfa_container *cnt, int dev_idx)
{
	struct tfa_device_list *dev;

	dev = tfa_cont_device(cnt, dev_idx);
	if (dev == NULL)
		return "!ERROR!";

	return tfa_cont_get_string(cnt, &dev->name);
}

/*
 * Get the customer name from the container file customer field
 * note that the input stringbuffer should be sizeof(customer field)+1
 */
int tfa_cont_get_customer_name(struct tfa_device *tfa, char *name)
{
	unsigned int i;
	int len = 0;

	for (i = 0; i < sizeof(tfa->cnt->customer); i++) {
		if (isalnum(tfa->cnt->customer[i])) /* copy char if valid */
			name[len++] = tfa->cnt->customer[i];
		if (tfa->cnt->customer[i] == '\0')
			break;
	}
	name[len++] = '\0';

	return len;
}

/*
 * Get the application name from the container file application field
 * note that the input stringbuffer should be sizeof(application field)+1
 */
int tfa_cont_get_app_name(struct tfa_device *tfa, char *name)
{
	unsigned int i;
	int len = 0;

	for (i = 0; i < sizeof(tfa->cnt->application); i++) {
		if (isalnum(tfa->cnt->application[i])) /* copy char if valid */
			name[len++] = tfa->cnt->application[i];
		if (tfa->cnt->application[i] == '\0')
			break;
	}
	name[len++] = '\0';

	return len;
}

/*
 * Get profile index of the calibration profile.
 * Returns: (profile index) if found, (-2) if no
 * calibration profile is found or (-1) on error
 */
int tfa_cont_get_cal_profile(struct tfa_device *tfa)
{
	int prof, cal_idx = -2;
	char prof_name[MAX_CONTROL_NAME] = {0};

	if ((tfa->dev_idx < 0) || (tfa->dev_idx >= tfa->cnt->ndev))
		return TFA_ERROR;

	/* search for the calibration profile in the list of profiles */
	for (prof = 0; prof < tfa->cnt->nprof; prof++) {
		strlcpy(prof_name, tfa_cont_profile_name(tfa->cnt,
			tfa->dev_idx, prof), MAX_CONTROL_NAME);
		if (strnstr(prof_name, ".cal", strlen(prof_name)) != NULL) {
			cal_idx = prof;
			pr_debug("Using calibration profile: '%s'\n",
				tfa_cont_profile_name(tfa->cnt,
					tfa->dev_idx, prof));
			break;
		}
	}

	pr_info("%s: cal_prof = %d", __func__, cal_idx);

	return cal_idx;
}

/*
 * Is the profile a standby profile
 */
int tfa_cont_is_standby_profile(struct tfa_device *tfa, int prof_idx)
{
	char prof_name[MAX_CONTROL_NAME] = {0};

	if ((tfa->dev_idx < 0) || (tfa->dev_idx >= tfa->cnt->ndev))
		return TFA_ERROR;

	strlcpy(prof_name, tfa_cont_profile_name(tfa->cnt,
		tfa->dev_idx, prof_idx), MAX_CONTROL_NAME);
	/* Check if next profile is standby profile */
	if (strnstr(prof_name, ".standby", strlen(prof_name)) != NULL) {
		pr_debug("Using Standby profile: '%s'\n",
			tfa_cont_profile_name(tfa->cnt,
			tfa->dev_idx, prof_idx));
		return 1;
	}

	return 0;
}

/*
 * Is the profile specific to device ?
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return 1 if the profile belongs to device or 0 if not
 */
int tfa_cont_is_dev_specific_profile(struct tfa_container *cnt,
	int dev_idx, int prof_idx)
{
	char *pch;
	int prof_name_len;
	char prof_name[MAX_CONTROL_NAME] = {0};
	char dev_substring[MAX_CONTROL_NAME] = {0};

	snprintf(prof_name, MAX_CONTROL_NAME, "%s",
		tfa_cont_profile_name(cnt, dev_idx, prof_idx));
	prof_name_len = strlen(prof_name);

	pch = strnchr(prof_name, prof_name_len, '.');
	if (!pch)
		return 0;

	snprintf(dev_substring, MAX_CONTROL_NAME,
		".%s", tfa_cont_device_name(cnt, dev_idx));
	if (prof_name_len < strlen(dev_substring))
		return 0;

	/* Check if next profile name contains device name */
	pch = strnstr(prof_name, dev_substring, prof_name_len);
	if (!pch) {
		pr_debug("dev profile: '%s' of device '%s'\n",
			prof_name, dev_substring);
		return 1;
	}

	return 0;
}

/*
 * Get the name of the profile at certain index for a device
 * in the container file
 * return profile name
 */
char *tfa_cont_profile_name(struct tfa_container *cnt,
	int dev_idx, int prof_idx)
{
	struct tfa_profile_list *prof = NULL;

	/* the Nth profiles for this device */
	prof = tfa_cont_get_dev_prof_list(cnt, dev_idx, prof_idx);

	/* If the index is out of bound */
	if (prof == NULL)
		return "NONE";

	return tfa_cont_get_string(cnt, &prof->name);
}

/*
 * return the device list pointer
 */
struct tfa_device_list *tfa_cont_device(struct tfa_container *cnt,
	int dev_idx)
{
	return tfa_cont_get_dev_list(cnt, dev_idx);
}

/*
 * check CRC for container
 * CRC is calculated over the bytes following the CRC field
 * return non zero value on error
 */
int tfa_cont_crc_check_container(struct tfa_container *cont)
{
	uint8_t *base;
	size_t size;
	uint32_t crc;

	base = (uint8_t *)&cont->crc + 4;
	/* ptr to bytes following the CRC field */
	size = (size_t)(cont->size - (base - (uint8_t *)cont));
	/* nr of bytes following the CRC field */
	crc = ~crc32_le(~0u, base, size);

	return crc != cont->crc;
}

enum tfa98xx_error tfa98xx_factory_trimmer(struct tfa_device *tfa)
{
	return (tfa->dev_ops.factory_trimmer)(tfa);
}

enum tfa98xx_error tfa_set_filters(struct tfa_device *tfa, int prof_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_profile_list *prof
		= tfa_cont_get_dev_prof_list(tfa->cnt, tfa->dev_idx, prof_idx);
	unsigned int i;

	if (!prof)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* If we are in powerdown there is no need to set filters */
	if (TFA_GET_BF(tfa, PWDN) == 1)
		return TFA98XX_ERROR_OK;

	/* loop the profile to find filter settings */
	for (i = 0; i < prof->length; i++) {
		/* write values before default section */
		if (prof->list[i].type == dsc_default)
			break;

		/* write all filter settings */
		if (prof->list[i].type == dsc_filter) {
			if (tfa_cont_write_item(tfa,
				&prof->list[i]) != TFA98XX_ERROR_OK)
				return err;
		}
	}

	return err;
}

int tfa_tib_dsp_msgmulti(struct tfa_device *tfa,
	int length, const char *buffer)
{
	uint8_t *buf = (uint8_t *)buffer;
	static enum tfa_blob_index idx = BLOB_INDEX_REGULAR; /* default */
	static uint8_t *blob[BLOB_INDEX_MAX] = {NULL, NULL};
	static uint8_t *blobptr[BLOB_INDEX_MAX] = {NULL, NULL};
	static int blob_p_index[BLOB_INDEX_MAX] = {-1, -1};
	static int total[BLOB_INDEX_MAX] = {0, 0};
	int post_len = 0;
	uint8_t cmd, cc;

	/* checks for 24b_BE or 32_LE */
	int len_word_in_bytes = (tfa->convert_dsp32) ? 4 : 3;
	/* TODO: get rid of these magic constants
	 * max size should depend on the tfa device type
	 */
	/*
	 * int tfadsp_max_msg_size = (tfa->convert_dsp32)
	 *  ? MAX_APR_MSG_SIZE : (MAX_APR_MSG_SIZE * 3 / 4);
	 * 5336 or 4000
	 */
	/* TEMPORARY, set 16KB to utilize dsp_msg_packet */
	int tfadsp_max_msg_size = 16 * 1024;

	/* set to blob for individual message */
	if (tfa->individual_msg)
		idx = BLOB_INDEX_INDIVIDUAL; /* until it's transferred */

	/* Transfer buffered messages */
	if (length == -1) {
		int i, total_len = total[idx];

		/* No data found */
		if (blob[idx] == NULL || buf == NULL) {
			pr_err("%s: %s is NULL (index %d)!\n", __func__,
				(blob[idx] == NULL) ? "blob" : "buf", idx);
			return TFA_ERROR;
		}

		pr_debug("%s: transfer blob (index %d)\n",
			__func__, idx);

		/* add total - specially merged fom legacy */
		if (tfa->convert_dsp32) {
			if (total_len <= 0xffff) {
				blob[idx][2] = (uint8_t)
					((total_len >> 8) & 0xff);
				blob[idx][3] = (uint8_t)(total_len & 0xff);
			}
		} else {
			if (total_len <= 0xff)
				blob[idx][0] = (uint8_t)(total_len & 0xff);
		}
		/* set last length field to zero */
		for (i = total_len;
			i < (total_len + len_word_in_bytes); i++)
			blob[idx][i] = 0;

		total_len += len_word_in_bytes;
		memcpy(buf, blob[idx], total_len);

		if (blob_p_index[idx] != -1)
			blob_p_index[idx] = tfa98xx_buffer_pool_access
				(blob_p_index[idx], 0, &blob[idx], POOL_RETURN);
		else
			kfree(blob[idx]);
		/* set blob to NULL pointer, to free memory */
		blob[idx] = NULL;

		/* reset to blob for regular message */
		idx = BLOB_INDEX_REGULAR; /* default */
		tfa->individual_msg = 0; /* reset flag */

		return total_len;
	}

	/* Flush buffer */
	if (length == -2) {
		if (blob[idx] == NULL) {
			pr_info("%s: already flushed - NULL (index %d)!\n",
				__func__, idx);
			return 0;
		}

		pr_debug("%s: flush blob (index %d)\n",
			__func__, idx);

		if (blob_p_index[idx] != -1)
			blob_p_index[idx] = tfa98xx_buffer_pool_access
				(blob_p_index[idx], 0, &blob[idx], POOL_RETURN);
		else
			kfree(blob[idx]);
		/* set blob to NULL pointer, to free memory */
		blob[idx] = NULL;

		/* reset to blob for regular message */
		idx = BLOB_INDEX_REGULAR; /* default */

		return 0;
	}

	/* Allocate buffer */
	if (blob[idx] == NULL) {
		if (tfa->verbose)
			pr_debug("%s, creating multi-message:\n", __func__);

		pr_debug("%s: allocate blob (index %d)\n",
			__func__, idx);

		/* return if already allocated, to get a new one */
		if (blob_p_index[idx] != -1)
			tfa98xx_buffer_pool_access
				(blob_p_index[idx], 0, &blob[idx], POOL_RETURN);
		blob_p_index[idx] = tfa98xx_buffer_pool_access
			(-1, 64 * 1024, &blob[idx], POOL_GET);
		if (blob_p_index[idx] != -1) {
			pr_debug("%s: allocated from buffer_pool[%d]\n",
				__func__, blob_p_index[idx]);
		} else {
			blob[idx] = kmalloc(64 * 1024, GFP_KERNEL);
			/* max length is 64k */
			if (blob[idx] == NULL)
				return TFA98XX_ERROR_FAIL;
		}

		/* add command ID for multi-msg = 0x008015 */
		if (tfa->convert_dsp32) {
			blob[idx][0] = FW_PAR_ID_SET_MULTI_MESSAGE;
			blob[idx][1] = 0x80 | MODULE_FRAMEWORK;
			blob[idx][2] = 0x0;
			blob[idx][3] = 0x0;
		} else {
			blob[idx][0] = 0x0;
			blob[idx][1] = 0x80 | MODULE_FRAMEWORK;
			blob[idx][2] = FW_PAR_ID_SET_MULTI_MESSAGE;
		}
		pr_debug("%s: multi-msg (index %d) [0]=0x%x-[1]=0x%x-[2]=0x%x\n",
			__func__, idx,
			blob[idx][0], blob[idx][1], blob[idx][2]);

		blobptr[idx] = blob[idx];
		blobptr[idx] += len_word_in_bytes;
		total[idx] = len_word_in_bytes;
	}

	/* check total message size after concatination */
	post_len = total[idx] + length + (2 * len_word_in_bytes);
	if (post_len > tfadsp_max_msg_size) {
		pr_debug("%s: set buffer full for blob (index %d): %d >= max %d, current length: %d\n",
			__func__, idx, post_len,
			tfadsp_max_msg_size, total[idx]);
		return TFA98XX_ERROR_BUFFER_TOO_SMALL;
	}

	if (buf == NULL) {
		pr_err("%s: buf is NULL (index %d)!\n",
			__func__, idx);
		return TFA98XX_ERROR_FAIL;
	}

	/* Accumulate messages to buffer */
	if (tfa->verbose)
		pr_debug("%s, id:0x%02x%02x%02x, length:%d\n",
			__func__, buf[0], buf[1], buf[2], length);

	/* add length field (length in words) to the multi message */
	if (tfa->convert_dsp32) {
		*blobptr[idx]++ = (uint8_t) /* lsb */
			((length / len_word_in_bytes) & 0xff);
		*blobptr[idx]++ = (uint8_t) /* msb */
			(((length / len_word_in_bytes) & 0xff00) >> 8);
		*blobptr[idx]++ = 0x0;
		*blobptr[idx]++ = 0x0;
	} else {
		*blobptr[idx]++ = 0x0;
		*blobptr[idx]++ = (uint8_t) /* msb */
			(((length / len_word_in_bytes) & 0xff00) >> 8);
		*blobptr[idx]++ = (uint8_t) /* lsb */
			((length / len_word_in_bytes) & 0xff);
	}
	memcpy(blobptr[idx], buf, length);
	blobptr[idx] += length;
	total[idx] += (length + len_word_in_bytes);

	/* SetRe25 message is always the last message of the multi-msg */
	pr_debug("%s: length (%d), [0]=0x%x-[1]=0x%x-[2]=0x%x\n",
		__func__, length, buf[0], buf[1], buf[2]);

	cmd = (tfa->convert_dsp32) ? buf[0] : buf[2];
	cc = (tfa->convert_dsp32) ? buf[2] : buf[0];

	if (idx == BLOB_INDEX_INDIVIDUAL) {
		pr_debug("%s: set last message for individual call: module=%d cmd=%d (index %d)\n",
			__func__, buf[1], cmd, idx);
		return 1; /* 1 means last message is done! */
	}

	if (cmd == SB_PARAM_SET_RE25C && buf[1]
		== (0x80 | MODULE_SPEAKERBOOST)) {
		pr_debug("%s: found last message - sending: Re25C cmd=%d (index %d)\n",
			__func__, cmd, idx);
		return 1; /* 1 means last message is done! */
	}

	if (cmd & 0x80) { /* ..._GET_* command */
		pr_debug("%s: found last message - sending: module=%d cmd=%d CC=%d (index %d)\n",
			__func__, buf[1], cmd, cc, idx);
		return 1; /* 1 means last message is done! with CC check */
	}

	return 0;
}

