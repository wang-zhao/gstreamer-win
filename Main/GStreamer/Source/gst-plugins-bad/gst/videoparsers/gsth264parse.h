/* GStreamer H.264 Parser
 * Copyright (C) <2010> Collabora ltd
 * Copyright (C) <2010> Nokia Corporation
 * Copyright (C) <2011> Intel Corporation
 *
 * Copyright (C) <2010> Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
 * Copyright (C) <2011> Thibault Saunier <thibault.saunier@collabora.com>
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

#ifndef __GST_H264_PARSE_H__
#define __GST_H264_PARSE_H__

#include <gst/gst.h>
#include <gst/base/gstbaseparse.h>
#include <gst/codecparsers/gsth264parser.h>

G_BEGIN_DECLS

typedef struct _H264Params H264Params;

#define GST_TYPE_H264_PARSE \
  (gst_h264_parse_get_type())
#define GST_H264_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_H264_PARSE,GstH264Parse))
#define GST_H264_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_H264_PARSE,GstH264ParseClass))
#define GST_IS_H264_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_H264_PARSE))
#define GST_IS_H264_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_H264_PARSE))

GType gst_h264_parse_get_type (void);

typedef struct _GstH264Parse GstH264Parse;
typedef struct _GstH264ParseClass GstH264ParseClass;

struct _GstH264Parse
{
  GstBaseParse baseparse;

  GstPadChainFunction parse_chain;

  /* stream */
  gint width, height;
  gint fps_num, fps_den;
  gint aspect_ratio_idc;
  gint sar_width, sar_height;
  gint upstream_par_n, upstream_par_d;
  GstBuffer *codec_data;
  guint nal_length_size;
  gboolean packetized;

  /* state */
  GstH264NalParser *nalparser;
  GstH264NalUnit nalu;
  guint align;
  guint format;
  guint current_off;
  gboolean packetized_last;
  gboolean packetized_chunked;

  GstClockTime last_report;
  gboolean push_codec;

  /* collected SPS and PPS NALUs */
  GstBuffer *sps_nals[GST_H264_MAX_SPS_COUNT];
  GstBuffer *pps_nals[GST_H264_MAX_PPS_COUNT];

  /* Infos we need to keep track of */
  guint32 sei_cpb_removal_delay;
  guint8 sei_pic_struct;
  guint8 sei_pic_struct_pres_flag;
  guint field_pic_flag;

  /* cached timestamps */
  /* (trying to) track upstream dts and interpolate */
  GstClockTime dts;
  /* dts at start of last buffering period */
  GstClockTime ts_trn_nb;
  gboolean do_ts;

  /* frame parsing */
  /*guint last_nal_pos;*/
  /*guint next_sc_pos;*/
  gint idr_pos, sei_pos;
  gboolean update_caps;
  GstAdapter *frame_out;
  gboolean keyframe;
  gboolean frame_start;
  /* AU state */
  gboolean picture_start;

  /* props */
  gboolean split_packetized;
  guint interval;

  GstClockTime pending_key_unit_ts;
  GstEvent *force_key_unit_event;
};

struct _GstH264ParseClass
{
  GstBaseParseClass parent_class;
};

G_END_DECLS
#endif
