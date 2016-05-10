/* GStreamer
 * Copyright (C) 2004 Benjamin Otte <otte@gnome.org>
 *               2005 Wim Taymans <wim@fluendo.com>
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


#include <gst/gst_private.h>
#include "gstdatacache.h"
#include <string.h>
#ifdef G_OS_WIN32
#include <Windows.h>
#endif
/* default size for the assembled data buffer */
#define DEFAULT_SIZE (64*1024)

GST_DEBUG_CATEGORY_STATIC (gst_data_cache_debug);
#define GST_CAT_DEFAULT gst_data_cache_debug

#define GST_DATACACHE_GET_PRIVATE(obj)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_DATACACHE, GstDataCachePrivate))

struct _GstDataCachePrivate
{
  GMutex lock;
  guint8 *buf;
  guint32 buf_size;
  guint32 data_size;
  gchar lock_dbg_info[256];
};

#ifdef G_OS_WIN32
#define LOCK_CACHE(__cache)\
{ g_mutex_lock(&__cache->priv->lock);\
  _snprintf(&__cache->priv->lock_dbg_info[0], sizeof(__cache->priv->lock_dbg_info), "Line:%d Thrd: %d", __LINE__, GetCurrentThreadId());\
}


#define UNLOCK_CACHE(__cache)\
{  g_mutex_unlock(&__cache->priv->lock);\
}
#else

#define LOCK_CACHE(__cache)  g_mutex_lock(&__cache->priv->lock)


#define UNLOCK_CACHE(__cache)   g_mutex_unlock(&__cache->priv->lock)

#endif

#define _do_init(thing) \
  GST_DEBUG_CATEGORY_INIT (gst_data_cache_debug, "data_cache", 0, "object to splice and merge buffers to desired size")
GST_BOILERPLATE_FULL (GstDataCache, gst_data_cache, GObject, G_TYPE_OBJECT,
    _do_init);

static void gst_data_cache_dispose (GObject * object);
static void gst_data_cache_finalize (GObject * object);

static void
gst_data_cache_base_init (gpointer g_class)
{
  /* nop */
}
enum PropertyId
{
  arg_0,
  property_buffer_size
};


static  void 
gst_data_cache_set_property	(GObject        *object,
                             guint           property_id,
                             const GValue   *value,
                             GParamSpec     *pspec);
static void 
gst_data_cache_get_property(GObject        *object,
                            guint           property_id,
                            GValue         *value,
                            GParamSpec     *pspec);

static void
gst_data_cache_class_init (GstDataCacheClass * klass)
{
  GObjectClass *object = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GstDataCachePrivate));

  object->dispose = gst_data_cache_dispose;
  object->finalize = gst_data_cache_finalize;

  object->set_property = gst_data_cache_set_property;
  object->get_property = gst_data_cache_get_property;

  g_object_class_install_property(object, property_buffer_size, 
    g_param_spec_uint("buffer-size", "total buffer size", "Buffer Size", 4*1024,5*1024*1024, 64*1024,  
    G_PARAM_READWRITE |G_PARAM_STATIC_STRINGS));


}

static void
gst_data_cache_init (GstDataCache * data_cache, GstDataCacheClass * g_class)
{
  data_cache->priv = GST_DATACACHE_GET_PRIVATE (data_cache);
  data_cache->priv->buf = 0;
  data_cache->priv->buf_size = DEFAULT_SIZE;
  data_cache->priv->data_size = 0;
  g_mutex_init(&data_cache->priv->lock);
}

static void
gst_data_cache_dispose (GObject * object)
{
  GstDataCache *data_cache = GST_DATACACHE (object);

  gst_data_cache_clear (data_cache);

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_data_cache_finalize (GObject * object)
{
  GstDataCache *data_cache = GST_DATACACHE (object);

  LOCK_CACHE(data_cache);

  if( data_cache->priv->buf)
  {
    g_free(data_cache->priv->buf);
    data_cache->priv->buf = 0;
    data_cache->priv->data_size = 0;
  }
  UNLOCK_CACHE(data_cache);

  g_mutex_clear(&data_cache->priv->lock);


  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static  void 
gst_data_cache_set_property	(GObject        *object,
                             guint           property_id,
                             const GValue   *value,
                             GParamSpec     *pspec)
{
  GstDataCache* data_cache = GST_DATACACHE(object);
  switch (property_id)
  {
  case property_buffer_size:
    LOCK_CACHE(data_cache);
    if( data_cache->priv->buf )
      g_free(data_cache->priv->buf);
    data_cache->priv->buf = 0;
    data_cache->priv->data_size = 0;
    data_cache->priv->buf_size = g_value_get_uint(value);
    UNLOCK_CACHE(data_cache);
    break;
  default:
    break;
  }
}

static void 
gst_data_cache_get_property(GObject        *object,
                            guint           property_id,
                            GValue         *value,
                            GParamSpec     *pspec)
{
  GstDataCache* data_cache = GST_DATACACHE(object);
  switch (property_id)
  {
  case property_buffer_size:
    g_value_set_uint(value, data_cache->priv->buf_size);
    break;
  default:
    break;
  }
}

/**
 * gst_data_cache_new:
 *
 * Creates a new #GstDataCache. Free with g_object_unref().
 *
 * Returns: (transfer full): a new #GstDataCache
 */
GstDataCache *
gst_data_cache_new (guint size)
{
  return g_object_new (GST_TYPE_DATACACHE, "buffer-size",size,NULL);
}

/**
 * gst_data_cache_clear:
 * @data_cache: a #GstDataCache
 *
 * Removes all buffers from @data_cache.
 */
void
gst_data_cache_clear (GstDataCache * data_cache)
{
  g_return_if_fail (GST_IS_DATACACHE (data_cache));

  LOCK_CACHE(data_cache);
  data_cache->priv->data_size = 0;
  UNLOCK_CACHE(data_cache);
}


/**
 * gst_data_cache_push:
 * @data_cache: a #GstDataCache
 * @buf: (transfer full): a #GstBuffer to add to queue in the data_cache
 *
 * Adds the data from @buf to the data stored inside @data_cache and takes
 * ownership of the buffer.
 */
guint
gst_data_cache_push (GstDataCache * data_cache, GstBuffer * buf)
{
  guint size;

  g_return_val_if_fail (GST_IS_DATACACHE (data_cache), 0);
  g_return_val_if_fail (GST_IS_BUFFER (buf), 0);
  LOCK_CACHE(data_cache);

  if( !data_cache->priv->buf )
    data_cache->priv->buf = g_malloc(data_cache->priv->buf_size);

  if( !data_cache->priv->buf )
  {
    UNLOCK_CACHE(data_cache);
    return 0;
  }

  size = GST_BUFFER_SIZE (buf);
  if (!gst_data_cache_check(data_cache, size))
  {
    UNLOCK_CACHE(data_cache);
    return 0;
  }  
  memcpy(data_cache->priv->buf + data_cache->priv->data_size, GST_BUFFER_DATA(buf), size);
  data_cache->priv->data_size += size;

  UNLOCK_CACHE(data_cache);
  return size;
}


guint8 *
gst_data_cache_lock(GstDataCache * data_cache, guint* nbytes)
{ 
  g_return_val_if_fail (GST_IS_DATACACHE (data_cache), 0);

  LOCK_CACHE(data_cache);
  if( *nbytes <= 0 )
    *nbytes = data_cache->priv->data_size;

  if( *nbytes <= 0 || data_cache->priv->buf == 0 )
  {
    UNLOCK_CACHE(data_cache);
    return 0;
  }

  if( *nbytes > data_cache->priv->data_size )
    *nbytes = data_cache->priv->data_size;
  
  if( *nbytes <= 0 || data_cache->priv->buf == 0 )
  {
	  UNLOCK_CACHE(data_cache);
	  return 0;
  }
  return data_cache->priv->buf;
}
void 
gst_data_cache_unlock(GstDataCache * data_cache, guint nbytes)
{
  g_return_if_fail (GST_IS_DATACACHE (data_cache));

  if( nbytes > 0)
  {
    if( nbytes >= data_cache->priv->data_size)
      data_cache->priv->data_size = 0;
    else 
    {
      data_cache->priv->data_size -= nbytes;
      g_memmove(data_cache->priv->buf, data_cache->priv->buf + nbytes, data_cache->priv->data_size);
    }
  }
  UNLOCK_CACHE(data_cache);
}

guint
gst_data_cache_available (GstDataCache * data_cache)
{
  g_return_val_if_fail (GST_IS_DATACACHE (data_cache), 0);

  return data_cache->priv->data_size;
}

gboolean
gst_data_cache_check (GstDataCache *data_cache, guint size)
{
  g_return_val_if_fail (GST_IS_DATACACHE (data_cache), FALSE);

  return (data_cache->priv->buf_size - data_cache->priv->data_size) >= size;
}
