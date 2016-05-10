/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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

/* First, include the header file for the plugin, to bring in the
 * object definition and other useful things.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#ifdef HAVE_FFMPEG_UNINSTALLED
#include <avcodec.h>
#include <avformat.h>
#else
#include <DemuxAPI/libavcodec/avcodec.h>
#include <DemuxAPI/demuxer/demuxer.h>
#endif

#include "gstffmpeg.h"
#include "gstffmpegutils.h"

demuxer_t g_demuxer;


GST_DEBUG_CATEGORY (ffmpeg_debug);

static GStaticMutex gst_avcodec_mutex = G_STATIC_MUTEX_INIT;


int
gst_ffmpeg_avcodec_open (AVCodecContext * avctx, AVCodec * codec)
{
  int ret;

  g_static_mutex_lock (&gst_avcodec_mutex);
  ret = g_demuxer.avcodec_open (avctx, codec);
  g_static_mutex_unlock (&gst_avcodec_mutex);

  return ret;
}

int
gst_ffmpeg_avcodec_close (AVCodecContext * avctx)
{
  int ret;

  g_static_mutex_lock (&gst_avcodec_mutex);
  ret = g_demuxer.avcodec_close (avctx);
  g_static_mutex_unlock (&gst_avcodec_mutex);

  return ret;
}

int
gst_ffmpeg_av_find_stream_info (AVFormatContext * ic)
{
  int ret;

  g_static_mutex_lock (&gst_avcodec_mutex);
  ret = g_demuxer.av_find_stream_info (ic);
  g_static_mutex_unlock (&gst_avcodec_mutex);

  return ret;
}

#ifndef GST_DISABLE_GST_DEBUG
static void
gst_ffmpeg_log_callback (void *ptr, int level, const char *fmt, va_list vl)
{
#if 0
  GstDebugLevel gst_level;
  gint len = strlen (fmt);
  gchar *fmt2 = NULL;

  if (_shut_up_I_am_probing)
    return;

  switch (level) {
    case AV_LOG_QUIET:
      gst_level = GST_LEVEL_NONE;
      break;
    case AV_LOG_ERROR:
      gst_level = GST_LEVEL_ERROR;
      break;
    case AV_LOG_INFO:
      gst_level = GST_LEVEL_INFO;
      break;
    case AV_LOG_DEBUG:
      gst_level = GST_LEVEL_DEBUG;
      break;
    default:
      gst_level = GST_LEVEL_INFO;
      break;
  }

  /* remove trailing newline as it gets already appended by the logger */
  if (fmt[len - 1] == '\n') {
    fmt2 = g_strdup (fmt);
    fmt2[len - 1] = '\0';
  }

  gst_debug_log_valist (ffmpeg_debug, gst_level, "", "", 0, NULL,
      fmt2 ? fmt2 : fmt, vl);

  g_free (fmt2);
#endif
}
#endif

#ifndef GST_DISABLE_GST_DEBUG
gboolean _shut_up_I_am_probing = FALSE;
#endif




enum { alloc_size_stack_cap = 256 };
size_t alloc_size_stack[alloc_size_stack_cap+1];
int alloc_size_stack_index = alloc_size_stack_cap-1;

void push(size_t s)
{
  alloc_size_stack[alloc_size_stack_index] = s;
  alloc_size_stack_index--;
  if( alloc_size_stack_index < 0 )
  {
    alloc_size_stack_index ++ ;
    memmove(alloc_size_stack+1, alloc_size_stack, 
      sizeof(size_t)*alloc_size_stack_cap);
  }

}

int __cdecl DemuxerALLOC_HOOK(
                       int nAllocType,
                       void * pvData,
                       size_t nSize,
                       int nBlockUse,
                       long lRequest,
                       const unsigned char * szFileName,
                       int nLine)
{

#ifdef _DEBUG
  if( nAllocType == 1)
  {
    if(nSize == 7179 )
    //_CrtDbgBreak();
    push(nSize);
  }
#endif		
  return 1;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (ffmpeg_debug, "ffmpeg", 0, "FFmpeg elements");
  get_demuxer(&g_demuxer);

#ifndef GST_DISABLE_GST_DEBUG

  g_demuxer.av_log_set_callback (gst_ffmpeg_log_callback);
#endif

  gst_ffmpeg_init_pix_fmt_info ();

  g_demuxer.av_register_all ();

  if( g_demuxer._CrtSetAllocHook)
    g_demuxer._CrtSetAllocHook(DemuxerALLOC_HOOK);

  gst_ffmpegdemux_register (plugin);

 // gst_ffmpegdec_register (plugin);


  g_demuxer.av_register_protocol (&gstreamer_protocol);
  g_demuxer.av_register_protocol (&gstpipe_protocol);

  /* Now we can return the pointer to the newly created Plugin object. */
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "ffmpeg-demuxer",
    "All FFmpeg codecs and formats (" FFMPEG_SOURCE ")",
    plugin_init, PACKAGE_VERSION, "LGPL", "FFmpeg", "http://ffmpeg.org/")
