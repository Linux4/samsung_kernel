#ifndef __TZTUI_H__
#define __TZTUI_H__

#include "tzdev.h"

/*#define TZTUI_DEBUG*/

#ifdef TZTUI_DEBUG
#define DBG(...)    pr_info( "[tztui] DBG : " __VA_ARGS__)
#define ERR(...)    pr_alert("[tztui] ERR : " __VA_ARGS__)
#else
#define DBG(...)
#define ERR(...)    pr_alert("[tztui] ERR : " __VA_ARGS__)
#endif


/* SMC Commands list */
#define TZTUI_SMC_TUI_EVENT         0x0000000a

/* TUI Events list */
#define TUI_EVENT_TOUCH_IRQ         0x00000001
#define TUI_EVENT_CANCEL            0x00000002
#define TUI_EVENT_TIMEOUT           0x00000004

#define tztui_smc_tui_event(tui_event)        tzdev_smc_cmd(TZTUI_SMC_TUI_EVENT, tui_event, 0x0ul, 0x0ul)

#endif  /* __TZTUI_H__ */
