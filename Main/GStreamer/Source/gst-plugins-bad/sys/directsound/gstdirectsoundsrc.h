/*
 * GStreamer
 * Copyright 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright 2005 S�bastien Moutte <sebastien@moutte.net>
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

#ifndef __GST_DIRECTSOUNDSRC_H__
#define __GST_DIRECTSOUNDSRC_H__

#include <gst/gst.h>
#include <gst/audio/gstaudiosrc.h>

#include <windows.h>
#include <dsound.h>

/* add here some headers if needed */


G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_DIRECTSOUND_SRC (gst_directsound_src_get_type())
#define GST_DIRECTSOUND_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DIRECTSOUND_SRC,GstDirectSoundSrc))
#define GST_DIRECTSOUND_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DIRECTSOUND_SRC,GstDirectSoundSrcClass))
#define GST_IS_DIRECTSOUND_SRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DIRECTSOUND_SRC))
#define GST_IS_DIRECTSOUND_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DIRECTSOUND_SRC))

typedef struct _GstDirectSoundSrc      GstDirectSoundSrc;
typedef struct _GstDirectSoundSrcClass GstDirectSoundSrcClass;

#define GST_DSOUND_LOCK(obj)	(g_mutex_lock (obj->dsound_lock))
#define GST_DSOUND_UNLOCK(obj)	(g_mutex_unlock (obj->dsound_lock))

#define GST_DSOUND_CHANGE_LOCK(obj) (g_mutex_lock(obj->dsound_change_lock))
#define GST_DSOUND_CHANGE_UNLOCK(obj) (g_mutex_unlock(obj->dsound_change_lock))

struct _GstDirectSoundSrc
{

  GstAudioSrc src;

  HINSTANCE DSoundDLL; /* DLL instance */
  LPDIRECTSOUNDCAPTURE pDSC; /* DirectSoundCapture*/
  LPDIRECTSOUNDCAPTUREBUFFER pDSBSecondary;  /*Secondaty capturebuffer*/
  DWORD current_circular_offset;

  HANDLE rghEvent;
  DWORD notifysize;

  guint buffer_size;
  guint latency_size;
  guint bytes_per_sample;

  guint buffer_time;
  guint latency_time;

  gchar* device_name;
  gchar* device_name_locale;

  GMutex *dsound_lock;
  GMutex *dsound_change_lock;


  HRESULT (WINAPI * pDSoundCaptureCreate) (LPGUID,
      LPDIRECTSOUNDCAPTURE *, LPUNKNOWN);

  HRESULT (WINAPI* pDirectSoundCaptureEnumerate) (
      LPDSENUMCALLBACK,LPVOID);

  GUID deviceId;

  gboolean need_restart;
  GstClockTime stream_time;

  WAVEFORMATEX wfx;
  gint device_changed;
  gint device_changed_signaled;

  struct
  {
      GCond exit_event;
      GCond init_event;
      GMutex lock;
      GThread* thread;
      gboolean running;
  } device_monitor;

  guint32 last_dummy_time;
  guint32 try_to_start_time;
  gint try_to_start_count;
  gboolean invalid;
  guint32 invalid_time;
  guint32 invalid_dur;
};

struct _GstDirectSoundSrcClass 
{
  GstAudioSrcClass parent_class;
};

GType gst_directsound_src_get_type (void);

G_END_DECLS

#endif /* __GST_DIRECTSOUNDSRC_H__ */
