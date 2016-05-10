/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gstdelay.h:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __GST_DELAY_H__
#define __GST_DELAY_H__

#include <gst/gst.h>
#include "diskcache.h"
#include <windows.h>

G_BEGIN_DECLS

#define GST_TYPE_DELAY \
  (gst_delay_get_type())
#define GST_DELAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DELAY,GstDelay))
#define GST_QUEUE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DELAY,GstDelayClass))
#define GST_IS_DELAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DELAY))
#define GST_IS_DELAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DELAY))

typedef struct _GstDelay GstDelay;
typedef struct _GstDelayClass GstDelayClass;


/**
 * GstDelay:
 *
 * Opaque #GstDelay structure.
 */
struct _GstDelay {
  GstElement element;

  /*< private >*/
  GstPad *sinkpad;
  GstPad *srcpad;

  /* segments to keep track of timestamps */
  GstSegment sink_segment;
  GstSegment src_segment;

  GstClockTime delay_time,current_level_time;

  /* position of src/sink */
  GstClockTime sinktime, srctime;
  /* TRUE if either position needs to be recalculated */
  gboolean sink_tainted, src_tainted;

  /* flowreturn when srcpad is paused */
  GstFlowReturn srcresult;
  gboolean      unexpected;
  gboolean      eos;

  /* the queue of data we're keeping our grubby hands on */
  GQueue *queue;
  GstClock* sys_clock;
  DiskCache* disk_cache;

  GMutex *qlock;        /* lock for queue (vs object lock) */
  gboolean waiting_add;
  GCond *item_add;      /* signals buffers now available for reading */
  gboolean waiting_del;
  GCond *item_del;      /* signals space now available for writing */

  gboolean head_needs_discont, tail_needs_discont;
  gboolean push_newsegment;

  gboolean silent;      /* don't emit signals */

  /* whether the first new segment has been applied to src */
  gboolean newseg_applied_to_src;

  HANDLE render_event;
};

struct _GstDelayClass {
  GstElementClass parent_class;

  /* signals - 'running' is called from both sides
   * which might make it sort of non-useful... */
  void (*underrun)      (GstDelay *queue);
  void (*running)       (GstDelay *queue);
  void (*overrun)       (GstDelay *queue);

  void (*pushing)       (GstDelay *queue);
};

GType gst_delay_get_type (void);

G_END_DECLS


#endif /* __GST_DELAY_H__ */
