obj-$(CONFIG_INNOCMMB) := inno_plat.o

inno_plat-objs := inno_plat_pxa.o

ifdef CONFIG_INNOCMMB_DEBUG
EXTRA_CFLAGS += -DDEBUG
endif
EXTRA_CFLAGS += -I$(src)/../include 
