/* Farsight
 * Copyright (C) 2007 Nokia Corporation
 * Copyright (C) 2007 Collabora Ltd
 *  @author: Olivier Crete <olivier.crete@collabora.co.uk>
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


#ifndef __GST_RTPCNPAYLOAD_H__
#define __GST_RTPCNPAYLOAD_H__

#include <gst/gst.h>
#include <gst/rtp/gstbasertppayload.h>

G_BEGIN_DECLS

typedef struct _GstRTPCNPayload GstRTPCNPayload;
typedef struct _GstRTPCNPayloadClass GstRTPCNPayloadClass;

#define GST_TYPE_RTPCNPAYLOAD \
  (gst_rtpcnpayload_get_type())
#define GST_RTPCNPAYLOAD(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTPCNPAYLOAD,GstRTPCNPayload))
#define GST_RTPCNPAYLOAD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTPCNPAYLOAD,GstRTPCNPayload))
#define GST_IS_RTPCNPAYLOAD(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTPCNPAYLOAD))
#define GST_IS_RTPCNPAYLOAD_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTPCNPAYLOAD))

struct _GstRTPCNPayload
{
  GstBaseRTPPayload payload;
};

struct _GstRTPCNPayloadClass
{
  GstBaseRTPPayloadClass parent_class;
};

gboolean gst_rtpcnpayload_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif /* __GST_RTPCNPAYLOAD_H__ */
