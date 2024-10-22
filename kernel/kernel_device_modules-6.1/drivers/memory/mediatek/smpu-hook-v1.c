// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

 #include <soc/mediatek/smpu.h>
 #include <linux/bits.h>
 #include <linux/kernel.h>
 #include <linux/module.h>
 #include <linux/ratelimit.h>

static bool axi_id_is_gpu(unsigned int axi_id, int vio_type)
{
	unsigned int port;
	unsigned int id;
	unsigned int i;
	struct smpu *mpu = NULL;

	port = axi_id & (BIT_MASK(3) - 1);
	id = axi_id >> 3;

	switch (vio_type) {
	case VIO_TYPE_NSMPU:
			mpu = global_nsmpu;
			break;
	case VIO_TYPE_SSMPU:
			mpu = global_ssmpu;
			break;
	default:
			break;
	}
	if (!mpu)
		return false;

	for (i = 0; i < mpu->bypass_axi_num; i++) {
		if (port == mpu->bypass_axi[i].port && ((id & mpu->bypass_axi[i].axi_mask)
					== mpu->bypass_axi[i].axi_value))
			return true;
	}

	return false;
}
int bypass_info(unsigned int offset, int vio_type)
{
	unsigned int i;
	struct smpu *mpu = NULL;

	switch (vio_type) {
	case VIO_TYPE_NSMPU:
			mpu = global_nsmpu;
			break;
	case VIO_TYPE_SSMPU:
			mpu = global_ssmpu;
			break;
	default:
			break;
	}
	if (!mpu) {
		pr_info("%s mpu is NULL\n", __func__);
		return -1;
	}

	for (i = 0; i < mpu->bypass_miu_reg_num; i++) {
		if (offset == mpu->bypass_miu_reg[i])
			return i;
	}
	return -1;

}

bool aid_is_gpu_w(int vio_type, struct smpu_reg_info_t *dump)
{
	int i;
	ssize_t msg_len = 0;
	struct smpu *mpu = NULL;

	switch (vio_type) {
	case VIO_TYPE_NSMPU:
			mpu = global_nsmpu;
			break;
	case VIO_TYPE_SSMPU:
			mpu = global_ssmpu;
			break;
	default:
			break;
	}
	if (!mpu) {
		pr_info("%s mpu is NULL\n", __func__);
		return -1;
	}

	if (!mpu->gpu_bypass_list)
		return false;

	if (dump[mpu->gpu_bypass_list[0]].value == mpu->gpu_bypass_list[1]) {
		pr_info("[SMPU] gpu write violation occurs\n");
		for (i = 0; i < mpu->dump_cnt; i++) {
			if (msg_len < MAX_GPU_VIO_LEN)
				msg_len += scnprintf(mpu->vio_msg_gpu + msg_len, MAX_GPU_VIO_LEN - msg_len,
					"[%x]%x;", dump[i].offset,
					dump[i].value);
		}
		pr_info("%s: %s", __func__, mpu->vio_msg_gpu);
		return true;
	}
	return false;


}

static irqreturn_t smpu_isr_vio_hook(struct smpu_reg_info_t *dump, unsigned int leng, int vio_type)
{
	int i;
	unsigned int srinfo_r = 0, axi_id_r = 0;
	unsigned int srinfo_w = 0, axi_id_w = 0;
	bool bypass, result;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1*HZ, 3);

	for (i = 0; i < leng; i++) {
		switch (bypass_info(dump[i].offset, vio_type)) {
		case WRITE_SRINFO:
			srinfo_w = dump[i].value;
			break;
		case READ_SRINFO:
			srinfo_r = dump[i].value;
			break;
		case WRITE_AXI:
			if (srinfo_w == 3)
				axi_id_w |= (dump[i].value & (BIT_MASK(20) - 1));
			break;
		case READ_AXI:
			if (srinfo_r == 3)
				axi_id_r |= (dump[i].value & (BIT_MASK(20) - 1));
			break;
		case WRITE_AXI_MSB:
			if (srinfo_w == 3) {
				axi_id_w &= (BIT_MASK(16) - 1);
				axi_id_w |= ((dump[i].value &
				(BIT_MASK(4) - 1)) << 16);
			}
			break;
		case READ_AXI_MSB:
			if (srinfo_r == 3) {
				axi_id_r &= (BIT_MASK(16) - 1);
				axi_id_r |= ((dump[i].value &
				(BIT_MASK(4) - 1)) << 16);
			}
			break;
		default:
			break;


	}

//	pr_info("srinfo_r is %d, srinfo_w is %d\n", srinfo_r, srinfo_w);

	if (srinfo_r == 3 && !axi_id_is_gpu(axi_id_r, vio_type))
		bypass = true;
	else if (srinfo_w == 3 && !axi_id_is_gpu(axi_id_w, vio_type))
		bypass = true;
	else
		bypass = false;

	//mt6897 bypass gpu w vio
	 result = aid_is_gpu_w(vio_type, dump);
	 bypass = bypass ||  result;

	if (bypass == true) {
		if (__ratelimit(&ratelimit)) {
			pr_info("srinfo_r %d, axi_id_r 0x%x\n", srinfo_r, axi_id_r);
			pr_info("srinfo_w %d, axi_id_w 0x%x\n", srinfo_w, axi_id_w);
		}
	}

	}
//	pr_info("bypass is %d", bypass);
	return (bypass) ? IRQ_HANDLED : IRQ_NONE;
}

static __init int smpu_hook_init(void)
{
	int ret;

	pr_info("smpu-hook was loaded\n");

	ret = mtk_smpu_isr_hook_register(smpu_isr_vio_hook);

	if (ret)
		pr_err("Failed to register the smpu isr hook function\n");

	return 0;
}

module_init(smpu_hook_init);

MODULE_DESCRIPTION("MediaTek SMPU HOOK Driver");
MODULE_LICENSE("GPL");

