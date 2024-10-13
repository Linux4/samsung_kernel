
ifeq ($(CONFIG_OF), y)
obj-$(CONFIG_SIPC)		+= sipc.o smsg.o smem.o sbuf.o sblock.o sipc_debugfs.o seblock.o
else
obj-$(CONFIG_SIPC)		+= smsg.o smem.o sbuf.o sblock.o seblock.o sipc_debugfs.o
endif

obj-$(CONFIG_SIPC_SPIPE)	+= spipe.o

obj-$(CONFIG_SIPC_SETH)	+= seth.o

obj-$(CONFIG_SIPC_SPOOL)	+= spool.o

obj-$(CONFIG_SIPC_STTY)	+= stty.o

obj-$(CONFIG_SIPC_DUAL_SIM_PLUG) += dual_sim_plug.o

obj-$(CONFIG_ZERO_COPY_SIPX) += sipx.o
