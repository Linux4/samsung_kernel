/* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 start */
#ifndef _LINUX_CHG_TCPC_INFO_H
#define _LINUX_CHG_TCPC_INFO_H

enum chg_ic_supplier {
    UNKNOWN_CHG,
    SGM41513,
    UPM6910,
    SC89601,
    ETA6963,
    SY6970,
    BQ25601,
    RT9467,
    SC89890H,
    SGM41511,
    SGM41541,
    /* A06 code for SR-AL7160A-01-341 by shixuanxuan at 20230325 start*/
    UPM6922,
    /* A06 code for SR-AL7160A-01-341 by shixuanxuan at 20230325 end*/
    /* A06 code for AL7160A-58 by zhangziyi at 20240415 start */
    CX2560X,
    /* A06 code for AL7160A-58 by zhangziyi at 20240415 end */
};

enum tcpc_cc_supplier {
    UNKNOWN_CC,
    HUSB311,
    FUSB302,
    ET7304,
    SGM7220,
    WUSB3801,
    FUSB301A,
    /* A06 code for  SR-AL7160A-01-275|SR-AL7160A-01-274 by shixuanxuan at 20240326 start */
    CPS8851,
    UPM7610,
    WUSB3812C,
    UPM6922P,
    /* A06 code for  SR-AL7160A-01-275|SR-AL7160A-01-274 by shixuanxuan at 20240326 end */
};
/* A06 code for AL7160A-445 by jiashixian at 20240610 start */
#if defined(CONFIG_HQ_PROJECT_O8)
enum chg_pump_supplier {
    UNKNOWN_CP,
    UPM6722,
    NU2115A,
    SGM41606S,
    NX853X,
};
extern enum chg_pump_supplier cp_info;
#endif
/* A06 code for AL7160A-445 by jiashixian at 20240610 end */
#endif
/* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 end */