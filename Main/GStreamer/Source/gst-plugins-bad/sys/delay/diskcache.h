#ifndef __diskcache_h__
#define __diskcache_h__

#include <glib.h>

typedef struct _DiskCache DiskCache;

DiskCache* gst_disk_cache_new();
gint64 gst_disk_cache_add_data(DiskCache* dc, gpointer data, glong size);
gboolean gst_disk_cache_remove_data(DiskCache* dc, gpointer data, gint64 start, glong size);
void gst_disk_cache_free(DiskCache* dc);

#endif __diskcache_h__