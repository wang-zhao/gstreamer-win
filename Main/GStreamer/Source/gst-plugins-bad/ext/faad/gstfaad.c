/* GStreamer FAAD (Free AAC Decoder) plugin
 * Copyright (C) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
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
 * SECTION:element-faad
 * @seealso: faac
 *
 * faad decodes AAC (MPEG-4 part 3) stream.
 *
 * <refsect2>
 * <title>Example launch lines</title>
 * |[
 * gst-launch filesrc location=example.mp4 ! qtdemux ! faad ! audioconvert ! audioresample ! autoaudiosink
 * ]| Play aac from mp4 file.
 * |[
 * gst-launch filesrc location=example.adts ! faad ! audioconvert ! audioresample ! autoaudiosink
 * ]| Play standalone aac bitstream.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gst/audio/audio.h>
#include <gst/audio/multichannel.h>

/* These are the correct types for these functions, as defined in the source,
 * with types changed to match glib types, since those are defined for us.
 * However, upstream FAAD is distributed with a broken header file that defined
 * these wrongly (in a way which was broken on 64 bit systems).
 *
 * Upstream CVS still has the bug, but has also renamed all the public symbols
 * for Better Corporate Branding (or whatever), so we need to take that
 * (FAAD_IS_NEAAC) into account as well.
 *
 * We must call them using these definitions. Most distributions now have the
 * corrected header file (they distribute a patch along with the source), 
 * but not all, hence this Truly Evil Hack.
 *
 * Note: The prototypes don't need to be defined conditionaly, as the cpp will
 * do that for us.
 */
#if FAAD2_MINOR_VERSION < 7
#ifdef FAAD_IS_NEAAC
#define NeAACDecInit NeAACDecInit_no_definition
#define NeAACDecInit2 NeAACDecInit2_no_definition
#else
#define faacDecInit faacDecInit_no_definition
#define faacDecInit2 faacDecInit2_no_definition
#endif
#endif /* FAAD2_MINOR_VERSION < 7 */

#include "gstfaad.h"

#if FAAD2_MINOR_VERSION < 7
#ifdef FAAD_IS_NEAAC
#undef NeAACDecInit
#undef NeAACDecInit2
#else
#undef faacDecInit
#undef faacDecInit2
#endif

extern long faacDecInit (faacDecHandle, guint8 *, guint32, guint32 *, guint8 *);
extern gint8 faacDecInit2 (faacDecHandle, guint8 *, guint32,
    guint32 *, guint8 *);

#endif /* FAAD2_MINOR_VERSION < 7 */

GST_DEBUG_CATEGORY_STATIC (faad_debug);
#define GST_CAT_DEFAULT faad_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg, " "mpegversion = (int) 2; "
        "audio/mpeg, mpegversion = (int) 4, stream-format = (string) { raw, adts }")
    );

#define STATIC_INT_CAPS(bpp) \
  "audio/x-raw-int, " \
    "endianness = (int) BYTE_ORDER, " \
    "signed = (bool) TRUE, " \
    "width = (int) " G_STRINGIFY (bpp) ", " \
    "depth = (int) " G_STRINGIFY (bpp) ", " \
    "rate = (int) [ 8000, 96000 ], " \
    "channels = (int) [ 1, 8 ]"

#if 0
#define STATIC_FLOAT_CAPS(bpp) \
  "audio/x-raw-float, " \
    "endianness = (int) BYTE_ORDER, " \
    "depth = (int) " G_STRINGIFY (bpp) ", " \
    "rate = (int) [ 8000, 96000 ], " \
    "channels = (int) [ 1, 8 ]"
#endif

/*
 * All except 16-bit integer are disabled until someone fixes FAAD.
 * FAAD allocates approximately 8*1024*2 bytes bytes, which is enough
 * for 1 frame (1024 samples) of 6 channel (5.1) 16-bit integer 16bpp
 * audio, but not for any other. You'll get random segfaults, crashes
 * and even valgrind goes crazy.
 */

#define STATIC_CAPS \
  STATIC_INT_CAPS (16)
#if 0
#define NOTUSED "; " \
STATIC_INT_CAPS (24) \
    "; " \
STATIC_INT_CAPS (32) \
    "; " \
STATIC_FLOAT_CAPS (32) \
    "; " \
STATIC_FLOAT_CAPS (64)
#endif

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (STATIC_CAPS)
    );

static void gst_faad_reset (GstFaad * faad);

static gboolean gst_faad_start (GstAudioDecoder * dec);
static gboolean gst_faad_stop (GstAudioDecoder * dec);
static gboolean gst_faad_set_format (GstAudioDecoder * dec, GstCaps * caps);
static gboolean gst_faad_parse (GstAudioDecoder * dec, GstAdapter * adapter,
    gint * offset, gint * length);
static GstFlowReturn gst_faad_handle_frame (GstAudioDecoder * dec,
    GstBuffer * buffer);
static void gst_faad_flush (GstAudioDecoder * dec, gboolean hard);

static gboolean gst_faad_open_decoder (GstFaad * faad);
static void gst_faad_close_decoder (GstFaad * faad);

GST_BOILERPLATE (GstFaad, gst_faad, GstAudioDecoder, GST_TYPE_AUDIO_DECODER);

static void
gst_faad_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &src_template);
  gst_element_class_add_static_pad_template (element_class,
      &sink_template);

  gst_element_class_set_details_simple (element_class, "AAC audio decoder",
      "Codec/Decoder/Audio",
      "Free MPEG-2/4 AAC decoder",
      "Ronald Bultje <rbultje@ronald.bitfreak.net>");

  GST_DEBUG_CATEGORY_INIT (faad_debug, "faad", 0, "AAC decoding");
}

static void
gst_faad_class_init (GstFaadClass * klass)
{
  GstAudioDecoderClass *base_class = GST_AUDIO_DECODER_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  base_class->start = GST_DEBUG_FUNCPTR (gst_faad_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_faad_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_faad_set_format);
  base_class->parse = GST_DEBUG_FUNCPTR (gst_faad_parse);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_faad_handle_frame);
  base_class->flush = GST_DEBUG_FUNCPTR (gst_faad_flush);
}

static void
gst_faad_init (GstFaad * faad, GstFaadClass * klass)
{
  gst_faad_reset (faad);
}

static void
gst_faad_reset_stream_state (GstFaad * faad)
{
  if (faad->handle)
    faacDecPostSeekReset (faad->handle, 0);
}

static void
gst_faad_reset (GstFaad * faad)
{
  faad->samplerate = -1;
  faad->channels = -1;
  faad->init = FALSE;
  faad->packetised = FALSE;
  g_free (faad->channel_positions);
  faad->channel_positions = NULL;
  faad->last_header = 0;

  gst_faad_reset_stream_state (faad);
}

static gboolean
gst_faad_start (GstAudioDecoder * dec)
{
  GstFaad *faad = GST_FAAD (dec);

  GST_DEBUG_OBJECT (dec, "start");
  gst_faad_reset (faad);

  /* call upon legacy upstream byte support (e.g. seeking) */
  gst_audio_decoder_set_byte_time (dec, TRUE);
  /* never mind a few errors */
  gst_audio_decoder_set_max_errors (dec, 10);

  return TRUE;
}

static gboolean
gst_faad_stop (GstAudioDecoder * dec)
{
  GstFaad *faad = GST_FAAD (dec);

  GST_DEBUG_OBJECT (dec, "stop");
  gst_faad_reset (faad);
  gst_faad_close_decoder (faad);

  return TRUE;
}

static gint
aac_rate_idx (gint rate)
{
  if (92017 <= rate)
    return 0;
  else if (75132 <= rate)
    return 1;
  else if (55426 <= rate)
    return 2;
  else if (46009 <= rate)
    return 3;
  else if (37566 <= rate)
    return 4;
  else if (27713 <= rate)
    return 5;
  else if (23004 <= rate)
    return 6;
  else if (18783 <= rate)
    return 7;
  else if (13856 <= rate)
    return 8;
  else if (11502 <= rate)
    return 9;
  else if (9391 <= rate)
    return 10;
  else
    return 11;
}

static gboolean
gst_faad_set_format (GstAudioDecoder * dec, GstCaps * caps)
{
  GstFaad *faad = GST_FAAD (dec);
  GstStructure *str = gst_caps_get_structure (caps, 0);
  GstBuffer *buf;
  const GValue *value;

  /* clean up current decoder, rather than trying to reconfigure */
  gst_faad_close_decoder (faad);

  /* Assume raw stream */
  faad->packetised = FALSE;

  if ((value = gst_structure_get_value (str, "codec_data"))) {
#if FAAD2_MINOR_VERSION >= 7
    unsigned long samplerate;
#else
    guint32 samplerate;
#endif
    guint8 channels;
    guint8 *cdata;
    guint csize;

    /* We have codec data, means packetised stream */
    faad->packetised = TRUE;

    buf = gst_value_get_buffer (value);
    g_return_val_if_fail (buf != NULL, FALSE);

    cdata = GST_BUFFER_DATA (buf);
    csize = GST_BUFFER_SIZE (buf);

    if (csize < 2)
      goto wrong_length;

    GST_DEBUG_OBJECT (faad,
        "codec_data: object_type=%d, sample_rate=%d, channels=%d",
        ((cdata[0] & 0xf8) >> 3),
        (((cdata[0] & 0x07) << 1) | ((cdata[1] & 0x80) >> 7)),
        ((cdata[1] & 0x78) >> 3));

    if (!gst_faad_open_decoder (faad))
      goto open_failed;
    /* someone forgot that char can be unsigned when writing the API */
    if ((gint8) faacDecInit2 (faad->handle, cdata, csize, &samplerate,
            &channels) < 0)
      goto init_failed;

    if (channels != ((cdata[1] & 0x78) >> 3)) {
      /* https://bugs.launchpad.net/ubuntu/+source/faad2/+bug/290259 */
      GST_WARNING_OBJECT (faad,
          "buggy faad version, wrong nr of channels %d instead of %d", channels,
          ((cdata[1] & 0x78) >> 3));
    }

    GST_DEBUG_OBJECT (faad, "codec_data init: channels=%u, rate=%u", channels,
        (guint32) samplerate);

    /* not updating these here, so they are updated in the
     * chain function, and new caps are created etc. */
    faad->samplerate = 0;
    faad->channels = 0;

    faad->init = TRUE;
  } else if ((value = gst_structure_get_value (str, "framed")) &&
      g_value_get_boolean (value) == TRUE) {
    faad->packetised = TRUE;
    faad->init = FALSE;
    GST_DEBUG_OBJECT (faad, "we have packetized audio");
  } else {
    faad->init = FALSE;
  }

  faad->fake_codec_data[0] = 0;
  faad->fake_codec_data[1] = 0;

  if (faad->packetised && !faad->init) {
    gint rate, channels;

    if (gst_structure_get_int (str, "rate", &rate) &&
        gst_structure_get_int (str, "channels", &channels)) {
      gint rate_idx, profile;

      profile = 3;              /* 0=MAIN, 1=LC, 2=SSR, 3=LTP */
      rate_idx = aac_rate_idx (rate);

      faad->fake_codec_data[0] = ((profile + 1) << 3) | ((rate_idx & 0xE) >> 1);
      faad->fake_codec_data[1] = ((rate_idx & 0x1) << 7) | (channels << 3);
      GST_LOG_OBJECT (faad, "created fake codec data (%u,%u): 0x%x 0x%x", rate,
          channels, (int) faad->fake_codec_data[0],
          (int) faad->fake_codec_data[1]);
    }
  }

  return TRUE;

  /* ERRORS */
wrong_length:
  {
    GST_DEBUG_OBJECT (faad, "codec_data less than 2 bytes long");
    gst_object_unref (faad);
    return FALSE;
  }
open_failed:
  {
    GST_DEBUG_OBJECT (faad, "failed to create decoder");
    gst_object_unref (faad);
    return FALSE;
  }
init_failed:
  {
    GST_DEBUG_OBJECT (faad, "faacDecInit2() failed");
    gst_object_unref (faad);
    return FALSE;
  }
}

static GstAudioChannelPosition *
gst_faad_chanpos_to_gst (GstFaad * faad, guchar * fpos, guint num,
    gboolean * channel_map_failed)
{
  GstAudioChannelPosition *pos;
  guint n;
  gboolean unknown_channel = FALSE;

  *channel_map_failed = FALSE;

  /* special handling for the common cases for mono and stereo */
  if (num == 1 && fpos[0] == FRONT_CHANNEL_CENTER) {
    GST_DEBUG_OBJECT (faad, "mono common case; won't set channel positions");
    return NULL;
  } else if (num == 2 && fpos[0] == FRONT_CHANNEL_LEFT
      && fpos[1] == FRONT_CHANNEL_RIGHT) {
    GST_DEBUG_OBJECT (faad, "stereo common case; won't set channel positions");
    return NULL;
  }

  pos = g_new (GstAudioChannelPosition, num);
  for (n = 0; n < num; n++) {
    GST_DEBUG_OBJECT (faad, "faad channel %d as %d", n, fpos[n]);
    switch (fpos[n]) {
      case FRONT_CHANNEL_LEFT:
        pos[n] = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
        break;
      case FRONT_CHANNEL_RIGHT:
        pos[n] = GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
        break;
      case FRONT_CHANNEL_CENTER:
        /* argh, mono = center */
        if (num == 1)
          pos[n] = GST_AUDIO_CHANNEL_POSITION_MONO;
        else
          pos[n] = GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER;
        break;
      case SIDE_CHANNEL_LEFT:
        pos[n] = GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT;
        break;
      case SIDE_CHANNEL_RIGHT:
        pos[n] = GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT;
        break;
      case BACK_CHANNEL_LEFT:
        pos[n] = GST_AUDIO_CHANNEL_POSITION_REAR_LEFT;
        break;
      case BACK_CHANNEL_RIGHT:
        pos[n] = GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT;
        break;
      case BACK_CHANNEL_CENTER:
        pos[n] = GST_AUDIO_CHANNEL_POSITION_REAR_CENTER;
        break;
      case LFE_CHANNEL:
        pos[n] = GST_AUDIO_CHANNEL_POSITION_LFE1;
        break;
      default:
        GST_DEBUG_OBJECT (faad, "unknown channel %d at %d", fpos[n], n);
        unknown_channel = TRUE;
        break;
    }
  }
  if (unknown_channel) {
    g_free (pos);
    pos = NULL;
    switch (num) {
      case 1:{
        GST_DEBUG_OBJECT (faad,
            "FAAD reports unknown 1 channel mapping. Forcing to mono");
        break;
      }
      case 2:{
        GST_DEBUG_OBJECT (faad,
            "FAAD reports unknown 2 channel mapping. Forcing to stereo");
        break;
      }
      default:{
        GST_WARNING_OBJECT (faad,
            "Unsupported FAAD channel position 0x%x encountered", fpos[n]);
        *channel_map_failed = TRUE;
        break;
      }
    }
  }

  return pos;
}

static gboolean
gst_faad_update_caps (GstFaad * faad, faacDecFrameInfo * info)
{
  GstAudioChannelPosition *pos;
  gboolean ret;
  gboolean channel_map_failed;
  GstCaps *caps;
  gboolean fmt_change = FALSE;

  /* see if we need to renegotiate */
  if (info->samplerate != faad->samplerate ||
      info->channels != faad->channels || !faad->channel_positions) {
    fmt_change = TRUE;
  } else {
    gint i;

    for (i = 0; i < info->channels; i++) {
      if (info->channel_position[i] != faad->channel_positions[i])
        fmt_change = TRUE;
    }
  }

  if (G_LIKELY (!fmt_change))
    return TRUE;

  /* store new negotiation information */
  faad->samplerate = info->samplerate;
  faad->channels = info->channels;
  g_free (faad->channel_positions);
  faad->channel_positions = g_memdup (info->channel_position, faad->channels);

  caps = gst_caps_new_simple ("audio/x-raw-int",
      "endianness", G_TYPE_INT, G_BYTE_ORDER,
      "signed", G_TYPE_BOOLEAN, TRUE,
      "width", G_TYPE_INT, 16,
      "depth", G_TYPE_INT, 16,
      "rate", G_TYPE_INT, faad->samplerate,
      "channels", G_TYPE_INT, faad->channels, NULL);

  faad->bps = 16 / 8;

  channel_map_failed = FALSE;
  pos =
      gst_faad_chanpos_to_gst (faad, faad->channel_positions, faad->channels,
      &channel_map_failed);
  if (channel_map_failed) {
    GST_DEBUG_OBJECT (faad, "Could not map channel positions");
    gst_caps_unref (caps);
    return FALSE;
  }
  if (pos) {
    gst_audio_set_channel_positions (gst_caps_get_structure (caps, 0), pos);
    g_free (pos);
  }

  GST_DEBUG_OBJECT (faad, "New output caps: %" GST_PTR_FORMAT, caps);

  ret = gst_pad_set_caps (GST_AUDIO_DECODER_SRC_PAD (faad), caps);
  gst_caps_unref (caps);

  return ret;
}

/*
 * Find syncpoint in ADTS/ADIF stream. Doesn't work for raw,
 * packetized streams. Be careful when calling.
 * Returns FALSE on no-sync, fills offset/length if one/two
 * syncpoints are found, only returns TRUE when it finds two
 * subsequent syncpoints (similar to mp3 typefinding in
 * gst/typefind/) for ADTS because 12 bits isn't very reliable.
 */
static gboolean
gst_faad_sync (GstFaad * faad, const guint8 * data, guint size, gboolean next,
    gint * off, gint * length)
{
  guint n = 0;
  gint snc;
  gboolean ret = FALSE;
  guint len = 0;

  GST_LOG_OBJECT (faad, "Finding syncpoint");

  /* check for too small a buffer */
  if (size < 3)
    goto exit;

  for (n = 0; n < size - 3; n++) {
    snc = GST_READ_UINT16_BE (&data[n]);
    if ((snc & 0xfff6) == 0xfff0) {
      /* we have an ADTS syncpoint. Parse length and find
       * next syncpoint. */
      GST_LOG_OBJECT (faad,
          "Found one ADTS syncpoint at offset 0x%x, tracing next...", n);

      if (size - n < 5) {
        GST_LOG_OBJECT (faad, "Not enough data to parse ADTS header");
        break;
      }

      len = ((data[n + 3] & 0x03) << 11) |
          (data[n + 4] << 3) | ((data[n + 5] & 0xe0) >> 5);
      if (n + len + 2 >= size) {
        GST_LOG_OBJECT (faad, "Frame size %d, next frame is not within reach",
            len);
        if (next) {
          break;
        } else if (n + len <= size) {
          GST_LOG_OBJECT (faad, "but have complete frame and no next frame; "
              "accept ADTS syncpoint at offset 0x%x (framelen %u)", n, len);
          ret = TRUE;
          break;
        }
      }

      snc = GST_READ_UINT16_BE (&data[n + len]);
      if ((snc & 0xfff6) == 0xfff0) {
        GST_LOG_OBJECT (faad,
            "Found ADTS syncpoint at offset 0x%x (framelen %u)", n, len);
        ret = TRUE;
        break;
      }

      GST_LOG_OBJECT (faad, "No next frame found... (should be at 0x%x)",
          n + len);
    } else if (!memcmp (&data[n], "ADIF", 4)) {
      /* we have an ADIF syncpoint. 4 bytes is enough. */
      GST_LOG_OBJECT (faad, "Found ADIF syncpoint at offset 0x%x", n);
      ret = TRUE;
      break;
    }
  }

exit:

  *off = n;

  if (ret) {
    *length = len;
  } else {
    GST_LOG_OBJECT (faad, "Found no syncpoint");
  }

  return ret;
}

static gboolean
looks_like_valid_header (guint8 * input_data, guint input_size)
{
  if (input_size < 4)
    return FALSE;

  if (input_data[0] == 'A'
      && input_data[1] == 'D' && input_data[2] == 'I' && input_data[3] == 'F')
    /* ADIF type header */
    return TRUE;

  if (input_data[0] == 0xff && (input_data[1] >> 4) == 0xf)
    /* ADTS type header */
    return TRUE;

  return FALSE;
}

static GstFlowReturn
gst_faad_parse (GstAudioDecoder * dec, GstAdapter * adapter,
    gint * offset, gint * length)
{
  GstFaad *faad;
  const guint8 *data;
  guint size;
  gboolean sync, eos;

  faad = GST_FAAD (dec);

  size = gst_adapter_available (adapter);
  g_return_val_if_fail (size > 0, GST_FLOW_ERROR);

  gst_audio_decoder_get_parse_state (dec, &sync, &eos);

  if (faad->packetised) {
    *offset = 0;
    *length = size;
    return GST_FLOW_OK;
  } else {
    data = gst_adapter_peek (adapter, size);
    return gst_faad_sync (faad, data, size, !eos, offset, length) ?
        GST_FLOW_OK : GST_FLOW_UNEXPECTED;
  }
}

static GstFlowReturn
gst_faad_handle_frame (GstAudioDecoder * dec, GstBuffer * buffer)
{
  GstFaad *faad;
  GstFlowReturn ret = GST_FLOW_OK;
  guint input_size;
  guchar *input_data;
  GstBuffer *outbuf;
  faacDecFrameInfo info;
  void *out;

  faad = GST_FAAD (dec);

  /* no fancy draining */
  if (G_UNLIKELY (!buffer))
    return GST_FLOW_OK;

  input_data = GST_BUFFER_DATA (buffer);
  input_size = GST_BUFFER_SIZE (buffer);

init:
  /* init if not already done during capsnego */
  if (!faad->init) {
#if FAAD2_MINOR_VERSION >= 7
    unsigned long rate;
#else
    guint32 rate;
#endif
    guint8 ch;

    GST_DEBUG_OBJECT (faad, "initialising ...");
    if (!gst_faad_open_decoder (faad))
      goto open_failed;
    /* We check if the first data looks like it might plausibly contain
     * appropriate initialisation info... if not, we use our fake_codec_data
     */
    if (looks_like_valid_header (input_data, input_size) || !faad->packetised) {
      if (faacDecInit (faad->handle, input_data, input_size, &rate, &ch) < 0)
        goto init_failed;

      GST_DEBUG_OBJECT (faad, "faacDecInit() ok: rate=%u,channels=%u",
          (guint32) rate, ch);
    } else {
      if ((gint8) faacDecInit2 (faad->handle, faad->fake_codec_data, 2,
              &rate, &ch) < 0) {
        goto init2_failed;
      }
      GST_DEBUG_OBJECT (faad, "faacDecInit2() ok: rate=%u,channels=%u",
          (guint32) rate, ch);
    }

    faad->init = TRUE;

    /* make sure we create new caps below */
    faad->samplerate = 0;
    faad->channels = 0;
  }

  /* decode cycle */
  info.error = 0;

  do {

    if (!faad->packetised) {
      /* faad only really parses ADTS header at Init time, not when decoding,
       * so monitor for changes and kick faad when needed */
      if (GST_READ_UINT32_BE (input_data) >> 4 != faad->last_header >> 4) {
        GST_DEBUG_OBJECT (faad, "ADTS header changed, forcing Init");
        faad->last_header = GST_READ_UINT32_BE (input_data);
        /* kick hard */
        gst_faad_close_decoder (faad);
        faad->init = FALSE;
        goto init;
      }
    }

    out = faacDecDecode (faad->handle, &info, input_data, input_size);

    if (info.error > 0) {
      /* give up on frame and bail out */
      gst_audio_decoder_finish_frame (dec, NULL, 1);
      goto decode_failed;
    }

    GST_LOG_OBJECT (faad, "%d bytes consumed, %d samples decoded",
        (guint) info.bytesconsumed, (guint) info.samples);

    if (out && info.samples > 0) {
      if (!gst_faad_update_caps (faad, &info))
        goto negotiation_failed;

      /* C's lovely propensity for int overflow.. */
      if (info.samples > G_MAXUINT / faad->bps)
        goto sample_overflow;

      /* note: info.samples is total samples, not per channel */
      ret =
          gst_pad_alloc_buffer_and_set_caps (GST_AUDIO_DECODER_SRC_PAD
          (faad), 0, info.samples * faad->bps,
          GST_PAD_CAPS (GST_AUDIO_DECODER_SRC_PAD (faad)), &outbuf);
      if (ret != GST_FLOW_OK)
        goto out;

      memcpy (GST_BUFFER_DATA (outbuf), out, GST_BUFFER_SIZE (outbuf));

      ret = gst_audio_decoder_finish_frame (dec, outbuf, 1);
    }
  } while (FALSE);

out:
  return ret;

/* ERRORS */
open_failed:
  {
    GST_ELEMENT_ERROR (faad, STREAM, DECODE, (NULL),
        ("Failed to open decoder"));
    ret = GST_FLOW_ERROR;
    goto out;
  }
init_failed:
  {
    GST_ELEMENT_ERROR (faad, STREAM, DECODE, (NULL),
        ("Failed to init decoder from stream"));
    ret = GST_FLOW_ERROR;
    goto out;
  }
init2_failed:
  {
    GST_ELEMENT_ERROR (faad, STREAM, DECODE, (NULL),
        ("%s() failed", (faad->handle) ? "faacDecInit2" : "faacDecOpen"));
    ret = GST_FLOW_ERROR;
    goto out;
  }
decode_failed:
  {
    GST_AUDIO_DECODER_ERROR (faad, 1, STREAM, DECODE, (NULL),
        ("decoding error: %s", faacDecGetErrorMessage (info.error)), ret);
    goto out;
  }
negotiation_failed:
  {
    GST_ELEMENT_ERROR (faad, CORE, NEGOTIATION, (NULL),
        ("Setting caps on source pad failed"));
    ret = GST_FLOW_ERROR;
    goto out;
  }
sample_overflow:
  {
    GST_ELEMENT_ERROR (faad, STREAM, DECODE, (NULL),
        ("Output buffer too large"));
    ret = GST_FLOW_ERROR;
    goto out;
  }
}

static void
gst_faad_flush (GstAudioDecoder * dec, gboolean hard)
{
  gst_faad_reset_stream_state (GST_FAAD (dec));
}

static gboolean
gst_faad_open_decoder (GstFaad * faad)
{
  faacDecConfiguration *conf;

  faad->handle = faacDecOpen ();

  if (faad->handle == NULL) {
    GST_WARNING_OBJECT (faad, "faacDecOpen() failed");
    return FALSE;
  }

  conf = faacDecGetCurrentConfiguration (faad->handle);
  conf->defObjectType = LC;
  conf->dontUpSampleImplicitSBR = 1;
  conf->outputFormat = FAAD_FMT_16BIT;

  if (faacDecSetConfiguration (faad->handle, conf) == 0) {
    GST_WARNING_OBJECT (faad, "faacDecSetConfiguration() failed");
    return FALSE;
  }

  return TRUE;
}

static void
gst_faad_close_decoder (GstFaad * faad)
{
  if (faad->handle) {
    faacDecClose (faad->handle);
    faad->handle = NULL;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "faad", GST_RANK_PRIMARY, GST_TYPE_FAAD);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "faad",
    "Free AAC Decoder (FAAD)",
    plugin_init, VERSION, "GPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
