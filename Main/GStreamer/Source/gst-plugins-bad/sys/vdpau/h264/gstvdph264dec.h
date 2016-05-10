/* GStreamer
 *
 * Copyright (C) 2009 Carl-Anton Ingmarsson <ca.ingmarsson@gmail.com>.
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

#ifndef __GST_VDP_H264_DEC_H__
#define __GST_VDP_H264_DEC_H__

#include <gst/gst.h>

#include "../gstvdp/gstvdpdecoder.h"

#include "gsth264parser.h"
#include "gsth264dpb.h"

G_BEGIN_DECLS

#define GST_TYPE_VDP_H264_DEC		         (gst_vdp_h264_dec_get_type())
#define GST_VDP_H264_DEC(obj)		         (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VDP_H264_DEC,GstVdpH264Dec))
#define GST_VDP_H264_DEC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VDP_H264_DEC,GstVdpH264DecClass))
#define GST_VDP_H264_DEC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_VDP_H264_DEC,GstVdpH264DecClass))
#define GST_IS_VDP_H264_DEC(obj)	       (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VDP_H264_DEC))
#define GST_IS_VDP_H264_DEC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VDP_H264_DEC))
#define GST_VDP_H264_DEC_CAST(obj)	     ((GstVdpH264Dec *)(obj))

typedef struct _GstVdpH264Dec GstVdpH264Dec;
typedef struct _GstVdpH264DecClass GstVdpH264DecClass;


struct _GstVdpH264Dec {
  GstVdpDecoder vdp_decoder;

  gboolean packetized;
  guint8 nal_length_size;

  GstH264Parser *parser;
  GstH264DPB *dpb;

  GstH264Sequence *sequence;
  gboolean got_idr;
  VdpDecoder decoder;
  
  guint poc_msb;
  guint prev_poc_lsb;
};

struct _GstVdpH264DecClass {
  GstVdpDecoderClass vdp_decoder_class;
};

GType gst_vdp_h264_dec_get_type (void);

G_END_DECLS

#endif /* __GST_VDP_H264_DEC_H__ */