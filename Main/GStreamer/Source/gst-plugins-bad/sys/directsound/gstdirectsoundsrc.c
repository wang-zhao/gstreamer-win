/*
 * GStreamer
 * Copyright 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright 2005 S閎astien Moutte <sebastien@moutte.net>
 * Copyright 2006 Joni Valtanen <joni.valtanen@movial.fi>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

/*
TODO: add device selection and check rate etc.
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/gstbaseaudiosrc.h>

#include "gstdirectsoundsrc.h"

#include <gst/interfaces/propertyprobe.h>

#include <windows.h>
#include <dsound.h>
#include <MMSystem.h>

#define COBJMACROS
#include <mmdeviceapi.h>

GST_DEBUG_CATEGORY_STATIC (directsoundsrc_debug);
#define GST_CAT_DEFAULT directsoundsrc_debug

/* defaults here */
#define DEFAULT_DEVICE 0

/* properties */
enum
{
  PROP_0,
  PROP_DEVICE_NAME,
  PROP_MONITOR_DEVICE
};
enum
{
  SYSTEM_DEVICE_CHANGED,
  LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

static void
gst_directsound_src_get_times(GstBaseSrc *src, GstBuffer *buffer,
                              GstClockTime *start, GstClockTime *end);

static void gst_directsound_src_finalise (GObject * object);

static void gst_directsound_src_set_property (GObject * object,
                                              guint prop_id, const GValue * value, GParamSpec * pspec);

static void gst_directsound_src_get_property (GObject * object,
                                              guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_directsound_src_open (GstAudioSrc * asrc);
static gboolean gst_directsound_src_close (GstAudioSrc * asrc);
static gboolean gst_directsound_src_prepare (GstAudioSrc * asrc,
                                             GstRingBufferSpec * spec);
static gboolean gst_directsound_src_unprepare (GstAudioSrc * asrc);
static void gst_directsound_src_reset (GstAudioSrc * asrc);
static GstCaps *gst_directsound_src_getcaps (GstBaseSrc * bsrc);

static guint gst_directsound_src_read (GstAudioSrc * asrc,
                                       gpointer data, guint length);

static void gst_directsound_src_dispose (GObject * object);

static void gst_directsound_src_do_init (GType type);

static guint gst_directsound_src_delay (GstAudioSrc * asrc);

static void
gst_directsound_src_stop_device_monitor(GstDirectSoundSrc* dsrc);

static GstStaticPadTemplate directsound_src_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
                         GST_PAD_SRC,
                         GST_PAD_ALWAYS,
                         GST_STATIC_CAPS ("audio/x-raw-int, "
                         "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, "
                         "signed = (boolean) { TRUE, FALSE }, "
                         "width = (int) 16, "
                         "depth = (int) 16, "
                         "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ]; "
                         "audio/x-raw-int, "
                         "signed = (boolean) { TRUE, FALSE }, "
                         "width = (int) 8, "
                         "depth = (int) 8, "
                         "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ]"));


static const GList *
gst_directsoundsrc_probe_get_properties (GstPropertyProbe * probe)
{
  GObjectClass *klass = G_OBJECT_GET_CLASS (probe);
  static GList *props = NULL;

  if (!props) {
    GParamSpec *pspec;

    pspec = g_object_class_find_property (klass, "device-name");
    props = g_list_append (props, pspec);
  }

  return props;
}
static BOOL CALLBACK DSEnumProc_GValueArray(LPGUID lpGUID, 
                                            LPCSTR lpszDesc,
                                            LPCSTR lpszDrvName, 
                                            LPVOID lpContext )
{
  GValueArray *array = (GValueArray*)lpContext;
  GValue value = { 0 };
  gchar * utf8name = NULL;


  if (lpGUID == NULL)
  {
    lpszDesc = "DefaultCapture";
  }
  utf8name = g_locale_to_utf8(lpszDesc, strlen(lpszDesc), NULL, NULL, NULL);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, utf8name);
  g_value_array_append (array, &value);
  g_value_unset (&value);
  g_free (utf8name);
  return TRUE;
}

static GValueArray *
gst_directsoundsrc_get_device_name_values (GstDirectSoundSrc * src)
{
  GValueArray *array = g_value_array_new (0);

  if(src->pDirectSoundCaptureEnumerate )
  {
    (src->pDirectSoundCaptureEnumerate)(DSEnumProc_GValueArray, array);
  }

  return array;
}

static GValueArray *
gst_directsoundsrc_probe_get_values (GstPropertyProbe * probe,
                                     guint prop_id, const GParamSpec * pspec)
{
  GstDirectSoundSrc *src = GST_DIRECTSOUND_SRC (probe);
  GValueArray *array = NULL;

  switch (prop_id) {
    case PROP_DEVICE_NAME:
      array = gst_directsoundsrc_get_device_name_values (src);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }

  return array;
}


static void
gst_directsoundsrc_probe_probe_property (GstPropertyProbe * probe,
                                         guint prop_id, const GParamSpec * pspec)
{
  GObjectClass *klass = G_OBJECT_GET_CLASS (probe);

  switch (prop_id) {
    case PROP_DEVICE_NAME:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }
}



static gboolean
gst_directsoundsrc_probe_needs_probe (GstPropertyProbe * probe,
                                      guint prop_id, const GParamSpec * pspec)
{
  static gboolean init = FALSE;
  gboolean ret = FALSE;

  if (!init) {
    ret = TRUE;
    init = TRUE;
  }

  return ret;
}

static void
gst_directsoundsrc_probe_interface_init (GstPropertyProbeInterface * iface)
{
  iface->get_properties = gst_directsoundsrc_probe_get_properties;
  iface->needs_probe = gst_directsoundsrc_probe_needs_probe;
  iface->probe_property = gst_directsoundsrc_probe_probe_property;
  iface->get_values = gst_directsoundsrc_probe_get_values;
}


static void
gst_directsound_src_do_init (GType type)
{ 
  static const GInterfaceInfo dsoundsrc_info = {
    (GInterfaceInitFunc) gst_directsoundsrc_probe_interface_init,
    NULL,
    NULL,
  };

  g_type_add_interface_static (type,
    GST_TYPE_PROPERTY_PROBE, &dsoundsrc_info);


  GST_DEBUG_CATEGORY_INIT (directsoundsrc_debug, "directsoundsrc", 0,
    "DirectSound Src");
}

GST_BOILERPLATE_FULL (GstDirectSoundSrc, gst_directsound_src, GstAudioSrc,
                      GST_TYPE_AUDIO_SRC, gst_directsound_src_do_init);

static void
gst_directsound_src_dispose (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->dispose (object);
}
static void
gst_directsound_src_free_library(GstDirectSoundSrc *dsoundsrc);

static void
gst_directsound_src_finalise (GObject * object)
{
  GstDirectSoundSrc *dsoundsrc = GST_DIRECTSOUND_SRC (object);
  // last chance to close monitor thread, try it any way
  gst_directsound_src_stop_device_monitor(dsoundsrc);

  if( dsoundsrc->device_name )
    g_free(dsoundsrc->device_name);
  if(dsoundsrc->device_name_locale )
    g_free(dsoundsrc->device_name_locale);

  dsoundsrc->device_name = NULL;
  dsoundsrc->device_name_locale = NULL;

  g_mutex_free (dsoundsrc->dsound_lock);
  dsoundsrc->dsound_lock = NULL;

  g_mutex_free(dsoundsrc->dsound_change_lock);
  dsoundsrc->dsound_change_lock = NULL;

  gst_directsound_src_free_library(dsoundsrc);

}

//return TRUE continue, FALSE stop
static BOOL CALLBACK DSEnumProc_appendlist(LPGUID lpGUID, 
                                           LPCSTR lpszDesc,
                                           LPCSTR lpszDrvName, 
                                           LPVOID lpContext )
{
  GList** pplist = (GList**)lpContext;
  *pplist = g_list_append(*pplist, g_strdup_printf("%s,%s", lpszDesc, lpszDrvName));
  return TRUE;
}
static const CLSID CLSID_MMDeviceEnumerator = { 0xbcde0395, 0xe52f, 0x467c,
{0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}
};
static const IID IID_IMMDeviceEnumerator = { 0xa95664d2, 0x9614, 0x4f35,
{0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}
};
static const IID IID_IMMNotificationClient = {0x7991EEC9, 0x7E89, 0x4D85, 
{0x83, 0x90, 0x6C, 0x70, 0x3C, 0xEC, 0x60, 0xC0}
};

static const IID IID_IUnknown = {0x00000000, 0x0000, 0x0000,
{0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
};

typedef struct  _MMNotifcationClient{
  IMMNotificationClient parent;
  GstDirectSoundSrc* directsound;
}MMNotifcationClient;


static
HRESULT __stdcall __gst_directsound_src_IMMNotificationClient_QueryInterface ( 
  IMMNotificationClient * This,
  /* [in] */ REFIID riid,
  /* [iid_is][out] */ 
  __RPC__deref_out  void **ppvObject)
{
  if( IsEqualIID(riid, &IID_IMMNotificationClient) ||
    IsEqualIID(riid, &IID_IUnknown))
  {
    *ppvObject = This;
    return S_OK;
  }
  else
  {
    return E_NOINTERFACE;
  }
}
static
ULONG __stdcall __gst_directsound_src_IMMNotificationClient_AddRef ( 
  IMMNotificationClient * This)
{
  return 2;
}
static
ULONG __stdcall __gst_directsound_src_IMMNotificationClient_Release ( 
  IMMNotificationClient * This)
{
  return 1;
}
static
/* [helpstring][id] */ HRESULT  __stdcall __gst_directsound_src_IMMNotificationClient_OnDeviceStateChanged ( 
  IMMNotificationClient * This,
  /* [in] */ 
  __in  LPCWSTR pwstrDeviceId,
  /* [in] */ 
  __in  DWORD dwNewState)
{
  MMNotifcationClient* c = (MMNotifcationClient*)(This);
  g_atomic_int_compare_and_exchange(&c->directsound->device_changed, 0, 1);
  g_atomic_int_compare_and_exchange(&c->directsound->device_changed_signaled, 0, 1);
  return S_OK;
}
static
/* [helpstring][id] */ HRESULT  __stdcall __gst_directsound_src_IMMNotificationClient_OnDeviceAdded ( 
  IMMNotificationClient * This,
  /* [in] */ 
  __in  LPCWSTR pwstrDeviceId)
{   
  MMNotifcationClient* c = (MMNotifcationClient*)(This);
  g_atomic_int_compare_and_exchange(&c->directsound->device_changed, 0, 1);
  g_atomic_int_compare_and_exchange(&c->directsound->device_changed_signaled, 0, 1);
  return S_OK;
}
static
/* [helpstring][id] */ HRESULT  __stdcall __gst_directsound_src_IMMNotificationClient_OnDeviceRemoved ( 
  IMMNotificationClient * This,
  /* [in] */ 
  __in  LPCWSTR pwstrDeviceId)
{
  MMNotifcationClient* c = (MMNotifcationClient*)(This);
  g_atomic_int_compare_and_exchange(&c->directsound->device_changed, 0, 1);
  g_atomic_int_compare_and_exchange(&c->directsound->device_changed_signaled, 0, 1);
  return S_OK;
}
static
/* [helpstring][id] */ HRESULT __stdcall __gst_directsound_src_IMMNotificationClient_OnDefaultDeviceChanged ( 
  IMMNotificationClient * This,
  /* [in] */ 
  __in  EDataFlow flow,
  /* [in] */ 
  __in  ERole role,
  /* [in] */ 
  __in  LPCWSTR pwstrDefaultDeviceId)
{
  MMNotifcationClient* c = (MMNotifcationClient*)(This);
  g_atomic_int_compare_and_exchange(&c->directsound->device_changed, 0, 1);
  g_atomic_int_compare_and_exchange(&c->directsound->device_changed_signaled, 0, 1);
  return S_OK;
}
static
/* [helpstring][id] */ HRESULT  __stdcall __gst_directsound_src_IMMNotificationClient_OnPropertyValueChanged ( 
  IMMNotificationClient * This,
  /* [in] */ 
  __in  LPCWSTR pwstrDeviceId,
  /* [in] */ 
  __in  const PROPERTYKEY key)
{  
  MMNotifcationClient* c = (MMNotifcationClient*)(This);
  g_atomic_int_compare_and_exchange(&c->directsound->device_changed, 0, 1);
  g_atomic_int_compare_and_exchange(&c->directsound->device_changed_signaled, 0, 1);
  return S_OK;
}



static IMMNotificationClientVtbl MNotificationClientFuncs={
  __gst_directsound_src_IMMNotificationClient_QueryInterface,
  __gst_directsound_src_IMMNotificationClient_AddRef,
  __gst_directsound_src_IMMNotificationClient_Release,
  __gst_directsound_src_IMMNotificationClient_OnDeviceStateChanged,
  __gst_directsound_src_IMMNotificationClient_OnDeviceAdded,
  __gst_directsound_src_IMMNotificationClient_OnDeviceRemoved,
  __gst_directsound_src_IMMNotificationClient_OnDefaultDeviceChanged,
  __gst_directsound_src_IMMNotificationClient_OnPropertyValueChanged
};


static IMMNotificationClient*
__gst_directsound_src_new_MMNotifcationClient(GstDirectSoundSrc* src)
{
  struct _MMNotifcationClient* rs =  g_malloc0(sizeof(struct _MMNotifcationClient));
  IMMNotificationClient* return_val = (IMMNotificationClient*)(rs);

  rs->parent.lpVtbl = &MNotificationClientFuncs;
  rs->directsound = src;

  return return_val;
}
static void 
check_device_changed_and_signal(GstDirectSoundSrc* src)
{
  if(g_atomic_int_compare_and_exchange (&src->device_changed_signaled, 2, 0))
  {
    g_signal_emit (G_OBJECT (src),
      signals[SYSTEM_DEVICE_CHANGED], 0);
  }
  g_atomic_int_compare_and_exchange (&src->device_changed_signaled, 1, 2);
}

static gpointer
__gst_directsound_src_mon_thread(gpointer data){

  GstDirectSoundSrc* dsrc;
  gint64 time_to_wait_util;

  GList* last;
  GList* this;
  int i;
  HRESULT hr;
  IMMDeviceEnumerator* devEnum = NULL;
  IMMNotificationClient* client = NULL;

  dsrc = (GstDirectSoundSrc*)(data);
  
  g_mutex_lock(&dsrc->device_monitor.lock);
  g_cond_signal(&dsrc->device_monitor.init_event);

  CoInitialize(0);

  hr = CoCreateInstance(&CLSID_MMDeviceEnumerator,NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void**)&devEnum);

  if( hr == S_OK )
  {
    client = __gst_directsound_src_new_MMNotifcationClient(dsrc);
    IMMDeviceEnumerator_RegisterEndpointNotificationCallback(devEnum, client);

    while (dsrc->device_monitor.running)
    { 
      time_to_wait_util = g_get_monotonic_time() + GST_MSECOND;
      if( g_cond_wait_until(&dsrc->device_monitor.exit_event, &dsrc->device_monitor.lock, time_to_wait_util))
      {
        break;
      }
      check_device_changed_and_signal(dsrc);
    }

    IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(devEnum, client);
    g_free(client);
  }
  else
  {    
    last = NULL;
    this = NULL;

    if(dsrc->pDirectSoundCaptureEnumerate)
    {
      while(dsrc->device_monitor.running)
      {
        if( last != NULL )
        {
          g_list_free_full(last, g_free);
          last = NULL;
        }

        last = this;
        this = NULL;

        time_to_wait_util = g_get_monotonic_time() + GST_MSECOND;
        if( !g_cond_wait_until(&dsrc->device_monitor.exit_event, &dsrc->device_monitor.lock, time_to_wait_util))
        {
          (dsrc->pDirectSoundCaptureEnumerate)(DSEnumProc_appendlist, &this);

          if( last != NULL && this != NULL )
          {
            if(g_list_length(last) != g_list_length(this))
            {
              g_atomic_int_compare_and_exchange(&dsrc->device_changed, 0, 1);
              g_atomic_int_compare_and_exchange(&dsrc->device_changed_signaled, 0, 1);
            }
            else
            {
              for(i = g_list_length(last)-1; i >=0; i -- )
              {
                if(_stricmp(g_list_nth_data(last, i), g_list_nth_data(this, i)) != 0 )
                {
                  g_atomic_int_compare_and_exchange(&dsrc->device_changed, 0, 1);
                  g_atomic_int_compare_and_exchange(&dsrc->device_changed_signaled, 0, 1);
                  break;
                }
              }
            }
          }
          check_device_changed_and_signal(dsrc);
          continue;
        }

        if( last != NULL )
        {
          g_list_free_full(last, g_free);
          last = NULL;
        }
        if( this != NULL )
        {
          g_list_free_full(this, g_free);
          this = NULL;
        }
        break;
      }
    }
  }

  g_mutex_unlock(&dsrc->device_monitor.lock);
  g_thread_exit(0);
  return 0;
}

static void
gst_directsound_src_start_device_monitor(GstDirectSoundSrc* dsrc)
{
  if ( dsrc->device_monitor.thread == NULL )
  {
    dsrc->device_monitor.running = TRUE;
    g_mutex_init(&dsrc->device_monitor.lock);
    g_cond_init(&dsrc->device_monitor.exit_event);
    g_cond_init(&dsrc->device_monitor.init_event); 
    
    // lock monitor lock, then monitor thread will wait for us till we are ready.
    g_mutex_lock(&dsrc->device_monitor.lock);
    
    dsrc->device_monitor.thread = g_thread_new("dsound_mon", __gst_directsound_src_mon_thread, dsrc);   
    
    g_cond_wait(&dsrc->device_monitor.init_event, &dsrc->device_monitor.lock);
    g_mutex_unlock(&dsrc->device_monitor.lock);
  }
}

static void
gst_directsound_src_stop_device_monitor(GstDirectSoundSrc* dsrc)
{
  if( dsrc->device_monitor.thread )
  {
    //g_cond_signal(&dsrc->device_monitor.exit_event);
    dsrc->device_monitor.running = FALSE;
    g_thread_join(dsrc->device_monitor.thread);
    g_thread_unref(dsrc->device_monitor.thread);
    dsrc->device_monitor.thread = NULL;
  }

  g_mutex_clear(&dsrc->device_monitor.lock);
  g_cond_clear(&dsrc->device_monitor.init_event);
  g_cond_clear(&dsrc->device_monitor.exit_event);
}


static void
gst_directsound_src_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  GST_DEBUG ("initializing directsoundsrc base\n");

  gst_element_class_set_details_simple (element_class, "Direct Sound Audio Src",
    "Source/Audio",
    "Capture from a soundcard via DIRECTSOUND",
    "Joni Valtanen <joni.valtanen@movial.fi>");

  gst_element_class_add_static_pad_template (element_class,
      &directsound_src_src_factory);
}


/* initialize the plugin's class */
static void
gst_directsound_src_class_init (GstDirectSoundSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstBaseAudioSrcClass *gstbaseaudiosrc_class;
  GstAudioSrcClass *gstaudiosrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstbaseaudiosrc_class = (GstBaseAudioSrcClass *) klass;
  gstaudiosrc_class = (GstAudioSrcClass *) klass;

  GST_DEBUG ("initializing directsoundsrc class\n");

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_directsound_src_finalise);
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_directsound_src_dispose);
  gobject_class->get_property =
    GST_DEBUG_FUNCPTR (gst_directsound_src_get_property);
  gobject_class->set_property =
    GST_DEBUG_FUNCPTR (gst_directsound_src_set_property);

  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_directsound_src_getcaps);

  gstaudiosrc_class->open = GST_DEBUG_FUNCPTR (gst_directsound_src_open);
  gstaudiosrc_class->close = GST_DEBUG_FUNCPTR (gst_directsound_src_close);
  gstaudiosrc_class->read = GST_DEBUG_FUNCPTR (gst_directsound_src_read);
  gstaudiosrc_class->prepare = GST_DEBUG_FUNCPTR (gst_directsound_src_prepare);
  gstaudiosrc_class->unprepare =
    GST_DEBUG_FUNCPTR (gst_directsound_src_unprepare);
  gstaudiosrc_class->delay = GST_DEBUG_FUNCPTR (gst_directsound_src_delay);
  gstaudiosrc_class->reset = GST_DEBUG_FUNCPTR (gst_directsound_src_reset);

  gstbasesrc_class->get_times = GST_DEBUG_FUNCPTR (gst_directsound_src_get_times);

  g_object_class_install_property
    (gobject_class, PROP_DEVICE_NAME,
    g_param_spec_string ("device-name", "Device name",
    "Human-readable name of the sound device", NULL,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class,
    PROP_MONITOR_DEVICE, g_param_spec_boolean("monitor-device",
    "Monitor Device","turn on/off device monitor", FALSE,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  signals[SYSTEM_DEVICE_CHANGED] = g_signal_new("system-device-changed",
    GST_TYPE_DIRECTSOUND_SRC, G_SIGNAL_RUN_LAST, 0, (GSignalAccumulator)NULL, (gpointer)NULL,
    g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

}

static void 
gst_directsound_src_get_times(GstBaseSrc *src, GstBuffer *buffer,
                              GstClockTime *start, GstClockTime *end)
{ 
  GstDirectSoundSrc *dsoundsrc;
  GstClockTime stream_time;

  dsoundsrc = GST_DIRECTSOUND_SRC (src);
  stream_time = GST_BUFFER_TIMESTAMP(buffer);

  if(GST_CLOCK_TIME_IS_VALID(stream_time))
  {
    if( dsoundsrc->need_restart )
    {
      *start = stream_time;
      *end = *start + GST_BUFFER_DURATION(buffer);
    }
    else
    {
      *start = GST_CLOCK_TIME_NONE;
      *end = GST_CLOCK_TIME_NONE;
    }

    if( (dsoundsrc->stream_time == GST_CLOCK_TIME_NONE || dsoundsrc->stream_time < stream_time))
      dsoundsrc->stream_time = stream_time;
  }
}

static GstCaps *
gst_directsound_src_getcaps (GstBaseSrc * bsrc)
{
  GstDirectSoundSrc *dsoundsrc;
  GstCaps *caps = NULL;
  GST_DEBUG ("get caps\n");

  dsoundsrc = GST_DIRECTSOUND_SRC (bsrc);


  caps = gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD
    (bsrc)));
  return caps;

}

static void gst_directsound_src_monitor_device(GstDirectSoundSrc* src, BOOL monitor)
{
  if (monitor)
  {
    gst_directsound_src_start_device_monitor(src);
  }
  else 
  {
    gst_directsound_src_stop_device_monitor(src);
  }
}

static void
gst_directsound_src_set_property (GObject * object, guint prop_id,
                                  const GValue * value, GParamSpec * pspec)
{
  GstDirectSoundSrc *src = GST_DIRECTSOUND_SRC (object);
  GST_DEBUG ("set property\n");

  switch (prop_id) {
  case PROP_DEVICE_NAME:
    if (src->device_name) {
      g_free (src->device_name);
      src->device_name = NULL;
    }
    if( src->device_name_locale )
    {
      g_free( src->device_name_locale);
      src->device_name_locale = NULL;
    }

    if (g_value_get_string (value)) {
      src->device_name = g_strdup (g_value_get_string (value));
      src->device_name_locale = g_locale_from_utf8(src->device_name, -1, NULL, NULL, NULL);


    }
    break;
  case PROP_MONITOR_DEVICE:
    gst_directsound_src_monitor_device(src, g_value_get_boolean(value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
gst_directsound_src_get_property (GObject * object, guint prop_id,
                                  GValue * value, GParamSpec * pspec)
{
  GstDirectSoundSrc *src = GST_DIRECTSOUND_SRC (object);

  GST_DEBUG ("get property\n");

  switch (prop_id) {
    case PROP_DEVICE_NAME:
      g_value_set_string(value, src->device_name);
      break;
    case PROP_MONITOR_DEVICE:
      g_value_set_boolean(value, src->device_monitor.thread != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


/* initialize the new element
* instantiate pads and add them to element
* set functions
* initialize structure
*/
static void
gst_directsound_src_init (GstDirectSoundSrc * src,
                          GstDirectSoundSrcClass * gclass)
{
  GST_DEBUG ("initializing directsoundsrc\n");
  src->dsound_lock = g_mutex_new ();
  src->dsound_change_lock = g_mutex_new();
  src->device_name = NULL;
  src->device_name_locale = NULL;
  src->stream_time = GST_CLOCK_TIME_NONE;
  src->device_changed_signaled = 0;
  src->device_monitor.thread = NULL;
  src->invalid = FALSE;

  src->DSoundDLL = LoadLibrary ("dsound.dll");
  if( src->DSoundDLL){

    /* Building the DLL Calls */
    src->pDSoundCaptureCreate =
      (void *) GetProcAddress (src->DSoundDLL,
      "DirectSoundCaptureCreate");

    src->pDirectSoundCaptureEnumerate =
      (void* ) GetProcAddress(src->DSoundDLL,
      "DirectSoundCaptureEnumerateA");  
  }
}
//return TRUE continue, FALSE stop
static BOOL CALLBACK DSEnumProc(LPGUID lpGUID, 
                                LPCSTR lpszDesc,
                                LPCSTR lpszDrvName, 
                                LPVOID lpContext )
{
  LPVOID* ppParam;
  LPGUID ppGuid;
  gchar* device_name;

  ppParam = (LPVOID*)lpContext;

  device_name = ppParam[0];
  ppGuid = ppParam[1];

  if( lpszDesc && device_name)
  {
    if( _stricmp(lpszDesc, device_name) == 0)
    {
      *ppGuid = *lpGUID;
      return FALSE;
    }
  }
  return TRUE;
}
static void
gst_directsound_src_free_library(GstDirectSoundSrc *dsoundsrc)
{
  if( dsoundsrc->pDirectSoundCaptureEnumerate)
    dsoundsrc->pDirectSoundCaptureEnumerate = NULL;
  if( dsoundsrc->pDSoundCaptureCreate)
    dsoundsrc->pDSoundCaptureCreate = NULL;

  if( dsoundsrc->DSoundDLL )
  {
    FreeLibrary(dsoundsrc->DSoundDLL);
    dsoundsrc->DSoundDLL = NULL;
  }
}

// DSDEVID_DefaultCapture;//	System-wide default audio capture device.
// DSDEVID_DefaultVoiceCapture //Default voice capture device.

static gboolean
gst_directsound_src_open (GstAudioSrc * asrc)
{
  GstDirectSoundSrc *dsoundsrc;
  HRESULT hRes;                 /* Result for windows functions */
  GUID deviceId = GUID_NULL;

  GST_DEBUG ("initializing directsoundsrc\n");

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);
  dsoundsrc->stream_time = GST_CLOCK_TIME_NONE;
  dsoundsrc->deviceId = GUID_NULL;
  dsoundsrc->need_restart = FALSE;

  if(! dsoundsrc->DSoundDLL)
    goto dsound_open;

  if( !dsoundsrc->pDSoundCaptureCreate)
    goto capture_function;

  if(dsoundsrc->device_name != NULL)
  {
    if(_stricmp(dsoundsrc->device_name, "DefaultCapture") == 0)
    {
      deviceId = DSDEVID_DefaultCapture;
    }
    else if( _stricmp(dsoundsrc->device_name, "DefaultVoiceCapture") == 0)
    {
      deviceId = DSDEVID_DefaultVoiceCapture;
    }
    else if( dsoundsrc->pDirectSoundCaptureEnumerate )
    {
      void* param[] ={dsoundsrc->device_name_locale, &deviceId, 0};

      (dsoundsrc->pDirectSoundCaptureEnumerate)(DSEnumProc, param);
    }
  }

  /* Create capture object */
  hRes = (dsoundsrc->pDSoundCaptureCreate) (IsEqualGUID(&deviceId,&GUID_NULL) ? NULL : &deviceId, &dsoundsrc->pDSC, NULL);
  if (FAILED (hRes)) {
    goto capture_object;
  }

  dsoundsrc->deviceId = deviceId;

  gst_directsound_src_start_device_monitor(dsoundsrc);

  dsoundsrc->invalid = FALSE;

  return TRUE;

capture_function:
  {
    GST_ELEMENT_ERROR (dsoundsrc, RESOURCE, OPEN_READ,
      ("Unable to get capturecreate function"), (NULL));
    return FALSE;
  }
capture_object:
  {
    GST_ELEMENT_ERROR (dsoundsrc, RESOURCE, OPEN_READ,
      ("Unable to create capture object"), (NULL));
    return FALSE;
  }
dsound_open:
  {
    DWORD err = GetLastError ();
    GST_ELEMENT_ERROR (dsoundsrc, RESOURCE, OPEN_READ,
      ("Unable to open dsound.dll"), (NULL));
    g_print ("0x%lx\n", HRESULT_FROM_WIN32 (err));
    return FALSE;
  }
}

static gboolean
gst_directsound_src_close (GstAudioSrc * asrc)
{
  GstDirectSoundSrc *dsoundsrc;
  HRESULT hRes;                 /* Result for windows functions */

  GST_DEBUG ("initializing directsoundsrc\n");

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);

  gst_directsound_src_stop_device_monitor(dsoundsrc);

  if ( dsoundsrc->pDSC )
  {
    /* Release capture handler  */
    hRes = IDirectSoundCapture_Release (dsoundsrc->pDSC);
    dsoundsrc->pDSC = NULL;
  }
  return TRUE;
}

static gboolean
gst_directsound_src_prepare (GstAudioSrc * asrc, GstRingBufferSpec * spec)
{

  GstDirectSoundSrc *dsoundsrc;
  WAVEFORMATEX wfx;             /* Wave format structure */
  HRESULT hRes;                 /* Result for windows functions */
  DSCBUFFERDESC descSecondary;  /* Capturebuffer decsiption */

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);

  GST_DEBUG ("initializing directsoundsrc\n");

  /* Define buffer */
  memset (&wfx, 0, sizeof (WAVEFORMATEX));
  wfx.wFormatTag = WAVE_FORMAT_PCM;     /* should be WAVE_FORMAT_PCM */
  wfx.nChannels = spec->channels;
  wfx.nSamplesPerSec = spec->rate;      /* 8000|11025|22050|44100 */
  wfx.wBitsPerSample = spec->width;     // 8|16;

  wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
  wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
  wfx.cbSize = 0;               /* This size is allways for PCM-format */

  /* 1 or 2 Channels etc...
  FIXME: Never really tested. Is this ok?
  */
  if (spec->width == 16 && spec->channels == 1) {
    spec->format = GST_S16_LE;
  } else if (spec->width == 16 && spec->channels == 2) {
    spec->format = GST_U16_LE;
  } else if (spec->width == 8 && spec->channels == 1) {
    spec->format = GST_S8;
  } else if (spec->width == 8 && spec->channels == 2) {
    spec->format = GST_U8;
  }

  /* Set the buffer size to two seconds. 
  This should never reached. 
  */
  dsoundsrc->buffer_size = wfx.nAvgBytesPerSec * 2;

  //notifysize * 16; //spec->width; /*original 16*/
  GST_DEBUG ("Buffer size: %d", dsoundsrc->buffer_size);

  /* Init secondary buffer desciption */
  memset (&descSecondary, 0, sizeof (DSCBUFFERDESC));
  descSecondary.dwSize = sizeof (DSCBUFFERDESC);
  descSecondary.dwFlags = 0;
  descSecondary.dwReserved = 0;

  /* This is not primary buffer so have to set size  */
  descSecondary.dwBufferBytes = dsoundsrc->buffer_size;
  descSecondary.lpwfxFormat = &wfx;

  dsoundsrc->invalid = FALSE;
  /* Create buffer */
  hRes = IDirectSoundCapture_CreateCaptureBuffer (dsoundsrc->pDSC,
    &descSecondary, &dsoundsrc->pDSBSecondary, NULL);
  if (hRes != DS_OK) {
    goto capture_buffer;
  }

  spec->channels = wfx.nChannels;
  spec->rate = wfx.nSamplesPerSec;
  spec->bytes_per_sample = (spec->width / 8) * spec->channels;
  dsoundsrc->bytes_per_sample = spec->bytes_per_sample;
  dsoundsrc->wfx = wfx;

  GST_DEBUG ("latency time: %" G_GUINT64_FORMAT " - buffer time: %" G_GUINT64_FORMAT,
    spec->latency_time, spec->buffer_time);

  /* Buffer-time should be allways more than 2*latency */
  if (spec->buffer_time < spec->latency_time * 2) {
    spec->buffer_time = spec->latency_time * 2;
    GST_WARNING ("buffer-time was less than latency");
  }

  /* Save the times */
  dsoundsrc->buffer_time = spec->buffer_time;
  dsoundsrc->latency_time = spec->latency_time;

  dsoundsrc->latency_size = (gint) wfx.nAvgBytesPerSec *
    dsoundsrc->latency_time / 1000000.0;


  spec->segsize = (guint) (((double) spec->buffer_time / 1000000.0) *
    wfx.nAvgBytesPerSec);

  /* just in case */
  if (spec->segsize < 1)
    spec->segsize = 1;

  spec->segtotal = spec->width * (wfx.nAvgBytesPerSec / spec->segsize);

  GST_DEBUG ("bytes/sec: %lu, buffer size: %d, segsize: %d, segtotal: %d",
    wfx.nAvgBytesPerSec,
    dsoundsrc->buffer_size, spec->segsize, spec->segtotal);

  spec->silence_sample[0] = 0;
  spec->silence_sample[1] = 0;
  spec->silence_sample[2] = 0;
  spec->silence_sample[3] = 0;

  if (spec->width != 16 && spec->width != 8)
    goto dodgy_width;

  /* Not readed anything yet */
  dsoundsrc->current_circular_offset = 0;

  dsoundsrc->last_dummy_time = timeGetTime();

  GST_DEBUG ("GstRingBufferSpec->channels: %d, GstRingBufferSpec->rate: %d, \
             GstRingBufferSpec->bytes_per_sample: %d\n\
             WAVEFORMATEX.nSamplesPerSec: %ld, WAVEFORMATEX.wBitsPerSample: %d, \
             WAVEFORMATEX.nBlockAlign: %d, WAVEFORMATEX.nAvgBytesPerSec: %ld\n", spec->channels, spec->rate, spec->bytes_per_sample, wfx.nSamplesPerSec, wfx.wBitsPerSample, wfx.nBlockAlign, wfx.nAvgBytesPerSec);

  return TRUE;

capture_buffer:
  {
    GST_ELEMENT_ERROR (dsoundsrc, RESOURCE, OPEN_READ,
      ("Unable to create capturebuffer"), (NULL));
    return FALSE;
  }
dodgy_width:
  {
    GST_ELEMENT_ERROR (dsoundsrc, RESOURCE, OPEN_READ,
      ("Unexpected width %d", spec->width), (NULL));
    return FALSE;
  }

}

static gboolean
gst_directsound_src_unprepare (GstAudioSrc * asrc)
{
  GstDirectSoundSrc *dsoundsrc;

  HRESULT hRes;                 /* Result for windows functions */

  /* Resets */
  GST_DEBUG ("unpreparing directsoundsrc");

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);
  if( dsoundsrc->pDSBSecondary )
  {
    /* Stop capturing */
    hRes = IDirectSoundCaptureBuffer_Stop (dsoundsrc->pDSBSecondary);

    /* Release buffer  */
    hRes = IDirectSoundCaptureBuffer_Release (dsoundsrc->pDSBSecondary);
    dsoundsrc->pDSBSecondary = NULL;
  }
  return TRUE;

}
static gboolean
gst_directsound_src_restart( GstDirectSoundSrc *dsoundsrc, int time_in_us)
{
  DSCBUFFERDESC descSecondary;
  HRESULT hRes = DS_OK;
  //    GstClockTime base_time;
  //    GstClockTime stream_time;
  //    GstClockTime now_time;
  //    GstClock* clock;
  gboolean dowait =FALSE;
  GUID deviceId = GUID_NULL;

  GST_DSOUND_CHANGE_LOCK(dsoundsrc);

  dsoundsrc->last_dummy_time = timeGetTime();
  if( dsoundsrc->pDSBSecondary )
  {
    hRes = IDirectSoundCaptureBuffer_Stop (dsoundsrc->pDSBSecondary);
    hRes = IDirectSoundCaptureBuffer_Release (dsoundsrc->pDSBSecondary);
    dsoundsrc->pDSBSecondary = NULL;
  }

  if( dsoundsrc->pDSC)
  {
    hRes = IDirectSoundCapture_Release (dsoundsrc->pDSC);
    dsoundsrc->pDSC = NULL;
  }

  dsoundsrc->invalid = FALSE;


  GST_DSOUND_CHANGE_UNLOCK(dsoundsrc);  

  //stream_time = dsoundsrc->stream_time;
  //
  //if( GST_CLOCK_TIME_IS_VALID(stream_time))
  //{
  //    base_time = GST_ELEMENT_CAST (dsoundsrc)->base_time;
  //    clock = GST_ELEMENT_CAST (dsoundsrc)->clock;
  //    stream_time += base_time;
  //    if( clock )
  //    {
  //        now_time = gst_clock_get_time(clock);
  //        gst_clock_id_wait (gst_clock_new_single_shot_id (clock, stream_time),NULL);
  //        dowait = TRUE;
  //    }
  //}

  dsoundsrc->try_to_start_count ++;

  // if( !dowait )
  {
    if( time_in_us > 0 )
    {
      guint32 util = dsoundsrc->try_to_start_time + time_in_us * dsoundsrc->try_to_start_count;
      if( dsoundsrc->last_dummy_time < util)
      {
        guint32 towait = util-dsoundsrc->last_dummy_time;
        if( towait < G_MAXINT32 )
          Sleep(towait);
      }
    }
    else
      Sleep(dsoundsrc->latency_time/1000);
  }

  GST_DSOUND_CHANGE_LOCK(dsoundsrc);

  //if( IsEqualGUID(&dsoundsrc->deviceId ,&GUID_NULL))
  {
    if(dsoundsrc->device_name != NULL)
    {
      if( dsoundsrc->pDirectSoundCaptureEnumerate )
      {
        void* param[] ={dsoundsrc->device_name_locale, &deviceId, 0};

        (dsoundsrc->pDirectSoundCaptureEnumerate)(DSEnumProc, param);
      }

      if(! IsEqualGUID(&deviceId ,&GUID_NULL))
        dsoundsrc->deviceId = deviceId;
    }
  }

  /* Create capture object */
  hRes = (dsoundsrc->pDSoundCaptureCreate) (IsEqualGUID(&dsoundsrc->deviceId ,&GUID_NULL) 
    ? NULL: &dsoundsrc->deviceId, &dsoundsrc->pDSC, NULL);

  if (FAILED (hRes)) {

    GST_DSOUND_CHANGE_UNLOCK(dsoundsrc);
    return FALSE;
  }


  /* Init secondary buffer desciption */
  memset (&descSecondary, 0, sizeof (DSCBUFFERDESC));
  descSecondary.dwSize = sizeof (DSCBUFFERDESC);
  descSecondary.dwFlags = 0;
  descSecondary.dwReserved = 0;

  /* This is not primary buffer so have to set size  */
  descSecondary.dwBufferBytes = dsoundsrc->buffer_size;
  descSecondary.lpwfxFormat = &dsoundsrc->wfx;

  /* Create buffer */
  hRes = IDirectSoundCapture_CreateCaptureBuffer (dsoundsrc->pDSC,
    &descSecondary, &dsoundsrc->pDSBSecondary, NULL);
  GST_DSOUND_CHANGE_UNLOCK(dsoundsrc);

  if (hRes != DS_OK) {
    return FALSE;
  }
  return TRUE;
}

static gboolean 
gst_directsound_src_is_invalid_device(GstDirectSoundSrc* dsoundsrc, gpointer data, guint length)
{
    gint dur;
    guint towait;
    guint now;
    if( dsoundsrc->invalid == TRUE)
    {
        dur =  (dsoundsrc->bytes_per_sample > 0 && dsoundsrc->wfx.nSamplesPerSec > 0 ) ?
            gst_util_uint64_scale_int(length/dsoundsrc->bytes_per_sample, GST_USECOND, dsoundsrc->wfx.nSamplesPerSec )
            : 24;

        dsoundsrc->invalid_dur += dur;

        now = timeGetTime();
        towait = dsoundsrc->invalid_time + dsoundsrc->invalid_dur;
        if (towait > now )
            Sleep(MAX(towait - now, 5000));

        memset(data, 0, length);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static void
gst_directsound_src_invalid_device(GstDirectSoundSrc* dsoundsrc, gpointer data, guint length)
{
    dsoundsrc->invalid_time = timeGetTime();
    dsoundsrc->invalid_dur = (dsoundsrc->bytes_per_sample > 0 && dsoundsrc->wfx.nSamplesPerSec > 0 ) ?
        gst_util_uint64_scale_int(length/dsoundsrc->bytes_per_sample, GST_USECOND, dsoundsrc->wfx.nSamplesPerSec )
        : 24;
    dsoundsrc->invalid = TRUE;
    memset(data, 0, length);    

}


/* 
return number of readed bytes */
static guint
gst_directsound_src_read (GstAudioSrc * asrc, gpointer data, guint length)
{
  GstDirectSoundSrc *dsoundsrc;
  int zero_count = 0;

  HRESULT hRes;                 /* Result for windows functions */
  DWORD dwCurrentCaptureCursor = 0;
  DWORD dwBufferSize = 0;

  LPVOID pLockedBuffer1 = NULL;
  LPVOID pLockedBuffer2 = NULL;
  DWORD dwSizeBuffer1 = 0;
  DWORD dwSizeBuffer2 = 0;

  DWORD dwStatus = 0;

  GST_DEBUG ("reading directsoundsrc\n");

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);

  GST_DSOUND_LOCK (dsoundsrc);

  if( dsoundsrc->need_restart || g_atomic_int_compare_and_exchange (&dsoundsrc->device_changed, 1, 0))
  {
    dsoundsrc->need_restart = TRUE;

    if(dsoundsrc->try_to_start_time == 0 )
    {
      dsoundsrc->try_to_start_time = timeGetTime();
      dsoundsrc->try_to_start_count = 0;
    }

    if( gst_directsound_src_restart(dsoundsrc,
      (dsoundsrc->bytes_per_sample > 0 && dsoundsrc->wfx.nSamplesPerSec > 0 ) ?
      gst_util_uint64_scale_int(length/dsoundsrc->bytes_per_sample, GST_USECOND, dsoundsrc->wfx.nSamplesPerSec )
      : 24) )
    {
      dsoundsrc->need_restart = FALSE;
      dsoundsrc->try_to_start_time = 0;
      dsoundsrc->try_to_start_count = 0;
    }
    else
    {
      GST_DSOUND_UNLOCK (dsoundsrc);
      return -1;
    }
  }

  if (gst_directsound_src_is_invalid_device(dsoundsrc, data, length))
  {
      GST_DSOUND_UNLOCK (dsoundsrc);
      return length;
  }

  /* Get current buffer status */
  hRes = IDirectSoundCaptureBuffer_GetStatus (dsoundsrc->pDSBSecondary,
    &dwStatus);

  if( hRes != DS_OK )
  {
    dsoundsrc->need_restart = TRUE;
    GST_DSOUND_UNLOCK (dsoundsrc);
    return -1;
  }

  /* Starting capturing if not allready */
  if (!(dwStatus & DSCBSTATUS_CAPTURING)) {
    hRes = IDirectSoundCaptureBuffer_Start (dsoundsrc->pDSBSecondary,
      DSCBSTART_LOOPING);
    //    Sleep (dsoundsrc->latency_time/1000);
    GST_DEBUG ("capture started");

    if( hRes != DS_OK)
    {
      //if( hRes == DSERR_NODRIVER)
      {
        dsoundsrc->need_restart = TRUE;
        GST_DSOUND_UNLOCK (dsoundsrc);
        return -1;
      }
     /* GST_DSOUND_UNLOCK (dsoundsrc);

      return 0;*/
    }
  }
  
  //  calculate_buffersize:
  while (length > dwBufferSize) {
    Sleep (dsoundsrc->latency_time / 1000);

    hRes =
      IDirectSoundCaptureBuffer_GetCurrentPosition (dsoundsrc->pDSBSecondary,
      &dwCurrentCaptureCursor, NULL);

    if( hRes != DS_OK)
    {
      //if( hRes == DSERR_NODRIVER)
      {
        dsoundsrc->need_restart = TRUE;
        GST_DSOUND_UNLOCK (dsoundsrc);
        return -1;
      }
      //GST_DSOUND_UNLOCK (dsoundsrc);

      //return 0;
    }
    //DSERR_INVALIDPARAM
    //DSERR_NODRIVER
    //DSERR_OUTOFMEMORY

    // 在虚拟机上无法采集音频，如果连续监测采集不到数据，认为设备不可用
    if  (dwCurrentCaptureCursor == 0 )
        zero_count ++;
    else
        zero_count = 0;

    if (zero_count > 15) 
    {
        gst_directsound_src_invalid_device(dsoundsrc, data, length);
        GST_DSOUND_UNLOCK (dsoundsrc);
        return length;
    }

    /* calculate the buffer */
    if (dwCurrentCaptureCursor < dsoundsrc->current_circular_offset) {
      dwBufferSize = dsoundsrc->buffer_size -
        (dsoundsrc->current_circular_offset - dwCurrentCaptureCursor);
    } else {
      dwBufferSize =
        dwCurrentCaptureCursor - dsoundsrc->current_circular_offset;
    }
  }                             // while (...

  /* Lock the buffer */
  hRes = IDirectSoundCaptureBuffer_Lock (dsoundsrc->pDSBSecondary,
    dsoundsrc->current_circular_offset,
    length,
    &pLockedBuffer1, &dwSizeBuffer1, &pLockedBuffer2, &dwSizeBuffer2, 0L);

  /* Copy buffer data to another buffer */
  if (hRes == DS_OK) {
    memcpy (data, pLockedBuffer1, dwSizeBuffer1);
  }

  /* ...and if something is in another buffer */
  if (pLockedBuffer2 != NULL) {
    memcpy (((guchar *) data + dwSizeBuffer1), pLockedBuffer2, dwSizeBuffer2);
  }

  dsoundsrc->current_circular_offset += dwSizeBuffer1 + dwSizeBuffer2;
  dsoundsrc->current_circular_offset %= dsoundsrc->buffer_size;

  IDirectSoundCaptureBuffer_Unlock (dsoundsrc->pDSBSecondary,
    pLockedBuffer1, dwSizeBuffer1, pLockedBuffer2, dwSizeBuffer2);

  GST_DSOUND_UNLOCK (dsoundsrc);

  /* return length (readed data size in bytes) */
  return length;

}

static guint
gst_directsound_src_delay (GstAudioSrc * asrc)
{
  GstDirectSoundSrc *dsoundsrc;
  HRESULT hRes;
  DWORD dwCurrentCaptureCursor;
  DWORD dwBytesInQueue = 0;
  gint nNbSamplesInQueue = 0;

  GST_DEBUG ("Delay\n");

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);

  GST_DSOUND_CHANGE_LOCK (dsoundsrc);

  if( dsoundsrc->pDSBSecondary )
  {
    /* evaluate the number of samples in queue in the circular buffer */
    hRes =
      IDirectSoundCaptureBuffer_GetCurrentPosition (dsoundsrc->pDSBSecondary,
      &dwCurrentCaptureCursor, NULL);
    /* FIXME: Check is this calculated right */
    if (hRes == S_OK) {
      if (dwCurrentCaptureCursor < dsoundsrc->current_circular_offset) {
        dwBytesInQueue =
          dsoundsrc->buffer_size - (dsoundsrc->current_circular_offset -
          dwCurrentCaptureCursor);
      } else {
        dwBytesInQueue =
          dwCurrentCaptureCursor - dsoundsrc->current_circular_offset;
      }

      nNbSamplesInQueue = dwBytesInQueue / dsoundsrc->bytes_per_sample;
    }
  }
  else
  {
    DWORD time = timeGetTime();

    nNbSamplesInQueue =  MIN( time - dsoundsrc->last_dummy_time, dsoundsrc->buffer_time /1000 ) *  GST_BASE_AUDIO_SRC(dsoundsrc)->ringbuffer->spec.rate / 1000;
  }
  GST_DSOUND_CHANGE_UNLOCK (dsoundsrc);
  return nNbSamplesInQueue;
}

static void
gst_directsound_src_reset (GstAudioSrc * asrc)
{
  GstDirectSoundSrc *dsoundsrc;
  LPVOID pLockedBuffer = NULL;
  DWORD dwSizeBuffer = 0;

  GST_DEBUG ("reset directsoundsrc\n");

  dsoundsrc = GST_DIRECTSOUND_SRC (asrc);

#if 0
  IDirectSoundCaptureBuffer_Stop (dsoundsrc->pDSBSecondary);
#endif

  GST_DSOUND_LOCK (dsoundsrc);

  if (dsoundsrc->pDSBSecondary) {
    /*stop capturing */
    HRESULT hRes = IDirectSoundCaptureBuffer_Stop (dsoundsrc->pDSBSecondary);

    /*reset position */
    /*    hRes = IDirectSoundCaptureBuffer_SetCurrentPosition (dsoundsrc->pDSBSecondary, 0); */

    /*reset the buffer */
    hRes = IDirectSoundCaptureBuffer_Lock (dsoundsrc->pDSBSecondary,
      dsoundsrc->current_circular_offset, dsoundsrc->buffer_size,
      pLockedBuffer, &dwSizeBuffer, NULL, NULL, 0L);

    if (SUCCEEDED (hRes)) {
      memset (pLockedBuffer, 0, dwSizeBuffer);

      hRes =
        IDirectSoundCaptureBuffer_Unlock (dsoundsrc->pDSBSecondary,
        pLockedBuffer, dwSizeBuffer, NULL, 0);
    }
    dsoundsrc->current_circular_offset = 0;

  }
  dsoundsrc->invalid = FALSE;

  GST_DSOUND_UNLOCK (dsoundsrc);
}
