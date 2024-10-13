#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/hid.h>

#include "hid-ids.h"

static bool hid_pogppin_match(struct hid_device *hdev,bool ignore_special_driver)
{
	return true;
}

static int hid_pogopin_probe(struct hid_device *hdev,const struct hid_device_id *id)
{
	int ret;

	hdev->quirks |= HID_QUIRK_INPUT_PER_APP;

	ret = hid_parse(hdev);
	if (ret){
		hid_err(hdev, "%s: parse failed\n", __func__);
		return ret;
	}

	return 0;
}

static const struct hid_device_id hid_pogopin_table[] = {
	{ HID_DEVICE(HID_BUS_ANY, HID_GROUP_ANY, I2C_VENDOR_ID_KBD, I2C_PRODUCT_ID_KBD) },
	{ }
};
MODULE_DEVICE_TABLE(hid, hid_pogopin_table);

static struct hid_driver hid_pogppin = {
	.name = "hid_pogppin",
	.id_table = hid_pogopin_table,
	.match = hid_pogppin_match,
	.probe = hid_pogopin_probe,
};
module_hid_driver(hid_pogppin);

MODULE_AUTHOR("KeyBoard");
MODULE_DESCRIPTION("Pogopin HID driver");
MODULE_LICENSE("GPL");
