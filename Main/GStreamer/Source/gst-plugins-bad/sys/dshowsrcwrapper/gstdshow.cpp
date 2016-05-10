/* GStreamer
 * Copyright (C) 2007 Sebastien Moutte <sebastien@moutte.net>
 *
 * gstdshow.cpp:
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

#include "gstdshow.h"
#include "gstdshowfakesink.h"
#include <dvdmedia.h>
#include <atlconv.h>

#ifdef __cplusplus
extern "C" {
#endif

int win32_regex_match(const char*, const char*);

#ifdef __cplusplus
};
#endif

const GUID MEDIASUBTYPE_I420
= { 0x30323449, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B,
                                 0x71
                                }
  };

void
gst_dshow_free_mediatype(AM_MEDIA_TYPE* pmt) {
    if (pmt != NULL) {
        if (pmt->cbFormat != 0) {
            CoTaskMemFree((PVOID) pmt->pbFormat);
            pmt->cbFormat = 0;
            pmt->pbFormat = NULL;
        }

        if (pmt->pUnk != NULL) {
            /* Unecessary because pUnk should not be used, but safest. */
            pmt->pUnk->Release();
            pmt->pUnk = NULL;
        }

        CoTaskMemFree(pmt);
    }
}

void
gst_dshow_free_pin_mediatype(gpointer pt) {
    GstCapturePinMediaType* pin_mediatype = (GstCapturePinMediaType*) pt;

    if (pin_mediatype) {
        if (pin_mediatype->capture_pin) {
            pin_mediatype->capture_pin->Release();
            pin_mediatype->capture_pin = NULL;
        }

        if (pin_mediatype->mediatype) {
            gst_dshow_free_mediatype(pin_mediatype->mediatype);
            pin_mediatype->mediatype = NULL;
        }
        g_free(pt);
    }
}

GstCapturePinMediaType*
gst_dshow_new_pin_mediatype(IPin* pin) {
    GstCapturePinMediaType* pin_mediatype = g_new0(GstCapturePinMediaType, 1);

    pin->AddRef();
    pin_mediatype->capture_pin = pin;

    return pin_mediatype;
}

GstCapturePinMediaType*
gst_dshow_new_pin_mediatype_from_enum_mediatypes(IPin* pin, IEnumMediaTypes* enum_mediatypes) {
    GstCapturePinMediaType* pin_mediatype = gst_dshow_new_pin_mediatype(pin);
    VIDEOINFOHEADER* video_info = NULL;
    VIDEOINFOHEADER2* video_info2 = NULL;

    HRESULT hres = enum_mediatypes->Next(1, &pin_mediatype->mediatype, NULL);

    if (hres != S_OK || !pin_mediatype->mediatype) {
        gst_dshow_free_pin_mediatype(pin_mediatype);
        return NULL;
    }

    if (pin_mediatype->mediatype->formattype == FORMAT_MPEGVideo ||
            pin_mediatype->mediatype->formattype == FORMAT_VideoInfo) {
        video_info = (VIDEOINFOHEADER*) pin_mediatype->mediatype->pbFormat;

        pin_mediatype->defaultWidth = video_info->bmiHeader.biWidth;
        pin_mediatype->defaultHeight = video_info->bmiHeader.biHeight;

        if (video_info->AvgTimePerFrame > 0) {
            pin_mediatype->defaultFPS = (gint)(GST_SECOND * 10.0 / video_info->AvgTimePerFrame + 0.5);
        } else {
            pin_mediatype->defaultFPS = 0;
        }
    } else if (pin_mediatype->mediatype->formattype == FORMAT_MPEG2Video ||
               pin_mediatype->mediatype->formattype == FORMAT_VideoInfo2) {
        video_info2 = (VIDEOINFOHEADER2*) pin_mediatype->mediatype->pbFormat;

        pin_mediatype->defaultWidth = video_info2->bmiHeader.biWidth;
        pin_mediatype->defaultHeight = video_info2->bmiHeader.biHeight;

        if (video_info2->AvgTimePerFrame > 0) {
            pin_mediatype->defaultFPS = (gint)(GST_SECOND * 10.0 / video_info2->AvgTimePerFrame + 0.5);
        } else {
            pin_mediatype->defaultFPS = 0;
        }
    }

    pin_mediatype->granularityWidth = 1;
    pin_mediatype->granularityHeight = 1;

    return pin_mediatype;
}

GstCapturePinMediaType*
gst_dshow_new_pin_mediatype_from_streamcaps(IPin* pin, gint id, IAMStreamConfig* streamcaps) {
    GstCapturePinMediaType* pin_mediatype = gst_dshow_new_pin_mediatype(pin);
    VIDEOINFOHEADER* video_info = NULL;
    VIDEOINFOHEADER2* video_info2 = NULL;

    HRESULT hres = streamcaps->GetStreamCaps(id, &pin_mediatype->mediatype,
                   (BYTE*) & pin_mediatype->vscc);

    if (FAILED(hres) || !pin_mediatype->mediatype) {
        gst_dshow_free_pin_mediatype(pin_mediatype);
        return NULL;
    }

    if (pin_mediatype->mediatype->formattype == FORMAT_MPEGVideo ||
            pin_mediatype->mediatype->formattype == FORMAT_VideoInfo) {
        video_info = (VIDEOINFOHEADER*) pin_mediatype->mediatype->pbFormat;

        pin_mediatype->defaultWidth = video_info->bmiHeader.biWidth;
        pin_mediatype->defaultHeight = video_info->bmiHeader.biHeight;

        if (video_info->AvgTimePerFrame > 0) {
            pin_mediatype->defaultFPS = (gint)(GST_SECOND * 10.0 / video_info->AvgTimePerFrame + 0.5);
        } else {
            pin_mediatype->defaultFPS = 0;
        }
    } else if (pin_mediatype->mediatype->formattype == FORMAT_MPEG2Video ||
               pin_mediatype->mediatype->formattype == FORMAT_VideoInfo2) {
        video_info2 = (VIDEOINFOHEADER2*) pin_mediatype->mediatype->pbFormat;

        pin_mediatype->defaultWidth = video_info2->bmiHeader.biWidth;
        pin_mediatype->defaultHeight = video_info2->bmiHeader.biHeight;

        if (video_info2->AvgTimePerFrame > 0) {
            pin_mediatype->defaultFPS = (gint)(GST_SECOND * 10.0 / video_info2->AvgTimePerFrame + 0.5);
        } else {
            pin_mediatype->defaultFPS = 0;
        }
    }

    pin_mediatype->granularityWidth = pin_mediatype->vscc.OutputGranularityX;
    pin_mediatype->granularityHeight = pin_mediatype->vscc.OutputGranularityY;

    return pin_mediatype;
}

void
gst_dshow_free_pins_mediatypes(GList* pins_mediatypes) {
    while (pins_mediatypes != NULL) {
        gst_dshow_free_pin_mediatype(pins_mediatypes->data);
        pins_mediatypes = g_list_remove_link(pins_mediatypes, pins_mediatypes);
    }
}

gboolean
gst_dshow_check_mediatype(AM_MEDIA_TYPE* media_type, const GUID sub_type,
                          const GUID format_type) {
    RPC_STATUS rpcstatus;

    g_return_val_if_fail(media_type != NULL, FALSE);

    return
        UuidCompare(&media_type->subtype, (UUID*) & sub_type,
                    &rpcstatus) == 0 && rpcstatus == RPC_S_OK &&
        //IsEqualGUID (&media_type->subtype, &sub_type)
        UuidCompare(&media_type->formattype, (UUID*) & format_type,
                    &rpcstatus) == 0 && rpcstatus == RPC_S_OK;
}

gboolean
gst_dshow_get_pin_from_filter(IBaseFilter* filter, PIN_DIRECTION pindir,
                              IPin** pin) {
    gboolean ret = FALSE;
    IEnumPins* enumpins = NULL;
    IPin* pintmp = NULL;
    HRESULT hres;
    *pin = NULL;

    hres = filter->EnumPins(&enumpins);

    if (FAILED(hres)) {
        return ret;
    }

    while (enumpins->Next(1, &pintmp, NULL) == S_OK) {
        PIN_DIRECTION pindirtmp;
        hres = pintmp->QueryDirection(&pindirtmp);

        if (hres == S_OK && pindir == pindirtmp) {
            *pin = pintmp;
            ret = TRUE;
            break;
        }

        pintmp->Release();
    }

    enumpins->Release();

    return ret;
}

gboolean
gst_dshow_find_filter(CLSID input_majortype, CLSID input_subtype,
                      CLSID output_majortype, CLSID output_subtype,
                      gchar* prefered_filter_name, IBaseFilter** filter) {
    gboolean ret = FALSE;
    HRESULT hres;
    GUID arrayInTypes[2];
    GUID arrayOutTypes[2];
    IFilterMapper2* mapper = NULL;
    IEnumMoniker* enum_moniker = NULL;
    IMoniker* moniker = NULL;
    ULONG fetched;
    gchar* prefered_filter_upper = NULL;
    gboolean exit = FALSE;

    /* initialize output parameter */
    if (filter) {
        *filter = NULL;
    }

    /* create a private copy of prefered filter substring in upper case */
    if (prefered_filter_name) {
        prefered_filter_upper = g_strdup(prefered_filter_name);
        _strupr(prefered_filter_upper);
    }

    hres = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC,
                            IID_IFilterMapper2, (void**) &mapper);

    if (FAILED(hres)) {
        goto clean;
    }

    memcpy(&arrayInTypes[0], &input_majortype, sizeof(CLSID));
    memcpy(&arrayInTypes[1], &input_subtype, sizeof(CLSID));
    memcpy(&arrayOutTypes[0], &output_majortype, sizeof(CLSID));
    memcpy(&arrayOutTypes[1], &output_subtype, sizeof(CLSID));

    hres =
        mapper->EnumMatchingFilters(&enum_moniker, 0, FALSE,
                                    MERIT_DO_NOT_USE + 1, TRUE, 1, arrayInTypes, NULL, NULL, FALSE, TRUE, 1,
                                    arrayOutTypes, NULL, NULL);

    if (FAILED(hres)) {
        goto clean;
    }

    enum_moniker->Reset();

    while (hres = enum_moniker->Next(1, &moniker, &fetched), hres == S_OK
            && !exit) {
        IBaseFilter* filter_temp = NULL;
        IPropertyBag* property_bag = NULL;
        gchar* friendly_name = NULL;

        hres =
            moniker->BindToStorage(NULL, NULL, IID_IPropertyBag,
                                   (void**) &property_bag);

        if (SUCCEEDED(hres) && property_bag) {
            VARIANT varFriendlyName;
            VariantInit(&varFriendlyName);

            hres = property_bag->Read(L"FriendlyName", &varFriendlyName, NULL);

            if (hres == S_OK && varFriendlyName.bstrVal) {
                friendly_name =
                    g_utf16_to_utf8((const gunichar2*) varFriendlyName.bstrVal,
                                    wcslen(varFriendlyName.bstrVal), NULL, NULL, NULL);

                if (friendly_name) {
                    _strupr(friendly_name);
                }

                SysFreeString(varFriendlyName.bstrVal);
            }

            property_bag->Release();
        }

        hres =
            moniker->BindToObject(NULL, NULL, IID_IBaseFilter,
                                  (void**) &filter_temp);

        if (SUCCEEDED(hres) && filter_temp) {
            ret = TRUE;

            if (filter) {
                if (*filter) {
                    (*filter)->Release();
                }

                *filter = filter_temp;
                (*filter)->AddRef();

                if (prefered_filter_upper && friendly_name &&
                        strstr(friendly_name, prefered_filter_upper)) {
                    exit = TRUE;
                }
            }

            /* if we just want to know if the formats are supported OR
               if we don't care about what will be the filter used
               => we can stop enumeration */
            if (!filter || !prefered_filter_upper) {
                exit = TRUE;
            }

            filter_temp->Release();
        }

        if (friendly_name) {
            g_free(friendly_name);
        }

        moniker->Release();
    }

clean:

    if (prefered_filter_upper) {
        g_free(prefered_filter_upper);
    }

    if (enum_moniker) {
        enum_moniker->Release();
    }

    if (mapper) {
        mapper->Release();
    }

    return ret;
}


gchar*
gst_dshow_getdevice_from_devicename(const GUID* device_category,
                                    gchar** device_name) {
    gchar* ret = NULL;
    ICreateDevEnum* devices_enum = NULL;
    IEnumMoniker* enum_moniker = NULL;
    IMoniker* moniker = NULL;
    HRESULT hres = S_FALSE;
    ULONG fetched;
    gboolean bfound = FALSE;
    gboolean matched = FALSE;
    gboolean regex = FALSE;
    gchar* local_device = NULL;
    int device_count = 0;

    hres = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                            IID_ICreateDevEnum, (void**) &devices_enum);

    if (hres != S_OK) {
        GST_ERROR("CoCreateInstance failed. %X", hres);
        /*error */
        goto clean;
    }

    hres = devices_enum->CreateClassEnumerator(*device_category,
            &enum_moniker, 0);

    if (hres != S_OK || !enum_moniker) {
        /*error */
        GST_ERROR("CreateClassEnumerator failed. %X", hres);
        goto clean;
    }

    local_device = *device_name;

    // ������ʽƥ��
    if (local_device && _strnicmp(local_device, "REG:", 4) == 0) {
        local_device += 4;

        while (*local_device && isspace(*local_device)) {
            local_device++;
        }

        GST_INFO("device name use regex search...");
        regex = TRUE;
    }


    enum_moniker->Reset();

    while (hres = enum_moniker->Next(1, &moniker, &fetched), hres == S_OK
            && !bfound) {
        device_count ++;
        IPropertyBag* property_bag = NULL;
        hres =
            moniker->BindToStorage(NULL, NULL, IID_IPropertyBag,
                                   (void**) &property_bag);

        if (SUCCEEDED(hres) && property_bag) {
            VARIANT varFriendlyName;
            VariantInit(&varFriendlyName);

            hres = property_bag->Read(L"FriendlyName", &varFriendlyName, NULL);

            if (hres == S_OK && varFriendlyName.bstrVal) {
                USES_CONVERSION;
                gchar* friendly_name =
                    g_utf16_to_utf8((const gunichar2*) varFriendlyName.bstrVal,
                                    wcslen(varFriendlyName.bstrVal), NULL, NULL, NULL);
                gchar* friendly_name_mb = g_strdup(W2A(varFriendlyName.bstrVal));

                if (!*device_name) {
                    *device_name = g_strdup(friendly_name);
                    local_device = *device_name;
                }


                matched = FALSE;

                // ������ʽƥ��
                if (regex) {
                    matched = win32_regex_match(local_device, friendly_name);
                }
                // ��ȷƥ��
                else {
                    matched = (_stricmp(local_device, friendly_name) == 0);
                }

                //���������ַ��޷�ƥ�����⣬ ת��locale�ٳ���һ�αȽ�
                if (!matched) {
                    LPCWSTR locale_local = (LPCWSTR)g_utf8_to_utf16(local_device, -1, 0, 0, 0);

                    if (locale_local) {
                        gchar* local_device_mb = g_strdup(W2A(locale_local));
                        g_free((gpointer)locale_local);

                        if (local_device_mb && friendly_name_mb) {
                            if (regex) {
                                matched = win32_regex_match(local_device_mb, friendly_name_mb);
                            } else {
                                matched = (_stricmp(local_device_mb, friendly_name_mb) == 0);
                            }
                        }
                        g_free(local_device_mb);
                    }
                }

                g_free(friendly_name_mb);

                if (matched) {
                    WCHAR* wszDisplayName = NULL;
                    hres = moniker->GetDisplayName(NULL, NULL, &wszDisplayName);

                    if (hres == S_OK && wszDisplayName) {
                        ret = g_utf16_to_utf8((const gunichar2*) wszDisplayName,
                                              wcslen(wszDisplayName), NULL, NULL, NULL);
                        CoTaskMemFree(wszDisplayName);
                        GST_INFO("get device found: %s", ret);
                    } else {
                        GST_ERROR("get device moniker->GetDisplayName failed %X", hres);
                    }

                    bfound = TRUE;
                } else {
                    GST_WARNING("finding device %s, but device %s found, regex %d", local_device, friendly_name, regex);
                }

                SysFreeString(varFriendlyName.bstrVal);
                g_free(friendly_name);
            } else {
                GST_ERROR("property_bag->Read FriendlyName failed %X", hres);
            }

            property_bag->Release();
        } else {
            GST_ERROR("get device moniker->BindToStorage failed %X", hres);
        }

        moniker->Release();
    }


clean:

    if (enum_moniker) {
        enum_moniker->Release();
    }

    if (devices_enum) {
        devices_enum->Release();
    }

    GST_INFO("get device result: %s, device count tried %d", ret, device_count);

    return ret;
}

gboolean
gst_dshow_show_propertypage(IBaseFilter* base_filter) {
    gboolean ret = FALSE;
    ISpecifyPropertyPages* pProp = NULL;
    HRESULT hres =
        base_filter->QueryInterface(IID_ISpecifyPropertyPages, (void**) &pProp);

    if (SUCCEEDED(hres)) {
        /* Get the filter's name and IUnknown pointer. */
        FILTER_INFO FilterInfo;
        CAUUID caGUID;
        IUnknown* pFilterUnk = NULL;
        hres = base_filter->QueryFilterInfo(&FilterInfo);
        base_filter->QueryInterface(IID_IUnknown, (void**) &pFilterUnk);

        /* Show the page. */
        pProp->GetPages(&caGUID);
        pProp->Release();
        OleCreatePropertyFrame(GetDesktopWindow(), 0, 0, FilterInfo.achName,
                               1, &pFilterUnk, caGUID.cElems, caGUID.pElems, 0, 0, NULL);

        pFilterUnk->Release();
        FilterInfo.pGraph->Release();
        CoTaskMemFree(caGUID.pElems);
    }

    return ret;
}
#define SUBTYPE_FIX 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
static const GUID MEDIASUBTYPE_v210 = { MAKEFOURCC('v', '2', '1', '0'), SUBTYPE_FIX };
static const GUID MEDIASUBTYPE_HDYC = { MAKEFOURCC('H', 'D', 'Y', 'C'), SUBTYPE_FIX };

GstVideoFormat
gst_dshow_guid_to_gst_video_format(AM_MEDIA_TYPE* mediatype) {
    if (gst_dshow_check_mediatype(mediatype, MEDIASUBTYPE_I420, FORMAT_VideoInfo)) {
        return GST_VIDEO_FORMAT_I420;
    }

    if (gst_dshow_check_mediatype(mediatype, MEDIASUBTYPE_RGB24, FORMAT_VideoInfo)) {
        return GST_VIDEO_FORMAT_BGR;
    }

    if (gst_dshow_check_mediatype(mediatype, MEDIASUBTYPE_YUY2, FORMAT_VideoInfo)) {
        return GST_VIDEO_FORMAT_YUY2;
    }

    if (gst_dshow_check_mediatype(mediatype, MEDIASUBTYPE_YVYU, FORMAT_VideoInfo)) {
        return GST_VIDEO_FORMAT_YVYU;
    }

    if (gst_dshow_check_mediatype(mediatype, MEDIASUBTYPE_UYVY, FORMAT_VideoInfo)) {
        return GST_VIDEO_FORMAT_UYVY;
    }

    if (gst_dshow_check_mediatype(mediatype, MEDIASUBTYPE_v210, FORMAT_VideoInfo)) {
        return GST_VIDEO_FORMAT_v210;
    }

    if (gst_dshow_check_mediatype(mediatype, MEDIASUBTYPE_HDYC, FORMAT_VideoInfo)) {
        return GST_VIDEO_FORMAT_UYVY;
    }

    return GST_VIDEO_FORMAT_UNKNOWN;
}

GstCaps*
gst_dshow_new_video_caps(GstVideoFormat video_format, const gchar* name,
                         GstCapturePinMediaType* pin_mediatype) {
    GstCaps* video_caps = NULL;
    GstStructure* video_structure = NULL;
    gint min_w, max_w;
    gint min_h, max_h;
    gint min_fr, max_fr;

    /* raw video format */
    switch (video_format) {
    case GST_VIDEO_FORMAT_BGR:
        video_caps = gst_caps_from_string(GST_VIDEO_CAPS_BGR);
        break;

    case GST_VIDEO_FORMAT_I420:
        video_caps = gst_caps_from_string(GST_VIDEO_CAPS_YUV("I420"));
        break;

    case GST_VIDEO_FORMAT_YUY2:
        video_caps = gst_caps_from_string(GST_VIDEO_CAPS_YUV("YUY2"));
        break;

    case GST_VIDEO_FORMAT_YVYU:
        video_caps = gst_caps_from_string(GST_VIDEO_CAPS_YUV("YVYU"));
        break;

    case GST_VIDEO_FORMAT_UYVY:
        video_caps = gst_caps_from_string(GST_VIDEO_CAPS_YUV("UYVY"));
        break;

    case GST_VIDEO_FORMAT_v210:
        video_caps = gst_caps_from_string(GST_VIDEO_CAPS_YUV("v210"));
        break;

    default:
        break;
    }

    /* other video format */
    if (!video_caps) {
        if (g_strcasecmp(name, "video/x-dv, systemstream=FALSE") == 0) {
            video_caps = gst_caps_new_simple("video/x-dv",
                                             "systemstream", G_TYPE_BOOLEAN, FALSE,
                                             "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('d', 'v', 's', 'd'),
                                             NULL);
        } else if (g_strcasecmp(name, "video/x-dv, systemstream=TRUE") == 0) {
            video_caps = gst_caps_new_simple("video/x-dv",
                                             "systemstream", G_TYPE_BOOLEAN, TRUE, NULL);
            return video_caps;
        }
    }

    if (!video_caps) {
        return NULL;
    }

    video_structure = gst_caps_get_structure(video_caps, 0);

    /* Hope GST_TYPE_INT_RANGE_STEP will exits in future gstreamer releases  */
    /* because we could use :  */
    /* "width", GST_TYPE_INT_RANGE_STEP, video_default->minWidth, video_default->maxWidth,  video_default->granularityWidth */
    /* instead of : */
    /* "width", GST_TYPE_INT_RANGE, video_default->minWidth, video_default->maxWidth */

    /* For framerate we do not need a step (granularity) because  */
    /* "The IAMStreamConfig::SetFormat method will set the frame rate to the closest  */
    /* value that the filter supports" as it said in the VIDEO_STREAM_CONFIG_CAPS dshwo doc */

    min_w = pin_mediatype->vscc.MinOutputSize.cx;
    max_w = pin_mediatype->vscc.MaxOutputSize.cx;
    min_h = pin_mediatype->vscc.MinOutputSize.cy;
    max_h = pin_mediatype->vscc.MaxOutputSize.cy;
    min_fr = (gint)(10000000 / pin_mediatype->vscc.MaxFrameInterval);
    max_fr = (gint)(10000000 / pin_mediatype->vscc.MinFrameInterval);

    if (min_w == max_w) {
        gst_structure_set(video_structure, "width", G_TYPE_INT, min_w, NULL);
    } else
        gst_structure_set(video_structure,
                          "width", GST_TYPE_INT_RANGE, min_w, max_w, NULL);

    if (min_h == max_h) {
        gst_structure_set(video_structure, "height", G_TYPE_INT, min_h, NULL);
    } else
        gst_structure_set(video_structure,
                          "height", GST_TYPE_INT_RANGE, min_h, max_h, NULL);

    if (min_fr == max_fr)
        gst_structure_set(video_structure, "framerate",
                          GST_TYPE_FRACTION, min_fr, 1, NULL);
    else
        gst_structure_set(video_structure, "framerate",
                          GST_TYPE_FRACTION_RANGE, min_fr, 1, max_fr, 1, NULL);

    return video_caps;
}
