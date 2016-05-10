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

#ifndef __TSMUX_COMMON_H__
#define __TSMUX_COMMON_H__

#include <glib.h>
#include <gst/gst.h>
#undef TS_DEBUG_ON

G_BEGIN_DECLS

#define TSMUX_SYNC_BYTE 0x47
#define TSMUX_PACKET_LENGTH 188
#define TSMUX_HEADER_LENGTH 4
#define TSMUX_PAYLOAD_LENGTH (TSMUX_PACKET_LENGTH - TSMUX_HEADER_LENGTH)

#define TSMUX_MIN_ES_DESC_LEN 8

/* Frequency for PCR representation */
#define TSMUX_SYS_CLOCK_FREQ (27000000L)
/* Frequency for PTS values */
#define TSMUX_CLOCK_FREQ (TSMUX_SYS_CLOCK_FREQ / 300)

#define TSMUX_PACKET_FLAG_NONE            (0)
#define TSMUX_PACKET_FLAG_ADAPTATION      (1 << 0)
#define TSMUX_PACKET_FLAG_DISCONT         (1 << 1)
#define TSMUX_PACKET_FLAG_RANDOM_ACCESS   (1 << 2)
#define TSMUX_PACKET_FLAG_PRIORITY        (1 << 3)
#define TSMUX_PACKET_FLAG_WRITE_PCR       (1 << 4)
#define TSMUX_PACKET_FLAG_WRITE_OPCR      (1 << 5)
#define TSMUX_PACKET_FLAG_WRITE_SPLICE    (1 << 6)
#define TSMUX_PACKET_FLAG_WRITE_ADAPT_EXT (1 << 7)

/* PES stream specific flags */
#define TSMUX_PACKET_FLAG_PES_FULL_HEADER   (1 << 8)
#define TSMUX_PACKET_FLAG_PES_WRITE_PTS     (1 << 9)
#define TSMUX_PACKET_FLAG_PES_WRITE_PTS_DTS (1 << 10)
#define TSMUX_PACKET_FLAG_PES_WRITE_ESCR    (1 << 11)
#define TSMUX_PACKET_FLAG_PES_EXT_STREAMID  (1 << 12)

/* PAT interval (1/20th sec) */
#define TSMUX_DEFAULT_PAT_INTERVAL (TSMUX_CLOCK_FREQ / 20)
/* PMT interval (1/20th sec) */
#define TSMUX_DEFAULT_PMT_INTERVAL (TSMUX_CLOCK_FREQ / 20)

#define TSMUX_DEFAULT_MAX_PENDING_FRAMES 35

typedef struct TsMuxPacketInfo TsMuxPacketInfo;
typedef struct TsMuxProgram TsMuxProgram;
typedef struct TsMuxStream TsMuxStream;

struct TsMuxPacketInfo {
  guint16 pid;
  guint32 flags;

  guint64 pcr;
  guint64 opcr;

  guint8 splice_countdown;

  guint8 private_data_len;
  guint8 private_data [256];

  guint8 packet_count; /* continuity counter */

  guint stream_avail; /* Number of payload bytes available */
  gboolean packet_start_unit_indicator;
};

static inline void
tsmux_put16 (guint8 **pos, guint16 val)
{
  *(*pos)++ = (val >> 8) & 0xff;
  *(*pos)++ = val & 0xff;
}

static inline void
tsmux_put32 (guint8 **pos, guint32 val)
{
  *(*pos)++ = (val >> 24) & 0xff;
  *(*pos)++ = (val >> 16) & 0xff;
  *(*pos)++ = (val >> 8) & 0xff;
  *(*pos)++ = val & 0xff;
}

static inline void
tsmux_put_ts (guint8 **pos, guint8 id, gint64 ts)
{
  /* 1: 4 bit id value | TS [32..30] | marker_bit */
  *(*pos)++ = ((id << 4) | ((ts >> 29) & 0x0E) | 0x01) & 0xff;
  /* 2, 3: TS[29..15] | marker_bit */
  tsmux_put16 (pos, ((ts >> 14) & 0xfffe) | 0x01);
  /* 4, 5: TS[14..0] | marker_bit */
  tsmux_put16 (pos, ((ts << 1) & 0xfffe) | 0x01);
}

#ifdef TS_DEBUG_ON
#define TS_DEBUG(...) g_print(__VA_ARGS__); g_print ("\n")
#else
#define TS_DEBUG(...)
#endif

G_END_DECLS

#endif
