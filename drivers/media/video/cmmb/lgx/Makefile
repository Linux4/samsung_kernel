obj-$(CONFIG_INNOCMMB)	:= inno_lgx.o
inno_lgx-objs := _inno_lgx.o inno_uam.o

ifdef CONFIG_INNOCMMB_DEBUG
EXTRA_CFLAGS += -DDEBUG
endif
EXTRA_CFLAGS += -I$(src)/../include  
