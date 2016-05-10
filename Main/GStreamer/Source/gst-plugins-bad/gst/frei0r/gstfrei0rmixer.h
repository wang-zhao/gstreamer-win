/* GStreamer
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __GST_FREI0R_MIXER_H__
#define __GST_FREI0R_MIXER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstcollectpads.h>

#include "frei0r.h"
#include "gstfrei0r.h"

G_BEGIN_DECLS

#define GST_FREI0R_MIXER(obj) \
  ((GstFrei0rMixer *) obj)
#define GST_FREI0R_MIXER_CLASS(klass) \
  ((GstFrei0rMixerClass *) klass)
#define GST_FREI0R_MIXER_GET_CLASS(obj) \
  ((GstFrei0rMixerClass *) g_type_class_peek (G_TYPE_FROM_INSTANCE (obj)))

typedef struct _GstFrei0rMixer GstFrei0rMixer;
typedef struct _GstFrei0rMixerClass GstFrei0rMixerClass;

struct _GstFrei0rMixer {
  GstElement parent;

  GstCollectPads *collect;
  GstPad *src;
  GstPad *sink0, *sink1, *sink2;

  GstCaps *caps;
  GstVideoFormat fmt;
  gint width, height;

  GstEvent *newseg_event;

  GstPadEventFunction collect_event;

  f0r_instance_t *f0r_instance;
  GstFrei0rPropertyValue *property_cache;
};

struct _GstFrei0rMixerClass {
  GstElementClass parent;

  f0r_plugin_info_t *info;
  GstFrei0rFuncTable *ftable;

  GstFrei0rProperty *properties;
  gint n_properties;
};

GstFrei0rPluginRegisterReturn gst_frei0r_mixer_register (GstPlugin *plugin, const gchar * vendor, const f0r_plugin_info_t *info, const GstFrei0rFuncTable *ftable);

G_END_DECLS

#endif /* __GST_FREI0R_MIXER_H__ */
