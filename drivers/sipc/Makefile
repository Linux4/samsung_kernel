
ifeq ($(CONFIG_OF), y)
obj-$(CONFIG_SIPC)		+= sipc.o smsg.o smem.o sbuf.o sblock.o sipc_debugfs.o
else
obj-$(CONFIG_SIPC)		+= smsg.o smem.o sbuf.o sblock.o sipc_debugfs.o
endif

obj-$(CONFIG_SIPC_SPIPE)	+= spipe.o

obj-$(CONFIG_SIPC_SETH)	+= seth.o

obj-$(CONFIG_SIPC_SPOOL)	+= spool.o

obj-$(CONFIG_SIPC_STTY)	+= stty.o
