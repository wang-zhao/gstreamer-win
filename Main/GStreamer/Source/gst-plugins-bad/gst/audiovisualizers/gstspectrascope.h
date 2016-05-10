/* GStreamer
 * Copyright (C) <2011> Stefan Kost <ensonic@users.sf.net>
 *
 * gstspectrascope.h: simple oscilloscope
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef __GST_SPECTRA_SCOPE_H__
#define __GST_SPECTRA_SCOPE_H__

#include "gstbaseaudiovisualizer.h"
#include <gst/fft/gstffts16.h>

G_BEGIN_DECLS
#define GST_TYPE_SPECTRA_SCOPE            (gst_spectra_scope_get_type())
#define GST_SPECTRA_SCOPE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SPECTRA_SCOPE,GstSpectraScope))
#define GST_SPECTRA_SCOPE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SPECTRA_SCOPE,GstSpectraScopeClass))
#define GST_IS_SPECTRA_SCOPE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SPECTRA_SCOPE))
#define GST_IS_SPECTRA_SCOPE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SPECTRA_SCOPE))
typedef struct _GstSpectraScope GstSpectraScope;
typedef struct _GstSpectraScopeClass GstSpectraScopeClass;

struct _GstSpectraScope
{
  GstBaseAudioVisualizer parent;

  GstFFTS16 *fft_ctx;
  GstFFTS16Complex *freq_data;
};

struct _GstSpectraScopeClass
{
  GstBaseAudioVisualizerClass parent_class;
};

GType gst_spectra_scope_get_type (void);
gboolean gst_spectra_scope_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* __GST_SPECTRA_SCOPE_H__ */