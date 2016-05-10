/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@schleef.org>
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

#include <gst/gst.h>
#include "gstdecklink.h"
//#include "gstdecklinkaudiosink.h"
//#include "gstdecklinkvideosink.h"
#include "gstdecklinkaudiosrc.h"
#include "gstdecklinkvideosrc.h"
#include "gstdecklinksrc.h"

GST_DEBUG_CATEGORY_STATIC(gst_decklink_debug);
#define GST_CAT_DEFAULT gst_decklink_debug

GType
gst_decklink_mode_get_type(void) {
    static gsize id = 0;
    static const GEnumValue modes[] = {
        {GST_DECKLINK_MODE_UNK, "auto", "auto mode"},
        {GST_DECKLINK_MODE_NTSC, "ntsc", "NTSC SD 60i"},
        {GST_DECKLINK_MODE_NTSC2398, "ntsc2398", "NTSC SD 60i (24 fps)"},
        {GST_DECKLINK_MODE_PAL, "pal", "PAL SD 50i"},
        {GST_DECKLINK_MODE_NTSC_P, "ntsc-p", "NTSC SD 60p"},
        {GST_DECKLINK_MODE_PAL_P, "pal-p", "PAL SD 50p"},

        {GST_DECKLINK_MODE_1080p2398, "1080p2398", "HD1080 23.98p"},
        {GST_DECKLINK_MODE_1080p24, "1080p24", "HD1080 24p"},
        {GST_DECKLINK_MODE_1080p25, "1080p25", "HD1080 25p"},
        {GST_DECKLINK_MODE_1080p2997, "1080p2997", "HD1080 29.97p"},
        {GST_DECKLINK_MODE_1080p30, "1080p30", "HD1080 30p"},

        {GST_DECKLINK_MODE_1080i50, "1080i50", "HD1080 50i"},
        {GST_DECKLINK_MODE_1080i5994, "1080i5994", "HD1080 59.94i"},
        {GST_DECKLINK_MODE_1080i60, "1080i60", "HD1080 60i"},

        {GST_DECKLINK_MODE_1080p50, "1080p50", "HD1080 50p"},
        {GST_DECKLINK_MODE_1080p5994, "1080p5994", "HD1080 59.94p"},
        {GST_DECKLINK_MODE_1080p60, "1080p60", "HD1080 60p"},

        {GST_DECKLINK_MODE_720p50, "720p50", "HD720 50p"},
        {GST_DECKLINK_MODE_720p5994, "720p5994", "HD720 59.94p"},
        {GST_DECKLINK_MODE_720p60, "720p60", "HD720 60p"},

        {GST_DECKLINK_MODE_2048p2398, "2048p2398", "2k 23.98p"},
        {GST_DECKLINK_MODE_2048p24, "2048p24", "2k 24p"},
        {GST_DECKLINK_MODE_2048p25, "2048p25", "2k 25p"},

        {GST_DECKLINK_MODE_3184p2398, "3184p2398", "4k 23.98p"},
        {GST_DECKLINK_MODE_3184p24, "3184p24", "4k 24p"},
        {GST_DECKLINK_MODE_3184p25, "3184p25", "4k 25p"},
        {GST_DECKLINK_MODE_3184p2997, "3184p2997", "4k 29.97p"},
        {GST_DECKLINK_MODE_3184p30, "3184p30", "4k 30p"},
        {GST_DECKLINK_MODE_3184p50, "3184p50", "4k 50p"},
        {GST_DECKLINK_MODE_3184p5994, "3184p5994", "4k 59.94p"},
        {GST_DECKLINK_MODE_3184p60, "3184p60", "4k 60p"},

        {0, NULL, NULL}
    };

    if (g_once_init_enter(&id)) {
        GType tmp = g_enum_register_static("GstDecklinkModes", modes);
        g_once_init_leave(&id, tmp);
    }

    return (GType) id;
}

GType
gst_decklink_connection_get_type(void) {
    static gsize id = 0;
    static const GEnumValue connections[] = {
        {GST_DECKLINK_CONNECTION_SDI, "sdi", "SDI"},
        {GST_DECKLINK_CONNECTION_HDMI, "hdmi", "HDMI"},
        {GST_DECKLINK_CONNECTION_OPTICAL_SDI, "optical-sdi", "Optical SDI"},
        {GST_DECKLINK_CONNECTION_COMPONENT, "component", "Component"},
        {GST_DECKLINK_CONNECTION_COMPOSITE, "composite", "Composite"},
        {GST_DECKLINK_CONNECTION_SVIDEO, "svideo", "S-Video"},
        {0, NULL, NULL}
    };

    if (g_once_init_enter(&id)) {
        GType tmp = g_enum_register_static("GstDecklinkConnection", connections);
        g_once_init_leave(&id, tmp);
    }

    return (GType) id;
}

GType
gst_decklink_audio_connection_get_type(void) {
    static gsize id = 0;
    static const GEnumValue connections[] = {
        {GST_DECKLINK_AUDIO_CONNECTION_AUTO, "auto", "Automatic"},
        {
            GST_DECKLINK_AUDIO_CONNECTION_EMBEDDED, "embedded",
            "SDI/HDMI embedded audio"
        },
        {GST_DECKLINK_AUDIO_CONNECTION_AES_EBU, "aes", "AES/EBU input"},
        {GST_DECKLINK_AUDIO_CONNECTION_ANALOG, "analog", "Analog input"},
        {0, NULL, NULL}
    };

    if (g_once_init_enter(&id)) {
        GType tmp =
            g_enum_register_static("GstDecklinkAudioConnection", connections);
        g_once_init_leave(&id, tmp);
    }

    return (GType) id;
}

static void gst_decklink_input_default_VideoInputFrameArrived(GstDecklinkInput* input,
        IDeckLinkVideoInputFrame* video_frame,
        IDeckLinkAudioInputPacket* audio_packet);

static gint gst_decklink_input_start_streams_nolock(GstDecklinkInput* self);
static gint gst_decklink_input_stop_streams_nolock(GstDecklinkInput* self);

#define NTSC 10, 11, false, false
#define PAL 12, 11, true, false
#define HD 1, 1, false, true

static const GstDecklinkMode modes[] = {
    {bmdModeUnknown, 720, 486, 30000, 1001, true, NTSC},
    {bmdModeNTSC, 720, 486, 30000, 1001, true, NTSC},
    {bmdModeNTSC2398, 720, 486, 24000, 1001, true, NTSC},
    {bmdModePAL, 720, 576, 25, 1, true, PAL},
    {bmdModeNTSCp, 720, 486, 30000, 1001, false, NTSC},
    {bmdModePALp, 720, 576, 25, 1, false, PAL},

    {bmdModeHD1080p2398, 1920, 1080, 24000, 1001, false, HD},
    {bmdModeHD1080p24, 1920, 1080, 24, 1, false, HD},
    {bmdModeHD1080p25, 1920, 1080, 25, 1, false, HD},
    {bmdModeHD1080p2997, 1920, 1080, 30000, 1001, false, HD},
    {bmdModeHD1080p30, 1920, 1080, 30, 1, false, HD},

    {bmdModeHD1080i50, 1920, 1080, 50, 1, true, HD},
    {bmdModeHD1080i5994, 1920, 1080, 60000, 1001, true, HD},
    {bmdModeHD1080i6000, 1920, 1080, 60, 1, true, HD},

    {bmdModeHD1080p50, 1920, 1080, 50, 1, false, HD},
    {bmdModeHD1080p5994, 1920, 1080, 60000, 1001, false, HD},
    {bmdModeHD1080p6000, 1920, 1080, 60, 1, false, HD},

    {bmdModeHD720p50, 1280, 720, 50, 1, false, HD},
    {bmdModeHD720p5994, 1280, 720, 60000, 1001, false, HD},
    {bmdModeHD720p60, 1280, 720, 60, 1, false, HD},

    {bmdMode2k2398, 2048, 1556, 24000, 1001, false, HD},
    {bmdMode2k24, 2048, 1556, 24, 1, false, HD},
    {bmdMode2k25, 2048, 1556, 25, 1, false, HD},

    {bmdMode4K2160p2398, 3840, 2160, 24000, 1001, false, HD},
    {bmdMode4K2160p24, 3840, 2160, 24, 1, false, HD},
    {bmdMode4K2160p25, 3840, 2160, 25, 1, false, HD},
    {bmdMode4K2160p2997, 3840, 2160, 30000, 1001, false, HD},
    {bmdMode4K2160p30, 3840, 2160, 30, 1, false, HD},
    {bmdMode4K2160p50, 3840, 2160, 55, 1, false, HD},
    {bmdMode4K2160p5994, 3840, 2160, 60000, 1001, false, HD},
    {bmdMode4K2160p60, 3840, 2160, 60, 1, false, HD}
};

const GstDecklinkMode*
gst_decklink_get_mode(GstDecklinkModeEnum e) {
    return &modes[e];
}

gint
gst_decklink_get_mode_enum(BMDDisplayMode mode) {
    gint i = 0;

    for (i = 0; i < sizeof(modes) / sizeof(GstDecklinkMode); i ++) {
        if (modes[i].mode == mode) {
            return i;
        }
    }

    return -1;
}

BMDVideoConnection
gst_decklink_get_video_connection(GstDecklinkConnectionEnum e) {
    switch (e) {
    case GST_DECKLINK_CONNECTION_HDMI:
        return bmdVideoConnectionHDMI;

    case GST_DECKLINK_CONNECTION_OPTICAL_SDI:
        return bmdVideoConnectionOpticalSDI;

    case GST_DECKLINK_CONNECTION_COMPONENT:
        return bmdVideoConnectionComponent;

    case GST_DECKLINK_CONNECTION_COMPOSITE:
        return bmdVideoConnectionComposite;

    case GST_DECKLINK_CONNECTION_SVIDEO:
        return bmdVideoConnectionSVideo;

    case GST_DECKLINK_CONNECTION_SDI:
    default:
        return bmdVideoConnectionSDI;
    }
}

BMDAudioConnection
gst_decklink_get_audio_connection(GstDecklinkAudioConnectionEnum e) {
    switch (e) {
    case GST_DECKLINK_AUDIO_CONNECTION_EMBEDDED:
        return bmdAudioConnectionEmbedded;

    case GST_DECKLINK_AUDIO_CONNECTION_AES_EBU:
        return bmdAudioConnectionAESEBU;

    case GST_DECKLINK_AUDIO_CONNECTION_ANALOG:
        return bmdAudioConnectionAnalog;

    case GST_DECKLINK_AUDIO_CONNECTION_AUTO:
    default:
        return bmdAudioConnectionEmbedded;
    }
}

static GstStructure*
gst_decklink_mode_get_structure(GstDecklinkModeEnum e) {
    const GstDecklinkMode* mode = &modes[e];

    return gst_structure_new("video/x-raw-yuv",
                             "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'),
                             "width", G_TYPE_INT, mode->width,
                             "height", G_TYPE_INT, mode->height,
                             "framerate", GST_TYPE_FRACTION, mode->fps_n, mode->fps_d,
                             "pixel-aspect-ratio", GST_TYPE_FRACTION, mode->par_n, mode->par_d, 
                             "chroma-site", G_TYPE_STRING, "mpeg2",
                             NULL);
}

GstCaps*
gst_decklink_mode_get_caps(GstDecklinkModeEnum e) {
    GstCaps* caps;

    caps = gst_caps_new_empty();
    gst_caps_append_structure(caps, gst_decklink_mode_get_structure(e));

    return caps;
}

GstCaps*
gst_decklink_mode_get_template_caps(void) {
    int i;
    GstCaps* caps;
    GstStructure* s;

    caps = gst_caps_new_empty();

    for (i = 0; i < (int) G_N_ELEMENTS(modes); i++) {
        s = gst_decklink_mode_get_structure((GstDecklinkModeEnum) i);
        gst_caps_append_structure(caps, s);
    }

    return caps;
}

#define GST_TYPE_DECKLINK_CLOCK \
  (gst_decklink_clock_get_type())
#define GST_DECKLINK_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DECKLINK_CLOCK,GstDecklinkClock))
#define GST_DECKLINK_CLOCK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DECKLINK_CLOCK,GstDecklinkClockClass))
#define GST_IS_Decklink_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DECKLINK_CLOCK))
#define GST_IS_Decklink_CLOCK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DECKLINK_CLOCK))
#define GST_DECKLINK_CLOCK_CAST(obj) \
  ((GstDecklinkClock*)(obj))

typedef struct _GstDecklinkClock GstDecklinkClock;
typedef struct _GstDecklinkClockClass GstDecklinkClockClass;

struct _GstDecklinkClock {
    GstSystemClock clock;

    IDeckLinkInput* input;
    IDeckLinkOutput* output;

    GstClockTime start_time;
    GstClockTime last_time;
    GOnce once;
};
typedef void (*frame_arrived_func)(GstDecklinkInput* input, IDeckLinkVideoInputFrame* frame,
                                   IDeckLinkAudioInputPacket* audio);

struct _GstDecklinkClockClass {
    GstSystemClockClass parent_class;
};

GType gst_decklink_clock_get_type(void);
static GstClock* gst_decklink_clock_new(const gchar* name);

typedef struct _Device Device;
struct _Device {
    GstDecklinkOutput output;
    GstDecklinkInput input;
};

class GStreamerDecklinkInputCallback: public IDeckLinkInputCallback {
private:
    GstDecklinkInput* m_input;
    GMutex m_mutex;
    gint m_refcount;
public:
    GStreamerDecklinkInputCallback(GstDecklinkInput* input)
        : IDeckLinkInputCallback() {
        m_input = input;
        g_mutex_init(&m_mutex);
    }

    virtual ~ GStreamerDecklinkInputCallback() {
        g_mutex_clear(&m_mutex);
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, LPVOID*) {
        return E_NOINTERFACE;
    }

    virtual ULONG STDMETHODCALLTYPE AddRef(void) {
        ULONG ret;

        g_mutex_lock(&m_mutex);
        m_refcount++;
        ret = m_refcount;
        g_mutex_unlock(&m_mutex);

        return ret;
    }

    virtual ULONG STDMETHODCALLTYPE Release(void) {
        ULONG ret;

        g_mutex_lock(&m_mutex);
        m_refcount--;
        ret = m_refcount;
        g_mutex_unlock(&m_mutex);


        if (ret == 0) {
            delete this;
        }

        return ret;
    }

    virtual HRESULT STDMETHODCALLTYPE
    VideoInputFormatChanged(BMDVideoInputFormatChangedEvents event,
                            IDeckLinkDisplayMode* newmode, BMDDetectedVideoInputFormatFlags flag) {

        BMDDisplayMode mode =  newmode->GetDisplayMode();
        input_mode_changed_fun f = m_input->input_mode_changed;

        if (f) {
            f(m_input, mode);
        }

        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE
    VideoInputFrameArrived(IDeckLinkVideoInputFrame* video_frame,
                           IDeckLinkAudioInputPacket* audio_packet) {
        frame_arrived_func f = m_input->frame_arrived;

        if (f) {
            f(m_input, video_frame, audio_packet);
        }

        return S_OK;
    }
};

#ifdef _MSC_VER
/* FIXME: We currently never deinit this */

static GMutex com_init_lock;
static GMutex com_deinit_lock;
static GCond com_init_cond;
static GCond com_deinit_cond;
static GCond com_deinited_cond;
static gboolean com_initialized = FALSE;

/* COM initialization/uninitialization thread */
static gpointer
gst_decklink_com_thread(gpointer data) {
    HRESULT res;

    g_mutex_lock(&com_init_lock);

    /* Initialize COM with a MTA for this process. This thread will
     * be the first one to enter the apartement and the last one to leave
     * it, unitializing COM properly */

    res = CoInitializeEx(0, COINIT_MULTITHREADED);

    if (res == S_FALSE) {
        GST_WARNING("COM has been already initialized in the same process");
    } else if (res == RPC_E_CHANGED_MODE) {
        GST_WARNING("The concurrency model of COM has changed.");
    } else {
        GST_INFO("COM intialized succesfully");
    }

    com_initialized = TRUE;

    /* Signal other threads waiting on this condition that COM was initialized */
    g_cond_signal(&com_init_cond);

    g_mutex_unlock(&com_init_lock);

    /* Wait until the unitialize condition is met to leave the COM apartement */
    g_mutex_lock(&com_deinit_lock);
    g_cond_wait(&com_deinit_cond, &com_deinit_lock);

    CoUninitialize();
    GST_INFO("COM unintialized succesfully");
    com_initialized = FALSE;
    g_cond_signal(&com_deinited_cond);
    g_mutex_unlock(&com_deinit_lock);

    return NULL;
}
#endif /* _MSC_VER */

static GOnce devices_once = G_ONCE_INIT;
static int n_devices;
static Device devices[10];
static void deinit_devices() {

}


struct {
    gboolean detected;
    gboolean started;
    GList* chars;
    BMDDisplayMode detected_mode;
} detect_context = {0};

gpointer change_mode_thrd(gpointer data) {
    GstDecklinkInput* input = reinterpret_cast<GstDecklinkInput*>(data);

    g_mutex_lock(&input->lock);
    g_mutex_lock(&input->mode_list_lock);

    if (input->pending_modes == 0) {
        g_mutex_unlock(&input->mode_list_lock);
        g_mutex_unlock(&input->lock);
        return NULL;
    }

    BMDDisplayMode mode = *((BMDDisplayMode*)(&input->pending_modes->data));
    GList* tmp = input->pending_modes;
    input->pending_modes = tmp->next;
    g_list_free_1(tmp);
    g_mutex_unlock(&input->mode_list_lock);

    if (input->started) {
        input->input->StopStreams();
        input->input->FlushStreams();
    }

    gst_decklink_set_mode(input, mode, input->cfg.pixel_format, input->cfg.flags);

    if (input->started) {
        input->input->StartStreams();
    }

    input->mode = gst_decklink_get_mode((GstDecklinkModeEnum)gst_decklink_get_mode_enum(mode));
    input->mode_changed = TRUE;
    g_mutex_unlock(&input->lock);

    g_thread_exit(0);
    return NULL;
}

void change_mode(GstDecklinkInput* input) {
    GThread* ptrd = g_thread_new("changemode", change_mode_thrd, input);

    if (ptrd) {
        g_thread_unref(ptrd);
    }
}


void gst_decklink_input_mode_changed(GstDecklinkInput* input, BMDDisplayMode mode) {
    detect_context.detected_mode = mode;
    detect_context.detected = TRUE;

    g_mutex_lock(&input->mode_list_lock);
    input->pending_modes = g_list_append(input->pending_modes, (gpointer)mode);
    g_mutex_unlock(&input->mode_list_lock);

    change_mode(input);
}


static gpointer
init_devices(gpointer data) {
    IDeckLinkIterator* iterator;
    IDeckLink* decklink = NULL;
    HRESULT ret;
    int i;

    memset(&devices[0], 0, sizeof(devices));
#ifdef _MSC_VER
    // Start COM thread for Windows

    g_mutex_init(&com_init_lock);
    g_cond_init(&com_init_cond);
    g_mutex_init(&com_deinit_lock);
    g_cond_init(&com_deinit_cond);
    g_cond_init(&com_deinited_cond);

    g_mutex_lock(&com_init_lock);

    /* create the COM initialization thread */
    g_thread_create((GThreadFunc) gst_decklink_com_thread, NULL, FALSE, NULL);

    /* wait until the COM thread signals that COM has been initialized */
    g_cond_wait(&com_init_cond, &com_init_lock);
    g_mutex_unlock(&com_init_lock);

    atexit(deinit_devices);
#endif /* _MSC_VER */

    iterator = CreateDeckLinkIteratorInstance();

    if (iterator == NULL) {
        GST_ERROR("no driver");
        return NULL;
    }

    i = 0;
    ret = iterator->Next(&decklink);

    while (ret == S_OK) {
        ret = decklink->QueryInterface(IID_IDeckLinkInput,
                                       (void**) &devices[i].input.input);

        if (ret != S_OK) {
            GST_WARNING("selected device does not have input interface");
        } else {
            devices[i].input.device = decklink;
            devices[i].input.clock = gst_decklink_clock_new("GstDecklinkInputClock");
            GST_DECKLINK_CLOCK_CAST(devices[i].input.clock)->input =
                devices[i].input.input;
            devices[i].input.
            input->SetCallback(new GStreamerDecklinkInputCallback(&devices[i].input));
            devices[i].input.frame_arrived = gst_decklink_input_default_VideoInputFrameArrived;
            devices[i].input.input_mode_changed = gst_decklink_input_mode_changed;
        }

        ret = decklink->QueryInterface(IID_IDeckLinkOutput,
                                       (void**) &devices[i].output.output);

        if (ret != S_OK) {
            GST_WARNING("selected device does not have output interface");
        } else {
            devices[i].output.device = decklink;
            devices[i].output.clock =
                gst_decklink_clock_new("GstDecklinkOutputClock");
            GST_DECKLINK_CLOCK_CAST(devices[i].output.clock)->output =
                devices[i].output.output;
        }

        ret = decklink->QueryInterface(IID_IDeckLinkConfiguration,
                                       (void**) &devices[i].input.config);

        if (ret != S_OK) {
            GST_WARNING("selected device does not have config interface");
        }

        ret = decklink->QueryInterface(IID_IDeckLinkAttributes, (void**)&devices[i].input.attribute);

        if (ret != S_OK) {
            GST_WARNING("selected device does not have attributes interface");
        } else {
            bool autodetect = FALSE;
            devices[i].input.attribute->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &autodetect);

            if (!autodetect) {
                GST_WARNING("selected device does not support autodetect");
            } else {
                devices[i].input.support_detection = TRUE;
            }
        }

        ret = iterator->Next(&decklink);
        i++;

        if (i == 10) {
            GST_WARNING("this hardware has more then 10 devices");
            break;
        }
    }

    n_devices = i;

    iterator->Release();

    return NULL;
}

GstDecklinkOutput*
gst_decklink_acquire_nth_output(gint n, GstElement* sink, gboolean is_audio) {
    GstDecklinkOutput* output;

    g_once(&devices_once, init_devices, NULL);

    if (n >= n_devices) {
        return NULL;
    }

    output = &devices[n].output;

    if (!output->output) {
        GST_ERROR("Device %d has no output", n);
        return NULL;
    }

    g_mutex_lock(&output->lock);

    if (is_audio && !output->audiosink) {
        output->audiosink = GST_ELEMENT_CAST(gst_object_ref(sink));
        g_mutex_unlock(&output->lock);
        return output;
    } else if (!output->videosink) {
        output->videosink = GST_ELEMENT_CAST(gst_object_ref(sink));
        g_mutex_unlock(&output->lock);
        return output;
    }

    g_mutex_unlock(&output->lock);

    GST_ERROR("Output device %d (audio: %d) in use already", n, is_audio);
    return NULL;
}

void
gst_decklink_release_nth_output(gint n, GstElement* sink, gboolean is_audio) {
    GstDecklinkOutput* output;

    if (n >= n_devices) {
        return;
    }

    output = &devices[n].output;
    g_assert(output->output);

    g_mutex_lock(&output->lock);

    if (is_audio) {
        g_assert(output->audiosink == sink);
        gst_object_unref(sink);
        output->audiosink = NULL;
    } else {
        g_assert(output->videosink == sink);
        gst_object_unref(sink);
        output->videosink = NULL;
    }

    g_mutex_unlock(&output->lock);
}

void
gst_decklink_output_set_audio_clock(GstDecklinkOutput* output,
                                    GstClock* clock) {
    g_mutex_lock(&output->lock);

    if (output->audio_clock) {
        gst_object_unref(output->audio_clock);
    }

    output->audio_clock = clock;

    if (clock) {
        gst_object_ref(clock);
    }

    g_mutex_unlock(&output->lock);
}


GstClock*
gst_decklink_output_get_audio_clock(GstDecklinkOutput* output) {
    GstClock* ret = NULL;

    g_mutex_lock(&output->lock);

    if (output->audio_clock) {
        ret = GST_CLOCK_CAST(gst_object_ref(output->audio_clock));
    }

    g_mutex_unlock(&output->lock);

    return ret;
}


static gint gst_decklink_input_start_streams_nolock(GstDecklinkInput* self) {
    HRESULT hr = S_OK;

    if (!self->started) {
        hr = self->input->StartStreams();

        if (hr == S_OK) {
            self->started = TRUE;
        }
    }

    return hr;
}

gint gst_decklink_input_start_streams(GstDecklinkInput* self) {
    HRESULT hr = S_OK;
    g_mutex_lock(&self->lock);
    hr = gst_decklink_input_start_streams_nolock(self);
    g_mutex_unlock(&self->lock);
    return hr;
}
static gint gst_decklink_input_stop_streams_nolock(GstDecklinkInput* self) {
    HRESULT hr = S_OK;

    if (self->started) {
        hr = self->input->StopStreams();
        self->input->FlushStreams();

        if (hr == S_OK) {
            self->started = FALSE;
        }
    }

    g_mutex_lock(&self->mode_list_lock);
    g_list_free(self->pending_modes);
    self->pending_modes = 0;
    g_mutex_unlock(&self->mode_list_lock);

    return hr;
}
gint gst_decklink_input_stop_streams(GstDecklinkInput* self) {
    HRESULT hr = S_OK;
    g_mutex_lock(&self->lock);
    hr = gst_decklink_input_stop_streams_nolock(self);
    g_mutex_unlock(&self->lock);
    return hr;
}

void gst_decklink_detect_input_reset_context() {
    detect_context.detected = FALSE;
    g_list_free_full(detect_context.chars, g_free);
    detect_context.chars = NULL;
}

void gst_decklink_detect_input_start() {
    detect_context.started = TRUE;
    g_list_free_full(detect_context.chars, g_free);
    detect_context.chars = NULL;
}

gboolean gst_decklink_detect_is_detected() {
    return FALSE;
}

int gst_decklink_detect_input_frame_char(BYTE* buffer, long width, long height, long rowbytes) {
    return 0;
}

void gst_decklink_detect_input_stop() {
    detect_context.started = FALSE;
    detect_context.detected = gst_decklink_detect_is_detected();
    g_list_free_full(detect_context.chars, g_free);
    detect_context.chars = NULL;
}

typedef struct _frame_chars {
    gboolean valid_source;
    BMDDisplayMode mode;
} frame_chars;

void gst_decklink_detect_input_add_frame(BYTE* buffer, long width, long height, long rowbytes,
        BMDDisplayMode mode, BMDPixelFormat fmt, BMDFrameFlags flags) {
    if (detect_context.started) {
        frame_chars* fc = (frame_chars*)g_malloc0(sizeof(frame_chars));
        fc->valid_source = ((flags & bmdFrameHasNoInputSource) == 0) ;
        fc->mode = mode;
        detect_context.chars = g_list_append(detect_context.chars, fc);
        int n = g_list_length(detect_context.chars);

        if (n > 10) {
            gint ok = 0;
            gint no = 0;
            GList* s = detect_context.chars;
            BMDDisplayMode dmode = bmdModeUnknown;

            while (s) {
                fc = (frame_chars*)s->data;
                s = g_list_next(s);

                if (fc->valid_source) {
                    ok ++ ;
                    dmode = fc->mode;
                } else {
                    no ++ ;
                }
            }

            if (ok > no) {
                detect_context.detected_mode = dmode;
                detect_context.detected = TRUE;
            }
        }
    }
}

static void gst_decklink_input_default_VideoInputFrameArrived(GstDecklinkInput* input,
        IDeckLinkVideoInputFrame* video_frame,
        IDeckLinkAudioInputPacket* audio_packet) {
    GstClockTime clock_time = gst_clock_get_time(input->clock);


    for (int i = 0 ; i < 3; i ++) {
        if (g_mutex_trylock(&input->lock)) {
            if (input->got_video_frame && video_frame) {
                GstClockTime capture_time = clock_time -
                                            gst_element_get_base_time(input->videosrc);
                input->got_video_frame(input->videosrc, video_frame, capture_time);
            }

            if (input->got_audio_packet && audio_packet) {
                GstClockTime capture_time = clock_time -
                                            gst_element_get_base_time(input->audiosrc);
                input->got_audio_packet(input->audiosrc, audio_packet, capture_time);
            }

            g_mutex_unlock(&input->lock);
            break;
        }

        Sleep(5);
    }
}

static void gst_decklink_input_detect_signal_frame_arrived(GstDecklinkInput* input,
        IDeckLinkVideoInputFrame* frame, IDeckLinkAudioInputPacket* audio) {
    if (frame) {
        BYTE* buffer;
        frame->GetBytes((void**)(&buffer));
        BMDFrameFlags flags = frame->GetFlags();
        IDeckLinkVideoFrameAncillary* Ianci = NULL;
        frame->GetAncillaryData(&Ianci);
        BMDDisplayMode mode = bmdModeUnknown;
        BMDPixelFormat fmt = frame->GetPixelFormat();

        if (Ianci) {
            mode = Ianci->GetDisplayMode();
            Ianci->Release();
        }

        gst_decklink_detect_input_add_frame(buffer,
                                            frame->GetWidth(), frame->GetHeight(), frame->GetRowBytes(),
                                            mode, fmt, flags);
    }
}

gint gst_decklink_set_connection(GstDecklinkInput* input, BMDVideoConnection conn) {
    gint hr = S_OK;

    if (input->config) {
        hr = input->config->SetInt(bmdDeckLinkConfigVideoInputConnection, conn);

        if (hr == S_OK) {
            input->cfg.current_video_connection = conn;
        }
    }

    return hr;
}

gint gst_decklink_set_mode(GstDecklinkInput* input, BMDDisplayMode mode, BMDPixelFormat fmt,
                           BMDVideoInputFlags flags) {
    gint hr = S_OK;

    if (input->input) {
        hr = input->input->EnableVideoInput(mode, fmt, flags);

        if (hr == S_OK) {
            input->cfg.mode = mode;
            input->cfg.pixel_format = fmt;
            input->cfg.flags = flags;
        }
    }

    g_mutex_lock(&input->mode_list_lock);
    g_list_free(input->pending_modes);
    input->pending_modes = 0;
    g_mutex_unlock(&input->mode_list_lock);

    return hr;
}


static gboolean gst_decklink_try_signal(GstDecklinkInput* input, GstDecklinkModeEnum mode) {
    HRESULT hr = S_OK;

    hr = input->input->EnableVideoInput(gst_decklink_get_mode(mode)->mode, bmdFormat8BitYUV,
                                        bmdVideoInputFlagDefault);

    if (hr != S_OK) {
        return FALSE;
    }

    gst_decklink_detect_input_reset_context();
    gst_decklink_input_start_streams_nolock(input);
    gst_decklink_detect_input_start();
    Sleep(2000);
    gst_decklink_detect_input_stop();
    gst_decklink_input_stop_streams_nolock(input);

    return detect_context.detected;
}

static void gst_decklink_input_config_restore(GstDecklinkInput* input) {
    if (input->backup_cfg.current_video_connection) {
        gst_decklink_set_connection(input, input->backup_cfg.current_video_connection);
    }

    if (input->backup_cfg.mode) {
        gst_decklink_set_mode(input, input->backup_cfg.mode, input->backup_cfg.pixel_format,
                              input->backup_cfg.flags);
    }

    memset(&input->backup_cfg, 0, sizeof(input->backup_cfg));
}

static void gst_decklink_input_config_backup(GstDecklinkInput* input) {
    memcpy(&input->backup_cfg, &input->cfg, sizeof(input->cfg));
}

gint gst_decklink_detect_signal_nth_input(gint n, BMDVideoConnection conn, gboolean* allow) {
    GstDecklinkInput* input;
    gint ret = -1;
    HRESULT hr = S_OK;

    g_once(&devices_once, init_devices, NULL);

    if (n >= n_devices) {
        return ret;
    }

    input = &devices[n].input;

    if (!input->input) {
        GST_ERROR("Device %d has no input", n);
        return ret;
    }

    g_mutex_lock(&input->lock);
    frame_arrived_func backup_func = input->frame_arrived;
    input->frame_arrived = gst_decklink_input_detect_signal_frame_arrived;
    gboolean need_start = FALSE;

    if (input->started) {
        gst_decklink_input_stop_streams_nolock(input);
        need_start = TRUE;
    }

    gst_decklink_input_config_backup(input);

    hr = gst_decklink_set_connection(input, conn);

    if (hr != S_OK) {
        GST_WARNING_OBJECT(input, "Failed to enable video connection");
        input->frame_arrived = backup_func;

        if (need_start) {
            gst_decklink_input_start_streams_nolock(input);
        }

        g_mutex_unlock(&input->lock);
        return ret;
    }

    if (input->support_detection) {
        gst_decklink_detect_input_reset_context();
        gst_decklink_detect_input_start();

        hr = gst_decklink_set_mode(input, bmdModeNTSC, bmdFormat8BitYUV,
                                   bmdVideoInputEnableFormatDetection);
        gst_decklink_input_start_streams_nolock(input);

        for (int i = 0 ; i < 10; i ++) {
            if (allow && !(*allow)) {
                break;
            }

            if (detect_context.detected) {
                ret = gst_decklink_get_mode_enum(detect_context.detected_mode);
                break;
            }

            Sleep(1000);
        }

        gst_decklink_detect_input_stop();
        gst_decklink_input_stop_streams_nolock(input);
    } else {
        GstDecklinkModeEnum mode;

        for (mode = GST_DECKLINK_MODE_NTSC; mode  <= GST_DECKLINK_MODE_720p60; (*((int*)(&mode)))++) {
            if (allow && !(*allow)) {
                break;
            }

            if (gst_decklink_try_signal(input, mode)) {
                ret = mode;
                break;
            }
        }
    }

    gst_decklink_input_config_restore(input);


    input->frame_arrived = backup_func;

    if (need_start) {
        gst_decklink_input_start_streams_nolock(input);
    }

    g_mutex_unlock(&input->lock);
    return ret;
}

GstDecklinkInput*
gst_decklink_acquire_nth_input(gint n, GstElement* src, gboolean is_audio) {
    GstDecklinkInput* input;

    g_once(&devices_once, init_devices, NULL);

    if (n >= n_devices) {
        return NULL;
    }

    input = &devices[n].input;

    if (!input->input) {
        GST_ERROR("Device %d has no input", n);
        return NULL;
    }

    g_mutex_lock(&input->lock);

    if (is_audio && !input->audiosrc) {
        input->audiosrc = GST_ELEMENT_CAST(gst_object_ref(src));
        g_mutex_unlock(&input->lock);
        return input;
    } else if (!input->videosrc) {
        input->videosrc = GST_ELEMENT_CAST(gst_object_ref(src));
        g_mutex_unlock(&input->lock);
        return input;
    }

    g_mutex_unlock(&input->lock);

    GST_ERROR("Input device %d (audio: %d) in use already", n, is_audio);
    return NULL;
}

void
gst_decklink_release_nth_input(gint n, GstElement* src, gboolean is_audio) {
    GstDecklinkInput* input;

    if (n >= n_devices) {
        return;
    }

    input = &devices[n].input;
    g_assert(input->input);

    g_mutex_lock(&input->lock);

    if (is_audio) {
        g_assert(input->audiosrc == src);
        gst_object_unref(src);
        input->audiosrc = NULL;
    } else {
        g_assert(input->videosrc == src);
        gst_object_unref(src);
        input->videosrc = NULL;
    }

    g_mutex_unlock(&input->lock);
}

G_DEFINE_TYPE(GstDecklinkClock, gst_decklink_clock, GST_TYPE_SYSTEM_CLOCK);

static GstClockTime gst_decklink_clock_get_internal_time(GstClock* clock);

static void
gst_decklink_clock_class_init(GstDecklinkClockClass* klass) {
    GstClockClass* clock_class = (GstClockClass*) klass;

    clock_class->get_internal_time = gst_decklink_clock_get_internal_time;
}

static void
gst_decklink_clock_init(GstDecklinkClock* clock) {
    GST_OBJECT_FLAG_SET(clock, GST_CLOCK_FLAG_CAN_SET_MASTER);
    clock->start_time = GST_CLOCK_TIME_NONE;
    clock->last_time = GST_CLOCK_TIME_NONE;
    clock->input = NULL;
    clock->output = NULL;
    clock->once.status = G_ONCE_STATUS_NOTCALLED;
    clock->once.retval = NULL;
}

static GstClock*
gst_decklink_clock_new(const gchar* name) {
    GstDecklinkClock* self =
        GST_DECKLINK_CLOCK(g_object_new(GST_TYPE_DECKLINK_CLOCK, "name", name,
                                        "clock-type", GST_CLOCK_TYPE_MONOTONIC, NULL));

    return GST_CLOCK_CAST(self);
}
static gpointer
set_start_offset(gpointer a) {
    gpointer* aa = reinterpret_cast<void**>(a);
    GstDecklinkClock* self = GST_DECKLINK_CLOCK_CAST(aa[0]);
    GstClockTime* start_time = (GstClockTime*)aa[1];

    self->start_time = *start_time;
    return start_time;
}


static GstClockTime
gst_decklink_clock_get_internal_time(GstClock* clock) {

    GstDecklinkClock* self = GST_DECKLINK_CLOCK(clock);
    GstClockTime result;
    BMDTimeValue time;
    HRESULT ret;

    GST_OBJECT_LOCK(clock);

    if (self->input != NULL) {
        ret =
            self->input->GetHardwareReferenceClock(GST_SECOND, &time, NULL, NULL);

        if (ret == S_OK && time >= 0) {
            result = time;
        } else {
            result = GST_CLOCK_TIME_NONE;
        }
    } else if (self->output != NULL) {
        ret =
            self->output->GetHardwareReferenceClock(GST_SECOND, &time, NULL, NULL);

        if (ret == S_OK && time >= 0) {
            result = time;
        } else {
            result = GST_CLOCK_TIME_NONE;
        }
    } else {
        result = GST_CLOCK_TIME_NONE;
    }

    GST_OBJECT_UNLOCK(clock);
    GST_INFO_OBJECT(clock, "result %" GST_TIME_FORMAT, GST_TIME_ARGS(result));

    if (GST_CLOCK_TIME_IS_VALID(result)) {
        if (self->once.status != G_ONCE_STATUS_READY) {
            void* a[] = {self, &result};
            g_once_impl(&self->once, set_start_offset, a);
        }

        result -= self->start_time;

        if (GST_CLOCK_TIME_IS_VALID(self->last_time)) {
            result = MAX(result, self->last_time);
        }

        self->last_time = result;
    }

    return result;
}

static gboolean
plugin_init(GstPlugin* plugin) {
    GST_DEBUG_CATEGORY_INIT(gst_decklink_debug, "decklink", 0,
                            "debug category for decklink plugin");

    //gst_element_register (plugin, "decklinkaudiosink", GST_RANK_NONE,
    //    GST_TYPE_DECKLINK_AUDIO_SINK);
    //gst_element_register (plugin, "decklinkvideosink", GST_RANK_NONE,
    //    GST_TYPE_DECKLINK_VIDEO_SINK);
    gst_element_register(plugin, "decklinkaudiosrc", GST_RANK_NONE,
                         GST_TYPE_DECKLINK_AUDIO_SRC);
    gst_element_register(plugin, "decklinkvideosrc", GST_RANK_NONE,
                         GST_TYPE_DECKLINK_VIDEO_SRC);
    gst_element_register(plugin, "decklinksrc", GST_RANK_NONE,
                         GST_TYPE_DECKLINK_SRC);
    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  "decklink",
                  "Blackmagic Decklink plugin",
                  plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
