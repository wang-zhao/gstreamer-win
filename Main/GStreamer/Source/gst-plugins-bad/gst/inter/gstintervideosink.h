/* GStreamer
 * Copyright (C) 2011 David A. Schleef <ds@schleef.org>
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

#ifndef _GST_INTER_VIDEO_SINK_H_
#define _GST_INTER_VIDEO_SINK_H_

#include <gst/base/gstbasesink.h>
#include "gstintersurface.h"

G_BEGIN_DECLS

#define GST_TYPE_INTER_VIDEO_SINK   (gst_inter_video_sink_get_type())
#define GST_INTER_VIDEO_SINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_INTER_VIDEO_SINK,GstInterVideoSink))
#define GST_INTER_VIDEO_SINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_INTER_VIDEO_SINK,GstInterVideoSinkClass))
#define GST_IS_INTER_VIDEO_SINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_INTER_VIDEO_SINK))
#define GST_IS_INTER_VIDEO_SINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_INTER_VIDEO_SINK))

typedef struct _GstInterVideoSink GstInterVideoSink;
typedef struct _GstInterVideoSinkClass GstInterVideoSinkClass;

struct _GstInterVideoSink
{
  GstBaseSink base_intervideosink;

  GstInterSurface *surface;

  int fps_n;
  int fps_d;
};

struct _GstInterVideoSinkClass
{
  GstBaseSinkClass base_intervideosink_class;
};

GType gst_inter_video_sink_get_type (void);

G_END_DECLS

#endif
