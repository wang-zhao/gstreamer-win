/*
 * GStreamer
 * Copyright (C) 2011 Stefan Sauer <ensonic@users.sf.net>
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
/*
 * Freeverb
 *
 * Written by Jezar at Dreampoint, June 2000
 * http://www.dreampoint.co.uk
 * This code is public domain
 *
 * Translated to C by Peter Hanappe, Mai 2001
 * Transformed into a GStreamer plugin by Stefan Sauer, Nov 2011
 */

/**
 * SECTION:element-freeverb
 *
 * Reverberation/room effect.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch audiotestsrc wave=saw ! freeverb ! autoaudiosink
 * gst-launch filesrc location="melo1.ogg" ! decodebin ! audioconvert ! freeverb ! autoaudiosink
 * ]|
 * </refsect2>
 */

/* FIXME:
 * - add mono-to-mono, then we might also need stereo-to-mono ?
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdlib.h>

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/controller/gstcontroller.h>

#include "gstfreeverb.h"

#define GST_CAT_DEFAULT gst_freeverb_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_ROOM_SIZE,
  PROP_DAMPING,
  PROP_PAN_WIDTH,
  PROP_LEVEL
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-float, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [ 1, 2 ], "
        "endianness = (int) BYTE_ORDER, " "width = (int) 32; "
        "audio/x-raw-int, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [ 1, 2 ], "
        "endianness = (int) BYTE_ORDER, "
        "width = (int) 16, " "depth = (int) 16, " "signed = (boolean) true")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-float, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) 2, "
        "endianness = (int) BYTE_ORDER, " "width = (int) 32; "
        "audio/x-raw-int, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) 2, "
        "endianness = (int) BYTE_ORDER, "
        "width = (int) 16, " "depth = (int) 16, " "signed = (boolean) true")
    );

#define _do_init(type) {                                                       \
  const GInterfaceInfo preset_interface_info = { NULL, NULL, NULL };           \
  g_type_add_interface_static (type, GST_TYPE_PRESET, &preset_interface_info); \
                                                                               \
  GST_DEBUG_CATEGORY_INIT (gst_freeverb_debug, "freeverb", 0,                  \
      "freeverb element");                                                     \
}

GST_BOILERPLATE_FULL (GstFreeverb, gst_freeverb, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM, _do_init);

static void gst_freeverb_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_freeverb_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_freeverb_finalize (GObject * object);

static gboolean gst_freeverb_get_unit_size (GstBaseTransform * base,
    GstCaps * caps, guint * size);
static GstCaps *gst_freeverb_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps);
static gboolean gst_freeverb_set_caps (GstBaseTransform * base,
    GstCaps * incaps, GstCaps * outcaps);

static GstFlowReturn gst_freeverb_transform (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer * outbuf);

static gboolean gst_freeverb_transform_m2s_int (GstFreeverb * filter,
    gint16 * idata, gint16 * odata, guint num_samples);
static gboolean gst_freeverb_transform_s2s_int (GstFreeverb * filter,
    gint16 * idata, gint16 * odata, guint num_samples);
static gboolean gst_freeverb_transform_m2s_float (GstFreeverb * filter,
    gfloat * idata, gfloat * odata, guint num_samples);
static gboolean gst_freeverb_transform_s2s_float (GstFreeverb * filter,
    gfloat * idata, gfloat * odata, guint num_samples);


/* Table with processing functions: [channels][format] */
static GstFreeverbProcessFunc process_functions[2][2] = {
  {
        (GstFreeverbProcessFunc) gst_freeverb_transform_m2s_int,
        (GstFreeverbProcessFunc) gst_freeverb_transform_m2s_float,
      },
  {
        (GstFreeverbProcessFunc) gst_freeverb_transform_s2s_int,
        (GstFreeverbProcessFunc) gst_freeverb_transform_s2s_float,
      }
};

/***************************************************************
 *
 *                           REVERB
 */

/* Denormalising:
 *
 * Another method fixes the problem cheaper: Use a small DC-offset in
 * the filter calculations.  Now the signals converge not against 0,
 * but against the offset.  The constant offset is invisible from the
 * outside world (i.e. it does not appear at the output.  There is a
 * very small turn-on transient response, which should not cause
 * problems.
 */

//#define DC_OFFSET 0
#define DC_OFFSET 1e-8
//#define DC_OFFSET 0.001f

/* all pass filter */

typedef struct _freeverb_allpass
{
  gfloat feedback;
  gfloat *buffer;
  gint bufsize;
  gint bufidx;
} freeverb_allpass;

static void
freeverb_allpass_setbuffer (freeverb_allpass * allpass, gint size)
{
  allpass->bufidx = 0;
  allpass->buffer = g_new (gfloat, size);
  allpass->bufsize = size;
}

static void
freeverb_allpass_release (freeverb_allpass * allpass)
{
  g_free (allpass->buffer);
}

static void
freeverb_allpass_init (freeverb_allpass * allpass)
{
  gint i, len = allpass->bufsize;
  gfloat *buf = allpass->buffer;

  for (i = 0; i < len; i++) {
    buf[i] = DC_OFFSET;         /* this is not 100 % correct. */
  }
}

static void
freeverb_allpass_setfeedback (freeverb_allpass * allpass, gfloat val)
{
  allpass->feedback = val;
}

/*
static gfloat
freeverb_allpass_getfeedback(freeverb_allpass* allpass)
{
  return allpass->feedback;
}*/

#define freeverb_allpass_process(_allpass, _input_1) \
{ \
  gfloat output; \
  gfloat bufout; \
  bufout = _allpass.buffer[_allpass.bufidx]; \
  output = bufout-_input_1; \
  _allpass.buffer[_allpass.bufidx] = _input_1 + (bufout * _allpass.feedback); \
  if (++_allpass.bufidx >= _allpass.bufsize) { \
    _allpass.bufidx = 0; \
  } \
  _input_1 = output; \
}

/* comb filter */

typedef struct _freeverb_comb
{
  gfloat feedback;
  gfloat filterstore;
  gfloat damp1;
  gfloat damp2;
  gfloat *buffer;
  gint bufsize;
  gint bufidx;
} freeverb_comb;

static void
freeverb_comb_setbuffer (freeverb_comb * comb, gint size)
{
  comb->filterstore = 0;
  comb->bufidx = 0;
  comb->buffer = g_new (gfloat, size);
  comb->bufsize = size;
}

static void
freeverb_comb_release (freeverb_comb * comb)
{
  g_free (comb->buffer);
}

static void
freeverb_comb_init (freeverb_comb * comb)
{
  gint i, len = comb->bufsize;
  gfloat *buf = comb->buffer;

  for (i = 0; i < len; i++) {
    buf[i] = DC_OFFSET;         /* This is not 100 % correct. */
  }
}

static void
freeverb_comb_setdamp (freeverb_comb * comb, gfloat val)
{
  comb->damp1 = val;
  comb->damp2 = 1 - val;
}

/*
static gfloat
freeverb_comb_getdamp(freeverb_comb* comb)
{
  return comb->damp1;
}*/

static void
freeverb_comb_setfeedback (freeverb_comb * comb, gfloat val)
{
  comb->feedback = val;
}

/*
static gfloat
freeverb_comb_getfeedback(freeverb_comb* comb)
{
  return comb->feedback;
}*/

#define freeverb_comb_process(_comb, _input_1, _output) \
{ \
  gfloat _tmp = _comb.buffer[_comb.bufidx]; \
  _comb.filterstore = (_tmp * _comb.damp2) + (_comb.filterstore * _comb.damp1); \
  _comb.buffer[_comb.bufidx] = _input_1 + (_comb.filterstore * _comb.feedback); \
  if (++_comb.bufidx >= _comb.bufsize) { \
    _comb.bufidx = 0; \
  } \
  _output += _tmp; \
}

#define numcombs 8
#define numallpasses 4
#define	fixedgain 0.015f
#define scalewet 1.0f
#define scaledry 1.0f
#define scaledamp 1.0f
#define scaleroom 0.28f
#define offsetroom 0.7f
#define stereospread 23

/* These values assume 44.1KHz sample rate
 * they will need scaling for 96KHz (or other) sample rates.
 * The values were obtained by listening tests.
 */
#define combtuningL1 1116
#define combtuningR1 (1116 + stereospread)
#define combtuningL2 1188
#define combtuningR2 (1188 + stereospread)
#define combtuningL3 1277
#define combtuningR3 (1277 + stereospread)
#define combtuningL4 1356
#define combtuningR4 (1356 + stereospread)
#define combtuningL5 1422
#define combtuningR5 (1422 + stereospread)
#define combtuningL6 1491
#define combtuningR6 (1491 + stereospread)
#define combtuningL7 1557
#define combtuningR7 (1557 + stereospread)
#define combtuningL8 1617
#define combtuningR8 (1617 + stereospread)
#define allpasstuningL1 556
#define allpasstuningR1 (556 + stereospread)
#define allpasstuningL2 441
#define allpasstuningR2 (441 + stereospread)
#define allpasstuningL3 341
#define allpasstuningR3 (341 + stereospread)
#define allpasstuningL4 225
#define allpasstuningR4 (225 + stereospread)

struct _GstFreeverbPrivate
{
  gfloat roomsize;
  gfloat damp;
  gfloat wet, wet1, wet2, dry;
  gfloat width;
  gfloat gain;
  /*
     The following are all declared inline
     to remove the need for dynamic allocation
     with its subsequent error-checking messiness
   */
  /* Comb filters */
  freeverb_comb combL[numcombs];
  freeverb_comb combR[numcombs];
  /* Allpass filters */
  freeverb_allpass allpassL[numallpasses];
  freeverb_allpass allpassR[numallpasses];
};

static void
freeverb_revmodel_init (GstFreeverb * filter)
{
  GstFreeverbPrivate *priv = filter->priv;
  gint i;

  for (i = 0; i < numcombs; i++) {
    freeverb_comb_init (&priv->combL[i]);
    freeverb_comb_init (&priv->combR[i]);
  }
  for (i = 0; i < numallpasses; i++) {
    freeverb_allpass_init (&priv->allpassL[i]);
    freeverb_allpass_init (&priv->allpassR[i]);
  }
}

static void
freeverb_revmodel_free (GstFreeverb * filter)
{
  GstFreeverbPrivate *priv = filter->priv;
  gint i;

  for (i = 0; i < numcombs; i++) {
    freeverb_comb_release (&priv->combL[i]);
    freeverb_comb_release (&priv->combR[i]);
  }
  for (i = 0; i < numallpasses; i++) {
    freeverb_allpass_release (&priv->allpassL[i]);
    freeverb_allpass_release (&priv->allpassR[i]);
  }
}

/* GObject vmethod implementations */

static void
gst_freeverb_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
  gst_element_class_set_details_simple (element_class, "Stereo positioning",
      "Filter/Effect/Audio",
      "Reverberation/room effect", "Stefan Sauer <ensonic@users.sf.net>");
}

static void
gst_freeverb_class_init (GstFreeverbClass * klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (GstFreeverbPrivate));

  gobject_class = (GObjectClass *) klass;
  gobject_class->set_property = gst_freeverb_set_property;
  gobject_class->get_property = gst_freeverb_get_property;
  gobject_class->finalize = gst_freeverb_finalize;

  g_object_class_install_property (gobject_class, PROP_ROOM_SIZE,
      g_param_spec_float ("room-size", "Room size",
          "Size of the simulated room", 0.0, 1.0, 0.5,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DAMPING,
      g_param_spec_float ("damping", "Damping", "Damping of high frequencies",
          0.0, 1.0, 0.2,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAN_WIDTH,
      g_param_spec_float ("width", "Width", "Stereo panorama width", 0.0, 1.0,
          1.0,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_LEVEL,
      g_param_spec_float ("level", "Level", "dry/wet level", 0.0, 1.0, 0.5,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
          G_PARAM_STATIC_STRINGS));

  GST_BASE_TRANSFORM_CLASS (klass)->get_unit_size =
      GST_DEBUG_FUNCPTR (gst_freeverb_get_unit_size);
  GST_BASE_TRANSFORM_CLASS (klass)->transform_caps =
      GST_DEBUG_FUNCPTR (gst_freeverb_transform_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->set_caps =
      GST_DEBUG_FUNCPTR (gst_freeverb_set_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->transform =
      GST_DEBUG_FUNCPTR (gst_freeverb_transform);
}

static void
gst_freeverb_init (GstFreeverb * filter, GstFreeverbClass * klass)
{
  filter->priv =
      G_TYPE_INSTANCE_GET_PRIVATE (filter, GST_TYPE_FREEVERB,
      GstFreeverbPrivate);

  filter->width = 0;
  filter->channels = 0;
  filter->format_float = FALSE;
  filter->process = NULL;

  gst_base_transform_set_gap_aware (GST_BASE_TRANSFORM (filter), TRUE);

  freeverb_revmodel_init (filter);
}

static void
gst_freeverb_finalize (GObject * object)
{
  GstFreeverb *filter = GST_FREEVERB (object);

  freeverb_revmodel_free (filter);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_freeverb_set_process_function (GstFreeverb * filter)
{
  gint channel_index, format_index;

  /* set processing function */
  channel_index = filter->channels - 1;
  if (channel_index > 1 || channel_index < 0) {
    filter->process = NULL;
    return FALSE;
  }

  format_index = (filter->format_float) ? 1 : 0;

  filter->process = process_functions[channel_index][format_index];

  g_assert (filter->process);
  return TRUE;
}

static void
gst_freeverb_init_rev_model (GstFreeverb * filter)
{
  gfloat srfactor = filter->rate / 44100.0f;
  GstFreeverbPrivate *priv = filter->priv;

  freeverb_revmodel_free (filter);

  priv->gain = fixedgain;

  freeverb_comb_setbuffer (&priv->combL[0], combtuningL1 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[0], combtuningR1 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[1], combtuningL2 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[1], combtuningR2 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[2], combtuningL3 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[2], combtuningR3 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[3], combtuningL4 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[3], combtuningR4 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[4], combtuningL5 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[4], combtuningR5 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[5], combtuningL6 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[5], combtuningR6 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[6], combtuningL7 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[6], combtuningR7 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[7], combtuningL8 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[7], combtuningR8 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassL[0], allpasstuningL1 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassR[0], allpasstuningR1 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassL[1], allpasstuningL2 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassR[1], allpasstuningR2 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassL[2], allpasstuningL3 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassR[2], allpasstuningR3 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassL[3], allpasstuningL4 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassR[3], allpasstuningR4 * srfactor);

  /* clear buffers */
  freeverb_revmodel_init (filter);

  /* set default values */
  freeverb_allpass_setfeedback (&priv->allpassL[0], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassR[0], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassL[1], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassR[1], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassL[2], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassR[2], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassL[3], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassR[3], 0.5f);
}

static void
gst_freeverb_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFreeverb *filter = GST_FREEVERB (object);
  GstFreeverbPrivate *priv = filter->priv;
  gint i;

  switch (prop_id) {
    case PROP_ROOM_SIZE:
      filter->room_size = g_value_get_float (value);
      priv->roomsize = (filter->room_size * scaleroom) + offsetroom;
      for (i = 0; i < numcombs; i++) {
        freeverb_comb_setfeedback (&priv->combL[i], priv->roomsize);
        freeverb_comb_setfeedback (&priv->combR[i], priv->roomsize);
      }
      break;
    case PROP_DAMPING:
      filter->damping = g_value_get_float (value);
      priv->damp = filter->damping * scaledamp;
      for (i = 0; i < numcombs; i++) {
        freeverb_comb_setdamp (&priv->combL[i], priv->damp);
        freeverb_comb_setdamp (&priv->combR[i], priv->damp);
      }
      break;
    case PROP_PAN_WIDTH:
      filter->pan_width = g_value_get_float (value);
      priv->width = filter->pan_width;
      priv->wet1 = priv->wet * (priv->width / 2.0f + 0.5f);
      priv->wet2 = priv->wet * ((1.0f - priv->width) / 2.0f);
      break;
    case PROP_LEVEL:
      filter->level = g_value_get_float (value);
      priv->wet = filter->level * scalewet;
      priv->dry = (1.0 - filter->level) * scaledry;
      priv->wet1 = priv->wet * (priv->width / 2.0f + 0.5f);
      priv->wet2 = priv->wet * ((1.0f - priv->width) / 2.0f);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_freeverb_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstFreeverb *filter = GST_FREEVERB (object);

  switch (prop_id) {
    case PROP_ROOM_SIZE:
      g_value_set_float (value, filter->room_size);
      break;
    case PROP_DAMPING:
      g_value_set_float (value, filter->damping);
      break;
    case PROP_PAN_WIDTH:
      g_value_set_float (value, filter->pan_width);
      break;
    case PROP_LEVEL:
      g_value_set_float (value, filter->level);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

static gboolean
gst_freeverb_get_unit_size (GstBaseTransform * base, GstCaps * caps,
    guint * size)
{
  gint width, channels;
  GstStructure *structure;
  gboolean ret;

  g_assert (size);

  /* this works for both float and int */
  structure = gst_caps_get_structure (caps, 0);
  ret = gst_structure_get_int (structure, "width", &width);
  ret &= gst_structure_get_int (structure, "channels", &channels);

  *size = width * channels / 8;

  GST_INFO_OBJECT (base, "unit size: %u", *size);

  return ret;
}

static GstCaps *
gst_freeverb_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps)
{
  GstCaps *res;
  GstStructure *structure;

  /* transform caps gives one single caps so we can just replace
   * the channel property with our range. */
  res = gst_caps_copy (caps);
  structure = gst_caps_get_structure (res, 0);
  if (direction == GST_PAD_SRC) {
    GST_INFO_OBJECT (base, "allow 1-2 channels");
    gst_structure_set (structure, "channels", GST_TYPE_INT_RANGE, 1, 2, NULL);
  } else {
    GST_INFO_OBJECT (base, "allow 2 channels");
    gst_structure_set (structure, "channels", G_TYPE_INT, 2, NULL);
  }

  return res;
}

static gboolean
gst_freeverb_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstFreeverb *filter = GST_FREEVERB (base);
  const GstStructure *structure;
  gboolean ret;
  gint width, rate;
  const gchar *fmt;

  /*GST_INFO ("incaps are %" GST_PTR_FORMAT, incaps); */

  structure = gst_caps_get_structure (incaps, 0);
  ret = gst_structure_get_int (structure, "channels", &filter->channels);
  if (!ret)
    goto no_channels;

  ret = gst_structure_get_int (structure, "width", &width);
  if (!ret)
    goto no_width;
  filter->width = width / 8;

  ret = gst_structure_get_int (structure, "rate", &rate);
  if (!ret)
    goto no_rate;
  filter->rate = rate;

  fmt = gst_structure_get_name (structure);
  if (!strcmp (fmt, "audio/x-raw-int"))
    filter->format_float = FALSE;
  else
    filter->format_float = TRUE;

  GST_DEBUG_OBJECT (filter, "try to process %s input_1 with %d channels", fmt,
      filter->channels);

  ret = gst_freeverb_set_process_function (filter);
  if (!ret)
    GST_WARNING_OBJECT (filter, "can't process input_1 with %d channels",
        filter->channels);

  gst_freeverb_init_rev_model (filter);
  filter->drained = FALSE;
  GST_INFO_OBJECT (base, "model configured");

  return ret;

no_channels:
  GST_DEBUG_OBJECT (filter, "no channels in caps");
  return ret;
no_width:
  GST_DEBUG_OBJECT (filter, "no width in caps");
  return ret;
no_rate:
  GST_DEBUG_OBJECT (filter, "no rate in caps");
  return ret;
}

static gboolean
gst_freeverb_transform_m2s_int (GstFreeverb * filter,
    gint16 * idata, gint16 * odata, guint num_samples)
{
  GstFreeverbPrivate *priv = filter->priv;
  gint i, k;
  gfloat out_l1, out_r1, input_1;
  gfloat out_l2, out_r2, input_2;
  gboolean drained = TRUE;

  for (k = 0; k < num_samples; k++) {

    out_l1 = out_r1 = 0.0;

    /* The original Freeverb code expects a stereo signal and 'input_1'
     * is set to the sum of the left and right input_1 sample. Since
     * this code works on a mono signal, 'input_1' is set to twice the
     * input_1 sample. */
    input_2 = (gfloat) * idata++;
    input_1 = (2.0f * input_2 + DC_OFFSET) * priv->gain;

    /* Accumulate comb filters in parallel */
    for (i = 0; i < numcombs; i++) {
      freeverb_comb_process (priv->combL[i], input_1, out_l1);
      freeverb_comb_process (priv->combR[i], input_1, out_r1);
    }
    /* Feed through allpasses in series */
    for (i = 0; i < numallpasses; i++) {
      freeverb_allpass_process (priv->allpassL[i], out_l1);
      freeverb_allpass_process (priv->allpassR[i], out_r1);
    }

    /* Remove the DC offset */
    out_l1 -= DC_OFFSET;
    out_r1 -= DC_OFFSET;

    /* Calculate output */
    out_l2 = out_l1 * priv->wet1 + out_r1 * priv->wet2 + input_2 * priv->dry;
    out_r2 = out_r1 * priv->wet1 + out_l1 * priv->wet2 + input_2 * priv->dry;
    *odata++ = (gint16) CLAMP (out_l2, G_MININT16, G_MAXINT16);
    *odata++ = (gint16) CLAMP (out_r2, G_MININT16, G_MAXINT16);

    if (abs (out_l2) > 0 || abs (out_r2) > 0)
      drained = FALSE;
  }
  return drained;
}

static gboolean
gst_freeverb_transform_s2s_int (GstFreeverb * filter,
    gint16 * idata, gint16 * odata, guint num_samples)
{
  GstFreeverbPrivate *priv = filter->priv;
  gint i, k;
  gfloat out_l1, out_r1, input_1l, input_1r;
  gfloat out_l2, out_r2, input_2l, input_2r;
  gboolean drained = TRUE;

  for (k = 0; k < num_samples; k++) {

    out_l1 = out_r1 = 0.0;

    input_2l = (gfloat) * idata++;
    input_2r = (gfloat) * idata++;
    input_1l = (input_2l + DC_OFFSET) * priv->gain;
    input_1r = (input_2r + DC_OFFSET) * priv->gain;

    /* Accumulate comb filters in parallel */
    for (i = 0; i < numcombs; i++) {
      freeverb_comb_process (priv->combL[i], input_1l, out_l1);
      freeverb_comb_process (priv->combR[i], input_1r, out_r1);
    }
    /* Feed through allpasses in series */
    for (i = 0; i < numallpasses; i++) {
      freeverb_allpass_process (priv->allpassL[i], out_l1);
      freeverb_allpass_process (priv->allpassR[i], out_r1);
    }

    /* Remove the DC offset */
    out_l1 -= DC_OFFSET;
    out_r1 -= DC_OFFSET;

    /* Calculate output */
    out_l2 = out_l1 * priv->wet1 + out_r1 * priv->wet2 + input_2l * priv->dry;
    out_r2 = out_r1 * priv->wet1 + out_l1 * priv->wet2 + input_2r * priv->dry;
    *odata++ = (gint16) CLAMP (out_l2, G_MININT16, G_MAXINT16);
    *odata++ = (gint16) CLAMP (out_r2, G_MININT16, G_MAXINT16);

    if (abs (out_l2) > 0 || abs (out_r2) > 0)
      drained = FALSE;
  }
  return drained;
}

static gboolean
gst_freeverb_transform_m2s_float (GstFreeverb * filter,
    gfloat * idata, gfloat * odata, guint num_samples)
{
  GstFreeverbPrivate *priv = filter->priv;
  gint i, k;
  gfloat out_l1, out_r1, input_1;
  gfloat out_l2, out_r2, input_2;
  gboolean drained = TRUE;

  for (k = 0; k < num_samples; k++) {

    out_l1 = out_r1 = 0.0;

    /* The original Freeverb code expects a stereo signal and 'input_1'
     * is set to the sum of the left and right input_1 sample. Since
     * this code works on a mono signal, 'input_1' is set to twice the
     * input_1 sample. */
    input_2 = *idata++;
    input_1 = (2.0f * input_2 + DC_OFFSET) * priv->gain;

    /* Accumulate comb filters in parallel */
    for (i = 0; i < numcombs; i++) {
      freeverb_comb_process (priv->combL[i], input_1, out_l1);
      freeverb_comb_process (priv->combR[i], input_1, out_r1);
    }
    /* Feed through allpasses in series */
    for (i = 0; i < numallpasses; i++) {
      freeverb_allpass_process (priv->allpassL[i], out_l1);
      freeverb_allpass_process (priv->allpassR[i], out_r1);
    }

    /* Remove the DC offset */
    out_l1 -= DC_OFFSET;
    out_r1 -= DC_OFFSET;

    /* Calculate output */
    out_l2 = out_l1 * priv->wet1 + out_r1 * priv->wet2 + input_2 * priv->dry;
    out_r2 = out_r1 * priv->wet1 + out_l1 * priv->wet2 + input_2 * priv->dry;
    *odata++ = out_l2;
    *odata++ = out_r2;

    if (fabs (out_l2) > 0 || fabs (out_r2) > 0)
      drained = FALSE;
  }
  return drained;
}

static gboolean
gst_freeverb_transform_s2s_float (GstFreeverb * filter,
    gfloat * idata, gfloat * odata, guint num_samples)
{
  GstFreeverbPrivate *priv = filter->priv;
  gint i, k;
  gfloat out_l1, out_r1, input_1l, input_1r;
  gfloat out_l2, out_r2, input_2l, input_2r;
  gboolean drained = TRUE;

  for (k = 0; k < num_samples; k++) {

    out_l1 = out_r1 = 0.0;

    input_2l = *idata++;
    input_2r = *idata++;
    input_1l = (input_2l + DC_OFFSET) * priv->gain;
    input_1r = (input_2r + DC_OFFSET) * priv->gain;

    /* Accumulate comb filters in parallel */
    for (i = 0; i < numcombs; i++) {
      freeverb_comb_process (priv->combL[i], input_1l, out_l1);
      freeverb_comb_process (priv->combR[i], input_1r, out_r1);
    }
    /* Feed through allpasses in series */
    for (i = 0; i < numallpasses; i++) {
      freeverb_allpass_process (priv->allpassL[i], out_l1);
      freeverb_allpass_process (priv->allpassR[i], out_r1);
    }

    /* Remove the DC offset */
    out_l1 -= DC_OFFSET;
    out_r1 -= DC_OFFSET;

    /* Calculate output */
    out_l2 = out_l1 * priv->wet1 + out_r1 * priv->wet2 + input_2l * priv->dry;
    out_r2 = out_r1 * priv->wet1 + out_l1 * priv->wet2 + input_2r * priv->dry;
    *odata++ = out_l2;
    *odata++ = out_r2;

    if (fabs (out_l2) > 0 || fabs (out_r2) > 0)
      drained = FALSE;
  }
  return drained;
}

/* this function does the actual processing
 */
static GstFlowReturn
gst_freeverb_transform (GstBaseTransform * base, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstFreeverb *filter = GST_FREEVERB (base);
  guint num_samples = GST_BUFFER_SIZE (outbuf) / (2 * filter->width);
  GstClockTime timestamp;

  timestamp = GST_BUFFER_TIMESTAMP (inbuf);
  timestamp =
      gst_segment_to_stream_time (&base->segment, GST_FORMAT_TIME, timestamp);

  GST_DEBUG_OBJECT (filter, "processing %u samples at %" GST_TIME_FORMAT,
      num_samples, GST_TIME_ARGS (timestamp));

  if (GST_CLOCK_TIME_IS_VALID (timestamp))
    gst_object_sync_values (G_OBJECT (filter), timestamp);

  if (G_UNLIKELY (GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_DISCONT))) {
    filter->drained = FALSE;
  }
  if (G_UNLIKELY (GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_GAP))) {
    if (filter->drained) {
      GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_GAP);
      memset (GST_BUFFER_DATA (outbuf), 0, GST_BUFFER_SIZE (outbuf));
      return GST_FLOW_OK;
    }
  } else {
    filter->drained = FALSE;
  }

  filter->drained = filter->process (filter, GST_BUFFER_DATA (inbuf),
      GST_BUFFER_DATA (outbuf), num_samples);

  if (filter->drained) {
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_GAP);
  }

  return GST_FLOW_OK;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  gst_controller_init (NULL, NULL);

  return gst_element_register (plugin, "freeverb",
      GST_RANK_NONE, GST_TYPE_FREEVERB);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "freeverb",
    "Reverberation/room effect",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
