/* GStreamer
 * Copyright (C) 2010 Oblong Industries, Inc.
 * Copyright (C) 2010 Collabora Multimedia
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
 * SECTION:element-gstjp2kdecimator
 *
 * The jp2kdecimator element removes information from JPEG2000 images without reencoding.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v videotestsrc num-buffers=1 ! jp2kenc ! \
 *   gstjp2kdecimator max-decomposition-levels=2 ! jp2kdec ! \
 *   ffmpegcolorspace ! autovideosink
 * ]|
 * This pipelines encodes a test image to JPEG2000, only keeps 3 decomposition levels
 * decodes the decimated image again and shows it on the screen.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "gstjp2kdecimator.h"

#include "jp2kcodestream.h"

#include <gst/base/gstbytereader.h>
#include <gst/base/gstbitreader.h>
#include <gst/base/gstbytewriter.h>

#include <string.h>

static GstStaticPadTemplate sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/x-jpc"));

static GstStaticPadTemplate src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/x-jpc"));

enum
{
  PROP_0,
  PROP_MAX_LAYERS,
  PROP_MAX_DECOMPOSITION_LEVELS
};

#define DEFAULT_MAX_LAYERS (0)
#define DEFAULT_MAX_DECOMPOSITION_LEVELS (-1)

static void gst_jp2k_decimator_finalize (GObject * obj);
static void gst_jp2k_decimator_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_jp2k_decimator_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_jp2k_decimator_sink_chain (GstPad * pad,
    GstBuffer * inbuf);
static gboolean gst_jp2k_decimator_sink_setcaps (GstPad * pad, GstCaps * caps);

static gboolean gst_jp2k_decimator_event (GstPad * pad, GstEvent * event);
static GstCaps *gst_jp2k_decimator_getcaps (GstPad * pad);
static gboolean gst_jp2k_decimator_query (GstPad * pad, GstQuery * query);
static const GstQueryType *gst_jp2k_decimator_query_type (GstPad * pad);

GST_DEBUG_CATEGORY (gst_jp2k_decimator_debug);
#define GST_CAT_DEFAULT gst_jp2k_decimator_debug

GST_BOILERPLATE (GstJP2kDecimator, gst_jp2k_decimator, GstElement,
    GST_TYPE_ELEMENT);

static void
gst_jp2k_decimator_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstelement_class,
      "JPEG2000 decimator",
      "Filter/Image",
      "Removes information from JPEG2000 streams without recompression",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &sink_pad_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &src_pad_template);
}

static void
gst_jp2k_decimator_class_init (GstJP2kDecimatorClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_jp2k_decimator_set_property;
  gobject_class->get_property = gst_jp2k_decimator_get_property;
  gobject_class->finalize = gst_jp2k_decimator_finalize;

  g_object_class_install_property (gobject_class, PROP_MAX_LAYERS,
      g_param_spec_int ("max-layers", "Maximum Number of Layers",
          "Maximum number of layers to keep (0 == all)", 0, 65535,
          DEFAULT_MAX_LAYERS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_DECOMPOSITION_LEVELS,
      g_param_spec_int ("max-decomposition-levels",
          "Maximum Number of Decomposition Levels",
          "Maximum number of decomposition levels to keep (-1 == all)", -1, 32,
          DEFAULT_MAX_DECOMPOSITION_LEVELS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_jp2k_decimator_init (GstJP2kDecimator * self,
    GstJP2kDecimatorClass * g_class)
{
  self->max_layers = DEFAULT_MAX_LAYERS;
  self->max_decomposition_levels = DEFAULT_MAX_DECOMPOSITION_LEVELS;

  self->sinkpad = gst_pad_new_from_static_template (&sink_pad_template, "sink");
  gst_pad_set_chain_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_jp2k_decimator_sink_chain));
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_jp2k_decimator_event));
  gst_pad_set_setcaps_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_jp2k_decimator_sink_setcaps));
  gst_pad_set_getcaps_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_jp2k_decimator_getcaps));
  gst_pad_set_query_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_jp2k_decimator_query));
  gst_pad_set_query_type_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_jp2k_decimator_query_type));
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  self->srcpad = gst_pad_new_from_static_template (&src_pad_template, "src");
  gst_pad_set_event_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_jp2k_decimator_event));
  gst_pad_set_getcaps_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_jp2k_decimator_getcaps));
  gst_pad_set_query_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_jp2k_decimator_query));
  gst_pad_set_query_type_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_jp2k_decimator_query_type));
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);
}

static void
gst_jp2k_decimator_finalize (GObject * obj)
{
  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_jp2k_decimator_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstJP2kDecimator *self = GST_JP2K_DECIMATOR (object);

  switch (prop_id) {
    case PROP_MAX_LAYERS:
      self->max_layers = g_value_get_int (value);
      break;
    case PROP_MAX_DECOMPOSITION_LEVELS:
      self->max_decomposition_levels = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_jp2k_decimator_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstJP2kDecimator *self = GST_JP2K_DECIMATOR (object);

  switch (prop_id) {
    case PROP_MAX_LAYERS:
      g_value_set_int (value, self->max_layers);
      break;
    case PROP_MAX_DECOMPOSITION_LEVELS:
      g_value_set_int (value, self->max_decomposition_levels);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_jp2k_decimator_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  GstJP2kDecimator *self = GST_JP2K_DECIMATOR (gst_pad_get_parent (pad));
  gboolean ret;

  GST_DEBUG_OBJECT (pad, "Setting caps: %" GST_PTR_FORMAT, caps);

  ret = gst_pad_set_caps (self->srcpad, caps);

  gst_object_unref (self);

  return ret;
}

static GstCaps *
gst_jp2k_decimator_getcaps (GstPad * pad)
{
  GstJP2kDecimator *self = GST_JP2K_DECIMATOR (gst_pad_get_parent (pad));
  GstPad *otherpad;
  GstCaps *tmp, *ret;

  otherpad = (pad == self->srcpad) ? self->sinkpad : self->srcpad;

  tmp = gst_pad_peer_get_caps_reffed (otherpad);
  if (tmp) {
    ret = gst_caps_intersect (tmp, gst_pad_get_pad_template_caps (pad));
    gst_caps_unref (tmp);
  } else {
    ret = gst_caps_copy (gst_pad_get_pad_template_caps (pad));
  }

  gst_object_unref (self);

  GST_LOG_OBJECT (pad, "Returning caps: %" GST_PTR_FORMAT, ret);

  return ret;
}

static gboolean
gst_jp2k_decimator_query (GstPad * pad, GstQuery * query)
{
  GstJP2kDecimator *self = GST_JP2K_DECIMATOR (gst_pad_get_parent (pad));
  gboolean ret;
  GstPad *otherpad;

  otherpad = (pad == self->srcpad) ? self->sinkpad : self->srcpad;

  GST_LOG_OBJECT (pad, "Handling query of type '%s'",
      gst_query_type_get_name (GST_QUERY_TYPE (query)));

  ret = gst_pad_peer_query (otherpad, query);

  gst_object_unref (self);
  return ret;
}

static const GstQueryType *
gst_jp2k_decimator_query_type (GstPad * pad)
{
  GstJP2kDecimator *self = GST_JP2K_DECIMATOR (gst_pad_get_parent (pad));
  GstPad *otherpad, *peer;
  const GstQueryType *types = NULL;

  otherpad = (pad == self->srcpad) ? self->sinkpad : self->srcpad;
  peer = gst_pad_get_peer (otherpad);
  if (peer) {
    types = gst_pad_get_query_types (peer);
    gst_object_unref (peer);
  }

  gst_object_unref (self);

  return types;
}

static gboolean
gst_jp2k_decimator_event (GstPad * pad, GstEvent * event)
{
  GstJP2kDecimator *self = GST_JP2K_DECIMATOR (gst_pad_get_parent (pad));
  GstPad *otherpad;
  gboolean ret;

  GST_LOG_OBJECT (pad, "Got %s event", GST_EVENT_TYPE_NAME (event));

  otherpad = (pad == self->srcpad) ? self->sinkpad : self->srcpad;
  ret = gst_pad_push_event (otherpad, event);

  gst_object_unref (self);
  return ret;
}

static GstFlowReturn
gst_jp2k_decimator_decimate_jpc (GstJP2kDecimator * self, GstBuffer * inbuf,
    GstBuffer ** outbuf_)
{
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  GstByteReader reader = GST_BYTE_READER_INIT_FROM_BUFFER (inbuf);
  GstByteWriter writer;
  MainHeader main_header;

  gst_byte_writer_init_with_size (&writer, GST_BUFFER_SIZE (inbuf), FALSE);

  /* main header */
  memset (&main_header, 0, sizeof (MainHeader));
  ret = parse_main_header (self, &reader, &main_header);
  if (ret != GST_FLOW_OK)
    goto done;

  ret = decimate_main_header (self, &main_header);
  if (ret != GST_FLOW_OK)
    goto done;

  ret = write_main_header (self, &writer, &main_header);
  if (ret != GST_FLOW_OK)
    goto done;

  outbuf = gst_byte_writer_reset_and_get_buffer (&writer);
  gst_buffer_copy_metadata (outbuf, inbuf, GST_BUFFER_COPY_ALL);

  GST_DEBUG_OBJECT (self,
      "Decimated buffer from %u bytes to %u bytes (%.2lf%%)",
      GST_BUFFER_SIZE (inbuf), GST_BUFFER_SIZE (outbuf),
      (100 * GST_BUFFER_SIZE (outbuf)) / ((gdouble) GST_BUFFER_SIZE (inbuf)));

done:
  *outbuf_ = outbuf;
  reset_main_header (self, &main_header);
  gst_buffer_unref (inbuf);

  return ret;
}

static GstFlowReturn
gst_jp2k_decimator_sink_chain (GstPad * pad, GstBuffer * inbuf)
{
  GstJP2kDecimator *self = GST_JP2K_DECIMATOR (GST_PAD_PARENT (pad));
  GstFlowReturn ret;
  GstBuffer *outbuf;

  GST_LOG_OBJECT (pad,
      "Handling inbuf with timestamp %" GST_TIME_FORMAT " and duration %"
      GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (inbuf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (inbuf)));

  if (self->max_layers == 0 && self->max_decomposition_levels == -1) {
    outbuf = inbuf;
    inbuf = NULL;
    ret = GST_FLOW_OK;
  } else {
    ret = gst_jp2k_decimator_decimate_jpc (self, inbuf, &outbuf);
  }

  if (G_UNLIKELY (ret != GST_FLOW_OK))
    return ret;

  ret = gst_pad_push (self->srcpad, outbuf);

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_jp2k_decimator_debug, "jp2kdecimator", 0,
      "JPEG2000 decimator");

  gst_element_register (plugin, "jp2kdecimator", GST_RANK_NONE,
      gst_jp2k_decimator_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "jp2kdecimator",
    "JPEG2000 decimator", plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
