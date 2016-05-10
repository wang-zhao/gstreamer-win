/* GStreamer
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __MXF_MUX_H__
#define __MXF_MUX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstcollectpads.h>

#include "mxfessence.h"

G_BEGIN_DECLS

#define GST_TYPE_MXF_MUX \
  (gst_mxf_mux_get_type ())
#define GST_MXF_MUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MXF_MUX, GstMXFMux))
#define GST_MXF_MUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MXF_MUX, GstMXFMuxClass))
#define GST_IS_MXF_MUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MXF_MUX))
#define GST_IS_MXF_MUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MXF_MUX))

typedef struct
{
  GstCollectData collect;

  guint64 pos;
  GstClockTime last_timestamp;

  MXFMetadataFileDescriptor *descriptor;

  GstAdapter *adapter;
  gboolean have_complete_edit_unit;

  gpointer mapping_data;
  const MXFEssenceElementWriter *writer;
  MXFEssenceElementWriteFunc write_func;

  MXFMetadataSourcePackage *source_package;
  MXFMetadataTimelineTrack *source_track;
} GstMXFMuxPad;

typedef enum
{
  GST_MXF_MUX_STATE_HEADER,
  GST_MXF_MUX_STATE_DATA,
  GST_MXF_MUX_STATE_EOS,
  GST_MXF_MUX_STATE_ERROR
} GstMXFMuxState;

typedef struct _GstMXFMux {
  GstElement element;

  GstPad *srcpad;
  GstCollectPads *collect;

  /* <private> */
  GstPadEventFunction collect_event;

  GstMXFMuxState state;
  guint n_pads;

  guint64 offset;

  MXFPartitionPack partition;
  MXFPrimerPack primer;
  
  GHashTable *metadata;
  GList *metadata_list;
  MXFMetadataPreface *preface;

  MXFFraction min_edit_rate;
  guint64 last_gc_position;
  GstClockTime last_gc_timestamp;

  gchar *application;
} GstMXFMux;

typedef struct _GstMXFMuxClass {
  GstElementClass parent;
} GstMXFMuxClass;

GType gst_mxf_mux_get_type (void);

G_END_DECLS

#endif /* __MXF_MUX_H__ */
