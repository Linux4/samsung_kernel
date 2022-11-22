postest——位置测试
本测试主要测试驱动的基本操作是否正确。该测试会将一个正常的block标记成bad_block，测试结束后需要调用强制擦除接口将该bad_block恢复正常（将/sys/kernel/debug/nfc_base/allowEraseBadBlock置为1）。

参数说明：
dev:用于指定测试使用的mtd分区，默认值为4
bad_pos:用于指定坏块标记的位置，默认值为0
bad_len:用于指定坏块标记的长度，默认值为2 bytes

使用方法：
1 将/sys/kernel/debug/nfc_base/allowEraseBadBlock置为1；
cmd:echo "1" > /sys/kernel/debug/nfc_base/allowEraseBadBlock
2 root后加载测试模块并传入参数，测试结果通过kernel log输出;
cmd:insmod nandflash_postest.ko dev=4 bad_pos=0 bad_len=2
3 将/sys/kernel/debug/nfc_base/allowEraseBadBlock置为0
cmd:echo "0" > /sys/kernel/debug/nfc_base/allowEraseBadBlock