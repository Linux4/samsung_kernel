obj-$(CONFIG_SEC_LOG_BUF)		+= sec_log_buf.o
sec_log_buf-objs			:= sec_log_buf_main.o \
					   sec_log_buf_logger.o \
					   sec_log_buf_builtin.o \
					   sec_log_buf_console.o \
					   sec_log_buf_vh_logbuf.o \
					   sec_log_buf_last_kmsg.o \
					   sec_log_buf_ap_klog.o

ifeq ($(CONFIG_SEC_LOG_BUF_USING_KPROBE),y)
sec_log_buf-objs			+= sec_log_buf_kprobe.o
endif

ifeq ($(CONFIG_DEBUG_FS),y)
sec_log_buf-objs			+= sec_log_buf_debugfs.o
endif

CFLAGS_sec_log_buf_vh_logbuf.o		= -I$(srctree)/kernel/printk
