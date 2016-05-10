/*
 * GStreamer
 * Copyright 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright 2008 Vincent Penquerc'h <ogg.k.ogg.k@googlemail.com>
 * Copyright (C) <2009> Young-Ho Cha <ganadist@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

/**
 * SECTION:element-tiger
 * @see_also: katedec
 *
 * <refsect2>
 * <para>
 * This element decodes and renders Kate streams
 * <ulink url="http://libkate.googlecode.com/">Kate</ulink> is a free codec
 * for text based data, such as subtitles. Any number of kate streams can be
 * embedded in an Ogg stream.
 * </para>
 * <para>
 * libkate (see above url) and <ulink url="http://libtiger.googlecode.com/">libtiger</ulink>
 * are needed to build this element.
 * </para>
 * <title>Example pipeline</title>
 * <para>
 * This pipeline renders a Kate stream on top of a Theora video multiplexed
 * in the same stream:
 * <programlisting>
 * gst-launch \
 *   filesrc location=video.ogg ! oggdemux name=demux \
 *   demux. ! queue ! theoradec ! ffmpegcolorspace ! tiger name=tiger \
 *   demux. ! queue ! kateparse ! tiger. \
 *   tiger. ! ffmpegcolorspace ! autovideosink
 * </programlisting>
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstkate.h"
#include "gstkatetiger.h"

GST_DEBUG_CATEGORY_EXTERN (gst_katetiger_debug);
#define GST_CAT_DEFAULT gst_katetiger_debug

#define GST_KATE_TIGER_MUTEX_LOCK(element) \
  do { \
    /*GST_LOG_OBJECT ((element), "locking from %s:%d\n",__FILE__,__LINE__);*/ \
    g_mutex_lock ((element)->mutex); \
    /*GST_LOG_OBJECT ((element), "ready from %s:%d\n",__FILE__,__LINE__);*/ \
  } while(0)

#define GST_KATE_TIGER_MUTEX_UNLOCK(element) \
  do { \
    /*GST_LOG_OBJECT ((element), "unlocking from %s:%d\n",__FILE__,__LINE__);*/ \
    g_mutex_unlock ((element)->mutex); \
  } while(0)

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_DEFAULT_FONT_DESC = DECODER_BASE_ARG_COUNT,
  ARG_QUALITY,
  ARG_DEFAULT_FONT_EFFECT,
  ARG_DEFAULT_FONT_EFFECT_STRENGTH,
  ARG_DEFAULT_FONT_RED,
  ARG_DEFAULT_FONT_GREEN,
  ARG_DEFAULT_FONT_BLUE,
  ARG_DEFAULT_FONT_ALPHA,
  ARG_DEFAULT_BACKGROUND_RED,
  ARG_DEFAULT_BACKGROUND_GREEN,
  ARG_DEFAULT_BACKGROUND_BLUE,
  ARG_DEFAULT_BACKGROUND_ALPHA,
  ARG_SILENT
};

/* RGB -> YUV blitting routines taken from textoverlay,
   original code from Young-Ho Cha <ganadist@gmail.com> */

#define COMP_Y(ret, r, g, b) \
{ \
   ret = (int) (((19595 * r) >> 16) + ((38470 * g) >> 16) + ((7471 * b) >> 16)); \
   ret = CLAMP (ret, 0, 255); \
}

#define COMP_U(ret, r, g, b) \
{ \
   ret = (int) (-((11059 * r) >> 16) - ((21709 * g) >> 16) + ((32768 * b) >> 16) + 128); \
   ret = CLAMP (ret, 0, 255); \
}

#define COMP_V(ret, r, g, b) \
{ \
   ret = (int) (((32768 * r) >> 16) - ((27439 * g) >> 16) - ((5329 * b) >> 16) + 128); \
   ret = CLAMP (ret, 0, 255); \
}

#define BLEND(ret, alpha, v0, v1) \
{ \
	ret = (v0 * alpha + v1 * (255 - alpha)) / 255; \
}

#define OVER(ret, alphaA, Ca, alphaB, Cb, alphaNew)	\
{ \
    gint _tmp; \
    _tmp = (Ca * alphaA + Cb * alphaB * (255 - alphaA) / 255) / alphaNew; \
    ret = CLAMP (_tmp, 0, 255); \
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
# define TIGER_ARGB_A 3
# define TIGER_ARGB_R 2
# define TIGER_ARGB_G 1
# define TIGER_ARGB_B 0
#else
# define TIGER_ARGB_A 0
# define TIGER_ARGB_R 1
# define TIGER_ARGB_G 2
# define TIGER_ARGB_B 3
#endif

#define TIGER_UNPREMULTIPLY(a,r,g,b) G_STMT_START { \
  b = (a > 0) ? MIN ((b * 255 + a / 2) / a, 255) : 0; \
  g = (a > 0) ? MIN ((g * 255 + a / 2) / a, 255) : 0; \
  r = (a > 0) ? MIN ((r * 255 + a / 2) / a, 255) : 0; \
} G_STMT_END

static GstStaticPadTemplate kate_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("subtitle_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("subtitle/x-kate; application/x-kate")
    );

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define TIGER_VIDEO_CAPS \
    GST_VIDEO_CAPS_xRGB ";" GST_VIDEO_CAPS_BGRx ";" \
    GST_VIDEO_CAPS_YUV ("{AYUV, I420, YV12, UYVY, NV12, NV21}")
#else
#define TIGER_VIDEO_CAPS \
    GST_VIDEO_CAPS_BGRx ";" GST_VIDEO_CAPS_xRGB ";" \
    GST_VIDEO_CAPS_YUV ("{AYUV, I420, YV12, UYVY, NV12, NV21}")
#endif

static GstStaticPadTemplate video_sink_factory =
GST_STATIC_PAD_TEMPLATE ("video_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIGER_VIDEO_CAPS));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIGER_VIDEO_CAPS));

GST_BOILERPLATE (GstKateTiger, gst_kate_tiger, GstElement, GST_TYPE_ELEMENT);

static GType
gst_kate_tiger_font_effect_get_type (void)
{
  static GType font_effect_type = 0;

  if (!font_effect_type) {
    static const GEnumValue font_effects[] = {
      {tiger_font_plain, "none", "none"},
      {tiger_font_shadow, "shadow", "shadow"},
      {tiger_font_outline, "outline", "outline"},
      {0, NULL, NULL}
    };
    font_effect_type = g_enum_register_static ("GstFontEffect", font_effects);
  }

  return font_effect_type;
}

static void gst_kate_tiger_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_kate_tiger_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_kate_tiger_dispose (GObject * object);

static GstFlowReturn gst_kate_tiger_kate_chain (GstPad * pad, GstBuffer * buf);
static GstFlowReturn gst_kate_tiger_video_chain (GstPad * pad, GstBuffer * buf);
static GstStateChangeReturn gst_kate_tiger_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_kate_tiger_kate_sink_query (GstPad * pad, GstQuery * query);
static gboolean gst_kate_tiger_kate_event (GstPad * pad, GstEvent * event);
static gboolean gst_kate_tiger_video_event (GstPad * pad, GstEvent * event);
static gboolean gst_kate_tiger_video_set_caps (GstPad * pad, GstCaps * caps);
static gboolean gst_kate_tiger_source_event (GstPad * pad, GstEvent * event);

static void
gst_kate_tiger_base_init (gpointer gclass)
{

  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &kate_sink_factory);
  gst_element_class_add_static_pad_template (element_class,
      &video_sink_factory);
  gst_element_class_set_details_simple (element_class, "Kate stream renderer",
      "Mixer/Video/Overlay/Subtitle",
      "Decodes and renders Kate streams on top of a video",
      "Vincent Penquerc'h <ogg.k.ogg.k@googlemail.com>");
}

/* initialize the plugin's class */
static void
gst_kate_tiger_class_init (GstKateTigerClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_kate_tiger_get_property);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_kate_tiger_set_property);
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_kate_tiger_dispose);

  gst_kate_util_install_decoder_base_properties (gobject_class);

  g_object_class_install_property (gobject_class, ARG_QUALITY,
      g_param_spec_double ("quality", "Rendering quality",
          "Rendering quality (0 is faster, 1 is best and slower)",
          0.0, 1.0, 1.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_DEFAULT_FONT_DESC,
      g_param_spec_string ("default-font-desc", "Default font description",
          "Default font description (Pango style) to render text with",
          "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_DEFAULT_FONT_EFFECT,
      g_param_spec_enum ("default-font-effect", "Default font effect",
          "Whether to apply an effect to text by default, for increased readability",
          gst_kate_tiger_font_effect_get_type (),
          tiger_font_outline,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      ARG_DEFAULT_FONT_EFFECT_STRENGTH,
      g_param_spec_double ("default-font-effect-strength",
          "Default font effect strength",
          "How pronounced should the font effect be (effect dependent)", 0.0,
          1.0, 0.5, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_DEFAULT_FONT_RED,
      g_param_spec_int ("default-font-red",
          "Default font color (red component)",
          "Default font color (red component, between 0 and 255) to render text with",
          0, 255, 255, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_DEFAULT_FONT_GREEN,
      g_param_spec_int ("default-font-green",
          "Default font color (green component)",
          "Default font color (green component, between 0 and 255) to render text with",
          0, 255, 255, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_DEFAULT_FONT_BLUE,
      g_param_spec_int ("default-font-blue",
          "Default font color (blue component)",
          "Default font color (blue component, between 0 and 255) to render text with",
          0, 255, 255, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_DEFAULT_FONT_ALPHA,
      g_param_spec_int ("default-font-alpha",
          "Default font color (alpha component)",
          "Default font color (alpha component, between 0 and 255) to render text with",
          0, 255, 255, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_DEFAULT_BACKGROUND_RED,
      g_param_spec_int ("default-background-red",
          "Default background color (red component)",
          "Default background color (red component, between 0 and 255) to render text with",
          0, 255, 255, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_DEFAULT_BACKGROUND_GREEN,
      g_param_spec_int ("default-background-green",
          "Default background color (green component)",
          "Default background color (green component, between 0 and 255) to render text with",
          0, 255, 255, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_DEFAULT_BACKGROUND_BLUE,
      g_param_spec_int ("default-background-blue",
          "Default background color (blue component)",
          "Default background color (blue component, between 0 and 255) to render text with",
          0, 255, 255, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_DEFAULT_BACKGROUND_ALPHA,
      g_param_spec_int ("default-background-alpha",
          "Default background color (alpha component)",
          "Default background color (alpha component, between 0 and 255) to render text with",
          0, 255, 255, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* FIXME 0.11: rename to "visible" or "text-visible" or "render-text" */
  g_object_class_install_property (gobject_class, ARG_SILENT,
      g_param_spec_boolean ("silent", "silent",
          "Whether to render the stream",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_kate_tiger_change_state);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_kate_tiger_init (GstKateTiger * tiger, GstKateTigerClass * gclass)
{
  GST_DEBUG_OBJECT (tiger, "gst_kate_tiger_init");

  tiger->mutex = g_mutex_new ();
  tiger->cond = g_cond_new ();

  tiger->katesinkpad =
      gst_pad_new_from_static_template (&kate_sink_factory, "subtitle_sink");
  gst_pad_set_chain_function (tiger->katesinkpad,
      GST_DEBUG_FUNCPTR (gst_kate_tiger_kate_chain));
  gst_pad_set_query_function (tiger->katesinkpad,
      GST_DEBUG_FUNCPTR (gst_kate_tiger_kate_sink_query));
  gst_pad_set_event_function (tiger->katesinkpad,
      GST_DEBUG_FUNCPTR (gst_kate_tiger_kate_event));
  gst_element_add_pad (GST_ELEMENT (tiger), tiger->katesinkpad);

  tiger->videosinkpad =
      gst_pad_new_from_static_template (&video_sink_factory, "video_sink");
  gst_pad_set_chain_function (tiger->videosinkpad,
      GST_DEBUG_FUNCPTR (gst_kate_tiger_video_chain));
  gst_pad_use_fixed_caps (tiger->videosinkpad);
  gst_pad_set_setcaps_function (tiger->videosinkpad,
      GST_DEBUG_FUNCPTR (gst_kate_tiger_video_set_caps));
  gst_pad_set_event_function (tiger->videosinkpad,
      GST_DEBUG_FUNCPTR (gst_kate_tiger_video_event));
  gst_element_add_pad (GST_ELEMENT (tiger), tiger->videosinkpad);

  tiger->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_event_function (tiger->srcpad, gst_kate_tiger_source_event);
  gst_pad_use_fixed_caps (tiger->srcpad);
  gst_element_add_pad (GST_ELEMENT (tiger), tiger->srcpad);

  gst_kate_util_decode_base_init (&tiger->decoder, FALSE);

  tiger->tr = NULL;

  tiger->default_font_desc = NULL;
  tiger->quality = -1.0;
  tiger->default_font_effect = tiger_font_outline;
  tiger->default_font_effect_strength = 0.5;
  tiger->default_font_r = 255;
  tiger->default_font_g = 255;
  tiger->default_font_b = 255;
  tiger->default_font_a = 255;
  tiger->default_background_r = 0;
  tiger->default_background_g = 0;
  tiger->default_background_b = 0;
  tiger->default_background_a = 0;
  tiger->silent = FALSE;

  tiger->video_width = 0;
  tiger->video_height = 0;

  tiger->seen_header = FALSE;
}

static void
gst_kate_tiger_dispose (GObject * object)
{
  GstKateTiger *tiger = GST_KATE_TIGER (object);

  GST_LOG_OBJECT (tiger, "disposing");

  if (tiger->default_font_desc) {
    g_free (tiger->default_font_desc);
    tiger->default_font_desc = NULL;
  }

  g_free (tiger->render_buffer);
  tiger->render_buffer = NULL;

  g_cond_free (tiger->cond);
  tiger->cond = NULL;

  g_mutex_free (tiger->mutex);
  tiger->mutex = NULL;

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_kate_tiger_update_quality (GstKateTiger * tiger)
{
  if (tiger->tr && tiger->quality >= 0.0) {
    tiger_renderer_set_quality (tiger->tr, tiger->quality);
  }
}

static void
gst_kate_tiger_update_default_font_effect (GstKateTiger * tiger)
{
  if (tiger->tr) {
    tiger_renderer_set_default_font_effect (tiger->tr,
        tiger->default_font_effect, tiger->default_font_effect_strength);
  }
}

static void
gst_kate_tiger_update_default_font_color (GstKateTiger * tiger)
{
  if (tiger->tr) {
    tiger_renderer_set_default_font_color (tiger->tr,
        tiger->default_font_r / 255.0,
        tiger->default_font_g / 255.0,
        tiger->default_font_b / 255.0, tiger->default_font_a / 255.0);
  }
}

static void
gst_kate_tiger_update_default_background_color (GstKateTiger * tiger)
{
  if (tiger->tr) {
    tiger_renderer_set_default_background_fill_color (tiger->tr,
        tiger->default_background_r / 255.0,
        tiger->default_background_g / 255.0,
        tiger->default_background_b / 255.0,
        tiger->default_background_a / 255.0);
  }
}

static void
gst_kate_tiger_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstKateTiger *tiger = GST_KATE_TIGER (object);
  const char *str;

  GST_KATE_TIGER_MUTEX_LOCK (tiger);

  switch (prop_id) {
    case ARG_DEFAULT_FONT_DESC:
      if (tiger->default_font_desc) {
        g_free (tiger->default_font_desc);
        tiger->default_font_desc = NULL;
      }
      str = g_value_get_string (value);
      if (str) {
        tiger->default_font_desc = g_strdup (str);
        if (tiger->tr)
          tiger_renderer_set_default_font_description (tiger->tr,
              tiger->default_font_desc);
      }
      break;
    case ARG_QUALITY:
      tiger->quality = g_value_get_double (value);
      gst_kate_tiger_update_quality (tiger);
      break;
    case ARG_DEFAULT_FONT_EFFECT:
      tiger->default_font_effect = g_value_get_enum (value);
      gst_kate_tiger_update_default_font_effect (tiger);
      break;
    case ARG_DEFAULT_FONT_EFFECT_STRENGTH:
      tiger->default_font_effect_strength = g_value_get_double (value);
      gst_kate_tiger_update_default_font_effect (tiger);
      break;
    case ARG_DEFAULT_FONT_RED:
      tiger->default_font_r = g_value_get_int (value);
      gst_kate_tiger_update_default_font_color (tiger);
      break;
    case ARG_DEFAULT_FONT_GREEN:
      tiger->default_font_g = g_value_get_int (value);
      gst_kate_tiger_update_default_font_color (tiger);
      break;
    case ARG_DEFAULT_FONT_BLUE:
      tiger->default_font_b = g_value_get_int (value);
      gst_kate_tiger_update_default_font_color (tiger);
      break;
    case ARG_DEFAULT_FONT_ALPHA:
      tiger->default_font_a = g_value_get_int (value);
      gst_kate_tiger_update_default_font_color (tiger);
      break;
    case ARG_DEFAULT_BACKGROUND_RED:
      tiger->default_background_r = g_value_get_int (value);
      gst_kate_tiger_update_default_background_color (tiger);
      break;
    case ARG_DEFAULT_BACKGROUND_GREEN:
      tiger->default_background_g = g_value_get_int (value);
      gst_kate_tiger_update_default_background_color (tiger);
      break;
    case ARG_DEFAULT_BACKGROUND_BLUE:
      tiger->default_background_b = g_value_get_int (value);
      gst_kate_tiger_update_default_background_color (tiger);
      break;
    case ARG_DEFAULT_BACKGROUND_ALPHA:
      tiger->default_background_a = g_value_get_int (value);
      gst_kate_tiger_update_default_background_color (tiger);
      break;
    case ARG_SILENT:
      tiger->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
}

static void
gst_kate_tiger_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstKateTiger *tiger = GST_KATE_TIGER (object);

  GST_KATE_TIGER_MUTEX_LOCK (tiger);

  switch (prop_id) {
    case ARG_DEFAULT_FONT_DESC:
      g_value_set_string (value,
          tiger->default_font_desc ? tiger->default_font_desc : "");
      break;
    case ARG_QUALITY:
      g_value_set_double (value, tiger->quality);
      break;
    case ARG_DEFAULT_FONT_EFFECT:
      g_value_set_enum (value, tiger->default_font_effect);
      break;
    case ARG_DEFAULT_FONT_EFFECT_STRENGTH:
      g_value_set_double (value, tiger->default_font_effect_strength);
      break;
    case ARG_DEFAULT_FONT_RED:
      g_value_set_int (value, tiger->default_font_r);
      break;
    case ARG_DEFAULT_FONT_GREEN:
      g_value_set_int (value, tiger->default_font_g);
      break;
    case ARG_DEFAULT_FONT_BLUE:
      g_value_set_int (value, tiger->default_font_b);
      break;
    case ARG_DEFAULT_FONT_ALPHA:
      g_value_set_int (value, tiger->default_font_a);
      break;
    case ARG_DEFAULT_BACKGROUND_RED:
      g_value_set_int (value, tiger->default_background_r);
      break;
    case ARG_DEFAULT_BACKGROUND_GREEN:
      g_value_set_int (value, tiger->default_background_g);
      break;
    case ARG_DEFAULT_BACKGROUND_BLUE:
      g_value_set_int (value, tiger->default_background_b);
      break;
    case ARG_DEFAULT_BACKGROUND_ALPHA:
      g_value_set_int (value, tiger->default_background_a);
      break;
    case ARG_SILENT:
      g_value_set_boolean (value, tiger->silent);
      break;
    default:
      if (!gst_kate_util_decoder_base_get_property (&tiger->decoder, object,
              prop_id, value, pspec)) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
      break;
  }

  GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
}

/* GstElement vmethod implementations */

/* chain function
 * this function does the actual processing
 */

static GstFlowReturn
gst_kate_tiger_kate_chain (GstPad * pad, GstBuffer * buf)
{
  GstKateTiger *tiger = GST_KATE_TIGER (gst_pad_get_parent (pad));
  const kate_event *ev = NULL;
  GstFlowReturn rflow = GST_FLOW_OK;

  GST_KATE_TIGER_MUTEX_LOCK (tiger);

  GST_LOG_OBJECT (tiger, "Got kate buffer, caps %" GST_PTR_FORMAT,
      GST_BUFFER_CAPS (buf));

  /* Now that we have the lock, check if we're flushing */
  if (tiger->decoder.kate_flushing) {
    GST_DEBUG_OBJECT (tiger, "Flushing, disregarding buffer");
    goto done;
  }

  /* Unfortunately, it can happen that the start of the stream is not sent,
     for instance if there's a stream selector upstream, which is switched
     from another Kate stream. If this happens, then we can fallback on the
     headers stored in the caps (if any). */
  if (!tiger->seen_header) {
    if (GST_BUFFER_SIZE (buf) == 0 || (GST_BUFFER_DATA (buf)[0] & 0x80) == 0) {
      /* Not a header, try to fall back on caps */
      GstStructure *s;
      const GValue *streamheader;

      GST_INFO_OBJECT (tiger, "Headers not seen, start of stream is cut off");
      s = gst_caps_get_structure (GST_BUFFER_CAPS (buf), 0);
      streamheader = gst_structure_get_value (s, "streamheader");
      if (streamheader && G_VALUE_TYPE (streamheader) == GST_TYPE_ARRAY) {
        GstPad *tagpad = gst_pad_get_peer (pad);
        GArray *array;
        gint i;

        GST_INFO_OBJECT (tiger, "Falling back on caps to initialize decoder");
        array = g_value_peek_pointer (streamheader);
        for (i = 0; i < array->len; i++) {
          GValue *value = &g_array_index (array, GValue, i);
          if (G_VALUE_TYPE (value) == GST_TYPE_BUFFER) {
            GstBuffer *hbuf = g_value_peek_pointer (value);
            gst_buffer_ref (hbuf);
            rflow =
                gst_kate_util_decoder_base_chain_kate_packet (&tiger->decoder,
                GST_ELEMENT_CAST (tiger), pad, hbuf, tiger->srcpad, tagpad,
                NULL, NULL);
          } else {
            GST_WARNING_OBJECT (tiger,
                "Streamheader index %d does not hold a buffer", i);
          }
        }
        gst_object_unref (tagpad);
        tiger->seen_header = TRUE;
      } else {
        GST_WARNING_OBJECT (tiger, "No headers seen, and no headers on caps");
      }
    } else {
      tiger->seen_header = TRUE;
    }
  }

  if (gst_kate_util_decoder_base_update_segment (&tiger->decoder,
          GST_ELEMENT_CAST (tiger), buf)) {
    GstPad *tagpad = gst_pad_get_peer (pad);
    rflow =
        gst_kate_util_decoder_base_chain_kate_packet (&tiger->decoder,
        GST_ELEMENT_CAST (tiger), pad, buf, tiger->srcpad, tagpad, NULL, &ev);
    if (G_LIKELY (rflow == GST_FLOW_OK)) {
      if (ev) {
        int ret = tiger_renderer_add_event (tiger->tr, ev->ki, ev);
        GST_INFO_OBJECT (tiger, "adding event for %p from %f to %f: %p, \"%s\"",
            ev->ki, ev->start_time, ev->end_time, ev->bitmap, ev->text);
        if (G_UNLIKELY (ret < 0)) {
          GST_WARNING_OBJECT (tiger,
              "failed to add Kate event to Tiger renderer: %s",
              gst_kate_util_get_error_message (ret));
        }
      }
    }
    gst_object_unref (tagpad);
  }

  /* we want to avoid shooting ahead of the video stream, or we will
     get segment updates which will place us ahead of it, and we won't
     be able to convert a video timestamp back into a kate timestamp */
  if (G_LIKELY (GST_BUFFER_TIMESTAMP_IS_VALID (buf))) {
    while (1) {
      gint64 kate_time, video_time;
      kate_time =
          gst_segment_to_running_time (&tiger->decoder.kate_segment,
          GST_FORMAT_TIME, GST_BUFFER_TIMESTAMP (buf));
      video_time =
          gst_segment_to_running_time (&tiger->video_segment, GST_FORMAT_TIME,
          tiger->video_segment.last_stop);
      GST_DEBUG_OBJECT (tiger, "Kate time %.2f, video time %.2f (kts %ld)",
          kate_time / (float) GST_SECOND, video_time / (float) GST_SECOND,
          (long) GST_BUFFER_TIMESTAMP (buf));
      if (kate_time <= video_time) {
        break;
      }
      GST_LOG_OBJECT (tiger, "Waiting to return from chain function");
      g_cond_wait (tiger->cond, tiger->mutex);
      if (tiger->decoder.kate_flushing) {
        GST_DEBUG_OBJECT (tiger, "Flushing while waiting");
        break;
      }
      GST_LOG_OBJECT (tiger, "Woken up, checking time again");
    }
  }

done:
  GST_KATE_TIGER_MUTEX_UNLOCK (tiger);

  gst_object_unref (tiger);
  gst_buffer_unref (buf);

  return rflow;
}

static gboolean
gst_kate_tiger_video_set_caps (GstPad * pad, GstCaps * caps)
{
  GstKateTiger *tiger = GST_KATE_TIGER (gst_pad_get_parent (pad));
  GstVideoFormat format;
  gint w, h;
  gboolean ret;

  GST_KATE_TIGER_MUTEX_LOCK (tiger);

  /* Cairo expects ARGB in native endianness, and that's what we get
     as we've forced it in the caps. We might allow swapped red/blue
     at some point, and get tiger to swap, to make some cases faster */
  tiger->swap_rgb = FALSE;

  if (gst_video_format_parse_caps (caps, &format, &w, &h)) {
    tiger->video_format = format;
    tiger->video_width = w;
    tiger->video_height = h;
  }

  GST_KATE_TIGER_MUTEX_UNLOCK (tiger);

  ret = gst_pad_set_caps (tiger->srcpad, caps);

  gst_object_unref (tiger);
  return ret;
}

static gdouble
gst_kate_tiger_get_time (GstKateTiger * tiger)
{
  gint64 rt =
      gst_segment_to_running_time (&tiger->video_segment, GST_FORMAT_TIME,
      tiger->video_segment.last_stop);
  gint64 pos =
      gst_segment_to_position (&tiger->decoder.kate_segment, GST_FORMAT_TIME,
      rt);
  return pos / (gdouble) GST_SECOND;
}

static inline void
gst_kate_tiger_blit_1 (GstKateTiger * tiger, guchar * dest, gint xpos,
    gint ypos, const guint8 * image, gint image_width, gint image_height,
    guint dest_stride)
{
  gint i, j = 0;
  gint x, y;
  guchar r, g, b, a;
  const guint8 *pimage;
  guchar *py;
  gint width = image_width;
  gint height = image_height;

  if (xpos < 0) {
    xpos = 0;
  }

  if (xpos + width > tiger->video_width) {
    width = tiger->video_width - xpos;
  }

  if (ypos + height > tiger->video_height) {
    height = tiger->video_height - ypos;
  }

  dest += (ypos / 1) * dest_stride;

  for (i = 0; i < height; i++) {
    pimage = image + 4 * (i * image_width);
    py = dest + i * dest_stride + xpos;
    for (j = 0; j < width; j++) {
      b = pimage[TIGER_ARGB_B];
      g = pimage[TIGER_ARGB_G];
      r = pimage[TIGER_ARGB_R];
      a = pimage[TIGER_ARGB_A];
      TIGER_UNPREMULTIPLY (a, r, g, b);

      pimage += 4;
      if (a == 0) {
        py++;
        continue;
      }
      COMP_Y (y, r, g, b);
      x = *py;
      BLEND (*py++, a, y, x);
    }
  }
}

static inline void
gst_kate_tiger_blit_sub2x2cbcr (GstKateTiger * tiger,
    guchar * destcb, guchar * destcr, gint xpos, gint ypos,
    const guint8 * image, gint image_width, gint image_height,
    guint destcb_stride, guint destcr_stride, guint pix_stride)
{
  gint i, j;
  gint x, cb, cr;
  gushort r, g, b, a;
  gushort r1, g1, b1, a1;
  const guint8 *pimage1, *pimage2;
  guchar *pcb, *pcr;
  gint width = image_width - 2;
  gint height = image_height - 2;

  xpos *= pix_stride;

  if (xpos < 0) {
    xpos = 0;
  }

  if (xpos + width > tiger->video_width) {
    width = tiger->video_width - xpos;
  }

  if (ypos + height > tiger->video_height) {
    height = tiger->video_height - ypos;
  }

  destcb += (ypos / 2) * destcb_stride;
  destcr += (ypos / 2) * destcr_stride;

  for (i = 0; i < height; i += 2) {
    pimage1 = image + 4 * (i * image_width);
    pimage2 = pimage1 + 4 * image_width;
    pcb = destcb + (i / 2) * destcb_stride + xpos / 2;
    pcr = destcr + (i / 2) * destcr_stride + xpos / 2;
    for (j = 0; j < width; j += 2) {
      b = pimage1[TIGER_ARGB_B];
      g = pimage1[TIGER_ARGB_G];
      r = pimage1[TIGER_ARGB_R];
      a = pimage1[TIGER_ARGB_A];
      TIGER_UNPREMULTIPLY (a, r, g, b);
      pimage1 += 4;

      b1 = pimage1[TIGER_ARGB_B];
      g1 = pimage1[TIGER_ARGB_G];
      r1 = pimage1[TIGER_ARGB_R];
      a1 = pimage1[TIGER_ARGB_A];
      TIGER_UNPREMULTIPLY (a1, r1, g1, b1);
      b += b1;
      g += g1;
      r += r1;
      a += a1;
      pimage1 += 4;

      b1 = pimage2[TIGER_ARGB_B];
      g1 = pimage2[TIGER_ARGB_G];
      r1 = pimage2[TIGER_ARGB_R];
      a1 = pimage2[TIGER_ARGB_A];
      TIGER_UNPREMULTIPLY (a1, r1, g1, b1);
      b += b1;
      g += g1;
      r += r1;
      a += a1;
      pimage2 += 4;

      /* + 2 for rounding */
      b1 = pimage2[TIGER_ARGB_B];
      g1 = pimage2[TIGER_ARGB_G];
      r1 = pimage2[TIGER_ARGB_R];
      a1 = pimage2[TIGER_ARGB_A];
      TIGER_UNPREMULTIPLY (a1, r1, g1, b1);
      b += b1 + 2;
      g += g1 + 2;
      r += r1 + 2;
      a += a1 + 2;
      pimage2 += 4;

      b /= 4;
      g /= 4;
      r /= 4;
      a /= 4;

      if (a == 0) {
        pcb += pix_stride;
        pcr += pix_stride;
        continue;
      }
      COMP_U (cb, r, g, b);
      COMP_V (cr, r, g, b);

      x = *pcb;
      BLEND (*pcb, a, cb, x);
      x = *pcr;
      BLEND (*pcr, a, cr, x);

      pcb += pix_stride;
      pcr += pix_stride;
    }
  }
}

/* FIXME:
 *  - use proper strides and offset for I420
 */

static inline void
gst_kate_tiger_blit_NV12_NV21 (GstKateTiger * tiger,
    guint8 * yuv_pixels, gint xpos, gint ypos, const guint8 * image,
    gint image_width, gint image_height)
{
  int y_stride, uv_stride;
  int u_offset, v_offset;
  int h, w;

  /* because U/V is 2x2 subsampled, we need to round, either up or down,
   * to a boundary of integer number of U/V pixels:
   */
  xpos = GST_ROUND_UP_2 (xpos);
  ypos = GST_ROUND_UP_2 (ypos);

  w = tiger->video_width;
  h = tiger->video_height;

  y_stride = gst_video_format_get_row_stride (tiger->video_format, 0, w);
  uv_stride = gst_video_format_get_row_stride (tiger->video_format, 1, w);
  u_offset =
      gst_video_format_get_component_offset (tiger->video_format, 1, w, h);
  v_offset =
      gst_video_format_get_component_offset (tiger->video_format, 2, w, h);

  gst_kate_tiger_blit_1 (tiger, yuv_pixels, xpos, ypos, image, image_width,
      image_height, y_stride);
  gst_kate_tiger_blit_sub2x2cbcr (tiger, yuv_pixels + u_offset,
      yuv_pixels + v_offset, xpos, ypos, image, image_width, image_height,
      uv_stride, uv_stride, 2);
}

static inline void
gst_kate_tiger_blit_I420_YV12 (GstKateTiger * tiger,
    guint8 * yuv_pixels, gint xpos, gint ypos, const guint8 * image,
    gint image_width, gint image_height)
{
  int y_stride, u_stride, v_stride;
  int u_offset, v_offset;
  int h, w;

  /* because U/V is 2x2 subsampled, we need to round, either up or down,
   * to a boundary of integer number of U/V pixels:
   */
  xpos = GST_ROUND_UP_2 (xpos);
  ypos = GST_ROUND_UP_2 (ypos);

  w = tiger->video_width;
  h = tiger->video_height;

  y_stride = gst_video_format_get_row_stride (tiger->video_format, 0, w);
  u_stride = gst_video_format_get_row_stride (tiger->video_format, 1, w);
  v_stride = gst_video_format_get_row_stride (tiger->video_format, 2, w);
  u_offset =
      gst_video_format_get_component_offset (tiger->video_format, 1, w, h);
  v_offset =
      gst_video_format_get_component_offset (tiger->video_format, 2, w, h);

  gst_kate_tiger_blit_1 (tiger, yuv_pixels, xpos, ypos, image, image_width,
      image_height, y_stride);
  gst_kate_tiger_blit_sub2x2cbcr (tiger, yuv_pixels + u_offset,
      yuv_pixels + v_offset, xpos, ypos, image, image_width, image_height,
      u_stride, v_stride, 1);
}

static inline void
gst_kate_tiger_blit_UYVY (GstKateTiger * tiger,
    guint8 * yuv_pixels, gint xpos, gint ypos, const guint8 * image,
    gint image_width, gint image_height)
{
  int a0, r0, g0, b0;
  int a1, r1, g1, b1;
  int y0, y1, u, v;
  int i, j;
  int h, w;
  const guint8 *pimage;
  guchar *dest;

  /* because U/V is 2x horizontally subsampled, we need to round to a
   * boundary of integer number of U/V pixels in x dimension:
   */
  xpos = GST_ROUND_UP_2 (xpos);

  w = image_width - 2;
  h = image_height - 2;

  if (xpos < 0) {
    xpos = 0;
  }

  if (xpos + w > tiger->video_width) {
    w = tiger->video_width - xpos;
  }

  if (ypos + h > tiger->video_height) {
    h = tiger->video_height - ypos;
  }

  for (i = 0; i < h; i++) {
    pimage = image + i * image_width * 4;
    dest = yuv_pixels + (i + ypos) * tiger->video_width * 2 + xpos * 2;
    for (j = 0; j < w; j += 2) {
      b0 = pimage[TIGER_ARGB_B];
      g0 = pimage[TIGER_ARGB_G];
      r0 = pimage[TIGER_ARGB_R];
      a0 = pimage[TIGER_ARGB_A];
      TIGER_UNPREMULTIPLY (a0, r0, g0, b0);
      pimage += 4;

      b1 = pimage[TIGER_ARGB_B];
      g1 = pimage[TIGER_ARGB_G];
      r1 = pimage[TIGER_ARGB_R];
      a1 = pimage[TIGER_ARGB_A];
      TIGER_UNPREMULTIPLY (a1, r1, g1, b1);
      pimage += 4;

      a0 += a1 + 2;
      a0 /= 2;
      if (a0 == 0) {
        dest += 4;
        continue;
      }

      COMP_Y (y0, r0, g0, b0);
      COMP_Y (y1, r1, g1, b1);

      b0 += b1 + 2;
      g0 += g1 + 2;
      r0 += r1 + 2;

      b0 /= 2;
      g0 /= 2;
      r0 /= 2;

      COMP_U (u, r0, g0, b0);
      COMP_V (v, r0, g0, b0);

      BLEND (*dest, a0, u, *dest);
      dest++;
      BLEND (*dest, a0, y0, *dest);
      dest++;
      BLEND (*dest, a0, v, *dest);
      dest++;
      BLEND (*dest, a0, y1, *dest);
      dest++;
    }
  }
}

static inline void
gst_kate_tiger_blit_AYUV (GstKateTiger * tiger,
    guint8 * rgb_pixels, gint xpos, gint ypos, const guint8 * image,
    gint image_width, gint image_height)
{
  int a, r, g, b, a1;
  int y, u, v;
  int i, j;
  int h, w;
  const guint8 *pimage;
  guchar *dest;

  w = image_width;
  h = image_height;

  if (xpos < 0) {
    xpos = 0;
  }

  if (xpos + w > tiger->video_width) {
    w = tiger->video_width - xpos;
  }

  if (ypos + h > tiger->video_height) {
    h = tiger->video_height - ypos;
  }

  for (i = 0; i < h; i++) {
    pimage = image + i * image_width * 4;
    dest = rgb_pixels + (i + ypos) * 4 * tiger->video_width + xpos * 4;
    for (j = 0; j < w; j++) {
      a = pimage[TIGER_ARGB_A];
      b = pimage[TIGER_ARGB_B];
      g = pimage[TIGER_ARGB_G];
      r = pimage[TIGER_ARGB_R];

      TIGER_UNPREMULTIPLY (a, r, g, b);

      // convert background to yuv
      COMP_Y (y, r, g, b);
      COMP_U (u, r, g, b);
      COMP_V (v, r, g, b);

      // preform text "OVER" background alpha compositing
      a1 = a + (dest[0] * (255 - a)) / 255 + 1; // add 1 to prevent divide by 0
      OVER (dest[1], a, y, dest[0], dest[1], a1);
      OVER (dest[2], a, u, dest[0], dest[2], a1);
      OVER (dest[3], a, v, dest[0], dest[3], a1);
      dest[0] = a1 - 1;         // remove the temporary 1 we added

      pimage += 4;
      dest += 4;
    }
  }
}

static void
gst_kate_tiger_blend_yuv (GstKateTiger * tiger, GstBuffer * video_frame,
    const guint8 * image, gint image_width, gint image_height)
{
  gint xpos = 0, ypos = 0;

  switch (tiger->video_format) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
      gst_kate_tiger_blit_I420_YV12 (tiger,
          GST_BUFFER_DATA (video_frame), xpos, ypos, image, image_width,
          image_height);
      break;
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
      gst_kate_tiger_blit_NV12_NV21 (tiger,
          GST_BUFFER_DATA (video_frame), xpos, ypos, image, image_width,
          image_height);
      break;
    case GST_VIDEO_FORMAT_UYVY:
      gst_kate_tiger_blit_UYVY (tiger,
          GST_BUFFER_DATA (video_frame), xpos, ypos, image, image_width,
          image_height);
      break;
    case GST_VIDEO_FORMAT_AYUV:
      gst_kate_tiger_blit_AYUV (tiger,
          GST_BUFFER_DATA (video_frame), xpos, ypos, image, image_width,
          image_height);
      break;
    default:
      g_assert_not_reached ();
  }
}

static GstFlowReturn
gst_kate_tiger_video_chain (GstPad * pad, GstBuffer * buf)
{
  GstKateTiger *tiger = GST_KATE_TIGER (gst_pad_get_parent (pad));
  GstFlowReturn rflow = GST_FLOW_OK;
  unsigned char *ptr;
  int ret;
  kate_float t;

  GST_KATE_TIGER_MUTEX_LOCK (tiger);

  GST_LOG_OBJECT (tiger, "got video frame, %u bytes", GST_BUFFER_SIZE (buf));

  if (G_UNLIKELY (tiger->video_flushing)) {
    GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
    gst_object_unref (tiger);
    gst_buffer_unref (buf);
    return GST_FLOW_WRONG_STATE;
  }

  if (G_LIKELY (GST_BUFFER_TIMESTAMP_IS_VALID (buf))) {
    gst_segment_set_last_stop (&tiger->video_segment, GST_FORMAT_TIME,
        GST_BUFFER_TIMESTAMP (buf));
    g_cond_broadcast (tiger->cond);
  }

  /* Update first with a dummy buffer pointer we cannot write to, but with the
     right dimensions. If there is nothing to draw, we will not have to make
     it writeable. */
  ptr = GST_BUFFER_DATA (buf);
  ret =
      tiger_renderer_set_buffer (tiger->tr, ptr, tiger->video_width,
      tiger->video_height, tiger->video_width * 4, tiger->swap_rgb);
  if (G_UNLIKELY (ret < 0)) {
    GST_WARNING_OBJECT (tiger,
        "Tiger renderer failed to set buffer to video frame: %s",
        gst_kate_util_get_error_message (ret));
    goto pass;
  }

  /* update the renderer at the time of the video frame */
  t = gst_kate_tiger_get_time (tiger);
  GST_LOG_OBJECT (tiger, "Video segment calc: last stop %ld, time %.3f",
      (long) tiger->video_segment.last_stop, t);
  ret = tiger_renderer_update (tiger->tr, t, 1);
  if (G_UNLIKELY (ret < 0)) {
    GST_WARNING_OBJECT (tiger, "Tiger renderer failed to update: %s",
        gst_kate_util_get_error_message (ret));
    goto pass;
  }

  /* if there nothing to draw, we can just push the video buffer as is */
  if (ret > 0 || tiger->silent)
    goto pass;

  /* there is something to draw, so first make the buffer writable */
  buf = gst_buffer_make_writable (buf);
  if (G_UNLIKELY (!buf)) {
    GST_WARNING_OBJECT (tiger, "Failed to make video buffer writable");
    goto pass;
  }

  /* and setup that buffer before rendering */
  if (gst_video_format_is_yuv (tiger->video_format)) {
    guint8 *tmp = g_realloc (tiger->render_buffer,
        tiger->video_width * tiger->video_height * 4);
    if (!tmp) {
      GST_WARNING_OBJECT (tiger, "Failed to allocate render buffer");
      goto pass;
    }
    tiger->render_buffer = tmp;
    ptr = tiger->render_buffer;
    tiger_renderer_set_surface_clear_color (tiger->tr, 1, 0.0, 0.0, 0.0, 0.0);
  } else {
    ptr = GST_BUFFER_DATA (buf);
  }
  ret =
      tiger_renderer_set_buffer (tiger->tr, ptr, tiger->video_width,
      tiger->video_height, tiger->video_width * 4, tiger->swap_rgb);
  if (G_UNLIKELY (ret < 0)) {
    GST_WARNING_OBJECT (tiger,
        "Tiger renderer failed to set buffer to video frame: %s",
        gst_kate_util_get_error_message (ret));
    goto pass;
  }
  ret = tiger_renderer_render (tiger->tr);
  if (G_UNLIKELY (ret < 0)) {
    GST_WARNING_OBJECT (tiger,
        "Tiger renderer failed to render to video frame: %s",
        gst_kate_util_get_error_message (ret));
  } else {
    GST_LOG_OBJECT (tiger, "Tiger renderer rendered on video frame at %f", t);
  }

  if (gst_video_format_is_yuv (tiger->video_format)) {
    gst_kate_tiger_blend_yuv (tiger, buf, tiger->render_buffer,
        tiger->video_width, tiger->video_height);
  }

pass:
  GST_KATE_TIGER_MUTEX_UNLOCK (tiger);

  rflow = gst_pad_push (tiger->srcpad, buf);

  gst_object_unref (tiger);

  return rflow;
}

static GstStateChangeReturn
gst_kate_tiger_change_state (GstElement * element, GstStateChange transition)
{
  GstKateTiger *tiger = GST_KATE_TIGER (element);
  GstStateChangeReturn res;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_DEBUG_OBJECT (tiger, "PAUSED -> READY, clearing kate state");
      GST_KATE_TIGER_MUTEX_LOCK (tiger);
      gst_kate_util_decoder_base_set_flushing (&tiger->decoder, TRUE);
      g_cond_broadcast (tiger->cond);
      if (tiger->tr) {
        tiger_renderer_destroy (tiger->tr);
        tiger->tr = NULL;
      }
      gst_segment_init (&tiger->video_segment, GST_FORMAT_UNDEFINED);
      tiger->video_flushing = TRUE;
      GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
      break;
    default:
      break;
  }

  res =
      gst_kate_decoder_base_change_state (&tiger->decoder, element,
      parent_class, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG_OBJECT (tiger, "READY -> PAUSED, initializing kate state");
      GST_KATE_TIGER_MUTEX_LOCK (tiger);
      if (tiger->decoder.initialized) {
        int ret = tiger_renderer_create (&tiger->tr);
        if (ret < 0) {
          GST_WARNING_OBJECT (tiger, "failed to create tiger renderer: %s",
              gst_kate_util_get_error_message (ret));
        } else {
          ret =
              tiger_renderer_set_default_font_description (tiger->tr,
              tiger->default_font_desc);
          if (ret < 0) {
            GST_WARNING_OBJECT (tiger,
                "failed to set tiger default font description: %s",
                gst_kate_util_get_error_message (ret));
          }
          gst_kate_tiger_update_default_font_color (tiger);
          gst_kate_tiger_update_default_background_color (tiger);
          gst_kate_tiger_update_default_font_effect (tiger);
          gst_kate_tiger_update_quality (tiger);
        }
      }
      gst_segment_init (&tiger->video_segment, GST_FORMAT_UNDEFINED);
      tiger->video_flushing = FALSE;
      tiger->seen_header = FALSE;
      GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
      break;
    default:
      break;
  }

  return res;
}

static gboolean
gst_kate_tiger_seek (GstKateTiger * tiger, GstPad * pad, GstEvent * event)
{
  GstFormat format;
  gdouble rate;
  GstSeekFlags flags;
  GstSeekType cur_type, stop_type;
  gint64 cur, stop;

  gst_event_parse_seek (event, &rate, &format, &flags, &cur_type, &cur,
      &stop_type, &stop);

  if (flags & GST_SEEK_FLAG_FLUSH)
    gst_pad_push_event (tiger->srcpad, gst_event_new_flush_start ());

  GST_KATE_TIGER_MUTEX_LOCK (tiger);
  tiger->video_flushing = TRUE;
  gst_kate_util_decoder_base_set_flushing (&tiger->decoder, TRUE);
  g_cond_broadcast (tiger->cond);
  GST_KATE_TIGER_MUTEX_UNLOCK (tiger);

  if (format == GST_FORMAT_TIME) {
    /* if seeking in time, we can update tiger to remove any appropriate events */
    kate_float target;
    switch (cur_type) {
      case GST_SEEK_TYPE_SET:
        target = cur / (float) GST_SECOND;
        break;
      case GST_SEEK_TYPE_CUR:
        GST_WARNING_OBJECT (tiger,
            "Seeking from the current segment, cannot work out target so flushing everything");
        target = (kate_float) 0;
        break;
      case GST_SEEK_TYPE_END:
        GST_WARNING_OBJECT (tiger,
            "Seeking from the end, cannot work out target so flushing everything");
        target = (kate_float) 0;
        break;
      default:
        GST_WARNING_OBJECT (tiger, "Unexpected seek type");
        target = (kate_float) 0;
        break;
    }
    GST_INFO_OBJECT (tiger, "Seeking in time to %f", target);
    GST_KATE_TIGER_MUTEX_LOCK (tiger);
    tiger_renderer_seek (tiger->tr, target);
    GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
  }

  /* forward to both sinks */
  gst_event_ref (event);
  if (gst_pad_push_event (tiger->videosinkpad, event)) {
    int ret = gst_pad_push_event (tiger->katesinkpad, event);
    if (ret) {
      return TRUE;
    } else {
      return FALSE;
    }
  } else {
    gst_event_unref (event);
    return FALSE;
  }
}

static gboolean
gst_kate_tiger_source_event (GstPad * pad, GstEvent * event)
{
  GstKateTiger *tiger =
      (GstKateTiger *) (gst_object_get_parent (GST_OBJECT (pad)));
  gboolean res = TRUE;

  g_return_val_if_fail (tiger != NULL, FALSE);

  GST_LOG_OBJECT (tiger, "Event on source pad: %s",
      GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      GST_INFO_OBJECT (tiger, "Seek on source pad");
      res = gst_kate_tiger_seek (tiger, pad, event);
      break;
    default:
      res = gst_pad_event_default (pad, event);
      break;
  }

  gst_object_unref (tiger);

  return res;
}

static gboolean
gst_kate_tiger_handle_kate_event (GstPad * pad, GstEvent * event)
{
  GstKateTiger *tiger =
      (GstKateTiger *) (gst_object_get_parent (GST_OBJECT (pad)));
  gboolean res = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
      GST_INFO_OBJECT (tiger, "New segment on Kate pad");
      GST_KATE_TIGER_MUTEX_LOCK (tiger);
      g_cond_broadcast (tiger->cond);
      gst_kate_util_decoder_base_new_segment_event (&tiger->decoder, event);
      GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
      gst_event_unref (event);
      break;
    case GST_EVENT_FLUSH_START:
      GST_KATE_TIGER_MUTEX_LOCK (tiger);
      gst_kate_util_decoder_base_set_flushing (&tiger->decoder, TRUE);
      GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
      g_cond_broadcast (tiger->cond);
      gst_event_unref (event);
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_KATE_TIGER_MUTEX_LOCK (tiger);
      gst_kate_util_decoder_base_set_flushing (&tiger->decoder, FALSE);
      GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
      gst_event_unref (event);
      break;
    case GST_EVENT_EOS:
      /* we ignore this, it just means we don't have anymore Kate packets, but
         the Tiger renderer will still draw (if appropriate) on incoming video */
      GST_INFO_OBJECT (tiger, "EOS on Kate pad");
      GST_KATE_TIGER_MUTEX_LOCK (tiger);
      g_cond_broadcast (tiger->cond);
      GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
      gst_event_unref (event);
      break;
    default:
      res = gst_pad_event_default (pad, event);
      break;
  }

  gst_object_unref (tiger);

  return res;
}

static gboolean
gst_kate_tiger_kate_event (GstPad * pad, GstEvent * event)
{
  GstKateTiger *tiger =
      (GstKateTiger *) (gst_object_get_parent (GST_OBJECT (pad)));
  gboolean res = TRUE;

  g_return_val_if_fail (tiger != NULL, FALSE);

  GST_LOG_OBJECT (tiger, "Event on Kate pad: %s", GST_EVENT_TYPE_NAME (event));

  /* Delay events till we've set caps */
  if (gst_kate_util_decoder_base_queue_event (&tiger->decoder, event,
          &gst_kate_tiger_handle_kate_event, pad)) {
    gst_object_unref (tiger);
    return TRUE;
  }

  res = gst_kate_tiger_handle_kate_event (pad, event);

  gst_object_unref (tiger);

  return res;
}

static gboolean
gst_kate_tiger_handle_video_event (GstPad * pad, GstEvent * event)
{
  GstKateTiger *tiger =
      (GstKateTiger *) (gst_object_get_parent (GST_OBJECT (pad)));
  gboolean res = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
    {
      gboolean update;
      gdouble rate, arate;
      GstFormat format;
      gint64 start, stop, time;

      gst_event_parse_new_segment_full (event, &update, &rate, &arate, &format,
          &start, &stop, &time);

      if (format == GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (tiger, "video pad segment:"
            " Update %d, rate %g arate %g format %d start %" GST_TIME_FORMAT
            " %" GST_TIME_FORMAT " position %" GST_TIME_FORMAT,
            update, rate, arate, format, GST_TIME_ARGS (start),
            GST_TIME_ARGS (stop), GST_TIME_ARGS (time));

        GST_KATE_TIGER_MUTEX_LOCK (tiger);
        gst_segment_set_newsegment_full (&tiger->video_segment, update, rate,
            arate, format, start, stop, time);
        GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
      }

      res = gst_pad_event_default (pad, event);
      break;
    }
    case GST_EVENT_FLUSH_START:
      GST_KATE_TIGER_MUTEX_LOCK (tiger);
      gst_segment_init (&tiger->video_segment, GST_FORMAT_UNDEFINED);
      tiger->video_flushing = TRUE;
      GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
      g_cond_broadcast (tiger->cond);
      res = gst_pad_event_default (pad, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_KATE_TIGER_MUTEX_LOCK (tiger);
      gst_segment_init (&tiger->video_segment, GST_FORMAT_UNDEFINED);
      tiger->video_flushing = FALSE;
      GST_KATE_TIGER_MUTEX_UNLOCK (tiger);
      res = gst_pad_event_default (pad, event);
      break;
    default:
      res = gst_pad_event_default (pad, event);
      break;
  }

  gst_object_unref (tiger);

  return res;
}

static gboolean
gst_kate_tiger_video_event (GstPad * pad, GstEvent * event)
{
  GstKateTiger *tiger =
      (GstKateTiger *) (gst_object_get_parent (GST_OBJECT (pad)));
  gboolean res = TRUE;

  g_return_val_if_fail (tiger != NULL, FALSE);

  GST_INFO_OBJECT (tiger, "Event on video pad: %s",
      GST_EVENT_TYPE_NAME (event));

  res = gst_kate_tiger_handle_video_event (pad, event);

  gst_object_unref (tiger);

  return res;
}

gboolean
gst_kate_tiger_kate_sink_query (GstPad * pad, GstQuery * query)
{
  GstKateTiger *tiger = GST_KATE_TIGER (gst_pad_get_parent (pad));
  gboolean res = gst_kate_decoder_base_sink_query (&tiger->decoder,
      GST_ELEMENT_CAST (tiger), pad, query);
  GST_INFO_OBJECT (tiger, "Query on Kate pad");
  gst_object_unref (tiger);
  return res;
}
