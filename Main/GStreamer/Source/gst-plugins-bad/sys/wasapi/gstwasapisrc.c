/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
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
 * SECTION:element-wasapisrc
 *
 * Provides audio capture from the Windows Audio Session API available with
 * Vista and newer.
 *
 * <refsect2>
 * <title>Example pipelines</title>
 * |[
 * gst-launch-0.10 -v wasapisrc ! fakesink
 * ]| Capture from the default audio device and render to fakesink.
 * </refsect2>
 */

#include "gstwasapisrc.h"
#include <gst/audio/gstaudioclock.h>
#include <Windows.h>

GST_DEBUG_CATEGORY_STATIC(gst_wasapi_src_debug);
#define GST_CAT_DEFAULT gst_wasapi_src_debug

enum {
    PROP_0,
    PROP_USE_OUTPUT,
    PROP_DEVICE_CHANGED,
    PROP_LAST,
};

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
        GST_PAD_SRC,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS("audio/x-raw-float, "
                        "rate = (int) [ 1, MAX ], "
                        "channels = (int) [ 1, MAX ], "
                        "endianness = (int) BYTE_ORDER, "
                        "width = (int) 32"));

static void gst_wasapi_src_dispose(GObject* object);
static void gst_wasapi_src_finalize(GObject* object);

static GstClock* gst_wasapi_src_provide_clock(GstElement* element);

static gboolean gst_wasapi_src_start(GstBaseSrc* src);
static gboolean gst_wasapi_src_start_impl(GstBaseSrc* src);
static gboolean gst_wasapi_src_stop(GstBaseSrc* src);
static gboolean gst_wasapi_src_stop_impl(GstBaseSrc* src);
static gboolean gst_wasapi_src_query(GstBaseSrc* src, GstQuery* query);

static GstFlowReturn gst_wasapi_src_create(GstPushSrc* src, GstBuffer** buf);

static GstClockTime gst_wasapi_src_get_time(GstClock* clock,
        gpointer user_data);

static
void  gst_wasapi_src_set_property(GObject*        object,
                                  guint           property_id,
                                  const GValue*   value,
                                  GParamSpec*     pspec);
static
void gst_wasapi_src_get_property(GObject*        object,
                                 guint           property_id,
                                 GValue*         value,
                                 GParamSpec*     pspec);

GST_BOILERPLATE(GstWasapiSrc, gst_wasapi_src, GstPushSrc, GST_TYPE_PUSH_SRC);

static void
gst_wasapi_src_base_init(gpointer gclass) {
    GstElementClass* element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_add_static_pad_template(element_class, &src_template);
    gst_element_class_set_details_simple(element_class, "WasapiSrc",
                                         "Source/Audio",
                                         "Stream audio from an audio capture device through WASAPI",
                                         "Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>");
}

static void
gst_wasapi_src_class_init(GstWasapiSrcClass* klass) {
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass* gstelement_class = GST_ELEMENT_CLASS(klass);
    GstBaseSrcClass* gstbasesrc_class = GST_BASE_SRC_CLASS(klass);
    GstPushSrcClass* gstpushsrc_class = GST_PUSH_SRC_CLASS(klass);

    gobject_class->dispose = gst_wasapi_src_dispose;
    gobject_class->finalize = gst_wasapi_src_finalize;

    gstelement_class->provide_clock = gst_wasapi_src_provide_clock;

    gstbasesrc_class->start = gst_wasapi_src_start;
    gstbasesrc_class->stop = gst_wasapi_src_stop;
    gstbasesrc_class->query = gst_wasapi_src_query;
    gobject_class->get_property = gst_wasapi_src_get_property;
    gobject_class->set_property = gst_wasapi_src_set_property;

    gstpushsrc_class->create = gst_wasapi_src_create;

    g_object_class_install_property(gobject_class, PROP_USE_OUTPUT, g_param_spec_boolean("use-output",
                                    "use output", "use output channel as audio source", FALSE,
                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_DEVICE_CHANGED,
                                    g_param_spec_boolean("device-changed",
                                            "device changed", "set to true if audio device has changed", FALSE,
                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    GST_DEBUG_CATEGORY_INIT(gst_wasapi_src_debug, "wasapisrc",
                            0, "Windows audio session API source");
}

static void
gst_wasapi_src_default_format(WAVEFORMATEXTENSIBLE* wfxe) {
    wfxe->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    wfxe->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    wfxe->Samples.wValidBitsPerSample = 32;
    wfxe->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfxe->Format.nChannels = 2;
    wfxe->Format.nSamplesPerSec = 48000;
    wfxe->Format.nAvgBytesPerSec = 48000 * 2 * 4;
    wfxe->Format.nBlockAlign = 8;
    wfxe->Format.wBitsPerSample = 32;
    wfxe->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
};

static void
gst_wasapi_src_init(GstWasapiSrc* self, GstWasapiSrcClass* gclass) {
    GstBaseSrc* basesrc = GST_BASE_SRC(self);

    gst_base_src_set_format(basesrc, GST_FORMAT_TIME);
    gst_base_src_set_live(basesrc, TRUE);

    self->rate = 48000;
    self->buffer_time = 100 * GST_MSECOND;
    self->period_time = 100 * GST_MSECOND;
    self->latency = GST_CLOCK_TIME_NONE;
    self->samples_per_buffer = self->rate / (GST_SECOND / self->period_time);

    self->start_time = GST_CLOCK_TIME_NONE;
    self->next_time = GST_CLOCK_TIME_NONE;
    self->use_output = FALSE;
    self->device_cookie = 0;
    self->current_device_cookie = 0;
    self->deviceLost = FALSE;
    self->sys_clock = gst_system_clock_obtain();
    self->device_id = 0;

#if GST_CHECK_VERSION(0, 10, 31) || (GST_CHECK_VERSION(0, 10, 30) && GST_VERSION_NANO > 0)
    self->clock = gst_audio_clock_new_full("GstWasapiSrcClock",
                                           gst_wasapi_src_get_time, gst_object_ref(self),
                                           (GDestroyNotify) gst_object_unref);
#else
    self->clock = gst_audio_clock_new("GstWasapiSrcClock",
                                      gst_wasapi_src_get_time, self);
#endif

    self->data_pos = 0;
    self->first = 0;
    self->time_base = 0;
    self->dev_time_base = 0;
    gst_wasapi_src_default_format(&self->wfxe);
    CoInitialize(NULL);
}

static void
gst_wasapi_src_dispose(GObject* object) {
    GstWasapiSrc* self = GST_WASAPI_SRC(object);

    if (self->clock != NULL) {
        gst_object_unref(self->clock);
        self->clock = NULL;
    }

    if (self->device_id != NULL) {
        free(self->device_id);
        self->device_id = 0;
    }

    G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
gst_wasapi_src_finalize(GObject* object) {
    GstWasapiSrc* self = GST_WASAPI_SRC(object);

    CoUninitialize();

    gst_object_unref(self->sys_clock);

    if (self->caps) {
        gst_caps_unref(self->caps);
        self->caps = NULL;
    }

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static GstClock*
gst_wasapi_src_provide_clock(GstElement* element) {
    GstWasapiSrc* self = GST_WASAPI_SRC(element);
    GstClock* clock;

    GST_OBJECT_LOCK(self);

    if (self->client_clock == NULL) {
        goto wrong_state;
    }

    clock = GST_CLOCK(gst_object_ref(self->clock));

    GST_OBJECT_UNLOCK(self);
    return clock;

    /* ERRORS */
wrong_state: {
        GST_OBJECT_UNLOCK(self);
        GST_DEBUG_OBJECT(self, "IAudioClock not acquired");
        return NULL;
    }
}

static
void  gst_wasapi_src_set_property(GObject*        object,
                                  guint           prop_id,
                                  const GValue*   value,
                                  GParamSpec*     pspec) {
    GstWasapiSrc* sel = GST_WASAPI_SRC(object);

    switch (prop_id) {
    case PROP_USE_OUTPUT:
        sel->use_output = g_value_get_boolean(value);
        break;

    case PROP_DEVICE_CHANGED:
        if (g_value_get_boolean(value)) {
            sel->device_cookie ++;
        }

        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static
void gst_wasapi_src_get_property(GObject*        object,
                                 guint           prop_id,
                                 GValue*         value,
                                 GParamSpec*     pspec) {
    GstWasapiSrc* sel = GST_WASAPI_SRC(object);

    switch (prop_id) {
    case PROP_USE_OUTPUT:
        g_value_set_boolean(value, sel->use_output);
        break;

    case PROP_DEVICE_CHANGED:
        g_value_set_boolean(value, sel->current_device_cookie != sel->device_cookie);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}
static gboolean
gst_wasapi_src_start(GstBaseSrc* src) {
    GstWasapiSrc* self = GST_WASAPI_SRC(src);
    self->first = TRUE;
    self->data_pos = 0;
    self->time_base = 0;

    if (!gst_wasapi_src_start_impl(src)) {
        self->deviceLost = TRUE;
    } else {
        self->deviceLost = FALSE;
        self->next_time = gst_clock_get_time(self->sys_clock);
    }

    return TRUE;
}
static gboolean
gst_wasapi_src_start_impl(GstBaseSrc* src) {
    GstWasapiSrc* self = GST_WASAPI_SRC(src);
    gboolean res = FALSE;
    IAudioClient* client = NULL;
    IAudioClock* client_clock = NULL;
    guint64 client_clock_freq = 0;
    IAudioCaptureClient* capture_client = NULL;
    HRESULT hr;
    gint dcookie;


    dcookie = self->device_cookie;

    if (!gst_wasapi_util_get_default_device_client(GST_ELEMENT(self),
            self->use_output ? FALSE : TRUE, self->buffer_time, self->period_time,
            self->use_output ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0, &client,
            &self->latency, &self->wfxe, &self->device_id)) {
        goto beach;
    }

    self->samples_per_buffer = self->wfxe.Format.nSamplesPerSec / (GST_SECOND / self->period_time);


    hr = IAudioClient_GetService(client, &IID_IAudioClock, &client_clock);

    if (hr != S_OK) {
        GST_ERROR_OBJECT(self, "IAudioClient::GetService (IID_IAudioClock) "
                         "failed");
        goto beach;
    }

    hr = IAudioClock_GetFrequency(client_clock, &client_clock_freq);

    if (hr != S_OK) {
        GST_ERROR_OBJECT(self, "IAudioClock::GetFrequency () failed");
        goto beach;
    }

    hr = IAudioClient_GetService(client, &IID_IAudioCaptureClient,
                                 &capture_client);

    if (hr != S_OK) {
        GST_ERROR_OBJECT(self, "IAudioClient::GetService "
                         "(IID_IAudioCaptureClient) failed");
        goto beach;
    }

    hr = IAudioClient_Start(client);

    if (hr != S_OK) {
        GST_ERROR_OBJECT(self, "IAudioClient::Start failed");
        goto beach;
    }

    self->client = client;
    self->client_clock = client_clock;
    self->client_clock_freq = client_clock_freq;
    self->capture_client = capture_client;
    self->dev_time_base = gst_wasapi_src_get_time(NULL, self);

    self->current_device_cookie = dcookie;

    res = TRUE;

beach:

    if (!res) {
        if (capture_client != NULL) {
            IUnknown_Release(capture_client);
        }

        if (client_clock != NULL) {
            IUnknown_Release(client_clock);
        }

        if (client != NULL) {
            IUnknown_Release(client);
        }
    }

    return res;
}

static gboolean
gst_wasapi_src_stop(GstBaseSrc* src) {
    GstWasapiSrc* self = GST_WASAPI_SRC(src);

    self->first = TRUE;
    self->data_pos = 0;
    self->time_base = 0;

    if (self->caps) {
        gst_caps_unref(self->caps);
        self->caps = NULL;
    }

    return gst_wasapi_src_stop_impl(src);
}

static gboolean
gst_wasapi_src_stop_impl(GstBaseSrc* src) {
    GstWasapiSrc* self = GST_WASAPI_SRC(src);

    if (self->client != NULL) {
        IAudioClient_Stop(self->client);
    }

    if (self->capture_client != NULL) {
        IUnknown_Release(self->capture_client);
        self->capture_client = NULL;
    }

    if (self->client_clock != NULL) {
        IUnknown_Release(self->client_clock);
        self->client_clock = NULL;
    }

    if (self->client != NULL) {
        IUnknown_Release(self->client);
        self->client = NULL;
    }

    if (self->device_id != NULL) {
        free(self->device_id);
        self->device_id = 0;
    }

    return TRUE;
}

static gboolean
gst_wasapi_src_query(GstBaseSrc* src, GstQuery* query) {
    GstWasapiSrc* self = GST_WASAPI_SRC(src);
    gboolean ret = FALSE;

    GST_DEBUG_OBJECT(self, "query for %s",
                     gst_query_type_get_name(GST_QUERY_TYPE(query)));

    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_LATENCY: {
        GstClockTime min_latency, max_latency;

        min_latency = self->latency + self->period_time;
        max_latency = min_latency;

        GST_DEBUG_OBJECT(self, "reporting latency of min %" GST_TIME_FORMAT
                         " max %" GST_TIME_FORMAT,
                         GST_TIME_ARGS(min_latency), GST_TIME_ARGS(max_latency));

        gst_query_set_latency(query, TRUE, min_latency, max_latency);
        ret = TRUE;
        break;
    }

    default:
        ret = GST_BASE_SRC_CLASS(parent_class)->query(src, query);
        break;
    }

    return ret;
}

static gboolean
gst_wasapi_src_reinit(GstWasapiSrc* self) {
    if (self->data_pos > 0) {
        self->time_base += gst_util_uint64_scale(self->data_pos, GST_SECOND,
                           self->wfxe.Format.nAvgBytesPerSec);
        self->data_pos = 0;
    }

    self->reinit_time = gst_clock_get_time(self->sys_clock);

    gst_wasapi_src_stop_impl(GST_BASE_SRC(self));

    if (gst_wasapi_src_start_impl(GST_BASE_SRC(self))) {
        self->deviceLost = FALSE;

        if (self->caps) {
            gst_caps_unref(self->caps);
            self->caps = NULL;
        }

        return TRUE;
    } else {
        return FALSE;
    }
}

static GstCaps*
gst_wasapi_src_get_caps(GstWasapiSrc* self) {
    if (!self->caps) {
        self->caps = gst_caps_new_simple("audio/x-raw-float",
                                         "rate", G_TYPE_INT, self->wfxe.Format.nSamplesPerSec,
                                         "channels", G_TYPE_INT, self->wfxe.Format.nChannels,
                                         "endianness", G_TYPE_INT, 1234,
                                         "width", G_TYPE_INT, (self->wfxe.Format.wBitsPerSample + 7) & (~7),
                                         "depth", G_TYPE_INT, self->wfxe.Format.wBitsPerSample,
                                         NULL);
    }

    return self->caps;
}

static GstFlowReturn
gst_wasapi_src_create_empty(GstWasapiSrc* self, GstBuffer** buf) {

    gint32 nsamples;
    GstFlowReturn ret;
    guint64 devpos;
    gint32 buflen;
    GstClockTime timestamp;


    nsamples = self->wfxe.Format.nSamplesPerSec /  100;
    devpos = self->data_pos / self->wfxe.Format.nBlockAlign;

    buflen = nsamples * self->wfxe.Format.nBlockAlign;

    ret = gst_pad_alloc_buffer_and_set_caps(GST_BASE_SRC_PAD(self),
                                            devpos, buflen, gst_wasapi_src_get_caps(self), buf);

    timestamp = self->time_base + gst_util_uint64_scale(self->data_pos, GST_SECOND,
                self->wfxe.Format.nAvgBytesPerSec);

    if (ret == GST_FLOW_OK) {
        memset(GST_BUFFER_DATA(*buf), 0, buflen);
        GST_BUFFER_TIMESTAMP(*buf) = timestamp;
        GST_BUFFER_DURATION(*buf) = GST_SECOND / 100;
        GST_BUFFER_OFFSET_END(*buf) = devpos + nsamples;

        self->next_time = self->start_time + timestamp + GST_BUFFER_DURATION(*buf);

        return ret;

    } else {
        return ret;
    }
}

static GstFlowReturn
gst_wasapi_src_create(GstPushSrc* src, GstBuffer** buf) {
    GstWasapiSrc* self = GST_WASAPI_SRC(src);
    GstBaseSrc* basesrc = GST_BASE_SRC(self);

    GstFlowReturn ret = GST_FLOW_OK;
    GstClock* clock;
    GstClockTime timestamp, duration = self->period_time;
    HRESULT hr;
    gfloat* samples = NULL;
    guint32 nsamples_read = 0, nsamples;
    DWORD flags = 0;
    guint64 devpos;
    guint32 pkt_size = 0;
    gint device_cookie;

    if (self->first) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    }

    device_cookie = self->device_cookie;

    if (self->current_device_cookie != device_cookie) {
        if (!gst_wasapi_util_is_default_device(GST_ELEMENT(self), TRUE, self->device_id)) {
            self->deviceLost = TRUE;
        } else {
            self->current_device_cookie = device_cookie;
        }
    }

    *buf = NULL;
    GST_OBJECT_LOCK(self);
    clock = GST_ELEMENT_CLOCK(self);


    if (clock != NULL) {
        gst_object_ref(clock);
    }

    GST_OBJECT_UNLOCK(self);

    if (self->deviceLost) {
        GstClockTime now = gst_clock_get_time(self->sys_clock);

        if (GST_CLOCK_TIME_IS_VALID(self->next_time)) {
            GstClockID id;

            id = gst_clock_new_single_shot_id(self->sys_clock, self->next_time);
            gst_clock_id_wait(id, NULL);
            gst_clock_id_unref(id);
            self->next_time = GST_CLOCK_TIME_NONE;
        }

        if (now - self->reinit_time > GST_SECOND) {
            if (!gst_wasapi_src_reinit(self)) {
                ret = gst_wasapi_src_create_empty(self, buf);
                goto beach;
            }
        } else {
            ret = gst_wasapi_src_create_empty(self, buf);
            goto beach;
        }
    }

    do {
        pkt_size = 0;
        hr = IAudioCaptureClient_GetNextPacketSize(self->capture_client, &pkt_size);

        if (FAILED(hr)) {
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED ||
                    hr == AUDCLNT_E_SERVICE_NOT_RUNNING) {
                self->deviceLost = TRUE;

                GST_WARNING_OBJECT(self, "Audio device has been lost, attempting to reinitialize");
                self->reinit_time = gst_clock_get_time(self->sys_clock);
                ret = gst_wasapi_src_create_empty(self, buf);
                goto beach;
            } else {
                GST_ELEMENT_ERROR(self, RESOURCE, READ, (NULL),
                                  ("audio device failed"));

                ret = GST_FLOW_ERROR;
                goto beach;
            }
        }

        if (pkt_size <= 0) {
            if (GST_CLOCK_TIME_IS_VALID(self->next_time)) {
                if (gst_clock_get_time(self->sys_clock) > (self->next_time + GST_SECOND / 100)) {

                    ret = gst_wasapi_src_create_empty(self, buf);
                    goto beach;
                }
            }
        }

        if (!basesrc->live_running) {
            ret = GST_FLOW_WRONG_STATE;
            goto beach;
        }

    } while (pkt_size <= 0);

    hr = IAudioCaptureClient_GetBuffer(self->capture_client,
                                       (BYTE**) & samples, &nsamples_read, &flags, &devpos, &timestamp);

    if (hr != S_OK) {

        if (hr == AUDCLNT_E_DEVICE_INVALIDATED ||
                hr == AUDCLNT_E_SERVICE_NOT_RUNNING) {
            self->deviceLost = TRUE;

            GST_WARNING_OBJECT(self, "Audio device has been lost, attempting to reinitialize");
            self->reinit_time = gst_clock_get_time(self->sys_clock);
            ret = gst_wasapi_src_create_empty(self, buf);
            goto beach;
        }

        if (hr == AUDCLNT_S_BUFFER_EMPTY) {
            ret = gst_wasapi_src_create_empty(self, buf);
            goto beach;
        }

        GST_ERROR_OBJECT(self, "IAudioCaptureClient::GetBuffer () failed: %s",
                         gst_wasapi_util_hresult_to_string(hr));
        ret = GST_FLOW_ERROR;
        goto beach;
    }

    if (flags != 0) {
        GST_WARNING_OBJECT(self, "devpos %" G_GUINT64_FORMAT ": flags=0x%08x",
                           devpos, flags);

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
            IAudioCaptureClient_ReleaseBuffer(self->capture_client, nsamples_read);
            ret = gst_wasapi_src_create_empty(self, buf);
            goto beach;
        }
    }

    /* FIXME: Why do we get 1024 sometimes and not a multiple of
     *        samples_per_buffer? Shouldn't WASAPI provide a DISCONT
     *        flag if we read too slow?
     */
    nsamples = nsamples_read;

    if (nsamples > self->samples_per_buffer) {
        GST_WARNING_OBJECT(self,
                           "devpos %" G_GUINT64_FORMAT ": got %d samples, expected %d, clipping!",
                           devpos, nsamples, self->samples_per_buffer);

        nsamples = self->samples_per_buffer;
    }

    pkt_size = nsamples * self->wfxe.Format.nBlockAlign;

    duration = gst_util_uint64_scale(pkt_size, GST_SECOND, self->wfxe.Format.nAvgBytesPerSec);

    devpos = self->data_pos / self->wfxe.Format.nBlockAlign;

    timestamp = self->time_base + gst_util_uint64_scale(self->data_pos, GST_SECOND,
                self->wfxe.Format.nAvgBytesPerSec);

    ret = gst_pad_alloc_buffer_and_set_caps(GST_BASE_SRC_PAD(self),
                                            devpos,
                                            pkt_size, gst_wasapi_src_get_caps(self), buf);

    if (ret == GST_FLOW_OK) {
        gfloat* dst;

        GST_BUFFER_OFFSET_END(*buf) = devpos + nsamples;
        GST_BUFFER_TIMESTAMP(*buf) = timestamp;
        GST_BUFFER_DURATION(*buf) = duration;

        dst = (gfloat*) GST_BUFFER_DATA(*buf);

        memcpy(dst, samples, pkt_size);
        self->next_time = self->start_time + timestamp + duration;
    }

    hr = IAudioCaptureClient_ReleaseBuffer(self->capture_client, nsamples_read);

    if (hr != S_OK) {
        GST_ERROR_OBJECT(self, "IAudioCaptureClient::ReleaseBuffer () failed: %s",
                         gst_wasapi_util_hresult_to_string(hr));
        ret = GST_FLOW_ERROR;
        goto beach;
    }

beach:

    if (ret == GST_FLOW_OK) {
        if (self->first == TRUE) {
            self->start_time = gst_clock_get_time(self->sys_clock);
            self->first = FALSE;
        }

        self->data_pos += GST_BUFFER_SIZE(*buf);
    }

    if (clock != NULL) {
        gst_object_unref(clock);
    }

    return ret;
}

static GstClockTime
gst_wasapi_src_get_time(GstClock* clock, gpointer user_data) {
    GstWasapiSrc* self = GST_WASAPI_SRC(user_data);
    /*   HRESULT hr;
       guint64 devpos;*/
    GstClockTime result;

    /*    if (G_UNLIKELY(self->client_clock == NULL)) {
            return GST_CLOCK_TIME_NONE;
        }

        hr = IAudioClock_GetPosition(self->client_clock, &devpos, NULL);

        if (G_UNLIKELY(hr != S_OK)) {
            return GST_CLOCK_TIME_NONE;
        }

        result = gst_util_uint64_scale_int(devpos, GST_SECOND,
            self->client_clock_freq);  */

    result = self->time_base + gst_util_uint64_scale(self->data_pos, GST_SECOND,
             self->wfxe.Format.nAvgBytesPerSec);

    /*
       GST_DEBUG_OBJECT (self, "devpos = %" G_GUINT64_FORMAT
       " frequency = %" G_GUINT64_FORMAT
       " result = %" G_GUINT64_FORMAT " ms",
       devpos, self->client_clock_freq, GST_TIME_AS_MSECONDS (result));
     */

    return result;
}
