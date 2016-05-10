/* GStreamer
 * Copyright (C) 2004 Benjamin Otte <otte@gnome.org>
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

#include <gst/gst.h>

#ifndef __GST_DATACACHE_H__
#define __GST_DATACACHE_H__

G_BEGIN_DECLS


#define GST_TYPE_DATACACHE \
  (gst_data_cache_get_type())
#define GST_DATACACHE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DATACACHE, GstDataCache))
#define GST_DATACACHE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DATACACHE, GstDataCacheClass))
#define GST_DATACACHE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_DATACACHE, GstDataCacheClass))
#define GST_IS_DATACACHE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DATACACHE))
#define GST_IS_DATACACHE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DATACACHE))

typedef struct _GstDataCache GstDataCache;
typedef struct _GstDataCacheClass GstDataCacheClass;
typedef struct _GstDataCachePrivate GstDataCachePrivate;

/**
 * GstDataCache:
 *
 * The opaque #GstDataCache data structure.
 */
struct _GstDataCache {
  GObject       object;

  /*< private >*/
  GstDataCachePrivate *priv;
  gpointer _gst_reserved[GST_PADDING - 2];
};

struct _GstDataCacheClass {
  GObjectClass  parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GType                   gst_data_cache_get_type            (void);
GstDataCache *          gst_data_cache_new                 (guint size);
void                    gst_data_cache_clear               (GstDataCache *data_cache);
guint                   gst_data_cache_push                (GstDataCache *data_cache, GstBuffer* buf);
guint8*                 gst_data_cache_lock                (GstDataCache *data_cache, guint* size);
void                    gst_data_cache_unlock              (GstDataCache *data_cache, guint size);
guint                   gst_data_cache_available           (GstDataCache *data_cache);
gboolean                gst_data_cache_check               (GstDataCache *data_cache, guint size);

G_END_DECLS

#endif /* __GST_DATACACHE_H__ */
