obj-$(CONFIG_INNOCMMB) := inno_core.o
inno_core-objs := _inno_core.o inno_comm.o inno_ctl.o inno_irq.o inno_power.o inno_sys.o

inno_core-objs += inno_spi.o

ifdef CONFIG_INNOCMMB_DEBUG
EXTRA_CFLAGS += -DDEBUG
endif
EXTRA_CFLAGS += -I$(src)/../include 

