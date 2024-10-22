#include "stm32_pogo_keyboard_v3.h"

static void release_all_key_row(struct stm32_keypad_dev *stm32);
static int keypad_set_input_dev_row(struct stm32_keypad_dev *stm32, struct pogo_data_struct pogo_data);
static void pogo_kpd_event_row(struct stm32_keypad_dev *stm32, u16 *key_values, int len);

static struct stm32_keypad_dev_function row = {
	release_all_key_row,
	keypad_set_input_dev_row,
	pogo_kpd_event_row,
};

struct stm32_keypad_dev_function *setup_stm32_keypad_dev_row(void)
{
	return &row;
}

static void release_all_key_row(struct stm32_keypad_dev *stm32)
{
	int i, j;

	if (!stm32->input_dev)
		return;

	if (!stm32->dtdata_row) {
		input_err(true, &stm32->pdev->dev, "%s: dtdata_row is null\n", __func__);
		return;
	}

	input_info(true, &stm32->pdev->dev, "%s\n", __func__);

	for (i = 0; i < stm32->dtdata_row->num_row; i++) {
		if (!stm32->key_state[i])
			continue;

		for (j = 0; j < stm32->dtdata_row->num_column; j++) {
			if (stm32->key_state[i] & (1 << j)) {
				int code = MATRIX_SCAN_CODE(i, j, STM32_KPC_ROW_SHIFT);

				input_event(stm32->input_dev, EV_MSC, MSC_SCAN, code);
				input_report_key(stm32->input_dev, stm32->keycode[code], 0);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &stm32->pdev->dev, "RA code(0x%X|%d) R:C(%d:%d)\n",
						stm32->keycode[code], stm32->keycode[code], i, j);
#else
				input_info(true, &stm32->pdev->dev, "RA (%d:%d)\n", i, j);
#endif
				stm32->key_state[i] &= ~(1 << j);
			}
		}
	}
	input_sync(stm32->input_dev);
}

static int keypad_set_input_dev_row(struct stm32_keypad_dev *stm32, struct pogo_data_struct pogo_data)
{
	struct input_dev *input_dev;
	static char name[128];
	int ret = 0;

	if (stm32->input_dev) {
		input_err(true, &stm32->pdev->dev, "%s input dev already exist.\n", __func__);
		return ret;
	}

	if (!stm32->dtdata_row) {
		input_err(true, &stm32->pdev->dev, "%s: dtdata_row is null\n", __func__);
		return -ENOMEM;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		input_err(true, &stm32->pdev->dev, "%s: Failed to allocate memory for input device\n", __func__);
		return -ENOMEM;
	}

	stm32->input_dev = input_dev;
	stm32->input_dev->dev.parent = &stm32->pdev->dev;

	if (pogo_data.model_info)
		snprintf(name, sizeof(name), "Book Cover Keyboard %s(%s)",
				pogo_data.model_info->has_touchpad ? "" : "Slim ",
				pogo_data.model_info->name);
	else
		snprintf(name, sizeof(name), INPUT_NAME_DEFAULT);
	stm32->input_dev->name = name;

	input_info(true, &stm32->pdev->dev, "%s: input_name: %s\n", __func__, stm32->input_dev->name);
	stm32->input_dev->id.bustype = BUS_I2C;
	stm32->input_dev->id.vendor = INPUT_VENDOR_ID_SAMSUNG;
	stm32->input_dev->id.product = INPUT_PRODUCT_ID_POGO_KEYBOARD;
	stm32->input_dev->flush = NULL;
	stm32->input_dev->event = NULL;

	input_set_drvdata(stm32->input_dev, stm32);
	input_set_capability(stm32->input_dev, EV_MSC, MSC_SCAN);
	set_bit(EV_KEY, stm32->input_dev->evbit);
	if (pogo_data.module_id == 1) {
		matrix_keypad_build_keymap(stm32->dtdata_row->keymap_data2, NULL, stm32->dtdata_row->num_row,
						stm32->dtdata_row->num_column, stm32->keycode, stm32->input_dev);
	} else {
		matrix_keypad_build_keymap(stm32->dtdata_row->keymap_data1, NULL, stm32->dtdata_row->num_row,
						stm32->dtdata_row->num_column, stm32->keycode, stm32->input_dev);
	}

	ret = input_register_device(stm32->input_dev);
	if (ret) {
		input_err(true, &stm32->pdev->dev, "%s: Failed to register input device\n", __func__);
		input_free_device(input_dev);
		stm32->input_dev = NULL;
		return -ENODEV;
	}

	input_info(true, &stm32->pdev->dev, "%s: done\n", __func__);
	return ret;
}
static void pogo_kpd_event_row(struct stm32_keypad_dev *stm32, u16 *key_values, int len)
{
	int i, event_count;

	if (!stm32->input_dev) {
		input_err(true, &stm32->pdev->dev, "%s: input dev is null\n", __func__);
		return;
	}

	if (!stm32->dtdata_row) {
		input_err(true, &stm32->pdev->dev, "%s: dtdata_row is null\n", __func__);
		return;
	}

	if (!key_values || !len) {
		input_err(true, &stm32->pdev->dev, "%s: no event data\n", __func__);
		return;
	}

	event_count = len / sizeof(struct stm32_keyevent_data);
	for (i = 0; i < event_count; i++) {
		struct stm32_keyevent_data_row keydata;
		int code;

		keydata.data[0] = key_values[i];
		code = MATRIX_SCAN_CODE(keydata.row, keydata.col, STM32_KPC_ROW_SHIFT);

		if (keydata.row >= stm32->dtdata_row->num_row || keydata.col >= stm32->dtdata_row->num_column)
			continue;

		if (keydata.press != STM32_KPC_DATA_PRESS) /* Release */
			stm32->key_state[keydata.row] &= ~(1 << keydata.col);
		else /* Press */
			stm32->key_state[keydata.row] |= (1 << keydata.col);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		input_info(true, &stm32->pdev->dev, "%s '%s' 0x%02X(%d) R:C(%d:%d)\n",
				keydata.press != STM32_KPC_DATA_PRESS ? "R" : "P",
				stm32->key_name[stm32->keycode[code]], stm32->keycode[code],
				stm32->keycode[code], keydata.row, keydata.col);
#else
		input_info(true, &stm32->pdev->dev, "%s\n", keydata.press != STM32_KPC_DATA_PRESS ? "R" : "P");
#endif
		input_event(stm32->input_dev, EV_MSC, MSC_SCAN, code);
		input_report_key(stm32->input_dev, stm32->keycode[code], keydata.press);
		input_sync(stm32->input_dev);
	}
	input_dbg(false, &stm32->pdev->dev, "%s--\n", __func__);
}

int stm32_keypad_parse_dt_row(struct device *dev, struct stm32_keypad_dev *stm32)
{
	struct device_node *np = dev->of_node;
	struct matrix_keymap_data *keymap_data1;
	struct matrix_keymap_data *keymap_data2;
	int ret, keymap_len1, keymap_len2, i;
	u32 *keymap1, *keymap2;
	const __be32 *map1;
	const __be32 *map2;

	stm32->dtdata_row = devm_kzalloc(dev, sizeof(struct stm32_keypad_dtdata_row), GFP_KERNEL);
	if (!stm32->dtdata_row)
		return -ENOMEM;

	ret = of_property_read_u32(np, "keypad,num-rows", &stm32->dtdata_row->num_row);
	if (ret)
		input_err(true, dev, "%s unable to get num-rows\n", __func__);

	ret = of_property_read_u32(np, "keypad,num-columns", &stm32->dtdata_row->num_column);
	if (ret)
		input_err(true, dev, "%s unable to get num-columns\n", __func__);

	map1 = of_get_property(np, "linux,keymap1", &keymap_len1);
	if (!map1)
		input_err(true, dev, "%s Keymap not specified\n", __func__);

	map2 = of_get_property(np, "linux,keymap2", &keymap_len2);
	if (!map2)
		input_err(true, dev, "%s Keymap not specified\n", __func__);

	keymap_data1 = devm_kzalloc(dev, sizeof(*keymap_data1), GFP_KERNEL);
	if (!keymap_data1)
		return -ENOMEM;

	keymap_data2 = devm_kzalloc(dev, sizeof(*keymap_data2), GFP_KERNEL);
	if (!keymap_data2)
		return -ENOMEM;

	keymap_data1->keymap_size = (unsigned int)(keymap_len1 / sizeof(u32));
	keymap_data2->keymap_size = (unsigned int)(keymap_len2 / sizeof(u32));

	keymap1 = devm_kzalloc(dev, sizeof(uint32_t) * keymap_data1->keymap_size, GFP_KERNEL);
	if (!keymap1)
		return -ENOMEM;

	keymap2 = devm_kzalloc(dev, sizeof(uint32_t) * keymap_data2->keymap_size, GFP_KERNEL);
	if (!keymap2)
		return -ENOMEM;

	for (i = 0; i < keymap_data1->keymap_size; i++) {
		unsigned int key = be32_to_cpup(map1 + i);
		int keycode, row, col;

		row = (key >> 24) & 0xff;
		col = (key >> 16) & 0xff;
		keycode = key & 0xffff;
		keymap1[i] = KEY(row, col, keycode);
	}

	for (i = 0; i < keymap_data2->keymap_size; i++) {
		unsigned int key2 = be32_to_cpup(map2 + i);
		int keycode2, row2, col2;

		row2 = (key2 >> 24) & 0xff;
		col2 = (key2 >> 16) & 0xff;
		keycode2 = key2 & 0xffff;
		keymap2[i] = KEY(row2, col2, keycode2);
	}

	/* short keymap for easy finding key */
	snprintf(stm32->key_name[KEY_1], 10, "1");
	snprintf(stm32->key_name[KEY_2], 10, "2");
	snprintf(stm32->key_name[KEY_3], 10, "3");
	snprintf(stm32->key_name[KEY_4], 10, "4");
	snprintf(stm32->key_name[KEY_5], 10, "5");
	snprintf(stm32->key_name[KEY_6], 10, "6");
	snprintf(stm32->key_name[KEY_7], 10, "7");
	snprintf(stm32->key_name[KEY_8], 10, "8");
	snprintf(stm32->key_name[KEY_9], 10, "9");
	snprintf(stm32->key_name[KEY_0], 10, "0");
	snprintf(stm32->key_name[KEY_MINUS], 10, "-");
	snprintf(stm32->key_name[KEY_EQUAL], 10, "=");
	snprintf(stm32->key_name[KEY_BACKSPACE], 10, "Backspc");
	snprintf(stm32->key_name[KEY_TAB], 10, "Tab");
	snprintf(stm32->key_name[KEY_Q], 10, "q");
	snprintf(stm32->key_name[KEY_W], 10, "w");
	snprintf(stm32->key_name[KEY_E], 10, "e");
	snprintf(stm32->key_name[KEY_R], 10, "r");
	snprintf(stm32->key_name[KEY_T], 10, "t");
	snprintf(stm32->key_name[KEY_Y], 10, "y");
	snprintf(stm32->key_name[KEY_U], 10, "u");
	snprintf(stm32->key_name[KEY_I], 10, "i");
	snprintf(stm32->key_name[KEY_O], 10, "o");
	snprintf(stm32->key_name[KEY_P], 10, "p");
	snprintf(stm32->key_name[KEY_LEFTBRACE], 10, "[");
	snprintf(stm32->key_name[KEY_RIGHTBRACE], 10, "]");
	snprintf(stm32->key_name[KEY_ENTER], 10, "Enter");
	snprintf(stm32->key_name[KEY_LEFTCTRL], 10, "L CTRL");
	snprintf(stm32->key_name[KEY_A], 10, "a");
	snprintf(stm32->key_name[KEY_S], 10, "s");
	snprintf(stm32->key_name[KEY_D], 10, "d");
	snprintf(stm32->key_name[KEY_F], 10, "f");
	snprintf(stm32->key_name[KEY_G], 10, "g");
	snprintf(stm32->key_name[KEY_H], 10, "h");
	snprintf(stm32->key_name[KEY_J], 10, "j");
	snprintf(stm32->key_name[KEY_K], 10, "k");
	snprintf(stm32->key_name[KEY_L], 10, "l");
	snprintf(stm32->key_name[KEY_SEMICOLON], 10, ";");
	snprintf(stm32->key_name[KEY_APOSTROPHE], 10, "\'");
	snprintf(stm32->key_name[KEY_GRAVE], 10, "`");
	snprintf(stm32->key_name[KEY_LEFTSHIFT], 10, "L Shift");
	snprintf(stm32->key_name[KEY_BACKSLASH], 10, "\\"); /* US : backslash , UK : pound*/
	snprintf(stm32->key_name[KEY_Z], 10, "z");
	snprintf(stm32->key_name[KEY_X], 10, "x");
	snprintf(stm32->key_name[KEY_C], 10, "c");
	snprintf(stm32->key_name[KEY_V], 10, "v");
	snprintf(stm32->key_name[KEY_B], 10, "b");
	snprintf(stm32->key_name[KEY_N], 10, "n");
	snprintf(stm32->key_name[KEY_M], 10, "m");
	snprintf(stm32->key_name[KEY_COMMA], 10, ",");
	snprintf(stm32->key_name[KEY_DOT], 10, ".");
	snprintf(stm32->key_name[KEY_SLASH], 10, "/");
	snprintf(stm32->key_name[KEY_RIGHTSHIFT], 10, "R Shift");
	snprintf(stm32->key_name[KEY_LEFTALT], 10, "L ALT");
	snprintf(stm32->key_name[KEY_SPACE], 10, " ");
	snprintf(stm32->key_name[KEY_CAPSLOCK], 10, "CAPSLOCK");
	snprintf(stm32->key_name[KEY_102ND], 10, "\\"); /* only UK : backslash */
	snprintf(stm32->key_name[KEY_RIGHTALT], 10, "R ALT");
	snprintf(stm32->key_name[KEY_UP], 10, "Up");
	snprintf(stm32->key_name[KEY_LEFT], 10, "Left");
	snprintf(stm32->key_name[KEY_RIGHT], 10, "Right");
	snprintf(stm32->key_name[KEY_DOWN], 10, "Down");
	snprintf(stm32->key_name[KEY_HANGEUL], 10, "Lang");
	snprintf(stm32->key_name[KEY_LEFTMETA], 10, "L Meta");
	snprintf(stm32->key_name[KEY_NUMERIC_POUND], 10, "#");
	snprintf(stm32->key_name[706], 10, "SIP"); /* SIP_ON_OFF */
	snprintf(stm32->key_name[710], 10, "Sfinder"); /* SIP_ON_OFF */
	snprintf(stm32->key_name[KEY_ESC], 10, "ESC");
	snprintf(stm32->key_name[KEY_FN], 10, "Fn");
	snprintf(stm32->key_name[KEY_F1], 10, "F1");
	snprintf(stm32->key_name[KEY_F2], 10, "F2");
	snprintf(stm32->key_name[KEY_F3], 10, "F3");
	snprintf(stm32->key_name[KEY_F4], 10, "F4");
	snprintf(stm32->key_name[KEY_F5], 10, "F5");
	snprintf(stm32->key_name[KEY_F6], 10, "F6");
	snprintf(stm32->key_name[KEY_F7], 10, "F7");
	snprintf(stm32->key_name[KEY_F8], 10, "F8");
	snprintf(stm32->key_name[KEY_F9], 10, "F9");
	snprintf(stm32->key_name[KEY_F10], 10, "F10");
	snprintf(stm32->key_name[KEY_F11], 10, "F11");
	snprintf(stm32->key_name[KEY_F12], 10, "F12");
	snprintf(stm32->key_name[KEY_DEX_ON], 10, "DEX");
	snprintf(stm32->key_name[KEY_DELETE], 10, "DEL");
	snprintf(stm32->key_name[KEY_HOMEPAGE], 10, "HOME");
	snprintf(stm32->key_name[KEY_END], 10, "END");
	snprintf(stm32->key_name[KEY_PAGEUP], 10, "PgUp");
	snprintf(stm32->key_name[KEY_PAGEDOWN], 10, "PgDown");
	snprintf(stm32->key_name[KEY_TOUCHPAD_ON], 10, "Tpad ON");
	snprintf(stm32->key_name[KEY_TOUCHPAD_OFF], 10, "Tpad OFF");
	snprintf(stm32->key_name[KEY_VOLUMEDOWN], 10, "VOL DOWN");
	snprintf(stm32->key_name[KEY_VOLUMEUP], 10, "VOL UP");
	snprintf(stm32->key_name[KEY_MUTE], 10, "VOL MUTE");
	snprintf(stm32->key_name[705], 10, "APPS"); //apps
	snprintf(stm32->key_name[KEY_BRIGHTNESSDOWN], 10, "B-DOWN");
	snprintf(stm32->key_name[KEY_BRIGHTNESSUP], 10, "B-UP");
	snprintf(stm32->key_name[KEY_PLAYPAUSE], 10, "PLAYPAUSE");
	snprintf(stm32->key_name[KEY_NEXTSONG], 10, "NEXT");
	snprintf(stm32->key_name[KEY_PREVIOUSSONG], 10, "PREV");
	snprintf(stm32->key_name[KEY_SYSRQ], 10, "SYSRQ");
	snprintf(stm32->key_name[709], 10, "SETTING");
	snprintf(stm32->key_name[713], 10, "KBD SHARE");
	snprintf(stm32->key_name[714], 10, "FN LOCK");
	snprintf(stm32->key_name[715], 10, "FN UNLOCK");

	keymap_data1->keymap = keymap1;
	stm32->dtdata_row->keymap_data1 = keymap_data1;
	keymap_data2->keymap = keymap2;
	stm32->dtdata_row->keymap_data2 = keymap_data2;

	stm32->keycode = devm_kzalloc(dev,
				(stm32->dtdata_row->num_row << STM32_KPC_ROW_SHIFT) * sizeof(unsigned short),
				GFP_KERNEL);
	if (!stm32->keycode)
		return -ENOMEM;

	input_info(true, dev,
			"row:%d, col:%d, keymap1 size:%d keymap2 size:%d\n",
			stm32->dtdata_row->num_row, stm32->dtdata_row->num_column,
			stm32->dtdata_row->keymap_data1->keymap_size,
			stm32->dtdata_row->keymap_data2->keymap_size);

	return 0;
}
