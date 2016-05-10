/*
 * GStreamer
 * Copyright (C) 2010 Jan Schmidt <thaytan@noraisin.net>
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

/**
 * SECTION:element-rtmpsink
 *
 * This element delivers data to a streaming server via RTMP. It uses
 * librtmp, and supports any protocols/urls that librtmp supports.
 * The URL/location can contain extra connection or session parameters
 * for librtmp, such as 'flashver=version'. See the librtmp documentation
 * for more detail
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v videotestsrc ! ffenc_flv ! flvmux ! rtmpsink location='rtmp://localhost/path/to/stream live=1'
 * ]| Encode a test video stream to FLV video format and stream it via RTMP.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <glib/gstdio.h>

#include "gstrtmpsink.h"

#ifdef G_OS_WIN32
#include <winsock2.h>
#else
#define RTMP_FreeMemory free
#endif

#include <stdlib.h>

GST_DEBUG_CATEGORY_STATIC(gst_rtmp_sink_debug);
#define GST_CAT_DEFAULT gst_rtmp_sink_debug

#define DEFAULT_LOCATION NULL

enum {
    PROP_0,
    PROP_LOCATION
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE("sink",
        GST_PAD_SINK,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-flv")
                                                                   );

static void gst_rtmp_sink_uri_handler_init(gpointer g_iface,
        gpointer iface_data);
static void gst_rtmp_sink_set_property(GObject* object, guint prop_id,
                                       const GValue* value, GParamSpec* pspec);
static void gst_rtmp_sink_get_property(GObject* object, guint prop_id,
                                       GValue* value, GParamSpec* pspec);
static void gst_rtmp_sink_finalize(GObject* object);
static gboolean gst_rtmp_sink_stop(GstBaseSink* sink);
static gboolean gst_rtmp_sink_start(GstBaseSink* sink);
static GstFlowReturn gst_rtmp_sink_render(GstBaseSink* sink, GstBuffer* buf);

static void
_do_init(GType gtype) {
    static const GInterfaceInfo urihandler_info = {
        gst_rtmp_sink_uri_handler_init,
        NULL,
        NULL
    };

    g_type_add_interface_static(gtype, GST_TYPE_URI_HANDLER, &urihandler_info);

    GST_DEBUG_CATEGORY_INIT(gst_rtmp_sink_debug, "rtmpsink", 0,
                            "RTMP server element");
}

GST_BOILERPLATE_FULL(GstRTMPSink, gst_rtmp_sink, GstBaseSink,
                     GST_TYPE_BASE_SINK, _do_init);


static void
gst_rtmp_sink_base_init(gpointer klass) {
    GstElementClass* element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_details_simple(element_class,
                                         "RTMP output sink",
                                         "Sink/Network", "Sends FLV content to a server via RTMP",
                                         "Jan Schmidt <thaytan@noraisin.net>");

    gst_element_class_add_static_pad_template(element_class,
            &sink_template);
}

/* initialize the plugin's class */
static void
gst_rtmp_sink_class_init(GstRTMPSinkClass* klass) {
    GObjectClass* gobject_class;
    GstBaseSinkClass* gstbasesink_class = (GstBaseSinkClass*) klass;

    gobject_class = (GObjectClass*) klass;
    gobject_class->finalize = gst_rtmp_sink_finalize;
    gobject_class->set_property = gst_rtmp_sink_set_property;
    gobject_class->get_property = gst_rtmp_sink_get_property;

    gstbasesink_class->start = GST_DEBUG_FUNCPTR(gst_rtmp_sink_start);
    gstbasesink_class->stop = GST_DEBUG_FUNCPTR(gst_rtmp_sink_stop);
    gstbasesink_class->render = GST_DEBUG_FUNCPTR(gst_rtmp_sink_render);

    GST_DEBUG_CATEGORY_INIT(gst_rtmp_sink_debug, "rtmpsink", 0,
                            "RTMP server element");
    gst_element_class_install_std_props(GST_ELEMENT_CLASS(klass),
                                        "location", PROP_LOCATION, G_PARAM_READWRITE, NULL);
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_rtmp_sink_init(GstRTMPSink* sink, GstRTMPSinkClass* klass) {
#ifdef G_OS_WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        GST_ERROR_OBJECT(sink, "WSAStartup failed: 0x%08x", WSAGetLastError());
    }

#endif
    sink->restart = FALSE;
    sink->header = NULL;
    sink->cache = NULL;
    sink->retry_time = 0;

    sink->video_codec_data = NULL;
    sink->audio_codec_data = NULL;
    sink->metadata = NULL;
    sink->has_video = 0;
    sink->has_audio = 0;
}

static void
gst_rtmp_sink_finalize(GObject* object) {
    GstRTMPSink* sink = GST_RTMP_SINK(object);

#ifdef G_OS_WIN32
    WSACleanup();
#endif
    g_free(sink->uri);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}


static gboolean
gst_rtmp_sink_start_impl(GstBaseSink* basesink) {
    GstRTMPSink* sink = GST_RTMP_SINK(basesink);

    if (!sink->uri) {
        GST_ELEMENT_ERROR(sink, RESOURCE, OPEN_WRITE,
                          ("Please set URI for RTMP output"), ("No URI set before starting"));
        return FALSE;
    }

    sink->rtmp_uri = g_strdup(sink->uri);
    sink->rtmp = RTMP_Alloc();
    RTMP_Init(sink->rtmp);

    if (!RTMP_SetupURL(sink->rtmp, sink->rtmp_uri)) {
        GST_ELEMENT_ERROR(sink, RESOURCE, OPEN_WRITE, (NULL),
                          ("Failed to setup URL '%s'", sink->uri));
        RTMP_Free(sink->rtmp);
        sink->rtmp = NULL;
        g_free(sink->rtmp_uri);
        sink->rtmp_uri = NULL;
        return FALSE;
    }

    GST_DEBUG_OBJECT(sink, "Created RTMP object");

    /* Mark this as an output connection */
    RTMP_EnableWrite(sink->rtmp);

    if (sink->restart == FALSE) {
        sink->first = TRUE;
    }

    return TRUE;
}
static gboolean
gst_rtmp_sink_start(GstBaseSink* basesink) {
    GstRTMPSink* sink = GST_RTMP_SINK(basesink);
    sink->restart = FALSE;
    return gst_rtmp_sink_start_impl(basesink);
}
static gboolean
gst_rtmp_sink_stop(GstBaseSink* basesink) {
    GstRTMPSink* sink = GST_RTMP_SINK(basesink);

    gst_buffer_replace(&sink->cache, NULL);
    gst_buffer_replace(&sink->header, NULL);
    gst_buffer_replace(&sink->video_codec_data, NULL);
    gst_buffer_replace(&sink->audio_codec_data, NULL);
    gst_buffer_replace(&sink->metadata, NULL);

    if (sink->rtmp) {
        RTMP_Close(sink->rtmp);
        RTMP_Free(sink->rtmp);
        sink->rtmp = NULL;
    }

    if (sink->rtmp_uri) {
        g_free(sink->rtmp_uri);
        sink->rtmp_uri = NULL;
    }

    return TRUE;
}

static gboolean gst_rtmp_sink_restart(GstBaseSink* bsink) {
    gst_rtmp_sink_stop(bsink);
    return gst_rtmp_sink_start_impl(bsink);
}

static void
gst_rtmp_sink_parse_specific_buffer(GstRTMPSink* sink, GstBuffer* buf) {
    guint8* data = GST_BUFFER_DATA(buf);
    gint32 len = GST_BUFFER_SIZE(buf);
    gint32 datalen = 0;

    guint8 type = data[0];

    if (len < 11) {
        return;
    }


    datalen = ((guint32)data[1] << 16 | (guint32)data[2] << 8 | (guint32)data[3])  + 15;

    if (len < datalen) {
        GST_ELEMENT_WARNING(sink, STREAM, FORMAT, (NULL), ("flv pkt is not complete"));
        return;
    }

    switch (type) {
    case 8: {
        if (data[12] == 0) {
            if (sink->audio_codec_data) {
                gst_buffer_unref(sink->audio_codec_data);
                sink->audio_codec_data = NULL;
            }

            sink->audio_codec_data = gst_buffer_copy(buf);
            GST_BUFFER_TIMESTAMP(sink->audio_codec_data) = GST_CLOCK_TIME_NONE;

            if (len > datalen) {
                GST_BUFFER_SIZE(sink->audio_codec_data) = datalen;
            }
        }
    }
    break;

    case 9: {
        if (data[12] == 0) {
            if (sink->video_codec_data) {
                gst_buffer_unref(sink->video_codec_data);
                sink->video_codec_data = NULL;
            }

            sink->video_codec_data = gst_buffer_copy(buf);
            GST_BUFFER_TIMESTAMP(sink->video_codec_data) = GST_CLOCK_TIME_NONE;

            if (len > datalen) {
                GST_BUFFER_SIZE(sink->video_codec_data) = datalen;
            }
        }
    }
    break;

    case 18: {
        if (sink->metadata) {
            gst_buffer_unref(sink->metadata);
            sink->metadata = NULL;
        }

        sink->metadata = gst_buffer_copy(buf);
        GST_BUFFER_TIMESTAMP(sink->metadata) = GST_CLOCK_TIME_NONE;

        if (len > datalen) {
            GST_BUFFER_SIZE(sink->metadata) = datalen;
        }
    }
    break;

    default:
        break;
    }
}

static GstFlowReturn
gst_rtmp_sink_render(GstBaseSink* bsink, GstBuffer* buf) {
    GstRTMPSink* sink = GST_RTMP_SINK(bsink);
    GstBuffer* reffed_buf = NULL;
    GstBuffer* header = NULL;
    GstBuffer* videocodec = NULL;
    GstBuffer* audiocodec = NULL;
    GstBuffer* metadata = NULL;
    gboolean restarted = FALSE;

    gst_rtmp_sink_parse_specific_buffer(sink, buf);

    if (sink->restart) {
        gint64 time_now = g_get_real_time();

        if (time_now > sink->retry_time && time_now - sink->retry_time < 3000000) {
            return GST_FLOW_OK;
        }

        header = gst_buffer_ref(sink->header);

        if (sink->has_audio && sink->audio_codec_data) {
            audiocodec = gst_buffer_ref(sink->audio_codec_data);
        }

        if (sink->has_video && sink->video_codec_data) {
            videocodec = gst_buffer_ref(sink->video_codec_data);
        }

        if (sink->metadata) {
            metadata = gst_buffer_ref(sink->metadata);
        }

        restarted = gst_rtmp_sink_restart(bsink);

        gst_buffer_replace(&sink->header, header);
        gst_buffer_unref(header);
        header = NULL;

        if (videocodec) {
            gst_buffer_replace(&sink->video_codec_data, videocodec);
            gst_buffer_unref(videocodec);
            videocodec = NULL;
        }

        if (audiocodec) {
            gst_buffer_replace(&sink->audio_codec_data, audiocodec);
            gst_buffer_unref(audiocodec);
            audiocodec = NULL;
        }

        if (metadata) {
            gst_buffer_replace(&sink->metadata, metadata);
            gst_buffer_unref(metadata);
            metadata = NULL;
        }

        if (!restarted) {
            GST_ELEMENT_ERROR(sink, RESOURCE, OPEN_WRITE, (NULL),
                              ("Could not restart RTMP stream"));

            return GST_FLOW_UNEXPECTED;
        }

        sink->retry_time = g_get_real_time();
    }

    if (sink->first || sink->restart) {
        /* open the connection */
        if (!sink->rtmp) {
            return GST_FLOW_ERROR;
        }

        if (!RTMP_IsConnected(sink->rtmp)) {
            if (!RTMP_Connect(sink->rtmp, NULL)
                    || !RTMP_ConnectStream(sink->rtmp, 0)) {
                if (sink->first) {
                    GST_ELEMENT_ERROR(sink, RESOURCE, OPEN_WRITE, (NULL),
                                      ("Could not connect to RTMP stream \"%s\" for writing", sink->uri));

                    if (sink->rtmp) {
                        RTMP_Free(sink->rtmp);
                        sink->rtmp = NULL;
                    }

                    if (sink->rtmp_uri) {
                        g_free(sink->rtmp_uri);
                        sink->rtmp_uri = NULL;
                    }

                    return GST_FLOW_ERROR;
                } else { /* restart */
                    GST_ELEMENT_WARNING(sink, RESOURCE, OPEN_WRITE, (NULL),
                                        ("Could not connect to RTMP stream \"%s\" for writing", sink->uri));
                    return GST_FLOW_OK;
                }
            }

            GST_DEBUG_OBJECT(sink, "Opened connection to %s", sink->rtmp_uri);
        }


        if (sink->restart) {
            GstBuffer* newbuf = NULL;
            GstBuffer* newbuf1 = NULL;
            newbuf = gst_buffer_ref(sink->header);

            if (sink->metadata) {
                newbuf1 = gst_buffer_merge(newbuf, sink->metadata);
                gst_buffer_unref(newbuf);
                newbuf = newbuf1;
                newbuf1 = NULL;
            }

            if (sink->video_codec_data) {
                newbuf1 = gst_buffer_merge(newbuf, sink->video_codec_data);
                gst_buffer_unref(newbuf);
                newbuf = newbuf1;
                newbuf1 = NULL;
            }

            if (sink->audio_codec_data) {
                newbuf1 = gst_buffer_merge(newbuf, sink->audio_codec_data);
                gst_buffer_unref(newbuf);
                newbuf = newbuf1;
                newbuf1 = NULL;
            }

            gst_buffer_replace(&sink->cache, newbuf);
            gst_buffer_unref(newbuf);

            sink->restart = FALSE;
        } else { /*first*/
            guint8* data = GST_BUFFER_DATA(buf);
            gint32 len = GST_BUFFER_SIZE(buf);
            gint32 hdrlen = 0;

            if (len < 9  || (data[0] != 'F' || data[1] != 'L' || data[2] != 'V')) {
                GST_ELEMENT_ERROR(sink, STREAM, FORMAT, (NULL),
                                  ("FLV header format error"));
                return GST_FLOW_ERROR;
            }

            sink->first = FALSE;

            sink->has_video = !!(data[4] & 1);
            sink->has_audio = !!(data[4] & 4);

            hdrlen = ((gint32)data[5] << 24 | (gint32)data[6] << 16 | (gint32)data[7] << 8 |
                      (gint32)data[8]) + 4;

            sink->header = gst_buffer_copy(buf);
            GST_BUFFER_TIMESTAMP(sink->header) = GST_CLOCK_TIME_NONE;

            if (len <= hdrlen) {
                GST_LOG_OBJECT(sink, "Caching first buffer of size %" G_GSIZE_FORMAT
                               " for concatenation", GST_BUFFER_SIZE(buf));
                gst_buffer_replace(&sink->cache, buf);
                return GST_FLOW_OK;
            }

            GST_BUFFER_SIZE(sink->header) = hdrlen;
        }
    }

    if (sink->cache) {
        GST_LOG_OBJECT(sink, "Joining 2nd buffer of size %" G_GSIZE_FORMAT
                       " to cached buf", GST_BUFFER_SIZE(buf));
        reffed_buf = gst_buffer_merge(sink->cache, buf);
        GST_BUFFER_OFFSET(reffed_buf) = GST_BUFFER_OFFSET(buf);
        GST_BUFFER_TIMESTAMP(reffed_buf) = GST_BUFFER_TIMESTAMP(buf);

        buf = reffed_buf;
        gst_buffer_unref(sink->cache);
        sink->cache = NULL;
    }

    GST_LOG_OBJECT(sink, "Sending %" G_GSIZE_FORMAT " bytes to RTMP server",
                   GST_BUFFER_SIZE(buf));

#ifdef _DEBUG
    {
        static int g_size = 0;
        char msg[120];
        g_size += GST_BUFFER_SIZE(buf);

        sprintf(msg, "rtmp sink %d,%d\r\n",  GST_BUFFER_SIZE(buf), g_size);
        //OutputDebugString(msg);
    }
#endif

    if (RTMP_Write(sink->rtmp, GST_BUFFER_DATA(buf) , GST_BUFFER_SIZE(buf)) <= 0) {
        GST_ELEMENT_WARNING(sink, RESOURCE, WRITE, (NULL),
                            ("Could not write RTMP stream to \"%s\"", sink->uri));
        sink->restart = TRUE;
        sink->retry_time = 0;
    }

    if (reffed_buf) {
        gst_buffer_unref(reffed_buf);
    }

    return GST_FLOW_OK;
}

/*
 * URI interface support.
 */
static GstURIType
gst_rtmp_sink_uri_get_type(void) {
    return GST_URI_SINK;
}

static const gchar* const*
gst_rtmp_sink_uri_get_protocols(void) {
    static const gchar* protocols[] =
    { "rtmp", "rtmpt", "rtmps", "rtmpe", "rtmfp", "rtmpte", "rtmpts", NULL };

    return protocols;
}

static const gchar*
gst_rtmp_sink_uri_get_uri(GstURIHandler* handler) {
    GstRTMPSink* sink = GST_RTMP_SINK(handler);

    return sink->uri;
}

static gboolean
gst_rtmp_sink_uri_set_uri(GstURIHandler* handler, const gchar* uri) {
    GstRTMPSink* sink = GST_RTMP_SINK(handler);
    gboolean ret = TRUE;

    if (GST_STATE(sink) >= GST_STATE_PAUSED) {
        /*g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_STATE,
            "Changing the URI on rtmpsink when it is running is not supported");*/
        return FALSE;
    }

    g_free(sink->uri);
    sink->uri = NULL;

    if (uri != NULL) {
        int protocol;
        AVal host;
        unsigned int port;
        AVal playpath, app;

        if (!RTMP_ParseURL(uri, &protocol, &host, &port, &playpath, &app) ||
                !host.av_len || !playpath.av_len) {
            GST_ELEMENT_ERROR(sink, RESOURCE, OPEN_WRITE,
                              ("Failed to parse URI %s", uri), (NULL));
            ret = FALSE;
        } else {
            sink->uri = g_strdup(uri);
        }

        if (playpath.av_val) {
            RTMP_FreeMemory(playpath.av_val);
        }
    }

    if (ret) {
        GST_DEBUG_OBJECT(sink, "Changed URI to %s", GST_STR_NULL(uri));
    }

    return ret;
}

static void
gst_rtmp_sink_uri_handler_init(gpointer g_iface, gpointer iface_data) {
    GstURIHandlerInterface* iface = (GstURIHandlerInterface*) g_iface;

    iface->get_type = gst_rtmp_sink_uri_get_type;
    iface->get_protocols = gst_rtmp_sink_uri_get_protocols;
    iface->get_uri = gst_rtmp_sink_uri_get_uri;
    iface->set_uri = gst_rtmp_sink_uri_set_uri;
}

static void
gst_rtmp_sink_set_property(GObject* object, guint prop_id,
                           const GValue* value, GParamSpec* pspec) {
    GstRTMPSink* sink = GST_RTMP_SINK(object);

    switch (prop_id) {
    case PROP_LOCATION:
        gst_rtmp_sink_uri_set_uri(GST_URI_HANDLER(sink),
                                  g_value_get_string(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gst_rtmp_sink_get_property(GObject* object, guint prop_id,
                           GValue* value, GParamSpec* pspec) {
    GstRTMPSink* sink = GST_RTMP_SINK(object);

    switch (prop_id) {
    case PROP_LOCATION:
        g_value_set_string(value, sink->uri);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}
