ifeq ($(CONFIG_LZO_HW_ALGO),y)
obj-y	+= lzo1x_hw_compress.o zipenc_drv.o
obj-y	+= lzo1x_hw_decompress.o zipdec_drv.o
endif
lzo_compress-objs := lzo1x_compress.o
lzo_decompress-objs := lzo1x_decompress_safe.o

obj-$(CONFIG_LZO_COMPRESS) += lzo_compress.o
obj-$(CONFIG_LZO_DECOMPRESS) += lzo_decompress.o
