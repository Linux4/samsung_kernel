#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmp_trace

#if !defined(_TRACE_MMP_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMP_TRACE_H
#include <video/mmp_ioctl.h>
#include <linux/tracepoint.h>
#include "sync.h"

TRACE_EVENT(surface,
	TP_PROTO(u32 id, struct mmp_surface *surface),

	TP_ARGS(id, surface),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, addr)
		__field(u32, xsrc)
		__field(u32, ysrc)
		__field(u32, xpos)
		__field(u32, ypos)
		__field(u32, pix_fmt)
		__field(u32, pitch)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->addr = surface->addr.phys[0];
		__entry->xsrc = surface->win.xsrc;
		__entry->ysrc = surface->win.ysrc;
		__entry->xpos = surface->win.xpos;
		__entry->ypos = surface->win.ypos;
		__entry->pix_fmt = surface->win.pix_fmt;
		__entry->pitch = surface->win.pitch[0];
	),

	TP_printk("surface layer:%d,addr:0x%x,pitch:0x%x,xsrc:%d,ysrc:%d,xpos:%d,ypos:%d,fmt:0x%x",
		__entry->id, __entry->addr, __entry->pitch,
		__entry->xsrc, __entry->ysrc, __entry->xpos,
		__entry->ypos, __entry->pix_fmt)
);

TRACE_EVENT(win,
	TP_PROTO(u32 id, struct mmp_win *win, int flip),

	TP_ARGS(id, win, flip),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, xsrc)
		__field(u32, ysrc)
		__field(u32, xpos)
		__field(u32, ypos)
		__field(u32, pix_fmt)
		__field(u32, pitch)
		__field(u32, flip)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->xsrc = win->xsrc;
		__entry->ysrc = win->ysrc;
		__entry->xpos = win->xpos;
		__entry->ypos = win->ypos;
		__entry->pix_fmt = win->pix_fmt;
		__entry->pitch = win->pitch[0];
		__entry->flip = flip;
	),

	TP_printk("%s window overlay:%d,pitch: 0x%x,xsrc:%d,ysrc:%d,xpos:%d,ypos:%d,fmt:0x%x",
		__entry->flip ? "flip" : "trigger",
		__entry->id, __entry->pitch, __entry->xsrc,
		__entry->ysrc, __entry->xpos, __entry->ypos,
		__entry->pix_fmt)
);

TRACE_EVENT(addr,
	TP_PROTO(u32 id, struct mmp_addr *addr, int flip),

	TP_ARGS(id, addr, flip),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, addr)
		__field(u32, flip)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->addr = addr->phys[0];
		__entry->flip = flip;
	),

	TP_printk("%s address overlay: %d, addr: 0x%x",
		__entry->flip ? "flip" : "trigger",
		__entry->id, __entry->addr)
);

TRACE_EVENT(dma,
	TP_PROTO(u32 id, u32 dma, int flip),

	TP_ARGS(id, dma, flip),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, dma)
		__field(u32, flip)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->dma = dma;
		__entry->flip = flip;
	),

	TP_printk("%s dma overlay: %d, dma: %d",
		__entry->flip ? "flip" : "trigger",
		__entry->id, __entry->dma)
);

TRACE_EVENT(alpha,
	TP_PROTO(u32 id, struct mmp_alpha *alpha, int flip),

	TP_ARGS(id, alpha, flip),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, path)
		__field(u32, configure)
		__field(u32, flip)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->path = alpha->alphapath;
		__entry->configure = alpha->config;
		__entry->flip = flip;
	),

	TP_printk("%s dma overlay: %d, path: %d, config: %d",
		__entry->flip ? "flip" : "trigger",
		__entry->id, __entry->path, __entry->configure)
);

TRACE_EVENT(commit,
	TP_PROTO(struct mmp_path *path, int flip),

	TP_ARGS(path, flip),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, commit)
		__field(u32, flip)
	),

	TP_fast_assign(
		__entry->id = path->id;
		__entry->commit = atomic_read(&path->commit);
		__entry->flip = flip;
	),

	TP_printk("%s commit overlay: %d, commit: %d",
		__entry->flip ? ((__entry->flip == 2)
		? "wait" : "flip") : "trigger",
		__entry->id, __entry->commit)
);

TRACE_EVENT(vsync,
	TP_PROTO(int on),

	TP_ARGS(on),

	TP_STRUCT__entry(
		__field(u32, on)
	),

	TP_fast_assign(
		__entry->on = on;
	),

	TP_printk("vsync is %s ",
		__entry->on ? "enable" : "disable")
);

TRACE_EVENT(underflow,
	TP_PROTO(struct mmp_path *path, u32 sts),

	TP_ARGS(path, sts),

	TP_STRUCT__entry(
		__string(name, path->name)
		__field(u32, sts)
		__field(u32, id)
	),

	TP_fast_assign(
		__assign_str(name, path->name);
		__entry->sts = sts;
		__entry->id = path->id;
	),

	TP_printk("%s%s%s", __get_str(name),
			__entry->sts & gfx_udflow_imask(__entry->id) ? ": graphic layer" : "",
			__entry->sts & vid_udflow_imask(__entry->id) ? ": video layer" : "")
);

TRACE_EVENT(fence,
	TP_PROTO(struct sync_timeline *timeline, int id, int commit,
		int frame, int pause),

	TP_ARGS(timeline, id, commit, frame, pause),

	TP_STRUCT__entry(
		__string(name, timeline->name)
		__array(char, value, 32)
		__field(unsigned long, obj)
		__field(u32, id)
		__field(u32, commit)
		__field(u32, frame)
		__field(u32, pause)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->commit = commit;
		__entry->frame = frame;
		__entry->obj = (unsigned long)timeline;
		__assign_str(name, timeline->name);
		__entry->pause = pause;
		if (timeline->ops->timeline_value_str) {
			timeline->ops->timeline_value_str(timeline,
					      __entry->value,
					      sizeof(__entry->value));
		} else {
			__entry->value[0] = '\0';
		}
	),

	TP_printk("fence %s, name=%s, obj=0x%lx, overlay=%d, value=%s,commit=%d, frame=%d",
		__entry->pause ? "pause" : "work",
		__get_str(name), __entry->obj, __entry->id, __entry->value,
		__entry->commit, __entry->frame)
);
/* This file can get included multiple times, TRACE_HEADER_MULTI_READ at top */
#endif /* _TRACE_MMP_LCD_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH video

#define TRACE_INCLUDE_FILE mmp_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
