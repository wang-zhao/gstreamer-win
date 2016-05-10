/*
 * Farsight Voice+Video library
 *
 *  Copyright 2007 Collabora Ltd, 
 *  Copyright 2007 Nokia Corporation
 *   @author: Philippe Kalaf <philippe.kalaf@collabora.co.uk>.
 *  Copyright 2007 Wim Taymans <wim@fluendo.com>
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
 *
 */

#ifndef __GST_RTP_JITTER_BUFFER_H__
#define __GST_RTP_JITTER_BUFFER_H__

#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>

G_BEGIN_DECLS

/* #define's don't like whitespacey bits */
#define GST_TYPE_RTP_JITTER_BUFFER \
  (gst_rtp_jitter_buffer_get_type())
#define GST_RTP_JITTER_BUFFER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
  GST_TYPE_RTP_JITTER_BUFFER,GstRTPJitterBuffer))
#define GST_RTP_JITTER_BUFFER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
  GST_TYPE_RTP_JITTER_BUFFER,GstRTPJitterBufferClass))
#define GST_IS_RTP_JITTER_BUFFER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_JITTER_BUFFER))
#define GST_IS_RTP_JITTER_BUFFER_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_JITTER_BUFFER))

typedef struct _GstRTPJitterBuffer GstRTPJitterBuffer;
typedef struct _GstRTPJitterBufferClass GstRTPJitterBufferClass;
typedef struct _GstRTPJitterBufferPrivate GstRTPJitterBufferPrivate;

/**
 * GstRTPJitterBuffer:
 *
 * Opaque jitterbuffer structure.
 */
struct _GstRTPJitterBuffer
{
  GstElement parent;

  /*< private >*/
  GstRTPJitterBufferPrivate *priv;

  gpointer _gst_reserved[GST_PADDING];
};

struct _GstRTPJitterBufferClass
{
  GstElementClass parent_class;

  /* signals */
  GstCaps* (*request_pt_map) (GstRTPJitterBuffer *buffer, guint pt);

  void     (*clear_pt_map)   (GstRTPJitterBuffer *buffer);

  /*< private > */
  gpointer _gst_reserved[GST_PADDING];
};

GType gst_rtp_jitter_buffer_get_type (void);

G_END_DECLS

#endif /* __GST_RTP_JITTER_BUFFER_H__ */
