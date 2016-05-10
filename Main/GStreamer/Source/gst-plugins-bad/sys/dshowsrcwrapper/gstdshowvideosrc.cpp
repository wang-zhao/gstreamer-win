/* GStreamer
 * Copyright (C)  2007 Sebastien Moutte <sebastien@moutte.net>
 * Copyright (C)  2009 Julien Isorce <julien.isorce@gmail.com>
 *
 * gstdshowvideosrc.c:
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdshowvideosrc.h"

#include <gst/video/video.h>
#include <dvdmedia.h>
#include <atlcomcli.h>

GST_DEBUG_CATEGORY_STATIC(dshowvideosrc_debug);
#define GST_CAT_DEFAULT dshowvideosrc_debug

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
        GST_PAD_SRC,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS(GST_VIDEO_CAPS_BGR ";"
                        GST_VIDEO_CAPS_YUV("{ I420 }") ";"
                        GST_VIDEO_CAPS_YUV("{ YUY2 }") ";"
                        GST_VIDEO_CAPS_YUV("{ YVYU }") ";"
                        GST_VIDEO_CAPS_YUV("{ UYVY }") ";"
                        GST_VIDEO_CAPS_YUV("{ v210 }") ";"
                        "video/x-dv,"
                        "systemstream = (boolean) FALSE,"
                        "width = (int) [ 1, MAX ],"
                        "height = (int) [ 1, MAX ],"
                        "framerate = (fraction) [ 0, MAX ],"
                        "format = (fourcc) dvsd;" "video/x-dv," "systemstream = (boolean) TRUE")
                                                                  );

static void gst_dshowvideosrc_init_interfaces(GType type);

GST_BOILERPLATE_FULL(GstDshowVideoSrc, gst_dshowvideosrc, GstPushSrc,
                     GST_TYPE_PUSH_SRC, gst_dshowvideosrc_init_interfaces);

enum {
    PROP_0,
    PROP_DEVICE,
    PROP_DEVICE_NAME,
    PROP_SHOW_FILTER_PROPERTY,
    PROP_SHOW_FILTER_PROPERTY_IDS,
    PROP_CAPS,
};

static void gst_dshowvideosrc_probe_interface_init(GstPropertyProbeInterface*
        iface);
static const GList* gst_dshowvideosrc_probe_get_properties(GstPropertyProbe*
        probe);
static GValueArray* gst_dshowvideosrc_probe_get_values(GstPropertyProbe*
        probe, guint prop_id, const GParamSpec* pspec);
static GValueArray* gst_dshowvideosrc_get_device_name_values(GstDshowVideoSrc*
        src);
static gboolean gst_dshowvideosrc_probe_needs_probe(GstPropertyProbe* probe,
        guint prop_id, const GParamSpec* pspec);
static void gst_dshowvideosrc_probe_probe_property(GstPropertyProbe* probe,
        guint prop_id, const GParamSpec* pspec);


static void gst_dshowvideosrc_dispose(GObject* gobject);
static void gst_dshowvideosrc_set_property(GObject* object, guint prop_id,
        const GValue* value, GParamSpec* pspec);
static void gst_dshowvideosrc_get_property(GObject* object, guint prop_id,
        GValue* value, GParamSpec* pspec);
static GstCaps* gst_dshowvideosrc_get_caps(GstBaseSrc* src);
static GstStateChangeReturn gst_dshowvideosrc_change_state(GstElement*
        element, GstStateChange transition);


static gboolean gst_dshowvideosrc_start(GstBaseSrc* bsrc);
static gboolean gst_dshowvideosrc_stop(GstBaseSrc* bsrc);
static gboolean gst_dshowvideosrc_unlock(GstBaseSrc* bsrc);
static gboolean gst_dshowvideosrc_unlock_stop(GstBaseSrc* bsrc);
static gboolean gst_dshowvideosrc_set_caps(GstBaseSrc* bsrc, GstCaps* caps);
static GstCaps* gst_dshowvideosrc_get_caps(GstBaseSrc* bsrc);
static void gst_dshowvideosrc_src_fixate(GstBaseSrc* bsrc, GstCaps* caps);
static GstFlowReturn gst_dshowvideosrc_create(GstPushSrc* psrc,
        GstBuffer** buf);

/*utils*/
static GstCaps* gst_dshowvideosrc_getcaps_from_streamcaps(GstDshowVideoSrc*
        src, IPin* pin);
static GstCaps* gst_dshowvideosrc_getcaps_from_enum_mediatypes(GstDshowVideoSrc*
        src, IPin* pin);
static gboolean gst_dshowvideosrc_push_buffer(guint8* buffer, guint size,
        gpointer src_object, GstClockTime duration);

static void
gst_dshowvideosrc_init_interfaces(GType type) {
    static const GInterfaceInfo dshowvideosrc_info = {
        (GInterfaceInitFunc) gst_dshowvideosrc_probe_interface_init,
        NULL,
        NULL,
    };

    g_type_add_interface_static(type,
                                GST_TYPE_PROPERTY_PROBE, &dshowvideosrc_info);
}

static void
gst_dshowvideosrc_probe_interface_init(GstPropertyProbeInterface* iface) {
    iface->get_properties = gst_dshowvideosrc_probe_get_properties;
    iface->needs_probe = gst_dshowvideosrc_probe_needs_probe;
    iface->probe_property = gst_dshowvideosrc_probe_probe_property;
    iface->get_values = gst_dshowvideosrc_probe_get_values;
}

static void
gst_dshowvideosrc_base_init(gpointer klass) {
    GstElementClass* element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_static_pad_template(element_class, &src_template);

    gst_element_class_set_details_simple(element_class,
                                         "DirectShow video capture source", "Source/Video",
                                         "Receive data from a directshow video capture graph",
                                         "Sebastien Moutte <sebastien@moutte.net>");
}

static void
gst_dshowvideosrc_class_init(GstDshowVideoSrcClass* klass) {
    GObjectClass* gobject_class;
    GstElementClass* gstelement_class;
    GstBaseSrcClass* gstbasesrc_class;
    GstPushSrcClass* gstpushsrc_class;

    gobject_class = (GObjectClass*) klass;
    gstelement_class = (GstElementClass*) klass;
    gstbasesrc_class = (GstBaseSrcClass*) klass;
    gstpushsrc_class = (GstPushSrcClass*) klass;

    gobject_class->dispose = GST_DEBUG_FUNCPTR(gst_dshowvideosrc_dispose);
    gobject_class->set_property =
        GST_DEBUG_FUNCPTR(gst_dshowvideosrc_set_property);
    gobject_class->get_property =
        GST_DEBUG_FUNCPTR(gst_dshowvideosrc_get_property);

    gstelement_class->change_state =
        GST_DEBUG_FUNCPTR(gst_dshowvideosrc_change_state);

    gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR(gst_dshowvideosrc_get_caps);
    gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR(gst_dshowvideosrc_set_caps);
    gstbasesrc_class->fixate = GST_DEBUG_FUNCPTR(gst_dshowvideosrc_src_fixate);
    gstbasesrc_class->start = GST_DEBUG_FUNCPTR(gst_dshowvideosrc_start);
    gstbasesrc_class->stop = GST_DEBUG_FUNCPTR(gst_dshowvideosrc_stop);
    gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR(gst_dshowvideosrc_unlock);
    gstbasesrc_class->unlock_stop =
        GST_DEBUG_FUNCPTR(gst_dshowvideosrc_unlock_stop);

    gstpushsrc_class->create = GST_DEBUG_FUNCPTR(gst_dshowvideosrc_create);

    g_object_class_install_property
    (gobject_class, PROP_DEVICE,
     g_param_spec_string("device", "Device",
                         "Directshow device path (@..classID/name)", NULL,
                         static_cast < GParamFlags >(G_PARAM_READWRITE)));

    g_object_class_install_property(
        gobject_class, PROP_SHOW_FILTER_PROPERTY, g_param_spec_boolean("show-property", "show_property",
                "show dshow filter property page", 0,
                static_cast<GParamFlags>(G_PARAM_READWRITE)));

    g_object_class_install_property(
        gobject_class, PROP_SHOW_FILTER_PROPERTY_IDS,
        g_param_spec_string("property-filter", "property_filter",
                            "filter used to show dshow filter property page", 0, static_cast<GParamFlags>(G_PARAM_READWRITE)));

    g_object_class_install_property
    (gobject_class, PROP_DEVICE_NAME,
     g_param_spec_string("device-name", "Device name",
                         "Human-readable name of the sound device", NULL,
                         static_cast < GParamFlags >(G_PARAM_READWRITE)));

    g_object_class_install_property
    (gobject_class, PROP_CAPS,
     g_param_spec_boxed("caps", "video caps",
                        "caps used to set up dshow stream config", GST_TYPE_CAPS,
                        static_cast < GParamFlags >(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    GST_DEBUG_CATEGORY_INIT(dshowvideosrc_debug, "dshowvideosrc", 0,
                            "Directshow video source");

}

static void
gst_dshowvideosrc_init(GstDshowVideoSrc* src, GstDshowVideoSrcClass* klass) {
    src->device = NULL;
    src->device_name = NULL;
    src->video_cap_filter = NULL;
    src->dshow_fakesink = NULL;
    src->media_filter = NULL;
    src->filter_graph = NULL;
    src->caps = NULL;
    src->pins_mediatypes = NULL;
    src->is_rgb = FALSE;

    src->buffer_cond = g_cond_new();
    src->buffer_mutex = g_mutex_new();
    src->buffer = NULL;
    src->stop_requested = FALSE;
    src->device_name_locale = NULL;
    src->show_filter_property = 0;
    src->show_property_ids = NULL;
    src->filter_caps = NULL;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    gst_base_src_set_format(GST_BASE_SRC(src), GST_FORMAT_TIME);
    gst_base_src_set_live(GST_BASE_SRC(src), TRUE);

}

static void
gst_dshowvideosrc_src_fixate(GstBaseSrc* bsrc, GstCaps* caps) {
    /* If there is no desired video size, set default video size to device preffered video size */

    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(bsrc);
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    guint i = 0;
    gint res = -1;

    for (; i < gst_caps_get_size(src->caps) && res == -1; i++) {
        GstCaps* capstmp = gst_caps_copy_nth(src->caps, i);

        if (gst_caps_is_subset(caps, capstmp)) {
            res = i;
        }

        gst_caps_unref(capstmp);
    }

    if (res != -1) {
        GList* type_pin_mediatype = g_list_nth(src->pins_mediatypes, res);

        if (type_pin_mediatype) {
            GstCapturePinMediaType* pin_mediatype =
                (GstCapturePinMediaType*) type_pin_mediatype->data;
            gst_structure_fixate_field_nearest_int(structure, "width",
                                                   pin_mediatype->defaultWidth);
            gst_structure_fixate_field_nearest_int(structure, "height",
                                                   pin_mediatype->defaultHeight);
            gst_structure_fixate_field_nearest_fraction(structure, "framerate",
                    pin_mediatype->defaultFPS, 1000);
        }
    }
}

static void
gst_dshowvideosrc_dispose(GObject* gobject) {
    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(gobject);

    if (src->filter_caps) {
        gst_caps_unref(src->filter_caps);
        src->filter_caps = NULL;
    }

    if (src->device) {
        g_free(src->device);
        src->device = NULL;
    }

    if (src->device_name) {
        g_free(src->device_name);
        src->device_name = NULL;
    }

    if (src->device_name_locale) {
        g_free(src->device_name_locale);
        src->device_name_locale = NULL;
    }

    if (src->caps) {
        gst_caps_unref(src->caps);
        src->caps = NULL;
    }

    if (src->pins_mediatypes) {
        gst_dshow_free_pins_mediatypes(src->pins_mediatypes);
        src->pins_mediatypes = NULL;
    }

    if (src->show_property_ids) {
        g_free(src->show_property_ids);
        src->show_property_ids = NULL;
    }

    /* clean dshow */
    if (src->video_cap_filter) {
        src->video_cap_filter->Release();
        src->video_cap_filter = NULL;
    }

    if (src->buffer_mutex) {
        g_mutex_free(src->buffer_mutex);
        src->buffer_mutex = NULL;
    }

    if (src->buffer_cond) {
        g_cond_free(src->buffer_cond);
        src->buffer_cond = NULL;
    }

    if (src->buffer) {
        gst_buffer_unref(src->buffer);
        src->buffer = NULL;
    }

    CoUninitialize();

    G_OBJECT_CLASS(parent_class)->dispose(gobject);
}

static gboolean
gst_dshowvideosrc_probe_needs_probe(GstPropertyProbe* probe,
                                    guint prop_id, const GParamSpec* pspec) {
    static gboolean init = FALSE;
    gboolean ret = FALSE;

    if (!init) {
        ret = TRUE;
        init = TRUE;
    }

    return ret;
}

static void
gst_dshowvideosrc_probe_probe_property(GstPropertyProbe* probe,
                                       guint prop_id, const GParamSpec* pspec) {
    GObjectClass* klass = G_OBJECT_GET_CLASS(probe);

    switch (prop_id) {
    case PROP_DEVICE_NAME:
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(probe, prop_id, pspec);
        break;
    }
}

static const GList*
gst_dshowvideosrc_probe_get_properties(GstPropertyProbe* probe) {
    GObjectClass* klass = G_OBJECT_GET_CLASS(probe);
    static GList* props = NULL;

    if (!props) {
        GParamSpec* pspec;

        pspec = g_object_class_find_property(klass, "device-name");
        props = g_list_append(props, pspec);
    }

    return props;
}

static GValueArray*
gst_dshowvideosrc_get_device_name_values(GstDshowVideoSrc* src) {
    GValueArray* array = g_value_array_new(0);
    ICreateDevEnum* devices_enum = NULL;
    IEnumMoniker* moniker_enum = NULL;
    IMoniker* moniker = NULL;
    HRESULT hres = S_FALSE;
    ULONG fetched;

    hres = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                            IID_ICreateDevEnum, (LPVOID*) & devices_enum);

    if (hres != S_OK) {
        GST_ERROR("Can't create system device enumerator (error=0x%x)", hres);
        array = NULL;
        goto clean;
    }

    hres = devices_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
            &moniker_enum, 0);

    if (hres != S_OK || !moniker_enum) {
        GST_ERROR("Can't get enumeration of video devices (error=0x%x)", hres);
        array = NULL;
        goto clean;
    }

    moniker_enum->Reset();

    while (hres = moniker_enum->Next(1, &moniker, &fetched), hres == S_OK) {
        IPropertyBag* property_bag = NULL;

        hres =
            moniker->BindToStorage(NULL, NULL, IID_IPropertyBag,
                                   (LPVOID*) & property_bag);

        if (SUCCEEDED(hres) && property_bag) {
            VARIANT varFriendlyName;

            VariantInit(&varFriendlyName);
            hres = property_bag->Read(L"FriendlyName", &varFriendlyName, NULL);

            if (hres == S_OK && varFriendlyName.bstrVal) {
                gchar* friendly_name =
                    g_utf16_to_utf8((const gunichar2*) varFriendlyName.bstrVal,
                                    wcslen(varFriendlyName.bstrVal), NULL, NULL, NULL);

                GValue value = { 0 };
                g_value_init(&value, G_TYPE_STRING);
                g_value_set_string(&value, friendly_name);
                g_value_array_append(array, &value);
                g_value_unset(&value);
                g_free(friendly_name);
                SysFreeString(varFriendlyName.bstrVal);
            }

            property_bag->Release();
        }

        moniker->Release();
    }

clean:

    if (moniker_enum) {
        moniker_enum->Release();
    }

    if (devices_enum) {
        devices_enum->Release();
    }

    return array;
}

static GValueArray*
gst_dshowvideosrc_probe_get_values(GstPropertyProbe* probe,
                                   guint prop_id, const GParamSpec* pspec) {
    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(probe);
    GValueArray* array = NULL;

    switch (prop_id) {
    case PROP_DEVICE_NAME:
        array = gst_dshowvideosrc_get_device_name_values(src);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(probe, prop_id, pspec);
        break;
    }

    return array;
}

static void
gst_dshowvideosrc_set_property(GObject* object, guint prop_id,
                               const GValue* value, GParamSpec* pspec) {
    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(object);

    switch (prop_id) {
    case PROP_DEVICE: {
        if (src->device) {
            g_free(src->device);
            src->device = NULL;
        }

        if (g_value_get_string(value)) {
            src->device = g_strdup(g_value_get_string(value));
        }

        break;
    }

    case PROP_DEVICE_NAME: {
        if (src->device_name) {
            g_free(src->device_name);
            src->device_name = NULL;
        }

        if (src->device_name_locale) {
            g_free(src->device_name_locale);
            src->device_name_locale = NULL;
        }

        if (g_value_get_string(value)) {
            src->device_name = g_strdup(g_value_get_string(value));
            src->device_name_locale = g_locale_from_utf8(src->device_name, -1, NULL, NULL, NULL);
        }

        break;
    }

    case PROP_SHOW_FILTER_PROPERTY: {
        src->show_filter_property = g_value_get_boolean(value);
    }
    break;

    case PROP_SHOW_FILTER_PROPERTY_IDS: {
        GST_OBJECT_LOCK(src);

        gchar* ids = src->show_property_ids;
        src->show_property_ids = NULL;

        if (ids) {
            g_free(ids);
        }

        src->show_property_ids = g_strdup(g_value_get_string(value));

        GST_OBJECT_UNLOCK(src);
    }
    break;

    case PROP_CAPS: {
        GST_OBJECT_LOCK(src);

        if (src->filter_caps) {
            gst_caps_unref(src->filter_caps);
            src->filter_caps = NULL;
        }

        const GstCaps* caps = gst_value_get_caps(value);

        if (caps) {
            src->filter_caps = gst_caps_ref((GstCaps*)caps);
        }

        GST_OBJECT_UNLOCK(src);
    }
    break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gst_dshowvideosrc_get_property(GObject* object, guint prop_id,
                               GValue* value, GParamSpec* pspec) {

    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(object);

    switch (prop_id) {
    case PROP_DEVICE: {
        g_value_set_string(value, src->device);
    }
    break;

    case PROP_DEVICE_NAME: {
        g_value_set_string(value, src->device_name);
    }
    break;

    case PROP_SHOW_FILTER_PROPERTY: {
        g_value_set_boolean(value, src->show_filter_property);
    }
    break;

    case PROP_SHOW_FILTER_PROPERTY_IDS:
        g_value_set_string(value, src->show_property_ids);
        break;

    case PROP_CAPS:
        GST_OBJECT_LOCK(src);
        gst_value_set_caps(value, src->filter_caps);
        GST_OBJECT_UNLOCK(src);
        break;

    default:
        break;
    }

}

static gint gst_dshowvideosrc_get_caps_int(const GstStructure* st, const gchar* name) {
    if (!gst_structure_has_field(st, name)) {
        return 0;
    }

    const GValue* value = NULL;

    value = gst_structure_get_value(st, name);

    if (G_VALUE_TYPE(value) == G_TYPE_INT) {
        return g_value_get_int(value);
    } else if (G_VALUE_TYPE(value) == GST_TYPE_INT_RANGE) {
        return gst_value_get_int_range_max(value);
    } else if (G_VALUE_TYPE(value) == GST_TYPE_LIST) {
        int n = gst_value_list_get_size(value);
        int best = 0;

        for (int i = 0; i < n; i++) {
            const GValue* list_value = gst_value_list_get_value(value, i);

            if (G_VALUE_TYPE(list_value) == G_TYPE_INT) {
                int x = g_value_get_int(list_value);

                if (x > best) {
                    best = x;
                }
            }
        }

        return best;
    } else if (G_VALUE_TYPE(value) == GST_TYPE_ARRAY) {
        int n = gst_value_array_get_size(value);
        int best = 0;

        for (int i = 0; i < n; i++) {
            const GValue* list_value = gst_value_array_get_value(value, i);

            if (G_VALUE_TYPE(list_value) == G_TYPE_INT) {
                int x = g_value_get_int(list_value);

                if (x > best) {
                    best = x;
                }
            }
        }

        return best;
    }

    return 0;

}

static void
gst_dshowvideosrc_get_caps_frac(const GstStructure* st, const gchar* name, gint* num, gint* den) {
    *num = 0;
    *den = 1;

    if (!gst_structure_has_field(st, name)) {
        return ;
    }

    const GValue* value = NULL;

    value = gst_structure_get_value(st, name);

    if (G_VALUE_TYPE(value) == GST_TYPE_FRACTION) {
        *num = gst_value_get_fraction_numerator(value);
        *den = gst_value_get_fraction_denominator(value);
    } else if (G_VALUE_TYPE(value) == GST_TYPE_FRACTION_RANGE) {
        const GValue* max_value =  gst_value_get_fraction_range_max(value);
        *num = gst_value_get_fraction_numerator(max_value);
        *den = gst_value_get_fraction_denominator(max_value);
    } else if (G_VALUE_TYPE(value) == GST_TYPE_LIST) {
        int n = gst_value_list_get_size(value);
        int best = 0;

        for (int i = 0; i < n; i++) {
            const GValue* list_value = gst_value_list_get_value(value, i);

            if (G_VALUE_TYPE(list_value) == GST_TYPE_FRACTION) {
                gint list_num = gst_value_get_fraction_numerator(list_value);
                gint list_den = gst_value_get_fraction_denominator(list_value);

                if (list_num * *den > *num * list_den) {
                    *num = list_num;
                    *den = list_den;
                }
            }
        }
    } else if (G_VALUE_TYPE(value) == GST_TYPE_ARRAY) {
        int n = gst_value_array_get_size(value);
        int best = 0;

        for (int i = 0; i < n; i++) {
            const GValue* list_value = gst_value_array_get_value(value, i);

            if (G_VALUE_TYPE(list_value) == GST_TYPE_FRACTION) {
                gint list_num = gst_value_get_fraction_numerator(list_value);
                gint list_den = gst_value_get_fraction_denominator(list_value);

                if (list_num * *den > *num * list_den) {
                    *num = list_num;
                    *den = list_den;
                }
            }
        }
    }
}

static gint
gst_dshowvideosrc_cmp_caps(gconstpointer  a, gconstpointer  b) {
    const GstStructure* capsa = (const GstStructure*)(a);
    const GstStructure* capsb = (const GstStructure*)(b);

    gint widtha = gst_dshowvideosrc_get_caps_int(capsa, "width");
    gint heighta = gst_dshowvideosrc_get_caps_int(capsa, "height");
    gint widthb = gst_dshowvideosrc_get_caps_int(capsb, "width");
    gint heightb = gst_dshowvideosrc_get_caps_int(capsb, "height");

    gint numa = 0;
    gint dena = 0;
    gint numb = 0;
    gint denb = 0;
    gst_dshowvideosrc_get_caps_frac(capsa, "framerate", &numa, &dena);
    gst_dshowvideosrc_get_caps_frac(capsb, "framerate", &numb, &denb);

    if (widtha > widthb) {
        return 1;
    } else if (widtha < widthb) {
        return -1;
    } else {
        if (heighta > heightb) {
            return 1;
        } else if (heighta < heightb) {
            return -1;
        } else {
            if (numa * denb > numb * dena) {
                return 1;
            } else if (numa * denb < numb * dena) {
                return -1;
            } else {
                return 0;
            }
        }
    }
}

static GstCaps* gst_dshowvideosrc_sorted_caps(GstCaps* caps) {
    if (! caps) {
        return NULL;
    }

    if (gst_caps_is_empty(caps) || GST_CAPS_IS_SIMPLE(caps)) {
        return gst_caps_ref(caps);
    }

    GstCaps* copy_caps = gst_caps_copy(caps);

    GList* list = NULL;

    while (gst_caps_get_size(copy_caps) > 0) {
        GstStructure* st = gst_caps_steal_structure(copy_caps, 0);
        list = g_list_insert_sorted(list, st, gst_dshowvideosrc_cmp_caps);
    }

    int size = g_list_length(list);

    for (int j = size - 1; j >= 0; j --) {
        gst_caps_append_structure(copy_caps, GST_STRUCTURE(g_list_nth_data(list, j)));
    }

    g_list_free(list);

    return copy_caps;
}

static void ShowFilterProperty(IBaseFilter* filter) {

    ISpecifyPropertyPages* pSpecifyPropertyPages = NULL;
    HRESULT   status = filter->QueryInterface(IID_ISpecifyPropertyPages,
                       (void**)&pSpecifyPropertyPages);

    if (FAILED(status)) {
        return;
    }

    CAUUID cauuid = { 0, NULL};
    status = pSpecifyPropertyPages->GetPages(&cauuid);

    if (FAILED(status) || cauuid.cElems <= 0) {

        pSpecifyPropertyPages->Release();
        pSpecifyPropertyPages = NULL;
        return;
    }

    pSpecifyPropertyPages->Release();

    __try {
        status = OleCreatePropertyFrame(NULL,
                                        0,
                                        0,
                                        OLESTR("²É¼¯ÉèÖÃ"),
                                        1,
                                        (IUnknown**)&filter,
                                        cauuid.cElems,
                                        (GUID*)cauuid.pElems,
                                        0,
                                        0,
                                        NULL);

    } __except (EXCEPTION_EXECUTE_HANDLER) {

    }

    CoTaskMemFree(cauuid.pElems);
    cauuid.pElems = NULL;
    cauuid.cElems = 0;

}

static gboolean
gst_dshowvideosrc_can_show_property_page(GstDshowVideoSrc* src, CLSID* id) {
    if (src->show_filter_property) {
        if (src->show_property_ids == NULL || *src->show_property_ids == 0) {
            return TRUE;
        } else {

            gchar** tokens = g_strsplit(src->show_property_ids, ";", 0);

            gchar** cur = tokens;

            while (*cur &&** cur) {
                if (g_strrstr(src->device, *cur)) {
                    g_strfreev(tokens);
                    return TRUE;
                }

                cur ++;
            }

            g_strfreev(tokens);
        }
    }

    return FALSE;
}

typedef
struct _tag_VideoFilterInfo {

    int width;
    int height;
    int avg_time_per_frame;
    int bitcount;
    guint32 format;
    gboolean interlaced;
} VideoFilterInfo;


static
gboolean gst_dshowvideosrc_parse_filter_caps(const GstCaps* caps, VideoFilterInfo* vfi) {
    if (gst_caps_is_empty(caps)) {
        return FALSE;
    }

    if (!GST_CAPS_IS_SIMPLE(caps)) {
        return FALSE;
    }

    GstStructure* stru = gst_caps_get_structure(caps, 0);

    if (!stru) {
        return FALSE;
    }

    VideoFilterInfo local_vfi = {0};
    int count = 0;

    if (gst_structure_get_int(stru, "width", &local_vfi.width)) {
        count++;
    }

    if (gst_structure_get_int(stru, "height", &local_vfi.height)) {
        count++;
    }

    if (gst_structure_get_fourcc(stru, "format", &local_vfi.format)) {
        count++;
    }

    int fnum = 0;
    int fden = 0;

    if (gst_structure_get_fraction(stru, "framerate", &fnum, &fden)) {
        local_vfi.avg_time_per_frame = (int)(fden * 10000000.0 / fnum + 0.5f);
        count ++;
    }

    if (gst_structure_get_int(stru, "depth", &local_vfi.bitcount)) {
        count ++;
    }

    if (gst_structure_get_boolean(stru, "interlaced", &local_vfi.interlaced)) {
        count++;
    }

    if (count) {
        *vfi = local_vfi;
    }

    return !!count;
}

static gboolean match_media_type(VideoFilterInfo* vfi, AM_MEDIA_TYPE* pmt) {
    if (!pmt || pmt->majortype != MEDIATYPE_Video || !vfi) {
        return FALSE;
    }

    if (pmt->formattype == FORMAT_VideoInfo ||
            pmt->formattype == FORMAT_MPEGVideo) {
        VIDEOINFOHEADER* vif = (VIDEOINFOHEADER*)(pmt->pbFormat);

        return (vif->AvgTimePerFrame == vfi->avg_time_per_frame &&
                vif->bmiHeader.biWidth == vfi->width &&
                vif->bmiHeader.biHeight == vfi->height &&
                vif->bmiHeader.biCompression == vfi->format &&
                vif->bmiHeader.biBitCount == vfi->bitcount &&
                !vfi->interlaced);

    } else if (pmt->formattype == FORMAT_VideoInfo2 ||
               pmt->formattype == FORMAT_MPEG2Video) {
        VIDEOINFOHEADER2* vif2 = (VIDEOINFOHEADER2*)(pmt->pbFormat);

        return (vif2->AvgTimePerFrame == vfi->avg_time_per_frame &&
                vif2->bmiHeader.biWidth == vfi->width &&
                vif2->bmiHeader.biHeight == vfi->height &&
                vif2->bmiHeader.biCompression == vfi->format &&
                vif2->bmiHeader.biBitCount == vfi->bitcount &&
                (vif2->dwInterlaceFlags & AMINTERLACE_IsInterlaced) == vfi->interlaced);
    } else {
        return FALSE;
    }
}

static
gboolean gst_dshowvideosrc_filter_caps_disabled(GstDshowVideoSrc* src,
        GstCapturePinMediaType* type) {
    if (!type || ! src) {
        return FALSE;
    }

    VideoFilterInfo vfi = {0};

    if (!gst_dshowvideosrc_parse_filter_caps(src->filter_caps, &vfi)) {
        return FALSE;
    }

    if (!match_media_type(&vfi, type->mediatype)) {
        return TRUE;
    }

    return FALSE;
}

static
void gst_dshowvideosrc_setup_filter_caps(GstDshowVideoSrc* src) {
    if (src->video_cap_filter && src->filter_caps) {

        VideoFilterInfo vfi = {0};

        if (!gst_dshowvideosrc_parse_filter_caps(src->filter_caps, &vfi)) {
            return;
        }

        CComPtr<IEnumPins> enumPins;
        src->video_cap_filter->EnumPins(&enumPins);

        if (!enumPins) {
            return;
        }

        CComPtr<IPin> pin;

        while (enumPins->Next(1, &pin, NULL) == S_OK && !!pin) {
            CComQIPtr<IAMStreamConfig> ptrConfig((IPin*)pin);

            if (ptrConfig) {
                int count = 0;
                int size = 0;
                VIDEO_STREAM_CONFIG_CAPS vscc = {0};

                ptrConfig->GetNumberOfCapabilities(&count, &size);

                if (size != sizeof(vscc)) {
                    continue;
                }

                AM_MEDIA_TYPE* pmt = NULL;
                HRESULT hr = S_OK;

                for (int index = 0; index < count; index ++) {
                    hr = ptrConfig->GetStreamCaps(index, &pmt, (BYTE*)&vscc);

                    if (FAILED(hr) || !pmt) {
                        continue;
                    }

                    if (match_media_type(&vfi, pmt)) {
                        ptrConfig->SetFormat(pmt);
                        gst_dshow_free_mediatype(pmt);
                        break;
                    }

                    gst_dshow_free_mediatype(pmt);
                }
            }

            pin.Release();
        }
    }
}

static GstCaps*
gst_dshowvideosrc_get_caps(GstBaseSrc* basesrc) {
    HRESULT hres = S_OK;
    IBindCtx* lpbc = NULL;
    IMoniker* videom;
    DWORD dwEaten;
    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(basesrc);
    gunichar2* unidevice = NULL;

    if (src->caps) {
        return gst_dshowvideosrc_sorted_caps(src->caps);
    }

    if (!src->device) {
        src->device =
            gst_dshow_getdevice_from_devicename(&CLSID_VideoInputDeviceCategory,
                                                &src->device_name);

        if (!src->device) {
            GST_ERROR("No video device found.");
            return NULL;
        }
    }

    unidevice =
        g_utf8_to_utf16(src->device, strlen(src->device), NULL, NULL, NULL);

    if (!src->video_cap_filter) {
        hres = CreateBindCtx(0, &lpbc);

        if (SUCCEEDED(hres)) {
            hres =
                MkParseDisplayName(lpbc, (LPCOLESTR) unidevice, &dwEaten, &videom);

            if (SUCCEEDED(hres)) {
                hres = videom->BindToObject(lpbc, NULL, IID_IBaseFilter,
                                            (LPVOID*) & src->video_cap_filter);
                videom->Release();

                if (FAILED(hres) || !src->video_cap_filter) {
                    GST_ERROR("BindToObject failed, device is %s, error code=0x%X", src->device, hres);
                }
            } else {
                GST_ERROR("MkParseDisplayName failed, device is %s,error code=0x%X", src->device, hres);
            }

            lpbc->Release();
        } else {
            GST_ERROR("Create Bind Context failed,error code=0x%X", hres);
        }

        if (src->video_cap_filter) {

            if (src->filter_caps) {
                gst_dshowvideosrc_setup_filter_caps(src);
            }

            CLSID fid = GUID_NULL;
            src->video_cap_filter->GetClassID(&fid);

            if (gst_dshowvideosrc_can_show_property_page(src, &fid)) {
                ShowFilterProperty(src->video_cap_filter);
            }
        }
    }

    if (!src->caps) {
        src->caps = gst_caps_new_empty();
    }

    if (src->video_cap_filter && gst_caps_is_empty(src->caps)) {
        /* get the capture pins supported types */
        IPin* capture_pin = NULL;
        IEnumPins* enumpins = NULL;
        HRESULT hres;

        hres = src->video_cap_filter->EnumPins(&enumpins);

        if (SUCCEEDED(hres)) {
            while (enumpins->Next(1, &capture_pin, NULL) == S_OK) {
                IKsPropertySet* pKs = NULL;
                hres =
                    capture_pin->QueryInterface(IID_IKsPropertySet, (LPVOID*) & pKs);

                if (SUCCEEDED(hres) && pKs) {
                    DWORD cbReturned;
                    GUID pin_category;
                    RPC_STATUS rpcstatus;

                    hres =
                        pKs->Get(AMPROPSETID_Pin,
                                 AMPROPERTY_PIN_CATEGORY, NULL, 0, &pin_category, sizeof(GUID),
                                 &cbReturned);

                    /* we only want capture pins */
                    if (UuidCompare(&pin_category, (UUID*) & PIN_CATEGORY_CAPTURE,
                                    &rpcstatus) == 0) {
                        {
                            GstCaps* caps = NULL;
                            caps = gst_dshowvideosrc_getcaps_from_enum_mediatypes(src, capture_pin);

                            if (caps) {
                                gst_caps_append(src->caps, caps);
                            } else {
                                caps = gst_dshowvideosrc_getcaps_from_streamcaps(src, capture_pin);

                                if (caps) {
                                    gst_caps_append(src->caps, caps);
                                }

                            }
                        }

                        pKs->Release();
                    }
                }

                capture_pin->Release();
            }

            enumpins->Release();
        }
    }

    if (unidevice) {
        g_free(unidevice);
    }

    if (src->caps) {
        return gst_dshowvideosrc_sorted_caps(src->caps);
    }

    return NULL;
}

static GstStateChangeReturn
gst_dshowvideosrc_change_state(GstElement* element, GstStateChange transition) {
    HRESULT hres = S_FALSE;
    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(element);

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
        break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        if (src->media_filter) {
            hres = src->media_filter->Run(0);
        }

        if (hres != S_OK) {
            GST_ERROR("Can't RUN the directshow capture graph (error=0x%x)", hres);
            return GST_STATE_CHANGE_FAILURE;
        }

        break;

    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        if (src->media_filter) {
            hres = src->media_filter->Stop();
        }

        if (hres != S_OK) {
            GST_ERROR("Can't STOP the directshow capture graph (error=%d)", hres);
            return GST_STATE_CHANGE_FAILURE;
        }

        break;

    case GST_STATE_CHANGE_PAUSED_TO_READY:
        break;

    case GST_STATE_CHANGE_READY_TO_NULL:
        break;
    }

    return GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
}

static gboolean
gst_dshowvideosrc_start(GstBaseSrc* bsrc) {
    HRESULT hres = S_FALSE;
    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(bsrc);

    hres = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC,
                            IID_IFilterGraph, (LPVOID*) & src->filter_graph);

    if (hres != S_OK || !src->filter_graph) {
        GST_ERROR
        ("Can't create an instance of the dshow graph manager (error=0x%x)",
         hres);
        goto error;
    }

    hres = src->filter_graph->QueryInterface(IID_IMediaFilter,
            (LPVOID*) & src->media_filter);

    if (hres != S_OK || !src->media_filter) {
        GST_ERROR
        ("Can't get IMediacontrol interface from the graph manager (error=0x%x)",
         hres);
        goto error;
    }

    src->dshow_fakesink = new CDshowFakeSink;
    src->dshow_fakesink->AddRef();

    hres = src->filter_graph->AddFilter(src->video_cap_filter, L"capture");

    if (hres != S_OK) {
        GST_ERROR("Can't add video capture filter to the graph (error=0x%x)",
                  hres);
        goto error;
    }

    hres = src->filter_graph->AddFilter(src->dshow_fakesink, L"sink");

    if (hres != S_OK) {
        GST_ERROR("Can't add our fakesink filter to the graph (error=0x%x)", hres);
        goto error;
    }

    return TRUE;

error:

    if (src->dshow_fakesink) {
        src->dshow_fakesink->Release();
        src->dshow_fakesink = NULL;
    }

    if (src->media_filter) {
        src->media_filter->Release();
        src->media_filter = NULL;
    }

    if (src->filter_graph) {
        src->filter_graph->Release();
        src->filter_graph = NULL;
    }

    return FALSE;
}

static gboolean
gst_dshowvideosrc_set_caps(GstBaseSrc* bsrc, GstCaps* caps) {
    HRESULT hres;
    IPin* input_pin = NULL;
    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(bsrc);
    GstStructure* s = gst_caps_get_structure(caps, 0);

    /* search the negociated caps in our caps list to get its index and the corresponding mediatype */
    if (gst_caps_is_subset(caps, src->caps)) {
        guint i = 0;
        gint res = -1;

        for (; i < gst_caps_get_size(src->caps) && res == -1; i++) {
            GstCaps* capstmp = gst_caps_copy_nth(src->caps, i);

            if (gst_caps_is_subset(caps, capstmp)) {
                res = i;
            }

            gst_caps_unref(capstmp);
        }

        if (res != -1 && src->pins_mediatypes) {
            /* get the corresponding media type and build the dshow graph */
            GList* type_pin_mediatype = g_list_nth(src->pins_mediatypes, res);

            if (type_pin_mediatype) {
                GstCapturePinMediaType* pin_mediatype =
                    (GstCapturePinMediaType*) type_pin_mediatype->data;
                gchar* caps_string = NULL;
                gchar* src_caps_string = NULL;

                /* retrieve the desired video size */
                VIDEOINFOHEADER* video_info = NULL;
                VIDEOINFOHEADER2* video_info2 = NULL;
                gint width = 0;
                gint height = 0;
                gint numerator = 0;
                gint denominator = 0;
                gst_structure_get_int(s, "width", &width);
                gst_structure_get_int(s, "height", &height);
                gst_structure_get_fraction(s, "framerate", &numerator, &denominator);

                /* check if the desired video size is valid about granularity  */
                /* This check will be removed when GST_TYPE_INT_RANGE_STEP exits */
                /* See remarks in gst_dshow_new_video_caps function */
                if (pin_mediatype->granularityWidth != 0
                        && width % pin_mediatype->granularityWidth != 0)
                    g_warning("your desired video size is not valid : %d mod %d !=0\n",
                              width, pin_mediatype->granularityWidth);

                if (pin_mediatype->granularityHeight != 0
                        && height % pin_mediatype->granularityHeight != 0)
                    g_warning("your desired video size is not valid : %d mod %d !=0\n",
                              height, pin_mediatype->granularityHeight);

                /* update mediatype */
                if (pin_mediatype->mediatype->formattype == FORMAT_MPEGVideo ||
                        pin_mediatype->mediatype->formattype == FORMAT_VideoInfo) {

                    video_info = (VIDEOINFOHEADER*) pin_mediatype->mediatype->pbFormat;
                    video_info->bmiHeader.biWidth = width;
                    video_info->bmiHeader.biHeight = height;
                    video_info->AvgTimePerFrame =
                        (LONGLONG)(10000000.0 * denominator / (double) numerator + 0.5);
                    video_info->bmiHeader.biSizeImage = DIBSIZE(video_info->bmiHeader);
                    pin_mediatype->mediatype->lSampleSize = DIBSIZE(video_info->bmiHeader);
                } else if (pin_mediatype->mediatype->formattype == FORMAT_MPEG2Video ||
                           pin_mediatype->mediatype->formattype == FORMAT_VideoInfo2) {
                    video_info2 = (VIDEOINFOHEADER2*) pin_mediatype->mediatype->pbFormat;
                    video_info2->bmiHeader.biWidth = width;
                    video_info2->bmiHeader.biHeight = height;
                    video_info2->AvgTimePerFrame =
                        (LONGLONG)(10000000.0 * denominator / (double) numerator + 0.5);
                    video_info2->bmiHeader.biSizeImage = DIBSIZE(video_info2->bmiHeader);

                    pin_mediatype->mediatype->lSampleSize = DIBSIZE(video_info2->bmiHeader);
                }


                src->dshow_fakesink->gst_set_media_type(pin_mediatype->mediatype);
                src->dshow_fakesink->gst_set_buffer_callback(
                    (push_buffer_func) gst_dshowvideosrc_push_buffer, src);

                gst_dshow_get_pin_from_filter(src->dshow_fakesink, PINDIR_INPUT,
                                              &input_pin);

                if (!input_pin) {
                    GST_ERROR("Can't get input pin from our dshow fakesink");
                    goto error;
                }

                hres = src->filter_graph->ConnectDirect(pin_mediatype->capture_pin,
                                                        input_pin, pin_mediatype->mediatype);
                input_pin->Release();

                if (hres != S_OK) {
                    GST_ERROR
                    ("Can't connect capture filter with fakesink filter (error=0x%x)",
                     hres);
                    goto error;
                }

                /* save width and height negociated */
                gst_structure_get_int(s, "width", &src->width);
                gst_structure_get_int(s, "height", &src->height);

                src->is_rgb = FALSE;
                caps_string = gst_caps_to_string(caps);

                if (caps_string) {
                    if (strstr(caps_string, "video/x-raw-rgb")) {
                        src->is_rgb = TRUE;
                    } else {
                        src->is_rgb = FALSE;
                    }

                    g_free(caps_string);
                }
            }
        }
    }

    return TRUE;

error:
    return FALSE;
}

static gboolean
gst_dshowvideosrc_stop(GstBaseSrc* bsrc) {
    IPin* input_pin = NULL, *output_pin = NULL;
    HRESULT hres = S_FALSE;
    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(bsrc);

    if (!src->filter_graph) {
        return TRUE;
    }

    /* disconnect filters */
    gst_dshow_get_pin_from_filter(src->video_cap_filter, PINDIR_OUTPUT,
                                  &output_pin);

    if (output_pin) {
        hres = src->filter_graph->Disconnect(output_pin);
        output_pin->Release();
    }

    gst_dshow_get_pin_from_filter(src->dshow_fakesink, PINDIR_INPUT, &input_pin);

    if (input_pin) {
        hres = src->filter_graph->Disconnect(input_pin);
        input_pin->Release();
    }

    /* remove filters from the graph */
    src->filter_graph->RemoveFilter(src->video_cap_filter);
    src->filter_graph->RemoveFilter(src->dshow_fakesink);

    /* release our gstreamer dshow sink */
    src->dshow_fakesink->Release();
    src->dshow_fakesink = NULL;

    /* release media filter interface */
    src->media_filter->Release();
    src->media_filter = NULL;

    /* release the filter graph manager */
    src->filter_graph->Release();
    src->filter_graph = NULL;

    /* reset caps */
    if (src->caps) {
        gst_caps_unref(src->caps);
        src->caps = NULL;
    }

    /* reset device id */
    if (src->device) {
        g_free(src->device);
        src->device = NULL;
    }

    return TRUE;
}

static gboolean
gst_dshowvideosrc_unlock(GstBaseSrc* bsrc) {
    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(bsrc);

    g_mutex_lock(src->buffer_mutex);
    src->stop_requested = TRUE;
    g_cond_signal(src->buffer_cond);
    g_mutex_unlock(src->buffer_mutex);

    return TRUE;
}

static gboolean
gst_dshowvideosrc_unlock_stop(GstBaseSrc* bsrc) {
    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(bsrc);

    src->stop_requested = FALSE;

    return TRUE;
}

static GstFlowReturn
gst_dshowvideosrc_create(GstPushSrc* psrc, GstBuffer** buf) {
    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(psrc);

    g_mutex_lock(src->buffer_mutex);

    while (src->buffer == NULL && !src->stop_requested) {
        g_cond_wait(src->buffer_cond, src->buffer_mutex);
    }

    *buf = src->buffer;
    src->buffer = NULL;
    g_mutex_unlock(src->buffer_mutex);

    if (src->stop_requested) {
        if (*buf != NULL) {
            gst_buffer_unref(*buf);
            *buf = NULL;
        }

        return GST_FLOW_WRONG_STATE;
    }

    GST_DEBUG("dshowvideosrc_create => pts %" GST_TIME_FORMAT " duration %"
              GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(*buf)),
              GST_TIME_ARGS(GST_BUFFER_DURATION(*buf)));

    return GST_FLOW_OK;
}

static GstCaps*
gst_dshowvideosrc_getcaps_from_streamcaps(GstDshowVideoSrc* src, IPin* pin) {
    GstCaps* caps = NULL;
    HRESULT hres = S_OK;
    int icount = 0;
    int isize = 0;
    VIDEO_STREAM_CONFIG_CAPS vscc;
    int i = 0;
    IAMStreamConfig* streamcaps = NULL;

    hres = pin->QueryInterface(IID_IAMStreamConfig, (LPVOID*) & streamcaps);

    if (FAILED(hres)) {
        GST_ERROR("Failed to retrieve IAMStreamConfig (error=0x%x)", hres);
        return NULL;
    }

    streamcaps->GetNumberOfCapabilities(&icount, &isize);

    if (isize != sizeof(vscc)) {
        streamcaps->Release();
        return NULL;
    }

    caps = gst_caps_new_empty();

    for (i = 0; i < icount; i++) {

        GstCapturePinMediaType* pin_mediatype =
            gst_dshow_new_pin_mediatype_from_streamcaps(pin, i, streamcaps);

        if (pin_mediatype) {
            if (gst_dshowvideosrc_filter_caps_disabled(src, pin_mediatype)) {
                gst_dshow_free_pin_mediatype(pin_mediatype);
                continue;
            }

            GstCaps* mediacaps = NULL;
            GstVideoFormat video_format =
                gst_dshow_guid_to_gst_video_format(pin_mediatype->mediatype);

            if (video_format != GST_VIDEO_FORMAT_UNKNOWN) {
                mediacaps = gst_dshow_new_video_caps(video_format, NULL,
                                                     pin_mediatype);

            } else if (gst_dshow_check_mediatype(pin_mediatype->mediatype,
                                                 MEDIASUBTYPE_dvsd, FORMAT_VideoInfo)) {
                mediacaps =
                    gst_dshow_new_video_caps(GST_VIDEO_FORMAT_UNKNOWN,
                                             "video/x-dv, systemstream=FALSE", pin_mediatype);

            } else if (gst_dshow_check_mediatype(pin_mediatype->mediatype,
                                                 MEDIASUBTYPE_dvsd, FORMAT_DvInfo)) {
                mediacaps =
                    gst_dshow_new_video_caps(GST_VIDEO_FORMAT_UNKNOWN,
                                             "video/x-dv, systemstream=TRUE", pin_mediatype);

                pin_mediatype->granularityWidth = 0;
                pin_mediatype->granularityHeight = 0;
            }

            if (mediacaps) {
                src->pins_mediatypes =
                    g_list_append(src->pins_mediatypes, pin_mediatype);
                gst_caps_append(caps, mediacaps);
            } else {
                /* failed to convert dshow caps */
                gst_dshow_free_pin_mediatype(pin_mediatype);
            }
        }
    }

    streamcaps->Release();

    if (caps && gst_caps_is_empty(caps)) {
        gst_caps_unref(caps);
        caps = NULL;
    }

    return caps;
}

static GstCaps*
gst_dshowvideosrc_getcaps_from_enum_mediatypes(GstDshowVideoSrc* src, IPin* pin) {
    GstCaps* caps = NULL;
    IEnumMediaTypes* enum_mediatypes = NULL;
    HRESULT hres = S_OK;
    GstCapturePinMediaType* pin_mediatype = NULL;

    hres = pin->EnumMediaTypes(&enum_mediatypes);

    if (FAILED(hres)) {
        GST_ERROR("Failed to retrieve IEnumMediaTypes (error=0x%x)", hres);
        return NULL;
    }

    caps = gst_caps_new_empty();

    while ((pin_mediatype = gst_dshow_new_pin_mediatype_from_enum_mediatypes(pin,
                            enum_mediatypes)) != NULL) {

        GstCaps* mediacaps = NULL;
        GstVideoFormat video_format = gst_dshow_guid_to_gst_video_format(pin_mediatype->mediatype);

        if (video_format != GST_VIDEO_FORMAT_UNKNOWN)
            mediacaps = gst_video_format_new_caps(video_format,
                                                  pin_mediatype->defaultWidth, pin_mediatype->defaultHeight,
                                                  pin_mediatype->defaultFPS, 1000, 1, 1);

        if (mediacaps) {
            src->pins_mediatypes =
                g_list_append(src->pins_mediatypes, pin_mediatype);
            gst_caps_append(caps, mediacaps);
        } else {
            /* failed to convert dshow caps */
            gst_dshow_free_pin_mediatype(pin_mediatype);
        }
    }

    enum_mediatypes->Release();

    if (caps && gst_caps_is_empty(caps)) {
        gst_caps_unref(caps);
        caps = NULL;
    }

    return caps;
}

static gboolean
gst_dshowvideosrc_push_buffer(guint8* buffer, guint size, gpointer src_object,
                              GstClockTime duration) {
    GstDshowVideoSrc* src = GST_DSHOWVIDEOSRC(src_object);
    GstBuffer* buf = NULL;
    IPin* pPin = NULL;
    HRESULT hres = S_FALSE;
    AM_MEDIA_TYPE* pMediaType = NULL;

    if (!buffer || size == 0 || !src) {
        return FALSE;
    }

    /* create a new buffer assign to it the clock time as timestamp */
    buf = gst_buffer_new_and_alloc(size);

    GST_BUFFER_SIZE(buf) = size;

    GstClock* clock = gst_element_get_clock(GST_ELEMENT(src));
    GST_BUFFER_TIMESTAMP(buf) =
        GST_CLOCK_DIFF(gst_element_get_base_time(GST_ELEMENT(src)), gst_clock_get_time(clock));
    gst_object_unref(clock);

    GST_BUFFER_DURATION(buf) = duration;

    if (src->is_rgb) {
        /* FOR RGB directshow decoder will return bottom-up BITMAP
         * There is probably a way to get top-bottom video frames from
         * the decoder...
         */
        gint line = 0;
        gint stride = size / src->height;

        for (; line < src->height; line++) {
            memcpy(GST_BUFFER_DATA(buf) + (line * stride),
                   buffer + (size - ((line + 1) * (stride))), stride);
        }
    } else {
        memcpy(GST_BUFFER_DATA(buf), buffer, size);
    }

    GST_DEBUG("push_buffer => pts %" GST_TIME_FORMAT "duration %"
              GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buf)),
              GST_TIME_ARGS(duration));

    /* the negotiate() method already set caps on the source pad */
    gst_buffer_set_caps(buf, GST_PAD_CAPS(GST_BASE_SRC_PAD(src)));

    g_mutex_lock(src->buffer_mutex);

    if (src->buffer != NULL) {
        gst_buffer_unref(src->buffer);
    }

    src->buffer = buf;
    g_cond_signal(src->buffer_cond);
    g_mutex_unlock(src->buffer_mutex);

    return TRUE;
}
