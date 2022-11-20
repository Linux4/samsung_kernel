agetest， 即老化测试，用于获取单个nandflash 擦除块的寿命。该测试需要的时间较长，一般需要5小时以上，由nandflash的特性决定。为了确保被测试擦除块彻底被擦坏，需要使用强制擦除接口（将/sys/kernel/debug/nfc_base/allowEraseBadBlock置为1）。

参数说明：
dev:用于指定测试使用的mtd分区，默认值为4

使用方法：
1 将/sys/kernel/debug/nfc_base/allowEraseBadBlock置为1；
cmd:echo "1" > /sys/kernel/debug/nfc_base/allowEraseBadBlock
2 root后加载测试模块并传入参数，测试结果通过kernel log输出，通过erase_time来衡量某款nandflash的特性;
cmd:insmod nandflash_agetest.ko dev=4
3 将/sys/kernel/debug/nfc_base/allowEraseBadBlock置为0。
cmd:echo "0" > /sys/kernel/debug/nfc_base/allowEraseBadBlock

注意：
每进行一次测试，会增加一个bad block。