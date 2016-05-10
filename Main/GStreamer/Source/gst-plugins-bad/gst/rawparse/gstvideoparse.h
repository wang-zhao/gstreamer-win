/* GStreamer
 * Copyright (C) 2006 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2007,2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * gstvideoparse.h:
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

#ifndef __GST_VIDEO_PARSE_H__
#define __GST_VIDEO_PARSE_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/base/gstadapter.h>
#include <gst/video/video.h>

#include "gstrawparse.h"

#define GST_TYPE_VIDEO_PARSE \
  (gst_video_parse_get_type())
#define GST_VIDEO_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VIDEO_PARSE,GstVideoParse))
#define GST_VIDEO_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VIDEO_PARSE,GstVideoParseClass))
#define GST_IS_VIDEO_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VIDEO_PARSE))
#define GST_IS_VIDEO_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VIDEO_PARSE))

typedef struct _GstVideoParse GstVideoParse;
typedef struct _GstVideoParseClass GstVideoParseClass;

struct _GstVideoParse
{
  GstRawParse parent;

  /* properties */
  GstVideoFormat format;
  gint width;
  gint height;
  gint par_n, par_d;
  gboolean interlaced;
  gboolean top_field_first;
};

struct _GstVideoParseClass
{
  GstRawParseClass parent_class;
};


GType gst_video_parse_get_type (void);

#endif /*  __GST_VIDEO_PARSE_H__ */
