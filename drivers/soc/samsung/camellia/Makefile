EXTRA_CFLAGS=-I$(shell pwd)

obj-$(CONFIG_CAMELLIA) += camellia.o
camellia-y := main.o pm.o dump.o control_path.o data_path.o mailbox.o logsink.o

ccflags-y := -I $(srctree)
ccflags-$(CONFIG_CAMELLIA_DEBUG) := -DDEBUG
