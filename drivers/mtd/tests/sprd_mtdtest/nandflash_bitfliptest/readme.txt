bitfliptest——位翻转测试，用于测试当前系统在做数据存取时发生bit-flip的概率，测试时长由当前系统的稳定性和设定的阀值共同决定。

参数说明：
dev:用于指定测试使用的mtd分区，默认值为4
threshold:用于指定bit-flip的阀值，即当bit-flip的总数达到阀值即会停止，默认值为100。

使用方法：
root后加载测试模块并传入参数，测试结果通过kernel log输出;
cmd:insmod nandflash_bitfliptest.ko dev=4 threshold=100