/* GStreamer
 * Copyright (C) <2011> Stefan Kost <ensonic@users.sf.net>
 *
 * gstwavescope.c: simple oscilloscope
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
/**
 * SECTION:element-wavescope
 * @see_also: goom
 *
 * Wavescope is a simple audio visualisation element. It renders the waveforms
 * like on an oscilloscope.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch audiotestsrc ! audioconvert ! wavescope ! ximagesink
 * ]|
 * </refsect2>
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include "gstwavescope.h"

static GstStaticPadTemplate gst_wave_scope_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_xRGB_HOST_ENDIAN)
    );

static GstStaticPadTemplate gst_wave_scope_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_INT_STANDARD_PAD_TEMPLATE_CAPS)
    );


GST_DEBUG_CATEGORY_STATIC (wave_scope_debug);
#define GST_CAT_DEFAULT wave_scope_debug

enum
{
  PROP_0,
  PROP_STYLE
};

enum
{
  STYLE_DOTS = 0,
  STYLE_LINES,
  STYLE_COLOR_DOTS,
  STYLE_COLOR_LINES,
  NUM_STYLES
};

#define GST_TYPE_WAVE_SCOPE_STYLE (gst_wave_scope_style_get_type ())
static GType
gst_wave_scope_style_get_type (void)
{
  static GType gtype = 0;

  if (gtype == 0) {
    static const GEnumValue values[] = {
      {STYLE_DOTS, "draw dots (default)", "dots"},
      {STYLE_LINES, "draw lines", "lines"},
      {STYLE_COLOR_DOTS, "draw color dots", "color-dots"},
      {STYLE_COLOR_LINES, "draw color lines", "color-lines"},
      {0, NULL, NULL}
    };

    gtype = g_enum_register_static ("GstWaveScopeStyle", values);
  }
  return gtype;
}

static void gst_wave_scope_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_wave_scope_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_wave_scope_finalize (GObject * object);

static void render_dots (GstBaseAudioVisualizer * scope, guint32 * vdata,
    gint16 * adata, guint num_samples);
static void render_lines (GstBaseAudioVisualizer * scope, guint32 * vdata,
    gint16 * adata, guint num_samples);
static void render_color_dots (GstBaseAudioVisualizer * base, guint32 * vdata,
    gint16 * adata, guint num_samples);
static void render_color_lines (GstBaseAudioVisualizer * base, guint32 * vdata,
    gint16 * adata, guint num_samples);

static gboolean gst_wave_scope_setup (GstBaseAudioVisualizer * scope);
static gboolean gst_wave_scope_render (GstBaseAudioVisualizer * base,
    GstBuffer * audio, GstBuffer * video);


GST_BOILERPLATE (GstWaveScope, gst_wave_scope, GstBaseAudioVisualizer,
    GST_TYPE_BASE_AUDIO_VISUALIZER);

static void
gst_wave_scope_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (element_class, "Waveform oscilloscope",
      "Visualization",
      "Simple waveform oscilloscope", "Stefan Kost <ensonic@users.sf.net>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_wave_scope_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_wave_scope_sink_template);
}

static void
gst_wave_scope_class_init (GstWaveScopeClass * g_class)
{
  GObjectClass *gobject_class = (GObjectClass *) g_class;
  GstBaseAudioVisualizerClass *scope_class =
      (GstBaseAudioVisualizerClass *) g_class;

  gobject_class->set_property = gst_wave_scope_set_property;
  gobject_class->get_property = gst_wave_scope_get_property;
  gobject_class->finalize = gst_wave_scope_finalize;

  scope_class->setup = GST_DEBUG_FUNCPTR (gst_wave_scope_setup);
  scope_class->render = GST_DEBUG_FUNCPTR (gst_wave_scope_render);

  g_object_class_install_property (gobject_class, PROP_STYLE,
      g_param_spec_enum ("style", "drawing style",
          "Drawing styles for the wave form display.",
          GST_TYPE_WAVE_SCOPE_STYLE, STYLE_DOTS,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_wave_scope_init (GstWaveScope * scope, GstWaveScopeClass * g_class)
{
  /* do nothing */
}

static void
gst_wave_scope_finalize (GObject * object)
{
  GstWaveScope *scope = GST_WAVE_SCOPE (object);

  if (scope->flt) {
    g_free (scope->flt);
    scope->flt = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_wave_scope_setup (GstBaseAudioVisualizer * bscope)
{
  GstWaveScope *scope = GST_WAVE_SCOPE (bscope);

  if (scope->flt)
    g_free (scope->flt);

  scope->flt = g_new0 (gdouble, 6 * bscope->channels);

  return TRUE;
}

static void
gst_wave_scope_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWaveScope *scope = GST_WAVE_SCOPE (object);

  switch (prop_id) {
    case PROP_STYLE:
      scope->style = g_value_get_enum (value);
      switch (scope->style) {
        case STYLE_DOTS:
          scope->process = render_dots;
          break;
        case STYLE_LINES:
          scope->process = render_lines;
          break;
        case STYLE_COLOR_DOTS:
          scope->process = render_color_dots;
          break;
        case STYLE_COLOR_LINES:
          scope->process = render_color_lines;
          break;
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wave_scope_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWaveScope *scope = GST_WAVE_SCOPE (object);

  switch (prop_id) {
    case PROP_STYLE:
      g_value_set_enum (value, scope->style);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

#include "gstdrawhelpers.h"

static void
render_dots (GstBaseAudioVisualizer * base, guint32 * vdata, gint16 * adata,
    guint num_samples)
{
  gint channels = base->channels;
  guint i, c, s, x, y, oy;
  gfloat dx, dy;
  guint w = base->width;
  guint h = base->height;

  /* draw dots */
  dx = (gfloat) w / (gfloat) num_samples;
  dy = h / 65536.0;
  oy = h / 2;
  for (c = 0; c < channels; c++) {
    s = c;
    for (i = 0; i < num_samples; i++) {
      x = (guint) ((gfloat) i * dx);
      y = (guint) (oy + (gfloat) adata[s] * dy);
      s += channels;
      draw_dot (vdata, x, y, w, 0x00FFFFFF);
    }
  }
}

static void
render_lines (GstBaseAudioVisualizer * base, guint32 * vdata, gint16 * adata,
    guint num_samples)
{
  gint channels = base->channels;
  guint i, c, s, x, y, oy;
  gfloat dx, dy;
  guint w = base->width;
  guint h = base->height;
  gint x2, y2;

  /* draw lines */
  dx = (gfloat) (w - 1) / (gfloat) num_samples;
  dy = (h - 1) / 65536.0;
  oy = (h - 1) / 2;
  for (c = 0; c < channels; c++) {
    s = c;
    x2 = 0;
    y2 = (guint) (oy + (gfloat) adata[s] * dy);
    for (i = 1; i < num_samples; i++) {
      x = (guint) ((gfloat) i * dx);
      y = (guint) (oy + (gfloat) adata[s] * dy);
      s += channels;
      draw_line_aa (vdata, x2, x, y2, y, w, 0x00FFFFFF);
      x2 = x;
      y2 = y;
    }
  }
}

#define CUTOFF_1 0.15
#define CUTOFF_2 0.45
#define RESONANCE (1.0/0.5)

#define filter(in) G_STMT_START {                                              \
  flt[2] = in - (flt[1] * RESONANCE) - flt[0];                                 \
  flt[1] += (flt[2] * CUTOFF_1);                                               \
  flt[0] += (flt[1] * CUTOFF_1);                                               \
                                                                               \
  flt[5] = (flt[1] + flt[2]) - (flt[4] * RESONANCE) - flt[3];                  \
  flt[4] += (flt[5] * CUTOFF_2);                                               \
  flt[3] += (flt[4] * CUTOFF_2);                                               \
} G_STMT_END

static void
render_color_dots (GstBaseAudioVisualizer * base, guint32 * vdata,
    gint16 * adata, guint num_samples)
{
  GstWaveScope *scope = (GstWaveScope *) base;
  gint channels = base->channels;
  guint i, c, s, x, y, oy;
  gfloat dx, dy;
  guint w = base->width;
  guint h = base->height, h1 = h - 2;
  gdouble *flt = scope->flt;

  /* draw dots */
  dx = (gfloat) w / (gfloat) num_samples;
  dy = h / 65536.0;
  oy = h / 2;
  for (c = 0; c < channels; c++) {
    s = c;
    for (i = 0; i < num_samples; i++) {
      x = (guint) ((gfloat) i * dx);
      filter ((gfloat) adata[s]);

      y = (guint) (oy + flt[0] * dy);
      y = CLAMP (y, 0, h1);
      draw_dot_c (vdata, x, y, w, 0x00FF0000);

      y = (guint) (oy + flt[3] * dy);
      y = CLAMP (y, 0, h1);
      draw_dot_c (vdata, x, y, w, 0x0000FF00);

      y = (guint) (oy + (flt[4] + flt[5]) * dy);
      y = CLAMP (y, 0, h1);
      draw_dot_c (vdata, x, y, w, 0x000000FF);

      s += channels;
    }
    flt += 6;
  }
}

static void
render_color_lines (GstBaseAudioVisualizer * base, guint32 * vdata,
    gint16 * adata, guint num_samples)
{
  GstWaveScope *scope = (GstWaveScope *) base;
  gint channels = base->channels;
  guint i, c, s, x, y, oy;
  gfloat dx, dy;
  guint w = base->width;
  guint h = base->height, h1 = h - 2;
  gdouble *flt = scope->flt;
  gint x2, y2, y3, y4;

  /* draw lines */
  dx = (gfloat) (w - 1) / (gfloat) num_samples;
  dy = (h - 1) / 65536.0;
  oy = (h - 1) / 2;
  for (c = 0; c < channels; c++) {
    s = c;

    /* do first pixels */
    x2 = 0;
    filter ((gfloat) adata[s]);

    y = (guint) (oy + flt[0] * dy);
    y2 = CLAMP (y, 0, h1);

    y = (guint) (oy + flt[3] * dy);
    y3 = CLAMP (y, 0, h1);

    y = (guint) (oy + (flt[4] + flt[5]) * dy);
    y4 = CLAMP (y, 0, h1);

    for (i = 1; i < num_samples; i++) {
      x = (guint) ((gfloat) i * dx);
      filter ((gfloat) adata[s]);

      y = (guint) (oy + flt[0] * dy);
      y = CLAMP (y, 0, h1);
      draw_line_aa (vdata, x2, x, y2, y, w, 0x00FF0000);
      y2 = y;

      y = (guint) (oy + flt[3] * dy);
      y = CLAMP (y, 0, h1);
      draw_line_aa (vdata, x2, x, y3, y, w, 0x0000FF00);
      y3 = y;

      y = (guint) (oy + (flt[4] + flt[5]) * dy);
      y = CLAMP (y, 0, h1);
      draw_line_aa (vdata, x2, x, y4, y, w, 0x000000FF);
      y4 = y;

      x2 = x;
      s += channels;
    }
    flt += 6;
  }
}

static gboolean
gst_wave_scope_render (GstBaseAudioVisualizer * base, GstBuffer * audio,
    GstBuffer * video)
{
  GstWaveScope *scope = GST_WAVE_SCOPE (base);
  guint32 *vdata = (guint32 *) GST_BUFFER_DATA (video);
  gint16 *adata = (gint16 *) GST_BUFFER_DATA (audio);
  guint num_samples;

  num_samples = GST_BUFFER_SIZE (audio) / (base->channels * sizeof (gint16));
  scope->process (base, vdata, adata, num_samples);
  return TRUE;
}

gboolean
gst_wave_scope_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (wave_scope_debug, "wavescope", 0, "wavescope");

  return gst_element_register (plugin, "wavescope", GST_RANK_NONE,
      GST_TYPE_WAVE_SCOPE);
}
