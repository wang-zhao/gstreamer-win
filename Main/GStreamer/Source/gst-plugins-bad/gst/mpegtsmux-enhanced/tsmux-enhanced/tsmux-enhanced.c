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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "tsmux-enhanced.h"
#include "tsmuxstream-enhanced.h"
#include "crc-enhanced.h"
#ifdef _DEBUG
#include <windows.h>
#endif

#include <stdio.h>


/* Maximum total data length for a PAT section is 1024 bytes, minus an 
* 8 byte header, then the length of each program entry is 32 bits, 
* then finally a 32 bit CRC. Thus the maximum number of programs in this mux
* is (1024 - 8 - 4) / 4 = 253 because it only supports single section PATs */
#define TSMUX_MAX_PROGRAMS 253

#define TSMUX_SECTION_HDR_SIZE 8

#define TSMUX_DEFAULT_NETWORK_ID 0x0001
#define TSMUX_DEFAULT_TS_ID 0x0001

/* HACK: We use a fixed buffering offset for the PCR at the moment - 
* this is the amount 'in advance' of the stream that the PCR sits.
* 1 second atm */
#define TSMUX_PCR_OFFSET (TSMUX_CLOCK_FREQ)

/* Times per second to write PCR */
#define TSMUX_DEFAULT_PCR_FREQ (27)

static gboolean tsmux_write_pat (TsMux * mux, TsMuxStream* stream);
static gboolean tsmux_write_pmt (TsMux * mux, TsMuxProgram * program, TsMuxStream* stream);

/**
* tsmux_new:
*
* Create a new muxer session.
*
* Returns: A new #TsMux object.
*/
TsMux *
tsmux_new (void)
{
  TsMux *mux;

  mux = g_slice_new0 (TsMux);

  mux->transport_id = TSMUX_DEFAULT_TS_ID;

  mux->next_pgm_no = TSMUX_START_PROGRAM_ID;
  mux->next_pmt_pid = TSMUX_START_PMT_PID;
  mux->next_stream_pid = TSMUX_START_ES_PID;

  mux->pat_changed = TRUE;
  mux->last_pat_ts = -1;
  mux->pat_interval = TSMUX_DEFAULT_PAT_INTERVAL;
  mux->data_pos = 0;
  mux->bitrate = -1;
  mux->pending = 0;

  return mux;
}

/**
* tsmux_set_write_func:
* @mux: a #TsMux
* @func: a user callback function
* @user_data: user data passed to @func
*
* Set the callback function and user data to be called when @mux has output to
* produce. @user_data will be passed as user data in @func.
*/
void
tsmux_set_write_func (TsMux * mux, TsMuxWriteFunc func, void *user_data)
{
  g_return_if_fail (mux != NULL);

  mux->write_func = func;
  mux->write_func_data = user_data;
}

/**
* tsmux_set_pat_interval:
* @mux: a #TsMux
* @freq: a new PAT interval
*
* Set the interval (in cycles of the 90kHz clock) for writing out the PAT table.
*
* Many transport stream clients might have problems if the PAT table is not
* inserted in the stream at regular intervals, especially when initially trying
* to figure out the contents of the stream.
*/
void
tsmux_set_pat_interval (TsMux * mux, guint freq)
{
  g_return_if_fail (mux != NULL);

  mux->pat_interval = freq;
}

/**
* tsmux_get_pat_interval:
* @mux: a #TsMux
*
* Get the configured PAT interval. See also tsmux_set_pat_interval().
*
* Returns: the configured PAT interval
*/
guint
tsmux_get_pat_interval (TsMux * mux)
{
  g_return_val_if_fail (mux != NULL, 0);

  return mux->pat_interval;
}

/**
* tsmux_free:
* @mux: a #TsMux
*
* Free all resources associated with @mux. After calling this function @mux can
* not be used anymore.
*/
void
tsmux_free (TsMux * mux)
{
  GList *cur;

  g_return_if_fail (mux != NULL);

  /* Free all programs */
  for (cur = g_list_first (mux->programs); cur != NULL; cur = g_list_next (cur)) {
    TsMuxProgram *program = (TsMuxProgram *) cur->data;

    tsmux_program_free (program);
  }
  g_list_free (mux->programs);

  /* Free all streams */
  for (cur = g_list_first (mux->streams); cur != NULL; cur = g_list_next (cur)) {
    TsMuxStream *stream = (TsMuxStream *) cur->data;

    tsmux_stream_free (stream);
  }
  g_list_free (mux->streams);

  g_slice_free (TsMux, mux);
}

/**
* tsmux_program_new:
* @mux: a #TsMux
*
* Create a new program in the mising session @mux.
* 
* Returns: a new #TsMuxProgram or %NULL when the maximum number of programs has
* been reached.
*/
TsMuxProgram *
tsmux_program_new (TsMux * mux)
{
  TsMuxProgram *program;

  g_return_val_if_fail (mux != NULL, NULL);

  /* Ensure we have room for another program */
  if (mux->nb_programs == TSMUX_MAX_PROGRAMS)
    return NULL;

  program = g_slice_new0 (TsMuxProgram);

  program->pmt_changed = TRUE;
  program->last_pmt_ts = -1;
  program->pmt_interval = TSMUX_DEFAULT_PMT_INTERVAL;

  program->pgm_number = mux->next_pgm_no++;
  program->pmt_pid = mux->next_pmt_pid++;
  program->pcr_stream = NULL;
  program->last_pcr = -1;

  program->streams = g_array_sized_new (FALSE, TRUE, sizeof (TsMuxStream *), 1);

  mux->programs = g_list_prepend (mux->programs, program);
  mux->nb_programs++;
  mux->pat_changed = TRUE;

  return program;
}

/**
* tsmux_set_pmt_interval:
* @program: a #TsMuxProgram
* @freq: a new PMT interval
*
* Set the interval (in cycles of the 90kHz clock) for writing out the PMT table.
*
* Many transport stream clients might have problems if the PMT table is not
* inserted in the stream at regular intervals, especially when initially trying
* to figure out the contents of the stream.
*/
void
tsmux_set_pmt_interval (TsMuxProgram * program, guint freq)
{
  g_return_if_fail (program != NULL);

  program->pmt_interval = freq;
}

/**
* tsmux_get_pmt_interval:
* @program: a #TsMuxProgram
*
* Get the configured PMT interval. See also tsmux_set_pmt_interval().
*
* Returns: the configured PMT interval
*/
guint
tsmux_get_pmt_interval (TsMuxProgram * program)
{
  g_return_val_if_fail (program != NULL, 0);

  return program->pmt_interval;
}

/**
* tsmux_program_add_stream:
* @program: a #TsMuxProgram
* @stream: a #TsMuxStream
*
* Add @stream to @program.
*/
void
tsmux_program_add_stream (TsMuxProgram * program, TsMuxStream * stream)
{
  g_return_if_fail (program != NULL);
  g_return_if_fail (stream != NULL);

  program->nb_streams++;
  g_array_append_val (program->streams, stream);
  program->pmt_changed = TRUE;
}

/**
* tsmux_program_set_pcr_stream:
* @program: a #TsMuxProgram
* @stream: a #TsMuxStream
*
* Set @stream as the PCR stream for @program, overwriting the previously
* configured PCR stream. When @stream is NULL, program will have no PCR stream
* configured.
*/
void
tsmux_program_set_pcr_stream (TsMuxProgram * program, TsMuxStream * stream)
{
  g_return_if_fail (program != NULL);

  if (program->pcr_stream == stream)
    return;

  if (program->pcr_stream != NULL)
    tsmux_stream_pcr_unref (program->pcr_stream);
  if (stream)
    tsmux_stream_pcr_ref (stream);
  program->pcr_stream = stream;

  program->pmt_changed = TRUE;
}

/**
* tsmux_get_new_pid:
* @mux: a #TsMux
*
* Get a new free PID.
*
* Returns: a new free PID.
*/
guint16
tsmux_get_new_pid (TsMux * mux)
{
  g_return_val_if_fail (mux != NULL, -1);

  /* FIXME: It's possible that this next PID is already taken if a 
  * specific PID was requested earlier. We should find a free PID */
  return mux->next_stream_pid++;
}

void tsmux_suggest_gop_bitrate(TsMux* mux, gint32 bitrate)
{
#ifdef _DEBUG
  char msg[64];
#endif
  bitrate *= 1.05f;
  if( bitrate > mux->bitrate )
  {
    mux->bitrate = (gint)((2LL * mux->bitrate *  bitrate )/ (mux->bitrate + bitrate));
  }
#ifdef _DEBUG

  _snprintf(msg, 64, "gopbitrate:%d\r\n",  mux->bitrate);
  OutputDebugString(msg);
#endif


}

void  tsmux_suggest_bitrate(TsMux* mux, gint32 bitrate)
{
  if( bitrate > mux->bitrate )
    mux->bitrate = bitrate;
}
/**
* tsmux_create_stream:
* @mux: a #TsMux
* @stream_type: a #TsMuxStreamType
* @pid: the PID of the new stream.
*
* Create a new stream of @stream_type in the muxer session @mux.
*
* When @pid is set to #TSMUX_PID_AUTO, a new free PID will automatically
* be allocated for the new stream.
*
* Returns: a new #TsMuxStream.
*/
TsMuxStream *
tsmux_create_stream (TsMux * mux, TsMuxStreamType stream_type, guint16 pid)
{
  TsMuxStream *stream;
  guint16 new_pid;

  g_return_val_if_fail (mux != NULL, NULL);

  if (pid == TSMUX_PID_AUTO) {
    new_pid = tsmux_get_new_pid (mux);
  } else {
    new_pid = pid & 0x1FFF;
  }

  /* Ensure we're not creating a PID collision */
  if (tsmux_find_stream (mux, new_pid))
    return NULL;

  stream = tsmux_stream_new (new_pid, stream_type);

  mux->streams = g_list_prepend (mux->streams, stream);
  mux->nb_streams++;

  return stream;
}

/**
* tsmux_find_stream:
* @mux: a #TsMux
* @pid: the PID to find.
*
* Find the stream associated wih PID.
*
* Returns: a #TsMuxStream with @pid or NULL when the stream was not found.
*/
TsMuxStream *
tsmux_find_stream (TsMux * mux, guint16 pid)
{
  TsMuxStream *found = NULL;
  GList *cur;

  g_return_val_if_fail (mux != NULL, NULL);

  for (cur = g_list_first (mux->streams); cur != NULL; cur = g_list_next (cur)) {
    TsMuxStream *stream = (TsMuxStream *) cur->data;

    if (tsmux_stream_get_pid (stream) == pid) {
      found = stream;
      break;
    }
  }
  return found;
}


static gboolean
tsmux_packet_out (TsMux * mux, TsMuxStream* stream)
{
  gboolean ret;
  if (G_UNLIKELY (mux->write_func == NULL))
    return TRUE;

  ret =mux->write_func (mux->packet_buf, TSMUX_PACKET_LENGTH,
    mux->write_func_data, mux->new_pcr, stream);

  if( ret )
  {
    mux->data_pos += TSMUX_PACKET_LENGTH;
  }

  if(G_UNLIKELY(mux->new_pcr > 0) && G_LIKELY(stream) )
    stream->last_pcr = mux->new_pcr;

  return ret;
}

static gboolean 
tsmux_pending_packet(TsMux* mux)
{
  static gint8 header[] = {0x47, 0x1F,0xFF, 0x11};

  memset(mux->packet_buf, 0xFF, TSMUX_PACKET_LENGTH);
  memcpy(mux->packet_buf, &header[0], 4);

  return tsmux_packet_out(mux, 0);
}

/*
* adaptation_field() {
*   adaptation_field_length                              8 uimsbf
*   if(adaptation_field_length >0) {
*     discontinuity_indicator                            1 bslbf
*     random_access_indicator                            1 bslbf
*     elementary_stream_priority_indicator               1 bslbf
*     PCR_flag                                           1 bslbf
*     OPCR_flag                                          1 bslbf
*     splicing_point_flag                                1 bslbf
*     transport_private_data_flag                        1 bslbf
*     adaptation_field_extension_flag                    1 bslbf
*     if(PCR_flag == '1') {
*       program_clock_reference_base                    33 uimsbf
*       reserved                                         6 bslbf
*       program_clock_reference_extension                9 uimsbf
*     }
*     if(OPCR_flag == '1') {
*       original_program_clock_reference_base           33 uimsbf
*       reserved                                         6 bslbf
*       original_program_clock_reference_extension       9 uimsbf
*     }
*     if (splicing_point_flag == '1') {
*       splice_countdown                                 8 tcimsbf
*     }
*     if(transport_private_data_flag == '1') {
*       transport_private_data_length                    8 uimsbf
*       for (i=0; i<transport_private_data_length;i++){
*         private_data_byte                              8 bslbf
*       }
*     }
*     if (adaptation_field_extension_flag == '1' ) {
*       adaptation_field_extension_length                8 uimsbf
*       ltw_flag                                         1 bslbf
*       piecewise_rate_flag                              1 bslbf
*       seamless_splice_flag                             1 bslbf
*       reserved                                         5 bslbf
*       if (ltw_flag == '1') {
*         ltw_valid_flag                                 1 bslbf
*         ltw_offset                                    15 uimsbf
*       }
*       if (piecewise_rate_flag == '1') {
*         reserved                                       2 bslbf
*         piecewise_rate                                22 uimsbf
*       }
*       if (seamless_splice_flag == '1'){
*         splice_type                                    4 bslbf
*         DTS_next_AU[32..30]                            3 bslbf
*         marker_bit                                     1 bslbf
*         DTS_next_AU[29..15]                           15 bslbf
*         marker_bit                                     1 bslbf
*         DTS_next_AU[14..0]                            15 bslbf
*         marker_bit                                     1 bslbf
*       }
*       for ( i=0;i<N;i++) {
*         reserved                                       8 bslbf
*       }
*     }
*     for (i=0;i<N;i++){
*       stuffing_byte                                    8 bslbf
*     }
*   }
* }
*/
static gboolean
tsmux_write_adaptation_field (guint8 * buf,
                              TsMuxPacketInfo * pi, guint8 min_length, guint8 * written)
{
  guint8 pos = 2;
  guint8 flags = 0;

  g_assert (min_length <= TSMUX_PAYLOAD_LENGTH);

  /* Write out all the fields from the packet info only if the 
  * user set the flag to request the adaptation field - if the flag
  * isn't set, we're just supposed to write stuffing bytes */
  if (pi->flags & TSMUX_PACKET_FLAG_ADAPTATION) {
    TS_DEBUG ("writing adaptation fields");
    if (pi->flags & TSMUX_PACKET_FLAG_DISCONT)
      flags |= 0x80;
    if (pi->flags & TSMUX_PACKET_FLAG_RANDOM_ACCESS)
      flags |= 0x40;
    if (pi->flags & TSMUX_PACKET_FLAG_PRIORITY)
      flags |= 0x20;
    if (pi->flags & TSMUX_PACKET_FLAG_WRITE_PCR) {
      guint64 pcr_base;
      guint32 pcr_ext;

      pcr_base = (pi->pcr / 300);
      pcr_ext = (pi->pcr % 300);

      flags |= 0x10;
      TS_DEBUG ("Writing PCR %" G_GUINT64_FORMAT " + ext %u", pcr_base,
        pcr_ext);
      buf[pos++] = (pcr_base >> 25) & 0xff;
      buf[pos++] = (pcr_base >> 17) & 0xff;
      buf[pos++] = (pcr_base >> 9) & 0xff;
      buf[pos++] = (pcr_base >> 1) & 0xff;
      buf[pos++] = ((pcr_base << 7) & 0x80) | ((pcr_ext >> 8) & 0x01);
      buf[pos++] = (pcr_ext) & 0xff;
    }
    if (pi->flags & TSMUX_PACKET_FLAG_WRITE_OPCR) {
      guint64 opcr_base;
      guint32 opcr_ext;

      opcr_base = (pi->opcr / 300);
      opcr_ext = (pi->opcr % 300);

      flags |= 0x08;
      TS_DEBUG ("Writing OPCR");
      buf[pos++] = (opcr_base >> 25) & 0xff;
      buf[pos++] = (opcr_base >> 17) & 0xff;
      buf[pos++] = (opcr_base >> 9) & 0xff;
      buf[pos++] = (opcr_base >> 1) & 0xff;
      buf[pos++] = ((opcr_base << 7) & 0x80) | ((opcr_ext >> 8) & 0x01);
      buf[pos++] = (opcr_ext) & 0xff;
    }
    if (pi->flags & TSMUX_PACKET_FLAG_WRITE_SPLICE) {
      flags |= 0x04;
      buf[pos++] = pi->splice_countdown;
    }
    if (pi->private_data_len > 0) {
      flags |= 0x02;
      /* Private data to write, ensure we have enough room */
      if ((1 + pi->private_data_len) > (TSMUX_PAYLOAD_LENGTH - pos))
        return FALSE;
      buf[pos++] = pi->private_data_len;
      memcpy (&(buf[pos]), pi->private_data, pi->private_data_len);
      pos += pi->private_data_len;
      TS_DEBUG ("%u bytes of private data", pi->private_data_len);
    }
    if (pi->flags & TSMUX_PACKET_FLAG_WRITE_ADAPT_EXT) {
      flags |= 0x01;
      TS_DEBUG ("FIXME: write Adaptation extension");
      /* Write an empty extension for now */
      buf[pos++] = 1;
      buf[pos++] = 0;
    }
  }
  /* Write the flags at the start */
  buf[1] = flags;

  /* Stuffing bytes if needed */
  while (pos < min_length)
    buf[pos++] = 0xff;

  /* Write the adaptation field length, which doesn't include its own byte */
  buf[0] = pos - 1;

  if (written)
    *written = pos;

  return TRUE;
}

static gboolean
tsmux_write_ts_header (guint8 * buf, TsMuxPacketInfo * pi,
                       guint * payload_len_out, guint * payload_offset_out)
{
  guint8 *tmp;
  guint8 adaptation_flag;
  guint8 adapt_min_length = 0;
  guint8 adapt_len = 0;
  guint payload_len;
  gboolean write_adapt = FALSE;

  /* Sync byte */
  buf[0] = TSMUX_SYNC_BYTE;

  TS_DEBUG ("PID 0x%04x, counter = 0x%01x, %u bytes avail", pi->pid,
    pi->packet_count & 0x0f, pi->stream_avail);

  /* 3 bits: 
  *   transport_error_indicator
  *   payload_unit_start_indicator
  *   transport_priority: (00)
  * 13 bits: PID
  */
  tmp = buf + 1;
  if (pi->packet_start_unit_indicator) {
    tsmux_put16 (&tmp, 0x4000 | pi->pid);
  } else
    tsmux_put16 (&tmp, pi->pid);

  /* 2 bits: scrambling_control (NOT SUPPORTED) (00)
  * 2 bits: adaptation field control (1x has_adaptation_field | x1 has_payload)
  * 4 bits: continuity counter (xxxx)
  */
  adaptation_flag = pi->packet_count & 0x0f;

  if (pi->flags & TSMUX_PACKET_FLAG_ADAPTATION) {
    write_adapt = TRUE;
  }

  if (pi->stream_avail < TSMUX_PAYLOAD_LENGTH) {
    /* Need an adaptation field regardless for stuffing */
    adapt_min_length = TSMUX_PAYLOAD_LENGTH - pi->stream_avail;
    write_adapt = TRUE;
  }

  if (write_adapt) {
    gboolean res;

    /* Flag the adaptation field presence */
    adaptation_flag |= 0x20;
    res = tsmux_write_adaptation_field (buf + TSMUX_HEADER_LENGTH,
      pi, adapt_min_length, &adapt_len);
    if (G_UNLIKELY (res == FALSE))
      return FALSE;

    /* Should have written at least the number of bytes we requested */
    g_assert (adapt_len >= adapt_min_length);
  }

  /* The amount of packet data we wrote is the remaining space after
  * the adaptation field */
  *payload_len_out = payload_len = TSMUX_PAYLOAD_LENGTH - adapt_len;
  *payload_offset_out = TSMUX_HEADER_LENGTH + adapt_len;

  /* Now if we are going to write out some payload, flag that fact */
  if (payload_len > 0 && pi->stream_avail > 0) {
    /* Flag the presence of a payload */
    adaptation_flag |= 0x10;

    /* We must have enough data to fill the payload, or some calculation
    * went wrong */
    g_assert (payload_len <= pi->stream_avail);

    /* Packet with payload, increment the continuity counter */
    pi->packet_count++;
  }

  /* Write the byte of transport_scrambling_control, adaptation_field_control 
  * + continuity counter out */
  buf[3] = adaptation_flag;


  if (write_adapt) {
    TS_DEBUG ("Adaptation field of size >= %d + %d bytes payload",
      adapt_len, payload_len);
  } else {
    TS_DEBUG ("Payload of %d bytes only", payload_len);
  }

  return TRUE;
}

/**
* tsmux_write_stream_packet:
* @mux: a #TsMux
* @stream: a #TsMuxStream
*
* Write a packet of @stream.
*
* Returns: TRUE if the packet could be written.
*/
gboolean
tsmux_write_stream_packet (TsMux * mux, TsMuxStream * stream)
{
  guint payload_len, payload_offs;
  TsMuxPacketInfo *pi = &stream->pi;
  gboolean res;
  gboolean write_pat;
  gboolean is_pcr_stream;
  gint64 cur_dts;
  gint64 cur_pts;
  gint64 cur_pcr;
  gint64 last_pes_dts;
  gint64 last_pes_pos;
  gint64 new_pes_dts;
  GList *cur;

  g_return_val_if_fail (mux != NULL, FALSE);
  g_return_val_if_fail (stream != NULL, FALSE);
 
  mux->new_pcr = -1;

  cur_dts = tsmux_stream_get_dts (stream);
  cur_pts = tsmux_stream_get_pts (stream);

  // 如果没有dts,使用pts
  if( cur_dts < 0)
    cur_dts = cur_pts;

  is_pcr_stream = tsmux_stream_is_pcr(stream);
  pi->packet_start_unit_indicator = tsmux_stream_at_pes_start (stream);


  if (is_pcr_stream) 
  {
    cur_pcr = 0; 

    /* check if we need to rewrite pat */
    if (mux->last_pat_ts == -1 || mux->pat_changed)
      write_pat = TRUE;
    else if (cur_dts >= mux->last_pat_ts + mux->pat_interval)
      write_pat = TRUE;
    else
      write_pat = FALSE;

    if (write_pat) {
      mux->last_pat_ts = cur_dts;
      if (!tsmux_write_pat (mux, stream))
        return FALSE;
    }

    /* check if we need to rewrite any of the current pmts */
    for (cur = g_list_first (mux->programs); cur != NULL;
      cur = g_list_next (cur)) {
        TsMuxProgram *program = (TsMuxProgram *) cur->data;
        gboolean write_pmt;

        if (program->last_pmt_ts == -1 || program->pmt_changed)
          write_pmt = TRUE;
        else if (cur_dts >= program->last_pmt_ts + program->pmt_interval)
          write_pmt = TRUE;
        else
          write_pmt = FALSE;

        if (write_pmt) {
          program->last_pmt_ts = cur_dts;
          if (!tsmux_write_pmt (mux, program, stream))
            return FALSE;
        }
    }

    last_pes_dts = stream->pes_dts;
    last_pes_pos = stream->pes_header_pos;

    new_pes_dts = -1;
    
    // 如果是一个新的PES开始，则记录下开始位置，用于后来计算，保证PCR不会比PTS/DTS小
    if( stream->pes_start )
    {
      new_pes_dts = cur_dts;
      stream->pes_header_pos = stream->pes_header_pos2;
      stream->pes_dts = cur_dts;
      stream->pes_start = FALSE;
    }

    if( pi->packet_start_unit_indicator )
    {
      stream->pes_header_pos2 = mux->data_pos;
      stream->pes_start = TRUE;
    }

    // 如果还从来没有输出过PCR，则输出第一个PCR。
    if( stream->last_pcr_pos < 0 )
    {
      if (cur_dts != -1 )
        if(cur_dts >= TSMUX_PCR_OFFSET)
          cur_pcr = (cur_dts - TSMUX_PCR_OFFSET) * 300;
    }
    // 如果已经输出过PCR，计算PCR间隔，调整输出码率
    else
    {
      if( mux->bitrate > 0 )
      {
        gint64 pcr_diff = (mux->data_pos - stream->last_pcr_pos) * 8 * TSMUX_SYS_CLOCK_FREQ/ mux->bitrate ;
        gint64 dts_diff = (mux->data_pos - stream->pes_header_pos) *8 * TSMUX_CLOCK_FREQ/ mux->bitrate ;
        gint64 pcr_dts_diff = stream->last_pcr + pcr_diff - (cur_dts + dts_diff)*300;
        cur_pcr = stream->last_pcr + pcr_diff;

        // 如果PCR值大于DTS值，说明码率需要调整了，根据PCR-DTS差值，调整码率
        //if( pcr_dts_diff > -TSMUX_PCR_OFFSET*300 )
        //{
        // // if( pcr_dts_diff > 0 )
        //  {
        //    cur_pcr = (cur_dts + dts_diff - TSMUX_PCR_OFFSET)*300;
        //    cur_pcr = max(0, cur_pcr);
        //  }

        //  if(cur_pcr - stream->last_pcr > TSMUX_SYS_CLOCK_FREQ / TSMUX_DEFAULT_PCR_FREQ)
        //  {
        //    if( stream->last_pcr  != cur_pcr )
        //    {
        //      gint64 newbitrate =  (mux->data_pos - stream->last_pcr_pos) *(8 * TSMUX_SYS_CLOCK_FREQ) / (cur_pcr - stream->last_pcr) ;
        //      //gint newbitrate = (gint)((stream->last_pcr_pos-stream->pes_header_pos) * 8 * TSMUX_SYS_CLOCK_FREQ / ( stream->last_pcr - cur_dts*300));

        //      // 使用新的Bitrate
        //      if( newbitrate > mux->bitrate )
        //      {
        //       mux->bitrate = (gint)((2*newbitrate*mux->bitrate)/(newbitrate+mux->bitrate));
        //        // 保证新的Bitrate不会超出原bitrate太多
        // /*       if( newbitrate/2 > mux->bitrate )
        //          mux->bitrate *= 2;
        //        else
        //          mux->bitrate = newbitrate;*/
        //      }
        //    }
        //  }
        //}
        //// 码率正常，判断是否需要输出PCR
        //else
        { 
          // 如果需要输出PCR，对码率进行判断，是否需要填充包
          if( pcr_diff > TSMUX_SYS_CLOCK_FREQ / TSMUX_DEFAULT_PCR_FREQ)
          {
            if( (cur_dts + dts_diff)*300 - cur_pcr > TSMUX_PCR_OFFSET*300*2)
            {
              // 计算填充包的个数
              // 计算需要填充的时长
              gint64 pendingdur = (cur_dts + dts_diff)*300 - cur_pcr - (TSMUX_PCR_OFFSET*300*2);
              // 计算需要填充的数据
              gint64 datalen = pendingdur * mux->bitrate / TSMUX_SYS_CLOCK_FREQ / 8;
              // 计算填充包的个数
              if( datalen > TSMUX_PACKET_LENGTH)
              {
                mux->pending = datalen / TSMUX_PACKET_LENGTH;
              }
            }
          }
        }
      }
      // 如果没有指定有效的码率值，根据DTS估算实际码率
      else
      {
        if( last_pes_dts > 0 && new_pes_dts > 0 && last_pes_dts != new_pes_dts )
        {
          gint bitrate = (gint)((stream->pes_header_pos - last_pes_pos) * 8 * TSMUX_CLOCK_FREQ / (new_pes_dts-last_pes_dts));
          bitrate = bitrate > 0 ? bitrate : -bitrate;

          if( bitrate > 0 )
          {
            // 适量增加bitrate带宽
            bitrate = (gint)(bitrate * 1.2f);
            mux->bitrate = bitrate;
          }
        }
      }
    }

    /* Need to decide whether to write a new PCR in this packet */
    if (stream->last_pcr == -1 || (cur_pcr - stream->last_pcr > (TSMUX_SYS_CLOCK_FREQ / TSMUX_DEFAULT_PCR_FREQ))) 
    {
      stream->pi.flags |=
        TSMUX_PACKET_FLAG_ADAPTATION | TSMUX_PACKET_FLAG_WRITE_PCR;

/*#ifdef _DEBUG
      {
        char msg[128];
        _snprintf(msg,128, "%I64d,%I64d,%I64d,%I64d,%I64d\r\n", cur_pcr, cur_pts*300, cur_dts*300, cur_pcr-cur_pts*300,cur_pcr-cur_dts*300);
        OutputDebugString(msg);
      }
#endif*/ 

      stream->pi.pcr = cur_pcr;
      stream->last_pcr = cur_pcr;
      mux->new_pcr = cur_pcr;

      stream->last_pcr_pos = mux->data_pos;

    }  
  }

  if (pi->packet_start_unit_indicator)
    tsmux_stream_initialize_pes_packet (stream);
  pi->stream_avail = tsmux_stream_bytes_avail (stream);

  if (!tsmux_write_ts_header (mux->packet_buf, pi, &payload_len, &payload_offs))
    return FALSE;

  if (!tsmux_stream_get_data (stream, mux->packet_buf + payload_offs,
    payload_len))
    return FALSE;

  res = tsmux_packet_out (mux, stream);

  /* Reset all dynamic flags */
  stream->pi.flags &= TSMUX_PACKET_FLAG_PES_FULL_HEADER;

  {
    // 最多连续输出5个pending packet
    gint pending_packet = 0;
    while(mux->pending  > 0 && pending_packet < 5)
    {
      tsmux_pending_packet(mux);
      mux->pending -- ;
      pending_packet ++;

    }  
  }

  return res;
}

/**
* tsmux_program_free:
* @program: a #TsMuxProgram
*
* Free the resources of @program. After this call @program can not be used
* anymore.
*/
void
tsmux_program_free (TsMuxProgram * program)
{
  g_return_if_fail (program != NULL);

  g_array_free (program->streams, TRUE);
  g_slice_free (TsMuxProgram, program);
}

static gboolean
tsmux_write_section (TsMux * mux, TsMuxSection * section, TsMuxStream* stream)
{
  guint8 *cur_in;
  guint payload_remain;
  guint payload_len, payload_offs;
  TsMuxPacketInfo *pi;

  pi = &section->pi;

  pi->packet_start_unit_indicator = TRUE;

  cur_in = section->data;
  payload_remain = pi->stream_avail;

  while (payload_remain > 0) {
    if (pi->packet_start_unit_indicator) {
      /* Need to write an extra single byte start pointer */
      pi->stream_avail++;

      if (!tsmux_write_ts_header (mux->packet_buf, pi,
        &payload_len, &payload_offs)) {
          pi->stream_avail--;
          return FALSE;
      }
      pi->stream_avail--;

      /* Write the pointer byte */
      mux->packet_buf[payload_offs] = 0x00;

      payload_offs++;
      payload_len--;
      pi->packet_start_unit_indicator = FALSE;
    } else {
      if (!tsmux_write_ts_header (mux->packet_buf, pi,
        &payload_len, &payload_offs))
        return FALSE;
    }

    TS_DEBUG ("Outputting %d bytes to section. %d remaining after",
      payload_len, payload_remain - payload_len);

    memcpy (mux->packet_buf + payload_offs, cur_in, payload_len);

    cur_in += payload_len;
    payload_remain -= payload_len;

    if (G_UNLIKELY (!tsmux_packet_out (mux, stream))) {
      mux->new_pcr = -1;
      return FALSE;
    }
    mux->new_pcr = -1;
  }

  return TRUE;
}

static void
tsmux_write_section_hdr (guint8 * pos, guint8 table_id, guint16 len,
                         guint16 id, guint8 version, guint8 section_nr, guint8 last_section_nr)
{
  /* The length passed is the total length of the section, but we're not 
  * supposed to include the first 3 bytes of the header in the count */
  len -= 3;

  /* 1 byte table identifier */
  *pos++ = table_id;
  /* section_syntax_indicator = '0' | '0' | '11' reserved bits | (len >> 8) */
  tsmux_put16 (&pos, 0xB000 | len);
  /* 2 bytes transport/program id */
  tsmux_put16 (&pos, id);

  /* '11' reserved | version 'xxxxxx' | 'x' current_next */
  *pos++ = 0xC0 | ((version & 0x1F) << 1) | 0x01;
  *pos++ = section_nr;
  *pos++ = last_section_nr;
}

static gboolean
tsmux_write_pat (TsMux * mux, TsMuxStream* stream)
{
  GList *cur;
  TsMuxSection *pat = &mux->pat;

  if (mux->pat_changed) {
    /* program_association_section ()
    * table_id                                   8   uimsbf
    * section_syntax_indicator                   1   bslbf
    * '0'                                        1   bslbf
    * reserved                                   2   bslbf
    * section_length                            12   uimsbf
    * transport_stream_id                       16   uimsbf
    * reserved                                   2   bslbf
    * version_number                             5   uimsbf
    * current_next_indicator                     1   bslbf
    * section_number                             8   uimsbf 
    * last_section_number                        8   uimsbf
    * for (i = 0; i < N; i++) {
    *    program_number                         16   uimsbf
    *    reserved                                3   bslbf
    *    network_PID_or_program_map_PID         13   uimbsf
    * }
    * CRC_32                                    32   rbchof
    */
    guint8 *pos;
    guint32 crc;

    /* Prepare the section data after the section header */
    pos = pat->data + TSMUX_SECTION_HDR_SIZE;

    for (cur = g_list_first (mux->programs); cur != NULL;
      cur = g_list_next (cur)) {
        TsMuxProgram *program = (TsMuxProgram *) cur->data;

        tsmux_put16 (&pos, program->pgm_number);
        tsmux_put16 (&pos, 0xE000 | program->pmt_pid);
    }

    /* Measure the section length, include extra 4 bytes for CRC below */
    pat->pi.stream_avail = pos - pat->data + 4;

    /* Go back and write the header now that we know the final length.
    * table_id = 0 for PAT */
    tsmux_write_section_hdr (pat->data, 0x00, pat->pi.stream_avail,
      mux->transport_id, mux->pat_version, 0, 0);

    /* Calc and output CRC for data bytes, not including itself */
    crc = calc_crc32 (pat->data, pat->pi.stream_avail - 4);
    tsmux_put32 (&pos, crc);

    TS_DEBUG ("PAT has %d programs, is %u bytes",
      mux->nb_programs, pat->pi.stream_avail);
    mux->pat_changed = FALSE;
    mux->pat_version++;
  }

  return tsmux_write_section (mux, pat,stream);
}

static gboolean
tsmux_write_pmt (TsMux * mux, TsMuxProgram * program, TsMuxStream* stream)
{
  TsMuxSection *pmt = &program->pmt;

  if (program->pmt_changed) {
    /* program_association_section ()
    * table_id                                   8   uimsbf
    * section_syntax_indicator                   1   bslbf
    * '0'                                        1   bslbf
    * reserved                                   2   bslbf
    * section_length                            12   uimsbf
    * program_id                                16   uimsbf
    * reserved                                   2   bslbf
    * version_number                             5   uimsbf
    * current_next_indicator                     1   bslbf
    * section_number                             8   uimsbf 
    * last_section_number                        8   uimsbf
    * reserved                                   3   bslbf
    * PCR_PID                                   13   uimsbf
    * reserved                                   4   bslbf
    * program_info_length                       12   uimsbf
    * for (i = 0; i < N; i++)
    *   descriptor ()
    *
    * for (i = 0; i < N1; i++) {
    *    stream_type                             8   uimsbf
    *    reserved                                3   bslbf
    *    elementary_PID                         13   uimbsf
    *    reserved                                4   bslbf
    *    ES_info_length                         12   uimbsf
    *    for (i = 0; i < N1; i++) {
    *      descriptor ();
    *    }
    * }
    * CRC_32                                    32   rbchof
    */
    guint8 *pos;
    guint32 crc;
    guint i;

    /* Prepare the section data after the basic section header */
    pos = pmt->data + TSMUX_SECTION_HDR_SIZE;

    if (program->pcr_stream == NULL)
      tsmux_put16 (&pos, 0xFFFF);
    else
      tsmux_put16 (&pos, 0xE000 | tsmux_stream_get_pid (program->pcr_stream));

    /* 4 bits reserved, 12 bits program_info_length, descriptor : HDMV */
    tsmux_put16 (&pos, 0xF00C);
    tsmux_put16 (&pos, 0x0504);
    tsmux_put16 (&pos, 0x4844);
    tsmux_put16 (&pos, 0x4D56);
    tsmux_put16 (&pos, 0x8804);
    tsmux_put16 (&pos, 0x0FFF);
    tsmux_put16 (&pos, 0xFCFC);

    /* Write out the entries */
    for (i = 0; i < program->nb_streams; i++) {
      TsMuxStream *stream = g_array_index (program->streams, TsMuxStream *, i);
      guint32 es_info_len;

      /* FIXME: Use API to retrieve this from the stream */
      *pos++ = stream->stream_type;
      tsmux_put16 (&pos, 0xE000 | tsmux_stream_get_pid (stream));

      /* Write any ES descriptors needed */
      tsmux_stream_get_es_descrs (stream, mux->es_info_buf, &es_info_len);
      tsmux_put16 (&pos, 0xF000 | es_info_len);

      if (es_info_len > 0) {
        TS_DEBUG ("Writing descriptor of len %d for PID 0x%04x",
          es_info_len, tsmux_stream_get_pid (stream));
        if (G_UNLIKELY (pos + es_info_len >=
          pmt->data + TSMUX_MAX_SECTION_LENGTH))
          return FALSE;

        memcpy (pos, mux->es_info_buf, es_info_len);
        pos += es_info_len;
      }
    }

    /* Include the CRC in the byte count */
    pmt->pi.stream_avail = pos - pmt->data + 4;

    /* Go back and patch the pmt_header now that we know the length.
    * table_id = 2 for PMT */
    tsmux_write_section_hdr (pmt->data, 0x02, pmt->pi.stream_avail,
      program->pgm_number, program->pmt_version, 0, 0);

    /* Calc and output CRC for data bytes, 
    * but not counting the CRC bytes this time */
    crc = calc_crc32 (pmt->data, pmt->pi.stream_avail - 4);
    tsmux_put32 (&pos, crc);

    TS_DEBUG ("PMT for program %d has %d streams, is %u bytes",
      program->pgm_number, program->nb_streams, pmt->pi.stream_avail);

    pmt->pi.pid = program->pmt_pid;
    program->pmt_changed = FALSE;
    program->pmt_version++;
  }

  return tsmux_write_section (mux, pmt,stream);
}
