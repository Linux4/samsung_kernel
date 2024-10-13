/* iodebug_hotpoint.c
 *
 * add this file for IO debug.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 1, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/sprd_iodebug.h>
#include <linux/genhd.h>

extern struct dentry *iodebug_root_dir;

static unsigned int point_monitor_enable = 0;
static unsigned int point_monitor_index = 0;
static unsigned int point_monitor_threshold = 500;/*500ms*/

static int select_pid[6] = {
	-1,-1,-1,-1,-1,-1
};
static int select_pid_size = 6;

#define IODEBUG_MONITOR_ENABLE_ON (point_monitor_enable)
#define IODEBUG_MONITOR_INDEX_SET (point_monitor_index)
#define IODEBUG_DELAY_THRESHOLD (point_monitor_threshold)

struct iodebug_point_table{
	enum iodebug_point_e	index;
	char 			help[128];
};

static struct iodebug_point_table point_table[IODEBUG_POINT_MAX-1]={
	{IODEBUG_POINT_SYSREAD,		"sysread "},
	{IODEBUG_POINT_SYSWRITE,	"syswrite"},
	/*add new monitor point if need*/
};

static struct iodebug_point_record point_debug_record[IODEBUG_POINT_MAX-1];

#define IODEBUG_MONITOR_FLAG (0xA5A5)


static int monitor_pid_valid(pid_t pid)
{
	int i = 0;
	int check_on = 0;
	int pid_valid = 0;
	for (; i<select_pid_size; i++){
		if (select_pid[i] != -1)
			check_on = 1;
		if (pid == select_pid[i])
			pid_valid = 1;
	}
	return check_on ? pid_valid : 1;
}

static char* pointIndex2help(enum iodebug_point_e index)
{
	int i;
	for (i=0; i<sizeof(point_table)/sizeof(point_table[0]); i++){
		if(index == point_table[i].index)
			return point_table[i].help;
	}
	return "INVALID";
}

static struct iodebug_point_record* pointIndex2record(enum iodebug_point_e index)
{
	if ((index <= IODEBUG_POINT_MIN || index >= IODEBUG_POINT_MAX) ||
		(IODEBUG_MONITOR_INDEX_SET && (index != IODEBUG_MONITOR_INDEX_SET)))
		return NULL;

	return &(point_debug_record[index-1]);
}

static size_t print_enter_time(u64 ts, char *buf)
{
	unsigned long rem_nsec;

	rem_nsec = do_div(ts, 1000000000);

	if (!buf)
		return snprintf(NULL, 0, "%6lu.000000", (unsigned long)ts);

	return sprintf(buf, "%6lu.%06lu",(unsigned long)ts, rem_nsec / 1000);
}



static void iodebug_point_fifo_init(struct iodebug_point_record *record)
{
	int i;
	struct iodebug_point_monitor *fifo = &(record->buff[0]);

	record->fifo_head = fifo;
	for (i=0; i<(IODEBUG_POINT_RECORD_NUM -1); i++){
		fifo->next = fifo+1;
		fifo = fifo->next;
	}
	fifo->next = record->fifo_head;
}

static int iodebug_point_fifo_put( struct iodebug_point_record *record,
					struct iodebug_point_monitor *info )
{
	struct iodebug_point_monitor *dest = record->fifo_head;
	struct iodebug_point_monitor *next = dest->next;

	/*keep the next value*/
	info->next = next;

	/*store the lastest debug info*/
	memset(dest, 0, sizeof(struct iodebug_point_monitor));
	memcpy(dest, info, sizeof(struct iodebug_point_monitor));

	/*update record fifo head*/
	record->fifo_head = dest->next;

	if ( record->num < IODEBUG_POINT_RECORD_NUM ){
		record->num ++;
	}

	return 0;
}


static void iodebug_point_fifo_get(struct seq_file *seq_f,
					struct iodebug_point_record *record)
{
	int i;
	int count;
	unsigned long proc_flags;
	struct iodebug_point_monitor data;
	struct iodebug_point_monitor *get_buff;
#define TEXT_FORMAT ("pid:%5d,        taskname:%16s,        enter_time:%s,        time_usage:%dms,        disk:%s\n")

	/*empty fifo*/
	if (!record->num){
		return;
	}

	count = record->num;
	if (count  < IODEBUG_POINT_RECORD_NUM){
		spin_lock_irqsave(&record->lock, proc_flags);
		for (i=0; i<count; i++){
			memset(&data, 0, sizeof(struct iodebug_point_monitor));
			memcpy(&data, &record->buff[i], sizeof(struct iodebug_point_monitor));
			if (!monitor_pid_valid(data.pid))
				continue;
			seq_printf(seq_f, TEXT_FORMAT,
					  data.pid,
					  data.taskname,
					  data.time,
					  data.time_usage,
					  data.disk_name);
		}
		spin_unlock_irqrestore(&record->lock, proc_flags);
		return;
	}

	spin_lock_irqsave(&record->lock, proc_flags);
	get_buff = record->fifo_head;
	do{
		if (!monitor_pid_valid(get_buff->pid))
			goto NEXT_BUFF;

		memset(&data, 0, sizeof(struct iodebug_point_monitor));
		memcpy(&data, get_buff, sizeof(struct iodebug_point_monitor));
		seq_printf(seq_f, TEXT_FORMAT,
				  data.pid,
				  data.taskname,
				  data.time,
				  data.time_usage,
				  data.disk_name);
NEXT_BUFF:
		/*get next */
		get_buff = get_buff->next;
		count--;
	}while((get_buff != record->fifo_head) && (count > 0));
	spin_unlock_irqrestore(&record->lock, proc_flags);

}


unsigned int iodebug_point_enter( enum iodebug_point_e index,
					struct iodebug_point_monitor *info,
					struct file* filp)
{
	struct iodebug_point_record *record;
	struct inode *inode = NULL;
	/*get record for saving debug info*/
	record = pointIndex2record(index);
	if(!record)
		return -1;

	/*set dedug info*/
	memset(info, 0, sizeof(struct iodebug_point_monitor));
	info->entered = jiffies;
	info->flag = IODEBUG_MONITOR_FLAG;

	info->self = info;

	/*get disk name*/
	memcpy(info->disk_name,"null",sizeof("null"));
	if (filp && filp->f_mapping && filp->f_mapping->host){
		inode = filp->f_mapping->host;
		if (inode && inode->i_sb && inode->i_sb->s_bdev && inode->i_sb->s_bdev->bd_disk){
			memcpy(info->disk_name,inode->i_sb->s_bdev->bd_disk->disk_name,32);
		}
	}

	memset(&(info->time[0]), 0, 32);
	print_enter_time(local_clock(), &(info->time[0]));

	return 0;
}
EXPORT_SYMBOL(iodebug_point_enter);

unsigned int iodebug_point_end( enum iodebug_point_e index,
					struct iodebug_point_monitor *info)
{
	int ret;
	struct iodebug_point_record *record;
	unsigned long proc_flags;

	if (!IODEBUG_MONITOR_ENABLE_ON)
		return -1;

	/*check moniter pid.*/
	if (!monitor_pid_valid(current->pid))
		return -1;

	if ((info->flag != IODEBUG_MONITOR_FLAG) || (info->self != info)){
		/*shoul never to here!*/
		printk("[IODEBUG]:pid:0x%x(0x%x,%s),flag:0x%x,self:0x%lx,info:0x%lx.\n",
			info->pid,current->pid,current->comm,info->flag,(unsigned long)(info->self),(unsigned long)info);
		return -1;
	}

	info->pid = current->pid;
	memcpy(info->taskname,current->comm,sizeof(current->comm));
	info->time_usage = jiffies_to_msecs(jiffies - info->entered);

	/*ignore it if usage time less than threshold*/
	if (info->time_usage < IODEBUG_DELAY_THRESHOLD)
		return -1;

	/*get record for saving debug info*/
	record = pointIndex2record(index);
	if (!record)
		return -1;

	/*save info into recorder*/
	spin_lock_irqsave(&record->lock, proc_flags);
	ret = iodebug_point_fifo_put(record, info);
	spin_unlock_irqrestore(&record->lock, proc_flags);
	if (ret)
		printk(KERN_ERR "[IODEBUG]fail to save info to debug recorder.\n");

	return ret;

}
EXPORT_SYMBOL(iodebug_point_end);



static int show_point_table(struct seq_file *seq_f, void *v)
{
	int i;

	seq_printf(seq_f, "index              comment\n");
	for (i=0; i < sizeof(point_table)/sizeof(point_table[0]); i++){
		seq_printf(seq_f, "%3d                %s\n",
				point_table[i].index,point_table[i].help);
	}
	return 0;
}

static int iodebug_open_point_table(struct inode *inode, struct file *file)
{
	return single_open(file, show_point_table, NULL);
}
IODEBUG_FOPS(point_table);


static int show_point_monitor(struct seq_file *seq_f, void *v)
{
	struct iodebug_point_record *record;
	int i;
	int index = 0;

	if (!IODEBUG_MONITOR_ENABLE_ON)
		return 0;

	for (i=0; i<sizeof(point_debug_record)/sizeof(point_debug_record[0]); i++){
		index = point_debug_record[i].point_index;
		record = pointIndex2record(index);
		if (record){
			seq_printf(seq_f, "%s's monitor info:\n",
					pointIndex2help(index));
			iodebug_point_fifo_get(seq_f, record);
			seq_printf(seq_f, "\n");
		}
	}

	return 0;
}


static int iodebug_open_point_monitor(struct inode *inode, struct file *file)
{
	return single_open(file, show_point_monitor, NULL);
}
IODEBUG_FOPS(point_monitor);



int iodebug_point_monitor_init(void)
{
	int i;

	/*init info buffer*/
	for (i=0;i<sizeof(point_debug_record)/sizeof(point_debug_record[0]);i++){
		struct iodebug_point_record *record = &(point_debug_record[i]);

		record->point_index = point_table[i].index;
		record->num = 0;
		spin_lock_init(&record->lock);

		iodebug_point_fifo_init(record);
	}

	/*init debugfs entry*/
	IODEBUG_ADD_FILE("hotpoint_table", S_IRUSR, iodebug_root_dir,
			         NULL, iodebug_point_table_fops);

	IODEBUG_ADD_FILE("hotpoint_monitor", S_IRUSR, iodebug_root_dir,
			         NULL, iodebug_point_monitor_fops);

	return 0;
}
EXPORT_SYMBOL(iodebug_point_monitor_init);

/*io hotpoint monitor parameters*/
module_param_named(hotpoint_enable, point_monitor_enable, int, S_IRUGO | S_IWUSR);
module_param_named(hotpoint_index, point_monitor_index, int, S_IRUGO | S_IWUSR);
module_param_named(hotpoint_threshold, point_monitor_threshold, int,
				S_IRUGO | S_IWUSR);
module_param_array_named(hotpoint_pids, select_pid, int, &select_pid_size,
					S_IRUGO | S_IWUSR);


