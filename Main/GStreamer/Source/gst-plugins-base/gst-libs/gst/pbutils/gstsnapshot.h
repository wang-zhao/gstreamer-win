#ifndef __GST_GET_SNAPSHOT_DATA_H__
#define __GST_GET_SNAPSHOT_DATA_H__

#include <gst/gst.h>

G_BEGIN_DECLS

gboolean gst_get_snapshot_data(const gchar* file_path,  GstClockTime pos, gint width, gint height, const gchar* picture_type,gchar** data,gsize* size);

G_END_DECLS

#endif /* __GST_GET_SNAPSHOT_DATA_H__ */