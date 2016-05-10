/* GStreamer
 * Copyright (C) <2002> David A. Schleef <ds@schleef.org>
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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

#ifndef __GST_VIDEO_AVC_FILE_H__
#define __GST_VIDEO_AVC_FILE_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#define GST_TYPE_AVC_FILE_SRC \
  (gst_avc_file_src_get_type())
#define GST_AVC_FILE_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVC_FILE_SRC,GstAvcFileSrc))
#define GST_VIDEO_AVC_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVC_FILE_SRC,GstAvcFileSrcClass))
#define GST_IS_AVC_FILE_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVC_FILE_SRC))
#define GST_IS_AVC_FILE_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVC_FILE_SRC))



typedef struct _GstAvcFileSrc GstAvcFileSrc;
typedef struct _GstAvcFileSrcClass GstAvcFileSrcClass;

/**
 * GstAvcFileSrc:
 *
 * Opaque data structure.
 */
struct _GstAvcFileSrc {
  GstPushSrc element;

  /*< private >*/
  /* video state */
  gint width;
  gint height;

  gint rate_numerator;
  gint rate_denominator;

  /* private */
  gint64 timestamp_offset;              /* base offset */
  GstClockTime running_time;            /* total running time */
  gint64 n_frames;                      /* total frames sent */
  gboolean peer_alloc;
  FILE* file;
};

struct _GstAvcFileSrcClass {
  GstPushSrcClass parent_class;
};

GType gst_avc_file_src_get_type (void);

G_END_DECLS

#endif /* __GST_VIDEO_AVC_FILE_H__ */
