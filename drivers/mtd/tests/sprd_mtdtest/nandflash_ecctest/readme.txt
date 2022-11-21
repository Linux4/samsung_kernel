ecctest——纠错能力测试,用于获取当前系统实际纠错能力。

参数说明：
dev:用于指定测试使用的mtd分区，默认值为4

使用方法：
root后加载测试模块并传入参数，测试结果通过kernel log输出;
cmd:insmod nandflash_ecctest.ko dev=4