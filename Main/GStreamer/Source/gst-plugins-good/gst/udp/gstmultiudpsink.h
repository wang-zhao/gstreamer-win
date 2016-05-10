/* GStreamer
 * Copyright (C) <2005> Wim Taymand <wim@fluendo.com>
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

#ifndef __GST_MULTIUDPSINK_H__
#define __GST_MULTIUDPSINK_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

G_BEGIN_DECLS

#include "gstudpnetutils.h"
#include "gstudp.h"

#define GST_TYPE_MULTIUDPSINK            (gst_multiudpsink_get_type())
#define GST_MULTIUDPSINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MULTIUDPSINK,GstMultiUDPSink))
#define GST_MULTIUDPSINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MULTIUDPSINK,GstMultiUDPSinkClass))
#define GST_IS_MULTIUDPSINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MULTIUDPSINK))
#define GST_IS_MULTIUDPSINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MULTIUDPSINK))

typedef struct _GstMultiUDPSink GstMultiUDPSink;
typedef struct _GstMultiUDPSinkClass GstMultiUDPSinkClass;

typedef struct {
  gint refcount;

  int *sock;

  struct sockaddr_storage theiraddr;

  gchar *host;
  gint port;

  /* Per-client stats */
  guint64 bytes_sent;
  guint64 packets_sent;
  guint64 connect_time;
  guint64 disconnect_time;
} GstUDPClient;

/* sends udp packets to multiple host/port pairs.
 */
struct _GstMultiUDPSink {
  GstBaseSink parent;

  GMutex        *client_lock;
  GList         *clients;

  /* properties */
  guint64        bytes_to_serve;
  guint64        bytes_served;
  /* If we have a dual-stack (IPv4 and IPv6) capable system, we use 'sock' for
   * all connections. If it's two independent stacks (or IPv6 isn't 
   * supported at all), then 'sock' is for IPv4 only, and 'sock6' is used for IPv6).
   */
  gboolean       dualstack;
#if 0
   *
   * if dualstack
   *   use sock
   * else
   *   if addr is v4
   *     use sock
   *   else
   *     use sock6
#endif

  int            sock;        /* Actual socket FD */
  int            sockfd;      /* The FD for the socket to use, or -1 if we're 
                                 going to allocate a socket ourselves */
  gboolean       closefd;     /* TRUE if we should close an externally-
                                 supplied FD when we stop */
  gboolean       externalfd;  /* TRUE if the FD is externally supplied */

  int            sock6;
  int            sockfd6;
  gboolean       closefd6;
  gboolean       externalfd6;

  gboolean       auto_multicast;
  gint           ttl;
  gint           ttl_mc;
  gboolean       loop;
  gint           qos_dscp;
  guint16        ss_family;

  gboolean       send_duplicates;
  gint           buffer_size;
};

struct _GstMultiUDPSinkClass {
  GstBaseSinkClass parent_class;

  /* element methods */
  void          (*add)          (GstMultiUDPSink *sink, const gchar *host, gint port);
  void          (*remove)       (GstMultiUDPSink *sink, const gchar *host, gint port);
  void          (*clear)        (GstMultiUDPSink *sink);
  GValueArray*  (*get_stats)    (GstMultiUDPSink *sink, const gchar *host, gint port);

  /* signals */
  void          (*client_added) (GstElement *element, const gchar *host, gint port);
  void          (*client_removed) (GstElement *element, const gchar *host, gint port);
};

GType gst_multiudpsink_get_type(void);

void            gst_multiudpsink_add            (GstMultiUDPSink *sink, const gchar *host, gint port);
void            gst_multiudpsink_remove         (GstMultiUDPSink *sink, const gchar *host, gint port);
void            gst_multiudpsink_clear          (GstMultiUDPSink *sink);
GValueArray*    gst_multiudpsink_get_stats      (GstMultiUDPSink *sink, const gchar *host, gint port);

G_END_DECLS

#endif /* __GST_MULTIUDPSINK_H__ */
