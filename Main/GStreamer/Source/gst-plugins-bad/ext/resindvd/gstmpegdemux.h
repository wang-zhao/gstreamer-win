/* 
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
 * The Original Code is Fluendo MPEG Demuxer plugin.
 *
 * The Initial Developer of the Original Code is Fluendo, S.L.
 * Portions created by Fluendo, S.L. are Copyright (C) 2005
 * Fluendo, S.L. All Rights Reserved.
 *
 * Contributor(s): Wim Taymans <wim@fluendo.com>
 *                 Jan Schmidt <thaytan@noraisin.net>
 */

#ifndef __GST_FLUPS_DEMUX_H__
#define __GST_FLUPS_DEMUX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "gstpesfilter.h"

G_BEGIN_DECLS

#define GST_TYPE_FLUPS_DEMUX		(gst_flups_demux_get_type())
#define GST_FLUPS_DEMUX(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FLUPS_DEMUX,GstFluPSDemux))
#define GST_FLUPS_DEMUX_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FLUPS_DEMUX,GstFluPSDemuxClass))
#define GST_FLUPS_DEMUX_GET_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS((klass),GST_TYPE_FLUPS_DEMUX,GstFluPSDemuxClass))
#define GST_IS_FLUPS_DEMUX(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FLUPS_DEMUX))
#define GST_IS_FLUPS_DEMUX_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FLUPS_DEMUX))

typedef struct _GstFluPSStream GstFluPSStream;
typedef struct _GstFluPSDemux GstFluPSDemux;
typedef struct _GstFluPSDemuxClass GstFluPSDemuxClass;

#define GST_FLUPS_DEMUX_MAX_STREAMS	256
#define GST_FLUPS_DEMUX_MAX_PSM		256
#define MAX_DVD_AUDIO_STREAMS 8
#define MAX_DVD_SUBPICTURE_STREAMS 32

typedef enum {
  STATE_FLUPS_DEMUX_NEED_SYNC,
  STATE_FLUPS_DEMUX_SYNCED,
  STATE_FLUPS_DEMUX_NEED_MORE_DATA,
} GstFluPSDemuxState;

/* Information associated with a single FluPS stream. */
struct _GstFluPSStream {
  GstPad         * pad;

  gint           id;
  gint           type;
  gint           size_bound;

  gboolean       discont;
  gboolean       notlinked;
  gboolean       need_segment;
 
  GstClockTime   last_seg_start;
  GstClockTime   last_ts;
  GstClockTime   segment_thresh;
};

struct _GstFluPSDemux {
  GstElement     parent;

  GstPad         * sinkpad;

  GstAdapter     * adapter;
  GstAdapter     * rev_adapter;
  guint64        adapter_offset;
  guint32        last_sync_code;
  GstPESFilter   filter;

  gint64         mux_rate;
  guint64	     first_scr;
  guint64	     first_dts;
  guint64	     base_time;
  guint64        current_scr;
  guint64        next_scr;
  guint64        bytes_since_scr;
  gint64         scr_adjust;
  guint64        scr_rate_n;
  guint64        scr_rate_d;
  guint64        first_scr_offset;
  guint64        last_scr_offset;

  gint16         psm[GST_FLUPS_DEMUX_MAX_PSM];

  GstSegment     sink_segment;
  GstSegment     src_segment;

  /* stream output */
  GstFluPSStream * current_stream;
  guint64        next_pts;
  guint64        next_dts;
  GstFluPSStream ** streams;
  gboolean       need_no_more_pads;

  /* Indicates an MPEG-2 stream */
  gboolean is_mpeg2_pack;
  gboolean disable_stream_creation;

  /* Language codes event is stored when a dvd-lang-codes
   * custom event arrives from upstream */
  GstEvent * lang_codes;
  gint       audio_stream_types[MAX_DVD_AUDIO_STREAMS];
};

struct _GstFluPSDemuxClass {
  GstElementClass parent_class;

  GstPadTemplate *sink_template;
  GstPadTemplate *video_template;
  GstPadTemplate *audio_template;
  GstPadTemplate *private_template;
  GstPadTemplate *subpicture_template;
};

GType		gst_flups_demux_get_type	(void);

gboolean	gst_flups_demux_plugin_init	(GstPlugin *plugin);

G_END_DECLS

#endif /* __GST_FLUPS_DEMUX_H__ */
