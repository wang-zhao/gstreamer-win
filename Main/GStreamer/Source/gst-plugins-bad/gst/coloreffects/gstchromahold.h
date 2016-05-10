/* GStreamer
 *
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2007 Wim Taymans <wim.taymans@collabora.co.uk>
 * Copyright (C) 2007 Edward Hervey <edward.hervey@collabora.co.uk>
 * Copyright (C) 2007 Jan Schmidt <thaytan@noraisin.net>
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __GST_CHROMA_HOLD_H__
#define __GST_CHROMA_HOLD_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include <gst/controller/gstcontroller.h>

G_BEGIN_DECLS
#define GST_TYPE_CHROMA_HOLD \
  (gst_chroma_hold_get_type())
#define GST_CHROMA_HOLD(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CHROMA_HOLD,GstChromaHold))
#define GST_CHROMA_HOLD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CHROMA_HOLD,GstChromaHoldClass))
#define GST_IS_CHROMA_HOLD(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CHROMA_HOLD))
#define GST_IS_CHROMA_HOLD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CHROMA_HOLD))
typedef struct _GstChromaHold GstChromaHold;
typedef struct _GstChromaHoldClass GstChromaHoldClass;

struct _GstChromaHold
{
  GstVideoFilter parent;

  /* <private> */

  /* caps */
  GStaticMutex lock;

  GstVideoFormat format;
  gint width, height;

  guint target_r;
  guint target_g;
  guint target_b;
  guint tolerance;

  /* processing function */
  void (*process) (guint8 * dest, gint width, gint height,
      GstChromaHold * chroma_hold);

  /* pre-calculated values */
  gint hue;
};

struct _GstChromaHoldClass
{
  GstVideoFilterClass parent_class;
};

GType gst_chroma_hold_get_type (void);

G_END_DECLS
#endif /* __GST_CHROMA_HOLD_H__ */
