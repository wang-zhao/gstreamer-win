 /*
  * This library is licensed under 2 different licenses and you
  * can choose to use it under the terms of either one of them. The
  * two licenses are the MPL 1.1 and the LGPL.
  *
  * MPL:
  *
  * The contents of this file are subject to the Mozilla Public License
  * Version 1.1 (the "License"); you may not use this file except in
  * compliance with the License. You may obtain a copy of the License at
  * http://www.mozilla.org/MPL/.
  *
  * Software distributed under the License is distributed on an "AS IS"
  * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  * License for the specific language governing rights and limitations
  * under the License.
  *
  * LGPL:
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
  *
  * The Original Code is Fluendo MPEG Demuxer plugin.
  *
  * The Initial Developer of the Original Code is Fluendo, S.L.
  * Portions created by Fluendo, S.L. are Copyright (C) 2005
  * Fluendo, S.L. All Rights Reserved.
  *
  * Contributor(s): Wim Taymans <wim@fluendo.com>
  *                 Jan Schmidt <thaytan@noraisin.net>
  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstmpegdefs.h"
#include "gstmpegdemux.h"

#define MAX_DVD_AUDIO_STREAMS       8
#define MAX_DVD_SUBPICTURE_STREAMS  32
#define BLOCK_SZ                    32768
#define SCAN_SCR_SZ                 12
#define SCAN_PTS_SZ                 80

#define SEGMENT_THRESHOLD (300*GST_MSECOND)
#define VIDEO_SEGMENT_THRESHOLD (500*GST_MSECOND)

typedef enum
{
  SCAN_SCR,
  SCAN_DTS,
  SCAN_PTS
} SCAN_MODE;

/* We clamp scr delta with 0 so negative bytes won't be possible */
#define GSTTIME_TO_BYTES(time) \
  ((time != -1) ? gst_util_uint64_scale (MAX(0,(gint64) (GSTTIME_TO_MPEGTIME(time))), demux->scr_rate_n, demux->scr_rate_d) : -1)
#define BYTES_TO_GSTTIME(bytes) ((bytes != -1) ? MPEGTIME_TO_GSTTIME(gst_util_uint64_scale (bytes, demux->scr_rate_d, demux->scr_rate_n)) : -1)

#define ADAPTER_OFFSET_FLUSH(_bytes_) demux->adapter_offset += (_bytes_)

GST_DEBUG_CATEGORY_STATIC (gstflupsdemux_debug);
#define GST_CAT_DEFAULT (gstflupsdemux_debug)

/* MPEG2Demux signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_SYNC,
  /* FILL ME */
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpeg, "
        "mpegversion = (int) { 1, 2 }, "
        "systemstream = (boolean) TRUE;" "video/x-cdxa")
    );

static GstStaticPadTemplate video_template =
    GST_STATIC_PAD_TEMPLATE ("video_%02x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("video/mpeg, "
        "mpegversion = (int) { 1, 2, 4 }, " "systemstream = (boolean) FALSE;"
        "video/x-h264")
    );

static GstStaticPadTemplate audio_template =
    GST_STATIC_PAD_TEMPLATE ("audio_%02x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("audio/mpeg, mpegversion = (int) 1;"
        "audio/mpeg, mpegversion = (int) 4, stream-format = (string) { adts, loas };"
        "audio/x-private1-lpcm; "
        "audio/x-private1-ac3;" "audio/x-private1-dts;" "audio/ac3")
    );

static GstStaticPadTemplate subpicture_template =
GST_STATIC_PAD_TEMPLATE ("subpicture_%02x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("video/x-dvd-subpicture")
    );

static GstStaticPadTemplate private_template =
GST_STATIC_PAD_TEMPLATE ("private_%d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static void gst_flups_demux_base_init (GstFluPSDemuxClass * klass);
static void gst_flups_demux_class_init (GstFluPSDemuxClass * klass);
static void gst_flups_demux_init (GstFluPSDemux * demux);
static void gst_flups_demux_finalize (GstFluPSDemux * demux);
static void gst_flups_demux_reset (GstFluPSDemux * demux);

static gboolean gst_flups_demux_sink_event (GstPad * pad, GstEvent * event);
static GstFlowReturn gst_flups_demux_chain (GstPad * pad, GstBuffer * buffer);
static gboolean gst_flups_demux_sink_activate (GstPad * sinkpad);
static gboolean gst_flups_demux_sink_activate_push (GstPad * sinkpad,
    gboolean active);
static gboolean gst_flups_demux_sink_activate_pull (GstPad * sinkpad,
    gboolean active);
static void gst_flups_demux_loop (GstPad * pad);

static gboolean gst_flups_demux_src_event (GstPad * pad, GstEvent * event);
static gboolean gst_flups_demux_src_query (GstPad * pad, GstQuery * query);
static const GstQueryType *gst_flups_demux_src_query_type (GstPad * pad);

static GstStateChangeReturn gst_flups_demux_change_state (GstElement * element,
    GstStateChange transition);

static inline gboolean gst_flups_demux_scan_forward_ts (GstFluPSDemux * demux,
    guint64 * pos, SCAN_MODE mode, guint64 * rts);
static inline gboolean gst_flups_demux_scan_backward_ts (GstFluPSDemux * demux,
    guint64 * pos, SCAN_MODE mode, guint64 * rts);

static inline void gst_flups_demux_send_segment_updates (GstFluPSDemux * demux,
    GstClockTime new_time);
static inline void gst_flups_demux_clear_times (GstFluPSDemux * demux);

static GstElementClass *parent_class = NULL;

/*static guint gst_flups_demux_signals[LAST_SIGNAL] = { 0 };*/

GType
gst_flups_demux_get_type (void)
{
  static GType flups_demux_type = 0;

  if (!flups_demux_type) {
    static const GTypeInfo flups_demux_info = {
      sizeof (GstFluPSDemuxClass),
      (GBaseInitFunc) gst_flups_demux_base_init,
      NULL,
      (GClassInitFunc) gst_flups_demux_class_init,
      NULL,
      NULL,
      sizeof (GstFluPSDemux),
      0,
      (GInstanceInitFunc) gst_flups_demux_init,
    };

    flups_demux_type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstMpegPSDemux",
        &flups_demux_info, 0);

    GST_DEBUG_CATEGORY_INIT (gstflupsdemux_debug, "mpegpsdemux", 0,
        "MPEG program stream demultiplexer element");
  }

  return flups_demux_type;
}

static void
gst_flups_demux_base_init (GstFluPSDemuxClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  klass->sink_template = gst_static_pad_template_get (&sink_template);
  klass->video_template = gst_static_pad_template_get (&video_template);
  klass->audio_template = gst_static_pad_template_get (&audio_template);
  klass->subpicture_template =
      gst_static_pad_template_get (&subpicture_template);
  klass->private_template = gst_static_pad_template_get (&private_template);

  gst_element_class_add_pad_template (element_class, klass->video_template);
  gst_element_class_add_pad_template (element_class, klass->audio_template);
  gst_element_class_add_pad_template (element_class,
      klass->subpicture_template);
  gst_element_class_add_pad_template (element_class, klass->private_template);
  gst_element_class_add_pad_template (element_class, klass->sink_template);

  gst_element_class_set_details_simple (element_class,
      "The Fluendo MPEG Program Stream Demuxer", "Codec/Demuxer",
      "Demultiplexes MPEG Program Streams", "Wim Taymans <wim@fluendo.com>");
}

static void
gst_flups_demux_class_init (GstFluPSDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->finalize = (GObjectFinalizeFunc) gst_flups_demux_finalize;

  gstelement_class->change_state = gst_flups_demux_change_state;
}

static void
gst_flups_demux_init (GstFluPSDemux * demux)
{
  GstFluPSDemuxClass *klass = GST_FLUPS_DEMUX_GET_CLASS (demux);

  demux->sinkpad = gst_pad_new_from_template (klass->sink_template, "sink");
  gst_pad_set_event_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_sink_event));
  gst_pad_set_chain_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_chain));
  gst_pad_set_activate_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_sink_activate));
  gst_pad_set_activatepull_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_sink_activate_pull));
  gst_pad_set_activatepush_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_sink_activate_push));

  gst_element_add_pad (GST_ELEMENT (demux), demux->sinkpad);

  demux->streams =
      g_malloc0 (sizeof (GstFluPSStream *) * (GST_FLUPS_DEMUX_MAX_STREAMS));
  demux->streams_found =
      g_malloc0 (sizeof (GstFluPSStream *) * (GST_FLUPS_DEMUX_MAX_STREAMS));
  demux->found_count = 0;

}

static void
gst_flups_demux_finalize (GstFluPSDemux * demux)
{
  gst_flups_demux_reset (demux);
  g_free (demux->streams);
  g_free (demux->streams_found);

  G_OBJECT_CLASS (parent_class)->finalize (G_OBJECT (demux));
}

static void
gst_flups_demux_reset (GstFluPSDemux * demux)
{
  /* Clean up the streams and pads we allocated */
  gint i;
  GstEvent **p_ev;

  for (i = 0; i < GST_FLUPS_DEMUX_MAX_STREAMS; i++) {
    GstFluPSStream *stream = demux->streams[i];

    if (stream != NULL) {
      if (stream->pad)
        gst_element_remove_pad (GST_ELEMENT_CAST (demux), stream->pad);

      g_free (stream);
      demux->streams[i] = NULL;
    }
  }
  memset (demux->streams_found, 0,
      sizeof (GstFluPSStream *) * (GST_FLUPS_DEMUX_MAX_STREAMS));
  demux->found_count = 0;
  p_ev = &demux->lang_codes;

  gst_event_replace (p_ev, NULL);
}

static GstFluPSStream *
gst_flups_demux_create_stream (GstFluPSDemux * demux, gint id, gint stream_type)
{
  GstFluPSStream *stream;
  GstPadTemplate *template;
  gchar *name;
  GstFluPSDemuxClass *klass = GST_FLUPS_DEMUX_GET_CLASS (demux);
  GstCaps *caps;
  GstClockTime threshold = SEGMENT_THRESHOLD;

  name = NULL;
  template = NULL;
  caps = NULL;

  GST_DEBUG_OBJECT (demux, "create stream id 0x%02x, type 0x%02x", id,
      stream_type);

  switch (stream_type) {
    case ST_VIDEO_MPEG1:
    case ST_VIDEO_MPEG2:
    case ST_VIDEO_MPEG4:
    case ST_GST_VIDEO_MPEG1_OR_2:
    {
      gint mpeg_version = 1;
      if (stream_type == ST_VIDEO_MPEG2 ||
          (stream_type == ST_GST_VIDEO_MPEG1_OR_2 && demux->is_mpeg2_pack)) {
        mpeg_version = 2;
      }
      if (stream_type == ST_VIDEO_MPEG4) {
        mpeg_version = 4;
      }

      template = klass->video_template;
      name = g_strdup_printf ("video_%02x", id);
      caps = gst_caps_new_simple ("video/mpeg",
          "mpegversion", G_TYPE_INT, mpeg_version,
          "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
      threshold = VIDEO_SEGMENT_THRESHOLD;
      break;
    }
    case ST_AUDIO_MPEG1:
    case ST_AUDIO_MPEG2:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 1, NULL);
      break;
    case ST_PRIVATE_SECTIONS:
    case ST_PRIVATE_DATA:
    case ST_MHEG:
    case ST_DSMCC:
      break;
    case ST_AUDIO_AAC_ADTS:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "stream-format", G_TYPE_STRING, "adts", NULL);
      break;
    case ST_AUDIO_AAC_LOAS:    // LATM/LOAS AAC syntax
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "stream-format", G_TYPE_STRING, "loas", NULL);
      break;
    case ST_VIDEO_H264:
      template = klass->video_template;
      name = g_strdup_printf ("video_%02x", id);
      caps = gst_caps_new_simple ("video/x-h264", NULL);
      threshold = VIDEO_SEGMENT_THRESHOLD;
      break;
    case ST_PS_AUDIO_AC3:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_simple ("audio/x-private1-ac3", NULL);
      break;
    case ST_PS_AUDIO_DTS:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_simple ("audio/x-private1-dts", NULL);
      break;
    case ST_PS_AUDIO_LPCM:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_simple ("audio/x-private1-lpcm", NULL);
      break;
    case ST_PS_DVD_SUBPICTURE:
      template = klass->subpicture_template;
      name = g_strdup_printf ("subpicture_%02x", id);
      caps = gst_caps_new_simple ("video/x-dvd-subpicture", NULL);
      break;
    case ST_GST_AUDIO_RAWA52:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_simple ("audio/ac3", NULL);
      break;
    default:
      break;
  }

  if (name == NULL || template == NULL || caps == NULL) {
    if (name)
      g_free (name);
    if (caps)
      gst_caps_unref (caps);
    return FALSE;
  }

  stream = g_new0 (GstFluPSStream, 1);
  stream->id = id;
  stream->discont = TRUE;
  stream->need_segment = TRUE;
  stream->notlinked = FALSE;
  stream->type = stream_type;
  stream->pad = gst_pad_new_from_template (template, name);
  stream->segment_thresh = threshold;
  gst_pad_set_event_function (stream->pad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_src_event));
  gst_pad_set_query_function (stream->pad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_src_query));
  gst_pad_set_query_type_function (stream->pad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_src_query_type));
  gst_pad_use_fixed_caps (stream->pad);
  gst_pad_set_caps (stream->pad, caps);
  gst_caps_unref (caps);
  GST_DEBUG_OBJECT (demux, "create pad %s, caps %" GST_PTR_FORMAT, name, caps);
  g_free (name);


  return stream;
}

static GstFluPSStream *
gst_flups_demux_get_stream (GstFluPSDemux * demux, gint id, gint type)
{
  GstFluPSStream *stream = demux->streams[id];

  if (stream == NULL) {
    if (!(stream = gst_flups_demux_create_stream (demux, id, type)))
      goto unknown_stream;

    GST_DEBUG_OBJECT (demux, "adding pad for stream id 0x%02x type 0x%02x", id,
        type);

    gst_pad_set_active (stream->pad, TRUE);
    gst_element_add_pad (GST_ELEMENT (demux), stream->pad);

    demux->streams[id] = stream;
    demux->streams_found[demux->found_count++] = stream;
  }
  return stream;

  /* ERROR */
unknown_stream:
  {
    GST_DEBUG_OBJECT (demux, "unknown stream id 0x%02x type 0x%02x", id, type);
    return NULL;
  }
}

static GstFlowReturn
gst_flups_demux_send_data (GstFluPSDemux * demux, GstFluPSStream * stream,
    GstBuffer * buf)
{
  GstFlowReturn result;
  guint64 timestamp;
  guint size;

  if (stream == NULL)
    goto no_stream;

  /* timestamps */
  if (G_UNLIKELY (demux->next_pts != G_MAXUINT64))
    timestamp = MPEGTIME_TO_GSTTIME (demux->next_pts);
  else
    timestamp = GST_CLOCK_TIME_NONE;

  /* discont */
  if (G_UNLIKELY (stream->need_segment)) {
    gint64 time, start, stop;
    GstEvent *newsegment;

    GST_DEBUG ("timestamp:%" GST_TIME_FORMAT " base_time %" GST_TIME_FORMAT
        " src_segment.start:%" GST_TIME_FORMAT " .stop:%" GST_TIME_FORMAT,
        GST_TIME_ARGS (timestamp), GST_TIME_ARGS (demux->base_time),
        GST_TIME_ARGS (demux->src_segment.start),
        GST_TIME_ARGS (demux->src_segment.stop));

    if (GST_CLOCK_TIME_IS_VALID (demux->base_time) &&
        GST_CLOCK_TIME_IS_VALID (demux->src_segment.start))
      start = demux->base_time + demux->src_segment.start;
    else
      start = 0;

    if (GST_CLOCK_TIME_IS_VALID (demux->src_segment.stop) &&
        GST_CLOCK_TIME_IS_VALID (demux->base_time))
      stop = demux->base_time + demux->src_segment.stop;
    else
      stop = -1;

    if (timestamp != GST_CLOCK_TIME_NONE) {
      if (demux->src_segment.rate > 0) {
        if (GST_CLOCK_DIFF (start, timestamp) > GST_SECOND)
          start = timestamp;
      } else {
        if (GST_CLOCK_DIFF (stop, timestamp) > GST_SECOND)
          stop = timestamp;
      }
    }
    if (GST_CLOCK_TIME_IS_VALID (demux->base_time) && start > demux->base_time)
      time = start - demux->base_time;
    else
      time = 0;

    GST_INFO_OBJECT (demux, "sending new segment: rate %g applied_rate %g "
        "start: %" GST_TIME_FORMAT ", stop: %" GST_TIME_FORMAT
        ", time: %" GST_TIME_FORMAT " to pad %" GST_PTR_FORMAT,
        demux->sink_segment.rate, demux->sink_segment.applied_rate,
        GST_TIME_ARGS (start), GST_TIME_ARGS (stop),
        GST_TIME_ARGS (time), stream->pad);

    newsegment = gst_event_new_new_segment_full (FALSE,
        demux->sink_segment.rate, demux->sink_segment.applied_rate,
        GST_FORMAT_TIME, start, stop, time);

    gst_pad_push_event (stream->pad, newsegment);

    stream->need_segment = FALSE;
  }

  /* OK, sent new segment now prepare the buffer for sending */
  /* caps */
  gst_buffer_set_caps (buf, GST_PAD_CAPS (stream->pad));
  GST_BUFFER_TIMESTAMP (buf) = timestamp;

  /* update position in the segment */
  gst_segment_set_last_stop (&demux->src_segment, GST_FORMAT_TIME,
      MPEGTIME_TO_GSTTIME (demux->current_scr - demux->first_scr));
  GST_LOG_OBJECT (demux, "last stop position is now %" GST_TIME_FORMAT
      " current scr is %" GST_TIME_FORMAT,
      GST_TIME_ARGS (demux->src_segment.last_stop),
      GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->current_scr)));

  if (demux->src_segment.last_stop != GST_CLOCK_TIME_NONE) {
    GstClockTime new_time = demux->base_time + demux->src_segment.last_stop;

    if (stream->last_ts == GST_CLOCK_TIME_NONE || stream->last_ts < new_time) {
#if 0
      g_print ("last_ts update on pad %s to time %" GST_TIME_FORMAT "\n",
          GST_PAD_NAME (stream->pad), GST_TIME_ARGS (cur_scr_time));
#endif
      stream->last_ts = new_time;
    }

    gst_flups_demux_send_segment_updates (demux, new_time);
  }

  /* Set the buffer discont flag, and clear discont state on the stream */
  if (stream->discont) {
    GST_DEBUG_OBJECT (demux, "marking discont buffer");
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);

    stream->discont = FALSE;
  }
  size = GST_BUFFER_SIZE (buf);

  demux->next_pts = G_MAXUINT64;
  demux->next_dts = G_MAXUINT64;

  result = gst_pad_push (stream->pad, buf);
  GST_DEBUG_OBJECT (demux, "pushed stream id 0x%02x type 0x%02x, time: %"
      GST_TIME_FORMAT ", size %d. result: %s",
      stream->id, stream->type, GST_TIME_ARGS (timestamp),
      size, gst_flow_get_name (result));

  return result;

  /* ERROR */
no_stream:
  {
    GST_DEBUG_OBJECT (demux, "no stream given");
    gst_buffer_unref (buf);
    return GST_FLOW_OK;
  }
}

static inline void
gst_flups_demux_mark_discont (GstFluPSDemux * demux, gboolean discont,
    gboolean need_segment)
{
  gint i, count = demux->found_count;

  /* mark discont on all streams */
  for (i = 0; i < count; i++) {
    GstFluPSStream *stream = demux->streams_found[i];

    if (G_LIKELY (stream)) {
      stream->discont |= discont;
      stream->need_segment |= need_segment;
      GST_DEBUG_OBJECT (demux, "marked stream as discont %d, need_segment %d",
          stream->discont, stream->need_segment);
    }
  }
}

static inline gboolean
gst_flups_demux_send_event (GstFluPSDemux * demux, GstEvent * event)
{
  gint i, count = demux->found_count;
  gboolean ret = FALSE;

  for (i = 0; i < count; i++) {
    GstFluPSStream *stream = demux->streams_found[i];

    if (stream && !stream->notlinked) {
      (void) gst_event_ref (event);

      if (!gst_pad_push_event (stream->pad, event)) {
        GST_DEBUG_OBJECT (stream, "event %s was not handled correctly",
            GST_EVENT_TYPE_NAME (event));
      } else {
        /* If at least one push returns TRUE, then we return TRUE. */
        GST_DEBUG_OBJECT (stream, "event %s was handled correctly",
            GST_EVENT_TYPE_NAME (event));
        ret = TRUE;
      }
    }
  }

  gst_event_unref (event);
  return ret;
}

static gboolean
gst_flups_demux_handle_dvd_event (GstFluPSDemux * demux, GstEvent * event)
{
  const GstStructure *structure = gst_event_get_structure (event);
  const char *type = gst_structure_get_string (structure, "event");
  gint i;
  gchar cur_stream_name[32];
  GstFluPSStream *temp G_GNUC_UNUSED;

  if (strcmp (type, "dvd-lang-codes") == 0) {
    GstEvent **p_ev;
    /* Store the language codes event on the element, then iterate over the 
     * streams it specifies and retrieve them. The stream creation code then 
     * creates the pad appropriately and sends tag events as needed */
    p_ev = &demux->lang_codes, event;
    gst_event_replace (p_ev, event);

    GST_DEBUG_OBJECT (demux, "Handling language codes event");

    /* Create a video pad to ensure have it before emit no more pads */
    (void) gst_flups_demux_get_stream (demux, 0xe0, ST_VIDEO_MPEG2);

    /* Read out the languages for audio streams and request each one that 
     * is present */
    for (i = 0; i < MAX_DVD_AUDIO_STREAMS; i++) {
      gint stream_format;
      gint stream_id;

      g_snprintf (cur_stream_name, 32, "audio-%d-format", i);
      if (!gst_structure_get_int (structure, cur_stream_name, &stream_format))
        break;

      switch (stream_format) {
        case 0x0:
          /* AC3 */
          stream_id = 0x80 + i;
          temp = gst_flups_demux_get_stream (demux, stream_id, ST_PS_AUDIO_AC3);
          break;
        case 0x2:
        case 0x3:
          /* MPEG audio without and with extension stream are
           * treated the same */
          stream_id = 0xC0 + i;
          temp = gst_flups_demux_get_stream (demux, stream_id, ST_AUDIO_MPEG1);
          break;
        case 0x4:
          /* LPCM */
          stream_id = 0xA0 + i;
          temp =
              gst_flups_demux_get_stream (demux, stream_id, ST_PS_AUDIO_LPCM);
          break;
        case 0x6:
          /* DTS */
          stream_id = 0x88 + i;
          temp = gst_flups_demux_get_stream (demux, stream_id, ST_PS_AUDIO_DTS);
          break;
        case 0x7:
          /* FIXME: What range is SDDS? */
          break;
        default:
          GST_WARNING_OBJECT (demux,
              "Unknown audio stream format in language code event: %d",
              stream_format);
          break;
      }
    }

    /* And subtitle streams */
    for (i = 0; i < MAX_DVD_SUBPICTURE_STREAMS; i++) {
      g_snprintf (cur_stream_name, 32, "subpicture-%d-format", i);

      if (gst_structure_get_string (structure, cur_stream_name) == NULL)
        break;

      /* Retrieve the subpicture stream to force pad creation */
      temp = gst_flups_demux_get_stream (demux, 0x20 + i, ST_PS_DVD_SUBPICTURE);
    }

    GST_DEBUG_OBJECT (demux, "Created all pads from Language Codes event, "
        "signalling no-more-pads");

    gst_element_no_more_pads (GST_ELEMENT (demux));
    demux->need_no_more_pads = FALSE;
  }

  gst_event_unref (event);
  return TRUE;
}

static void
gst_flups_demux_flush (GstFluPSDemux * demux)
{
  GST_DEBUG_OBJECT (demux, "flushing demuxer");
  gst_adapter_clear (demux->adapter);
  gst_adapter_clear (demux->rev_adapter);
  gst_pes_filter_drain (&demux->filter);
  gst_flups_demux_clear_times (demux);
  demux->adapter_offset = G_MAXUINT64;
  demux->current_scr = G_MAXUINT64;
  demux->bytes_since_scr = 0;
}

static inline void
gst_flups_demux_clear_times (GstFluPSDemux * demux)
{
  gint i, count = demux->found_count;

  /* Clear the last ts for all streams */
  for (i = 0; i < count; i++) {
    GstFluPSStream *stream = demux->streams_found[i];

    if (G_LIKELY (stream)) {
      stream->last_seg_start = stream->last_ts = GST_CLOCK_TIME_NONE;
    }
  }
}

static inline void
gst_flups_demux_send_segment_updates (GstFluPSDemux * demux,
    GstClockTime new_start)
{
  GstClockTime base_time, stop, time;
  gint i, count = demux->found_count;
  GstEvent *event = NULL;

  /* Advance all lagging streams by sending a segment update */
  if ((base_time = demux->base_time) == GST_CLOCK_TIME_NONE)
    base_time = 0;

  stop = demux->src_segment.stop;
  if (stop != GST_CLOCK_TIME_NONE)
    stop += base_time;

  if (new_start > stop)
    return;

  time = demux->src_segment.time;
  time += new_start - (demux->src_segment.start + base_time);

  /* FIXME: Handle reverse playback */
  for (i = 0; i < count; i++) {
    GstFluPSStream *stream = demux->streams_found[i];

    if (stream) {
      if (stream->last_ts == GST_CLOCK_TIME_NONE ||
          stream->last_ts < demux->src_segment.start + base_time)
        stream->last_ts = demux->src_segment.start + base_time;

      if (stream->last_ts + stream->segment_thresh < new_start) {
#if 0
        g_print ("Segment update to pad %s time %" GST_TIME_FORMAT " stop now %"
            GST_TIME_FORMAT "\n", GST_PAD_NAME (stream->pad),
            GST_TIME_ARGS (new_start), GST_TIME_ARGS (demux->src_segment.stop));
#endif
        GST_DEBUG_OBJECT (demux,
            "Segment update to pad %s time %" GST_TIME_FORMAT,
            GST_PAD_NAME (stream->pad), GST_TIME_ARGS (new_start));
        if (event == NULL) {
          event = gst_event_new_new_segment_full (TRUE,
              demux->src_segment.rate, demux->src_segment.applied_rate,
              GST_FORMAT_TIME, new_start, stop, time);
        }
        gst_event_ref (event);
        gst_pad_push_event (stream->pad, event);
        stream->last_seg_start = stream->last_ts = new_start;
        stream->need_segment = FALSE;
      }
    }
  }

  if (event)
    gst_event_unref (event);
}

static inline void
gst_flups_demux_close_segment (GstFluPSDemux * demux)
{
  gint i, count = demux->found_count;
  GstEvent *event = NULL;
  guint64 base_time;

  GST_INFO_OBJECT (demux, "closing running segment %" GST_SEGMENT_FORMAT,
      &demux->src_segment);

  /* FIXME: Need to send a different segment-close to each pad where the
   * last_seg_start != clock_time_none, as that indicates a sparse-stream
   * event was sent there */

  if ((base_time = demux->base_time) == (guint64) - 1)
    base_time = 0;

  /* Close the current segment for a linear playback */
  if (demux->src_segment.rate >= 0) {
    /* for forward playback, we played from start to last_stop */
    event = gst_event_new_new_segment (TRUE,
        demux->src_segment.rate, demux->src_segment.format,
        demux->src_segment.start + base_time,
        demux->src_segment.last_stop + base_time, demux->src_segment.time);
  } else {
    gint64 stop;

    if ((stop = demux->src_segment.stop) == -1)
      stop = demux->src_segment.duration;

    /* for reverse playback, we played from stop to last_stop. */
    event = gst_event_new_new_segment (TRUE,
        demux->src_segment.rate, demux->src_segment.format,
        demux->src_segment.last_stop + base_time,
        stop + base_time, demux->src_segment.last_stop);
  }

  if (event) {
    for (i = 0; i < count; i++) {
      GstFluPSStream *stream = demux->streams_found[i];

      if (stream && !stream->notlinked && !stream->need_segment) {
        (void) gst_event_ref (event);

        if (!gst_pad_push_event (stream->pad, event)) {
          GST_DEBUG_OBJECT (stream, "event %s was not handled correctly",
              GST_EVENT_TYPE_NAME (event));
        } else {
          /* If at least one push returns TRUE, then we return TRUE. */
          GST_DEBUG_OBJECT (stream, "event %s was handled correctly",
              GST_EVENT_TYPE_NAME (event));
        }
      }
    }

    gst_event_unref (event);
  }
}

static inline gboolean
have_open_streams (GstFluPSDemux * demux)
{
  return (demux->streams_found[0] != NULL);
}

static gboolean
gst_flups_demux_sink_event (GstPad * pad, GstEvent * event)
{
  gboolean res = TRUE;
  GstFluPSDemux *demux;

  demux = GST_FLUPS_DEMUX (gst_pad_get_parent (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      gst_flups_demux_send_event (demux, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      gst_flups_demux_send_event (demux, event);
      gst_segment_init (&demux->sink_segment, GST_FORMAT_UNDEFINED);
      gst_flups_demux_flush (demux);
      break;
    case GST_EVENT_NEWSEGMENT:
    {
      gboolean update;
      gdouble rate;
      GstFormat format;
      gint64 start, stop, time;

      /* Close current segment */
      gst_flups_demux_close_segment (demux);

      {
        gdouble arate;

        gst_event_parse_new_segment_full (event, &update, &rate, &arate,
            &format, &start, &stop, &time);
        gst_segment_set_newsegment_full (&demux->sink_segment, update, rate,
            arate, format, start, stop, time);

        if (format == GST_FORMAT_BYTES && demux->scr_rate_n != G_MAXUINT64
            && demux->scr_rate_d != G_MAXUINT64) {

          gst_segment_set_newsegment_full (&demux->src_segment, update, rate,
              arate, GST_FORMAT_TIME, BYTES_TO_GSTTIME (start),
              BYTES_TO_GSTTIME (stop), BYTES_TO_GSTTIME (time));
        }

      }

      GST_INFO_OBJECT (demux, "received new segment: rate %g "
          "format %d, start: %" G_GINT64_FORMAT ", stop: %" G_GINT64_FORMAT
          ", time: %" G_GINT64_FORMAT, rate, format, start, stop, time);

      /* we need to emit a new segment */
      gst_flups_demux_mark_discont (demux, TRUE, TRUE);
      gst_event_unref (event);

      break;
    }
    case GST_EVENT_EOS:
      GST_INFO_OBJECT (demux, "Received EOS");
      if (!gst_flups_demux_send_event (demux, event)
          && !have_open_streams (demux)) {
        GST_WARNING_OBJECT (demux, "EOS and no streams open");
        GST_ELEMENT_ERROR (demux, STREAM, FAILED,
            ("Internal data stream error."), ("No valid streams detected"));
      }
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    {
      const GstStructure *structure = gst_event_get_structure (event);

      if (structure != NULL
          && gst_structure_has_name (structure, "application/x-gst-dvd")) {
        res = gst_flups_demux_handle_dvd_event (demux, event);
      } else {
        gst_flups_demux_send_event (demux, event);
      }
      break;
    }
    default:
      gst_flups_demux_send_event (demux, event);
      break;
  }

  gst_object_unref (demux);

  return res;
}

static gboolean
gst_flups_demux_handle_seek_push (GstFluPSDemux * demux, GstEvent * event)
{
  gboolean res = FALSE;
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  gint64 bstart, bstop;
  GstEvent *bevent;

  gst_event_parse_seek (event, &rate, &format, &flags, &start_type, &start,
      &stop_type, &stop);

  GST_DEBUG_OBJECT (demux, "seek event, rate: %f start: %" GST_TIME_FORMAT
      " stop: %" GST_TIME_FORMAT, rate, GST_TIME_ARGS (start),
      GST_TIME_ARGS (stop));

  if (format == GST_FORMAT_BYTES) {
    GST_DEBUG_OBJECT (demux, "seek not supported on format %d", format);
    goto not_supported;
  }

  GST_DEBUG_OBJECT (demux, "seek - trying directly upstream first");

  /* first try original format seek */
  (void) gst_event_ref (event);
  if ((res = gst_pad_push_event (demux->sinkpad, event)))
    goto done;

  if (format != GST_FORMAT_TIME) {
    /* From here down, we only support time based seeks */
    GST_DEBUG_OBJECT (demux, "seek not supported on format %d", format);
    goto not_supported;
  }

  /* We need to convert to byte based seek and we need a scr_rate for that. */
  if (demux->scr_rate_n == G_MAXUINT64 || demux->scr_rate_d == G_MAXUINT64) {
    GST_DEBUG_OBJECT (demux, "seek not possible, no scr_rate");
    goto not_supported;
  }

  GST_DEBUG_OBJECT (demux, "try with scr_rate interpolation");

  bstart = GSTTIME_TO_BYTES (start);
  bstop = GSTTIME_TO_BYTES (stop);

  GST_DEBUG_OBJECT (demux, "in bytes bstart %" G_GINT64_FORMAT " bstop %"
      G_GINT64_FORMAT, bstart, bstop);
  bevent = gst_event_new_seek (rate, GST_FORMAT_BYTES, flags, start_type,
      bstart, stop_type, bstop);

  res = gst_pad_push_event (demux->sinkpad, bevent);

done:
  gst_event_unref (event);
  return res;

not_supported:
  {
    gst_event_unref (event);

    return FALSE;
  }
}

static inline void
gst_flups_demux_do_seek (GstFluPSDemux * demux, GstSegment * seeksegment)
{
  gboolean found;
  guint64 fscr, offset;
  guint64 scr = GSTTIME_TO_MPEGTIME (seeksegment->last_stop + demux->base_time);
  guint64 scr_rate_n = demux->last_scr_offset - demux->first_scr_offset;
  guint64 scr_rate_d = demux->last_scr - demux->first_scr;

  /* In some clips the PTS values are completely unaligned with SCR values.
   * To improve the seek in that situation we apply a factor considering the
   * relationship between last PTS and last SCR */
  if (demux->last_scr > demux->last_pts)
    scr = gst_util_uint64_scale (scr, demux->last_scr, demux->last_pts);

  scr = MIN (demux->last_scr, scr);
  scr = MAX (demux->first_scr, scr);
  fscr = scr;

  GST_INFO_OBJECT (demux, "sink segment configured %" GST_SEGMENT_FORMAT
      ", trying to go at SCR: %" G_GUINT64_FORMAT, &demux->sink_segment, scr);

  offset =
      MIN (gst_util_uint64_scale (scr - demux->first_scr, scr_rate_n,
          scr_rate_d), demux->sink_segment.stop);

  found = gst_flups_demux_scan_forward_ts (demux, &offset, SCAN_SCR, &fscr);
  if (!found) {
    found = gst_flups_demux_scan_backward_ts (demux, &offset, SCAN_SCR, &fscr);
  }

  while (found && fscr < scr) {
    offset++;
    found = gst_flups_demux_scan_forward_ts (demux, &offset, SCAN_SCR, &fscr);
  }

  while (found && fscr > scr && offset > 0) {
    offset--;
    found = gst_flups_demux_scan_backward_ts (demux, &offset, SCAN_SCR, &fscr);
  }

  GST_INFO_OBJECT (demux, "doing seek at offset %" G_GUINT64_FORMAT
      " SCR: %" G_GUINT64_FORMAT " %" GST_TIME_FORMAT,
      offset, fscr, GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (fscr)));
  gst_segment_set_last_stop (&demux->sink_segment, GST_FORMAT_BYTES, offset);
}

static gboolean
gst_flups_demux_handle_seek_pull (GstFluPSDemux * demux, GstEvent * event)
{
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  gdouble rate;
  gboolean update, flush;
  GstSegment seeksegment;
  GstClockTime first_pts = MPEGTIME_TO_GSTTIME (demux->first_pts);

  gst_event_parse_seek (event, &rate, &format, &flags,
      &start_type, &start, &stop_type, &stop);

  if (format != GST_FORMAT_TIME)
    goto wrong_format;

  GST_DEBUG_OBJECT (demux, "Seek requested start %" GST_TIME_FORMAT " stop %"
      GST_TIME_FORMAT, GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

  /* We need to convert to byte based seek and we need a scr_rate for that. */
  if (demux->scr_rate_n == G_MAXUINT64 || demux->scr_rate_d == G_MAXUINT64)
    goto no_scr_rate;

  flush = flags & GST_SEEK_FLAG_FLUSH;
  /* keyframe = flags & GST_SEEK_FLAG_KEY_UNIT; *//* FIXME */

  if (flush) {
    /* Flush start up and downstream to make sure data flow and loops are
       idle */
    demux->flushing = TRUE;
    gst_flups_demux_send_event (demux, gst_event_new_flush_start ());
    gst_pad_push_event (demux->sinkpad, gst_event_new_flush_start ());
  } else {
    /* Pause the pulling task */
    gst_pad_pause_task (demux->sinkpad);
  }

  /* Take the stream lock */
  GST_PAD_STREAM_LOCK (demux->sinkpad);

  if (flush) {
    /* Stop flushing upstream we need to pull */
    demux->flushing = FALSE;
    gst_pad_push_event (demux->sinkpad, gst_event_new_flush_stop ());
  }

  /* Work on a copy until we are sure the seek succeeded. */
  memcpy (&seeksegment, &demux->src_segment, sizeof (GstSegment));

  GST_DEBUG_OBJECT (demux, "segment before configure %" GST_SEGMENT_FORMAT,
      &demux->src_segment);

  /* Apply the seek to our segment */
  gst_segment_set_seek (&seeksegment, rate, format, flags,
      start_type, start, stop_type, stop, &update);

  GST_DEBUG_OBJECT (demux, "seek segment configured %" GST_SEGMENT_FORMAT,
      &seeksegment);

  if (flush || seeksegment.last_stop != demux->src_segment.last_stop) {
    /* Do the actual seeking */
    gst_flups_demux_do_seek (demux, &seeksegment);
  }

  /* check the limits */
  if (seeksegment.rate > 0.0) {
    if (seeksegment.start < first_pts - demux->base_time) {
      seeksegment.start = first_pts - demux->base_time;
      seeksegment.last_stop = seeksegment.start;
    }
  }

  /* update the rate in our src segment */
  demux->sink_segment.rate = rate;

  GST_DEBUG_OBJECT (demux, "seek segment adjusted %" GST_SEGMENT_FORMAT,
      &seeksegment);

  if (flush) {
    /* Stop flushing, the sinks are at time 0 now */
    gst_flups_demux_send_event (demux, gst_event_new_flush_stop ());
  } else {
    gst_flups_demux_close_segment (demux);
  }

  if (flush || seeksegment.last_stop != demux->src_segment.last_stop) {
    gst_flups_demux_flush (demux);
  }

  /* Ok seek succeeded, take the newly configured segment */
  memcpy (&demux->src_segment, &seeksegment, sizeof (GstSegment));

  /* Notify about the start of a new segment */
  if (demux->src_segment.flags & GST_SEEK_FLAG_SEGMENT) {
    gst_element_post_message (GST_ELEMENT (demux),
        gst_message_new_segment_start (GST_OBJECT (demux),
            demux->src_segment.format, demux->src_segment.last_stop));
  }

  /* Tell all the stream a new segment is needed */
  gst_flups_demux_mark_discont (demux, TRUE, TRUE);

  gst_pad_start_task (demux->sinkpad,
      (GstTaskFunction) gst_flups_demux_loop, demux->sinkpad);

  GST_PAD_STREAM_UNLOCK (demux->sinkpad);

  gst_event_unref (event);
  return TRUE;

  /* ERRORS */
wrong_format:
  {
    GST_WARNING_OBJECT (demux, "we only support seeking in TIME or BYTES "
        "formats");
    gst_event_unref (event);
    return FALSE;
  }
no_scr_rate:
  {
    GST_WARNING_OBJECT (demux, "seek not possible, no scr_rate");
    gst_event_unref (event);
    return FALSE;
  }
}

static gboolean
gst_flups_demux_src_event (GstPad * pad, GstEvent * event)
{
  gboolean res = FALSE;
  GstFluPSDemux *demux;

  demux = GST_FLUPS_DEMUX (gst_pad_get_parent (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      if (demux->random_access) {
        res = gst_flups_demux_handle_seek_pull (demux, event);
      } else {
        res = gst_flups_demux_handle_seek_push (demux, event);
      }
      break;
    default:
      res = gst_pad_push_event (demux->sinkpad, event);
      break;
  }

  gst_object_unref (demux);

  return res;
}

static const GstQueryType *
gst_flups_demux_src_query_type (GstPad * pad)
{
  static const GstQueryType types[] = {
    GST_QUERY_POSITION,
    GST_QUERY_DURATION,
    GST_QUERY_SEEKING,
    0
  };

  return types;
}

static gboolean
gst_flups_demux_src_query (GstPad * pad, GstQuery * query)
{
  gboolean res = FALSE;
  GstFluPSDemux *demux;

  demux = GST_FLUPS_DEMUX (gst_pad_get_parent (pad));

  GST_LOG_OBJECT (demux, "Have query of type %d on pad %" GST_PTR_FORMAT,
      GST_QUERY_TYPE (query), pad);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      GstClockTime pos;
      GstFormat format;

      gst_query_parse_position (query, &format, NULL);

      if (format != GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (demux, "position not supported for format %d",
            format);
        goto not_supported;
      }

      pos = demux->src_segment.last_stop - demux->src_segment.start;
      GST_LOG_OBJECT (demux, "Position %" GST_TIME_FORMAT, GST_TIME_ARGS (pos));
      gst_query_set_position (query, format, pos);
      res = TRUE;
      break;
    }
    case GST_QUERY_DURATION:
    {
      GstFormat format;
      gint64 duration;
      GstPad *peer;

      gst_query_parse_duration (query, &format, NULL);

      if (G_LIKELY (format == GST_FORMAT_TIME &&
              GST_CLOCK_TIME_IS_VALID (demux->src_segment.duration))) {
        gst_query_set_duration (query, GST_FORMAT_TIME,
            demux->src_segment.duration);
        res = TRUE;
        break;
      }

      if ((peer = gst_pad_get_peer (demux->sinkpad)) == NULL) {
        GST_DEBUG_OBJECT (demux, "duration not possible, no peer");
        goto not_supported;
      }

      /* For any format other than bytes, see if upstream knows first */
      if (format == GST_FORMAT_BYTES) {
        GST_DEBUG_OBJECT (demux, "duration not supported for format %d",
            format);
        gst_object_unref (peer);
        goto not_supported;
      }

      if (gst_pad_query (peer, query)) {
        gst_object_unref (peer);
        res = TRUE;
        break;
      }

      /* Upstream didn't know, so we can only answer TIME queries from
       * here on */
      if (format != GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (demux, "duration not supported for format %d",
            format);
        gst_object_unref (peer);
        goto not_supported;
      }

      if (demux->mux_rate == -1) {
        GST_DEBUG_OBJECT (demux, "duration not possible, no mux_rate");
        gst_object_unref (peer);
        goto not_supported;
      }

      gst_query_set_duration (query, GST_FORMAT_BYTES, -1);

      if (!gst_pad_query (peer, query)) {
        GST_LOG_OBJECT (demux, "query on peer pad failed");
        gst_object_unref (peer);
        goto not_supported;
      }
      gst_object_unref (peer);

      gst_query_parse_duration (query, &format, &duration);

      GST_LOG_OBJECT (demux,
          "query on peer pad reported bytes %" G_GUINT64_FORMAT, duration);

      duration = BYTES_TO_GSTTIME (duration);

      GST_LOG_OBJECT (demux, "converted to time %" GST_TIME_FORMAT,
          GST_TIME_ARGS (duration));

      gst_query_set_duration (query, GST_FORMAT_TIME, duration);
      res = TRUE;
      break;
    }
    case GST_QUERY_SEEKING:{
      GstFormat fmt;

      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);

      res = TRUE;
      if (demux->random_access) {
        /* In pull mode we can seek in TIME format if we have the SCR */
        if (fmt != GST_FORMAT_TIME || demux->scr_rate_n == G_MAXUINT64
            || demux->scr_rate_d == G_MAXUINT64)
          gst_query_set_seeking (query, fmt, FALSE, -1, -1);
        else
          gst_query_set_seeking (query, fmt, TRUE, 0, -1);
      } else {
        if (fmt == GST_FORMAT_BYTES) {
          /* Seeking in BYTES format not supported at all */
          gst_query_set_seeking (query, fmt, FALSE, -1, -1);
        } else {
          GstQuery *peerquery;
          gboolean seekable;

          /* Then ask upstream */
          res = gst_pad_peer_query (demux->sinkpad, query);
          if (res) {
            /* If upstream can handle seeks we're done, if it
             * can't we still have our TIME->BYTES conversion seek
             */
            gst_query_parse_seeking (query, NULL, &seekable, NULL, NULL);
            if (seekable || fmt != GST_FORMAT_TIME)
              goto beach;
          }

          /* We can seek if upstream supports BYTES seeks and we
           * have the SCR
           */
          peerquery = gst_query_new_seeking (GST_FORMAT_BYTES);
          res = gst_pad_peer_query (demux->sinkpad, peerquery);
          if (!res || demux->scr_rate_n == G_MAXUINT64
              || demux->scr_rate_d == G_MAXUINT64) {
            gst_query_set_seeking (query, fmt, FALSE, -1, -1);
          } else {
            gst_query_parse_seeking (peerquery, NULL, &seekable, NULL, NULL);
            if (seekable)
              gst_query_set_seeking (query, GST_FORMAT_TIME, TRUE, 0, -1);
            else
              gst_query_set_seeking (query, fmt, FALSE, -1, -1);
          }

          gst_query_unref (peerquery);
          res = TRUE;
        }
      }
      break;
    }
    default:
      res = gst_pad_query_default (pad, query);
      break;
  }

beach:
  gst_object_unref (demux);

  return res;

not_supported:
  {
    gst_object_unref (demux);

    return FALSE;
  }
}

static void
gst_flups_demux_reset_psm (GstFluPSDemux * demux)
{
  gint i;

#define FILL_TYPE(start, stop, type)	\
  for (i=start; i <= stop; i++)			\
    demux->psm[i] = type;

  FILL_TYPE (0x00, 0x1f, -1);
  FILL_TYPE (0x20, 0x3f, ST_PS_DVD_SUBPICTURE);
  FILL_TYPE (0x40, 0x7f, -1);
  FILL_TYPE (0x80, 0x87, ST_PS_AUDIO_AC3);
  FILL_TYPE (0x88, 0x9f, ST_PS_AUDIO_DTS);
  FILL_TYPE (0xa0, 0xaf, ST_PS_AUDIO_LPCM);
  FILL_TYPE (0xbd, 0xbd, -1);
  FILL_TYPE (0xc0, 0xdf, ST_AUDIO_MPEG1);
  FILL_TYPE (0xe0, 0xef, ST_GST_VIDEO_MPEG1_OR_2);
  FILL_TYPE (0xf0, 0xff, -1);

#undef FILL_TYPE
}

/* ISO/IEC 13818-1:
 * pack_header() {
 *     pack_start_code                                   32  bslbf  -+
 *     '01'                                               2  bslbf   |
 *     system_clock_reference_base [32..30]               3  bslbf   |
 *     marker_bit                                         1  bslbf   |
 *     system_clock_reference_base [29..15]              15  bslbf   |
 *     marker_bit                                         1  bslbf   |
 *     system_clock_reference_base [14..0]               15  bslbf   |
 *     marker_bit                                         1  bslbf   | 112 bits
 *     system_clock_reference_extension                   9  ubslbf  |
 *     marker_bit                                         1  bslbf   |
 *     program_mux_rate                                  22  ubslbf  |
 *     marker_bit                                         1  bslbf   |
 *     marker_bit                                         1  bslbf   |
 *     reserved                                           5  bslbf   |
 *     pack_stuffing_length                               3  ubslbf -+
 *
 *     for (i = 0; i < pack_stuffing_length; i++) {
 *         stuffing_byte '1111 1111'                      8  bslbf
 *     }
 *
 * 112 bits = 14 bytes, as max value for pack_stuffing_length is 7, then
 * in total it's needed 14 + 7 = 21 bytes.
 */
#define PACK_START_SIZE     21

static GstFlowReturn
gst_flups_demux_parse_pack_start (GstFluPSDemux * demux)
{
  const guint8 *data;
  guint length;
  guint32 scr1, scr2;
  guint64 scr, scr_adjusted, new_rate;
  guint64 scr_rate_n;
  guint64 scr_rate_d;
  guint avail = gst_adapter_available (demux->adapter);

  GST_DEBUG ("parsing pack start");

  if (G_UNLIKELY (avail < PACK_START_SIZE))
    goto need_more_data;

  data = gst_adapter_peek (demux->adapter, PACK_START_SIZE);

  /* skip start code */
  data += 4;

  scr1 = GST_READ_UINT32_BE (data);
  scr2 = GST_READ_UINT32_BE (data + 4);

  /* fixed length to begin with, start code and two scr values */
  length = 8 + 4;

  /* start parsing the stream */
  if ((*data & 0xc0) == 0x40) {
    guint32 scr_ext;
    guint32 next32;
    guint8 stuffing_bytes;

    GST_DEBUG ("Found MPEG2 stream");
    demux->is_mpeg2_pack = TRUE;

    /* mpeg2 has more data */
    length += 2;

    /* :2=01 ! scr:3 ! marker:1==1 ! scr:15 ! marker:1==1 ! scr:15 */

    /* check markers */
    if (G_UNLIKELY ((scr1 & 0xc4000400) != 0x44000400))
      goto lost_sync;

    scr = ((guint64) scr1 & 0x38000000) << 3;
    scr |= ((guint64) scr1 & 0x03fff800) << 4;
    scr |= ((guint64) scr1 & 0x000003ff) << 5;
    scr |= ((guint64) scr2 & 0xf8000000) >> 27;

    /* marker:1==1 ! scr_ext:9 ! marker:1==1 */
    if (G_UNLIKELY ((scr2 & 0x04010000) != 0x04010000))
      goto lost_sync;

    scr_ext = (scr2 & 0x03fe0000) >> 17;
    /* We keep the offset of this scr */
    demux->cur_scr_offset = demux->adapter_offset + 12;

    GST_DEBUG_OBJECT (demux, "SCR: 0x%08" G_GINT64_MODIFIER "x SCRE: 0x%08x",
        scr, scr_ext);

    if (scr_ext) {
      scr = (scr * 300 + scr_ext % 300) / 300;
    }
    /* SCR has been converted into units of 90Khz ticks to make it comparable
       to DTS/PTS, that also implies 1 tick rounding error */
    data += 6;
    /* PMR:22 ! :2==11 ! reserved:5 ! stuffing_len:3 */
    next32 = GST_READ_UINT32_BE (data);
    if (G_UNLIKELY ((next32 & 0x00000300) != 0x00000300))
      goto lost_sync;

    new_rate = (next32 & 0xfffffc00) >> 10;

    stuffing_bytes = (next32 & 0x07);
    GST_DEBUG_OBJECT (demux, "stuffing bytes: %d", stuffing_bytes);

    data += 4;
    length += stuffing_bytes;
    while (stuffing_bytes--) {
      if (*data++ != 0xff)
        goto lost_sync;
    }
  } else {
    GST_DEBUG ("Found MPEG1 stream");
    demux->is_mpeg2_pack = FALSE;

    /* check markers */
    if (G_UNLIKELY ((scr1 & 0xf1000100) != 0x21000100))
      goto lost_sync;

    if (G_UNLIKELY ((scr2 & 0x01800001) != 0x01800001))
      goto lost_sync;

    /* :4=0010 ! scr:3 ! marker:1==1 ! scr:15 ! marker:1==1 ! scr:15 ! marker:1==1 */
    scr = ((guint64) scr1 & 0x0e000000) << 5;
    scr |= ((guint64) scr1 & 0x00fffe00) << 6;
    scr |= ((guint64) scr1 & 0x000000ff) << 7;
    scr |= ((guint64) scr2 & 0xfe000000) >> 25;

    /* We keep the offset of this scr */
    demux->cur_scr_offset = demux->adapter_offset + 8;

    /* marker:1==1 ! mux_rate:22 ! marker:1==1 */
    new_rate = (scr2 & 0x007ffffe) >> 1;

    data += 8;
  }
  new_rate *= MPEG_MUX_RATE_MULT;

  /* scr adjusted is the new scr found + the colected adjustment */
  scr_adjusted = scr + demux->scr_adjust;

  GST_LOG_OBJECT (demux,
      "SCR: %" G_GINT64_FORMAT " (%" G_GINT64_FORMAT "), mux_rate %"
      G_GINT64_FORMAT ", GStreamer Time:%" GST_TIME_FORMAT,
      scr, scr_adjusted, new_rate,
      GST_TIME_ARGS (MPEGTIME_TO_GSTTIME ((guint64) scr)));

  /* keep the first src in order to calculate delta time */
  if (G_UNLIKELY (demux->first_scr == G_MAXUINT64)) {
    demux->first_scr = scr;
    demux->first_scr_offset = demux->cur_scr_offset;
    demux->base_time = MPEGTIME_TO_GSTTIME (demux->first_scr);
    /* at begin consider the new_rate as the scr rate, bytes/clock ticks */
    scr_rate_n = new_rate;
    scr_rate_d = CLOCK_FREQ;
  } else if (G_LIKELY (demux->first_scr_offset != demux->cur_scr_offset)) {
    /* estimate byte rate related to the SCR */
    scr_rate_n = demux->cur_scr_offset - demux->first_scr_offset;
    scr_rate_d = scr_adjusted - demux->first_scr;
  } else {
    scr_rate_n = demux->scr_rate_n;
    scr_rate_d = demux->scr_rate_d;
  }

  GST_DEBUG_OBJECT (demux, "%s mode scr: %" G_GUINT64_FORMAT " at %"
      G_GUINT64_FORMAT ", first scr: %" G_GUINT64_FORMAT
      " at %" G_GUINT64_FORMAT ", scr rate: %" G_GUINT64_FORMAT
      "/%" G_GUINT64_FORMAT "(%f)",
      ((demux->sink_segment.rate >= 0.0) ? "forward" : "backward"),
      scr, demux->cur_scr_offset,
      demux->first_scr, demux->first_scr_offset,
      scr_rate_n, scr_rate_d, (float) scr_rate_n / scr_rate_d);

  /* adjustment of the SCR */
  if (G_LIKELY (demux->current_scr != G_MAXUINT64)) {
    guint64 diff;
    guint64 old_scr, old_mux_rate, bss, adjust = 0;

    /* keep SCR of the previous packet */
    old_scr = demux->current_scr;
    old_mux_rate = demux->mux_rate;

    /* Bytes since SCR is the amount we placed in the adapter since then
     * (demux->bytes_since_scr) minus the amount remaining in the adapter,
     * clamped to >= 0 */
    bss = MAX (0, (gint) (demux->bytes_since_scr - avail));

    /* estimate the new SCR using the previous one according the notes
       on point 2.5.2.2 of the ISO/IEC 13818-1 document */
    if (old_mux_rate != 0)
      adjust = (bss * CLOCK_FREQ) / old_mux_rate;

    if (demux->sink_segment.rate >= 0.0)
      demux->next_scr = old_scr + adjust;
    else
      demux->next_scr = old_scr - adjust;

    GST_DEBUG_OBJECT (demux,
        "bss: %" G_GUINT64_FORMAT ", next_scr: %" G_GUINT64_FORMAT
        ", old_scr: %" G_GUINT64_FORMAT ", scr: %" G_GUINT64_FORMAT,
        bss, demux->next_scr, old_scr, scr_adjusted);

    /* calculate the absolute deference between the last scr and
       the new one */
    if (G_UNLIKELY (old_scr > scr_adjusted))
      diff = old_scr - scr_adjusted;
    else
      diff = scr_adjusted - old_scr;

    /* if the difference is more than 1 second we need to reconfigure
       adjustment */
    if (G_UNLIKELY (diff > CLOCK_FREQ)) {
      demux->scr_adjust = demux->next_scr - scr;
      GST_DEBUG_OBJECT (demux, "discont found, diff: %" G_GINT64_FORMAT
          ", adjust %" G_GINT64_FORMAT, diff, demux->scr_adjust);
      scr_adjusted = demux->next_scr;
      /* don't update rate estimation on disconts */
      scr_rate_n = demux->scr_rate_n;
      scr_rate_d = demux->scr_rate_d;
    } else {
      demux->next_scr = scr_adjusted;
    }
  }

  /* update the current_scr and rate members */
  demux->mux_rate = new_rate;
  demux->current_scr = scr_adjusted;
  demux->scr_rate_n = scr_rate_n;
  demux->scr_rate_d = scr_rate_d;

  /* Reset the bytes_since_scr value to count the data remaining in the
   * adapter */
  demux->bytes_since_scr = avail;

  gst_adapter_flush (demux->adapter, length);
  ADAPTER_OFFSET_FLUSH (length);
  return GST_FLOW_OK;

lost_sync:
  {
    GST_DEBUG_OBJECT (demux, "lost sync");
    return GST_FLOW_LOST_SYNC;
  }
need_more_data:
  {
    GST_DEBUG_OBJECT (demux, "need more data");
    return GST_FLOW_NEED_MORE_DATA;
  }
}

/* ISO/IEC 13818-1:
 * system_header () {
 *     system_header_start_code                          32  bslbf  -+
 *     header_length                                     16  uimsbf  |
 *     marker_bit                                         1  bslbf   |
 *     rate_bound                                        22  uimsbf  |
 *     marker_bit                                         1  bslbf   |
 *     audio_bound                                        6  uimsbf  |
 *     fixed_flag                                         1  bslbf   |
 *     CSPS_flag                                          1  bslbf   | 96 bits
 *     system_audio_lock_flag                             1  bslbf   |
 *     system_video_lock_flag                             1  bslbf   |
 *     marker_bit                                         1  bslbf   |
 *     video_bound                                        5  uimsbf  |
 *     packet_rate_restriction_flag                       1  bslbf   |
 *     reserved_bits                                      7  bslbf  -+
 *     while (nextbits () = = '1') {
 *         stream_id                                      8  uimsbf -+
 *         '11'                                           2  bslbf   | 24 bits
 *         P-STD_buffer_bound_scale                       1  bslbf   |
 *         P-STD_buffer_size_bound                       13  uimsbf -+
 *     }
 * }
 * 96 bits = 12 bytes, 24 bits = 3 bytes.
 */

static GstFlowReturn
gst_flups_demux_parse_sys_head (GstFluPSDemux * demux)
{
  guint16 length;
  const guint8 *data;
  gboolean csps;

  /* start code + length */
  if (!(data = gst_adapter_peek (demux->adapter, 6)))
    goto need_more_data;

  /* skip start code */
  data += 4;

  length = GST_READ_UINT16_BE (data);
  GST_DEBUG_OBJECT (demux, "length %d", length);

  length += 6;

  if (!(data = gst_adapter_peek (demux->adapter, length)))
    goto need_more_data;

  /* skip start code and length */
  data += 6;

  /* marker:1==1 ! rate_bound:22 | marker:1==1 */
  if ((*data & 0x80) != 0x80)
    goto marker_expected;

  {
    guint32 rate_bound;

    if ((data[2] & 0x01) != 0x01)
      goto marker_expected;

    rate_bound = ((guint32) data[0] & 0x7f) << 15;
    rate_bound |= ((guint32) data[1]) << 7;
    rate_bound |= ((guint32) data[2] & 0xfe) >> 1;
    rate_bound *= MPEG_MUX_RATE_MULT;

    GST_DEBUG_OBJECT (demux, "rate bound %u", rate_bound);

    data += 3;
  }

  /* audio_bound:6==1 ! fixed:1 | constrained:1 */
  {
    guint8 audio_bound;
    gboolean fixed;

    /* max number of simultaneous audio streams active */
    audio_bound = (data[0] & 0xfc) >> 2;
    /* fixed or variable bitrate */
    fixed = (data[0] & 0x02) == 0x02;
    /* meeting constraints */
    csps = (data[0] & 0x01) == 0x01;

    GST_DEBUG_OBJECT (demux, "audio_bound %d, fixed %d, constrained %d",
        audio_bound, fixed, csps);
    data += 1;
  }

  /* audio_lock:1 | video_lock:1 | marker:1==1 | video_bound:5 */
  {
    gboolean audio_lock;
    gboolean video_lock;
    guint8 video_bound;

    audio_lock = (data[0] & 0x80) == 0x80;
    video_lock = (data[0] & 0x40) == 0x40;

    if ((data[0] & 0x20) != 0x20)
      goto marker_expected;

    /* max number of simultaneous video streams active */
    video_bound = (data[0] & 0x1f);

    GST_DEBUG_OBJECT (demux, "audio_lock %d, video_lock %d, video_bound %d",
        audio_lock, video_lock, video_bound);
    data += 1;
  }

  /* packet_rate_restriction:1 | reserved:7==0x7F */
  {
    gboolean packet_rate_restriction;

    if ((data[0] & 0x7f) != 0x7f)
      goto marker_expected;

    /* only valid if csps is set */
    if (csps) {
      packet_rate_restriction = (data[0] & 0x80) == 0x80;

      GST_DEBUG_OBJECT (demux, "packet_rate_restriction %d",
          packet_rate_restriction);
    }
  }
  data += 1;

  {
    gint stream_count = (length - 12) / 3;
    gint i;

    GST_DEBUG_OBJECT (demux, "number of streams: %d ", stream_count);

    for (i = 0; i < stream_count; i++) {
      guint8 stream_id;
      gboolean STD_buffer_bound_scale;
      guint16 STD_buffer_size_bound;
      guint32 buf_byte_size_bound;

      stream_id = *data++;
      if (!(stream_id & 0x80))
        goto sys_len_error;

      /* check marker bits */
      if ((*data & 0xC0) != 0xC0)
        goto no_placeholder_bits;

      STD_buffer_bound_scale = *data & 0x20;
      STD_buffer_size_bound = ((guint16) (*data++ & 0x1F)) << 8;
      STD_buffer_size_bound |= *data++;

      if (STD_buffer_bound_scale == 0) {
        buf_byte_size_bound = STD_buffer_size_bound * 128;
      } else {
        buf_byte_size_bound = STD_buffer_size_bound * 1024;
      }

      GST_DEBUG_OBJECT (demux, "STD_buffer_bound_scale %d",
          STD_buffer_bound_scale);
      GST_DEBUG_OBJECT (demux, "STD_buffer_size_bound %d or %d bytes",
          STD_buffer_size_bound, buf_byte_size_bound);
    }
  }

  gst_adapter_flush (demux->adapter, length);
  ADAPTER_OFFSET_FLUSH (length);
  return GST_FLOW_OK;

  /* ERRORS */
marker_expected:
  {
    GST_DEBUG_OBJECT (demux, "expecting marker");
    return GST_FLOW_LOST_SYNC;
  }
no_placeholder_bits:
  {
    GST_DEBUG_OBJECT (demux, "expecting placeholder bit values"
        " '11' after stream id");
    return GST_FLOW_LOST_SYNC;
  }
sys_len_error:
  {
    GST_DEBUG_OBJECT (demux, "error in system header length");
    return GST_FLOW_LOST_SYNC;
  }
need_more_data:
  {
    GST_DEBUG_OBJECT (demux, "need more data");
    return GST_FLOW_NEED_MORE_DATA;
  }
}

static GstFlowReturn
gst_flups_demux_parse_psm (GstFluPSDemux * demux)
{
  guint16 length = 0, info_length = 0, es_map_length = 0;
  guint8 psm_version = 0;
  const guint8 *data, *es_map_base;
  gboolean applicable;

  /* start code + length */
  if (!(data = gst_adapter_peek (demux->adapter, 6)))
    goto need_more_data;

  /* skip start code */
  data += 4;

  length = GST_READ_UINT16_BE (data);
  GST_DEBUG_OBJECT (demux, "length %u", length);

  if (G_UNLIKELY (length > 0x3FA))
    goto psm_len_error;

  length += 6;

  if (!(data = gst_adapter_peek (demux->adapter, length)))
    goto need_more_data;

  /* skip start code and length */
  data += 6;

  /* Read PSM applicable bit together with version */
  psm_version = GST_READ_UINT8 (data);
  applicable = (psm_version & 0x80) >> 7;
  psm_version &= 0x1F;
  GST_DEBUG_OBJECT (demux, "PSM version %u (applicable now %u)", psm_version,
      applicable);

  /* Jump over version and marker bit */
  data += 2;

  /* Read PS info length */
  info_length = GST_READ_UINT16_BE (data);
  /* Cap it to PSM length - needed bytes for ES map length and CRC */
  info_length = MIN (length - 16, info_length);
  GST_DEBUG_OBJECT (demux, "PS info length %u bytes", info_length);

  /* Jump over that section */
  data += (2 + info_length);

  /* Read ES map length */
  es_map_length = GST_READ_UINT16_BE (data);
  /* Cap it to PSM remaining length -  CRC */
  es_map_length = MIN (length - (16 + info_length), es_map_length);
  GST_DEBUG_OBJECT (demux, "ES map length %u bytes", es_map_length);

  /* Jump over the size */
  data += 2;

  /* Now read the ES map */
  es_map_base = data;
  while (es_map_base + 4 <= data + es_map_length) {
    guint8 stream_type = 0, stream_id = 0;
    guint16 stream_info_length = 0;

    stream_type = GST_READ_UINT8 (es_map_base);
    es_map_base++;
    stream_id = GST_READ_UINT8 (es_map_base);
    es_map_base++;
    stream_info_length = GST_READ_UINT16_BE (es_map_base);
    es_map_base += 2;
    /* Cap stream_info_length */
    stream_info_length = MIN (data + es_map_length - es_map_base,
        stream_info_length);

    GST_DEBUG_OBJECT (demux, "Stream type %02X with id %02X and %u bytes info",
        stream_type, stream_id, stream_info_length);
    if (G_LIKELY (stream_id != 0xbd))
      demux->psm[stream_id] = stream_type;
    else {
      /* Ignore stream type for private_stream_1 and discover it looking at
       * the stream data.
       * Fixes demuxing some clips with lpcm that was wrongly declared as
       * mpeg audio */
      GST_DEBUG_OBJECT (demux, "stream type for private_stream_1 ignored");
    }
    es_map_base += stream_info_length;
  }

  gst_adapter_flush (demux->adapter, length);
  ADAPTER_OFFSET_FLUSH (length);
  return GST_FLOW_OK;

psm_len_error:
  {
    GST_DEBUG_OBJECT (demux, "error in PSM length");
    return GST_FLOW_LOST_SYNC;
  }
need_more_data:
  {
    GST_DEBUG_OBJECT (demux, "need more data");
    return GST_FLOW_NEED_MORE_DATA;
  }
}

static void
gst_flups_demux_resync_cb (GstPESFilter * filter, GstFluPSDemux * demux)
{
}

static GstFlowReturn
gst_flups_demux_data_cb (GstPESFilter * filter, gboolean first,
    GstBuffer * buffer, GstFluPSDemux * demux)
{
  GstBuffer *out_buf;
  GstFlowReturn ret = GST_FLOW_OK;
  gint stream_type;
  guint32 start_code;
  guint8 id;
  guint8 *data;
  guint datalen;
  guint offset = 0;

  data = GST_BUFFER_DATA (buffer);
  datalen = GST_BUFFER_SIZE (buffer);

  start_code = filter->start_code;
  id = filter->id;

  if (first) {
    /* find the stream type */
    stream_type = demux->psm[id];
    if (stream_type == -1) {
      /* no stream type, if PS1, get the new id */
      if (start_code == ID_PRIVATE_STREAM_1 && datalen >= 2) {
        guint8 nframes;

        /* VDR writes A52 streams without any header bytes
         * (see ftp://ftp.mplayerhq.hu/MPlayer/samples/MPEG-VOB/vdr-AC3) */
        if (datalen >= 4) {
          guint hdr = GST_READ_UINT32_BE (data);

          if (G_UNLIKELY ((hdr & 0xffff0000) == AC3_SYNC_WORD)) {
            id = 0x80;
            stream_type = demux->psm[id] = ST_GST_AUDIO_RAWA52;
            GST_DEBUG_OBJECT (demux, "Found VDR raw A52 stream");
          }
        }

        if (G_LIKELY (stream_type == -1)) {
          /* new id is in the first byte */
          id = data[offset++];
          datalen--;

          /* and remap */
          stream_type = demux->psm[id];

          /* Now, if it's a subpicture stream - no more, otherwise
           * take the first byte too, since it's the frame count in audio
           * streams and our backwards compat convention is to strip it off */
          if (stream_type != ST_PS_DVD_SUBPICTURE) {
            /* Number of audio frames in this packet */
            nframes = data[offset++];
            datalen--;
            GST_DEBUG_OBJECT (demux, "private type 0x%02x, %d frames", id,
                nframes);
          } else {
            GST_DEBUG_OBJECT (demux, "private type 0x%02x, stream type %d", id,
                stream_type);
          }
        }
      }
      if (stream_type == -1)
        goto unknown_stream_type;
    }
    if (filter->pts != -1) {
      demux->next_pts = filter->pts + demux->scr_adjust;
      GST_DEBUG_OBJECT (demux, "PTS = %" G_GUINT64_FORMAT
          "(%" G_GUINT64_FORMAT ")", filter->pts, demux->next_pts);
    } else
      demux->next_pts = G_MAXUINT64;

    if (filter->dts != -1) {
      demux->next_dts = filter->dts + demux->scr_adjust;
    } else {
      demux->next_dts = demux->next_pts;
    }
    GST_DEBUG_OBJECT (demux, "DTS = orig %" G_GUINT64_FORMAT
        " (%" G_GUINT64_FORMAT ")", filter->dts, demux->next_dts);

    demux->current_stream = gst_flups_demux_get_stream (demux, id, stream_type);
  }

  if (G_UNLIKELY (demux->current_stream == NULL)) {
    GST_DEBUG_OBJECT (demux, "Dropping buffer for unknown stream id 0x%02x",
        id);
    goto done;
  }

  /* After 2 seconds of bitstream emit no more pads */
  if (demux->need_no_more_pads
      && (demux->current_scr - demux->first_scr) > 2 * CLOCK_FREQ) {
    GST_DEBUG_OBJECT (demux, "no more pads, notifying");
    gst_element_no_more_pads (GST_ELEMENT_CAST (demux));
    demux->need_no_more_pads = FALSE;
  }

  /* If the stream is not-linked, don't bother creating a sub-buffer
   * to send to it, unless we're processing a discont (which resets
   * the not-linked status and tries again */
  if (demux->current_stream->discont) {
    GST_DEBUG_OBJECT (demux, "stream is discont");
    demux->current_stream->notlinked = FALSE;
  }

  if (demux->current_stream->notlinked == FALSE) {
    out_buf = gst_buffer_create_sub (buffer, offset, datalen);

    ret = gst_flups_demux_send_data (demux, demux->current_stream, out_buf);
    if (ret == GST_FLOW_NOT_LINKED) {
      demux->current_stream->notlinked = TRUE;
      ret = GST_FLOW_OK;
    }
  }

done:
  gst_buffer_unref (buffer);

  return ret;

  /* ERRORS */
unknown_stream_type:
  {
    GST_DEBUG_OBJECT (demux, "unknown stream type %02x", id);
    ret = GST_FLOW_OK;
    goto done;
  }
}

static gboolean
gst_flups_demux_resync (GstFluPSDemux * demux, gboolean save)
{
  const guint8 *data;
  gint avail;
  guint32 code;
  gint offset;
  gboolean found;

  avail = gst_adapter_available (demux->adapter);
  if (G_UNLIKELY (avail < 4))
    goto need_data;

  /* Common case, read 4 bytes an check it */
  data = gst_adapter_peek (demux->adapter, 4);

  /* read currect code */
  code = GST_READ_UINT32_BE (data);

  /* The common case is that the sync code is at 0 bytes offset */
  if (G_LIKELY ((code & 0xffffff00) == 0x100L)) {
    GST_LOG_OBJECT (demux, "Found resync code %08x after 0 bytes", code);
    demux->last_sync_code = code;
    return TRUE;
  }

  /* Otherwise, we are starting at byte 4 and we need to search
     the sync code in all available data in the adapter */
  offset = 4;
  if (offset >= avail)
    goto need_data;             /* Not enough data to find sync */

  data = gst_adapter_peek (demux->adapter, avail);

  do {
    code = (code << 8) | data[offset++];
    found = (code & 0xffffff00) == 0x100L;
  } while (offset < avail && !found);

  if (!save || demux->sink_segment.rate >= 0.0) {
    GST_LOG_OBJECT (demux, "flushing %d bytes", offset - 4);
    /* forward playback, we can discard and flush the skipped bytes */
    gst_adapter_flush (demux->adapter, offset - 4);
    ADAPTER_OFFSET_FLUSH (offset - 4);
  } else {
    if (found) {
      GST_LOG_OBJECT (demux, "reverse saving %d bytes", offset - 4);
      /* reverse playback, we keep the flushed bytes and we will append them to
       * the next buffer in the chain function, which is the previous buffer in
       * the stream. */
      gst_adapter_push (demux->rev_adapter,
          gst_adapter_take_buffer (demux->adapter, offset - 4));
    } else {
      GST_LOG_OBJECT (demux, "reverse saving %d bytes", avail);
      /* nothing found, keep all bytes */
      gst_adapter_push (demux->rev_adapter,
          gst_adapter_take_buffer (demux->adapter, avail));
    }
  }

  if (found) {
    GST_LOG_OBJECT (demux, "Found resync code %08x after %d bytes",
        code, offset - 4);
    demux->last_sync_code = code;
  } else {
    GST_LOG_OBJECT (demux, "No resync after skipping %d", offset);
  }

  return found;

need_data:
  {
    GST_LOG_OBJECT (demux, "we need more data for resync %d", avail);
    return FALSE;
  }
}

static inline gboolean
gst_flups_demux_is_pes_sync (guint32 sync)
{
  return ((sync & 0xfc) == 0xbc) ||
      ((sync & 0xe0) == 0xc0) || ((sync & 0xf0) == 0xe0);
}

static inline gboolean
gst_flups_demux_scan_ts (GstFluPSDemux * demux, const guint8 * data,
    SCAN_MODE mode, guint64 * rts)
{
  gboolean ret = FALSE;
  guint32 scr1, scr2;
  guint64 scr;
  guint64 pts, dts;
  guint32 code;

  /* read the 4 bytes for the sync code */
  code = GST_READ_UINT32_BE (data);
  if (G_LIKELY (code != ID_PS_PACK_START_CODE))
    goto beach;

  /* skip start code */
  data += 4;

  scr1 = GST_READ_UINT32_BE (data);
  scr2 = GST_READ_UINT32_BE (data + 4);

  /* start parsing the stream */
  if ((*data & 0xc0) == 0x40) {
    guint32 scr_ext;
    guint32 next32;
    guint8 stuffing_bytes;

    /* :2=01 ! scr:3 ! marker:1==1 ! scr:15 ! marker:1==1 ! scr:15 */

    /* check markers */
    if ((scr1 & 0xc4000400) != 0x44000400)
      goto beach;

    scr = ((guint64) scr1 & 0x38000000) << 3;
    scr |= ((guint64) scr1 & 0x03fff800) << 4;
    scr |= ((guint64) scr1 & 0x000003ff) << 5;
    scr |= ((guint64) scr2 & 0xf8000000) >> 27;

    /* marker:1==1 ! scr_ext:9 ! marker:1==1 */
    if ((scr2 & 0x04010000) != 0x04010000)
      goto beach;

    scr_ext = (scr2 & 0x03fe0000) >> 17;

    if (scr_ext) {
      scr = (scr * 300 + scr_ext % 300) / 300;
    }
    /* SCR has been converted into units of 90Khz ticks to make it comparable
       to DTS/PTS, that also implies 1 tick rounding error */
    data += 6;
    /* PMR:22 ! :2==11 ! reserved:5 ! stuffing_len:3 */
    next32 = GST_READ_UINT32_BE (data);
    if ((next32 & 0x00000300) != 0x00000300)
      goto beach;

    stuffing_bytes = (next32 & 0x07);
    data += 4;
    while (stuffing_bytes--) {
      if (*data++ != 0xff)
        goto beach;
    }
  } else {
    /* check markers */
    if ((scr1 & 0xf1000100) != 0x21000100)
      goto beach;

    if ((scr2 & 0x01800001) != 0x01800001)
      goto beach;

    /* :4=0010 ! scr:3 ! marker:1==1 ! scr:15 ! marker:1==1 ! scr:15 ! marker:1==1 */
    scr = ((guint64) scr1 & 0x0e000000) << 5;
    scr |= ((guint64) scr1 & 0x00fffe00) << 6;
    scr |= ((guint64) scr1 & 0x000000ff) << 7;
    scr |= ((guint64) scr2 & 0xfe000000) >> 25;
    data += 8;
  }

  if (mode == SCAN_SCR) {
    *rts = scr;
    ret = TRUE;
  }

  /* read the 4 bytes for the PES sync code */
  code = GST_READ_UINT32_BE (data);
  if (!gst_flups_demux_is_pes_sync (code))
    goto beach;

  switch (code) {
    case ID_PS_PROGRAM_STREAM_MAP:
    case ID_PRIVATE_STREAM_2:
    case ID_ECM_STREAM:
    case ID_EMM_STREAM:
    case ID_PROGRAM_STREAM_DIRECTORY:
    case ID_DSMCC_STREAM:
    case ID_ITU_TREC_H222_TYPE_E_STREAM:
    case ID_PADDING_STREAM:
      goto beach;
    default:
      break;
  }

  /* skip sync code and size */
  data += 6;

  pts = dts = -1;

  /* stuffing bits, first two bits are '10' for mpeg2 pes so this code is
   * not triggered. */
  while (TRUE) {
    if (*data != 0xff)
      break;
    data++;
  }

  /* STD buffer size, never for mpeg2 */
  if ((*data & 0xc0) == 0x40)
    data += 2;

  /* PTS but no DTS, never for mpeg2 */
  if ((*data & 0xf0) == 0x20) {
    READ_TS (data, pts, beach);
  }
  /* PTS and DTS, never for mpeg2 */
  else if ((*data & 0xf0) == 0x30) {
    READ_TS (data, pts, beach);
    READ_TS (data, dts, beach);
  } else if ((*data & 0xc0) == 0x80) {
    /* mpeg2 case */
    guchar flags;

    /* 2: '10'
     * 2: PES_scrambling_control
     * 1: PES_priority
     * 1: data_alignment_indicator
     * 1: copyright
     * 1: original_or_copy
     */
    flags = *data++;

    if ((flags & 0xc0) != 0x80)
      goto beach;

    /* 2: PTS_DTS_flags
     * 1: ESCR_flag
     * 1: ES_rate_flag
     * 1: DSM_trick_mode_flag
     * 1: additional_copy_info_flag
     * 1: PES_CRC_flag
     * 1: PES_extension_flag
     */
    flags = *data++;

    /* 8: PES_header_data_length */
    data++;

    /* only DTS: this is invalid */
    if ((flags & 0xc0) == 0x40)
      goto beach;

    /* check for PTS */
    if ((flags & 0x80)) {
      READ_TS (data, pts, beach);
    }
    /* check for DTS */
    if ((flags & 0x40)) {
      READ_TS (data, dts, beach);
    }
  }

  if (mode == SCAN_DTS && dts != -1) {
    *rts = dts;
    ret = TRUE;
  }

  if (mode == SCAN_PTS && pts != -1) {
    *rts = pts;
    ret = TRUE;
  }
beach:
  return ret;
}

static inline gboolean
gst_flups_demux_scan_forward_ts (GstFluPSDemux * demux, guint64 * pos,
    SCAN_MODE mode, guint64 * rts)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer = NULL;
  guint64 offset = *pos;
  gboolean found = FALSE;
  guint64 ts = 0;
  guint scan_sz = (mode == SCAN_SCR ? SCAN_SCR_SZ : SCAN_PTS_SZ);
  guint cursor, to_read = BLOCK_SZ;
  guint8 *data;
  guint end_scan;

  do {
    if (offset + scan_sz > demux->sink_segment.stop)
      return FALSE;

    if (offset + to_read > demux->sink_segment.stop)
      to_read = demux->sink_segment.stop - offset;

    /* read some data */
    ret = gst_pad_pull_range (demux->sinkpad, offset, to_read, &buffer);
    if (G_UNLIKELY (ret != GST_FLOW_OK))
      return FALSE;

    data = GST_BUFFER_DATA (buffer);
    end_scan = GST_BUFFER_SIZE (buffer) - scan_sz;
    /* scan the block */
    for (cursor = 0; !found && cursor <= end_scan; cursor++) {
      found = gst_flups_demux_scan_ts (demux, data++, mode, &ts);
    }

    /* done with the buffer, unref it */
    gst_buffer_unref (buffer);

    if (found) {
      *rts = ts;
      *pos = offset + cursor - 1;
    } else {
      offset += cursor;
    }
  } while (!found && offset < demux->sink_segment.stop);

  return found;
}

static inline gboolean
gst_flups_demux_scan_backward_ts (GstFluPSDemux * demux, guint64 * pos,
    SCAN_MODE mode, guint64 * rts)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer = NULL;
  guint64 offset = *pos;
  gboolean found = FALSE;
  guint64 ts = 0;
  guint scan_sz = (mode == SCAN_SCR ? SCAN_SCR_SZ : SCAN_PTS_SZ);
  guint cursor, to_read = BLOCK_SZ;
  guint start_scan;
  guint8 *data;

  do {
    if (offset < scan_sz - 1)
      return FALSE;

    if (offset > BLOCK_SZ)
      offset -= BLOCK_SZ;
    else {
      to_read = offset + 1;
      offset = 0;
    }
    /* read some data */
    ret = gst_pad_pull_range (demux->sinkpad, offset, to_read, &buffer);
    if (G_UNLIKELY (ret != GST_FLOW_OK))
      return FALSE;

    start_scan = GST_BUFFER_SIZE (buffer) - scan_sz;
    data = GST_BUFFER_DATA (buffer) + start_scan;
    /* scan the block */
    for (cursor = (start_scan + 1); !found && cursor > 0; cursor--) {
      found = gst_flups_demux_scan_ts (demux, data--, mode, &ts);
    }

    /* done with the buffer, unref it */
    gst_buffer_unref (buffer);

    if (found) {
      *rts = ts;
      *pos = offset + cursor;
    }

  } while (!found && offset > 0);

  return found;
}

static inline gboolean
gst_flups_sink_get_duration (GstFluPSDemux * demux)
{
  gboolean res = FALSE;
  GstPad *peer;
  GstFormat format = GST_FORMAT_BYTES;
  gint64 length = 0;
  guint64 offset;
  guint i;
  guint64 scr = 0;

  /* init the sink segment */
  gst_segment_init (&demux->sink_segment, format);

  /* get peer to figure out length */
  if ((peer = gst_pad_get_peer (demux->sinkpad)) == NULL)
    goto beach;

  res = gst_pad_query_duration (peer, &format, &length);
  gst_object_unref (peer);

  if (!res || length <= 0)
    goto beach;

  GST_DEBUG_OBJECT (demux, "file length %" G_GINT64_FORMAT, length);

  /* update the sink segment */
  demux->sink_segment.stop = length;
  gst_segment_set_duration (&demux->sink_segment, format, length);
  gst_segment_set_last_stop (&demux->sink_segment, format, 0);

  /* Scan for notorious SCR and PTS to calculate the duration */
  /* scan for first SCR in the stream */
  offset = demux->sink_segment.start;
  gst_flups_demux_scan_forward_ts (demux, &offset, SCAN_SCR, &demux->first_scr);
  GST_DEBUG_OBJECT (demux, "First SCR: %" G_GINT64_FORMAT " %" GST_TIME_FORMAT
      " in packet starting at %" G_GUINT64_FORMAT,
      demux->first_scr, GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->first_scr)),
      offset);
  demux->first_scr_offset = offset;
  /* scan for last SCR in the stream */
  offset = demux->sink_segment.stop;
  gst_flups_demux_scan_backward_ts (demux, &offset, SCAN_SCR, &demux->last_scr);
  GST_DEBUG_OBJECT (demux, "Last SCR: %" G_GINT64_FORMAT " %" GST_TIME_FORMAT
      " in packet starting at %" G_GUINT64_FORMAT,
      demux->last_scr, GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->last_scr)),
      offset);
  demux->last_scr_offset = offset;
  /* scan for first PTS in the stream */
  offset = demux->sink_segment.start;
  gst_flups_demux_scan_forward_ts (demux, &offset, SCAN_PTS, &demux->first_pts);
  GST_DEBUG_OBJECT (demux, "First PTS: %" G_GINT64_FORMAT " %" GST_TIME_FORMAT
      " in packet starting at %" G_GUINT64_FORMAT,
      demux->first_pts, GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->first_pts)),
      offset);
  if (demux->first_pts != G_MAXUINT64) {
    /* scan for last PTS in the stream */
    offset = demux->sink_segment.stop;
    gst_flups_demux_scan_backward_ts (demux, &offset, SCAN_PTS,
        &demux->last_pts);
    GST_DEBUG_OBJECT (demux,
        "Last PTS: %" G_GINT64_FORMAT " %" GST_TIME_FORMAT
        " in packet starting at %" G_GUINT64_FORMAT, demux->last_pts,
        GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->last_pts)), offset);
  }
  /* Detect wrong SCR values */
  if (demux->first_scr > demux->last_scr) {
    GST_DEBUG_OBJECT (demux, "Wrong SCR values detected, searching for "
        "a better first SCR value");
    offset = demux->first_scr_offset;
    for (i = 0; i < 10; i++) {
      offset++;
      gst_flups_demux_scan_forward_ts (demux, &offset, SCAN_SCR, &scr);
      if (scr < demux->last_scr) {
        demux->first_scr = scr;
        demux->first_scr_offset = offset;
        /* Start demuxing from the right place */
        demux->sink_segment.last_stop = offset;
        GST_DEBUG_OBJECT (demux, "Replaced First SCR: %" G_GINT64_FORMAT
            " %" GST_TIME_FORMAT " in packet starting at %" G_GUINT64_FORMAT,
            demux->first_scr,
            GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->first_scr)), offset);
        break;
      }
    }
  }
  /* Set the base_time and avg rate */
  demux->base_time = MPEGTIME_TO_GSTTIME (demux->first_scr);
  demux->scr_rate_n = demux->last_scr_offset - demux->first_scr_offset;
  demux->scr_rate_d = demux->last_scr - demux->first_scr;

  if (G_LIKELY (demux->first_pts != G_MAXUINT64 &&
          demux->last_pts != G_MAXUINT64)) {
    /* update the src segment */
    demux->src_segment.start =
        MPEGTIME_TO_GSTTIME (demux->first_pts) - demux->base_time;
    demux->src_segment.stop = -1;
    gst_segment_set_duration (&demux->src_segment, GST_FORMAT_TIME,
        MPEGTIME_TO_GSTTIME (demux->last_pts - demux->first_pts));
    gst_segment_set_last_stop (&demux->src_segment, GST_FORMAT_TIME,
        demux->src_segment.start);
  }
  GST_INFO_OBJECT (demux, "sink segment configured %" GST_SEGMENT_FORMAT,
      &demux->sink_segment);
  GST_INFO_OBJECT (demux, "src segment configured %" GST_SEGMENT_FORMAT,
      &demux->src_segment);

  res = TRUE;

beach:
  return res;
}

static inline GstFlowReturn
gst_flups_demux_pull_block (GstPad * pad, GstFluPSDemux * demux,
    guint64 offset, guint size)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer;
  ret = gst_pad_pull_range (pad, offset, size, &buffer);
  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    GST_DEBUG_OBJECT (demux, "pull range at %" G_GUINT64_FORMAT
        " size %u failed", offset, size);
    goto beach;
  } else
    GST_LOG_OBJECT (demux, "pull range at %" G_GUINT64_FORMAT
        " size %u done", offset, size);

  if (demux->sink_segment.rate < 0) {
    GST_LOG_OBJECT (demux, "setting discont flag on backward rate");
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
  }
  ret = gst_flups_demux_chain (pad, buffer);

beach:
  return ret;
}

static void
gst_flups_demux_loop (GstPad * pad)
{
  GstFluPSDemux *demux;
  GstFlowReturn ret = GST_FLOW_OK;
  guint64 offset = 0;

  demux = GST_FLUPS_DEMUX (gst_pad_get_parent (pad));

  if (G_UNLIKELY (demux->flushing)) {
    ret = GST_FLOW_WRONG_STATE;
    goto pause;
  }

  if (G_UNLIKELY (demux->sink_segment.format == GST_FORMAT_UNDEFINED))
    gst_flups_sink_get_duration (demux);

  offset = demux->sink_segment.last_stop;
  if (demux->sink_segment.rate >= 0) {
    guint size = BLOCK_SZ;
    if (G_LIKELY (demux->sink_segment.stop != -1)) {
      size = MIN (size, demux->sink_segment.stop - offset);
    }
    /* pull in data */
    ret = gst_flups_demux_pull_block (pad, demux, offset, size);

    /* pause if something went wrong */
    if (G_UNLIKELY (ret != GST_FLOW_OK))
      goto pause;

    /* update our position */
    offset += size;
    gst_segment_set_last_stop (&demux->sink_segment, GST_FORMAT_BYTES, offset);

    /* check EOS condition */
    if ((demux->src_segment.flags & GST_SEEK_FLAG_SEGMENT) &&
        ((demux->sink_segment.last_stop >= demux->sink_segment.stop) ||
            (demux->src_segment.stop != -1 &&
                demux->src_segment.last_stop >= demux->src_segment.stop))) {
      GST_DEBUG_OBJECT (demux, "forward mode using segment reached end of "
          "segment pos %" GST_TIME_FORMAT " stop %" GST_TIME_FORMAT
          " pos in bytes %" G_GUINT64_FORMAT " stop in bytes %"
          G_GUINT64_FORMAT, GST_TIME_ARGS (demux->src_segment.last_stop),
          GST_TIME_ARGS (demux->src_segment.stop),
          demux->sink_segment.last_stop, demux->sink_segment.stop);
      ret = GST_FLOW_UNEXPECTED;
      goto pause;
    }
  } else {                      /* Reverse playback */
    guint64 size = MIN (offset, BLOCK_SZ);

    /* pull in data */
    ret = gst_flups_demux_pull_block (pad, demux, offset - size, size);

    /* pause if something went wrong */
    if (G_UNLIKELY (ret != GST_FLOW_OK))
      goto pause;

    /* update our position */
    offset -= size;
    gst_segment_set_last_stop (&demux->sink_segment, GST_FORMAT_BYTES, offset);

    /* check EOS condition */
    if (demux->sink_segment.last_stop <= demux->sink_segment.start ||
        demux->src_segment.last_stop <= demux->src_segment.start) {
      GST_DEBUG_OBJECT (demux, "reverse mode using segment reached end of "
          "segment pos %" GST_TIME_FORMAT " stop %" GST_TIME_FORMAT
          " pos in bytes %" G_GUINT64_FORMAT " stop in bytes %"
          G_GUINT64_FORMAT, GST_TIME_ARGS (demux->src_segment.last_stop),
          GST_TIME_ARGS (demux->src_segment.start),
          demux->sink_segment.last_stop, demux->sink_segment.start);
      ret = GST_FLOW_UNEXPECTED;
      goto pause;
    }
  }

  gst_object_unref (demux);

  return;

pause:
  {
    const gchar *reason = gst_flow_get_name (ret);

    GST_LOG_OBJECT (demux, "pausing task, reason %s", reason);
    gst_pad_pause_task (pad);

    if (ret == GST_FLOW_UNEXPECTED) {
      /* perform EOS logic */
      gst_element_no_more_pads (GST_ELEMENT_CAST (demux));
      if (demux->src_segment.flags & GST_SEEK_FLAG_SEGMENT) {
        gint64 stop;

        /* for segment playback we need to post when (in stream time)
         * we stopped, this is either stop (when set) or the duration. */
        if ((stop = demux->src_segment.stop) == -1)
          stop = demux->src_segment.duration;

        if (demux->sink_segment.rate >= 0) {
          GST_LOG_OBJECT (demux, "Sending segment done, at end of segment");
          gst_element_post_message (GST_ELEMENT_CAST (demux),
              gst_message_new_segment_done (GST_OBJECT_CAST (demux),
                  GST_FORMAT_TIME, stop));
        } else {                /* Reverse playback */
          GST_LOG_OBJECT (demux, "Sending segment done, at beginning of "
              "segment");
          gst_element_post_message (GST_ELEMENT_CAST (demux),
              gst_message_new_segment_done (GST_OBJECT_CAST (demux),
                  GST_FORMAT_TIME, demux->src_segment.start));
        }
      } else {
        /* normal playback, send EOS to all linked pads */
        gst_element_no_more_pads (GST_ELEMENT (demux));
        GST_LOG_OBJECT (demux, "Sending EOS, at end of stream");
        if (!gst_flups_demux_send_event (demux, gst_event_new_eos ())
            && !have_open_streams (demux)) {
          GST_WARNING_OBJECT (demux, "EOS and no streams open");
          GST_ELEMENT_ERROR (demux, STREAM, FAILED,
              ("Internal data stream error."), ("No valid streams detected"));
        }
      }
    } else if (ret == GST_FLOW_NOT_LINKED || ret < GST_FLOW_UNEXPECTED) {
      GST_ELEMENT_ERROR (demux, STREAM, FAILED,
          ("Internal data stream error."),
          ("stream stopped, reason %s", reason));
      gst_flups_demux_send_event (demux, gst_event_new_eos ());
    }

    gst_object_unref (demux);
    return;
  }
}

/* If we can pull that's prefered */
static gboolean
gst_flups_demux_sink_activate (GstPad * sinkpad)
{
  if (gst_pad_check_pull_range (sinkpad)) {
    return gst_pad_activate_pull (sinkpad, TRUE);
  } else {
    return gst_pad_activate_push (sinkpad, TRUE);
  }
}

/* This function gets called when we activate ourselves in push mode. */
static gboolean
gst_flups_demux_sink_activate_push (GstPad * sinkpad, gboolean active)
{
  GstFluPSDemux *demux;

  demux = GST_FLUPS_DEMUX (gst_pad_get_parent (sinkpad));

  demux->random_access = FALSE;

  gst_object_unref (demux);

  return TRUE;
}

/* this function gets called when we activate ourselves in pull mode.
 * We can perform  random access to the resource and we start a task
 * to start reading */
static gboolean
gst_flups_demux_sink_activate_pull (GstPad * sinkpad, gboolean active)
{
  GstFluPSDemux *demux;

  demux = GST_FLUPS_DEMUX (gst_pad_get_parent (sinkpad));

  if (active) {
    GST_DEBUG ("pull mode activated");
    demux->random_access = TRUE;
    gst_object_unref (demux);
    return gst_pad_start_task (sinkpad, (GstTaskFunction) gst_flups_demux_loop,
        sinkpad);
  } else {
    demux->random_access = FALSE;
    gst_object_unref (demux);
    return gst_pad_stop_task (sinkpad);
  }
}

static GstFlowReturn
gst_flups_demux_chain (GstPad * pad, GstBuffer * buffer)
{
  GstFluPSDemux *demux = GST_FLUPS_DEMUX (gst_pad_get_parent (pad));
  GstFlowReturn ret = GST_FLOW_OK;
  guint32 avail;
  gboolean save, discont;

  discont = GST_BUFFER_IS_DISCONT (buffer);

  if (discont) {
    GST_LOG_OBJECT (demux, "Received buffer with discont flag and"
        " offset %" G_GUINT64_FORMAT, GST_BUFFER_OFFSET (buffer));

    gst_pes_filter_drain (&demux->filter);
    gst_flups_demux_mark_discont (demux, TRUE, FALSE);

    /* mark discont on all streams */
    if (demux->sink_segment.rate >= 0.0) {
      demux->current_scr = G_MAXUINT64;
      demux->bytes_since_scr = 0;
    }
  } else {
    GST_LOG_OBJECT (demux, "Received buffer with offset %" G_GUINT64_FORMAT,
        GST_BUFFER_OFFSET (buffer));
  }

  /* We keep the offset to interpolate SCR */
  demux->adapter_offset = GST_BUFFER_OFFSET (buffer);

  gst_adapter_push (demux->adapter, buffer);
  demux->bytes_since_scr += GST_BUFFER_SIZE (buffer);

  avail = gst_adapter_available (demux->rev_adapter);
  if (avail > 0) {
    GST_LOG_OBJECT (demux, "appending %u saved bytes", avail);
    /* if we have a previous reverse chunk, append this now */
    /* FIXME this code assumes we receive discont buffers all thei
     * time */
    gst_adapter_push (demux->adapter,
        gst_adapter_take_buffer (demux->rev_adapter, avail));
  }

  avail = gst_adapter_available (demux->adapter);
  GST_LOG_OBJECT (demux, "avail now: %d, state %d", avail, demux->filter.state);

  switch (demux->filter.state) {
    case STATE_DATA_SKIP:
    case STATE_DATA_PUSH:
      ret = gst_pes_filter_process (&demux->filter);
      break;
    case STATE_HEADER_PARSE:
      break;
    default:
      break;
  }

  switch (ret) {
    case GST_FLOW_NEED_MORE_DATA:
      /* Go and get more data */
      ret = GST_FLOW_OK;
      goto done;
    case GST_FLOW_LOST_SYNC:
      /* for FLOW_OK or lost-sync, carry onto resync */
      ret = GST_FLOW_OK;
      break;
    case GST_FLOW_OK:
      break;
    default:
      /* Any other return value should be sent upstream immediately */
      goto done;
  }

  /* align adapter data to sync boundary, we keep the data up to the next sync
   * point. */
  save = TRUE;
  while (gst_flups_demux_resync (demux, save)) {
    gboolean ps_sync = TRUE;
    if (G_UNLIKELY (demux->flushing)) {
      ret = GST_FLOW_WRONG_STATE;
      goto done;
    }

    /* now switch on last synced byte */
    switch (demux->last_sync_code) {
      case ID_PS_PACK_START_CODE:
        ret = gst_flups_demux_parse_pack_start (demux);
        break;
      case ID_PS_SYSTEM_HEADER_START_CODE:
        ret = gst_flups_demux_parse_sys_head (demux);
        break;
      case ID_PS_END_CODE:
        ret = GST_FLOW_OK;
        goto done;
      case ID_PS_PROGRAM_STREAM_MAP:
        ret = gst_flups_demux_parse_psm (demux);
        break;
      default:
        if (gst_flups_demux_is_pes_sync (demux->last_sync_code)) {
          ret = gst_pes_filter_process (&demux->filter);
        } else {
          GST_DEBUG_OBJECT (demux, "sync_code=%08x, non PES sync found"
              ", continuing", demux->last_sync_code);
          ps_sync = FALSE;
          ret = GST_FLOW_LOST_SYNC;
        }
        break;
    }
    /* if we found a ps sync, we stop saving the data, any non-ps sync gets
     * saved up to the next ps sync. */
    if (ps_sync)
      save = FALSE;

    switch (ret) {
      case GST_FLOW_NEED_MORE_DATA:
        GST_DEBUG_OBJECT (demux, "need more data");
        ret = GST_FLOW_OK;
        goto done;
      case GST_FLOW_LOST_SYNC:
        if (!save || demux->sink_segment.rate >= 0.0) {
          GST_DEBUG_OBJECT (demux, "flushing 3 bytes");
          gst_adapter_flush (demux->adapter, 3);
          ADAPTER_OFFSET_FLUSH (3);
        } else {
          GST_DEBUG_OBJECT (demux, "saving 3 bytes");
          gst_adapter_push (demux->rev_adapter,
              gst_adapter_take_buffer (demux->adapter, 3));
        }
        ret = GST_FLOW_OK;
        break;
      default:
        break;
    }
  }
done:
  gst_object_unref (demux);

  return ret;
}

static GstStateChangeReturn
gst_flups_demux_change_state (GstElement * element, GstStateChange transition)
{
  GstFluPSDemux *demux = GST_FLUPS_DEMUX (element);
  GstStateChangeReturn result;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      demux->adapter = gst_adapter_new ();
      demux->rev_adapter = gst_adapter_new ();
      demux->adapter_offset = G_MAXUINT64;
      gst_pes_filter_init (&demux->filter, demux->adapter,
          &demux->adapter_offset);
      gst_pes_filter_set_callbacks (&demux->filter,
          (GstPESFilterData) gst_flups_demux_data_cb,
          (GstPESFilterResync) gst_flups_demux_resync_cb, demux);
      demux->filter.gather_pes = TRUE;
      demux->first_scr = G_MAXUINT64;
      demux->last_scr = G_MAXUINT64;
      demux->current_scr = G_MAXUINT64;
      demux->base_time = G_MAXUINT64;
      demux->scr_rate_n = G_MAXUINT64;
      demux->scr_rate_d = G_MAXUINT64;
      demux->first_pts = G_MAXUINT64;
      demux->last_pts = G_MAXUINT64;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      demux->current_scr = G_MAXUINT64;
      demux->mux_rate = G_MAXUINT64;
      demux->next_pts = G_MAXUINT64;
      demux->next_dts = G_MAXUINT64;
      demux->first_scr = G_MAXUINT64;
      demux->last_scr = G_MAXUINT64;
      demux->base_time = G_MAXUINT64;
      demux->scr_rate_n = G_MAXUINT64;
      demux->scr_rate_d = G_MAXUINT64;
      demux->need_no_more_pads = TRUE;
      demux->first_pts = G_MAXUINT64;
      demux->last_pts = G_MAXUINT64;
      gst_flups_demux_reset_psm (demux);
      gst_segment_init (&demux->sink_segment, GST_FORMAT_UNDEFINED);
      gst_segment_init (&demux->src_segment, GST_FORMAT_TIME);
      gst_flups_demux_flush (demux);
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_flups_demux_reset (demux);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_pes_filter_uninit (&demux->filter);
      g_object_unref (demux->adapter);
      demux->adapter = NULL;
      g_object_unref (demux->rev_adapter);
      demux->rev_adapter = NULL;
      break;
    default:
      break;
  }

  return result;
}

gboolean
gst_flups_demux_plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "mpegpsdemux",
          GST_RANK_PRIMARY, GST_TYPE_FLUPS_DEMUX))
    return FALSE;

  return TRUE;
}
