/**
*Name：<chg-tcpc_info.h>
*Author：<gaozhengwei@huaqin.com>
*Date：<2022.07.12>
*Purpose：<charger ic info and tcpc ic info>
*/
#ifndef _LINUX_CHG_TCPC_INFO_H
#define _LINUX_CHG_TCPC_INFO_H

enum chg_ic_supplier {
    UNKNOWN_CHG,
    SGM41513,
    UPM6910,
    ETA6963,
    SY6970,
    BQ25601,
    SC89601D,
    RT9467,
    SC89890H,
    SGM41511,
    SGM41541,
};

enum tcpc_cc_supplier {
    UNKNOWN_CC,
    SGM7220,
    WUSB3801,
    FUSB301A,
    HUSB311,
};

#endif
