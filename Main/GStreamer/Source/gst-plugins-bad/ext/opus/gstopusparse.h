/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2008> Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __GST_OPUS_PARSE_H__
#define __GST_OPUS_PARSE_H__

#include <gst/gst.h>
#include <gst/base/gstbaseparse.h>

G_BEGIN_DECLS

#define GST_TYPE_OPUS_PARSE \
  (gst_opus_parse_get_type())
#define GST_OPUS_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPUS_PARSE,GstOpusParse))
#define GST_OPUS_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPUS_PARSE,GstOpusParseClass))
#define GST_IS_OPUS_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPUS_PARSE))
#define GST_IS_OPUS_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPUS_PARSE))

typedef struct _GstOpusParse GstOpusParse;
typedef struct _GstOpusParseClass GstOpusParseClass;

struct _GstOpusParse {
  GstBaseParse       element;

  gboolean header_sent;
  GSList *headers;
  GstClockTime next_ts;
  GstBuffer *id_header;
  GstBuffer *comment_header;
};

struct _GstOpusParseClass {
  GstBaseParseClass parent_class;
};

GType gst_opus_parse_get_type (void);

G_END_DECLS

#endif /* __GST_OPUS_PARSE_H__ */
