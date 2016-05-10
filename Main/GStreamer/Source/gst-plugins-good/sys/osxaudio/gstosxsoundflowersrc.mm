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
#include "gstosxsoundflowersrc.h"
#include "gstosxaudioelement.h"
#include "SoundflowerController.h"



static SoundflowerController* theCtrl = NULL;
static volatile gint32 theCtrlRef = 0;
static GList* theClients = NULL;
static GOnce once = G_ONCE_INIT;
static GMutex lock;

static gpointer ctrl_ref() {
    if(!theCtrl) {
        theCtrl = [[SoundflowerController alloc] init];
        if( ![theCtrl isOk]) {
            [theCtrl release];
            theCtrl = NULL;
        }
        theCtrlRef = 0;
    }
    if( theCtrl)
        theCtrlRef ++;
    return theCtrl;
}

static void ctrl_unref(gpointer* pt) {
    if( pt && *pt && *pt == theCtrl) {
        theCtrlRef --;
        *pt = NULL;
        if( theCtrlRef <= 0) {
            [theCtrl release];
            theCtrl = NULL;
        }
    }
    
    
}

static gpointer init_ctrl(gpointer data) {
    g_mutex_init(&lock);
}

static void lock_ctrl() {
    g_once(&once,  init_ctrl, NULL);
    g_mutex_lock(&lock);
}

static void unlock_ctrl() {
    g_mutex_unlock(&lock);
}

GST_DEBUG_CATEGORY_STATIC (osx_soundflowersrc_debug);
#define GST_CAT_DEFAULT osx_soundflowersrc_debug

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
                                                                   GST_PAD_SRC,
                                                                   GST_PAD_ALWAYS,
                                                                   GST_STATIC_CAPS ("audio/x-raw-float, "
                                                                                    "endianness = (int) {" G_STRINGIFY (G_BYTE_ORDER) " }, "
                                                                                    "signed = (boolean) { TRUE }, "
                                                                                    "width = (int) 32, "
                                                                                    "depth = (int) 32, "
                                                                                    "rate = (int) {44100, 48000, 88200, 96000, 176400, 192000}, " "channels = 2")
                                                                   );

static gboolean gst_osx_soundflower_src_open (GstAudioSrc * asrc);
static gboolean gst_osx_soundflower_src_close (GstAudioSrc * asrc);
static gboolean gst_osx_soundflower_src_prepare (GstAudioSrc * asrc,
                                                 GstRingBufferSpec * spec);
static gboolean gst_osx_soundflower_src_unprepare (GstAudioSrc * asrc);
static void gst_osx_soundflower_src_reset (GstAudioSrc * asrc);
static guint gst_osx_soundflower_src_delay (GstAudioSrc * asrc);
static guint gst_osx_soundflower_src_read (GstAudioSrc * asrc,
                                           gpointer data, guint length);

static GstStateChangeReturn gst_osx_soundflower_src_change_state (GstElement * element,
                                                                  GstStateChange transition);

static gboolean
gst_osx_soundflower_src_register(GstOsxSoundflowerSrc* osxsrc);

static gboolean
gst_osx_soundflower_src_unregister(GstOsxSoundflowerSrc* osxsrc);

static gboolean
gst_osx_soundflower_src_acquire_ctrl(GstOsxSoundflowerSrc* osxsrc);

static gboolean
gst_osx_soundflower_src_release_ctrl(GstOsxSoundflowerSrc* osxsrc);

static gboolean
gst_osx_soundflower_src_clean(GstOsxSoundflowerSrc* osxsrc);


static GstCaps *gst_osx_soundflower_src_get_caps (GstBaseSrc * src);


static void
gst_osx_soundflower_src_finalize(GObject* obj);

static void
gst_osx_soundflower_src_do_init (GType type)
{
    GST_DEBUG_CATEGORY_INIT (osx_soundflowersrc_debug, "osxsoundflowersrc", 0,
                             "OSX Soundflower Src");
    
}

GST_BOILERPLATE_FULL (GstOsxSoundflowerSrc, gst_osx_soundflower_src, GstAudioSrc,
                      GST_TYPE_AUDIO_SRC, gst_osx_soundflower_src_do_init);

static void
gst_osx_soundflower_src_base_init (gpointer g_class)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
    
    gst_element_class_add_static_pad_template (element_class, &src_factory);
    
    gst_element_class_set_details_simple (element_class, "Audio Source (OSX Soundflower)",
                                          "Source/Audio",
                                          "Input from a Soundflower  in OS X",
                                          "wangzhao <wangzhao04 at baidu dot com>");
}

static void
gst_osx_soundflower_src_class_init (GstOsxSoundflowerSrcClass * klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    GstBaseSrcClass *gstbasesrc_class;
    GstAudioSrcClass *gstaudiosrc_class;
    
    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    gstbasesrc_class = (GstBaseSrcClass *) klass;
    gstaudiosrc_class = (GstAudioSrcClass *) klass;
    
    parent_class = (GstAudioSrcClass*)g_type_class_peek_parent (klass);
    
    gobject_class->finalize = gst_osx_soundflower_src_finalize;
    
    gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_osx_soundflower_src_get_caps);
    
    gstaudiosrc_class->open = GST_DEBUG_FUNCPTR (gst_osx_soundflower_src_open);
    gstaudiosrc_class->close = GST_DEBUG_FUNCPTR (gst_osx_soundflower_src_close);
    gstaudiosrc_class->read = GST_DEBUG_FUNCPTR (gst_osx_soundflower_src_read);
    gstaudiosrc_class->prepare = GST_DEBUG_FUNCPTR (gst_osx_soundflower_src_prepare);
    gstaudiosrc_class->unprepare =
    GST_DEBUG_FUNCPTR (gst_osx_soundflower_src_unprepare);
    gstaudiosrc_class->delay = GST_DEBUG_FUNCPTR (gst_osx_soundflower_src_delay);
    gstaudiosrc_class->reset = GST_DEBUG_FUNCPTR (gst_osx_soundflower_src_reset);
    gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_osx_soundflower_src_change_state);
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

static void
gst_osx_soundflower_src_finalize(GObject* obj)
{
    GstOsxSoundflowerSrc* osxsrc = GST_OSX_SOUNDFLOWER_SRC(obj);
    gst_osx_soundflower_src_unregister(osxsrc);
    gst_osx_soundflower_src_clean(osxsrc);
    gst_osx_soundflower_src_release_ctrl(osxsrc);
    g_mutex_clear(&osxsrc->sample_lock);
    g_cond_clear(&osxsrc->sample_cond);
}

static void
gst_osx_soundflower_src_init (GstOsxSoundflowerSrc * src, GstOsxSoundflowerSrcClass * gclass)
{
    gst_base_src_set_live (GST_BASE_SRC (src),  TRUE);
    gst_base_audio_src_set_provide_clock(GST_BASE_AUDIO_SRC(src), FALSE);
    src->ctrl = NULL;
    src->sample_list = NULL;
    src->register_id= NULL;
    src->next_time = GST_CLOCK_TIME_NONE;
    
    g_mutex_init(&src->sample_lock);
    g_cond_init(&src->sample_cond);
}

static GstCaps *
gst_osx_soundflower_src_get_caps (GstBaseSrc * src)
{
    GstElementClass *gstelement_class;
    GstOsxSoundflowerSrc *osxsrc;
    GstPadTemplate *pad_template;
    GstCaps *caps;
    SoundflowerController* ctrl;
    int rate = 0;
    
    gstelement_class = GST_ELEMENT_GET_CLASS (src);
    osxsrc = GST_OSX_SOUNDFLOWER_SRC (src);
    
    ctrl = (SoundflowerController*)osxsrc->ctrl;
    
    pad_template = gst_element_class_get_pad_template (gstelement_class, "src");
    g_return_val_if_fail (pad_template != NULL, NULL);
    
    caps = gst_caps_copy (gst_pad_template_get_caps (pad_template));
    if( ctrl) {
        [ctrl getSampleRate: &rate];
        if( rate) {
            gst_caps_set_simple(caps, "rate", G_TYPE_INT,  rate, NULL);
        }
    }
    return caps;
}

static gboolean gst_osx_soundflower_src_open (GstAudioSrc * asrc) {
    GstOsxSoundflowerSrc* osxsrc = GST_OSX_SOUNDFLOWER_SRC(asrc);
    gst_osx_soundflower_src_unregister(osxsrc);
    gst_osx_soundflower_src_release_ctrl(osxsrc);
    gst_osx_soundflower_src_clean(osxsrc);
    return gst_osx_soundflower_src_acquire_ctrl(osxsrc);
}

static gboolean gst_osx_soundflower_src_close (GstAudioSrc * asrc) {
    GstOsxSoundflowerSrc* osxsrc = GST_OSX_SOUNDFLOWER_SRC(asrc);
    gst_osx_soundflower_src_unregister(osxsrc);
    gst_osx_soundflower_src_release_ctrl(osxsrc);
    gst_osx_soundflower_src_clean(osxsrc);
    return TRUE;
}

static gboolean gst_osx_soundflower_src_prepare (GstAudioSrc * asrc,
                                                 GstRingBufferSpec * spec) {
    GstOsxSoundflowerSrc* osxsrc = GST_OSX_SOUNDFLOWER_SRC(asrc);
    gst_osx_soundflower_src_unregister(osxsrc);
    gst_osx_soundflower_src_clean(osxsrc);
    return gst_osx_soundflower_src_register(osxsrc);
}

static gboolean gst_osx_soundflower_src_unprepare (GstAudioSrc * asrc) {
    GstOsxSoundflowerSrc* osxsrc = GST_OSX_SOUNDFLOWER_SRC(asrc);
    gst_osx_soundflower_src_unregister(osxsrc);
    gst_osx_soundflower_src_clean(osxsrc);
    return TRUE;
}

static void gst_osx_soundflower_src_reset (GstAudioSrc * asrc) {
    GstOsxSoundflowerSrc* osxsrc = GST_OSX_SOUNDFLOWER_SRC(asrc);
    gst_osx_soundflower_src_unregister(osxsrc);
    gst_osx_soundflower_src_clean(osxsrc);
    gst_osx_soundflower_src_register(osxsrc);
}

static guint gst_osx_soundflower_src_delay (GstAudioSrc * asrc) {
    return 0;
}

static guint gst_osx_soundflower_src_read (GstAudioSrc * asrc,
                                           gpointer data, guint length) {
    GstBuffer* buf;
    int len;
    GstOsxSoundflowerSrc* osxsrc = GST_OSX_SOUNDFLOWER_SRC(asrc);
    
    g_mutex_lock(&osxsrc->sample_lock);
    if( !osxsrc->sample_list ) {
        if(osxsrc->register_id ) {
            g_cond_wait(&osxsrc->sample_cond, &osxsrc->sample_lock);
        }
    }
    
    if(!osxsrc->sample_list) {
        g_mutex_unlock(&osxsrc->sample_lock);
        return -1;
    }
    
    buf = (GstBuffer*)osxsrc->sample_list->data;
    osxsrc->sample_list = g_slist_delete_link(osxsrc->sample_list, osxsrc->sample_list);
    len = (length  > GST_BUFFER_SIZE(buf)) ? GST_BUFFER_SIZE(buf) : length;
    
    memcpy( data,  GST_BUFFER_DATA(buf),  len);
    if( GST_BUFFER_SIZE(buf) > length) {
        GST_BUFFER_DATA(buf) = GST_BUFFER_DATA(buf) + len;
        GST_BUFFER_SIZE(buf)  = GST_BUFFER_SIZE(buf) - len;
        osxsrc->sample_list = g_slist_prepend(osxsrc->sample_list, buf);
    } else {
        gst_buffer_unref(buf);
    }
    
    g_mutex_unlock(&osxsrc->sample_lock);
    return len;
}

static void
gst_osx_soundflower_src_callback(const void* data, const int len,  const Float64 sampleTime,  void* usr_data) {
    GstOsxSoundflowerSrc* osxsrc;
    osxsrc = (GstOsxSoundflowerSrc*)(usr_data);
    GstBuffer* buf;
    
    g_mutex_lock(&osxsrc->sample_lock);
    
    buf = gst_buffer_new_and_alloc(len);
    if( buf) {
        memcpy(GST_BUFFER_DATA(buf), data, len);
        GST_BUFFER_SIZE(buf) = len;
        osxsrc->sample_list = g_slist_append(osxsrc->sample_list, buf);
    }
    g_mutex_unlock(&osxsrc->sample_lock);
    
    g_cond_signal(&osxsrc->sample_cond);
    
    if (GST_CLOCK_TIME_NONE != osxsrc->next_time) {
        if (osxsrc->next_time != sampleTime) {
            GST_WARNING_OBJECT(osxsrc, "audio discontinue detected %d", (int)(sampleTime-osxsrc->next_time));
        }
    }
    osxsrc->next_time = sampleTime + len / 8;
}

static gboolean
gst_osx_soundflower_src_register(GstOsxSoundflowerSrc* osxsrc) {
    if( osxsrc->register_id)
        return TRUE;
    if( !osxsrc->ctrl)
        return FALSE;
    
    lock_ctrl();
    osxsrc->register_id =  [(SoundflowerController*)(osxsrc->ctrl) register: gst_osx_soundflower_src_callback: osxsrc];
    unlock_ctrl();
    return !!osxsrc->register_id;
}

static gboolean
gst_osx_soundflower_src_unregister(GstOsxSoundflowerSrc* osxsrc) {
    if (osxsrc->register_id && osxsrc->ctrl) {
        lock_ctrl();
        [(SoundflowerController*)(osxsrc->ctrl) unregister: &osxsrc->register_id];
        unlock_ctrl();
    }
    g_cond_signal(&osxsrc->sample_cond);
    return !osxsrc->register_id;
}

static gboolean
gst_osx_soundflower_src_acquire_ctrl(GstOsxSoundflowerSrc* osxsrc) {
    if( osxsrc->ctrl)
        return TRUE;
    lock_ctrl();
    osxsrc->ctrl = ctrl_ref();
    unlock_ctrl();
    return !!osxsrc->ctrl;
}

static gboolean
gst_osx_soundflower_src_release_ctrl(GstOsxSoundflowerSrc* osxsrc) {
    if(!osxsrc->ctrl)
        return TRUE;
    lock_ctrl();
    ctrl_unref(&osxsrc->ctrl);
    unlock_ctrl();
    return !osxsrc->ctrl;
}

static gboolean
gst_osx_soundflower_src_clean(GstOsxSoundflowerSrc* osxsrc) {
    g_mutex_lock(&osxsrc->sample_lock);
    
    g_slist_foreach (osxsrc->sample_list, (GFunc) gst_buffer_unref, NULL);
    g_slist_free (osxsrc->sample_list);
    osxsrc->sample_list = NULL;
    
    g_mutex_unlock(&osxsrc->sample_lock);
    
    return TRUE;
    
}

static GstStateChangeReturn
gst_osx_soundflower_src_change_state (GstElement * element,
                                      GstStateChange transition)
{
    GstStateChangeReturn ret;
    GstOsxSoundflowerSrc * osxsrc;
    
    osxsrc = GST_OSX_SOUNDFLOWER_SRC(element);
    
    switch (transition) {
        case GST_STATE_CHANGE_PAUSED_TO_READY:
            g_cond_signal(&osxsrc->sample_cond);
            break;
        default:
            break;
    }
    
    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
    
    return ret;
}
