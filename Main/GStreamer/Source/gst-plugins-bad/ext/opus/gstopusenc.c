/* GStreamer Opus Encoder
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2008> Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) <2011> Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
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
 * Based on the speexenc element
 */

/**
 * SECTION:element-opusenc
 * @see_also: opusdec, oggmux
 *
 * This element encodes raw audio to OPUS.
 *
 * <refsect2>
 * <title>Example pipelines</title>
 * |[
 * gst-launch -v audiotestsrc wave=sine num-buffers=100 ! audioconvert ! opusenc ! oggmux ! filesink location=sine.ogg
 * ]| Encode a test sine signal to Ogg/OPUS.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <opus/opus.h>

#include <gst/gsttagsetter.h>
#include <gst/audio/audio.h>
#include "gstopusheader.h"
#include "gstopuscommon.h"
#include "gstopusenc.h"

GST_DEBUG_CATEGORY_STATIC (opusenc_debug);
#define GST_CAT_DEFAULT opusenc_debug

/* Some arbitrary bounds beyond which it really doesn't make sense.
   The spec mentions 6 kb/s to 510 kb/s, so 4000 and 650000 ought to be
   safe as property bounds. */
#define LOWEST_BITRATE 4000
#define HIGHEST_BITRATE 650000

#define GST_OPUS_ENC_TYPE_BANDWIDTH (gst_opus_enc_bandwidth_get_type())
static GType
gst_opus_enc_bandwidth_get_type (void)
{
  static const GEnumValue values[] = {
    {OPUS_BANDWIDTH_NARROWBAND, "Narrow band", "narrowband"},
    {OPUS_BANDWIDTH_MEDIUMBAND, "Medium band", "mediumband"},
    {OPUS_BANDWIDTH_WIDEBAND, "Wide band", "wideband"},
    {OPUS_BANDWIDTH_SUPERWIDEBAND, "Super wide band", "superwideband"},
    {OPUS_BANDWIDTH_FULLBAND, "Full band", "fullband"},
    {OPUS_AUTO, "Auto", "auto"},
    {0, NULL, NULL}
  };
  static volatile GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_enum_register_static ("GstOpusEncBandwidth", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

#define GST_OPUS_ENC_TYPE_FRAME_SIZE (gst_opus_enc_frame_size_get_type())
static GType
gst_opus_enc_frame_size_get_type (void)
{
  static const GEnumValue values[] = {
    {2, "2.5", "2.5"},
    {5, "5", "5"},
    {10, "10", "10"},
    {20, "20", "20"},
    {40, "40", "40"},
    {60, "60", "60"},
    {0, NULL, NULL}
  };
  static volatile GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_enum_register_static ("GstOpusEncFrameSize", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "rate = (int) { 48000, 24000, 16000, 12000, 8000 }, "
        "channels = (int) [ 1, 8 ], "
        "endianness = (int) BYTE_ORDER, "
        "signed = (boolean) TRUE, " "width = (int) 16, " "depth = (int) 16")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-opus")
    );

#define DEFAULT_AUDIO           TRUE
#define DEFAULT_BITRATE         64000
#define DEFAULT_BANDWIDTH       OPUS_BANDWIDTH_FULLBAND
#define DEFAULT_FRAMESIZE       20
#define DEFAULT_CBR             TRUE
#define DEFAULT_CONSTRAINED_VBR TRUE
#define DEFAULT_COMPLEXITY      10
#define DEFAULT_INBAND_FEC      FALSE
#define DEFAULT_DTX             FALSE
#define DEFAULT_PACKET_LOSS_PERCENT 0
#define DEFAULT_MAX_PAYLOAD_SIZE 1024

enum
{
  PROP_0,
  PROP_AUDIO,
  PROP_BITRATE,
  PROP_BANDWIDTH,
  PROP_FRAME_SIZE,
  PROP_CBR,
  PROP_CONSTRAINED_VBR,
  PROP_COMPLEXITY,
  PROP_INBAND_FEC,
  PROP_DTX,
  PROP_PACKET_LOSS_PERCENT,
  PROP_MAX_PAYLOAD_SIZE
};

static void gst_opus_enc_finalize (GObject * object);

static gboolean gst_opus_enc_sink_event (GstAudioEncoder * benc,
    GstEvent * event);
static GstCaps *gst_opus_enc_sink_getcaps (GstAudioEncoder * benc);
static gboolean gst_opus_enc_setup (GstOpusEnc * enc);

static void gst_opus_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_opus_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static gboolean gst_opus_enc_start (GstAudioEncoder * benc);
static gboolean gst_opus_enc_stop (GstAudioEncoder * benc);
static gboolean gst_opus_enc_set_format (GstAudioEncoder * benc,
    GstAudioInfo * info);
static GstFlowReturn gst_opus_enc_handle_frame (GstAudioEncoder * benc,
    GstBuffer * buf);
static gint64 gst_opus_enc_get_latency (GstOpusEnc * enc);

static GstFlowReturn gst_opus_enc_encode (GstOpusEnc * enc, GstBuffer * buffer);

static void
gst_opus_enc_setup_interfaces (GType opusenc_type)
{
  static const GInterfaceInfo tag_setter_info = { NULL, NULL, NULL };
  const GInterfaceInfo preset_interface_info = {
    NULL,                       /* interface_init */
    NULL,                       /* interface_finalize */
    NULL                        /* interface_data */
  };

  g_type_add_interface_static (opusenc_type, GST_TYPE_TAG_SETTER,
      &tag_setter_info);
  g_type_add_interface_static (opusenc_type, GST_TYPE_PRESET,
      &preset_interface_info);

  GST_DEBUG_CATEGORY_INIT (opusenc_debug, "opusenc", 0, "Opus encoder");
}

GST_BOILERPLATE_FULL (GstOpusEnc, gst_opus_enc, GstAudioEncoder,
    GST_TYPE_AUDIO_ENCODER, gst_opus_enc_setup_interfaces);

static void
gst_opus_enc_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);
  gst_element_class_set_details_simple (element_class, "Opus audio encoder",
      "Codec/Encoder/Audio",
      "Encodes audio in Opus format",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");
}

static void
gst_opus_enc_class_init (GstOpusEncClass * klass)
{
  GObjectClass *gobject_class;
  GstAudioEncoderClass *base_class;

  gobject_class = (GObjectClass *) klass;
  base_class = (GstAudioEncoderClass *) klass;

  gobject_class->set_property = gst_opus_enc_set_property;
  gobject_class->get_property = gst_opus_enc_get_property;

  base_class->start = GST_DEBUG_FUNCPTR (gst_opus_enc_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_opus_enc_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_opus_enc_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_opus_enc_handle_frame);
  base_class->event = GST_DEBUG_FUNCPTR (gst_opus_enc_sink_event);
  base_class->getcaps = GST_DEBUG_FUNCPTR (gst_opus_enc_sink_getcaps);

  g_object_class_install_property (gobject_class, PROP_AUDIO,
      g_param_spec_boolean ("audio", "Audio or voice",
          "Audio or voice", DEFAULT_AUDIO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BITRATE,
      g_param_spec_int ("bitrate", "Encoding Bit-rate",
          "Specify an encoding bit-rate (in bps).",
          LOWEST_BITRATE, HIGHEST_BITRATE, DEFAULT_BITRATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_BANDWIDTH,
      g_param_spec_enum ("bandwidth", "Band Width", "Audio Band Width",
          GST_OPUS_ENC_TYPE_BANDWIDTH, DEFAULT_BANDWIDTH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_FRAME_SIZE,
      g_param_spec_enum ("frame-size", "Frame Size",
          "The duration of an audio frame, in ms", GST_OPUS_ENC_TYPE_FRAME_SIZE,
          DEFAULT_FRAMESIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_CBR,
      g_param_spec_boolean ("cbr", "Constant bit rate", "Constant bit rate",
          DEFAULT_CBR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_CONSTRAINED_VBR,
      g_param_spec_boolean ("constrained-vbr", "Constrained VBR",
          "Constrained VBR", DEFAULT_CONSTRAINED_VBR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_COMPLEXITY,
      g_param_spec_int ("complexity", "Complexity", "Complexity", 0, 10,
          DEFAULT_COMPLEXITY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_INBAND_FEC,
      g_param_spec_boolean ("inband-fec", "In-band FEC",
          "Enable forward error correction", DEFAULT_INBAND_FEC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_DTX,
      g_param_spec_boolean ("dtx", "DTX", "DTX", DEFAULT_DTX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_PACKET_LOSS_PERCENT, g_param_spec_int ("packet-loss-percentage",
          "Loss percentage", "Packet loss percentage", 0, 100,
          DEFAULT_PACKET_LOSS_PERCENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_MAX_PAYLOAD_SIZE, g_param_spec_uint ("max-payload-size",
          "Max payload size", "Maximum payload size in bytes", 2, 1275,
          DEFAULT_MAX_PAYLOAD_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_opus_enc_finalize);
}

static void
gst_opus_enc_finalize (GObject * object)
{
  GstOpusEnc *enc;

  enc = GST_OPUS_ENC (object);

  g_mutex_free (enc->property_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_opus_enc_init (GstOpusEnc * enc, GstOpusEncClass * klass)
{
  GstAudioEncoder *benc = GST_AUDIO_ENCODER (enc);

  GST_DEBUG_OBJECT (enc, "init");

  enc->property_lock = g_mutex_new ();

  enc->n_channels = -1;
  enc->sample_rate = -1;
  enc->frame_samples = 0;

  enc->bitrate = DEFAULT_BITRATE;
  enc->bandwidth = DEFAULT_BANDWIDTH;
  enc->frame_size = DEFAULT_FRAMESIZE;
  enc->cbr = DEFAULT_CBR;
  enc->constrained_vbr = DEFAULT_CONSTRAINED_VBR;
  enc->complexity = DEFAULT_COMPLEXITY;
  enc->inband_fec = DEFAULT_INBAND_FEC;
  enc->dtx = DEFAULT_DTX;
  enc->packet_loss_percentage = DEFAULT_PACKET_LOSS_PERCENT;
  enc->max_payload_size = DEFAULT_MAX_PAYLOAD_SIZE;

  /* arrange granulepos marking (and required perfect ts) */
  gst_audio_encoder_set_mark_granule (benc, TRUE);
  gst_audio_encoder_set_perfect_timestamp (benc, TRUE);
}

static gboolean
gst_opus_enc_start (GstAudioEncoder * benc)
{
  GstOpusEnc *enc = GST_OPUS_ENC (benc);

  GST_DEBUG_OBJECT (enc, "start");
  enc->tags = gst_tag_list_new ();
  enc->header_sent = FALSE;

  return TRUE;
}

static gboolean
gst_opus_enc_stop (GstAudioEncoder * benc)
{
  GstOpusEnc *enc = GST_OPUS_ENC (benc);

  GST_DEBUG_OBJECT (enc, "stop");
  enc->header_sent = FALSE;
  if (enc->state) {
    opus_multistream_encoder_destroy (enc->state);
    enc->state = NULL;
  }
  gst_tag_list_free (enc->tags);
  enc->tags = NULL;
  g_slist_foreach (enc->headers, (GFunc) gst_buffer_unref, NULL);
  g_slist_free (enc->headers);
  enc->headers = NULL;
  gst_tag_setter_reset_tags (GST_TAG_SETTER (enc));

  return TRUE;
}

static gint64
gst_opus_enc_get_latency (GstOpusEnc * enc)
{
  gint64 latency = gst_util_uint64_scale (enc->frame_samples, GST_SECOND,
      enc->sample_rate);
  GST_DEBUG_OBJECT (enc, "Latency: %" GST_TIME_FORMAT, GST_TIME_ARGS (latency));
  return latency;
}

static void
gst_opus_enc_setup_base_class (GstOpusEnc * enc, GstAudioEncoder * benc)
{
  gst_audio_encoder_set_latency (benc,
      gst_opus_enc_get_latency (enc), gst_opus_enc_get_latency (enc));
  gst_audio_encoder_set_frame_samples_min (benc,
      enc->frame_samples * enc->n_channels * 2);
  gst_audio_encoder_set_frame_samples_max (benc,
      enc->frame_samples * enc->n_channels * 2);
  gst_audio_encoder_set_frame_max (benc, 0);
}

static gint
gst_opus_enc_get_frame_samples (GstOpusEnc * enc)
{
  gint frame_samples = 0;
  switch (enc->frame_size) {
    case 2:
      frame_samples = enc->sample_rate / 400;
      break;
    case 5:
      frame_samples = enc->sample_rate / 200;
      break;
    case 10:
      frame_samples = enc->sample_rate / 100;
      break;
    case 20:
      frame_samples = enc->sample_rate / 50;
      break;
    case 40:
      frame_samples = enc->sample_rate / 25;
      break;
    case 60:
      frame_samples = 3 * enc->sample_rate / 50;
      break;
    default:
      GST_WARNING_OBJECT (enc, "Unsupported frame size: %d", enc->frame_size);
      frame_samples = 0;
      break;
  }
  return frame_samples;
}

static void
gst_opus_enc_setup_trivial_mapping (GstOpusEnc * enc, guint8 mapping[256])
{
  int n;

  for (n = 0; n < 255; ++n)
    mapping[n] = n;
}

static int
gst_opus_enc_find_channel_position (GstOpusEnc * enc, const GstAudioInfo * info,
    GstAudioChannelPosition position)
{
  int n;
  for (n = 0; n < enc->n_channels; ++n) {
    if (GST_AUDIO_INFO_POSITION (info, n) == position) {
      return n;
    }
  }
  return -1;
}

static int
gst_opus_enc_find_channel_position_in_vorbis_order (GstOpusEnc * enc,
    GstAudioChannelPosition position)
{
  int c;

  for (c = 0; c < enc->n_channels; ++c) {
    if (gst_opus_channel_positions[enc->n_channels - 1][c] == position) {
      GST_INFO_OBJECT (enc,
          "Channel position %s maps to index %d in Vorbis order",
          gst_opus_channel_names[position], c);
      return c;
    }
  }
  GST_WARNING_OBJECT (enc,
      "Channel position %s is not representable in Vorbis order",
      gst_opus_channel_names[position]);
  return -1;
}

static void
gst_opus_enc_setup_channel_mappings (GstOpusEnc * enc,
    const GstAudioInfo * info)
{
#define MAPS(idx,pos) (GST_AUDIO_INFO_POSITION (info, (idx)) == GST_AUDIO_CHANNEL_POSITION_##pos)

  int n;

  GST_DEBUG_OBJECT (enc, "Setting up channel mapping for %d channels",
      enc->n_channels);

  /* Start by setting up a default trivial mapping */
  enc->n_stereo_streams = 0;
  gst_opus_enc_setup_trivial_mapping (enc, enc->encoding_channel_mapping);
  gst_opus_enc_setup_trivial_mapping (enc, enc->decoding_channel_mapping);

  /* For one channel, use the basic RTP mapping */
  if (enc->n_channels == 1) {
    GST_INFO_OBJECT (enc, "Mono, trivial RTP mapping");
    enc->channel_mapping_family = 0;
    /* implicit mapping for family 0 */
    return;
  }

  /* For two channels, use the basic RTP mapping if the channels are
     mapped as left/right. */
  if (enc->n_channels == 2) {
    if (MAPS (0, FRONT_LEFT) && MAPS (1, FRONT_RIGHT)) {
      GST_INFO_OBJECT (enc, "Stereo, canonical mapping");
      enc->channel_mapping_family = 0;
      enc->n_stereo_streams = 1;
      /* The channel mapping is implicit for family 0, that's why we do not
         attempt to create one for right/left - this will be mapped to the
         Vorbis mapping below. */
      return;
    } else {
      GST_DEBUG_OBJECT (enc, "Stereo, but not canonical mapping, continuing");
    }
  }

  /* For channels between 1 and 8, we use the Vorbis mapping if we can
     find a permutation that matches it. Mono will have been taken care
     of earlier, but this code also handles it. Same for left/right stereo.
     There are two mappings. One maps the input channels to an ordering
     which has the natural pairs first so they can benefit from the Opus
     stereo channel coupling, and the other maps this ordering to the
     Vorbis ordering. */
  if (enc->n_channels >= 1 && enc->n_channels <= 8) {
    int c0, c1, c0v, c1v;
    int mapped;
    gboolean positions_done[256];
    static const GstAudioChannelPosition pairs[][2] = {
      {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
          GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT},
      {GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
          GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT},
      {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER,
          GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER},
      {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER,
          GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER},
      {GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
          GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT},
    };
    size_t pair;

    GST_DEBUG_OBJECT (enc,
        "In range for the Vorbis mapping, building channel mapping tables");

    enc->n_stereo_streams = 0;
    mapped = 0;
    for (n = 0; n < 256; ++n)
      positions_done[n] = FALSE;

    /* First, find any natural pairs, and move them to the front */
    for (pair = 0; pair < G_N_ELEMENTS (pairs); ++pair) {
      GstAudioChannelPosition p0 = pairs[pair][0];
      GstAudioChannelPosition p1 = pairs[pair][1];
      c0 = gst_opus_enc_find_channel_position (enc, info, p0);
      c1 = gst_opus_enc_find_channel_position (enc, info, p1);
      if (c0 >= 0 && c1 >= 0) {
        /* We found a natural pair */
        GST_DEBUG_OBJECT (enc, "Natural pair '%s/%s' found at %d %d",
            gst_opus_channel_names[p0], gst_opus_channel_names[p1], c0, c1);
        /* Find where they map in Vorbis order */
        c0v = gst_opus_enc_find_channel_position_in_vorbis_order (enc, p0);
        c1v = gst_opus_enc_find_channel_position_in_vorbis_order (enc, p1);
        if (c0v < 0 || c1v < 0) {
          GST_WARNING_OBJECT (enc,
              "Cannot map channel positions to Vorbis order, using unknown mapping");
          enc->channel_mapping_family = 255;
          enc->n_stereo_streams = 0;
          return;
        }

        enc->encoding_channel_mapping[mapped] = c0;
        enc->encoding_channel_mapping[mapped + 1] = c1;
        enc->decoding_channel_mapping[c0v] = mapped;
        enc->decoding_channel_mapping[c1v] = mapped + 1;
        enc->n_stereo_streams++;
        mapped += 2;
        positions_done[p0] = positions_done[p1] = TRUE;
      }
    }

    /* Now add all other input channels as mono streams */
    for (n = 0; n < enc->n_channels; ++n) {
      GstAudioChannelPosition position = GST_AUDIO_INFO_POSITION (info, n);

      /* if we already mapped it while searching for pairs, nothing else
         needs to be done */
      if (!positions_done[position]) {
        int cv;
        GST_DEBUG_OBJECT (enc, "Channel position %s is not mapped yet, adding",
            gst_opus_channel_names[position]);
        cv = gst_opus_enc_find_channel_position_in_vorbis_order (enc, position);
        if (cv < 0) {
          GST_WARNING_OBJECT (enc,
              "Cannot map channel positions to Vorbis order, using unknown mapping");
          enc->channel_mapping_family = 255;
          enc->n_stereo_streams = 0;
          return;
        }
        enc->encoding_channel_mapping[mapped] = n;
        enc->decoding_channel_mapping[cv] = mapped;
        mapped++;
      }
    }

#ifndef GST_DISABLE_DEBUG
    GST_INFO_OBJECT (enc,
        "Mapping tables built: %d channels, %d stereo streams", enc->n_channels,
        enc->n_stereo_streams);
    gst_opus_common_log_channel_mapping_table (GST_ELEMENT (enc), opusenc_debug,
        "Encoding mapping table", enc->n_channels,
        enc->encoding_channel_mapping);
    gst_opus_common_log_channel_mapping_table (GST_ELEMENT (enc), opusenc_debug,
        "Decoding mapping table", enc->n_channels,
        enc->decoding_channel_mapping);
#endif

    enc->channel_mapping_family = 1;
    return;
  }

  /* More than 8 channels, if future mappings are added for those */

  /* For other cases, we use undefined, with the default trivial mapping
     and all mono streams */
  GST_WARNING_OBJECT (enc, "Unknown mapping");
  enc->channel_mapping_family = 255;
  enc->n_stereo_streams = 0;

#undef MAPS
}

static gboolean
gst_opus_enc_set_format (GstAudioEncoder * benc, GstAudioInfo * info)
{
  GstOpusEnc *enc;

  enc = GST_OPUS_ENC (benc);

  g_mutex_lock (enc->property_lock);

  enc->n_channels = GST_AUDIO_INFO_CHANNELS (info);
  enc->sample_rate = GST_AUDIO_INFO_RATE (info);
  gst_opus_enc_setup_channel_mappings (enc, info);
  GST_DEBUG_OBJECT (benc, "Setup with %d channels, %d Hz", enc->n_channels,
      enc->sample_rate);

  /* handle reconfigure */
  if (enc->state) {
    opus_multistream_encoder_destroy (enc->state);
    enc->state = NULL;
  }
  if (!gst_opus_enc_setup (enc))
    return FALSE;

  enc->frame_samples = gst_opus_enc_get_frame_samples (enc);

  /* feedback to base class */
  gst_opus_enc_setup_base_class (enc, benc);

  g_mutex_unlock (enc->property_lock);

  return TRUE;
}

static gboolean
gst_opus_enc_setup (GstOpusEnc * enc)
{
  int error = OPUS_OK;

#ifndef GST_DISABLE_DEBUG
  GST_DEBUG_OBJECT (enc,
      "setup: %d Hz, %d channels, %d stereo streams, family %d",
      enc->sample_rate, enc->n_channels, enc->n_stereo_streams,
      enc->channel_mapping_family);
  GST_INFO_OBJECT (enc, "Mapping tables built: %d channels, %d stereo streams",
      enc->n_channels, enc->n_stereo_streams);
  gst_opus_common_log_channel_mapping_table (GST_ELEMENT (enc), opusenc_debug,
      "Encoding mapping table", enc->n_channels, enc->encoding_channel_mapping);
  gst_opus_common_log_channel_mapping_table (GST_ELEMENT (enc), opusenc_debug,
      "Decoding mapping table", enc->n_channels, enc->decoding_channel_mapping);
#endif

  enc->state = opus_multistream_encoder_create (enc->sample_rate,
      enc->n_channels, enc->n_channels - enc->n_stereo_streams,
      enc->n_stereo_streams, enc->encoding_channel_mapping,
      enc->audio_or_voip ? OPUS_APPLICATION_AUDIO : OPUS_APPLICATION_VOIP,
      &error);
  if (!enc->state || error != OPUS_OK)
    goto encoder_creation_failed;

  opus_multistream_encoder_ctl (enc->state, OPUS_SET_BITRATE (enc->bitrate), 0);
  opus_multistream_encoder_ctl (enc->state, OPUS_SET_BANDWIDTH (enc->bandwidth),
      0);
  opus_multistream_encoder_ctl (enc->state, OPUS_SET_VBR (!enc->cbr), 0);
  opus_multistream_encoder_ctl (enc->state,
      OPUS_SET_VBR_CONSTRAINT (enc->constrained_vbr), 0);
  opus_multistream_encoder_ctl (enc->state,
      OPUS_SET_COMPLEXITY (enc->complexity), 0);
  opus_multistream_encoder_ctl (enc->state,
      OPUS_SET_INBAND_FEC (enc->inband_fec), 0);
  opus_multistream_encoder_ctl (enc->state, OPUS_SET_DTX (enc->dtx), 0);
  opus_multistream_encoder_ctl (enc->state,
      OPUS_SET_PACKET_LOSS_PERC (enc->packet_loss_percentage), 0);

  GST_LOG_OBJECT (enc, "we have frame size %d", enc->frame_size);

  return TRUE;

encoder_creation_failed:
  GST_ERROR_OBJECT (enc, "Encoder creation failed");
  return FALSE;
}

static gboolean
gst_opus_enc_sink_event (GstAudioEncoder * benc, GstEvent * event)
{
  GstOpusEnc *enc;

  enc = GST_OPUS_ENC (benc);

  GST_DEBUG_OBJECT (enc, "sink event: %s", GST_EVENT_TYPE_NAME (event));
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:
    {
      GstTagList *list;
      GstTagSetter *setter = GST_TAG_SETTER (enc);
      const GstTagMergeMode mode = gst_tag_setter_get_tag_merge_mode (setter);

      gst_event_parse_tag (event, &list);
      gst_tag_setter_merge_tags (setter, list, mode);
      break;
    }
    default:
      break;
  }

  return FALSE;
}

static GstCaps *
gst_opus_enc_sink_getcaps (GstAudioEncoder * benc)
{
  GstOpusEnc *enc;
  GstCaps *caps;
  GstCaps *peercaps = NULL;
  GstCaps *intersect = NULL;
  guint i;
  gboolean allow_multistream;

  enc = GST_OPUS_ENC (benc);

  GST_DEBUG_OBJECT (enc, "sink getcaps");

  peercaps = gst_pad_peer_get_caps (GST_AUDIO_ENCODER_SRC_PAD (benc));
  if (!peercaps) {
    GST_DEBUG_OBJECT (benc, "No peercaps, returning template sink caps");
    return
        gst_caps_copy (gst_pad_get_pad_template_caps
        (GST_AUDIO_ENCODER_SINK_PAD (benc)));
  }

  intersect = gst_caps_intersect (peercaps,
      gst_pad_get_pad_template_caps (GST_AUDIO_ENCODER_SRC_PAD (benc)));
  gst_caps_unref (peercaps);

  if (gst_caps_is_empty (intersect))
    return intersect;

  allow_multistream = FALSE;
  for (i = 0; i < gst_caps_get_size (intersect); i++) {
    GstStructure *s = gst_caps_get_structure (intersect, i);
    gboolean multistream;
    if (gst_structure_get_boolean (s, "multistream", &multistream)) {
      if (multistream) {
        allow_multistream = TRUE;
      }
    } else {
      allow_multistream = TRUE;
    }
  }

  gst_caps_unref (intersect);

  caps =
      gst_caps_copy (gst_pad_get_pad_template_caps (GST_AUDIO_ENCODER_SINK_PAD
          (benc)));
  if (!allow_multistream) {
    GValue range = { 0 };
    g_value_init (&range, GST_TYPE_INT_RANGE);
    gst_value_set_int_range (&range, 1, 2);
    for (i = 0; i < gst_caps_get_size (caps); i++) {
      GstStructure *s = gst_caps_get_structure (caps, i);
      gst_structure_set_value (s, "channels", &range);
    }
    g_value_unset (&range);
  }

  GST_DEBUG_OBJECT (enc, "Returning caps: %" GST_PTR_FORMAT, caps);
  return caps;
}

static GstFlowReturn
gst_opus_enc_encode (GstOpusEnc * enc, GstBuffer * buf)
{
  guint8 *bdata, *data, *mdata = NULL;
  gsize bsize, size;
  gsize bytes;
  gint ret = GST_FLOW_OK;

  g_mutex_lock (enc->property_lock);

  bytes = enc->frame_samples * enc->n_channels * 2;
  if (G_LIKELY (buf)) {
    bdata = GST_BUFFER_DATA (buf);
    bsize = GST_BUFFER_SIZE (buf);
    if (G_UNLIKELY (bsize % bytes)) {
      GST_DEBUG_OBJECT (enc, "draining; adding silence samples");

      size = ((bsize / bytes) + 1) * bytes;
      mdata = g_malloc0 (size);
      memcpy (mdata, bdata, bsize);
      bdata = NULL;
      data = mdata;
    } else {
      data = bdata;
      size = bsize;
    }
  } else {
    GST_DEBUG_OBJECT (enc, "nothing to drain");
    goto done;
  }


  while (size) {
    gint outsize;
    GstBuffer *outbuf;

    ret = gst_pad_alloc_buffer_and_set_caps (GST_AUDIO_ENCODER_SRC_PAD (enc),
        GST_BUFFER_OFFSET_NONE, enc->max_payload_size * enc->n_channels,
        GST_PAD_CAPS (GST_AUDIO_ENCODER_SRC_PAD (enc)), &outbuf);

    if (GST_FLOW_OK != ret)
      goto done;

    GST_DEBUG_OBJECT (enc, "encoding %d samples (%d bytes)",
        enc->frame_samples, (int) bytes);

    outsize =
        opus_multistream_encode (enc->state, (const gint16 *) data,
        enc->frame_samples, GST_BUFFER_DATA (outbuf),
        enc->max_payload_size * enc->n_channels);

    if (outsize < 0) {
      GST_ERROR_OBJECT (enc, "Encoding failed: %d", outsize);
      ret = GST_FLOW_ERROR;
      goto done;
    } else if (outsize > enc->max_payload_size) {
      GST_WARNING_OBJECT (enc,
          "Encoded size %d is higher than max payload size (%d bytes)",
          outsize, enc->max_payload_size);
      ret = GST_FLOW_ERROR;
      goto done;
    }

    GST_DEBUG_OBJECT (enc, "Output packet is %u bytes", outsize);
    GST_BUFFER_SIZE (outbuf) = outsize;

    ret =
        gst_audio_encoder_finish_frame (GST_AUDIO_ENCODER (enc), outbuf,
        enc->frame_samples);

    if ((GST_FLOW_OK != ret) && (GST_FLOW_NOT_LINKED != ret))
      goto done;

    data += bytes;
    size -= bytes;
  }

done:

  g_mutex_unlock (enc->property_lock);

  if (mdata)
    g_free (mdata);

  return ret;
}

static GstFlowReturn
gst_opus_enc_handle_frame (GstAudioEncoder * benc, GstBuffer * buf)
{
  GstOpusEnc *enc;
  GstFlowReturn ret = GST_FLOW_OK;

  enc = GST_OPUS_ENC (benc);
  GST_DEBUG_OBJECT (enc, "handle_frame");

  if (!enc->header_sent) {
    GstCaps *caps;

    g_slist_foreach (enc->headers, (GFunc) gst_buffer_unref, NULL);
    g_slist_free (enc->headers);
    enc->headers = NULL;

    gst_opus_header_create_caps (&caps, &enc->headers, enc->n_channels,
        enc->n_stereo_streams, enc->sample_rate, enc->channel_mapping_family,
        enc->decoding_channel_mapping,
        gst_tag_setter_get_tag_list (GST_TAG_SETTER (enc)));


    /* negotiate with these caps */
    GST_DEBUG_OBJECT (enc, "here are the caps: %" GST_PTR_FORMAT, caps);

    gst_pad_set_caps (GST_AUDIO_ENCODER_SRC_PAD (enc), caps);
    gst_caps_unref (caps);

    enc->header_sent = TRUE;
  }

  GST_DEBUG_OBJECT (enc, "received buffer %p of %u bytes", buf,
      buf ? GST_BUFFER_SIZE (buf) : 0);

  ret = gst_opus_enc_encode (enc, buf);

  return ret;
}

static void
gst_opus_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOpusEnc *enc;

  enc = GST_OPUS_ENC (object);

  g_mutex_lock (enc->property_lock);

  switch (prop_id) {
    case PROP_AUDIO:
      g_value_set_boolean (value, enc->audio_or_voip);
      break;
    case PROP_BITRATE:
      g_value_set_int (value, enc->bitrate);
      break;
    case PROP_BANDWIDTH:
      g_value_set_enum (value, enc->bandwidth);
      break;
    case PROP_FRAME_SIZE:
      g_value_set_enum (value, enc->frame_size);
      break;
    case PROP_CBR:
      g_value_set_boolean (value, enc->cbr);
      break;
    case PROP_CONSTRAINED_VBR:
      g_value_set_boolean (value, enc->constrained_vbr);
      break;
    case PROP_COMPLEXITY:
      g_value_set_int (value, enc->complexity);
      break;
    case PROP_INBAND_FEC:
      g_value_set_boolean (value, enc->inband_fec);
      break;
    case PROP_DTX:
      g_value_set_boolean (value, enc->dtx);
      break;
    case PROP_PACKET_LOSS_PERCENT:
      g_value_set_int (value, enc->packet_loss_percentage);
      break;
    case PROP_MAX_PAYLOAD_SIZE:
      g_value_set_uint (value, enc->max_payload_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock (enc->property_lock);
}

static void
gst_opus_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOpusEnc *enc;

  enc = GST_OPUS_ENC (object);

#define GST_OPUS_UPDATE_PROPERTY(prop,type,ctl) do { \
  g_mutex_lock (enc->property_lock); \
  enc->prop = g_value_get_##type (value); \
  if (enc->state) { \
    opus_multistream_encoder_ctl (enc->state, OPUS_SET_##ctl (enc->prop)); \
  } \
  g_mutex_unlock (enc->property_lock); \
} while(0)

  switch (prop_id) {
    case PROP_AUDIO:
      enc->audio_or_voip = g_value_get_boolean (value);
      break;
    case PROP_BITRATE:
      GST_OPUS_UPDATE_PROPERTY (bitrate, int, BITRATE);
      break;
    case PROP_BANDWIDTH:
      GST_OPUS_UPDATE_PROPERTY (bandwidth, enum, BANDWIDTH);
      break;
    case PROP_FRAME_SIZE:
      g_mutex_lock (enc->property_lock);
      enc->frame_size = g_value_get_enum (value);
      enc->frame_samples = gst_opus_enc_get_frame_samples (enc);
      gst_opus_enc_setup_base_class (enc, GST_AUDIO_ENCODER (enc));
      g_mutex_unlock (enc->property_lock);
      break;
    case PROP_CBR:
      /* this one has an opposite meaning to the opus ctl... */
      g_mutex_lock (enc->property_lock);
      enc->cbr = g_value_get_boolean (value);
      opus_multistream_encoder_ctl (enc->state, OPUS_SET_VBR (!enc->cbr));
      g_mutex_unlock (enc->property_lock);
      break;
    case PROP_CONSTRAINED_VBR:
      GST_OPUS_UPDATE_PROPERTY (constrained_vbr, boolean, VBR_CONSTRAINT);
      break;
    case PROP_COMPLEXITY:
      GST_OPUS_UPDATE_PROPERTY (complexity, int, COMPLEXITY);
      break;
    case PROP_INBAND_FEC:
      GST_OPUS_UPDATE_PROPERTY (inband_fec, boolean, INBAND_FEC);
      break;
    case PROP_DTX:
      GST_OPUS_UPDATE_PROPERTY (dtx, boolean, DTX);
      break;
    case PROP_PACKET_LOSS_PERCENT:
      GST_OPUS_UPDATE_PROPERTY (packet_loss_percentage, int, PACKET_LOSS_PERC);
      break;
    case PROP_MAX_PAYLOAD_SIZE:
      g_mutex_lock (enc->property_lock);
      enc->max_payload_size = g_value_get_uint (value);
      g_mutex_unlock (enc->property_lock);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

#undef GST_OPUS_UPDATE_PROPERTY

}
