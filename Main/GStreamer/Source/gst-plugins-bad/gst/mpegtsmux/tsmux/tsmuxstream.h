/* 
 * Copyright 2006 BBC and Fluendo S.A. 
 *
 * This library is licensed under 4 different licenses and you
 * can choose to use it under the terms of any one of them. The
 * four licenses are the MPL 1.1, the LGPL, the GPL and the MIT
 * license.
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
 * GPL:
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
 *
 * MIT:
 *
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef __TSMUXSTREAM_H__
#define __TSMUXSTREAM_H__

#include <glib.h>

#include "tsmuxcommon.h"

G_BEGIN_DECLS

typedef enum TsMuxStreamType TsMuxStreamType;
typedef enum TsMuxStreamState TsMuxStreamState;
typedef struct TsMuxStreamBuffer TsMuxStreamBuffer;

typedef void (*TsMuxStreamBufferReleaseFunc) (guint8 *data, void *user_data);

/* Stream type assignments
 *
 *   0x00    ITU-T | ISO/IEC Reserved
 *   0x01    ISO/IEC 11172 Video
 *   0x02    ITU-T Rec. H.262 | ISO/IEC 13818-2 Video or
 *           ISO/IEC 11172-2 constrained parameter video
 *           stream
 *   0x03    ISO/IEC 11172 Audio
 *   0x04    ISO/IEC 13818-3 Audio
 *   0x05    ITU-T Rec. H.222.0 | ISO/IEC 13818-1
 *           private_sections
 *   0x06    ITU-T Rec. H.222.0 | ISO/IEC 13818-1 PES
 *           packets containing private data
 *   0x07    ISO/IEC 13522 MHEG
 *   0x08    ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A
 *           DSM CC
 *   0x09    ITU-T Rec. H.222.1
 *   0x0A    ISO/IEC 13818-6 type A
 *   0x0B    ISO/IEC 13818-6 type B
 *   0x0C    ISO/IEC 13818-6 type C
 *   0x0D    ISO/IEC 13818-6 type D
 *   0x0E    ISO/IEC 13818-1 auxiliary
 * 0x0F-0x7F ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved
 * 0x80-0xFF User Private
 */
enum TsMuxStreamType {
  TSMUX_ST_RESERVED                   = 0x00,
  TSMUX_ST_VIDEO_MPEG1                = 0x01,
  TSMUX_ST_VIDEO_MPEG2                = 0x02,
  TSMUX_ST_AUDIO_MPEG1                = 0x03,
  TSMUX_ST_AUDIO_MPEG2                = 0x04,
  TSMUX_ST_PRIVATE_SECTIONS           = 0x05,
  TSMUX_ST_PRIVATE_DATA               = 0x06,
  TSMUX_ST_MHEG                       = 0x07,
  TSMUX_ST_DSMCC                      = 0x08,
  TSMUX_ST_H222_1                     = 0x09,

  /* later extensions */
  TSMUX_ST_AUDIO_AAC                  = 0x0f,
  TSMUX_ST_VIDEO_MPEG4                = 0x10,
  TSMUX_ST_VIDEO_H264                 = 0x1b,

  /* private stream types */
  TSMUX_ST_PS_AUDIO_AC3               = 0x81,
  TSMUX_ST_PS_AUDIO_DTS               = 0x8a,
  TSMUX_ST_PS_AUDIO_LPCM              = 0x8b,
  TSMUX_ST_PS_DVD_SUBPICTURE          = 0xff,

  /* Non-standard definitions */
  TSMUX_ST_VIDEO_DIRAC                = 0xD1
};

enum TsMuxStreamState {
    TSMUX_STREAM_STATE_HEADER,
    TSMUX_STREAM_STATE_PACKET
};

/* TsMuxStream receives elementary streams for parsing.
 * Via the write_bytes() method, it can output a PES stream piecemeal */
struct TsMuxStream {
  TsMuxStreamState state;
  TsMuxPacketInfo pi;
  TsMuxStreamType stream_type;
  guint8 id; /* stream id */
  guint8 id_extended; /* extended stream id (13818-1 Amdt 2) */

  gboolean is_video_stream;

  /* List of data buffers available for writing out */
  GList *buffers;
  guint32 bytes_avail;

  /* Current data buffer being consumed */
  TsMuxStreamBuffer *cur_buffer;
  guint32 cur_buffer_consumed;

  TsMuxStreamBufferReleaseFunc buffer_release;

  guint32 pes_payload_size;
  guint32 cur_pes_payload_size;
  guint32 pes_bytes_written;

  /* PTS/DTS to write if the flags in the packet info are set */
  gint64 pts;
  gint64 dts;

  gint64 last_pts;
  gint64 last_dts;

  gint   pcr_ref;
  gint64 last_pcr;

  gint audio_sampling;
  gint audio_channels;
  gint audio_bitrate;
  gint video_framerate_num;
  gint video_framerate_den;
};

/* stream management */
TsMuxStream *	tsmux_stream_new 		(guint16 pid, TsMuxStreamType stream_type);
void 		tsmux_stream_free 		(TsMuxStream *stream);

guint16         tsmux_stream_get_pid            (TsMuxStream *stream);

void 		tsmux_stream_set_buffer_release_func 	(TsMuxStream *stream, 
       							 TsMuxStreamBufferReleaseFunc func);

/* Add a new buffer to the pool of available bytes. If pts or dts are not -1, they
 * indicate the PTS or DTS of the first access unit within this packet */
void 		tsmux_stream_add_data 		(TsMuxStream *stream, guint8 *data, guint len, 
       						 void *user_data, gint64 pts, gint64 dts,
                                                 gboolean random_access);

void* tsmux_stream_get_user_data(void* data);
void* tsmux_stream_head_user_data(TsMuxStream* stream);
void tsmux_stream_set_dts(void* data, gint64 dts, gboolean first);


void 		tsmux_stream_pcr_ref 		(TsMuxStream *stream);
void 		tsmux_stream_pcr_unref  	(TsMuxStream *stream);
gboolean	tsmux_stream_is_pcr 		(TsMuxStream *stream);

gboolean 	tsmux_stream_at_pes_start 	(TsMuxStream *stream);
void 		tsmux_stream_get_es_descrs 	(TsMuxStream *stream, guint8 *buf, guint32 *len);

gint 		tsmux_stream_bytes_in_buffer 	(TsMuxStream *stream);
gint 		tsmux_stream_bytes_avail 	(TsMuxStream *stream);
gboolean 	tsmux_stream_initialize_pes_packet (TsMuxStream *stream);
gboolean 	tsmux_stream_get_data 		(TsMuxStream *stream, guint8 *buf, guint len);

guint64 	tsmux_stream_get_pts 		(TsMuxStream *stream);

G_END_DECLS

#endif
