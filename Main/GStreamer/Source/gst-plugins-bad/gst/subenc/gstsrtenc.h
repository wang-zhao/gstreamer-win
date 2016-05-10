/* GStreamer
 * Copyright (C) <2008> Thijs Vermeir <thijsvermeir@gmail.com>
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

#ifndef __GST_SRT_ENC_H__
#define __GST_SRT_ENC_H__

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_TYPE_SRT_ENC \
  (gst_srt_enc_get_type())
#define GST_SRT_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SRT_ENC,GstSrtEnc))
#define GST_SRT_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SRT_ENC,GstSrtEnc))
#define GST_IS_SRT_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SRT_ENC))
#define GST_IS_SRT_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SRT_ENC))

typedef struct _GstSrtEnc GstSrtEnc;
typedef struct _GstSrtEncClass GstSrtEncClass;

struct _GstSrtEncClass
{
  GstElementClass parent_class;
};

struct _GstSrtEnc
{
  GstElement element;

  GstPad *sinkpad;
  GstPad *srcpad;

  /* properties */
  gint64 timestamp;
  gint64 duration;

  /* counter for subtitle entry */
  guint counter;
};

GType gst_srt_enc_get_type (void);

G_END_DECLS
#endif
