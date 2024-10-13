#ifndef __GSP_DEBUG_H_
#define __GSP_DEBUG_H_

#include "gsp_drv_common.h"
void gsp_timeout_print(struct gsp_context *ctx);
void print_work_queue_status(struct gsp_work_queue *wq);
void gsp_work_thread_status_print(struct gsp_context *ctx);

#endif
