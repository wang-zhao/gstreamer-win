/* ASF Parser plugin for GStreamer
 * Copyright (C) 2009 Thiago Santos <thiagoss@embedded.ufcg.edu.br>
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


#ifndef __GST_ASF_PARSE_H__
#define __GST_ASF_PARSE_H__


#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstbytereader.h>

#include "gstasfobjects.h"

G_BEGIN_DECLS

#define GST_TYPE_ASF_PARSE \
  (gst_asf_parse_get_type())
#define GST_ASF_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ASF_PARSE,GstAsfParse))
#define GST_ASF_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ASF_PARSE,GstAsfParseClass))
#define GST_IS_ASF_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ASF_PARSE))
#define GST_IS_ASF_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ASF_PARSE))

#define GST_ASF_PARSE_CAST(obj) ((GstAsfParse*)(obj))

enum GstAsfParsingState {
  ASF_PARSING_HEADERS,
  ASF_PARSING_DATA,
  ASF_PARSING_PACKETS,
  ASF_PARSING_INDEXES
};

typedef struct _GstAsfParse GstAsfParse;
typedef struct _GstAsfParseClass GstAsfParseClass;

struct _GstAsfParse {
  GstElement element;

  enum GstAsfParsingState parse_state;

  GstAdapter *adapter;

  GstPad *srcpad;
  GstPad *sinkpad;
  GstCaps *outcaps;

  guint64 parsed_packets;

  guint64 offset; /* used in pull mode */

  /* parsed info */
  GstAsfFileInfo *asfinfo;
  GstAsfPacketInfo *packetinfo; /* we keep it here to avoid allocs */
  guint64 headers_size;
  guint64 data_size;
};

struct _GstAsfParseClass {
  GstElementClass parent_class;
};

GType gst_asf_parse_get_type(void);
gboolean gst_asf_parse_plugin_init (GstPlugin * plugin);

G_END_DECLS


#endif /* __GST_ASF_PARSE_H__ */
