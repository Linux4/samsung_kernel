// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#include "mtk-ise-mailbox.h"

static void __iomem *mbox_base;
static uint32_t real_drv;
static struct mutex mbox_lock;

static uint32_t mlb_read(uint32_t offset)
{
	void __iomem *reg = mbox_base + offset;

	return readl(reg);
}

static void mlb_write(uint32_t offset, uint32_t value)
{
	void __iomem *reg = mbox_base + offset;

	writel(value, reg);
}

static uint32_t mailbox_print_request(const mailbox_request_t *request)
{
	int i;

	if (request == NULL)
		return MAILBOX_INVALID_PARAMETER;

	pr_debug(PFX "%s: Service ID     : %u\n", __func__, request->service_id);
	pr_debug(PFX "%s: Service version: %u\n", __func__, request->service_version);
	pr_debug(PFX "%s: Payload size   : %u\n", __func__, request->payload.size);
	pr_debug(PFX "%s: Payload content:", __func__);

	for (i = 0; i < request->payload.size; i++)
		pr_debug(" [0x%x]", request->payload.fields[i]);

	pr_debug("\n");

	return MAILBOX_SUCCESS;
}

static uint32_t mailbox_print_reply(const mailbox_reply_t *reply)
{
	int i;

	if (reply == NULL)
		return MAILBOX_INVALID_PARAMETER;

	pr_debug(PFX "%s: Service ID     : %u\n", __func__, reply->service_id);
	pr_debug(PFX "%s: Error status   : 0x%x\n", __func__, reply->status.error);
	pr_debug(PFX "%s: Payload size   : %u\n", __func__, reply->payload.size);
	pr_debug(PFX "%s: Payload content:", __func__);

	for (i = 0; i < reply->payload.size; i++)
		pr_debug(" [0x%x]", reply->payload.fields[i]);

	pr_debug("\n");

	return MAILBOX_SUCCESS;
}

static uint32_t mailbox_poll(uint32_t addr, uint32_t mask, uint32_t expected_value)
{
	uint32_t retry = MAILBOX_RETRY;

	while ((mlb_read(addr) & mask) != expected_value && retry) {
		retry--;
	}

	if (!retry)
		return MAILBOX_TIMEOUT;

	return MAILBOX_SUCCESS;
}

static uint32_t mailbox_check_safety(const uint32_t expected_header,
	const mailbox_request_t *request)
{
	uint32_t error = MAILBOX_SUCCESS;
	uint32_t header;
	uint32_t expected_value;
	uint32_t value;
	unsigned int i;

	if (request == NULL)
		return MAILBOX_INVALID_PARAMETER;

	error = mailbox_poll(MB_HOST_SAFETY_STATUS_OFFSET, 1, 1);
	if (error != MAILBOX_SUCCESS) {
		pr_err(PFX "%s: poll mailbox failed\n", __func__);
		return error;
	}

	header = ~mlb_read(MB_HOST_SAFETY_FIFO_OFFSET);
	if (header != expected_header) {
		pr_err(PFX "%s: safety failed, expected_header 0x%x, header 0x%x\n",
			__func__, expected_header, header);
		return MAILBOX_INVALID_SAFETY;
	}

	for (i = 0; i < request->payload.size; i++) {
		expected_value = request->payload.fields[i];
		value = ~mlb_read(MB_HOST_SAFETY_FIFO_OFFSET);
		if (value != expected_value) {
			pr_err(PFX "%s: safety failed, expected_value 0x%x, value 0x%x\n",
				__func__, expected_header, header);
			return MAILBOX_INVALID_SAFETY;
		}
	}

	mlb_write(MB_HOST_SAFETY_CONTROL_OFFSET, 1);

	return error;
}

static uint32_t mailbox_wait_for_result(void)
{
	return mailbox_poll(MB_HOST_OUT_STATUS_OFFSET, 1, 1);
}

static uint32_t mailbox_wait_for_scz_to_be_ready(void)
{
	return mailbox_poll(MB_HOST_IN_STATUS_OFFSET, 1, 1);
}

static void mailbox_push_to_fifo(uint32_t value)
{
	mlb_write(MB_HOST_IN_FIFO_OFFSET, value);
}

static uint32_t mailbox_pop_from_fifo(void)
{
	return mlb_read(MB_HOST_OUT_FIFO_OFFSET);
}

static void mailbox_flush_out_fifos(void)
{
	while ((mlb_read(MB_HOST_OUT_STATUS_OFFSET) & 1) == 1)
		mlb_write(MB_HOST_OUT_CONTROL_OFFSET, 1);

	while ((mlb_read(MB_HOST_SAFETY_STATUS_OFFSET) & 1) == 1)
		mlb_write(MB_HOST_SAFETY_CONTROL_OFFSET, 1);
}

static uint8_t mailbox_init_securyzr(void)
{
	uint32_t error = MAILBOX_SUCCESS;

	mlb_write(MB_HOST_IRQ_CONFIG_0_OFFSET, 0xFFFFFFFF);
	mlb_write(MB_HOST_IN_CONTROL_OFFSET, 1 | (1 << 1));
	mailbox_flush_out_fifos();

	error = mailbox_wait_for_scz_to_be_ready();
	if (error != MAILBOX_SUCCESS) {
		pr_err(PFX "%s: poll mailbox for scz failed\n", __func__);
		return (uint8_t)error;
	}

	return mlb_read(MB_HOST_BOOT_IP_INTEGRITY_STATUS_OFFSET) & 0xFF;
}

static uint32_t mailbox_get_boot_status(void)
{
	return mlb_read(MB_HOST_SECTION_BOOT_STATUS_OFFSET);
}

static uint32_t mailbox_get_reply(mailbox_reply_t *reply)
{
	uint32_t error = MAILBOX_SUCCESS;
	uint32_t packed_reply_header;
	int i;

	if (reply == NULL)
		return MAILBOX_INVALID_PARAMETER;

	error = mailbox_wait_for_result();
	if (error != MAILBOX_SUCCESS) {
		pr_err(PFX "%s: poll mailbox for result failed\n", __func__);
		return error;
	}

	packed_reply_header = mailbox_pop_from_fifo();
	mailbox_unpack_reply_header(packed_reply_header, reply);
	for (i = 0; i < reply->payload.size; i++)
		reply->payload.fields[i] = mailbox_pop_from_fifo();

	mlb_write(MB_HOST_OUT_CONTROL_OFFSET, 1);

	return MAILBOX_SUCCESS;
}

static mailbox_reply_t mailbox_request(const mailbox_request_t *request)
{
	mailbox_reply_t reply;
	uint32_t error = 0;
	uint32_t packed_header;
	unsigned int i;

	memset(&reply, 0, sizeof(reply));

	if (request == NULL) {
		error = MAILBOX_INVALID_PARAMETER;
	} else {
		packed_header = mailbox_pack_request_header(request);
		mailbox_print_request(request);

		error = mailbox_wait_for_scz_to_be_ready();
		reply.status.service_error = error;
		if (error != MAILBOX_SUCCESS) {
			pr_err(PFX "%s: poll mailbox for scz failed\n", __func__);
			return reply;
		}

		mailbox_push_to_fifo(packed_header);
		for (i = 0; i < request->payload.size; i++)
			mailbox_push_to_fifo(request->payload.fields[i]);
		mlb_write(MB_HOST_IN_CONTROL_OFFSET, 1);
		error = mailbox_check_safety(packed_header, request);
	}

	reply.status.service_error = error;
	if (error == MAILBOX_SUCCESS)
		error = mailbox_get_reply(&reply);
	else
		mailbox_flush_out_fifos();

	mailbox_print_reply(&reply);

	return reply;
}

static void mailbox_init_request(mailbox_request_t *request,
	request_type_enum request_type, uint8_t service_id, uint8_t service_version)
{
	if (request == NULL)
		return;

	request->request_type = request_type;
	request->service_id = service_id;
	request->service_version = service_version;
	request->payload.size = 0;
}

static uint32_t mailbox_request_add_field(mailbox_request_t *request,
	uint32_t payload_field)
{
	if (request == NULL)
		return MAILBOX_INVALID_PARAMETER;

	if (request->payload.size >= MAILBOX_PAYLOAD_FIELDS_COUNT)
		return MAILBOX_INVALID_SIZE;

	request->payload.fields[request->payload.size] = payload_field;
	request->payload.size++;

	return MAILBOX_SUCCESS;
}

mailbox_reply_t ise_mailbox_request(mailbox_request_t *request,
	mailbox_payload_t *payload, request_type_enum request_type,
	uint8_t service_id, uint8_t service_version)
{
	mailbox_reply_t reply = {0};
	uint32_t i;
	uint8_t status;

	mutex_lock(&mbox_lock);
	if (request == NULL || payload == NULL) {
		reply.status.service_error = MAILBOX_INVALID_PARAMETER;
		goto out;
	}

	if (payload->size == 0 || payload->size >= MAILBOX_PAYLOAD_FIELDS_COUNT) {
		reply.status.service_error = MAILBOX_INVALID_PARAMETER;
		goto out;
	}

	status = mailbox_init_securyzr();
	if (status != MAILBOX_SUCCESS) {
		pr_err(PFX "%s: init securyzr failed, status %u\n", __func__, status);
		reply.status.service_error = MAILBOX_TIMEOUT;
		goto out;
	}

	mailbox_init_request(request, request_type, service_id, service_version);
	for (i = 0; i < payload->size; i++)
		mailbox_request_add_field(request, payload->fields[i]);

	reply = mailbox_request(request);

out:
	mutex_unlock(&mbox_lock);
	return reply;
}
EXPORT_SYMBOL(ise_mailbox_request);

static void ise_ut(uint8_t service_id)
{
	mailbox_request_t request = {0};
	mailbox_payload_t payload = {0};
	mailbox_reply_t reply = {0};

	payload.size = 2;
	payload.fields[0] = 1;
	payload.fields[1] = 2;

	reply = ise_mailbox_request(&request, &payload, REQUEST_TRUSTY,
		service_id, TRUSTY_SR_VER_UT);
	if (reply.status.error != MAILBOX_SUCCESS)
		pr_info(PFX "%s: request mailbox failed 0x%x\n", __func__, reply.status.error);
	else if (reply.payload.fields[0] != 3)
		pr_info(PFX "%s: reply payload failed\n", __func__);
	else
		pr_info(PFX "%s: mtk ise mailbox pass\n", __func__);
}

ssize_t ise_dbg_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	char *parm_str, *cmd_str, *pinput;
	char input[32] = {0};
	long param;
	uint32_t len;
	int err;

	len = (count < (sizeof(input) - 1)) ? count : (sizeof(input) - 1);
	if (copy_from_user(input, buffer, len)) {
		pr_err(PFX "%s: copy from user failed\n", __func__);
		return -EFAULT;
	}

	input[len] = '\0';
	pinput = input;

	cmd_str = strsep(&pinput, " ");

	if (!cmd_str)
		return -EINVAL;

	parm_str = strsep(&pinput, " ");

	if (!parm_str)
		return -EINVAL;

	err = kstrtol(parm_str, 10, &param);

	if (err)
		return err;

	if (!strncmp(cmd_str, "ise_ut", sizeof("ise_ut"))) {
		if (param == TRUSTY_SR_ID_UT)
			ise_ut(TRUSTY_SR_ID_UT);
		return count;
	} else {
		return -EINVAL;
	}

	return count;
}

static const struct proc_ops ise_dbg_fops = {
	.proc_write = ise_dbg_write,
};

static int ise_mbox_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	uint32_t result;
	uint8_t status;
	int ret;

	if (IS_ERR(node)) {
		pr_err(PFX "%s: cannot find device node\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32(node, "mediatek,real-drv", &real_drv);
	if (ret || !real_drv) {
		pr_info(PFX "%s: ise mailbox dummy driver\n", __func__);
		return 0;
	}

	mbox_base = of_iomap(node, 0);
	if (unlikely(mbox_base == NULL)) {
		pr_err(PFX "%s: parse mbox base failed\n", __func__);
		return -EINVAL;
	}

	result = mailbox_get_boot_status();
	pr_info(PFX "%s: securyzr boot status 0x%x\n", __func__, result);
	status = mailbox_init_securyzr();
	pr_info(PFX "%s: init securyzr mailbox status %u\n", __func__, status);

	proc_create("ise_dbg", 0664, NULL, &ise_dbg_fops);

	mutex_init(&mbox_lock);

	return 0;
}

static int ise_mbox_remove(struct platform_device *pdev)
{
	mutex_destroy(&mbox_lock);
	return 0;
}

static const struct of_device_id ise_mbox_of_ids[] = {
	{.compatible = "mediatek,ise-mbox"},
	{}
};

static struct platform_driver ise_mbox_drv = {
	.probe = ise_mbox_probe,
	.remove = ise_mbox_remove,
	.driver = {
		.name = "mtk_ise_mbox",
		.of_match_table = ise_mbox_of_ids,
	}
};

static __init int mtk_ise_mbox_driver(void)
{
	int err = 0;

	err = platform_driver_register(&ise_mbox_drv);
	if (err) {
		pr_err(PFX "%s: register driver failed %d", __func__, err);
		return err;
	}

	return 0;
}

arch_initcall(mtk_ise_mbox_driver);
MODULE_LICENSE("GPL");
