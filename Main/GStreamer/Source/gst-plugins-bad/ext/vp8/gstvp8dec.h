/* VP8
 * Copyright (C) 2006 David Schleef <ds@schleef.org>
 * Copyright (C) 2008,2009,2010 Entropy Wave Inc
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
 *
 */

#ifndef __GST_VP8_DEC_H__
#define __GST_VP8_DEC_H__

#include <gst/gst.h>
#include <gst/video/gstbasevideodecoder.h>

/* FIXME: Undef HAVE_CONFIG_H because vpx_codec.h uses it,
 * which causes compilation failures */
#ifdef HAVE_CONFIG_H
#undef HAVE_CONFIG_H
#endif

#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

G_BEGIN_DECLS

#define GST_TYPE_VP8_DEC \
  (gst_vp8_dec_get_type())
#define GST_VP8_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VP8_DEC,GstVP8Dec))
#define GST_VP8_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VP8_DEC,GstVP8DecClass))
#define GST_IS_VP8_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VP8_DEC))
#define GST_IS_VP8_DEC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VP8_DEC))

typedef struct _GstVP8Dec GstVP8Dec;
typedef struct _GstVP8DecClass GstVP8DecClass;

struct _GstVP8Dec
{
  GstBaseVideoDecoder base_video_decoder;

  /* < private > */
  vpx_codec_ctx_t decoder;

  /* state */
  gboolean decoder_inited;

  /* properties */
  gboolean post_processing;
  enum vp8_postproc_level post_processing_flags;
  gint deblocking_level;
  gint noise_level;
};

struct _GstVP8DecClass
{
  GstBaseVideoDecoderClass base_video_decoder_class;
};

GType gst_vp8_dec_get_type (void);

G_END_DECLS

#endif /* __GST_VP8_DEC_H__ */
