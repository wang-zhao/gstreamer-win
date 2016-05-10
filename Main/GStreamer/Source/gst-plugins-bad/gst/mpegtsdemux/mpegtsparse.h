/*
 * mpegts_parse.h - GStreamer MPEG transport stream parser
 * Copyright (C) 2007 Alessandro Decina
 * 
 * Authors:
 *   Alessandro Decina <alessandro@nnva.org>
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


#ifndef GST_MPEG_TS_PARSE_H
#define GST_MPEG_TS_PARSE_H

#include <gst/gst.h>
#include "mpegtsbase.h"
#include "mpegtspacketizer.h"

G_BEGIN_DECLS

#define GST_TYPE_MPEGTS_PARSE \
  (mpegts_parse_get_type())
#define GST_MPEGTS_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MPEGTS_PARSE,MpegTSParse2))
#define GST_MPEGTS_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MPEGTS_PARSE,MpegTSParse2Class))
#define GST_IS_MPEGTS_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MPEGTS_PARSE))
#define GST_IS_MPEGTS_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MPEGTS_PARSE))

typedef struct _MpegTSParse2 MpegTSParse2;
typedef struct _MpegTSParse2Class MpegTSParse2Class;

struct _MpegTSParse2 {
  MpegTSBase parent;

  /* the following vars must be protected with the OBJECT_LOCK as they can be
   * accessed from the application thread and the streaming thread */
  gchar *program_numbers;
  GList *pads_to_add;
  GList *pads_to_remove;
  guint req_pads;

  gboolean need_sync_program_pads;
};

struct _MpegTSParse2Class {
  MpegTSBaseClass parent_class;
};

GType mpegts_parse_get_type(void);

gboolean gst_mpegtsparse_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif /* GST_MPEG_TS_PARSE_H */
