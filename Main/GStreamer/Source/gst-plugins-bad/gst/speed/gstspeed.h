/* -*- c-basic-offset: 2 -*-
 * GStreamer
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


#ifndef __GST_SPEED_H__
#define __GST_SPEED_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_SPEED \
  (gst_speed_get_type())
#define GST_SPEED(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SPEED,GstSpeed))
#define GST_SPEED_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SPEED,GstSpeedClass))
#define GST_IS_SPEED(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SPEED))
#define GST_IS_SPEED_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SPEED))

typedef struct _GstSpeed GstSpeed;
typedef struct _GstSpeedClass GstSpeedClass;

enum _GstSpeedFormat {
  GST_SPEED_FORMAT_INT,
  GST_SPEED_FORMAT_FLOAT
};

typedef enum _GstSpeedFormat GstSpeedFormat;

struct _GstSpeed {
  GstElement     element;

  GstPad        *sinkpad;
  GstPad        *srcpad;

  gfloat         speed;

  gint64         offset;
  gint64         timestamp;

  guint          rate;
  guint          channels;
  guint          width;

  guint          sample_size;
  GstSpeedFormat format;
};

struct _GstSpeedClass {
  GstElementClass parent_class;
};

GType gst_speed_get_type(void);

G_END_DECLS

#endif /* __GST_SPEED_H__ */
