/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif

#include "audio-pipe.h"

#include <linux/interrupt.h>
#include <sound/audio-sipc.h>

#include <sound/sprd_memcpy_ops.h>

static int audio_sipc_channel_open(uint8_t dst, uint16_t channel, int32_t timeout);
static int audio_sipc_channel_close(uint8_t dst, uint16_t channel, int32_t timeout);
static int audio_pipe_recv_cmd(uint32_t dst, uint16_t channel, uint32_t cmd,
	struct smsg *msg, int32_t timeout);
static int audio_sipc_send_cmd(uint32_t dst,
			uint16_t channel, uint32_t cmd,
			uint32_t value0, uint32_t value1,
			uint32_t value2, int32_t value3,
			int32_t timeout);

struct audio_pipe_device {
	struct audio_pipe_init_data	*init;
	int			major;
	int			minor;
	struct cdev		cdev;
};

struct audio_pipe_sbuf {
	uint8_t		dst;
	uint16_t    curChannel;
	uint8_t     is_opened;// 0 not opened ,1 opened
	int 		timeout;
	int 		minor;
};

static struct class		*audio_pipe_class;

static int audio_pipe_open(struct inode *inode, struct file *filp)
{
	int minor = iminor(filp->f_path.dentry->d_inode);
	struct audio_pipe_device *audio_pipe;
	struct audio_pipe_sbuf *audio_buf;
	int ret = 0;
	int timeout = -1;
	audio_pipe = container_of(inode->i_cdev, struct audio_pipe_device, cdev);
	pr_info("%s, minor = %d, audio_pipe->minor =%d\n", __func__, minor, audio_pipe->minor);
	audio_buf = kzalloc(sizeof(struct audio_pipe_sbuf), GFP_KERNEL);
	if (!audio_buf) {
		return -ENOMEM;
	}
	filp->private_data   = audio_buf;
	audio_buf->dst 		 = audio_pipe->init->dst;
	audio_buf->is_opened = 0;
	audio_buf->timeout   = -1;
	audio_buf->minor = audio_pipe->minor;
//	audio_buf->channel[audio_buf->minor] = 2 + audio_buf->minor;
	audio_buf->curChannel = SMSG_CH_DSP_ASSERT_CTL;
	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
	}
	ret = audio_sipc_channel_open(audio_buf->dst, audio_buf->curChannel, timeout);
	if(0 == ret){
		audio_buf->is_opened = 1;
		//audio_buf->channel	 = user_msg_in->channel;
		pr_err("channel opened %hhu\n",audio_buf->curChannel );
	}else{
		pr_err("%s audio_sipc_channel_open failed open\n", __func__);
		if (audio_buf) {
		kfree(audio_buf);
		audio_buf = NULL;
		}
	}
	#if 0
		audio_sipc_send_cmd(audio_buf->dst, user_msg_in->channel, user_msg_in->command,
		user_msg_in->parameter0, user_msg_in->parameter1, user_msg_in->parameter2, user_msg_in->parameter3, timeout);
	#endif

	return 0;
}

static int audio_pipe_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct audio_pipe_sbuf *audio_buf = filp->private_data;
	int timeout = -1;
	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
		pr_info("filp->f_flags & O_NONBLOCK=%u,timeout = %d\n", filp->f_flags & O_NONBLOCK, timeout);
	}
	if (audio_buf->dst>= SIPC_ID_NR) {
	pr_info("%s, invalid dst:%d \n", __func__, audio_buf->dst);
	ret = -EINVAL;
	}
	ret = saudio_ch_close(audio_buf->dst, audio_buf->curChannel,timeout);
	if (ret != 0) {
		printk(KERN_ERR "%s, Failed to close channel \n", __func__);
	}
	if (audio_buf) {
		kfree(audio_buf);
		audio_buf = NULL;
	}
	return ret;
}

/*
 * channel number from user space
*/
static ssize_t audio_pipe_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{

	struct audio_pipe_sbuf *audio_buf = filp->private_data;
	int timeout 				 = -1;
//	struct smsg *user_msg_in 	 = NULL;
	struct smsg  user_msg_out 	 = {0};
	int ret 					 = 0;
	pr_err("sizeof(struct smsg)=%d\n", sizeof(struct smsg));
	if(sizeof(struct smsg) != count){
		pr_err("input not a struct smsg type\n");
		ret = -EINVAL;
		goto error;
	}
	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
		pr_info("filp->f_flags & O_NONBLOCK=%u,timeout = %d\n", filp->f_flags & O_NONBLOCK, timeout);
	}
	#if 0
	user_msg_in = kzalloc(sizeof(struct smsg), GFP_KERNEL);
	if(NULL == user_msg_in){
		printk(KERN_ERR "%s failed allocate memery\n", __func__);
		ret = -ENOMEM;
		goto error;
	}
	if(unalign_copy_from_user(user_msg_in, (void __user *)buf, sizeof(struct smsg))){
		pr_err("%s failed unalign_copy_from_user\n", __func__);
		ret = -EFAULT;
		goto error;
	}
	pr_err("user_msg_in.channel=%#hx, user_msg_in.command=%#hx, user_msg_in.parameter0=%#x, user_msg_in.parameter1=%#x,user_msg_in.parameter2=%#x, user_msg_in.parameter3=%#x\n", 
		user_msg_in->channel, user_msg_in->command, user_msg_in->parameter0, user_msg_in->parameter1,
		user_msg_in->parameter2, user_msg_in->parameter3);
	#endif
	ret = audio_pipe_recv_cmd(audio_buf->dst, audio_buf->curChannel, 0,
		&user_msg_out, timeout);
	if(0 == ret){//return read size(byte)
		ret = sizeof(struct smsg);
	}
	if (unalign_copy_to_user((void __user *)buf, &user_msg_out, sizeof(struct smsg))) {
		printk(KERN_ERR "%s: failed to copy to user!\n", __func__);
		ret = -EFAULT;
		goto error;
	}
	pr_err("user_msg_out.channel=%#hx, user_msg_out.command=%#hx, user_msg_out.parameter0=%#x, user_msg_out.parameter1=%#x,user_msg_out.parameter2=%#x, user_msg_out.parameter3=%#x\n", 
		user_msg_out.channel, user_msg_out.command, user_msg_out.parameter0, user_msg_out.parameter1,
		user_msg_out.parameter2, user_msg_out.parameter3);
success:
	#if 0
	if(user_msg_in){
		kfree(user_msg_in);
		user_msg_in = NULL;
	}
	#endif
	return ret;
error:
	pr_err("%s failed", __func__);
	#if 0
	if(1 == audio_buf->is_opened){
		audio_sipc_channel_close(audio_buf->dst, user_msg_in->channel, timeout);
		audio_buf->is_opened = 0;
		pr_err("audio_sipc_channel_closed audio_buf->dst=%#hhu, user_msg_in->channel=%#hu\n", audio_buf->dst, user_msg_in->channel);
	}
	#endif
	#if 0
	if(user_msg_in){
		kfree(user_msg_in);
		user_msg_in = NULL;
	}
	#endif
	if(ret < 0){
		ret = -1;
	}
	return ret;
}

static ssize_t audio_pipe_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct smsg  user_msg_from_user 	 = {0};
	int ret = 0;
	struct audio_pipe_sbuf *audio_buf = filp->private_data;
	int timeout = -1;
	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
		pr_info("filp->f_flags & O_NONBLOCK=%u,timeout = %d\n", filp->f_flags & O_NONBLOCK, timeout);
	}
	if(unalign_copy_from_user(&user_msg_from_user, (void __user *)buf, sizeof(struct smsg))){
		pr_err("%s failed unalign_copy_from_user\n", __func__);
		ret = -EFAULT;
		goto error;
	}
	pr_err("user_msg_in.channel=%#hx, user_msg_in.command=%#hx, user_msg_in.parameter0=%#x, user_msg_in.parameter1=%#x,user_msg_in.parameter2=%#x, user_msg_in.parameter3=%#x\n", 
		user_msg_from_user.channel, user_msg_from_user.command, user_msg_from_user.parameter0, user_msg_from_user.parameter1,
		user_msg_from_user.parameter2, user_msg_from_user.parameter3);
	audio_sipc_send_cmd(audio_buf->dst,audio_buf->curChannel, user_msg_from_user.command,
			user_msg_from_user.parameter0, user_msg_from_user.parameter1, user_msg_from_user.parameter2, user_msg_from_user.parameter3, timeout);
	return 0;
error:
	pr_err("%s failed", __func__);
	return ret;
}

static unsigned int audio_pipe_poll(struct file *filp, poll_table *wait)
{
	//nothing todo
	pr_err("audio_pipe_poll not implemented");

	return 0;
}

static long audio_pipe_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	//nothing todo
	pr_err("audio_pipe_ioctl not implemented");

	return 0;
}

static int audio_sipc_channel_open(uint8_t dst, uint16_t channel, int32_t timeout)
{
	int ret = 0;
	if (dst >= SIPC_ID_NR) {
		pr_info("%s, invalid dst:%d \n", __func__, dst);
		ret = -EINVAL;
		goto error;
	}
	ret = saudio_ch_open(dst, channel, timeout);
	if (ret != 0) {
		printk(KERN_ERR "%s, Failed to open channel \n", __func__);
		goto error;
	}
	return 0;
error:
	return ret;
}

static int audio_sipc_channel_close(uint8_t dst, uint16_t channel, int32_t timeout)
{
	int ret = 0;
	if (dst>= SIPC_ID_NR) {
		pr_info("%s, invalid dst:%d \n", __func__, dst);
		ret = -EINVAL;
	}
	ret = saudio_ch_close(dst, channel,timeout);
	if (ret != 0) {
		printk(KERN_ERR "%s, Failed to close channel \n", __func__);
	}
	return 0;
error:
	return ret;
}

/*
* returns:
* 0 if sucess
*/
static int audio_pipe_recv_cmd(uint32_t dst, uint16_t channel, uint32_t cmd,
			struct smsg *msg, int32_t timeout)
{
	int ret = 0;
	struct smsg mrecv = { 0 };
	if (dst >= SIPC_ID_NR) {
		pr_info("%s, invalid dst:%d \n", __func__, dst);
		ret = -EINVAL;
		goto error;
	}
	smsg_set(&mrecv, channel, 0, 0, 0, 0, 0);
    #if 0
    while(-ERESTARTSYS == (ret = saudio_recv(dst, &mrecv, timeout))) {
            pr_err("%s,interrupted so recv again pipe\n", __func__);
    }
    #else
    ret = saudio_recv(dst, &mrecv, timeout);
    #endif
	if (ret < 0) {
		printk(KERN_ERR "%s, Failed to recv, dst:(%d), ret(%d) \n", __func__, dst, ret);
		goto error;
	}
	pr_info("%s, dst: %d, chan: 0x%x, cmd: 0x%x, value0: 0x%x, value1: 0x%x, value2: 0x%x, value3: 0x%x, timeout: 0x%x \n",
			__func__, dst, mrecv.channel, mrecv.command, mrecv.parameter0, mrecv.parameter1, mrecv.parameter2, mrecv.parameter3, timeout);
	//if (cmd == mrecv.command && mrecv.channel == channel) {
	if (mrecv.channel == channel) {
		memcpy(msg, &mrecv, sizeof(struct smsg));
		ret = 0;
	} else {
		printk(KERN_ERR "%s, Haven't got right, got cmd(0x%x), got chan(0x%x) \n",
				__func__, mrecv.command, mrecv.channel);
		ret = -EIO;
	}
error:
	return ret;
}


static int audio_sipc_send_cmd(uint32_t dst,
			uint16_t channel, uint32_t cmd,
			uint32_t value0, uint32_t value1,
			uint32_t value2, int32_t value3,
			int32_t timeout)
{
	int ret = 0;
	struct smsg msend = { 0 };

	if (dst >= SIPC_ID_NR) {
		pr_info("%s, invalid dst:%d \n", __func__, dst);
		ret = -EINVAL;
		goto error;
	}
	pr_info("%s, dst: %d, channel: 0x%x, cmd: 0x%x, value0: 0x%x, value1: 0x%x, value2: 0x%x, value3: 0x%x, timeout: 0x%x \n",
		__func__, dst, channel, cmd, value0, value1, value2, value3, timeout);
	smsg_set(&msend, channel, cmd, value0, value1, value2, value3);
	ret = saudio_send(dst, &msend, timeout);
	if (ret) {
		printk(KERN_ERR "%s, Failed to send cmd(0x%x), ret(%d) \n", __func__, cmd, ret);
		goto error;
	}
error:
	return ret;
}


static const struct file_operations audio_pipe_fops = {
	.open		= audio_pipe_open,
	.release	= audio_pipe_release,
	.read		= audio_pipe_read,
	.write		= audio_pipe_write,
	.poll		= audio_pipe_poll,
	.unlocked_ioctl	= audio_pipe_ioctl,
	.owner		= THIS_MODULE,
	.llseek		= default_llseek,
};

// no use
static int audio_pipe_parse_dt(struct audio_pipe_init_data **init, struct device *dev)
{
#ifdef CONFIG_OF
	struct device_node *np = dev->of_node;
	struct audio_pipe_init_data *pdata = NULL;
	int ret;
	uint32_t temp = 0;
	pdata = kzalloc(sizeof(struct audio_pipe_init_data), GFP_KERNEL);
	if (!pdata) {
		printk(KERN_ERR "Failed to allocate pdata memory\n");
		return -ENOMEM;
	}
	ret = of_property_read_string(np, "sprd,name", (const char**)&pdata->name);
	if (ret) {
		goto error;
	}
	ret = of_property_read_u8(np, "sprd,dst", &pdata->dst);
	if (ret) {
		goto error;
	}
	ret = of_property_read_u8(np, "sprd,devicesnr", &pdata->devicesnr);
	if (ret) {
		goto error;
	}
	ret = of_property_read_u32(np, "mailbox,core", &pdata->target_id);
	if (ret) {
		goto error;
	}
	*init = pdata;
	return ret;
error:
	pr_err("%s failed", __func__);
	kfree(pdata);
	*init = NULL;
	return ret;
#else
	return -ENODEV;
#endif
}

static inline void audio_pipe_destroy_pdata(struct audio_pipe_init_data **init)
{
#ifdef CONFIG_OF
	struct audio_pipe_init_data *pdata = *init;
	if (pdata) {
		kfree(pdata);
	}
	*init = NULL;
#else
	return;
#endif
}

void test_func(){
	/*result :
	* 1. sizeof(size_t) == 8, sizeof(unsigned long long int) =8, sizeof (unsigned long int)=8
	* 2.writel readl 32bit width
	*/
	// bug test sizeof
	pr_err("ninglei debug sizeof(size_t)= %d, sizeof(unsigned long long int) = %d, sizeof (unsigned long int)=%d\n", sizeof(size_t), sizeof(unsigned long long int), sizeof(unsigned long int));
	//bug test writel readl
	size_t *write_to_addr = kzalloc(sizeof(size_t), GFP_KERNEL);
	if(NULL == write_to_addr){
		printk(KERN_ERR "%s FAILED allocate memeory", __func__);
	}
	size_t read_to_addr = 0;
	if(NULL == write_to_addr){
		printk(KERN_ERR "ninglei debug Failed to alloc size_t \n");
	}
	writel(0x1122334455667788, write_to_addr);
	pr_err("ninglei debug *write_to_addr = 0x%lx\n", *write_to_addr);
	pr_err("ninglei debug readl(write_to_addr) = 0x%lx",readl(write_to_addr));
	kfree(write_to_addr);
}
#if 0
/*
* pipe function
* 1. share memery with dsp and other cpu identified by dst
* 2. you can alloc mutiple channle, all channel use the shared memery
* 3. there are one major char_dev with mutiple minor_char_dev
* 4. each minior_char_dev related with one channel
* 5.you can indicate the count of channels in dts, thus you will get the same quantity number minior_char_devs
* 6.you can use each channle for one function
*/
int audio_ipc_create(struct audio_pipe_init_data *init)// test do not use
{
	size_t audio_txaddr		= 0;
	size_t audio_rxaddr 	= 0;
	size_t audio_rwptraddr  = 0;
	uint32_t dst 			= 0;
	int ret  				= 0;

	static struct saudio_ipc s_audio_ipc_inst = {0};

	audio_txaddr = (size_t)ioremap_nocache(init->txbuf_addr, init->txbuf_size);
	if(!audio_txaddr){
		pr_info("%s:ioremap audio_txaddr return NULL\n", __func__);
		return -ENOMEM;
	}

	audio_rxaddr = (size_t)ioremap_nocache(init->rxbuf_addr, init->rxbuf_size);
	if(!audio_rxaddr){
		pr_info("%s:ioremap audio_rxaddr return NULL\n", __func__);
		return -ENOMEM;
	}

	audio_rwptraddr = (size_t)ioremap_nocache(init->rwptr_addr, init->rwptr_size);
	if(!audio_rwptraddr){
		pr_info("%s:ioremap audio_rwptraddr return NULL\n", __func__);
		return -ENOMEM;
	}

	pr_info("%s: after ioremap txbuf: vbase=0x%lx, pbase=0x%lx, size=0x%lx\n",
		__func__, audio_txaddr, init->txbuf_addr,init->txbuf_size);
	pr_info("%s: after ioremap rxbuf: vbase=0x%lx, pbase=0x%lx, size=0x%lx\n",
		__func__, audio_rxaddr, init->rxbuf_addr, init->rxbuf_size);
	pr_info("%s: after ioremap rwptr: vbase=0x%lx, pbase=0x%lx, size=0x%lx\n",
		__func__, audio_rwptraddr, init->rwptr_addr, init->rwptr_size);
	if(5 == init->target_id){
		dst = SIPC_ID_AGDSP;
	}
	s_audio_ipc_inst.dst 		 = dst;
	s_audio_ipc_inst.name 		 = init->name;
	s_audio_ipc_inst.txbuf_size  = (init->txbuf_size) / ((uint32_t)sizeof(struct smsg));
	s_audio_ipc_inst.txbuf_addr  = audio_txaddr;
	s_audio_ipc_inst.txbuf_rdptr = audio_rwptraddr + 0;
	s_audio_ipc_inst.txbuf_wrptr = audio_rwptraddr + 4;

	s_audio_ipc_inst.rxbuf_size  = ((uint32_t)init->rxbuf_size) / ((uint32_t)sizeof(struct smsg));
	s_audio_ipc_inst.rxbuf_addr  = audio_rxaddr;
	s_audio_ipc_inst.rxbuf_rdptr = audio_rwptraddr + 8;
	s_audio_ipc_inst.rxbuf_wrptr = audio_rwptraddr + 12;
	s_audio_ipc_inst.target_id   = init->target_id;

	/* create SIPC to target-id */
	ret = saudio_ipc_create(s_audio_ipc_inst.dst, &s_audio_ipc_inst);
	if (ret) {
		pr_err("%s saudio_ipc_create failed\n", __func__);
	}

}
#endif
static int audio_pipe_probe(struct platform_device *pdev)
{
	struct audio_pipe_init_data *init = pdev->dev.platform_data;
	struct audio_pipe_device *audio_pipe;
	dev_t devid;
	int i, rval;
	/* parse dt
	* confirm txbuffer, rxbuffer,their controls pointers, and target id
	*	*/
	if (pdev->dev.of_node && !init) {
		rval = audio_pipe_parse_dt(&init, &pdev->dev);
		if (rval) {
			printk(KERN_ERR "Failed to parse audio_pipe device tree, ret=%d\n", rval);
			return rval;
		}
	}
	pr_info("%s: after audio_pipe_parse_dt, name=%s, dst=%hhu, devicesnr=%hhu, target_id=%u, struct audio_pipe_init_data *init =0x%lx\n",
		__func__, init->name, init->dst, init->devicesnr, init->target_id, init);
	audio_pipe = kzalloc(sizeof(struct audio_pipe_device), GFP_KERNEL);
	if (audio_pipe == NULL) {
		audio_pipe_destroy_pdata(&init);
		printk(KERN_ERR "Failed to allocate audio_pipe_device\n");
		return -ENOMEM;
	}

	rval = alloc_chrdev_region(&devid, 0, init->devicesnr, init->name);
	if (rval != 0) {
		kfree(audio_pipe);
		audio_pipe_destroy_pdata(&init);
		printk(KERN_ERR "Failed to alloc audio_pipe chrdev\n");
		return rval;
	}
	cdev_init(&(audio_pipe->cdev), &audio_pipe_fops);
	rval = cdev_add(&(audio_pipe->cdev), devid, init->devicesnr);
	if (rval != 0) {
		kfree(audio_pipe);
		unregister_chrdev_region(devid, init->devicesnr);
		audio_pipe_destroy_pdata(&init);
		printk(KERN_ERR "Failed to add audio_pipe cdev\n");
		return rval;
	}
	audio_pipe->major = MAJOR(devid);
	audio_pipe->minor = MINOR(devid);
	if (init->devicesnr > 1) {
		for (i = 0; i < init->devicesnr; i++) {
			device_create(audio_pipe_class, NULL,
				MKDEV(audio_pipe->major, audio_pipe->minor + i),
				NULL, "%s%d", init->name, i);
//			audio_pipe->init->channel[i] = 2+i;
		}
	} else {
		device_create(audio_pipe_class, NULL,
			MKDEV(audio_pipe->major, audio_pipe->minor),
			NULL, "%s", init->name);
//		audio_pipe->init->channel[0] = 2;
	}
	audio_pipe->init = init;
	platform_set_drvdata(pdev, audio_pipe);
	return 0;
}

static int  audio_pipe_remove(struct platform_device *pdev)
{

	struct audio_pipe_device *audio_pipe = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < audio_pipe->init->devicesnr; i++) {
		device_destroy(audio_pipe_class,
				MKDEV(audio_pipe->major, audio_pipe->minor + i));
	}
	cdev_del(&(audio_pipe->cdev));
	unregister_chrdev_region(
		MKDEV(audio_pipe->major, audio_pipe->minor), audio_pipe->init->devicesnr);
	audio_pipe_destroy_pdata(&audio_pipe->init);
	kfree(audio_pipe);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id audio_pipe_match_table[] = {
	{.compatible = "sprd,sprd_audio_pipe", },
	{ },
};

static struct platform_driver audio_pipe_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_audio_pipe",
		.of_match_table = audio_pipe_match_table,
	},
	.probe = audio_pipe_probe,
	.remove = audio_pipe_remove,
};

static int __init audio_pipe_init(void)
{
	audio_pipe_class = class_create(THIS_MODULE, "sprd_audio_pipe");
	if (IS_ERR(audio_pipe_class))
		return PTR_ERR(audio_pipe_class);

	return platform_driver_register(&audio_pipe_driver);
}

static void __exit audio_pipe_exit(void)
{
	class_destroy(audio_pipe_class);
	platform_driver_unregister(&audio_pipe_driver);
}

module_init(audio_pipe_init);
module_exit(audio_pipe_exit);

MODULE_AUTHOR("SPRD");
MODULE_DESCRIPTION("SIPC/AUDIO_PIPE driver");
MODULE_LICENSE("GPL");
