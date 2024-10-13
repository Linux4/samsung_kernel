#ifndef __KUNIT__SEC_PMSG_TEST_H__
#define __KUNIT__SEC_PMSG_TEST_H__

#include "../sec_pmsg.h"

extern int __pmsg_handle_dt_debug_level(struct pmsg_drvdata *drvdata, struct device_node *np, unsigned int sec_dbg_level);
extern int ____logger_level_header(struct pmsg_logger *logger, struct logger_level_header_ctx *llhc);
extern char ____logger_level_prefix(struct pmsg_logger *logger);

#endif /* __KUNIT__SEC_PMSG_TEST_H__ */
