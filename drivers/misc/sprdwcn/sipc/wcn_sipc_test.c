#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <misc/wcn_bus.h>

#include "wcn_sipc.h"

#define WCN_SIPC_CHANNEL_MAX 32
#define TRANSF_LIST_MAX 64
#define POOL_SIZE TRANSF_LIST_MAX
#define pool_buf_size 4
#define CHNMG_SHOW_BUF_MAX (WCN_SIPC_CHANNEL_MAX * 56)

#define wcn_sipc_test_print(fmt, args...) \
	pr_info("wcn_sipc_test " fmt, ## args)

/* ugly code !! */
static struct chnmg *this_chnmg;
static struct channel *chnmg_find_channel(struct chnmg *chnmg, int id);

struct channel {
	int id;
	int inout;
	int status;

	struct mbuf_t *rx_pool_head;
	struct mbuf_t *rx_pool_tail;
	int rx_pool_num;
	struct mutex  pool_lock;
	wait_queue_head_t wait_rx_data;
	struct mbuf_t *now;
	int now_offset;

	struct mchn_ops_t mchn_ops;
	struct proc_dir_entry *file; /* self */
	struct proc_dir_entry *dir;
	char *name;
};

static int wcn_sipc_channel_open(struct inode *inode, struct file *file)
{
	struct channel *channel;

	channel = (struct channel *)PDE_DATA(inode);
	if (!channel)
		return -EIO;
	file->private_data = channel;

	return 0;
}

static int wcn_sipc_channel_release(struct inode *inode, struct file *file)
{
	return 0;
}

#define file_is_noblock(file) \
	((file->f_flags & O_NONBLOCK) == O_NONBLOCK)

static ssize_t wcn_sipc_channel_read(struct file *file, char *buffer,
		size_t count, loff_t *ppos)
{
	struct channel *channel;
	ssize_t ret_size = 0;
	int cp_len;
	int ret;

	channel = file->private_data;

	if (!channel || !count || !buffer)
		return 0;

REFILL_BUF:
	if (channel->now && channel->now->len > channel->now_offset) {
		cp_len = min_t(long, count - ret_size,
				channel->now->len - channel->now_offset);
		ret = copy_to_user(buffer + ret_size,
					channel->now->buf + channel->now_offset,
					cp_len);
		if (ret) {
			ret_size = -EFAULT;
			goto READ_EXIT;
		}

		channel->now_offset += cp_len;
		ret_size += cp_len;
		*ppos += cp_len;
	}
	wcn_sipc_test_print("%s ret_size [%d]  count [%d]\n",
						__func__, ret_size, count);
	if (ret_size < count) {
		channel->now_offset = 0;
		if (channel->now) {
			WCN_HERE_CHN(channel->id);
			ret = sprdwcn_bus_push_list(channel->id, channel->now,
					channel->now, 1);
			if (ret)
				wcn_sipc_test_print("%s push list error[%d]\n",
						__func__, ret);
			channel->now = NULL;
		}

		/* get a new mbuf */
GET_NEW_MBUF:
		WCN_HERE_CHN(channel->id);
		mutex_lock(&channel->pool_lock);
		if (channel->rx_pool_num != 0) {
			channel->rx_pool_num -= 1;
			channel->now = channel->rx_pool_head;
			channel->rx_pool_head = channel->rx_pool_head->next;
		}
		mutex_unlock(&channel->pool_lock);

		if  (!channel->now && !file_is_noblock(file) && !ret_size) {
			ret = wait_event_interruptible(channel->wait_rx_data,
					channel->rx_pool_num != 0);
			goto GET_NEW_MBUF;
		}

		if (channel->now)
			goto REFILL_BUF;
	}

READ_EXIT:
	return ret_size;
}

static ssize_t wcn_sipc_channel_write(struct file *file,
		const char *buffer, size_t count, loff_t *ppos)
{
	int mbuf_num;
	struct mbuf_t *head;
	struct mbuf_t *tail;
	struct mbuf_t *mbuf;
	struct channel *channel;
	ssize_t buf_offset;
	int i;
	unsigned short buf_len;
	int ret;

	channel = (struct channel *)file->private_data;
	mbuf_num = count / pool_buf_size + 1;
	mbuf_num = mbuf_num < TRANSF_LIST_MAX ? mbuf_num : TRANSF_LIST_MAX;

	ret = sprdwcn_bus_list_alloc(channel->id, &head, &tail, &mbuf_num);
	if (ret) {
		wcn_sipc_test_print("%s list is full\n", __func__);
		return 0;
	}

	buf_offset = 0;
	mbuf = head;
	for (i = 0; i < mbuf_num; i++) {
		buf_len = count - buf_offset  < pool_buf_size ?
			count - buf_offset : pool_buf_size;
		mbuf->buf = kzalloc(buf_len, GFP_KERNEL);
		if (!mbuf->buf) {
			wcn_sipc_test_print("%s no mem\n", __func__);
			break;
		}

		if (copy_from_user(mbuf->buf, buffer + buf_offset, buf_len)) {
			wcn_sipc_test_print("%s copy from user error\n",
					__func__);
			break;
		}

		mbuf->len = buf_len;
		mbuf = mbuf->next;
		buf_offset += buf_len;
	}

	if (!i)
		return 0;

	if (i < mbuf_num) {
		wcn_sipc_test_print("%s creat list error i[%d] mbuf_num[%d]\n",
				__func__, i, mbuf_num);
		kfree(mbuf->buf);
		sprdwcn_bus_list_free(channel->id, mbuf, tail, mbuf_num - i);
		tail = head;
		while (tail->next != mbuf)
			tail = tail->next;

		tail->next = NULL;
	}
	wcn_sipc_test_print("%s begin to push list\n", __func__);

	if (sprdwcn_bus_push_list(channel->id, head, tail, i)) {
		mbuf = head;
		while (!mbuf) {
			kfree(mbuf->buf);
			mbuf = mbuf->next;
		}
		sprdwcn_bus_list_free(channel->id, head, tail, i);
		return -EIO;
	}

	wcn_sipc_test_print("%s i[%d] mbuf_num[%d] byte[%ld]\n",
			__func__, i, mbuf_num, buf_offset);

	*ppos += buf_offset;
	return buf_offset;
}

static const struct file_operations wcn_sipc_channel_fops = {
	.owner = THIS_MODULE,
	.read = wcn_sipc_channel_read,
	.write = wcn_sipc_channel_write,
	.open = wcn_sipc_channel_open,
	.release = wcn_sipc_channel_release,
	.llseek = noop_llseek,
};

int calculate_throughput(int channel_id, struct mbuf_t *head,
		struct mbuf_t *tail, int num, const char *func_name)
{
	static struct timespec tm_begin;
	struct timespec tm_end;
	static int time_count;
	unsigned long time_total_ns;

	if (time_count == 0)
		getnstimeofday(&tm_begin);

	if (!num)
		return 0;

	if (sprdwcn_bus_push_list(channel_id, head, tail, num))
		wcn_sipc_test_print("%s push list error\n", func_name);

	time_count += num;
	if (time_count >= 1000) {
		getnstimeofday(&tm_end);
		time_total_ns = timespec_to_ns(&tm_end)
			- timespec_to_ns(&tm_begin);
		wcn_sipc_test_print("%s avg time[%ld] in [%d]\n",
				func_name, time_total_ns, time_count);
		time_count = 0;
	}

	return 0;

}

static int tx_pop_link(int channel_id,
		struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	int i;
	struct mbuf_t *mbuf;

	WCN_HERE_CHN(channel_id);
	wcn_sipc_test_print("%s is be called\n", __func__);
	mbuf_list_iter(head, num, mbuf, i) {
		kfree(mbuf->buf);
	}
	sprdwcn_bus_list_free(channel_id, head, tail, num);

	return 0;
}

static int rx_pop_link(int channel_id,
		struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	struct channel *channel = chnmg_find_channel(this_chnmg, channel_id);

	if (!channel) {
		WARN_ON(1);
		return 0;
	}
	WCN_HERE_CHN(channel_id);
	mutex_lock(&channel->pool_lock);
	if (channel->rx_pool_head) {
		channel->rx_pool_tail->next = head;
		channel->rx_pool_tail = tail;
		channel->rx_pool_num += num;
	} else {
		channel->rx_pool_head = head;
		channel->rx_pool_tail = tail;
		channel->rx_pool_num = num;
	}
	mutex_unlock(&channel->pool_lock);

	wake_up(&channel->wait_rx_data);
	return 0;
}

static void channel_mchn_ops_init(struct channel *channel)
{
	channel->mchn_ops.channel = channel->id;
	channel->mchn_ops.hif_type = HW_TYPE_SIPC;
	channel->mchn_ops.inout = channel->inout;
	channel->mchn_ops.pool_size = POOL_SIZE;
	channel->mchn_ops.pop_link = channel->inout ? tx_pop_link : rx_pop_link;
}

static struct channel *channel_init(int id, struct proc_dir_entry *dir)
{
	struct channel *channel;

	channel = kzalloc(sizeof(struct channel), GFP_KERNEL);
	if (!channel)
		return NULL;

	/* 16 is magic that string max length */
	channel->name = kzalloc(32, GFP_KERNEL);
	if (!channel->name)
		goto CHANNEL_FREE;

	channel->id = id;
	channel->inout = wcn_sipc_channel_dir(id);
	init_waitqueue_head(&channel->wait_rx_data);
	mutex_init(&channel->pool_lock);
	sprintf(channel->name, "wcn_sipc/channel_%d", id);
	channel_mchn_ops_init(channel);
	channel->file = proc_create_data(channel->name,
			0544, dir, &wcn_sipc_channel_fops,
			channel);
	if (!channel->file)
		goto CHANNEL_NAME_FREE;
	return channel;

CHANNEL_NAME_FREE:
	kfree(channel->name);
CHANNEL_FREE:
	kfree(channel);
	return NULL;
}

static int channel_register(struct channel *channel)
{
	return sprdwcn_bus_chn_init(&channel->mchn_ops);
}

static void channel_unregister(struct channel *channel)
{
	sprdwcn_bus_chn_deinit(&channel->mchn_ops);
}

static void channel_destroy(struct channel *channel)
{
	proc_remove(channel->file);
	kfree(channel->name);
	kfree(channel);
}

static ssize_t channel_show(struct channel *channel, char *kbuf,
		size_t buf_size)
{
	int ret;

	if (!channel)
		return 0;

	ret = snprintf(kbuf, buf_size, "[%d]\t[%s]\t[%d]\n", channel->id,
			channel->inout ? "tx" : "rx", channel->status);
	if (ret < 0) {
		wcn_sipc_test_print("%s channel print error id[%d] errno[%d]\n",
				__func__, channel->id, ret);
		return 0;
	}

	/* cut the \0 */
	return strlen(kbuf) - 1;
}

struct chnmg {
	struct proc_dir_entry *file; /* self */
	struct proc_dir_entry *defile;
	struct proc_dir_entry *dir;
	struct proc_dir_entry *print_level;
	struct proc_dir_entry *channel_debug;
	int num_channels;
	struct channel *channel[0];
};

static struct channel *chnmg_find_channel(struct chnmg *chnmg, int id)
{
	int i;

	for (i = 0; i < SIPC_CHN_NUM; i++) {
		if (chnmg->channel[i]) {
			if ((chnmg->channel[i])->id == id)
				return chnmg->channel[i];
		}
	}
	return NULL;
}

static struct channel *chnmg_find_channel_destroy(
						struct chnmg *chnmg, int id)
{
	int i;
	struct channel *channel = NULL;

	for (i = 0; i < SIPC_CHN_NUM; i++) {
		if (chnmg->channel[i] && ((chnmg->channel[i])->id == id)) {
			channel = chnmg->channel[i];
			chnmg->channel[i] = NULL;
		}
	}

	return channel;
}

/* chnmg channel manager */
static int wcn_sipc_chnmg_open(struct inode *inode, struct file *file)
{
	struct chnmg *chnmg;
	/* get channel_list head */
	chnmg = (struct chnmg *)PDE_DATA(inode);

	file->private_data = chnmg;
	return 0;
}

static int wcn_sipc_chnmg_release(struct inode *indoe, struct file *file)
{
	return 0;
}

static ssize_t wcn_sipc_chnmg_show(struct file *file, char *buffer,
		size_t count, loff_t *ppos)
{
	size_t ret;
	size_t buf_len;
	int i;
	struct chnmg *chnmg;
	char *kbuf;
	size_t remain_length;

	kbuf = kzalloc(CHNMG_SHOW_BUF_MAX, GFP_KERNEL);
	if (!kbuf)
		return 0;

	chnmg = file->private_data;

	for (i = 0, buf_len = 0; i < SIPC_CHN_NUM &&
			buf_len < CHNMG_SHOW_BUF_MAX; i++) {
		if (chnmg->channel[i]) {
			ret = channel_show(chnmg->channel[i], kbuf+buf_len,
					CHNMG_SHOW_BUF_MAX - buf_len);
			buf_len += ret;
		}
	}

	if (*ppos > buf_len) {
		kfree(kbuf);
		return 0;
	}

	remain_length = buf_len - *ppos;
	count = remain_length < count ? remain_length : count;
	if (copy_to_user(buffer, kbuf + *ppos, count)) {
		wcn_sipc_test_print("%s copy error\n", __func__);
		kfree(kbuf);
		return 0;
	}

	kfree(kbuf);
	*ppos += count;
	return count;
}

static int atoi(const char *str)
{
	int value = 0;

	while (*str >= '0' && *str <= '9') {
		value *= 10;
		value += *str - '0';
		str++;
	}
	return value;
}

static int wcn_sipc_chnmg_get_int_from_user(
					const char *user_buffer, size_t count)
{
	char *kbuf;
	char kbuf_num = 0;
	int channel_id;

	if (count > 10) {
		wcn_sipc_test_print("%s error count\n", __func__);
		return -EINVAL;
	}

	kbuf = kzalloc(count, GFP_KERNEL);
	if (!kbuf) {
		wcn_sipc_test_print("%s no memory\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(kbuf, user_buffer, count)) {
		kfree(kbuf);
		wcn_sipc_test_print("%s copy error\n", __func__);
		return -EIO;
	}
	kbuf_num = *kbuf;
	if (!isdigit(kbuf_num)) {
		kfree(kbuf);
		wcn_sipc_test_print("%s we only want number!\n", __func__);
		return -EINVAL;
	}

	channel_id = atoi(kbuf);
	kfree(kbuf);
	return channel_id;
}


static ssize_t wcn_sipc_chnmg_build(struct file *file, const char *buffer,
		size_t count, loff_t *ppos)
{
	int channel_id;
	struct channel *channel;
	struct chnmg *chnmg;
	int i;
	int ret = 0;

	chnmg = file->private_data;

	channel_id = wcn_sipc_chnmg_get_int_from_user(buffer, count);
	if (channel_id < 0 ||  channel_id > 32) {
		wcn_sipc_test_print("%s channel_id overflow %d\n", __func__,
				channel_id);
		return -EINVAL;
	}

	channel = chnmg_find_channel(chnmg, channel_id);
	if (channel) {
		wcn_sipc_test_print("%s channel is existed !\n", __func__);
		return -EINVAL;
	}

	channel = channel_init(channel_id, chnmg->dir);
	if (!channel) {
		wcn_sipc_test_print("%s channel init error\n", __func__);
		return -EIO;
	}

	for (i = 0; i < SIPC_CHN_NUM; i++) {
		if (!chnmg->channel[i]) {
			chnmg->channel[i] = channel;
			break;
		}
	}
	ret = channel_register(channel);
	if (ret) {
		wcn_sipc_test_print("%s channel register error ret %d\n",
						__func__, ret);
		channel_destroy(channel);
		chnmg->channel[i] = NULL;
	}
	*ppos = sizeof(struct channel) * (++chnmg->num_channels);

	return sizeof(struct channel);
}

static const struct file_operations wcn_sipc_chnmg_fops = {
	.owner = THIS_MODULE,
	.read = wcn_sipc_chnmg_show,
	.write = wcn_sipc_chnmg_build,
	.open = wcn_sipc_chnmg_open,
	.release = wcn_sipc_chnmg_release,
	.llseek = noop_llseek,
};

static ssize_t wcn_sipc_chnmg_destroy(struct file *file,
		const char *buffer, size_t count, loff_t *ppos)
{
	int channel_id;
	struct channel *channel;
	struct chnmg *chnmg;

	chnmg = file->private_data;

	channel_id = wcn_sipc_chnmg_get_int_from_user(buffer, count);
	if (channel_id < 0 ||  channel_id > 32) {
		wcn_sipc_test_print("%s channel_id overflow %d\n", __func__,
				channel_id);
		return -EINVAL;
	}

	channel = chnmg_find_channel_destroy(chnmg, channel_id);
	if (!channel) {
		wcn_sipc_test_print("%s channel is not existed!\n", __func__);
		return sizeof(struct channel);
	}

	channel_unregister(channel);
	channel_destroy(channel);

	*ppos = sizeof(struct channel) * (--chnmg->num_channels);
	return sizeof(struct channel);
}

static const struct file_operations wcn_sipc_chnmg_defile_fops = {
	.owner = THIS_MODULE,
	.read = wcn_sipc_chnmg_show,
	.write = wcn_sipc_chnmg_destroy,
	.open = wcn_sipc_chnmg_open,
	.release = wcn_sipc_chnmg_release,
	.llseek = noop_llseek,
};

static int print_level_open(struct inode *inode, struct file *file)
{
	struct chnmg *chnmg;
	/* get channel_list head */
	chnmg = (struct chnmg *)PDE_DATA(inode);

	file->private_data = chnmg;
	return 0;
}

static char wcn_sipc_print_switch;
char get_wcn_sipc_print_switch(void)
{
	return wcn_sipc_print_switch;
}

static ssize_t print_level_write(struct file *file, const char *buffer,
		size_t count, loff_t *ppos)
{
	int level = wcn_sipc_chnmg_get_int_from_user(buffer, count);

	wcn_sipc_test_print("%s get level %d->%d\n",
			__func__, wcn_sipc_print_switch, level);
	if (level < 0 && level > 16)
		return -EINVAL;

	wcn_sipc_print_switch = level;
	return count;
}

static ssize_t print_level_read(struct file *file, char *buffer,
		size_t count, loff_t *ppos)
{
	void *kbuf;
	size_t ret = 0;

	if ((*ppos)++ != 0)
		return 0;
	kbuf = kzalloc(16, GFP_KERNEL);
	if (kbuf == NULL)
		return -ENOMEM;

	snprintf(kbuf, 16, "%d\n", wcn_sipc_print_switch | 0x0);
	if (copy_to_user(buffer, kbuf, 16)) {
		kfree(kbuf);
		return -EFAULT;
	}
	kfree(kbuf);

	return ret;
}

const struct file_operations print_level = {
	.owner = THIS_MODULE,
	.read = print_level_read,
	.write = print_level_write,
	.open = print_level_open,
	.release = wcn_sipc_chnmg_release,
	.llseek = noop_llseek,
};

int channel_debug_snprint_tableinfo(char *buf, int buf_size)
{
	int ret;

	ret = snprintf(buf, buf_size, "chn\buf_alloc\buf_free");
	ret += snprintf(buf + ret, buf_size - ret,
			"\tmbuf_send_to_bus\tmbuf_recv_from_bus");
	ret += snprintf(buf + ret, buf_size - ret,
			"\tmbuf_alloc_num\tmbuf_free_num");
	ret += snprintf(buf + ret, buf_size - ret,
			"\tmbuf_giveback_to_user\tmbuf_recv_from_user");
	return ret;
}

int channel_debug_snprint_special_info(char *buf, int buf_size)
{
	int ret;

	ret = snprintf(buf, buf_size, "sp\tnet_alloc\tnet_free");
	ret += snprintf(buf + ret, buf_size - ret,
			"\tdev_kmalloc\tinterrupt_cb");
	ret += snprintf(buf + ret, buf_size - ret,
			"\tint_22_cp_num\tPacketNoFull");
	ret += snprintf(buf + ret, buf_size - ret,
			"\tbig_men_for_tx\tbig_men_for_rx");
	ret += snprintf(buf + ret, buf_size - ret,
			"\tmbuf_more_4\tmbuf_eq_1\n");
	return ret;
}

int channel_debug_snprint(char *buf, int buf_size, int chn)
{
	int ret;
	struct sipc_chn_info *sipc_chn;
	struct sipc_channel_statictics  *chn_static;

	sipc_chn = wcn_sipc_channel_get(chn);
	chn_static = &sipc_chn->chn_static;

	ret = snprintf(buf, buf_size, "%.2d ", chn);
	ret += snprintf(buf + ret, buf_size - ret,
		"\t%.8lld\t%.8lld\t%.8lld\t%.8lld",
		chn_static->buf_alloc_num,
		chn_static->buf_free_num,
		chn_static->mbuf_send_to_bus,
		chn_static->mbuf_recv_from_bus);
	ret += snprintf(buf + ret, buf_size - ret,
		"\t%.8lld\t%.8lld\t%.8lld\t%.8lld\n",
		chn_static->mbuf_alloc_num,
		chn_static->mbuf_free_num,
		chn_static->mbuf_giveback_to_user,
		chn_static->mbuf_recv_from_user);

	return ret;
}

static ssize_t wcn_sipc_channel_debug_read(struct file *file,
		char *buffer, size_t count, loff_t *ppos)
{
	void *kbuf;
	int kbuf_size = 4096;
	int info_size = 0;
	int ret;
	int i;
	int copy_size;

	kbuf = kzalloc(kbuf_size, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	ret = channel_debug_snprint_tableinfo(kbuf, kbuf_size);
	if (ret < 0) {
		kfree(kbuf);
		return ret;
	}
	info_size += ret;

	for (i = 0; i < 32; i++) {
		ret = channel_debug_snprint(kbuf + info_size,
				kbuf_size - info_size, i);
		if (ret < 0) {
			kfree(kbuf);
			return ret;
		}
		info_size += ret;
	}

	ret = channel_debug_snprint_special_info(kbuf + info_size,
			kbuf_size - info_size);
	if (ret < 0) {
		kfree(kbuf);
		return ret;
	}
	info_size += ret;

	ret = channel_debug_snprint(kbuf + info_size,
			kbuf_size - info_size, 32);
	if (ret < 0) {
		kfree(kbuf);
		return ret;
	}
	info_size += ret;


	info_size += 1;
	if (*ppos >= info_size) {
		kfree(kbuf);
		return 0;
	}

	copy_size = count > info_size ? info_size : count;
	if (copy_to_user(buffer, kbuf, copy_size)) {
		kfree(kbuf);
		return -EIO;
	}

	kfree(kbuf);
	*ppos += copy_size;
	return copy_size;
}


static ssize_t wcn_sipc_channel_debug_write(struct file *file,
		const char *buffer, size_t count, loff_t *ppos)
{
	return count;
}

static int wcn_sipc_channel_debug_open(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations channel_debug = {
	.owner = THIS_MODULE,
	.read = wcn_sipc_channel_debug_read,
	.write = wcn_sipc_channel_debug_write,
	.open = wcn_sipc_channel_debug_open,
	.llseek = noop_llseek,
};


struct proc_dir_entry *proc_sys;
int wcn_sipc_chnmg_init(void)
{
	struct chnmg *chnmg;

#if defined(WCN_SIPC_TEST_ONLY)
	/* do something to register wcn_bus */
	wcn_bus_init();
	sprdwcn_bus_preinit();
#endif
	proc_sys = proc_mkdir("wcn_sipc", NULL);
	if (!proc_sys)
		pr_err("%s build proc sys error!\n", __func__);

	wcn_sipc_test_print("%s into\n", __func__);
	chnmg = kzalloc(sizeof(struct chnmg) +
			sizeof(struct channel *) * SIPC_CHN_NUM,
			GFP_KERNEL);
	if (!chnmg)
		return -ENOMEM;

	chnmg->dir = NULL;

	chnmg->file = proc_create_data("wcn_sipc/chnmg", 0544, chnmg->dir,
			&wcn_sipc_chnmg_fops, chnmg);
	if (!chnmg->file)
		goto CHNMG_FILE_ERROR;

	chnmg->defile = proc_create_data("wcn_sipc/chnmg_destroy",
			0544, chnmg->dir, &wcn_sipc_chnmg_defile_fops, chnmg);
	if (!chnmg->defile)
		goto CHNMG_FILE_ERROR;

	chnmg->print_level = proc_create_data("wcn_sipc/print",
			0544, chnmg->dir, &print_level, chnmg);
	if (!chnmg->print_level)
		goto CHNMG_FILE_ERROR;

	chnmg->channel_debug = proc_create_data("wcn_sipc/channel_debug",
			0544, chnmg->dir, &channel_debug, chnmg);
	if (!chnmg->channel_debug)
		goto CHNMG_FILE_ERROR;

	wcn_sipc_test_print("%s init success!\n", __func__);
	this_chnmg = chnmg;
	return 0;

CHNMG_FILE_ERROR:
	kfree(chnmg);
	return -ENOMEM;
}

#if defined(WCN_SIPC_TEST_ONLY)
module_init(wcn_sipc_chnmg_init);

static void wcn_sipc_chnmg_exit(void)
{
	kfree(this_chnmg);
}
module_exit(wcn_sipc_chnmg_exit);
#endif
