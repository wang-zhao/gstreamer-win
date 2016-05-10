/* GStreamer
 * Copyright (C) 2010 David Schleef <ds@schleef.org>
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

#ifndef _GST_Y4M_DEC_H_
#define _GST_Y4M_DEC_H_

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS

#define GST_TYPE_Y4M_DEC   (gst_y4m_dec_get_type())
#define GST_Y4M_DEC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_Y4M_DEC,GstY4mDec))
#define GST_Y4M_DEC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_Y4M_DEC,GstY4mDecClass))
#define GST_IS_Y4M_DEC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_Y4M_DEC))
#define GST_IS_Y4M_DEC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_Y4M_DEC))

typedef struct _GstY4mDec GstY4mDec;
typedef struct _GstY4mDecClass GstY4mDecClass;

struct _GstY4mDec
{
  GstElement base_y4mdec;

  GstPad *sinkpad;
  GstPad *srcpad;
  GstAdapter *adapter;

  /* state */
  gboolean have_header;
  int frame_index;
  int header_size;

  gboolean have_new_segment;
  gint64 segment_start;
  gint64 segment_stop;
  gint64 segment_position;

  int width;
  int height;
  GstVideoFormat format;
  gboolean interlaced;
  gboolean tff;
  int fps_n;
  int fps_d;
  int par_n;
  int par_d;
  int frame_size;
};

struct _GstY4mDecClass
{
  GstElementClass base_y4mdec_class;
};

GType gst_y4m_dec_get_type (void);

G_END_DECLS

#endif
