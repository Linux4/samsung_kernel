stdtest——驱动标准测试
本测试是将mtd开源社区提供的6个标准测试（read/page/subpage/oob/speed/stress）整合为一个标准测试。通过该测试，说明驱动程序已达到基本要求。

参数说明：
dev:用于指定测试使用的mtd分区，默认值为4

使用方法：
root后加载测试模块并传入参数，测试结果通过kernel log输出。
cmd:insmod nandflash_stdtest.ko dev=4