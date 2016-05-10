/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@entropywave.com>
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdecklinkvideosrc.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC(gst_decklink_video_src_debug);
#define GST_CAT_DEFAULT gst_decklink_video_src_debug

#define DEFAULT_BUFFER_SIZE (5)
#define DEFAULT_PROVIDE_CLOCK TRUE

enum {
    PROP_0,
    PROP_CONNECTION,
    PROP_MODE,
    PROP_DEVICE_NUMBER,
    PROP_BUFFER_SIZE,
    PROP_DETECT_SIGNAL,
    PROP_ALLOW_DETECT,
    PROP_PROVIDE_CLOCK
};

typedef struct {
    IDeckLinkVideoInputFrame* frame;
    GstClockTime capture_time;
} CaptureFrame;

static void
capture_frame_free(void* data) {
    CaptureFrame* frame = (CaptureFrame*) data;

    frame->frame->Release();
    g_free(frame);
}

typedef struct {
    IDeckLinkVideoInputFrame* frame;
    IDeckLinkInput* input;
} VideoFrame;

static void
video_frame_free(void* data) {
    VideoFrame* frame = (VideoFrame*) data;

    frame->frame->Release();
    frame->input->Release();
    g_free(frame);
}

static void gst_decklink_video_src_set_property(GObject* object,
        guint property_id, const GValue* value, GParamSpec* pspec);
static void gst_decklink_video_src_get_property(GObject* object,
        guint property_id, GValue* value, GParamSpec* pspec);
static void gst_decklink_video_src_finalize(GObject* object);

static GstStateChangeReturn
gst_decklink_video_src_change_state(GstElement* element,
                                    GstStateChange transition);
static GstClock* gst_decklink_video_src_provide_clock(GstElement* element);

static GstCaps* gst_decklink_video_src_get_caps(GstBaseSrc* bsrc/*,
    GstCaps * filter*/);
static gboolean gst_decklink_video_src_query(GstBaseSrc* bsrc,
        GstQuery* query);
static gboolean gst_decklink_video_src_unlock(GstBaseSrc* bsrc);
static gboolean gst_decklink_video_src_unlock_stop(GstBaseSrc* bsrc);

static GstFlowReturn gst_decklink_video_src_create(GstPushSrc* psrc,
        GstBuffer** buffer);

static gboolean gst_decklink_video_src_open(GstDecklinkVideoSrc* self);
static gboolean gst_decklink_video_src_close(GstDecklinkVideoSrc* self);
static gboolean gst_decklink_video_src_set_clock_func(GstElement* element, GstClock* clock);

#define parent_class gst_decklink_video_src_parent_class
G_DEFINE_TYPE(GstDecklinkVideoSrc, gst_decklink_video_src, GST_TYPE_PUSH_SRC);

static void
gst_decklink_video_src_class_init(GstDecklinkVideoSrcClass* klass) {
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass* element_class = GST_ELEMENT_CLASS(klass);
    GstBaseSrcClass* basesrc_class = GST_BASE_SRC_CLASS(klass);
    GstPushSrcClass* pushsrc_class = GST_PUSH_SRC_CLASS(klass);
    GstCaps* templ_caps;

    gobject_class->set_property = gst_decklink_video_src_set_property;
    gobject_class->get_property = gst_decklink_video_src_get_property;
    gobject_class->finalize = gst_decklink_video_src_finalize;

    element_class->change_state =
        GST_DEBUG_FUNCPTR(gst_decklink_video_src_change_state);
    element_class->provide_clock =
        GST_DEBUG_FUNCPTR(gst_decklink_video_src_provide_clock);

    basesrc_class->get_caps = GST_DEBUG_FUNCPTR(gst_decklink_video_src_get_caps);
    basesrc_class->query = GST_DEBUG_FUNCPTR(gst_decklink_video_src_query);
    basesrc_class->unlock = GST_DEBUG_FUNCPTR(gst_decklink_video_src_unlock);
    basesrc_class->unlock_stop =
        GST_DEBUG_FUNCPTR(gst_decklink_video_src_unlock_stop);

    pushsrc_class->create = GST_DEBUG_FUNCPTR(gst_decklink_video_src_create);

    element_class->set_clock = GST_DEBUG_FUNCPTR(gst_decklink_video_src_set_clock_func);


    g_object_class_install_property(gobject_class, PROP_CONNECTION,
                                    g_param_spec_enum("connection", "input channel",
                                            "Video Mode to use for playback",
                                            GST_TYPE_DECKLINK_CONNECTION, GST_DECKLINK_CONNECTION_SDI,
                                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                                    G_PARAM_CONSTRUCT)));

    g_object_class_install_property(gobject_class, PROP_MODE,
                                    g_param_spec_enum("mode", "Playback Mode",
                                            "Video Mode to use for playback",
                                            GST_TYPE_DECKLINK_MODE, GST_DECKLINK_MODE_NTSC,
                                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                                    G_PARAM_CONSTRUCT)));


    g_object_class_install_property(gobject_class, PROP_DEVICE_NUMBER,
                                    g_param_spec_int("device-number", "Device number",
                                            "Output device instance to use", 0, G_MAXINT, 0,
                                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                                    G_PARAM_CONSTRUCT)));

    g_object_class_install_property(gobject_class, PROP_DETECT_SIGNAL,
                                    g_param_spec_enum("detect-signal", "Detect input-signal type",
                                            "Auto Detect input signal type",
                                            GST_TYPE_DECKLINK_MODE, GST_DECKLINK_MODE_720p50,
                                            (GParamFlags)(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobject_class, PROP_ALLOW_DETECT,
                                    g_param_spec_boolean("allow-detect", "allow or not detect signal",
                                            "allow or not detect signal", FALSE,
                                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));


    g_object_class_install_property(gobject_class, PROP_BUFFER_SIZE,
                                    g_param_spec_uint("buffer-size", "Buffer Size",
                                            "Size of internal buffer in number of video frames", 1,
                                            G_MAXINT, DEFAULT_BUFFER_SIZE,
                                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobject_class, PROP_PROVIDE_CLOCK,
                                    g_param_spec_boolean("provide-clock", "Provide Clock",
                                            "Provide a clock to be used as the global pipeline clock",
                                            DEFAULT_PROVIDE_CLOCK, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));



    templ_caps = gst_decklink_mode_get_template_caps();
    gst_element_class_add_pad_template(element_class,
                                       gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, templ_caps));

    gst_element_class_set_details_simple(element_class, "Decklink Video Source",
                                         "Video/Src", "Decklink Source", "David Schleef <ds@entropywave.com>, "
                                         "Sebastian Dröge <sebastian@centricular.com>");

    GST_DEBUG_CATEGORY_INIT(gst_decklink_video_src_debug, "decklinkvideosrc",
                            0, "debug category for decklinkvideosrc element");

    /*#ifdef _DEBUG
      gst_debug_set_threshold_for_name("decklinkvideosrc", GST_LEVEL_INFO);;
    #endif*/
}

static void
gst_decklink_video_src_init(GstDecklinkVideoSrc* self) {
    self->mode = GST_DECKLINK_MODE_NTSC;
    self->connection = GST_DECKLINK_CONNECTION_SDI;
    self->device_number = 0;
    self->buffer_size = DEFAULT_BUFFER_SIZE;
    self->allow_detect = FALSE;
    self->provide_clock = TRUE;

    gst_base_src_set_live(GST_BASE_SRC(self), TRUE);
    gst_base_src_set_format(GST_BASE_SRC(self), GST_FORMAT_TIME);

    g_mutex_init(&self->lock);
    g_cond_init(&self->cond);

    g_queue_init(&self->current_frames);
}

void
gst_decklink_video_src_set_provide_clock(GstDecklinkVideoSrc* src,
        gboolean provide) {
    g_return_if_fail(GST_IS_DECKLINK_VIDEO_SRC(src));

    GST_OBJECT_LOCK(src);
    src->provide_clock = provide;
    GST_OBJECT_UNLOCK(src);
}

gboolean
gst_decklink_video_src_get_provide_clock(GstDecklinkVideoSrc* src) {
    gboolean result;

    g_return_val_if_fail(GST_IS_DECKLINK_VIDEO_SRC(src), FALSE);

    GST_OBJECT_LOCK(src);
    result = src->provide_clock;
    GST_OBJECT_UNLOCK(src);

    return result;
}


void
gst_decklink_video_src_set_property(GObject* object, guint property_id,
                                    const GValue* value, GParamSpec* pspec) {
    GstDecklinkVideoSrc* self = GST_DECKLINK_VIDEO_SRC_CAST(object);

    switch (property_id) {
    case PROP_MODE:
        self->mode = (GstDecklinkModeEnum) g_value_get_enum(value);
        break;

    case  PROP_CONNECTION:
        self->connection = (GstDecklinkConnectionEnum) g_value_get_enum(value);
        break;

    case PROP_DEVICE_NUMBER:
        self->device_number = g_value_get_int(value);
        break;

    case PROP_BUFFER_SIZE:
        self->buffer_size = g_value_get_uint(value);
        break;

    case PROP_ALLOW_DETECT:
        self->allow_detect = g_value_get_boolean(value);
        break;

    case PROP_PROVIDE_CLOCK:
        gst_decklink_video_src_set_provide_clock(self, g_value_get_boolean(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static gint
gst_decklink_video_src_detect_signal(GstDecklinkVideoSrc* src) {
    gint val = -1;

    BMDVideoConnection vc = gst_decklink_get_video_connection(src->connection);

    val = gst_decklink_detect_signal_nth_input(src->device_number, vc, &src->allow_detect);

    return val;
}


void
gst_decklink_video_src_get_property(GObject* object, guint property_id,
                                    GValue* value, GParamSpec* pspec) {
    GstDecklinkVideoSrc* self = GST_DECKLINK_VIDEO_SRC_CAST(object);

    switch (property_id) {
    case PROP_MODE:
        g_value_set_enum(value, self->mode);
        break;

    case PROP_CONNECTION:
        g_value_set_enum(value, self->connection);
        break;

    case PROP_DEVICE_NUMBER:
        g_value_set_int(value, self->device_number);
        break;

    case PROP_BUFFER_SIZE:
        g_value_set_uint(value, self->buffer_size);
        break;

    case PROP_DETECT_SIGNAL: {
        int v = gst_decklink_video_src_detect_signal(self);

        if (v > GST_DECKLINK_MODE_UNK) {
            g_value_set_enum(value, v);
        } else {
            g_value_unset(value);
        }

        //g_value_init(value,G_TYPE_INVALID);
    }
    break;

    case PROP_ALLOW_DETECT:
        g_value_set_boolean(value, self->allow_detect);
        break;

    case PROP_PROVIDE_CLOCK:
        g_value_set_boolean(value, gst_decklink_video_src_get_provide_clock(self));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void
gst_decklink_video_src_finalize(GObject* object) {
    GstDecklinkVideoSrc* self = GST_DECKLINK_VIDEO_SRC_CAST(object);

    g_mutex_clear(&self->lock);
    g_cond_clear(&self->cond);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static GstCaps*
gst_decklink_video_src_get_caps(GstBaseSrc* bsrc/*, GstCaps * filter*/) {
    GstDecklinkVideoSrc* self = GST_DECKLINK_VIDEO_SRC_CAST(bsrc);
    GstCaps* mode_caps, *caps;

    mode_caps = gst_decklink_mode_get_caps(self->mode);
    //if (filter) {
    //  caps =
    //      gst_caps_intersect_full (filter, mode_caps, GST_CAPS_INTERSECT_FIRST);
    //  gst_caps_unref (mode_caps);
    //} else {
    caps = mode_caps;
    //}

    return caps;
}
static gboolean
gst_decklink_video_src_set_clock_func(GstElement* element, GstClock* clock) {
    return TRUE;
}
//#ifdef _DEBUG
//GstClockTime gst_time = 0;
//#endif

static void
gst_decklink_video_src_got_frame(GstElement* element,
                                 IDeckLinkVideoInputFrame* frame, GstClockTime capture_time) {
    GstDecklinkVideoSrc* self = GST_DECKLINK_VIDEO_SRC_CAST(element);

    GST_OBJECT_LOCK(element);

    if (element->clock && self->input->clock != element->clock) {
        capture_time = gst_clock_get_time(element->clock) - element->base_time;
    }

    GST_OBJECT_UNLOCK(element);

    //#ifdef _DEBUG
    //  {
    //      char msg[100];
    //      sprintf(msg, "%" G_GINT64_FORMAT " \t %" G_GINT64_FORMAT "\r\n", capture_time - gst_time, capture_time);
    //      OutputDebugStringA(msg);
    //      gst_time = capture_time;
    //  }
    //#endif

    GST_LOG_OBJECT(self, "Got video frame at %" GST_TIME_FORMAT,
                   GST_TIME_ARGS(capture_time));

    g_mutex_lock(&self->lock);

    if (!self->flushing) {
        CaptureFrame* f;

        while (g_queue_get_length(&self->current_frames) >= self->buffer_size) {
            f = (CaptureFrame*) g_queue_pop_head(&self->current_frames);
            GST_WARNING_OBJECT(self, "Dropping old frame at %" GST_TIME_FORMAT,
                               GST_TIME_ARGS(f->capture_time));
            capture_frame_free(f);
        }

        f = (CaptureFrame*) g_malloc0(sizeof(CaptureFrame));
        f->frame = frame;
        f->capture_time = capture_time;
        frame->AddRef();
        g_queue_push_tail(&self->current_frames, f);
        g_cond_signal(&self->cond);
    }

    g_mutex_unlock(&self->lock);
}

static GstFlowReturn
gst_decklink_video_src_create(GstPushSrc* bsrc, GstBuffer** buffer) {
    GstDecklinkVideoSrc* self = GST_DECKLINK_VIDEO_SRC_CAST(bsrc);
    GstFlowReturn flow_ret = GST_FLOW_OK;
    const guint8* data;
    gsize data_size;
    VideoFrame* vf;
    CaptureFrame* f;
    GstClockTime timestamp, duration;

    g_mutex_lock(&self->lock);

    while (g_queue_is_empty(&self->current_frames) && !self->flushing) {
        g_cond_wait(&self->cond, &self->lock);
    }

    f = (CaptureFrame*) g_queue_pop_head(&self->current_frames);
    g_mutex_unlock(&self->lock);

    if (self->flushing) {
        if (f) {
            capture_frame_free(f);
        }

        return GST_FLOW_WRONG_STATE;
    }

    f->frame->GetBytes((gpointer*) & data);
    long height = f->frame->GetHeight();
    long rlen = f->frame->GetRowBytes();
    data_size = rlen * height;


    vf = (VideoFrame*) g_malloc0(sizeof(VideoFrame));




    *buffer = gst_buffer_new();
    GST_BUFFER_MALLOCDATA(*buffer) = (guint8*)(vf);
    GST_BUFFER_FREE_FUNC(*buffer) = video_frame_free;
    GST_BUFFER_DATA(*buffer) = const_cast<guint8*>(data);
    GST_BUFFER_SIZE(*buffer) = data_size;
    GST_BUFFER_OFFSET(*buffer) = 0;
    GST_BUFFER_OFFSET_END(*buffer) = data_size;
    GST_BUFFER_FLAG_SET(*buffer, GST_MINI_OBJECT_FLAG_READONLY);

    if (self->input->mode_changed) {
        self->input->mode_changed = FALSE;
        GstCaps* caps = gst_decklink_mode_get_caps((GstDecklinkModeEnum)gst_decklink_get_mode_enum(self->input->mode->mode));
        gst_video_info_from_caps(&self->info, caps);
        gst_caps_replace(&GST_BUFFER_CAPS(*buffer), caps);
        gst_caps_unref(caps);
    }

    //*buffer =
    //    gst_buffer_new_wrapped_full ((GstMemoryFlags) GST_MEMORY_FLAG_READONLY,
    //    (gpointer) data, data_size, 0, data_size, vf,
    //    (GDestroyNotify) video_frame_free);

    vf->frame = f->frame;
    f->frame->AddRef();
    vf->input = self->input->input;
    vf->input->AddRef();

    duration =
        gst_util_uint64_scale_int(GST_SECOND, self->info.fps_d,
                                  self->info.fps_n);

    // Our capture time is the end timestamp, subtract the
    // duration to get the start timestamp
    if (f->capture_time >= duration) {
        timestamp = f->capture_time - duration;
    } else {
        timestamp = 0;
    }

    GST_BUFFER_TIMESTAMP(*buffer) = timestamp;
    GST_BUFFER_DURATION(*buffer) = duration;

#ifdef _DEBUG
    GST_WARNING_OBJECT(bsrc, "Timestamp %" G_GINT64_FORMAT, timestamp);
#else
    GST_INFO_OBJECT(bsrc, "Timestamp %" G_GINT64_FORMAT, timestamp);
#endif
    capture_frame_free(f);

    return flow_ret;
}

static gboolean
gst_decklink_video_src_query(GstBaseSrc* bsrc, GstQuery* query) {
    GstDecklinkVideoSrc* self = GST_DECKLINK_VIDEO_SRC_CAST(bsrc);
    gboolean ret = TRUE;

    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_LATENCY: {
        if (self->input) {
            GstClockTime min, max;
            const GstDecklinkMode* mode;

            mode = gst_decklink_get_mode(self->mode);

            min =
                gst_util_uint64_scale_ceil(GST_MSECOND, mode->fps_d, mode->fps_n);
            max = self->buffer_size * min;

            gst_query_set_latency(query, TRUE, min, max);
            ret = TRUE;
        } else {
            ret = FALSE;
        }

        break;
    }

    default:
        ret = GST_BASE_SRC_CLASS(parent_class)->query(bsrc, query);
        break;
    }

    return ret;
}

static gboolean
gst_decklink_video_src_unlock(GstBaseSrc* bsrc) {
    GstDecklinkVideoSrc* self = GST_DECKLINK_VIDEO_SRC_CAST(bsrc);

    g_mutex_lock(&self->lock);
    self->flushing = TRUE;
    g_cond_signal(&self->cond);
    g_mutex_unlock(&self->lock);

    return TRUE;
}

static gboolean
gst_decklink_video_src_unlock_stop(GstBaseSrc* bsrc) {
    GstDecklinkVideoSrc* self = GST_DECKLINK_VIDEO_SRC_CAST(bsrc);

    g_mutex_lock(&self->lock);
    self->flushing = FALSE;
    g_queue_foreach(&self->current_frames, (GFunc) capture_frame_free, NULL);
    g_queue_clear(&self->current_frames);
    g_mutex_unlock(&self->lock);

    return TRUE;
}

static gboolean
gst_decklink_video_src_open(GstDecklinkVideoSrc* self) {
    const GstDecklinkMode* mode;

    GstCaps* caps;
    HRESULT ret;

    GST_DEBUG_OBJECT(self, "Starting");

    self->input =
        gst_decklink_acquire_nth_input(self->device_number,
                                       GST_ELEMENT_CAST(self), FALSE);

    if (!self->input) {
        GST_ERROR_OBJECT(self, "Failed to acquire input");
        return FALSE;
    }

    mode = gst_decklink_get_mode(self->mode);
    g_assert(mode != NULL);

    ret = gst_decklink_set_connection(self->input, gst_decklink_get_video_connection(self->connection));

    if (ret != S_OK) {
        GST_ERROR_OBJECT(self, "Failed to enable video connection");
        gst_decklink_release_nth_input(self->device_number,
                                       GST_ELEMENT_CAST(self), FALSE);
        return FALSE;
    }

    if (mode->mode == bmdModeUnknown) {
        if (!self->input->support_detection) {
            GST_ERROR_OBJECT(self, "the card doesn't support auto mode.");
            gst_decklink_release_nth_input(self->device_number,
                                           GST_ELEMENT_CAST(self), FALSE);
            return FALSE;
        } else {
            ret = gst_decklink_set_mode(self->input, bmdModeNTSC, bmdFormat8BitYUV,
                                        bmdVideoInputEnableFormatDetection);
        }
    } else {
        ret = gst_decklink_set_mode(self->input, mode->mode, bmdFormat8BitYUV, bmdVideoInputFlagDefault);
    }

    if (ret != S_OK) {
        GST_ERROR_OBJECT(self, "Failed to enable video input");
        gst_decklink_release_nth_input(self->device_number,
                                       GST_ELEMENT_CAST(self), FALSE);
        return FALSE;
    }

    g_mutex_lock(&self->lock);

    g_mutex_lock(&self->input->lock);
    self->input->mode = mode;
    self->input->got_video_frame = gst_decklink_video_src_got_frame;
    g_mutex_unlock(&self->input->lock);

    caps = gst_decklink_mode_get_caps(self->mode);
    gst_video_info_from_caps(&self->info, caps);
    gst_caps_unref(caps);
    g_mutex_unlock(&self->lock);

    return TRUE;
}

static gboolean
gst_decklink_video_src_close(GstDecklinkVideoSrc* self) {

    GST_DEBUG_OBJECT(self, "Stopping");

    if (self->input) {
        g_mutex_lock(&self->input->lock);
        self->input->got_video_frame = NULL;
        self->input->mode = NULL;
        g_mutex_unlock(&self->input->lock);

        self->input->input->DisableVideoInput();
        gst_decklink_release_nth_input(self->device_number,
                                       GST_ELEMENT_CAST(self), FALSE);
        self->input = NULL;
    }

    return TRUE;
}

static GstStateChangeReturn
gst_decklink_video_src_change_state(GstElement* element,
                                    GstStateChange transition) {
    GstDecklinkVideoSrc* self = GST_DECKLINK_VIDEO_SRC_CAST(element);
    GstStateChangeReturn ret;


    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        if (!gst_decklink_video_src_open(self)) {
            ret = GST_STATE_CHANGE_FAILURE;
            goto out;
        }

        break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
        gst_element_post_message(element,
                                 gst_message_new_clock_provide(GST_OBJECT_CAST(element),
                                         self->input->clock, TRUE));
        self->flushing = FALSE;
        break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING: {
        GstClock* clock;

        clock = gst_element_get_clock(GST_ELEMENT_CAST(self));

        if (clock && clock != self->input->clock) {
            gst_clock_set_master(self->input->clock, clock);
        }

        if (clock) {
            gst_object_unref(clock);
        }

        break;
    }

    default:
        break;
    }

    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        return ret;
    }

    switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        gst_element_post_message(element,
                                 gst_message_new_clock_lost(GST_OBJECT_CAST(element),
                                         self->input->clock));
        gst_clock_set_master(self->input->clock, NULL);

        g_queue_foreach(&self->current_frames, (GFunc) capture_frame_free, NULL);
        g_queue_clear(&self->current_frames);
        break;

    case GST_STATE_CHANGE_PLAYING_TO_PAUSED: {
        HRESULT res;

        GST_DEBUG_OBJECT(self, "Stopping streams");
        res = gst_decklink_input_stop_streams(self->input);

        if (res != S_OK) {
            GST_ELEMENT_ERROR(self, STREAM, FAILED,
                              (NULL), ("Failed to stop streams: 0x%08x", res));
            ret = GST_STATE_CHANGE_FAILURE;
        }

        break;
    }

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING: {
        HRESULT res;

        GST_DEBUG_OBJECT(self, "Starting streams");
        res = gst_decklink_input_start_streams(self->input);

        if (res != S_OK) {
            GST_ELEMENT_ERROR(self, STREAM, FAILED,
                              (NULL), ("Failed to start streams: 0x%08x", res));
            ret = GST_STATE_CHANGE_FAILURE;
        }

        break;
    }

    case GST_STATE_CHANGE_READY_TO_NULL:
        gst_decklink_video_src_close(self);
        break;

    default:
        break;
    }

out:

    return ret;
}

static GstClock*
gst_decklink_video_src_provide_clock(GstElement* element) {
    GstDecklinkVideoSrc* self = GST_DECKLINK_VIDEO_SRC_CAST(element);

    if (!self->input) {
        return NULL;
    }

    if (!self->provide_clock) {
        return NULL;
    }

    return GST_CLOCK_CAST(gst_object_ref(self->input->clock));
}
