/* GStreamer ID3 v1 and v2 muxer
 *
 * Copyright (C) 2006 Christophe Fergeau <teuf@gnome.org>
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009 Pioneers of the Inevitable <songbird@songbirdnest.com>
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
 * SECTION:element-id3mux
 * @see_also: #GstID3Demux, #GstTagSetter
 *
 * This element adds ID3v2 tags to the beginning of a stream, and ID3v1 tags
 * to the end.
 *
 * It defaults to writing ID3 version 2.3.0 tags (since those are the most
 * widely supported), but can optionally write version 2.4.0 tags.
 *
 * Applications can set the tags to write using the #GstTagSetter interface.
 * Tags sent by upstream elements will be picked up automatically (and merged
 * according to the merge mode set via the tag setter interface).
 *
 * <refsect2>
 * <title>Example pipelines</title>
 * |[
 * gst-launch -v filesrc location=foo.ogg ! decodebin ! audioconvert ! lame ! id3mux ! filesink location=foo.mp3
 * ]| A pipeline that transcodes a file from Ogg/Vorbis to mp3 format with
 * ID3 tags that contain the same metadata as the the Ogg/Vorbis file.
 * Make sure the Ogg/Vorbis file actually has comments to preserve.
 * |[
 * gst-launch -m filesrc location=foo.mp3 ! id3demux ! fakesink silent=TRUE 2&gt; /dev/null | grep taglist
 * ]| Verify that tags have been written.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstid3mux.h"
#include <gst/tag/tag.h>

#include <string.h>

GST_DEBUG_CATEGORY (gst_id3_mux_debug);
#define GST_CAT_DEFAULT gst_id3_mux_debug

enum
{
  ARG_0,
  ARG_WRITE_V1,
  ARG_WRITE_V2,
  ARG_V2_MAJOR_VERSION
};

#define DEFAULT_WRITE_V1 FALSE
#define DEFAULT_WRITE_V2 TRUE
#define DEFAULT_V2_MAJOR_VERSION 3

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-id3"));

GST_BOILERPLATE (GstId3Mux, gst_id3_mux, GstTagMux, GST_TYPE_TAG_MUX);

static GstBuffer *gst_id3_mux_render_v2_tag (GstTagMux * mux,
    const GstTagList * taglist);
static GstBuffer *gst_id3_mux_render_v1_tag (GstTagMux * mux,
    const GstTagList * taglist);

static void gst_id3_mux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_id3_mux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_id3_mux_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class,
      &sink_template);

  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_details_simple (element_class,
      "ID3 v1 and v2 Muxer", "Formatter/Metadata",
      "Adds an ID3v2 header and ID3v1 footer to a file",
      "Michael Smith <msmith@songbirdnest.com>, "
      "Tim-Philipp Müller <tim centricular net>");
}

static void
gst_id3_mux_class_init (GstId3MuxClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_id3_mux_set_property;
  gobject_class->get_property = gst_id3_mux_get_property;

  g_object_class_install_property (gobject_class, ARG_WRITE_V1,
      g_param_spec_boolean ("write-v1", "Write id3v1 tag",
          "Write an id3v1 tag at the end of the file", DEFAULT_WRITE_V1,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_WRITE_V2,
      g_param_spec_boolean ("write-v2", "Write id3v2 tag",
          "Write an id3v2 tag at the start of the file", DEFAULT_WRITE_V2,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_V2_MAJOR_VERSION,
      g_param_spec_int ("v2-version", "Version (3 or 4) of id3v2 tag",
          "Set version (3 for id3v2.3, 4 for id3v2.4) of id3v2 tags",
          3, 4, DEFAULT_V2_MAJOR_VERSION,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  GST_TAG_MUX_CLASS (klass)->render_start_tag =
      GST_DEBUG_FUNCPTR (gst_id3_mux_render_v2_tag);
  GST_TAG_MUX_CLASS (klass)->render_end_tag =
      GST_DEBUG_FUNCPTR (gst_id3_mux_render_v1_tag);
}

static void
gst_id3_mux_init (GstId3Mux * id3mux, GstId3MuxClass * id3mux_class)
{
  id3mux->write_v1 = DEFAULT_WRITE_V1;
  id3mux->write_v2 = DEFAULT_WRITE_V2;

  id3mux->v2_major_version = DEFAULT_V2_MAJOR_VERSION;
}

static void
gst_id3_mux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstId3Mux *mux = GST_ID3_MUX (object);

  switch (prop_id) {
    case ARG_WRITE_V1:
      mux->write_v1 = g_value_get_boolean (value);
      break;
    case ARG_WRITE_V2:
      mux->write_v2 = g_value_get_boolean (value);
      break;
    case ARG_V2_MAJOR_VERSION:
      mux->v2_major_version = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_id3_mux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstId3Mux *mux = GST_ID3_MUX (object);

  switch (prop_id) {
    case ARG_WRITE_V1:
      g_value_set_boolean (value, mux->write_v1);
      break;
    case ARG_WRITE_V2:
      g_value_set_boolean (value, mux->write_v2);
      break;
    case ARG_V2_MAJOR_VERSION:
      g_value_set_int (value, mux->v2_major_version);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstBuffer *
gst_id3_mux_render_v2_tag (GstTagMux * mux, const GstTagList * taglist)
{
  GstId3Mux *id3mux = GST_ID3_MUX (mux);

  if (id3mux->write_v2)
    return id3_mux_render_v2_tag (mux, taglist, id3mux->v2_major_version);
  else
    return NULL;
}

static GstBuffer *
gst_id3_mux_render_v1_tag (GstTagMux * mux, const GstTagList * taglist)
{
  GstId3Mux *id3mux = GST_ID3_MUX (mux);

  if (id3mux->write_v1)
    return id3_mux_render_v1_tag (mux, taglist);
  else
    return NULL;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_id3_mux_debug, "id3mux", 0,
      "ID3 v1 and v2 tag muxer");

  if (!gst_element_register (plugin, "id3mux", GST_RANK_PRIMARY,
          GST_TYPE_ID3_MUX))
    return FALSE;

  gst_tag_register_musicbrainz_tags ();

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "id3tag",
    "ID3 v1 and v2 muxing plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
