#include <sound/audio-sipc.h>

#define COMPR_WAIT_FOREVER AUDIO_SIPC_WAIT_FOREVER
static int compr_ch_open(uint8_t dst, int timeout)
{
	int ret = 0;
	if (dst >= SIPC_ID_NR) {
		pr_info("%s, invalid dst:%d \n", __func__, dst);
		return -EINVAL;
	}
	ret = snd_audio_sipc_ch_open(dst, SMSG_CH_MP3_OFFLOAD, timeout);
	if (ret != 0) {
		printk(KERN_ERR "%s, Failed to open channel \n", __func__);
		return -EIO;
	}
	return 0;
}

static int compr_ch_close(uint8_t dst, int timeout)
{
	int ret = 0;

	if (dst >= SIPC_ID_NR) {
		pr_info("%s, invalid dst:%d \n", __func__, dst);
		return -EINVAL;
	}
	ret = snd_audio_sipc_ch_close(dst, SMSG_CH_MP3_OFFLOAD, timeout);
	if (ret != 0) {
		printk(KERN_ERR "%s, Failed to close channel \n", __func__);
		return -EIO;
	}
	return 0;
}

static int compr_send_cmd(uint32_t cmd, void *para, int32_t para_size)
{
	int ret = 0;

	ret = snd_audio_send_cmd(SMSG_CH_MP3_OFFLOAD, -1, -1, cmd,
		para, para_size, COMPR_WAIT_FOREVER);

	if (ret < 0) {
		printk(KERN_ERR "%s, Failed to send cmd(0x%x), ret(%d) \n", __func__, cmd, ret);
		return -EIO;
	}
	return 0;
}

static int compr_recv_cmd(uint32_t dst, uint32_t cmd, int32_t timeout)
{
	int ret = 0;
	uint32_t ret_val = 0;

	ret = snd_audio_recv_cmd(SMSG_CH_MP3_OFFLOAD, cmd, &ret_val, timeout);
	if (ret < 0) {
		printk(KERN_ERR "%s, Failed to send cmd(0x%x), ret(%d) \n", __func__, cmd, ret);
		return -EIO;
	}
	return 0;
}
