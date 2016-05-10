/*
 * mpegtsbase.h - GStreamer MPEG transport stream base class
 * Copyright (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
 *               2007 Alessandro Decina
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *  Author: Youness Alaoui <youness.alaoui@collabora.co.uk>, Collabora Ltd.
 *  Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 *
 * Authors:
 *   Alessandro Decina <alessandro@nnva.org>
 *   Edward Hervey <edward.hervey@collabora.co.uk>
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


#ifndef GST_MPEG_TS_BASE_H
#define GST_MPEG_TS_BASE_H

#include <gst/gst.h>
#include "mpegtspacketizer.h"

G_BEGIN_DECLS

#define GST_TYPE_MPEGTS_BASE \
  (mpegts_base_get_type())
#define GST_MPEGTS_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MPEGTS_BASE,MpegTSBase))
#define GST_MPEGTS_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MPEGTS_BASE,MpegTSBaseClass))
#define GST_IS_MPEGTS_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MPEGTS_BASE))
#define GST_IS_MPEGTS_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MPEGTS_BASE))
#define GST_MPEGTS_BASE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MPEGTS_BASE, MpegTSBaseClass))

typedef struct _MpegTSBase MpegTSBase;
typedef struct _MpegTSBaseClass MpegTSBaseClass;
typedef struct _MpegTSBaseStream MpegTSBaseStream;
typedef struct _MpegTSBaseProgram MpegTSBaseProgram;

struct _MpegTSBaseStream
{
  guint16 pid;
  guint8 stream_type;
  GstStructure* stream_info;
};

struct _MpegTSBaseProgram
{
  gint program_number;
  guint16 pmt_pid;
  guint16 pcr_pid;
  GstStructure *pmt_info;
  MpegTSBaseStream **streams;
  GList *stream_list;
  gint patcount;

  /* Pending Tags for the program */
  GstTagList *tags;
  guint event_id;

  /* TRUE if the program is currently being used */
  gboolean active;
};

typedef enum {
  /* PULL MODE */
  BASE_MODE_SCANNING,		/* Looking for PAT/PMT */
  BASE_MODE_SEEKING,		/* Seeking */
  BASE_MODE_STREAMING,		/* Normal mode (pushing out data) */

  /* PUSH MODE */
  BASE_MODE_PUSHING
} MpegTSBaseMode;

struct _MpegTSBase {
  GstElement element;

  GstPad *sinkpad;

  /* pull-based behaviour */
  MpegTSBaseMode mode;

  /* location of first sync point */
  guint64	initial_sync_point;

  /* Current pull offset (also set by seek handler) */
  guint64	seek_offset;

  /* Cached packetsize */
  guint16	packetsize;

  /* the following vars must be protected with the OBJECT_LOCK as they can be
   * accessed from the application thread and the streaming thread */
  GHashTable *programs;

  GstStructure *pat;
  MpegTSPacketizer2 *packetizer;

  /* arrays that say whether a pid is a known psi pid or a pes pid */
  /* Use MPEGTS_BIT_* to set/unset/check the values */
  guint8 *known_psi;
  guint8 *is_pes;

  gboolean disposed;

  /* size of the MpegTSBaseProgram structure, can be overridden
   * by subclasses if they have their own MpegTSBaseProgram subclasses. */
  gsize program_size;

  /* size of the MpegTSBaseStream structure, can be overridden
   * by subclasses if they have their own MpegTSBaseStream subclasses */
  gsize stream_size;

  /* Whether we saw a PAT yet */
  gboolean seen_pat;

  /* Offset from the origin to the first PAT (pullmode) */
  guint64    first_pat_offset;

  /* interpolation gap between the upstream timestamp and the pts */
  GstClockTime in_gap;
  GstClockTime first_buf_ts;

  /* Upstream segment */
  GstSegment segment;
};

struct _MpegTSBaseClass {
  GstElementClass parent_class;

  /* Virtual methods */
  void (*reset) (MpegTSBase *base);
  GstFlowReturn (*push) (MpegTSBase *base, MpegTSPacketizerPacket *packet, MpegTSPacketizerSection * section);
  gboolean (*push_event) (MpegTSBase *base, GstEvent * event);

  /* program_started gets called when program's pmt arrives for first time */
  void (*program_started) (MpegTSBase *base, MpegTSBaseProgram *program);
  /* program_stopped gets called when pat no longer has program's pmt */
  void (*program_stopped) (MpegTSBase *base, MpegTSBaseProgram *program);

  /* stream_added is called whenever a new stream has been identified */
  void (*stream_added) (MpegTSBase *base, MpegTSBaseStream *stream, MpegTSBaseProgram *program);
  /* stream_removed is called whenever a stream is no longer referenced */
  void (*stream_removed) (MpegTSBase *base, MpegTSBaseStream *stream);

  /* find_timestamps is called to find PCR */
  GstFlowReturn (*find_timestamps) (MpegTSBase * base, guint64 initoff, guint64 *offset);

  /* seek is called to wait for seeking */
  GstFlowReturn (*seek) (MpegTSBase * base, GstEvent * event, guint16 pid);

  /* flush all streams */
  void (*flush) (MpegTSBase * base);

  /* signals */
  void (*pat_info) (GstStructure *pat);
  void (*pmt_info) (GstStructure *pmt);
  void (*nit_info) (GstStructure *nit);
  void (*sdt_info) (GstStructure *sdt);
  void (*eit_info) (GstStructure *eit);
};

#define MPEGTS_BIT_SET(field, offs)    ((field)[(offs) / 8] |=  (1 << ((offs) % 8)))
#define MPEGTS_BIT_UNSET(field, offs)  ((field)[(offs) / 8] &= ~(1 << ((offs) % 8)))
#define MPEGTS_BIT_IS_SET(field, offs) ((field)[(offs) / 8] &   (1 << ((offs) % 8)))

GType mpegts_base_get_type(void);

MpegTSBaseProgram *mpegts_base_get_program (MpegTSBase * base, gint program_number);
MpegTSBaseProgram *mpegts_base_add_program (MpegTSBase * base, gint program_number, guint16 pmt_pid);

guint8 *mpegts_get_descriptor_from_stream (MpegTSBaseStream * stream, guint8 tag);
guint8 *mpegts_get_descriptor_from_program (MpegTSBaseProgram * program, guint8 tag);

gboolean
mpegts_base_handle_seek_event(MpegTSBase * base, GstPad * pad, GstEvent * event);

gboolean gst_mpegtsbase_plugin_init (GstPlugin * plugin);

gboolean mpegts_base_is_psi (MpegTSBase * base, MpegTSPacketizerPacket * packet);
gboolean mpegts_base_handle_psi (MpegTSBase * base, MpegTSPacketizerSection * section);

void mpegts_base_program_remove_stream (MpegTSBase * base, MpegTSBaseProgram * program, guint16 pid);

void mpegts_base_remove_program(MpegTSBase *base, gint program_number);
G_END_DECLS

#endif /* GST_MPEG_TS_BASE_H */
