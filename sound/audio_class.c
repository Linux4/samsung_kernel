#include <linux/device.h>
#include <linux/module.h>

struct class* audio_class;

static int __init sprd_audio_class_init(void)
{
    int ret;

    audio_class = class_create(THIS_MODULE, "audio");
    if (ret < 0) {
        pr_err("%s: register audio class failed, ret = %d\n", __func__, ret);
    }

    return ret;
}

static void __exit sprd_audio_class_exit(void)
{
    class_unregister(audio_class);
}

module_init(sprd_audio_class_init);
module_exit(sprd_audio_class_exit);

MODULE_DESCRIPTION("Spreadtrum Audio driver");
MODULE_LICENSE("GPL");
