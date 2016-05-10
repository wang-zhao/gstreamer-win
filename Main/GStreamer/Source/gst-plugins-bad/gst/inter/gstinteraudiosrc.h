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

#ifndef _GST_INTER_AUDIO_SRC_H_
#define _GST_INTER_AUDIO_SRC_H_

#include <gst/base/gstbasesrc.h>
#include "gstintersurface.h"

G_BEGIN_DECLS

#define GST_TYPE_INTER_AUDIO_SRC   (gst_inter_audio_src_get_type())
#define GST_INTER_AUDIO_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_INTER_AUDIO_SRC,GstInterAudioSrc))
#define GST_INTER_AUDIO_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_INTER_AUDIO_SRC,GstInterAudioSrcClass))
#define GST_IS_INTER_AUDIO_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_INTER_AUDIO_SRC))
#define GST_IS_INTER_AUDIO_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_INTER_AUDIO_SRC))

typedef struct _GstInterAudioSrc GstInterAudioSrc;
typedef struct _GstInterAudioSrcClass GstInterAudioSrcClass;

struct _GstInterAudioSrc
{
  GstBaseSrc base_interaudiosrc;

  GstInterSurface *surface;

  guint64 n_samples;
  int sample_rate;
};

struct _GstInterAudioSrcClass
{
  GstBaseSrcClass base_interaudiosrc_class;
};

GType gst_inter_audio_src_get_type (void);

G_END_DECLS

#endif
