#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstsnapshot.h"
#include <glib-2.0/glib.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

//const gchar* file_path----[in]文件路径
//gint width---[in]图片宽
//gint height---[in]图片高
//const gchar* picture_type---[in]图片类型，bmp,jpeg,png
//gchar** data---[in,oyt]存放图片数据地址
//gsize* size----[in,out]数据大小

#ifdef __cplusplus
extern "C" {
#endif 

  gboolean gst_get_snapshot_data(const gchar* file_path, GstClockTime pos, gint width, gint height, const gchar* picture_type,gchar** data,gsize* size)
  {
    gboolean bRet = FALSE;
    GstElement *dec = NULL;
    GstBus *bus = NULL;
    gst_init (NULL, NULL);

    char* descr = g_strdup_printf ("uridecodebin name=uridecodebin !queue2 !ffmpegcolorspace !videoscale !video/x-raw-rgb, width=%d, height=%d !appsink name=sink",
      width, height);
    GST_LOG(descr);
    GError *error = NULL;
    GstElement *pipeline = gst_parse_launch (descr, &error);
    g_free(descr);
    if (error != NULL) {
      GST_DEBUG ("could not construct pipeline: %s", error->message);
      g_clear_error(&error);
      goto result;
    }

    GstElement *sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
    GstElement *uridecode = gst_bin_get_by_name(GST_BIN(pipeline), "uridecodebin");

    g_object_set(uridecode, "uri", file_path, 0);
    g_object_unref(uridecode);

    int ret = gst_element_set_state (pipeline, GST_STATE_PAUSED);
    switch (ret) {
    case GST_STATE_CHANGE_FAILURE:
      GST_DEBUG ("failed to play the file\n");
      g_object_unref(sink);
      goto result;
    case GST_STATE_CHANGE_NO_PREROLL:
      GST_DEBUG ("live sources not supported yet\n");
      g_object_unref(sink);
      goto result;
    default:
      break;
    }
    ret = gst_element_get_state (pipeline, NULL, NULL, -1);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      GST_DEBUG ("failed to play the file\n");
      g_object_unref(sink);
      goto result;
    }

    gint64 position = 0;
    gint64 duration = 0;
    GstFormat format = GST_FORMAT_TIME;

    if (!gst_element_query_duration(pipeline, &format, &duration))
      duration = 0;
//     if (!gst_element_query_position(pipeline, &format, &position))
//       position = 0;
// 
//     GST_LOG ("duration = %" G_GINT64_FORMAT " position = %" G_GINT64_FORMAT "\n", duration, position);
//     if (duration != 0)
//       position = duration * 1 / 100;
//     else if (duration > 2 * GST_MSECOND)
//     {
//       position = duration  + 2 * GST_MSECOND;
//     }
//     else
//     {
//       position = duration * 0.5;
//     }


    GST_LOG("position = %" G_GINT64_FORMAT "\n", position);

    if(!gst_element_seek_simple (pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, pos))
    {
      g_object_unref(sink);
      goto result;
    }

    GstBuffer *buffer = NULL;
    GstCaps *caps = NULL;
    GstStructure *s = NULL;
    //buffer = gst_app_sink_pull_preroll (GST_APP_SINK(sink));
    g_signal_emit_by_name (sink, "pull-preroll", &buffer, NULL);
    g_object_unref(sink);

    if (buffer)
    {
      caps = GST_BUFFER_CAPS (buffer);
      if (!caps)
      {
        GST_DEBUG ("could not get snapshot format\n");
        gst_buffer_unref(buffer);
        goto result;
      }

      s = gst_caps_get_structure (caps, 0);
      gchar *string = gst_structure_to_string(s);
      GST_DEBUG("%s\n",string);


      int width,height,bpp;
      gst_structure_get_int (s, "bpp", &bpp);
      int res = gst_structure_get_int (s, "width", &width);
      res |= gst_structure_get_int (s, "height", &height);
      if (!res)
      {
        GST_DEBUG ("could not get snapshot dimension\n");
        gst_buffer_unref(buffer);
        goto result;
      }
      //GST_DEBUG("bpp = %d, width = %d, height = %d\n",bpp, width, height);

      GstVideoFormat fmt;
      gint w, h, par_n, par_d;
      if (!gst_video_format_parse_caps (caps, &fmt, &w, &h)) {
        GST_DEBUG ("parse_caps failed \n");
        gst_buffer_unref(buffer);
        goto result;
      }

      if (!gst_video_parse_caps_pixel_aspect_ratio (caps, &par_n, &par_d)) {
        GST_DEBUG("no pixel aspect ratio\n");
        gst_buffer_unref(buffer);
        goto result;
      }

      gint rowstride = gst_video_format_get_row_stride (fmt, 0, w);
      gboolean has_alpha = FALSE;
      if(bpp == 32)
      {
        has_alpha = TRUE;
      }

      //GST_DEBUG("rowstride = %d----%d\n",rowstride,GST_BUFFER_SIZE (buffer)/height);
      GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data (GST_BUFFER_DATA (buffer), GDK_COLORSPACE_RGB, has_alpha, 8, width, height, rowstride, NULL, NULL);
      //gdk_pixbuf_save(pixbuf, "C:\\test.jpg", picture_type, &error, NULL);
      if(pixbuf)
      {
        if(gdk_pixbuf_save_to_buffer(pixbuf, data, size,picture_type, &error, 0))
        {
          bRet = true;
        }
        else
        {
          g_clear_error(&error);
        }
        g_object_unref(pixbuf);
      }
      gst_buffer_unref(buffer);
    }

result:
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
    return bRet;
  }

#ifdef __cplusplus
};
#endif 
