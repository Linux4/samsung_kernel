#ifndef _SPRD_IODEBUG_H
#define _SPRD_IODEBUG_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/types.h>

/*must be less than MAX_LINKS*/
#define NETLINK_IODEBUG 29

enum iodebug_nlmsg_type_e {
	IODEBUG_NLMSG_TYPE_MIN = 0x10,
	NLMSGTYPE_U2K_GET_PID,
	NLMSGTYPE_K2U_SEND_EVENT,

	/*add other msg type before here*/
	IODEBUG_NLMSG_TYPE_MAX
};

enum iodebug_nlmsg_event_e {
	IODEBUG_NLMSG_EVENT_MIN = 100,
	NLMSGEVENT_K2U_SHOW_IO_INFO,
#ifdef CONFIG_SPRD_IODEBUG_VFS
	NLMSGEVENT_SHOW_IO_PERIOD,
	NLMSGEVENT_SHOW_IO_ANR,
#endif

	/*add other msg event key before here*/
	IODEBUG_NLMSG_EVENT_MAX
};


#define IODEBUG_FOPS(name)						\
	static const struct file_operations iodebug_##name##_fops = {	\
		.open = iodebug_open_##name,				\
		.read = seq_read,					\
		.llseek = seq_lseek,					\
		.release = single_release,				\
	};

#define IODEBUG_ADD_FILE(name, mode, parent, data, fops)		\
	{								\
		struct dentry *dent;					\
		dent = debugfs_create_file(name, mode, parent,data,	\
					  &fops);			\
		if (!dent){						\
			printk("Create %s file failed.\n",name);	\
			return -ENOMEM;					\
		}							\
	}


#define IODEBUG_ADD_VAR_U8(name, mode, parent, value)			\
	{								\
		struct dentry *dent;					\
		dent = debugfs_create_u8(name, mode, parent,value);	\
		if (!dent){						\
			printk("Create %s entry failed.\n",name);	\
			return -ENOMEM;					\
		}							\
	}


#define IODEBUG_ADD_VAR_U32(name, mode, parent, value)			\
	{								\
		struct dentry *dent;					\
		dent = debugfs_create_u32(name, mode, parent,value);	\
		if (!dent){						\
			printk("Create %s entry failed.\n",name);	\
			return -ENOMEM;					\
		}							\
	}

enum iodebug_point_e {
	IODEBUG_POINT_MIN = 0,
	IODEBUG_POINT_SYSREAD,
	IODEBUG_POINT_SYSWRITE,

	/*add new checkpoint before here*/
	IODEBUG_POINT_MAX
};


struct iodebug_point_monitor{
	pid_t				pid;
	char				taskname[TASK_COMM_LEN];
	char				time[32];
	signed long			entered;
	unsigned int			time_usage;
	unsigned int			flag;
	char				disk_name[32];
	struct iodebug_point_monitor	*next;
	struct iodebug_point_monitor	*self;
};

#define IODEBUG_POINT_RECORD_NUM ( 1 << 8)

struct iodebug_point_record{
	enum iodebug_point_e		point_index;
	spinlock_t			lock;
	int				num;
	struct iodebug_point_monitor  	buff[IODEBUG_POINT_RECORD_NUM];
	struct iodebug_point_monitor  	*fifo_head;
};

extern asmlinkage int iodebug_trace(const char *fmt, ...);
extern int iodebug_point_monitor_init(void);
#ifdef CONFIG_SPRD_IODEBUG
#define IODEBUG_TRACE(fmt...)	iodebug_trace(KBUILD_MODNAME ": " fmt)
#else
#define IODEBUG_TRACE(fmt...)	do { } while (0)
#endif

#ifdef CONFIG_SPRD_IODEBUG_HOTPOINT
extern unsigned int iodebug_point_enter( enum iodebug_point_e index,
					struct iodebug_point_monitor *info,
					struct file* filp);
extern unsigned int iodebug_point_end( enum iodebug_point_e index,
					struct iodebug_point_monitor *info);

#define IODEBUG_POINT_ENTER(point_index, filp)					\
		struct iodebug_point_monitor point_monitor_debug;		\
		iodebug_point_enter((point_index), &point_monitor_debug, (filp));

#define IODEBUG_POINT_END(point_index)						\
		iodebug_point_end((point_index), &point_monitor_debug);
#else
#define IODEBUG_POINT_ENTER(point_index, filp) (point_index)

#define IODEBUG_POINT_END(point_index) (point_index)
#endif

extern int iodebug_nl_send_msg(int msg_type, int event_key);
#define IODEBUG_NLMSG_EVENT(event_key)						\
		iodebug_nl_send_msg(NLMSGTYPE_K2U_SEND_EVENT,(event_key));

#ifdef CONFIG_SPRD_IODEBUG_IOSCHEDULE
extern void iodebug_ioschedule_timer_add(struct task_struct *task);
extern void iodebug_ioschedule_timer_cancel(struct task_struct *task);
#endif

#endif
