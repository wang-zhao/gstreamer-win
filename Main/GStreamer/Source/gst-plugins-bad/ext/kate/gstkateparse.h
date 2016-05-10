/* -*- c-basic-offset: 2 -*-
 * GStreamer
 * Copyright (C) <2004> Thomas Vander Stichele <thomas at apestaart dot org>
 * Copyright (C) <2008> Vincent Penquerc'h <ogg.k.ogg.k at googlemail dot com>
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


#ifndef __GST_KATE_PARSE_H__
#define __GST_KATE_PARSE_H__


#include <gst/gst.h>
#include <kate/kate.h>

G_BEGIN_DECLS
#define GST_TYPE_KATE_PARSE \
  (gst_kate_parse_get_type())
#define GST_KATE_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_KATE_PARSE,GstKateParse))
#define GST_KATE_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_KATE_PARSE,GstKateParseClass))
#define GST_IS_KATE_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_KATE_PARSE))
#define GST_IS_KATE_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_KATE_PARSE))
typedef struct _GstKateParse GstKateParse;
typedef struct _GstKateParseClass GstKateParseClass;

/**
 * GstKateParse:
 *
 * Opaque data structure.
 */
struct _GstKateParse
{
  GstElement element;

  GstPad *sinkpad;
  GstPad *srcpad;

  guint packetno;
  gboolean streamheader_sent;
  GList *streamheader;

  GQueue *event_queue;
  GQueue *buffer_queue;

  kate_info ki;
  kate_comment kc;
};

struct _GstKateParseClass
{
  GstElementClass parent_class;

  /* virtual functions */
    GstFlowReturn (*parse_packet) (GstKateParse * parse, GstBuffer * buf);
};

GType gst_kate_parse_get_type (void);

G_END_DECLS
#endif /* __GST_KATE_PARSE_H__ */
