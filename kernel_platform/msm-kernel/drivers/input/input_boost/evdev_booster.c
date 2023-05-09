#define ITAG " [Evdev Booster] "
#include <linux/input/input_booster.h>

#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/syscalls.h>

#if IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER)
spinlock_t ib_ev_lock;
struct workqueue_struct *ev_unbound_wq;
static struct device *evbst_dev;

void input_booster(struct ib_event_data *ib_ev_data);

int evdev_ib_notify_callback(struct notifier_block *nb,
	unsigned long event_type, void *data)
{
	if (ib_init_succeed) {
		spin_lock(&ib_ev_lock);
		struct ib_event_data *ib_ev_data = data;

		switch (event_type) {
		case IB_EVENT_TOUCH_BOOSTER:
			input_booster(ib_ev_data);
			break;
		default:
			break;
		}
		spin_unlock(&ib_ev_lock);
	}

	return 0;
}

static struct notifier_block ib_event_notifier = {
	 .notifier_call = evdev_ib_notify_callback,
	 .priority = 1,
};

int chk_next_data(const struct input_value *vals, int next_idx, int input_type)
{
	int ret_val = 0;
	int next_type = -1;
	int next_code = -1;

	next_type = vals[next_idx].type;
	next_code = vals[next_idx].code;

	switch (input_type) {
	case BTN_TOUCH:
		if ((next_type == EV_ABS) && (next_code == ABS_PRESSURE))
			ret_val = 1;
		break;
	case EV_KEY:
		ret_val = 1;
		break;
	default:
		break;
	}

	return ret_val;
}

int chk_boost_on_off(const struct input_value *vals, int idx, int dev_type)
{
	int ret_val = -1;

	if (dev_type < 0)
		return ret_val;

	/* In case of SPEN or HOVER, it must be empty multi event
	 * Before starting input booster.
	 */
	if (dev_type == SPEN || dev_type == HOVER) {
		if (!evdev_mt_event[dev_type] && vals[idx].value)
			ret_val = 1;
		else if (evdev_mt_event[dev_type] && !vals[idx].value)
			ret_val = 0;
	} else if (dev_type == TOUCH || dev_type == MULTI_TOUCH) {
		if (vals[idx].value >= 0)
			ret_val = 1;
		else
			ret_val = 0;
	} else if (vals[idx].value > 0)
		ret_val = 1;
	else if (vals[idx].value <= 0)
		ret_val = 0;

	return ret_val;
}

/*
 * get_device_type : Define type of device for input_booster.
 * device_type : Current device that in which input events triggered.
 * keyId : Each device get given unique keyid using Type, Code, Slot values
 *  to identify which booster will be triggered.
 * cur_idx : Pointing current handling index from input booster.
 *  cur_idx will be updated when cur_idx is same as head.
 * vals : Event set to determine what booster would be triggered.
 * evt_cnt : Total count of event in this time.
 */
int get_device_type(int *device_type, unsigned int *keyId,
					int *cur_idx, const struct input_value *vals, int evt_cnt)
{
	int i;
	int ret_val = -1;
	int dev_type = NONE_TYPE_DEVICE;
	int uniq_slot = 0;
	int next_idx = 0 ;
	int target_idx = 0;

	if (evt_cnt > MAX_EVENTS) {
		dev_warn_ratelimited(evbst_dev, "Exceed max event number\n");
		return ret_val;
	}

	/* Initializing device type before finding the proper device type. */
	*device_type = dev_type;

	for (i = *cur_idx; i < evt_cnt; i++) {
		pr_booster(" %s Type : %d, Code : %d, Value : %d, idx : %d\n",
			"Input Data || ", vals[i].type,
			vals[i].code, vals[i].value, i);

		if (vals[i].type == EV_SYN || vals[i].code == SYN_REPORT) {
			break;
		}

		if (vals[i].type == EV_KEY) {
			target_idx = i;
			switch (vals[i].code) {
			case BTN_TOUCH:
				next_idx = i+1;
				if (!chk_next_data(vals, next_idx, BTN_TOUCH))
					break;
				dev_type = SPEN;
				break;
			case BTN_TOOL_PEN:
				dev_type = HOVER;
				break;
			case KEY_BACK:
			case KEY_HOMEPAGE:
				dev_type = TOUCH_KEY;
				break;
			case KEY_VOLUMEUP:
			case KEY_VOLUMEDOWN:
			case KEY_POWER:
				dev_type = KEY;
				break;
			default:
				break;
			}

		} else if (vals[i].type == EV_ABS) {
			target_idx = i;
			switch (vals[i].code) {
			case ABS_MT_TRACKING_ID:
				if (vals[i].value >= 0) {
					evdev_mt_slot++;
				} else {
					evdev_mt_slot--;
					if (evdev_mt_slot < 0) {
						evdev_mt_slot = 0;
						break;
					}
				}

				if (vals[i].value >= 0) {
					if (evdev_mt_slot == 1) {
						dev_type = TOUCH;
						uniq_slot = 1;
					} else if (evdev_mt_slot == 2) {
						dev_type = MULTI_TOUCH;
						uniq_slot = 2;
					}
				} else if (vals[i].value < 0) {
					if (evdev_mt_slot == 0) {
						dev_type = TOUCH;
						uniq_slot = 1;
					} else if (evdev_mt_slot == 1) {
						dev_type = MULTI_TOUCH;
						uniq_slot = 2;
					}
				}

				pr_booster("Touch Booster Trigger(%d), T(%d) C(%d) V(%d), Idx(%d), Cnt(%d)",
							evdev_mt_slot,
							vals[i].type, vals[i].code, vals[i].value,
							i, evt_cnt);

				break;
			}
		} else if (vals[i].type == EV_MSC &&
					vals[i].code == MSC_SCAN) {
			next_idx = i++;
			if (!chk_next_data(vals, next_idx, EV_KEY))
				break;

			next_idx = i++;
			target_idx = next_idx;
			switch (vals[next_idx].code) {
			case BTN_LEFT: /* Checking Touch Button Event */
			case BTN_RIGHT:
			case BTN_MIDDLE:
				dev_type = MOUSE;
				//Remain the last of CODE value as a uniq_slot to recognize BTN Type (LEFT, RIGHT, MIDDLE)
				uniq_slot = vals[next_idx].code;
				break;
			default: /* Checking Keyboard Event */
				dev_type = KEYBOARD;
				uniq_slot = vals[next_idx].code;

				pr_booster("KBD Booster Trigger(%d), Type(%d), Code(%d), Val(%d), Idx(%d), Cnt(%d)\n",
						vals[next_idx].code,
						vals[i].type, vals[i].code, vals[i].value,
						i, evt_cnt);
				break;
			}
		}

		if (dev_type != NONE_TYPE_DEVICE ) {
			*keyId = create_uniq_id(vals[i].type, vals[i].code, uniq_slot);
			ret_val = chk_boost_on_off(vals, target_idx, dev_type);
			pr_booster("Dev type Find(%d), KeyID(%d), enable(%d), Target(%d)\n",
				dev_type, *keyId, ret_val, target_idx);
			break;
		}

	}

	*cur_idx = i+1;

	*device_type = dev_type;
	return ret_val;
}

// ********** Detect Events ********** //
//void input_booster(int head, int bufsize)
void input_booster(struct ib_event_data *ib_ev_data)
{
#if IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER_QC) || \
	IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER_SLSI) || \
	IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER_MTK)
	int dev_type = 0;
	int keyId = 0;
	int cur_idx = -1;

	pr_booster("%s Triggered :: evt_cnt(%d), ib_init_succeed(%d)",
		__func__, ib_ev_data->evt_cnt, ib_init_succeed);

	if (!ib_init_succeed || ib_ev_data->evt_cnt == 0) {
		pr_err(ITAG"ev_cnt(%d) dev is Null OR dt_infor hasn't mem alloc", ib_ev_data->evt_cnt);
		return;
	}

	cur_idx = 0;

	while (cur_idx < ib_ev_data->evt_cnt) {
		
		keyId = 0;

		int enable = get_device_type(&dev_type, &keyId, &cur_idx, ib_ev_data->vals, ib_ev_data->evt_cnt);
		if (enable < 0 || keyId == 0) {
			continue;
		}

		if (dev_type <= NONE_TYPE_DEVICE || dev_type >= MAX_DEVICE_TYPE_NUM) {
			continue;
		}

		if (enable == BOOSTER_ON) {
			evdev_mt_event[dev_type]++;
		} else {
			evdev_mt_event[dev_type]--;
		}

		pr_booster("Device_Type(%d), KeyId(%d), IB_Cnt(%d), enable(%d)",
			dev_type, keyId, trigger_cnt, enable);

		ib_trigger[trigger_cnt].key_id = keyId;
		ib_trigger[trigger_cnt].event_type = enable;
		ib_trigger[trigger_cnt].dev_type = dev_type;

		queue_work(ev_unbound_wq, &(ib_trigger[trigger_cnt++].ib_trigger_work));
		trigger_cnt = (trigger_cnt == MAX_IB_COUNT) ? 0 : trigger_cnt;
	}
#endif
}

static int __init ev_boost_init(void)
{
	int err;

	pr_info(ITAG" Input Booster Module Init\n");
	input_booster_init();
	pr_info(ITAG" Input Booster Module Init End\n");
	spin_lock_init(&ib_ev_lock);
	ib_notifier_register(&ib_event_notifier);
	ev_unbound_wq =
		alloc_ordered_workqueue("ev_unbound_wq", WQ_HIGHPRI);

	evbst_dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	dev_set_name(evbst_dev, "evdev_booster_dev");
	evbst_dev->release = NULL;
	err = device_register(evbst_dev);
	if (err)
		pr_err(ITAG" evdev device register failed");

	return 0;
}

static void __exit ev_boost_exit(void)
{
	ib_notifier_unregister(&ib_event_notifier);
	device_unregister(evbst_dev);
	kfree(evbst_dev);
	input_booster_exit();
}

late_initcall(ev_boost_init);
module_exit(ev_boost_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hongc.shim");
MODULE_DESCRIPTION("EVDEV Booster in kernel");
#endif //--CONFIG_SEC_INPUT_BOOSTER
