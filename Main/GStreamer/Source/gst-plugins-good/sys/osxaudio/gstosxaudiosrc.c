/*
 * GStreamer
 * Copyright (C) 2005,2006 Zaheer Abbas Merali <zaheerabbas at merali dot org>
 * Copyright (C) 2008 Pioneers of the Inevitable <songbird@songbirdnest.com>
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
 * which case the following provisions apply instead of the ones66666
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

/**
 * SECTION:element-osxaudiosrc
 *
 * This element captures raw audio samples using the CoreAudio api.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch osxaudiosrc ! wavenc ! filesink location=audio.wav
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreAudio/AudioHardware.h>
#include <gst/interfaces/propertyprobe.h>
#include "gstosxaudiosrc.h"
#include "gstosxaudioelement.h"

GST_DEBUG_CATEGORY_STATIC (osx_audiosrc_debug);
#define GST_CAT_DEFAULT osx_audiosrc_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  SIGNAL_SYSTEM_DEVICE_CHANGED,
  LAST_SIGNAL
  
};

static guint osx_signals[LAST_SIGNAL];

enum
{
  ARG_0,
  ARG_DEVICE,
  ARG_DEVICE_NAME,
  ARG_MONITOR_DEVICE,
};

#define DEFAULT_DEVICE_NAME "DefaultCapture"

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-float, "
        "endianness = (int) {" G_STRINGIFY (G_BYTE_ORDER) " }, "
        "signed = (boolean) { TRUE }, "
        "width = (int) 32, "
        "depth = (int) 32, "
        "rate = (int) [1, MAX], " "channels = (int) [1, MAX]")
    );

typedef struct _GstOsxAudioSrcDevice GstOsxAudioSrcDevice;

struct _GstOsxAudioSrcDevice
{
  AudioDeviceID device_id;
  gchar* device_name;
};

static void gst_osx_audio_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_osx_audio_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_osx_audio_src_get_caps (GstBaseSrc * src);

static GstRingBuffer *gst_osx_audio_src_create_ringbuffer (GstBaseAudioSrc *
    src);
static void gst_osx_audio_src_osxelement_init (gpointer g_iface,
    gpointer iface_data);
static OSStatus gst_osx_audio_src_io_proc (GstOsxRingBuffer * buf,
    AudioUnitRenderActionFlags * ioActionFlags,
    const AudioTimeStamp * inTimeStamp, UInt32 inBusNumber,
    UInt32 inNumberFrames, AudioBufferList * bufferList);
static void gst_osx_audio_src_select_device (GstOsxAudioSrc * osxsrc);
static void
gst_osx_audio_src_finalize(GObject* obj);

static void
gst_osx_audio_src_control_monitor_device (GstOsxAudioSrc * osxsrc, gboolean mon);

static GstStateChangeReturn gst_osx_audio_src_change_state (GstElement * element,
                                                     GstStateChange transition);

static GList *
gst_osx_audio_src_update_devices(GstOsxAudioSrc * osxsrc);


static const GList *
gst_osx_property_probe_get_properties (GstPropertyProbe * probe)
{
  GObjectClass *klass = G_OBJECT_GET_CLASS (probe);
  static GList *props = NULL;
  
  if (!props) {
    GParamSpec *pspec;
    
    pspec = g_object_class_find_property (klass, "device-name");
    props = g_list_prepend (NULL, pspec);
    
  }
  
  return props;
}


static void
gst_osx_property_probe_probe_property (GstPropertyProbe * probe,
                                        guint prop_id, const GParamSpec * pspec)
{
  if (!g_str_equal (pspec->name, "device-name")) {
    G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
  }
}

static gboolean
gst_osx_property_probe_needs_probe (GstPropertyProbe * probe,
                                     guint prop_id, const GParamSpec * pspec)
{
  /* don't cache probed data */
  return TRUE;
}


static GValueArray *
gst_osx_property_probe_get_values (GstPropertyProbe * probe,
                                    guint prop_id, const GParamSpec * pspec)
{
  GstObject * obj;
  GstOsxAudioSrc * src;
  GList * devices, * l;
  GValueArray * array;
  
  array = NULL;
  
  if (!g_str_equal (pspec->name, "device-name")) {
    G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
    return array;
  }
  
  obj = GST_OBJECT (probe);
  
  GST_OBJECT_LOCK (obj);
  
  src = GST_OSX_AUDIO_SRC(obj);
  
  devices = gst_osx_audio_src_update_devices(src);
  
  if (devices == NULL) {
    GST_DEBUG_OBJECT (obj, "No devices found");
    goto done;
  }
  
  array = g_value_array_new (1);
  
  for (l = devices; l != NULL; l = l->next) {
    GValue val = { 0, };
    
    g_value_init (&val, G_TYPE_STRING);
    g_value_set_string (&val, ((GstOsxAudioSrcDevice *) l->data)->device_name);
    g_value_array_append (array, &val);
    g_value_unset (&val);
  }
  
done:
  GST_OBJECT_UNLOCK (obj);
  return array;
}


static void
gst_osx_property_probe_interface_init (GstPropertyProbeInterface * iface)
{
  iface->get_properties = gst_osx_property_probe_get_properties;
  iface->probe_property = gst_osx_property_probe_probe_property;
  iface->needs_probe = gst_osx_property_probe_needs_probe;
  iface->get_values = gst_osx_property_probe_get_values;
}


static void
gst_osx_audio_src_do_init (GType type)
{
  static const GInterfaceInfo osxelement_info = {
    gst_osx_audio_src_osxelement_init,
    NULL,
    NULL
  };

  
  static const GInterfaceInfo probe_iface_info = {
    (GInterfaceInitFunc) gst_osx_property_probe_interface_init,
    NULL,
    NULL,
  };
  
  GST_DEBUG_CATEGORY_INIT (osx_audiosrc_debug, "osxaudiosrc", 0,
                           "OSX Audio Src");
  GST_DEBUG ("Adding static interface");
  g_type_add_interface_static (type, GST_OSX_AUDIO_ELEMENT_TYPE,
                               &osxelement_info);
  
  g_type_add_interface_static (type, GST_TYPE_PROPERTY_PROBE,
                               &probe_iface_info);
}

GST_BOILERPLATE_FULL (GstOsxAudioSrc, gst_osx_audio_src, GstBaseAudioSrc,
    GST_TYPE_BASE_AUDIO_SRC, gst_osx_audio_src_do_init);

static void
gst_osx_audio_src_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class, &src_factory);

  gst_element_class_set_details_simple (element_class, "Audio Source (OSX)",
      "Source/Audio",
      "Input from a sound card in OS X",
      "Zaheer Abbas Merali <zaheerabbas at merali dot org>");
}

static void
gst_osx_audio_src_class_init (GstOsxAudioSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstBaseAudioSrcClass *gstbaseaudiosrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstbaseaudiosrc_class = (GstBaseAudioSrcClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_osx_audio_src_set_property;
  gobject_class->get_property = gst_osx_audio_src_get_property;
  gobject_class->finalize = gst_osx_audio_src_finalize;
  gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_osx_audio_src_change_state);
  

  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_osx_audio_src_get_caps);

  g_object_class_install_property (gobject_class, ARG_DEVICE,
      g_param_spec_int ("device", "Device ID", "Device ID of input device",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_property (gobject_class, ARG_MONITOR_DEVICE,
      g_param_spec_boolean("monitor-device", "Monitor Device", "Start/Stop Monitor Device",
                           false, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_property (gobject_class, ARG_DEVICE_NAME,
      g_param_spec_string("device-name", "Device Name", "Device Name of input device",
                          DEFAULT_DEVICE_NAME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  osx_signals[SIGNAL_SYSTEM_DEVICE_CHANGED] =
      g_signal_new("system-device-changed", G_TYPE_FROM_CLASS(gobject_class), G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GstOsxAudioSrcClass, sys_device_changed),
            NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);
  
  gstbaseaudiosrc_class->create_ringbuffer =
      GST_DEBUG_FUNCPTR (gst_osx_audio_src_create_ringbuffer);
  
  /*
   * Force an own run loop for callbacks
   */
  
  CFRunLoopRef theRunLoop = NULL;
  AudioObjectPropertyAddress pro = {
    kAudioHardwarePropertyRunLoop,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  
  AudioObjectSetPropertyData(kAudioObjectSystemObject, &pro, 0, NULL, sizeof(CFRunLoopRef), theRunLoop);
}

static void gst_osx_audio_src_free_devices(GstOsxAudioSrc * src){
  GList * l;
  GstOsxAudioSrcDevice* dev;
  if (src)
  {
    for (l = src->devices; l != NULL; l=l->next) {
      dev = (GstOsxAudioSrcDevice*)l->data;
      g_free(dev->device_name);
      g_free(dev);
    }
    g_list_free(src->devices);
    src->devices = NULL;
  }
}

static GList *
gst_osx_audio_src_update_devices(GstOsxAudioSrc * osxsrc){
 
  AudioObjectPropertyAddress propertyAddress = {
    kAudioHardwarePropertyDevices,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  
  guint32 dataSize = 0;
  OSStatus status;
  guint32 deviceCount;
  AudioDeviceID* audioDevices;
  CFStringRef deviceName = NULL;
  guint32 i;
  guint32 streamCount;
  char buf[1025];
  
  gst_osx_audio_src_free_devices(osxsrc);
  
  GstOsxAudioSrcDevice* dev = g_malloc(sizeof(GstOsxAudioSrcDevice));
  dev->device_name = g_strdup(DEFAULT_DEVICE_NAME);
  dev->device_id = kAudioDeviceUnknown;
  osxsrc->devices = g_list_append(osxsrc->devices, dev);
  
  
  status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize);
  if(kAudioHardwareNoError != status) {
    GST_ERROR_OBJECT(osxsrc, "AudioObjectGetPropertyDataSize (kAudioHardwarePropertyDevices) failed: %i\n", status);
    return osxsrc->devices;
  }
  
  deviceCount = (guint32)(dataSize / sizeof(AudioDeviceID));
  
  audioDevices =(AudioDeviceID *)(malloc(dataSize));
  if(NULL == audioDevices) {
    GST_ERROR_OBJECT(osxsrc, "Unable to allocate memory");
    return osxsrc->devices;
  }
  
  status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize, audioDevices);
  if(kAudioHardwareNoError != status) {
    GST_ERROR_OBJECT(osxsrc, "AudioObjectGetPropertyData (kAudioHardwarePropertyDevices) failed: %i\n", status);
    free(audioDevices), audioDevices = NULL;
    return osxsrc->devices;
  }
  
  // Iterate through all the devices and determine which are input-capable
  propertyAddress.mScope = kAudioDevicePropertyScopeInput;
  for(i = 0; i < deviceCount; ++i) {
    // Determine if the device is an input device (it is an input device if it has input channels)
    dataSize = 0;
    
    propertyAddress.mSelector   = kAudioDevicePropertyStreams;
    
    status             = AudioObjectGetPropertyDataSize(audioDevices[i],
                                                                 &propertyAddress,
                                                                 0,
                                                                 NULL,
                                                                 &dataSize);
    streamCount          = dataSize / sizeof(AudioStreamID);
    
    if (streamCount == 0)
    {
      continue;
    }
    
    // Query device name
    deviceName = NULL;
    dataSize = sizeof(deviceName);
    propertyAddress.mSelector = kAudioDevicePropertyDeviceNameCFString;
    status = AudioObjectGetPropertyData(audioDevices[i], &propertyAddress, 0, NULL, &dataSize, &deviceName);
    if(kAudioHardwareNoError != status) {
      GST_ERROR_OBJECT(osxsrc, "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceNameCFString) failed: %i\n", status);
      continue;
    }
    GstOsxAudioSrcDevice* dev = g_malloc(sizeof(GstOsxAudioSrcDevice));
      memset(buf, 0, 1025);
      CFStringGetCString(deviceName, buf, 1024, kCFStringEncodingUTF8);
    dev->device_name = g_strdup(buf);
    dev->device_id = audioDevices[i];
    osxsrc->devices = g_list_append(osxsrc->devices, dev);
    
    CFRelease(deviceName);
  }
  
  free(audioDevices), audioDevices = NULL;
  
 
  return osxsrc->devices;
}

static void
gst_osx_audio_src_finalize(GObject* obj)
{
  GstOsxAudioSrc * src = GST_OSX_AUDIO_SRC(obj);
  gst_osx_audio_src_free_devices(src);
  gst_osx_audio_src_control_monitor_device(src, false);
                                 
}

static gboolean
gst_osx_audio_src_reset_func(GstOsxAudioSrc * osxsrc)
{
    GST_OBJECT_LOCK (osxsrc);
    gst_osx_audio_src_update_devices(osxsrc);
    GST_OBJECT_UNLOCK (osxsrc);
    
    //gst_element_get_state (osxsrc, &old, NULL, GST_CLOCK_TIME_NONE);
    
    return FALSE;
}

static void
gst_osx_audio_src_reset(GstOsxAudioSrc * osxsrc){
  g_idle_add((GSourceFunc)(gst_osx_audio_src_reset_func), osxsrc);
}

static OSStatus
gst_osx_audio_src_devices_changed(AudioObjectID inObjectID, UInt32 inNumberAddresses,
                                  const AudioObjectPropertyAddress inAddresses[], void* inData)
{
  GstOsxAudioSrc * osxsrc = (GstOsxAudioSrc*)(inData);
  if (!osxsrc){
    return -1;
  }
  g_signal_emit(G_OBJECT(osxsrc), osx_signals[SIGNAL_SYSTEM_DEVICE_CHANGED], 0);
  gst_osx_audio_src_reset(osxsrc);
  return 0;
}


static OSStatus
gst_osx_audio_src_default_device_changed(AudioObjectID inObjectID, UInt32 inNumberAddresses,
                                  const AudioObjectPropertyAddress inAddresses[], void* inData)
{
  GstOsxAudioSrc * osxsrc = (GstOsxAudioSrc*)(inData);
  if (!osxsrc) {
    return -1;
  }
  
  if (osxsrc->is_default_device)
  {
    gst_osx_audio_src_reset(osxsrc);
  }
  return 0;
}

static void
gst_osx_audio_src_control_monitor_device (GstOsxAudioSrc * osxsrc, gboolean mon)
{
  
  if (osxsrc->monitor_device == mon)
    return ;
  
  AudioObjectPropertyAddress audioDevicesAddress = {
    kAudioHardwarePropertyDevices,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  
  AudioObjectPropertyAddress defaultDeviceAddress =
  {
    kAudioHardwarePropertyDefaultInputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  
  if ( mon){
    AudioObjectAddPropertyListener(kAudioObjectSystemObject, &audioDevicesAddress, gst_osx_audio_src_devices_changed, osxsrc);
    AudioObjectAddPropertyListener(kAudioObjectSystemObject, &defaultDeviceAddress, gst_osx_audio_src_default_device_changed, osxsrc);
  }else{
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &audioDevicesAddress, gst_osx_audio_src_devices_changed, osxsrc);
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &defaultDeviceAddress, gst_osx_audio_src_default_device_changed, osxsrc);
  }
  
  
  osxsrc->monitor_device = mon;
  
}


static GstStateChangeReturn
gst_osx_audio_src_change_state (GstElement * element,
                                GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstOsxAudioSrc * osxsrc;

  osxsrc = GST_OSX_AUDIO_SRC(element);
  
  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      gst_osx_audio_src_control_monitor_device(osxsrc, true);
      break;
    default:
      break;
  }
  
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  
  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_osx_audio_src_control_monitor_device(osxsrc, false);
      break;
    default:
      break;
  }
    if (ret == GST_STATE_CHANGE_FAILURE && transition == GST_STATE_CHANGE_NULL_TO_READY)
        gst_osx_audio_src_control_monitor_device(osxsrc, false);
  
  return ret;
}

static void
gst_osx_audio_src_set_device_name ( GstOsxAudioSrc * osxsrc, const gchar* name)
{
  GList * l;
  GstOsxAudioSrcDevice * dev;
//  gboolean find;
 // gint i;
    GstOsxRingBuffer* ringbuf;
  GST_OBJECT_LOCK(osxsrc);
  
//  find = false;
  
//#  for (i = 0; i < 2; i ++) {
//    if (osxsrc->devices == NULL || i > 0) {
    //force update device list
    gst_osx_audio_src_update_devices(osxsrc);
//    }
    osxsrc->device_id = kAudioDeviceUnknown;
    for (l=osxsrc->devices; l!=NULL; l=l->next) {
      dev = (GstOsxAudioSrcDevice*)l->data;
      if (g_str_equal(name, dev->device_name)){
        osxsrc->device_id = dev->device_id;
//        find = true;
        break;
      }
    }
//    if (find) {
//      break;
//    }
//  }
//  if (!find) {
//    osxsrc->device_id = kAudioDeviceUnknown;
//  }

  osxsrc->is_default_device = osxsrc->device_id == kAudioDeviceUnknown;
    
    if (osxsrc->is_default_device) {
        gst_osx_audio_src_select_device(osxsrc);
    }
   if ( osxsrc->device_id != kAudioDeviceUnknown )
   {
     ringbuf = GST_OSX_RING_BUFFER(osxsrc->src.ringbuffer);
    if (ringbuf) {
        ringbuf->device_id = osxsrc->device_id;
    }
   }
   
  GST_OBJECT_UNLOCK(osxsrc);
}

static gchar *
gst_osx_audio_src_get_device_name ( GstOsxAudioSrc * osxsrc, AudioDeviceID id)
{
  GList * l;
  GstOsxAudioSrcDevice * dev;
  gchar* sret;
  gboolean find;
  gint i;
  sret = NULL;
  GST_OBJECT_LOCK(osxsrc);
  find = false;
  for (i = 0; i < 2 ; i++) {
    if (osxsrc->devices == NULL || i > 0) {
      gst_osx_audio_src_update_devices(osxsrc);
    }
    
    for (l=osxsrc->devices; l!=NULL; l=l->next) {
      dev = (GstOsxAudioSrcDevice*)l->data;
      if (id == dev->device_id){
        sret = dev->device_name;
        find = true;
        break;
      }
    }
    if (find) {
      break;
    }
  }
  GST_OBJECT_UNLOCK(osxsrc);
  return sret;
}

static void
gst_osx_audio_src_init (GstOsxAudioSrc * src, GstOsxAudioSrcClass * gclass)
{
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  src->device_id = kAudioDeviceUnknown;
  src->deviceChannels = -1;
  src->monitor_device = false;
  src->is_default_device = true;
  src->devices = NULL;
    src->error = 0;
}

static void
gst_osx_audio_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  gboolean mon;
  GstOsxAudioSrc *src = GST_OSX_AUDIO_SRC (object);

  switch (prop_id) {
    case ARG_DEVICE:
      src->device_id = g_value_get_int (value);
      src->is_default_device = src->device_id == kAudioDeviceUnknown;
          if (src->is_default_device) {
              gst_osx_audio_src_select_device(src);
          }
          if ( src->device_id != kAudioDeviceUnknown )
          {
             GstOsxRingBuffer* ringbuf = GST_OSX_RING_BUFFER(src->src.ringbuffer);
              if (ringbuf) {
                  ringbuf->device_id = src->device_id;
              }
          }

      break;
    case ARG_MONITOR_DEVICE:
      mon = g_value_get_boolean(value);
      gst_osx_audio_src_control_monitor_device(src, mon);
      break;
    case ARG_DEVICE_NAME:
      gst_osx_audio_src_set_device_name(src, g_value_get_string(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_osx_audio_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOsxAudioSrc *src = GST_OSX_AUDIO_SRC (object);

  switch (prop_id) {
    case ARG_DEVICE:
      g_value_set_int (value, src->device_id);
      break;
    case ARG_DEVICE_NAME:
      g_value_set_string(value, gst_osx_audio_src_get_device_name(src, src->device_id));
      break;
    case ARG_MONITOR_DEVICE:
      g_value_set_boolean(value, src->monitor_device);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_osx_audio_src_get_caps (GstBaseSrc * src)
{
  GstElementClass *gstelement_class;
  GstOsxAudioSrc *osxsrc;
  GstPadTemplate *pad_template;
  GstCaps *caps;
  gint min, max;

  gstelement_class = GST_ELEMENT_GET_CLASS (src);
  osxsrc = GST_OSX_AUDIO_SRC (src);

  if (osxsrc->deviceChannels == -1) {
    /* -1 means we don't know the number of channels yet.  for now, return
     * template caps.
     */
    return NULL;
  }

  max = osxsrc->deviceChannels;
  if (max < 1)
    max = 1;                    /* 0 channels means 1 channel? */

  min = MIN (1, max);

  pad_template = gst_element_class_get_pad_template (gstelement_class, "src");
  g_return_val_if_fail (pad_template != NULL, NULL);

  caps = gst_caps_copy (gst_pad_template_get_caps (pad_template));

  if (min == max) {
    gst_caps_set_simple (caps, "channels", G_TYPE_INT, max, NULL);
  } else {
    gst_caps_set_simple (caps, "channels", GST_TYPE_INT_RANGE, min, max, NULL);
  }

  return caps;
}

static GstRingBuffer *
gst_osx_audio_src_create_ringbuffer (GstBaseAudioSrc * src)
{
  GstOsxAudioSrc *osxsrc;
  GstOsxRingBuffer *ringbuffer;

  osxsrc = GST_OSX_AUDIO_SRC (src);

  gst_osx_audio_src_select_device (osxsrc);

  GST_DEBUG ("Creating ringbuffer");
  ringbuffer = g_object_new (GST_TYPE_OSX_RING_BUFFER, NULL);
  GST_DEBUG ("osx src 0x%p element 0x%p  ioproc 0x%p", osxsrc,
      GST_OSX_AUDIO_ELEMENT_GET_INTERFACE (osxsrc),
      (void *) gst_osx_audio_src_io_proc);

  ringbuffer->element = GST_OSX_AUDIO_ELEMENT_GET_INTERFACE (osxsrc);
    ringbuffer->gstelement = GST_ELEMENT(osxsrc);
  ringbuffer->is_src = TRUE;
  ringbuffer->device_id = osxsrc->device_id;

  return GST_RING_BUFFER (ringbuffer);
}

static OSStatus
gst_osx_audio_src_io_proc (GstOsxRingBuffer * buf,
    AudioUnitRenderActionFlags * ioActionFlags,
    const AudioTimeStamp * inTimeStamp,
    UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList * bufferList)
{
    GstOsxAudioSrc *src;
  OSStatus status;
  guint8 *writeptr;
  gint writeseg;
  gint len;
  gint remaining;
  gint offset = 0;
    
    src = GST_OSX_AUDIO_SRC(buf->gstelement);

  status = AudioUnitRender (buf->audiounit, ioActionFlags, inTimeStamp,
      inBusNumber, inNumberFrames, buf->recBufferList);

  if (status) {
      src->error ++;
      
    GST_WARNING_OBJECT (buf, "AudioUnitRender returned %d", (int) status);
    
      if( src->error > 5)
          GST_ELEMENT_ERROR(buf->gstelement,  RESOURCE, READ, (NULL),
                        ("DEVICE  Auido Unit Render Failed with %d", (int)status));
    return status;
  }
    src->error = 0;

  remaining = buf->recBufferList->mBuffers[0].mDataByteSize;

  while (remaining) {
    if (!gst_ring_buffer_prepare_read (GST_RING_BUFFER (buf),
            &writeseg, &writeptr, &len))
      return 0;

    len -= buf->segoffset;

    if (len > remaining)
      len = remaining;

    memcpy (writeptr + buf->segoffset,
        (char *) buf->recBufferList->mBuffers[0].mData + offset, len);

    buf->segoffset += len;
    offset += len;
    remaining -= len;

    if ((gint) buf->segoffset == GST_RING_BUFFER (buf)->spec.segsize) {
      /* we wrote one segment */
      gst_ring_buffer_advance (GST_RING_BUFFER (buf), 1);

      buf->segoffset = 0;
    }
  }
  return 0;
}

static void
gst_osx_audio_src_osxelement_init (gpointer g_iface, gpointer iface_data)
{
  GstOsxAudioElementInterface *iface = (GstOsxAudioElementInterface *) g_iface;

  iface->io_proc = (AURenderCallback) gst_osx_audio_src_io_proc;
}

static void
gst_osx_audio_src_select_device (GstOsxAudioSrc * osxsrc)
{
  OSStatus status;
  UInt32 propertySize;

  if (osxsrc->device_id == kAudioDeviceUnknown) {
    /* If no specific device has been selected by the user, then pick the
     * default device */
    GST_DEBUG_OBJECT (osxsrc, "Selecting device for OSXAudioSrc");
    propertySize = sizeof (osxsrc->device_id);
    status = AudioHardwareGetProperty (kAudioHardwarePropertyDefaultInputDevice,
        &propertySize, &osxsrc->device_id);

    if (status) {
      GST_WARNING_OBJECT (osxsrc,
          "AudioHardwareGetProperty returned %d", (int) status);
    } else {
      GST_DEBUG_OBJECT (osxsrc, "AudioHardwareGetProperty returned 0");
    }

    if (osxsrc->device_id == kAudioDeviceUnknown) {
      GST_WARNING_OBJECT (osxsrc,
          "AudioHardwareGetProperty: device_id is kAudioDeviceUnknown");
    }

    GST_DEBUG_OBJECT (osxsrc, "AudioHardwareGetProperty: device_id is %lu",
        (long) osxsrc->device_id);
  }
}
