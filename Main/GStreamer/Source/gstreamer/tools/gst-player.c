/* GStreamer
 * Copyright (C) 2010 David Hoyt <dhoyt@hoytsoft.org>
 *
 * gst-player.c: tool to launch and control GStreamer pipelines.
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
#  include "config.h"
#endif

/* FIXME: hack alert */
#ifdef HAVE_WIN32
#define WINVER 0x0501
#define DISABLE_FAULT_HANDLER
#include <windows.h>
#include <io.h>                 /* lseek, open, close, read */
#endif

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef DISABLE_FAULT_HANDLER
#include <sys/wait.h>
#endif
#include <locale.h>             /* for LC_ALL */
#include "tools.h"

#include <gst/interfaces/xoverlay.h>

/* #define gst_object_unref(obj) */

#define DEFAULT_PROGRAM_NAME "gst-player"
#define DEFAULT_AUDIO_SINK   "autoaudiosink"
#define DEFAULT_VIDEO_SINK   "autovideosink"

#define ERROR_INITIALIZATION 1

#define SEEK_TIMEOUT         40 * GST_MSECOND

#define FORWARD_RATE         1.0
#define REVERSE_RATE        -1.0

#define GST_PLAYER_PING_LOCK	     g_static_mutex_lock(&ping_lock);
#define GST_PLAYER_PING_UNLOCK     g_static_mutex_unlock(&ping_lock);
#define GST_PLAYER_COMMAND_LOCK	   g_static_mutex_lock(&cmd_lock);
#define GST_PLAYER_COMMAND_UNLOCK  g_static_mutex_unlock(&cmd_lock);
#define GST_PLAYER_MESSAGE_LOCK	   g_static_mutex_lock(&msg_lock);
#define GST_PLAYER_MESSAGE_UNLOCK  g_static_mutex_unlock(&msg_lock);

#define GST_PLAYER_PRINT_REF_COUNT(o) GST_PLAYER_PRINT("ref count: %d", o->object.object.ref_count)

#define GST_PLAYER_REPEAT_FOREVER(val) (val < 0)
#define GST_PLAYER_STR_EQUALS(s1, s2) (g_ascii_strcasecmp(s1, s2) == 0)

#define GST_PLAYER_MEDIA_IS_STILL(media_type) ((media_type & MEDIA_TYPE_STILL) == MEDIA_TYPE_STILL)
#define GST_PLAYER_MEDIA_IS_AUDIO(media_type) ((media_type & MEDIA_TYPE_AUDIO) == MEDIA_TYPE_AUDIO)
#define GST_PLAYER_MEDIA_IS_VIDEO(media_type) ((media_type & MEDIA_TYPE_VIDEO) == MEDIA_TYPE_VIDEO)
#define GST_PLAYER_MEDIA_IS_AUDIO_ONLY(media_type) (media_type == MEDIA_TYPE_AUDIO)
#define GST_PLAYER_MEDIA_IS_VIDEO_ONLY(media_type) (media_type == MEDIA_TYPE_VIDEO)

#define GST_PLAYER_KLASS_IS_IMAGE(klass)   (g_strstr_len(klass, -1, "/Image") || g_strstr_len(klass, -1, "Image/"))
#define GST_PLAYER_KLASS_IS_SOURCE(klass)  (g_strstr_len(klass, -1, "Source/") || g_strstr_len(klass, -1, "/Source"))
#define GST_PLAYER_KLASS_IS_PARSER(klass)  (g_strstr_len(klass, -1, "/Demuxer") || g_strstr_len(klass, -1, "Demuxer/"))
#define GST_PLAYER_KLASS_IS_DECODER(klass) (g_strstr_len(klass, -1, "/Decoder") || g_strstr_len(klass, -1, "Decoder/"))
#define GST_PLAYER_KLASS_IS_GENERIC(klass) (g_strcmp0(klass, "Generic") == 0 || g_strstr_len(klass, -1, "Generic/") || g_strstr_len(klass, -1, "/Generic"))

#define GST_PLAYER_STATE_CHANGE_ERROR(app) GST_ELEMENT_ERROR(app->pipeline, CORE, STATE_CHANGE, ("Unable to stream media"), (NULL))

#define GST_PLAYER_PRINT(format, ...)                                               \
  {                                                                                 \
    GST_PLAYER_MESSAGE_LOCK                                                         \
    fprintf(stderr, format "\n", ## __VA_ARGS__);                                   \
    fflush(stderr);                                                                 \
    GST_PLAYER_MESSAGE_UNLOCK                                                       \
  }

#define GST_PLAYER_EVENT(format, ...)                                               \
  {                                                                                 \
    if (g_app.command_mode) {                                                       \
      GST_PLAYER_MESSAGE_LOCK                                                       \
      fprintf(stderr, "event, " format "\n", ## __VA_ARGS__);                       \
      fflush(stderr);                                                               \
      GST_PLAYER_MESSAGE_UNLOCK                                                     \
    }                                                                               \
  }

#define GST_PLAYER_RESPONSE(format, ...)                                            \
  {                                                                                 \
    if (g_app.command_mode) {                                                       \
      GST_PLAYER_MESSAGE_LOCK                                                       \
      fprintf(stderr, "response, " format "\n", ## __VA_ARGS__);                    \
      fflush(stderr);                                                               \
      GST_PLAYER_MESSAGE_UNLOCK                                                     \
    }                                                                               \
  }

#define GST_PLAYER_ERROR(format, ...)                                               \
  {                                                                                 \
    if (g_app.command_mode) {                                                       \
      GST_PLAYER_MESSAGE_LOCK                                                       \
      fprintf(stderr, "error, " format "\n", ## __VA_ARGS__);                       \
      fflush(stderr);                                                               \
      GST_PLAYER_MESSAGE_UNLOCK                                                     \
    } else {                                                                        \
      GST_PLAYER_MESSAGE_LOCK                                                       \
      fprintf(stderr, format "\n", ## __VA_ARGS__);                                 \
      fflush(stderr);                                                               \
      GST_PLAYER_MESSAGE_UNLOCK                                                     \
    }                                                                               \
  }

#define GST_PLAYER_PLAIN_ERROR(code, msg, err)                                      \
  {                                                                                 \
    if (g_app.command_mode) {                                                       \
      GST_PLAYER_ERROR("%d, " msg ": %s", code, err)                                \
    } else {                                                                        \
      GST_PLAYER_ERROR(msg ": %s", err)                                             \
    }                                                                               \
  }

#define GST_PLAYER_GST_ERROR(err)                                                   \
  {                                                                                 \
    if (g_app.command_mode) {                                                       \
        GST_PLAYER_ERROR("%d, %s", err->code, GST_STR_NULL(err->message))           \
    } else {                                                                        \
        GST_PLAYER_ERROR("%s", GST_STR_NULL(err->message))                          \
    }                                                                               \
  }

#define GST_PLAYER_GST_ERROR_MSG(msg, err)                                          \
  {                                                                                 \
    if (g_app.command_mode) {                                                       \
      if (err) {                                                                    \
        GST_PLAYER_ERROR("%d, " msg ": %s", err->code, GST_STR_NULL(err->message))  \
      } else {                                                                      \
        GST_PLAYER_ERROR("%d, %s", 0, msg)                                          \
      }                                                                             \
    } else {                                                                        \
      if (err) {                                                                    \
        GST_PLAYER_ERROR(msg ": %s", GST_STR_NULL(err->message))                    \
      } else {                                                                      \
        GST_PLAYER_ERROR("%s", msg)                                                 \
      }                                                                             \
    }                                                                               \
  }

extern volatile gboolean glib_on_error_halt;

typedef enum _Command
{
  COMMAND_UNKNOWN            =  0, 
  COMMAND_QUIT               =  1, 
  COMMAND_PAUSE              =  2, 
  COMMAND_CONTINUE           =  3, 
  COMMAND_STOP               =  4, 
  COMMAND_STEP_FORWARD       =  5, 
  COMMAND_STEP_BACKWARD      =  6, 
  COMMAND_TIME_SEEK_FORWARD  =  7, 
  COMMAND_TIME_SEEK_BACKWARD =  8, 
  COMMAND_ADJUST_RATE        =  9, 
  COMMAND_ADJUST_VOLUME      = 10, 
  COMMAND_MUTE               = 11, 
  COMMAND_UNMUTE             = 12, 
  COMMAND_TOGGLE_MUTE        = 13,
  COMMAND_PING               = 14, 
  COMMAND_SNAPSHOT           = 15, 
  COMMAND_QUERY_POSITION     = 16, 
  COMMAND_QUERY_DURATION     = 17, 
  COMMAND_QUERY_VOLUME       = 18, 
  COMMAND_QUERY_MUTE         = 19, 
  COMMAND_EXPOSE             = 20
} Command;

typedef enum _VideoTestSrcPattern
{
  VIDEO_TEST_SRC_PATTERN_SMPTE = 0, 
  VIDEO_TEST_SRC_PATTERN_BLACK = 2
} VideoTestSrcPattern;

typedef enum _VideoScaleMethod
{
  VIDEO_SCALE_METHOD_NEAREST  = 0, 
  VIDEO_SCALE_METHOD_BILINEAR = 1, 
  VIDEO_SCALE_METHOD_4TAP     = 2
} VideoScaleMethod;

typedef enum _MediaType
{
  MEDIA_TYPE_UNKNOWN  = 0, 
  MEDIA_TYPE_STILL    = (1 << 0), 
  MEDIA_TYPE_AUDIO    = (1 << 1), 
  MEDIA_TYPE_VIDEO    = (1 << 2)
} MediaType;

typedef enum _EventLoopResult
{
  ELR_NO_ERROR = 0,
  ELR_ERROR,
  ELR_INTERRUPT
} EventLoopResult;

typedef struct _AppCommand AppCommand;
struct _AppCommand {
  Command cmd;
  gint argc; 
  gdouble arg0;
  gdouble arg1;
  gdouble arg2;
  gdouble arg3;
  gpointer data;
};

typedef struct _App App;
struct _App
{
  GstBus *bus;

  GstElement *pipeline;
  GstElement *playbin;
  GstElement *audio_sink;
  GstElement *video_sink;

  gchar* uri;

  gboolean command_mode;
  gboolean exiting;

  gboolean cmd_processor_please_exit;
  GThread* cmd_processor_thread;
  GMainLoop *main_loop;

  EventLoopResult caught_error;

  gint last_percent;
  gboolean buffering;
  gboolean buffering_disabled;
  gboolean download_disabled;

  gboolean ping_please_exit;
  GThread* ping_thread;
  guint64 ping_interval;

  gboolean waiting_eos;
  gboolean is_live;
  guintptr window_handle;
  gint buffer_size;
  gint64 buffer_duration;

  gboolean first_audio_round;
  gboolean first_video_round;

  gboolean auto_sync_disabled;

  gint repeat_count;
  gint total_repeat_count;

  gboolean is_test_pattern;
  gboolean has_multipartdemux;
  gboolean has_jpegdec;

  gfloat rate;

  gint width;
  gint height;

  gint fps_n;
  gint fps_d;
  gboolean has_fps;

  gfloat actual_fps;
  gint actual_width;
  gint actual_height;

  MediaType media_type;
};

static App g_app;
static GStaticMutex msg_lock = G_STATIC_MUTEX_INIT;
static GStaticMutex cmd_lock = G_STATIC_MUTEX_INIT;
static GCond* cmd_processor_started;
static gboolean cmd_processor_is_started;
static GStaticMutex ping_lock = G_STATIC_MUTEX_INIT;
static GCond* ping_started;
static gboolean ping_is_started;

static void uridecodebin_element_added(GstBin* playbin, GstElement* element, App* app);
static void decodebin_element_added(GstBin* playbin, GstElement* element, App* app);
static void uridecodebin_pad_added(GstElement *element, GstPad *pad, App* app);
static GstBusSyncReply xoverlay_bus_sync_handler(GstBus *bus, GstMessage *message, App* app);
static GstCaps* create_app_caps(App *app);
static GstCaps* create_caps(gboolean has_fps, gint fps_n, gint fps_d, gint width, gint height);
static GstCaps* create_app_pattern_caps(App* app);
static GstCaps* create_pattern_caps(gboolean has_fps, gint fps_n, gint fps_d, gint width, gint height);
static gboolean change_app_playback_direction(App* app, gboolean forward);
static gboolean change_playback_direction(GstElement* element, gfloat* rate, gboolean forward);
static gboolean app_step(App* app, gboolean forward);
static gboolean app_pause(App* app);
static gboolean app_stop(App* app);
static gboolean app_play(App* app);
static void examine_element(gchar* factory_name, GstElement* element, App* app);
static void examine_videosink(gchar* factory_name, GstElement* element, App* app);
static void notify_audio_caps(GstPad* pad, GstPad* peer, App* app);
static void notify_video_caps(GstPad* pad, GstPad* peer, App* app);
static AppCommand* create_app_command(gint argc, Command cmd, gdouble arg0, gdouble arg1, gdouble arg2, gdouble arg3, App* app);
static gboolean process_app_command(AppCommand* cmd);

static gchar* 
readline(gpointer file) 
{
  gchar* buffer = NULL;
  gchar* tmp_buf = NULL;
  gboolean line_read = FALSE;
  gint iteration = 0;
  gint offset = 0;

  if (!file)
    return NULL;

  while(!line_read) {
    if (!(tmp_buf = g_malloc(1024))) {
      if (buffer)
        g_free(buffer);
      return NULL;
    }

    if (!fgets(tmp_buf, 1024, file)) {
      g_free(tmp_buf);
      break;
    }

    if (tmp_buf[strlen(tmp_buf) - 1] == '\n')
      line_read = TRUE;

    offset = 1024 * (iteration + 1);

    if (!(buffer = g_realloc(buffer, offset))) {
      g_free(tmp_buf);
      return NULL;
    }

    offset = 1024 * iteration - iteration;

    if (!memcpy(buffer + offset, tmp_buf, 1024)) {
      g_free(tmp_buf);
      if (!buffer)
        g_free(buffer);
      return NULL;
    }

    g_free(tmp_buf);
    iteration++;
  }
  return buffer;
}

static gboolean 
g_object_property_exists(GObject* object, const gchar* property_name) 
{
  if (!object || !property_name)
    return FALSE;
  {
    GParamSpec* spec;
    spec = g_object_class_find_property(G_OBJECT_GET_CLASS(object), property_name);
    return ((spec) ? TRUE : FALSE);
  }
}

static gboolean 
g_object_property_exists_of_type(GObject* object, const gchar* property_name, GType g_type) 
{
  if (!object || !property_name)
    return FALSE;
  {
    GParamSpec* spec;
    spec = g_object_class_find_property(G_OBJECT_GET_CLASS(object), property_name);
    if (!spec)
      return FALSE;
    if (g_type == G_TYPE_INVALID || g_type == spec->value_type || g_type_is_a(spec->value_type, g_type))
      return TRUE;
    return FALSE;
  }
}

static gboolean 
transition_with_timeout(GstState intended_state, App* app) {
  GstState state = GST_STATE_NULL;
  gst_element_set_state(app->pipeline, intended_state);
  return (gst_element_get_state(app->pipeline, &state, NULL, 3000 * GST_MSECOND) == GST_STATE_CHANGE_SUCCESS && state == intended_state);
}

static gboolean 
query_state(GstState* state, App* app) 
{
  return (gst_element_get_state(app->pipeline, state, NULL, 0) == GST_STATE_CHANGE_SUCCESS);
}

static void 
examine_videosinks(GstBin* bin, App* app) 
{
  gboolean done;
  GstIterator* iter;
  GstElement* element;
  GstElementFactory* factory;

  /* Locate the sinks implementing the xoverlay interface. */
  done = FALSE;
  iter = gst_bin_iterate_all_by_interface(bin, gst_x_overlay_get_type());
    
  if (!iter)
    return;

  while (!done) {
    switch(gst_iterator_next(iter, &element)) {
      case GST_ITERATOR_OK:
        /* Expose these elements. */
        factory = gst_element_get_factory(element);
        if (factory)
          examine_videosink(GST_PLUGIN_FEATURE_NAME(factory), element, app);
        gst_object_unref(element);
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync(iter);
        break;
      case GST_ITERATOR_ERROR:
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
  }

  gst_iterator_free(iter);
}

static void 
examine_videosink(gchar* factory_name, GstElement* element, App* app) 
{
  if (!element)
    return;
  
  if (GST_PLAYER_MEDIA_IS_VIDEO_ONLY(app->media_type) && app->is_live) {
    /* Turn off sync if we've got a live source and it's video only (e.g. a motion jpeg camera). */
    if (!app->auto_sync_disabled && g_object_property_exists_of_type(G_OBJECT(element), "sync", G_TYPE_BOOLEAN))
      g_object_set(element, "sync", FALSE, NULL);

    /* Turn off max-lateness if we've got a live source and it's video only (e.g. a motion jpeg camera). */
    if (!app->auto_sync_disabled && g_object_property_exists_of_type(G_OBJECT(element), "max-lateness", G_TYPE_INT64))
      g_object_set(element, "max-lateness", (gint64)-1, NULL);
  }
}

static void 
examine_elements(GstBin* bin, App* app) 
{
  gboolean done;
  GstIterator* iter;
  GstElement* element;
  GstElementFactory* factory;

  done = FALSE;
  iter = gst_bin_iterate_recurse(bin);
    
  if (!iter)
    return;

  while (!done) {
    switch(gst_iterator_next(iter, &element)) {
      case GST_ITERATOR_OK:
        factory = gst_element_get_factory(element);
        if (factory)
          examine_element(GST_PLUGIN_FEATURE_NAME(factory), element, app);
        gst_object_unref(element);
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync(iter);
        break;
      case GST_ITERATOR_ERROR:
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
  }

  gst_iterator_free(iter);
}

static void 
examine_element(gchar* factory_name, GstElement* element, App* app) 
{
  if (!element)
    return;

  if (g_str_has_prefix(factory_name, "multipartdemux")) {
    
    if (g_object_property_exists(G_OBJECT(element), "single-stream"))
      g_object_set(element, "single-stream", TRUE, NULL);

  } else if (g_str_has_prefix(factory_name, "jpegdec")) {
    
    if (g_object_property_exists(G_OBJECT(element), "max-errors"))
      g_object_set(element, "max-errors", 10, NULL);

    /* Change IDCT method to float. On x86 processors, the output is noticeably better. */
    g_object_set(element, "idct-method", 2, NULL);

  } else if (g_str_has_prefix(factory_name, "souphttpsrc")) {
    
    if (app->is_live) {
      g_object_set(element, "do-timestamp", TRUE, NULL);
      g_object_set(element, "is-live", TRUE, NULL);
    }
    g_object_set(element, "timeout", 3, NULL);
    g_object_set(element, "automatic-redirect", TRUE, NULL);

  } else if (g_str_has_prefix(factory_name, "neonhttpsrc")) {

    if (app->is_live) {
      g_object_set(element, "do-timestamp", TRUE, NULL);
    }
    g_object_set(element, "accept-self-signed", TRUE, NULL);
    g_object_set(element, "connect-timeout", 3, NULL);
    g_object_set(element, "read-timeout", 3, NULL);
    g_object_set(element, "automatic-redirect", TRUE, NULL);

  }
}

static GstCaps* 
create_app_caps(App* app) 
{
  return create_caps(app->has_fps && !app->is_live, app->fps_n, app->fps_d, app->width, app->height);
}

static GstCaps* 
create_caps(gboolean has_fps, gint fps_n, gint fps_d, gint width, gint height) 
{
  GstCaps* caps;
  GstStructure* structures[2];

  /* Generate caps filter */
  
  structures[0] = gst_structure_new(
      "video/x-raw-yuv",
      NULL
  );
  structures[1] = gst_structure_new(
      "video/x-raw-rgb",
      NULL
  );

  if (has_fps) {
    gst_structure_set(structures[0], "framerate", GST_TYPE_FRACTION, fps_n, fps_d, NULL);
    gst_structure_set(structures[1], "framerate", GST_TYPE_FRACTION, fps_n, fps_d, NULL);
  }
  if (width > 0) {
    gst_structure_set(structures[0], "width", G_TYPE_INT, width, NULL);
    gst_structure_set(structures[1], "width", G_TYPE_INT, width, NULL);
  }
  if (height > 0) {
    gst_structure_set(structures[0], "height", G_TYPE_INT, height, NULL);
    gst_structure_set(structures[1], "height", G_TYPE_INT, height, NULL);
  }

  caps = gst_caps_new_full(structures[0], structures[1], NULL);

  return caps;
}

static GstCaps* 
create_app_pattern_caps(App* app) 
{
  return create_pattern_caps(app->has_fps && !app->is_live, app->fps_n, app->fps_d, app->width, app->height);
}

static GstCaps* 
create_pattern_caps(gboolean has_fps, gint fps_n, gint fps_d, gint width, gint height) 
{
  GstCaps* caps;
  GstStructure* structures[2];

  /* Generate caps filter */
  
  structures[0] = gst_structure_new(
      "video/x-raw-yuv",
      NULL
  );
  structures[1] = gst_structure_new(
      "video/x-raw-rgb",
      NULL
  );

  if (has_fps) {
    gst_structure_set(structures[0], "framerate", GST_TYPE_FRACTION, fps_n, fps_d, NULL);
    gst_structure_set(structures[1], "framerate", GST_TYPE_FRACTION, fps_n, fps_d, NULL);
  }
  if (width > 0) {
    gst_structure_set(structures[0], "width", G_TYPE_INT, width, NULL);
    gst_structure_set(structures[1], "width", G_TYPE_INT, width, NULL);
  }
  if (height > 0) {
    gst_structure_set(structures[0], "height", G_TYPE_INT, height, NULL);
    gst_structure_set(structures[1], "height", G_TYPE_INT, height, NULL);
  }

  /* Apply a certain colorspace to help reduce video driver quirkiness due to linear interpolation 
     of the last column of pixels with garbage outside the texture in various video driver 
     implementations. */
  gst_structure_set(structures[0], "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('I', '4', '2', '0'), NULL);

  caps = gst_caps_new_full(structures[0], structures[1], NULL);

  return caps;
}

static gboolean 
change_app_playback_direction(App* app, gboolean forward) 
{
  return change_playback_direction(app->pipeline, &app->rate, forward);
}

static gboolean 
change_playback_direction(GstElement* element, gfloat* rate, gboolean forward) 
{
  /* Courtesy totem */
  gboolean is_forward;
  gboolean retval;

  is_forward = (*rate >= 0.0);
  if (forward == is_forward)
    return TRUE;

  retval = FALSE;
  if (!forward) {
    GstEvent* evt;
    GstFormat fmt;
    gint64 cur;

    cur = 0;
    fmt = GST_FORMAT_TIME;

    if (gst_element_query_position(element, &fmt, &cur)) {
      /* Setting playback direction to reverse. */
      evt = gst_event_new_seek(REVERSE_RATE, fmt, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE, GST_SEEK_TYPE_SET, G_GINT64_CONSTANT(0), GST_SEEK_TYPE_SET, cur);
      if (!gst_element_send_event(element, evt)) {
        /* Failed to set playback direction to reverse. */
      } else {
        //gst_element_get_state(element, NULL, NULL, GST_CLOCK_TIME_NONE);
        *rate = REVERSE_RATE;
        retval = TRUE;
      }
    } else {
      /* Failed to query position to set playback to reverse. */
    }
  } else {
    GstEvent* evt;
    GstFormat fmt;
    gint64 cur;

    cur = 0;
    fmt = GST_FORMAT_TIME;

    if (gst_element_query_position(element, &fmt, &cur)) {
      /* Setting playback direction to forward. */
      evt = gst_event_new_seek(FORWARD_RATE, fmt, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE, GST_SEEK_TYPE_SET, cur, GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
      if (!gst_element_send_event(element, evt)) {
        /* Failed to set playback direction to forward. */
      } else {
        //gst_element_get_state(element, NULL, NULL, GST_CLOCK_TIME_NONE);
        *rate = FORWARD_RATE;
        retval = TRUE;
      }
    } else {
      /* Failed to query position to set playback to forward. */
    }
  }

  return retval;
}

static gboolean 
app_step(App* app, gboolean forward) 
{
  GstEvent* evt;
  
  if (!change_app_playback_direction(app, forward))
    return FALSE;

  //gst_element_set_state(GST_ELEMENT(app->pipeline), GST_STATE_PAUSED);
  //gst_element_get_state(app->pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

  app_pause(app);

  evt = gst_event_new_step(GST_FORMAT_BUFFERS, 1, 1.0, TRUE, FALSE);
  if (!evt)
    return FALSE;

  return gst_element_send_event(GST_ELEMENT(app->pipeline), evt);
}

static gboolean 
app_pause(App* app) 
{
  GstStateChangeReturn ret;
  GstState state;

  /* Get the current state */
  ret = gst_element_get_state(GST_ELEMENT(app->pipeline), &state, NULL, 0);

  if (app->is_live != FALSE &&
      ret != GST_STATE_CHANGE_NO_PREROLL &&
      ret != GST_STATE_CHANGE_SUCCESS &&
      state > GST_STATE_READY) {
    app_stop(app);
    return FALSE;
  }

  if (state > GST_STATE_PAUSED) {
    gst_element_set_state(GST_ELEMENT(app->pipeline), GST_STATE_PAUSED);
    gst_element_get_state(app->pipeline, NULL, NULL, -1);
  }
  return FALSE;
}

static gboolean 
app_stop(App* app) 
{
  GstState cur_state;
  GstBus *bus;

  bus = gst_element_get_bus(app->pipeline);
  gst_element_get_state(app->pipeline, &cur_state, NULL, 0);
  if (cur_state > GST_STATE_NULL) {
    //GstMessage *msg;

    gst_element_set_state (app->pipeline, GST_STATE_NULL);

    /* process all remaining state-change messages so everything gets
     * cleaned up properly (before the state change to NULL flushes them) */
    //while ((msg = gst_bus_poll(bus, GST_MESSAGE_STATE_CHANGED, 0))) {
    //  gst_bus_async_signal_func(bus, msg, NULL);
    //  gst_message_unref(msg);
    //}
  }

  /* and now drop all following messages until we start again. The
   * bus is set to flush=false again in bacon_video_widget_open()
   */
  gst_bus_set_flushing(bus, TRUE);
  gst_object_unref(bus);

  app->repeat_count = 0;
  app->rate = FORWARD_RATE;

  GST_PLAYER_EVENT("stopped");

  return FALSE;
}

static gboolean 
app_play(App* app) 
{
  GstState cur_state;
  GstBus *bus;

  /* Don't try to play if we're already doing that */
  gst_element_get_state(app->pipeline, &cur_state, NULL, 0);
  if (cur_state == GST_STATE_PLAYING)
    return FALSE;

  /* Flush the bus to make sure we don't get any messages
   * from the previous URI, see bug #607224.
   */
  bus = gst_element_get_bus(app->pipeline);
  gst_bus_set_flushing(bus, TRUE);

  gst_element_set_state(app->pipeline, GST_STATE_READY);

  gst_bus_set_flushing(bus, FALSE);
  gst_object_unref(bus);

  gst_element_set_state(app->pipeline, GST_STATE_PAUSED);

  /* Set direction to forward */
  if (change_app_playback_direction(app, TRUE) == FALSE) {
    return FALSE;
  }

  app->rate = FORWARD_RATE;
  gst_element_set_state (app->pipeline, GST_STATE_PLAYING);

  return FALSE;
}

static gboolean 
app_continue(App* app) 
{
  GstState cur_state;

  /* Don't try to continue if we're already playing */
  gst_element_get_state(app->pipeline, &cur_state, NULL, 0);
  if (cur_state == GST_STATE_PLAYING)
    return FALSE;

  gst_element_set_state(app->pipeline, GST_STATE_PLAYING);
  gst_element_get_state(app->pipeline, NULL, NULL, -1);
  return FALSE;
}

static gboolean 
app_ready(App* app) 
{
  GST_PLAYER_COMMAND_LOCK
  gst_element_set_state(app->pipeline, GST_STATE_NULL);
  gst_element_get_state(app->pipeline, NULL, NULL, -1);
  GST_PLAYER_EVENT("stopped");
  GST_PLAYER_COMMAND_UNLOCK
  return FALSE;
}

static gboolean 
app_repeat(App* app) 
{
  app->repeat_count++;
  if (app->repeat_count == G_MAXINT)
    app->repeat_count = 1;

  GST_PLAYER_COMMAND_LOCK

  /* TODO: Do something more intelligent here -- retrieve the segment and restore it */
  /*       after setting the state to READY. */
  gst_element_set_state(app->pipeline, GST_STATE_NULL);
  gst_element_get_state(app->pipeline, NULL, NULL, -1);

  if (transition_with_timeout(GST_STATE_PLAYING, app)) {
    GST_PLAYER_EVENT("repeat, %d", app->repeat_count);
  } else {
    GST_PLAYER_STATE_CHANGE_ERROR(app);
  }

  GST_PLAYER_COMMAND_UNLOCK

  return FALSE;
}

static gboolean 
app_is_playing(App* app) 
{
  GstState state = GST_STATE_NULL;
  return (query_state(&state, app) && state >= GST_STATE_PAUSED);
}

static gboolean 
app_expose(App* app) 
{
  gboolean done;
  GstIterator* iter;
  GstElement* element;

  /* Locate the sinks implementing the xoverlay interface. */
  done = FALSE;
  iter = gst_bin_iterate_all_by_interface(GST_BIN(app->video_sink), gst_x_overlay_get_type());
    
  if (!iter)
    return FALSE;

  while (!done) {
    switch(gst_iterator_next(iter, &element)) {
      case GST_ITERATOR_OK:
        /* Expose these elements. */
        gst_x_overlay_expose(GST_X_OVERLAY(element));
        gst_object_unref(element);
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync(iter);
        break;
      case GST_ITERATOR_ERROR:
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
  }

  gst_iterator_free(iter);

  return FALSE;
}

static MediaType 
determine_media_type(GstBin* bin) 
{
  if (!bin)
    return MEDIA_TYPE_UNKNOWN;
  {
    MediaType media_type;
    gint decoder_count;
    gboolean done;
    GstElement* element;
    GstIterator* iter;
    GstElementFactory* factory;
    const gchar* klass;

    done = FALSE;
    decoder_count = 0;
    media_type = MEDIA_TYPE_UNKNOWN;
    iter = gst_bin_iterate_recurse(bin);
    
    if (!iter)
      return MEDIA_TYPE_UNKNOWN;

    while (!done) {
      switch(gst_iterator_next(iter, &element)) {
        case GST_ITERATOR_OK:
          factory = gst_element_get_factory(element);
          gst_object_unref(element);

          if (factory) {
            klass = gst_element_factory_get_klass(factory);

            if (GST_PLAYER_KLASS_IS_GENERIC(klass) || GST_PLAYER_KLASS_IS_SOURCE(klass)) {
              continue;
            }

            if (GST_PLAYER_KLASS_IS_PARSER(klass) || (GST_PLAYER_KLASS_IS_DECODER(klass) && ++decoder_count > 1)) {
              media_type = MEDIA_TYPE_VIDEO;
              goto break_loop;
            }

            if (GST_PLAYER_KLASS_IS_IMAGE(klass)) {
              media_type = MEDIA_TYPE_STILL;
            }
          }
          break;
        case GST_ITERATOR_RESYNC:
          gst_iterator_resync(iter);
          break;
        case GST_ITERATOR_ERROR:
        case GST_ITERATOR_DONE:
          done = TRUE;
          break;
      }
    }

break_loop:

    gst_iterator_free(iter);

    return media_type;
  }
}

/* Courtesy gstscreenshot.c */
static gboolean
create_element (const gchar* factory_name, GstElement** element, GError** err)
{
  *element = gst_element_factory_make(factory_name, NULL);
  if (*element)
    return TRUE;

  if (err && *err == NULL) {
    *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN, "cannot create element '%s' - please check your GStreamer installation", factory_name);
  }

  return FALSE;
}

/* Courtesy gstscreenshot.c */
/* Modified slightly to encode the buffer as a jpeg and push it into a temp dir. */
static gchar* 
encode_buffer_to_tmp_file(GstBuffer* buf)
{
  GstElement *src, *csp, *vscale, *jpegenc, *sink, *pipeline;
  GstMessage *msg;
  GError *error = NULL;
  GstBus *bus;
  GstCaps *from_caps;
  GstFlowReturn ret;
  gint fd_file = -1;
  gchar *file = NULL;
  gchar* utf8_file = NULL;
  GstCaps* to_caps = NULL;

  from_caps = GST_BUFFER_CAPS(buf);

  g_return_val_if_fail(from_caps != NULL, NULL);

  /* videoscale is here to correct for the pixel-aspect-ratio for us */
  if (
    !create_element("appsrc", &src, &error) ||
    !create_element("ffmpegcolorspace", &csp, &error) ||
    !create_element("videoscale", &vscale, &error) ||
    !create_element("jpegenc", &jpegenc, &error) || 
    !create_element("filesink", &sink, &error)
  )
    goto no_elements;

  pipeline = gst_pipeline_new("snapshot-pipeline");
  if (pipeline == NULL)
    goto no_pipeline;

  /* Create a temporary file */
  fd_file = g_file_open_tmp(NULL, &file, &error);
  if (error || fd_file == -1)
    goto no_tmpfile;

  /* Close it right away b/c the filesink will want to write to it. */
  close(fd_file);
  fd_file = -1;

  /* Convert to UTF-8 */
  utf8_file = g_filename_to_utf8(file, -1, NULL, NULL, &error);
  g_free(file);
  if (error || !utf8_file)
    goto no_tmpfile;
  file = utf8_file;

  /* Set src options */
  g_object_set(src, "stream-type", 0 /*stream*/, NULL);
  //g_object_set(src, "format", GST_FORMAT_BUFFERS, NULL);
  g_object_set(src, "num-buffers", 1, NULL);

  /* Add black borders if necessary to keep the DAR */
  g_object_set(vscale, "add-borders", TRUE, NULL);

  /* Increase jpeg quality */
  g_object_set(jpegenc, "quality", 100, NULL);
  g_object_set(jpegenc, "idct-method", 2 /*float*/, NULL);

  /* Set the filesink output. */
  g_object_set(sink, "location", file, NULL);

  /* set caps */
  g_object_set(src, "caps", from_caps, NULL);

  gst_bin_add_many(GST_BIN(pipeline), src, vscale, csp, jpegenc, sink, NULL);

  if (!gst_element_link_many(src, vscale, csp, jpegenc, sink, NULL))
    goto link_failed;

  bus = gst_element_get_bus(pipeline);

  

  /* Set the size of the entire data stream to the size of the buffer since we know ahead of time what it is */
  g_object_set(src, "size", (gint64)GST_BUFFER_SIZE(buf), NULL);

  /* now set the pipeline to the playing state, after we push the buffer into
   * appsrc, this should preroll the converted buffer into the filesink */
  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  /* feed buffer in appsrc */
  g_signal_emit_by_name(src, "push-buffer", buf, &ret);

  if (GST_FLOW_IS_SUCCESS(ret)) {
    g_signal_emit_by_name(src, "end-of-stream", &ret);
    
    if (GST_FLOW_IS_SUCCESS(ret)) {
      /* now see what happens. We either got an error somewhere or the pipeline prerolled */
      msg = gst_bus_poll(bus, GST_MESSAGE_ERROR | GST_MESSAGE_EOS, 10 * GST_SECOND);

      if (msg) {
        switch(GST_MESSAGE_TYPE (msg)) {
          case GST_MESSAGE_EOS:
          {
            break;
          }
          case GST_MESSAGE_ERROR:{
            gchar *dbg = NULL;

            gst_message_parse_error(msg, &error, &dbg);
            if (error) {
              g_warning("Could not take screenshot: %s (%s)", error->message, GST_STR_NULL(dbg));
              g_error_free(error);
            } else {
              g_warning("Could not take screenshot (and NULL error!)");
            }
            g_free(dbg);
            break;
          }
          default:{
            g_return_val_if_reached (NULL);
          }
        }
        gst_message_unref(msg);
      } else {
        g_warning("Could not take screenshot: %s", "timeout during conversion");
      }
    }
  }

  gst_element_set_state(pipeline, GST_STATE_NULL);



  gst_object_unref(bus);
  gst_object_unref(pipeline);

  return file;

/* ERRORS */
no_elements:
  {
    g_warning("Could not take screenshot: %s", error->message);
    g_error_free(error);
    return NULL;
  }
no_pipeline:
  {
    g_warning("Could not take screenshot: %s", "no pipeline (unknown error)");
    return NULL;
  }
no_tmpfile:
  {
    if (error)
      g_warning("Could not take screenshot: %s", error->message);
    else
      g_warning("Could not take screenshot: %s", "unable to create temporary file for output");
    if (fd_file != -1)
      close(fd_file);
    if (file)
      g_free(file);
    if (utf8_file)
      g_free(utf8_file);
    if (error)
      g_error_free(error);
    return NULL;
  }
link_failed:
  {
    g_warning("Could not take screenshot: %s", "failed to link elements");
    gst_object_unref (pipeline);
    if (file)
      g_free(file);
    if (utf8_file)
      g_free(utf8_file);
    return NULL;
  }
}

static GstBuffer*
last_buffer(App* app) 
{
  gboolean done;
  gboolean found;
  GstBuffer* buffer = NULL;
  GstElement* sink = NULL;
  GstIterator* iter;
  GstElement* element;

  done = FALSE;
  found = FALSE;
  iter = gst_bin_iterate_recurse(GST_BIN(app->pipeline));
    
  if (!iter)
    return NULL;

  while (!done) {
    switch(gst_iterator_next(iter, &element)) {
      case GST_ITERATOR_OK:
        /* Verify that we're looking at a sink, it has a factory, and it has a "last-frame" property. */
        if (GST_OBJECT_FLAG_IS_SET(element, GST_ELEMENT_IS_SINK)) {
          if (g_object_property_exists_of_type(G_OBJECT(element), "last-buffer", GST_TYPE_BUFFER)) {
            
            GST_OBJECT_LOCK(element);
            /* Increments buffer ref count */
            /* Be sure and have the caller unref the buffer! */
            g_object_get(element, "last-buffer", &buffer, NULL);
            GST_OBJECT_UNLOCK(element);

            /* Ensure we have a buffer. */
            if (buffer) {
              GstCaps* caps;
              const gchar* name;
              GstStructure* structure;

              caps = GST_BUFFER_CAPS(buffer);
              structure = gst_caps_get_structure(caps, 0);
              name = gst_structure_get_name(structure);

              /* Is this a video buffer? */
              if (g_str_has_prefix(name, "video/") || g_str_has_prefix(name, "image/")) {
                /* Increment the ref count so the unref below won't count */
                gst_buffer_ref(buffer);
                done = TRUE;
                found = TRUE;
              }

              /* Using g_object_get() will increase the buffer's ref count - so decrement it here. */
              gst_buffer_unref(buffer);
            }
          }
        }
        /* gst_iterator_next() increments the ref count. */
        gst_object_unref(element);
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync(iter);
        break;
      case GST_ITERATOR_ERROR:
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
  }

  gst_iterator_free(iter);

  /* If we didn't find a valid buffer, then get out of here. */
  if (!found)
    return NULL;

  return buffer;
}

static gchar* 
create_snapshot(App* app)
{
  gchar* ret = NULL;
  GstBuffer* buffer = NULL;

  buffer = last_buffer(app);
  if (!buffer)
    return NULL;

  ret = encode_buffer_to_tmp_file(buffer);

  /* last_buffer() returns a buffer that's been reffed once for the encoding phase. */
  gst_buffer_unref(buffer);

  return ret;
}

static GstBusSyncReply 
xoverlay_bus_sync_handler(GstBus* bus, GstMessage* message, App* app)
{
  /* Ignore anything but 'prepare-xwindow-id' element messages */
  if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_ELEMENT)
    return GST_BUS_PASS;
  if (!gst_structure_has_name(message->structure, "prepare-xwindow-id"))
    return GST_BUS_PASS;

  if (app->window_handle != 0) {
    GstXOverlay *xoverlay;

    /* GST_MESSAGE_SRC (message) will be the video sink element */
    xoverlay = GST_X_OVERLAY(GST_MESSAGE_SRC(message));
    gst_x_overlay_set_window_handle(xoverlay, (guintptr)app->window_handle);
  }

  gst_message_unref (message);
  return GST_BUS_DROP;
}


static gboolean
bus_message(GstBus* bus, GstMessage* message, App* app)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_SEGMENT_DONE:
    case GST_MESSAGE_EOS:
      {
eos:
        /* Handle repeats */
        if (GST_PLAYER_REPEAT_FOREVER(app->total_repeat_count) || (app->total_repeat_count > 0 && app->repeat_count < app->total_repeat_count)) {
          g_idle_add(app_repeat, app);
          break;
        }
        app->exiting = TRUE;
        g_main_loop_quit(app->main_loop);
        GST_PLAYER_EVENT("stopped");
        break;
      }
    case GST_MESSAGE_ERROR:
      {
        if (!app->exiting) {
          GError *err = NULL;
          gchar *name, *debug = NULL;

          name = gst_object_get_path_string(GST_MESSAGE_SRC(message));
          gst_message_parse_error(message, &err, &debug);

          GST_PLAYER_COMMAND_LOCK
          GST_PLAYER_GST_ERROR(err);
          GST_PLAYER_COMMAND_UNLOCK

          g_error_free(err);
          g_free(debug);
          g_free(name);

          app->exiting = TRUE;
          g_main_loop_quit(app->main_loop);
          //app->exiting = TRUE;
          //if (!(GST_PLAYER_REPEAT_FOREVER(app->total_repeat_count) || (app->total_repeat_count > 0 && app->repeat_count < app->total_repeat_count))) {
          //  g_main_loop_quit(app->main_loop);
          //} else {
          //  g_timeout_add(1000, app_repeat, app);
          //}
        }
        break;
      }
    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState old_state, new_state, pending_state;

      gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);

      /* we only care about pipeline state change messages */
      if (GST_MESSAGE_SRC(message) != GST_OBJECT_CAST(app->pipeline))
        break;

      /* ignore when we are buffering since then we mess with the states
       * ourselves. */
      if (app->buffering)
        break;

      GST_PLAYER_COMMAND_LOCK
      if (new_state == GST_STATE_PLAYING && old_state == GST_STATE_PAUSED) {
        gint64 position = 0;
        GstFormat format = GST_FORMAT_TIME;
        gst_element_query_position(app->pipeline, &format, &position);
        GST_PLAYER_EVENT("position, %" G_GINT64_FORMAT, (position > 0 ? position / GST_MSECOND : 0));
        GST_PLAYER_EVENT("playing");
      } else if (new_state == GST_STATE_PAUSED && pending_state == GST_STATE_VOID_PENDING) {
        gint64 position = 0;
        GstFormat format = GST_FORMAT_TIME;
        gst_element_query_position(app->pipeline, &format, &position);
        GST_PLAYER_EVENT("position, %" G_GINT64_FORMAT, (position > 0 ? position / GST_MSECOND : 0));
        GST_PLAYER_EVENT("paused");
      } else if (new_state == GST_STATE_READY && pending_state == GST_STATE_NULL) {
        app->repeat_count = 0;
        GST_PLAYER_EVENT("stopped");
      }
      GST_PLAYER_COMMAND_UNLOCK

      /* else not an interesting message */
      break;
    }
    case GST_MESSAGE_BUFFERING:
    {
      gint percent;

      /* no state management needed for live pipelines */
      if (app->is_live)
        break;

      gst_message_parse_buffering(message, &percent);

      GST_PLAYER_COMMAND_LOCK
      if (percent >= 100) {
        /* completed buffering, set to PLAYING. */
        if (app->buffering)
          gst_element_set_state(app->pipeline, GST_STATE_PLAYING);

        /* a 100% message means buffering is done */
        app->buffering = FALSE;
        app->last_percent = 100;
      } else {

        /* we were not buffering but PLAYING, PAUSE the pipeline. */
        if (!app->buffering) {
          gst_element_set_state(app->pipeline, GST_STATE_PAUSED);
          GST_PLAYER_EVENT("paused");
        }

        /* no need to output another event if the percentage hasn't changed */
        if (percent != app->last_percent)
          GST_PLAYER_EVENT("buffering, %d", percent);
        
        app->buffering = TRUE;
        app->last_percent = percent;
      }
      GST_PLAYER_COMMAND_UNLOCK
      break;
    }
    case GST_MESSAGE_LATENCY:
    {
      gst_bin_recalculate_latency(GST_BIN(app->pipeline));
      break;
    }
    case GST_MESSAGE_REQUEST_STATE:
    {
      GstState state;
      gchar *name;
      GST_PLAYER_COMMAND_LOCK
      name = gst_object_get_path_string(GST_MESSAGE_SRC(message));
      gst_message_parse_request_state(message, &state);
      gst_element_set_state(app->pipeline, state);
      g_free(name);
      GST_PLAYER_COMMAND_UNLOCK
      break;
    }
    case GST_MESSAGE_APPLICATION:
    {
      const GstStructure *s;
      s = gst_message_get_structure (message);
      if (gst_structure_has_name (s, "GstLaunchInterrupt")) {
        /* this application message is posted when we caught an interrupt and
         * we need to stop the pipeline. */
        break;
      }
    }
    default:
      break;
  }
exit:
  return TRUE;
}

static void 
playbin_element_added(GstBin* playbin, GstElement* element, App* app)
{
  gchar* factory_name;
  GstElementFactory* factory;
  if (!element)
    return;

  factory = gst_element_get_factory(element);
  if (!factory)
    return;

  factory_name = GST_PLUGIN_FEATURE_NAME(factory);
  if (!g_str_has_prefix(factory_name, "uridecodebin"))
    return;

  g_object_set(element, "download", (app->download_disabled ? FALSE : TRUE), NULL);
  g_object_set(element, "use-buffering", (app->buffering_disabled ? FALSE : TRUE), NULL);

  g_signal_connect(element, "pad-added", G_CALLBACK(uridecodebin_pad_added), app);
  g_signal_connect(element, "element-added", G_CALLBACK(uridecodebin_element_added), app);
}

static void 
uridecodebin_element_added(GstBin* uridecodebin, GstElement* element, App* app)
{
  gchar* factory_name;
  GstElementFactory* factory;
  if (!element)
    return;

  factory = gst_element_get_factory(element);
  if (!factory)
    return;

  factory_name = GST_PLUGIN_FEATURE_NAME(factory);

  if (g_str_has_prefix(factory_name, "decodebin")) {
    g_signal_connect(element, "element-added", G_CALLBACK(decodebin_element_added), app);
  }

  examine_element(factory_name, element, app);
}

static void 
decodebin_element_added(GstBin* decodebin, GstElement* element, App* app)
{
  gchar* factory_name;
  GstElementFactory* factory;

  if (!element)
    return;

  factory = gst_element_get_factory(element);
  if (!factory)
    return;

  factory_name = GST_PLUGIN_FEATURE_NAME(factory);

  {
    gboolean is_now_live = FALSE;
    gboolean was_live = app->is_live;

    if (g_str_has_prefix(factory_name, "multipartdemux")) {
      app->has_multipartdemux = TRUE;
    } else if (g_str_has_prefix(factory_name, "jpegdec")) {
      app->has_jpegdec = TRUE;
    }

    examine_element(factory_name, element, app);

    /* We reevaluate if this is a live pipeline by determining if the user explicitly set it or if they have all the trappings of one. */
    app->is_live = is_now_live = was_live || (app->has_multipartdemux && app->has_jpegdec && !app->has_fps);

    /* If we've determined that this is a live pipeline, then reexamine the elements and apply properties that may fit now. */
    examine_elements(GST_BIN(app->pipeline), app);

    /* If we previously thought that the pipeline wasn't live, then we may be buffering. Instruct the pipeline to continue. */
    if (!was_live && is_now_live)
      gst_element_set_state(app->pipeline, GST_STATE_PLAYING);
  }
}

static void 
uridecodebin_pad_added(GstElement* element, GstPad* pad, App* app)
{
  if (gst_pad_is_linked(pad))
    return;

  {
    const gchar* name;
    GstCaps* caps;
    GstStructure* structure;
    caps = gst_pad_get_caps_reffed(pad);
    structure = gst_caps_get_structure(caps, 0);
    name = gst_structure_get_name(structure);

    if (g_str_has_prefix(name, "audio/")) {
      if (app->first_audio_round) {
        GstPad* sink_pad;
        GstPad* audio_bin_ghost_pad;
        GstElement *audio_bin;
        GstElement *audio_volume, *audio_convert, *audio_resample, *audio_scaletempo, *audio_convert_after_scaletempo, *audio_resample_after_scaletempo;

        app->first_audio_round = FALSE;
        app->media_type |= MEDIA_TYPE_AUDIO;

        /* Setup a bin w/ ghost pads containing a number of elements we'll need. */
        audio_bin = gst_bin_new("audio_bin");

        audio_volume = gst_element_factory_make("volume", NULL);
        audio_convert = gst_element_factory_make("audioconvert", NULL);
        audio_resample = gst_element_factory_make("audioresample", NULL);
        audio_scaletempo = gst_element_factory_make("scaletempo", NULL);
        audio_convert_after_scaletempo = gst_element_factory_make("audioconvert", NULL);
        audio_resample_after_scaletempo = gst_element_factory_make("audioresample", NULL);

        gst_bin_add_many(GST_BIN(audio_bin), audio_volume, audio_convert, audio_resample, audio_scaletempo, audio_convert_after_scaletempo, audio_resample_after_scaletempo, app->audio_sink, NULL);
        gst_element_link_many(audio_volume, audio_convert, audio_resample, audio_scaletempo, audio_convert_after_scaletempo, audio_resample_after_scaletempo, app->audio_sink, NULL);

        sink_pad = gst_element_get_static_pad(audio_volume, "sink");
        audio_bin_ghost_pad = gst_ghost_pad_new("sink", sink_pad);
        gst_element_add_pad(audio_bin, audio_bin_ghost_pad);
        gst_object_unref(sink_pad);

        app->audio_sink = audio_bin;

        g_object_set(app->playbin, "audio-sink", app->audio_sink, NULL);

        sink_pad = gst_element_get_static_pad(app->audio_sink, "sink");
        if (sink_pad) {
          g_signal_connect(sink_pad, "notify::caps", G_CALLBACK(notify_audio_caps), app);
          gst_object_unref(sink_pad);
        }
      }

    } else if (g_str_has_prefix(name, "video/")) {
      /* We need to do this only once since (for now) we only play the same media in the same process. */
      /* TODO: Support switching pipelines using commands instead of the command line. */
      if (app->first_video_round) {
        GstCaps* caps_filter;
        MediaType media_type;
        GstPad* sink_pad;
        GstPad* video_bin_ghost_pad;
        GstElement *video_bin;
        GstElement *video_scale, *video_colorspace, *video_capsfilter;

        app->first_video_round = FALSE;

        /* Determine if this is a still image or video we're looking at. */
        media_type = determine_media_type(GST_BIN(element));

        switch(media_type) {
          case MEDIA_TYPE_VIDEO:
            app->media_type |= MEDIA_TYPE_VIDEO;
            break;
          case MEDIA_TYPE_STILL:
            app->media_type |= MEDIA_TYPE_STILL;
            break;
        }

        /* Setup a bin w/ ghost pads containing a number of elements we'll need. */

        video_bin = gst_bin_new("video_bin");

        caps_filter = create_app_caps(app);

        video_scale = gst_element_factory_make("videoscale", NULL);
        video_colorspace = gst_element_factory_make("ffmpegcolorspace", NULL);
        video_capsfilter = gst_element_factory_make("capsfilter", NULL);

        g_object_set(video_scale, "method", VIDEO_SCALE_METHOD_BILINEAR, NULL);
        g_object_set(video_capsfilter, "caps", caps_filter, NULL);

        gst_caps_unref(caps_filter);

        if (!app->is_live) {
          GstElement *video_rate;

          video_rate = gst_element_factory_make("videorate", NULL);
          
          gst_bin_add_many(GST_BIN(video_bin), video_colorspace, video_rate, video_scale, video_capsfilter, app->video_sink, NULL);
          gst_element_link_many(video_colorspace, video_rate, video_scale, video_capsfilter, app->video_sink, NULL);
        } else {
          gst_bin_add_many(GST_BIN(video_bin), video_colorspace, video_scale, video_capsfilter, app->video_sink, NULL);
          gst_element_link_many(video_colorspace, video_scale, video_capsfilter, app->video_sink, NULL);
        }

        sink_pad = gst_element_get_static_pad(video_colorspace, "sink");
        video_bin_ghost_pad = gst_ghost_pad_new("sink", sink_pad);
        gst_element_add_pad(video_bin, video_bin_ghost_pad);
        gst_object_unref(sink_pad);

        app->video_sink = video_bin;

        /* If we have a still image, then we need to inject an image freeze element */
        if (GST_PLAYER_MEDIA_IS_STILL(app->media_type)) {
          GstPad* sink_pad;
          GstPad* image_bin_ghost_pad;
          GstElement* image_bin;
          GstElement* imagefreeze;

          imagefreeze = gst_element_factory_make("imagefreeze", NULL);

          /* Did we succeed in creating an imagefreeze element? */
          if (imagefreeze) {
            image_bin = gst_bin_new(NULL);
            
            gst_bin_add_many(GST_BIN(image_bin), imagefreeze, app->video_sink, NULL);
            gst_element_link_many(imagefreeze, app->video_sink, NULL);

            sink_pad = gst_element_get_static_pad(imagefreeze, "sink");
            image_bin_ghost_pad = gst_ghost_pad_new("sink", sink_pad);
            gst_element_add_pad(image_bin, image_bin_ghost_pad);
            gst_object_unref(sink_pad);

            app->video_sink = image_bin;
          }
        }
        g_object_set(app->playbin, "video-sink", app->video_sink, NULL);

        examine_videosinks(GST_BIN(app->video_sink), app);
        
        sink_pad = gst_element_get_static_pad(app->video_sink, "sink");
        if (sink_pad) {
          g_signal_connect(sink_pad, "notify::caps", G_CALLBACK(notify_video_caps), app);
          gst_object_unref(sink_pad);
        }
      }
    }
    gst_caps_unref(caps);
  }
}

static void 
notify_audio_caps(GstPad* pad, GstPad* peer, App* app) 
{
  if (!GST_PLAYER_MEDIA_IS_AUDIO_ONLY(app->media_type))
    return;
  {
    gint64 position;
    gint64 duration;
    gboolean mute;
    gdouble volume;
    GstFormat format = GST_FORMAT_TIME;

    if (!gst_element_query_duration(app->pipeline, &format, &duration))
      duration = 0;
    if (!gst_element_query_position(app->pipeline, &format, &position))
      position = 0;

    if (app->playbin && g_object_property_exists(G_OBJECT(app->playbin), "volume")) {
      g_object_get(app->playbin, "volume", &volume, NULL);
      if (volume < 0.0)
        volume = 0.0;
      if (volume > 1.0)
        volume = 1.0;
    } else {
      volume = 1.0;
    }
    
    if (app->playbin && g_object_property_exists(G_OBJECT(app->playbin), "mute")) {
      g_object_get(app->playbin, "mute", &mute, NULL);
    } else {
      mute = FALSE;
    }

    GST_PLAYER_EVENT("seekable, %d", !app->is_live && duration > 0);
    GST_PLAYER_EVENT("live, %d", app->is_live);
    GST_PLAYER_EVENT("uri, %s", app->uri);
    GST_PLAYER_EVENT("volume, %g", volume);
    GST_PLAYER_EVENT("mute, %d", (mute ? 1 : 0));
    if (GST_PLAYER_MEDIA_IS_AUDIO(app->media_type))
      GST_PLAYER_EVENT("media_type, %s", "audio");
    GST_PLAYER_EVENT("rate, %f", app->rate);
    GST_PLAYER_EVENT("duration, %" G_GINT64_FORMAT, (duration > 0 ? duration / GST_MSECOND : 0));
    GST_PLAYER_EVENT("position, %" G_GINT64_FORMAT, (position > 0 ? position / GST_MSECOND : 0));
  }
}

static void 
notify_video_caps(GstPad* pad, GstPad* peer, App* app) 
{
  GstCaps* caps;
  gint64 position;
  gint64 duration;
  gboolean mute;
  gdouble volume;
  GstStructure* structure;
  GstFormat format = GST_FORMAT_TIME;

  caps = gst_pad_get_negotiated_caps(pad);
  if (!caps)
    return;
  if (gst_caps_get_size(caps) <= 0) {
    gst_caps_unref(caps);
    return;
  }
  structure = gst_caps_get_structure(caps, 0);
  if (!structure) {
    gst_caps_unref(caps);
    return;
  }

  if (gst_structure_has_field_typed(structure, "framerate", GST_TYPE_FRACTION)) {
    gint fps_n = 0;
    gint fps_d = 0;
    if (gst_structure_get_fraction(structure, "framerate", &fps_n, &fps_d))
      app->actual_fps = (fps_d > 0 ? ((gfloat)fps_n / (gfloat)fps_d) : 0.0);
  }

  if (gst_structure_has_field_typed(structure, "width", G_TYPE_INT) && gst_structure_has_field_typed(structure, "height", G_TYPE_INT)) {
    gint width = 0;
    gint height = 0;
    if (gst_structure_get_int(structure, "width", &width))
      app->actual_width = width;
    if (gst_structure_get_int(structure, "height", &height))
      app->actual_height = height;
  }

  gst_caps_unref(caps);

  if (!gst_element_query_duration(app->pipeline, &format, &duration))
    duration = 0;
  if (!gst_element_query_position(app->pipeline, &format, &position))
    position = 0;

  if (app->playbin && g_object_property_exists(G_OBJECT(app->playbin), "volume")) {
    g_object_get(app->playbin, "volume", &volume, NULL);
    if (volume < 0.0)
      volume = 0.0;
    if (volume > 1.0)
      volume = 1.0;
  } else {
    volume = 1.0;
  }
  
  if (app->playbin && g_object_property_exists(G_OBJECT(app->playbin), "mute")) {
    g_object_get(app->playbin, "mute", &mute, NULL);
  } else {
    mute = FALSE;
  }

  GST_PLAYER_EVENT("window_size, %d, %d", app->actual_width, app->actual_height);
  GST_PLAYER_EVENT("fps, %g", app->actual_fps);
  GST_PLAYER_EVENT("seekable, %d", !app->is_live && duration > 0);
  GST_PLAYER_EVENT("live, %d", app->is_live);
  GST_PLAYER_EVENT("uri, %s", app->uri);
  GST_PLAYER_EVENT("volume, %g", volume);
  GST_PLAYER_EVENT("mute, %d", (mute ? 1 : 0));
  if (GST_PLAYER_MEDIA_IS_AUDIO(app->media_type))
    GST_PLAYER_EVENT("media_type, %s", "audio");
  if (GST_PLAYER_MEDIA_IS_VIDEO(app->media_type))
    GST_PLAYER_EVENT("media_type, %s", "video");
  if (GST_PLAYER_MEDIA_IS_STILL(app->media_type))
    GST_PLAYER_EVENT("media_type, %s", "still");
  GST_PLAYER_EVENT("rate, %f", app->rate);
  GST_PLAYER_EVENT("duration, %" G_GINT64_FORMAT, (duration > 0 ? duration / GST_MSECOND : 0));
  GST_PLAYER_EVENT("position, %" G_GINT64_FORMAT, (position > 0 ? position / GST_MSECOND : 0));
}

static void
interpret_command(gint argc, Command cmd, gdouble arg0, gdouble arg1, gdouble arg2, gdouble arg3, App* app)
{
  switch(cmd) {
    case COMMAND_PAUSE:
      if (app->is_live)
          break;
      g_idle_add(app_pause, app);
      break;
    case COMMAND_CONTINUE:
      g_idle_add(app_continue, app);
      break;
    case COMMAND_STOP:
      g_idle_add(app_stop, app);
      break;
    case COMMAND_QUIT:
      gst_element_set_state(app->pipeline, GST_STATE_NULL);
      gst_element_send_event(app->pipeline, gst_event_new_eos());
      g_main_loop_quit(app->main_loop);
      break;
    case COMMAND_STEP_FORWARD:
      {
        gint64 position = 0; 
        gint64 duration = 0;
        GstFormat format;

        if (app->is_live)
          break;

        if (GST_PLAYER_MEDIA_IS_AUDIO_ONLY(app->media_type))
          break;

        //format = GST_FORMAT_BUFFERS;
        //gst_element_query_position(GST_ELEMENT(app->pipeline), &format, &position);
        //gst_element_query_duration(GST_ELEMENT(app->pipeline), &format, &duration);

        //if (position < duration && position >= 0 && duration > 0) {
          app_step(app, TRUE);

          format = GST_FORMAT_TIME;
          if (gst_element_query_position(app->pipeline, &format, &position))
            GST_PLAYER_EVENT("position, %" G_GINT64_FORMAT, (position > 0 ? position / GST_MSECOND : 0));
        //}
        break;
      }
    case COMMAND_STEP_BACKWARD:
      {
        /* TODO: Find an alternative method. 
                 For now, it steps backward by performing a seek to the calculated time 
                 one frame behind the current position. */
        gint64 position = 0;
        gint64 offset = 0;
        GstFormat format = GST_FORMAT_TIME;

        if (app->is_live)
          break;

        if (app->actual_fps <= 0.0)
          break;

        if (GST_PLAYER_MEDIA_IS_AUDIO_ONLY(app->media_type))
          break;

        app_pause(app);

        if (gst_element_query_position(app->pipeline, &format, &position)) {
          /* Calculate where it should be in the stream and seek there. */
          offset = (1000000000 / app->actual_fps);
          position = (position > offset ? position - offset : 0);

          /* Ensure that GST_SEEK_KEY_UNIT is absent so that we get accurate seeking to the frame. */
          if (gst_element_seek_simple(app->pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_SKIP | GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT, (position > 0 ? position : 0))) {
            if (gst_element_query_position(app->pipeline, &format, &position))
              GST_PLAYER_EVENT("position, %" G_GINT64_FORMAT, (position > 0 ? position / GST_MSECOND : 0));
          }
        }
        break;
      }
    case COMMAND_TIME_SEEK_FORWARD:
      {
        if (argc >= 1 && !app->is_live) {
          gint64 position = 0; 
          gint64 duration = 0;
          GstFormat format = GST_FORMAT_TIME;

          if (
                 gst_element_query_position(app->pipeline, &format, &position) 
              && gst_element_query_duration(app->pipeline, &format, &duration) 
              && position < duration
          ) {
            if (gst_element_seek_simple(app->pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SKIP | GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT, arg0 * GST_MSECOND)) {
              if (gst_element_query_position(app->pipeline, &format, &position)) {
                GST_PLAYER_EVENT("position, %" G_GINT64_FORMAT, (position > 0 ? position / GST_MSECOND : 0));
              }
            }
          }
        }
      }
      break;
    case COMMAND_TIME_SEEK_BACKWARD:
      break;
    case COMMAND_ADJUST_RATE:
      if (argc >= 1 && !app->is_live) {
        //GstEvent* evt;
        //gint64 position;
        //GstFormat format;
        //gboolean res = FALSE;

        ////gst_element_get_state(app->pipeline, &state, NULL, -1);
        //gst_element_set_state(app->pipeline, GST_STATE_PAUSED);
        //gst_element_get_state(app->pipeline, NULL, NULL, -1);

        //gst_element_query_position(app->pipeline, &format, &position);
        ////evt = gst_event_new_seek(arg0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT, ((arg0 >= 0.0) ? GST_SEEK_TYPE_SET : GST_SEEK_TYPE_SET), ((arg0 >= 0.0) ? position : G_GINT64_CONSTANT(0)), ((arg0 >= 0.0) ? GST_SEEK_TYPE_SET : GST_SEEK_TYPE_SET), ((arg0 >= 0.0) ? GST_CLOCK_TIME_NONE : position));
        //evt = gst_event_new_seek(arg0, GST_FORMAT_TIME, GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT, GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
        //if (evt) {
        //  gst_event_ref(evt);
        //  res = gst_element_send_event(app->pipeline, evt);
        //  gst_event_unref(evt);

        //  if (res) {
        //    gst_element_get_state(app->pipeline, NULL, NULL, SEEK_TIMEOUT);
        //  }
        //}
      }
      break;
    case COMMAND_ADJUST_VOLUME:
      if (argc >= 1) {
        if (app->playbin && g_object_property_exists(G_OBJECT(app->playbin), "volume")) {
          if (arg0 < 0.0)
            arg0 = 0.0;
          if (arg0 > 1.0)
            arg0 = 1.0;
          g_object_set(app->playbin, "volume", arg0, NULL);
          GST_PLAYER_EVENT("volume, %g", arg0);
        }
      }
      break;
    case COMMAND_MUTE:
      if (app->playbin && g_object_property_exists(G_OBJECT(app->playbin), "mute")) {
        g_object_set(app->playbin, "mute", TRUE, NULL);
        GST_PLAYER_EVENT("mute, %d", 1);
      }
      break;
    case COMMAND_UNMUTE:
      if (app->playbin && g_object_property_exists(G_OBJECT(app->playbin), "mute")) {
        g_object_set(app->playbin, "mute", FALSE, NULL);
        GST_PLAYER_EVENT("mute, %d", 0);
      }
      break;
    case COMMAND_TOGGLE_MUTE:
      if (argc >= 1) {
        if (app->playbin && g_object_property_exists(G_OBJECT(app->playbin), "mute")) {
          gboolean is_muted = FALSE;
          g_object_get(app->playbin, "mute", &is_muted, NULL);
          g_object_set(app->playbin, "mute", !is_muted, NULL);
          GST_PLAYER_EVENT("mute, %d", (!is_muted ? 1 : 0));
        }
      }
      break;
    case COMMAND_PING:
      GST_PLAYER_RESPONSE("ping");
      break;
    case COMMAND_SNAPSHOT:
      {
        gchar* path;
        if ((path = create_snapshot(app))) {
          GST_PLAYER_EVENT("snapshot, %s", path);
          g_free(path);
        }
      }
      break;
    case COMMAND_QUERY_POSITION:
      {
        gint64 position = 0;
        GstFormat format = GST_FORMAT_TIME;

        if (gst_element_query_position(app->pipeline, &format, &position)) {
          GST_PLAYER_RESPONSE("query_position, %" G_GINT64_FORMAT, (position > 0 ? position / GST_MSECOND : 0));
        } else {
          GST_PLAYER_RESPONSE("query_position, 0");
        }
      }
      break;
    case COMMAND_QUERY_DURATION:
      {
        gint64 duration = 0;
        GstFormat format = GST_FORMAT_TIME;

        if (gst_element_query_duration(app->pipeline, &format, &duration)) {
          GST_PLAYER_RESPONSE("query_duration, %" G_GINT64_FORMAT, (duration > 0 ? duration / GST_MSECOND : 0));
        } else {
          GST_PLAYER_RESPONSE("query_duration, 0");
        }
      }
      break;
    case COMMAND_QUERY_VOLUME:
      {
        gdouble volume = 1.0;

        if (app->playbin && g_object_property_exists(G_OBJECT(app->playbin), "volume")) {
          g_object_get(app->playbin, "volume", &volume, NULL);
          if (volume < 0.0)
            volume = 0.0;
          if (volume > 1.0)
            volume = 1.0;
        } else {
          volume = 1.0;
        }
        GST_PLAYER_RESPONSE("query_volume, %g", volume);
      }
      break;
    case COMMAND_QUERY_MUTE:
      {
        gboolean mute = FALSE;

        if (app->playbin && g_object_property_exists(G_OBJECT(app->playbin), "mute")) {
          g_object_get(app->playbin, "mute", &mute, NULL);
        } else {
          mute = FALSE;
        }
        GST_PLAYER_RESPONSE("query_mute, %d", (mute ? 1 : 0));
      }
      break;
    case COMMAND_EXPOSE: 
      {
        if (GST_PLAYER_MEDIA_IS_AUDIO_ONLY(app->media_type))
          break;

        g_idle_add(app_expose, app);
      }
      break;
  }
}
static void
process_commands(App* app)
{
  gchar* line;
  gchar cmd[256];
  gdouble arg0;
  gdouble arg1;
  gdouble arg2;
  gdouble arg3;
  gint argc;
  Command command;

  GST_PLAYER_COMMAND_LOCK
  cmd_processor_is_started = TRUE;
  g_cond_signal(cmd_processor_started);
  GST_PLAYER_COMMAND_UNLOCK

  while(!app->cmd_processor_please_exit)
  {
    if (!(line = readline(stdin)))
      break;

    if ((argc = sscanf(line, "%255s %lf %lf %lf %lf", &cmd, &arg0, &arg1, &arg2, &arg3)) <= 0 || argc == EOF)
      continue;

    /* Decrement by one for the cmd */
    --argc;

    if (GST_PLAYER_STR_EQUALS(cmd, "PAUSE")) {
      command = COMMAND_PAUSE;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "CONTINUE")) {
      command = COMMAND_CONTINUE;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "PLAY")) {
      command = COMMAND_CONTINUE;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "STOP")) {
      command = COMMAND_STOP;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "QUIT")) {
      command = COMMAND_QUIT;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "EXIT")) {
      command = COMMAND_QUIT;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "STEP_FORWARD")) {
      command = COMMAND_STEP_FORWARD;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "STEP_BACKWARD")) {
      command = COMMAND_STEP_BACKWARD;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "TIME_SEEK")) {
      command = COMMAND_TIME_SEEK_FORWARD;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "TIME_SEEK_FORWARD")) {
      command = COMMAND_TIME_SEEK_FORWARD;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "TIME_SEEK_BACKWARD")) {
      command = COMMAND_TIME_SEEK_BACKWARD;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "ADJUST_RATE")) {
      command = COMMAND_ADJUST_RATE;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "RATE")) {
      command = COMMAND_ADJUST_RATE;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "ADJUST_VOLUME")) {
      command = COMMAND_ADJUST_VOLUME;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "VOLUME")) {
      command = COMMAND_ADJUST_VOLUME;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "MUTE")) {
      command = COMMAND_MUTE;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "UNMUTE")) {
      command = COMMAND_UNMUTE;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "TOGGLE_MUTE")) {
      command = COMMAND_TOGGLE_MUTE;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "PING")) {
      command = COMMAND_PING;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "SNAPSHOT")) {
      command = COMMAND_SNAPSHOT;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "QUERY_POSITION")) {
      command = COMMAND_QUERY_POSITION;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "QUERY_DURATION")) {
      command = COMMAND_QUERY_DURATION;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "QUERY_VOLUME")) {
      command = COMMAND_QUERY_VOLUME;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "QUERY_MUTE")) {
      command = COMMAND_QUERY_MUTE;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "EXPOSE")) {
      command = COMMAND_EXPOSE;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "REPAINT")) {
      command = COMMAND_EXPOSE;
    } else if (GST_PLAYER_STR_EQUALS(cmd, "PAINT")) {
      command = COMMAND_EXPOSE;
    } else {
      command = COMMAND_UNKNOWN;
    }
    
    g_idle_add(process_app_command, create_app_command(argc, command, arg0, arg1, arg2, arg3, app));
    
    g_free(line);
  }
}

static AppCommand* 
create_app_command(gint argc, Command cmd, gdouble arg0, gdouble arg1, gdouble arg2, gdouble arg3, App* app)
{
  AppCommand* p;

  p = g_new0(AppCommand, 1);
  p->argc = argc;
  p->cmd = cmd;
  p->arg0 = arg0;
  p->arg1 = arg1;
  p->arg2 = arg2;
  p->arg3 = arg3;
  p->data = app;

  return p;
}

static gboolean 
process_app_command(AppCommand* p)
{
  GST_PLAYER_COMMAND_LOCK
  interpret_command(p->argc, p->cmd, p->arg0, p->arg1, p->arg2, p->arg3, p->data);
  GST_PLAYER_COMMAND_UNLOCK
  
  /* Cleanup resources. */  
  g_free(p);

  /* Indicates that we want this executed only once. */
  return FALSE;
}

static void 
ping(App* app)
{
  gulong ping_interval;

  ping_interval = app->ping_interval * (G_USEC_PER_SEC / 1000);

  GST_PLAYER_PING_LOCK
  ping_is_started = TRUE;
  g_cond_signal(ping_started);
  GST_PLAYER_PING_UNLOCK

  while(!app->ping_please_exit)
  {
    /* We attempt to grab the lock here in case a command is */
    /* blocking elsewhere. Programs controlling this one can */
    /* determine if we've deadlocked if interpret_command() */
    /* hasn't returned (and thus hasn't unlocked the mutex) */
    /* because we won't be emitting a ping message within the */
    /* allowed time. */
    GST_PLAYER_COMMAND_LOCK
    GST_PLAYER_EVENT("ping");
    GST_PLAYER_COMMAND_UNLOCK
    g_usleep(ping_interval);
  }
}

static void 
sigint_handler(App* app)
{
  if (!app)
    return;
  g_main_loop_quit(app->main_loop);
}

#ifdef G_OS_WIN32
static BOOL CtrlHandler(DWORD fdwCtrlType){
 switch(fdwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
      sigint_handler(&g_app);
      return TRUE;
    case CTRL_BREAK_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      return FALSE;
    default:
      return FALSE;
  }
  return FALSE;
}
#endif

static void 
setup_sigint_handler(App* app) 
{
#ifdef G_OS_WIN32
  /* Setup the CTRL+C handler. */
  SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif
}

static void setup_error_reporting() 
{
#ifdef G_OS_WIN32
#if _WIN32_WINNT >= _WIN32_WINNT_WIN2K
  /* Request that the Windows error reporting dialog remain hidden if the app crashes. */
  SetErrorMode(SEM_NOGPFAULTERRORBOX);
#endif
#endif
}

void GPoolTest(gpointer       data, gpointer       user_data)
{

}

int
main (int argc, char *argv[])
{
  App* app = &g_app;
  gchar *audio_sink_desc = DEFAULT_AUDIO_SINK;
  gchar *video_sink_desc = DEFAULT_VIDEO_SINK;
  gchar *uri = NULL;
  gboolean live = FALSE;
  gdouble volume = 1.0;
  gboolean mute = FALSE;
  gboolean has_fps = FALSE;
  gboolean repeat_forever = FALSE;
  gboolean command_mode = FALSE;
  gboolean disable_buffering = FALSE;
  gboolean disable_download = FALSE;
  gboolean disable_auto_sync = FALSE;
  gint fps = 0;
  gint fps_n = 0;
  gint fps_d = 0;
  gint width = 0;
  gint height = 0;
  gint repeat_count = 0;
  gint buffer_size = -1;
  gint64 buffer_duration = -1;
  guintptr window_handle = 0;
  guint64 ping_interval = 0;
  GError *err = NULL;
  GOptionContext *ctx;
  GstState state, pending;
  GstStateChangeReturn ret;
  gint res = 0;
//  GThreadPool* gtest;

  GOptionEntry options[] = {
    {"window-handle",      'w', 0, G_OPTION_ARG_INT64,  &window_handle,     N_("Set the window handle in which to render video output"), NULL},
    {"uri",                'u', 0, G_OPTION_ARG_STRING, &uri,               N_("The URI to play media from (in playbin syntax)"), NULL},
    {"fps",                'f', 0, G_OPTION_ARG_INT,    &fps,               N_("Set the frames-per-second for video playback"), NULL},
    {"mute",               'm', 0, G_OPTION_ARG_NONE,   &mute,              N_("Mute all audio output"), NULL},
    {"live",               'l', 0, G_OPTION_ARG_NONE,   &live,              N_("Assume the source is live"), NULL},
    {"volume",             'v', 0, G_OPTION_ARG_DOUBLE, &volume,            N_("Set the volume for all audio output"), NULL},
    {"command-mode",       'c', 0, G_OPTION_ARG_NONE,   &command_mode,      N_("Accept and respond to commands through stdin/stdout"), NULL},
    {"repeat-count",       'r', 0, G_OPTION_ARG_INT,    &repeat_count,      N_("Set the number of times the media should play"), NULL},
    {"repeat-forever",       0, 0, G_OPTION_ARG_NONE,   &repeat_forever,    N_("Continuously replay the media until the window is forcibly closed or the application exits"), NULL},
    {"fps-n",                0, 0, G_OPTION_ARG_INT,    &fps_n,             N_("Set the frames-per-second numerator for video playback"), NULL},
    {"fps-d",                0, 0, G_OPTION_ARG_INT,    &fps_d,             N_("Set the frames-per-second denominator for video playback"), NULL},
    {"width",                0, 0, G_OPTION_ARG_INT,    &width,             N_("Force a width for video playback"), NULL},
    {"height",               0, 0, G_OPTION_ARG_INT,    &height,            N_("Force a height for video playback"), NULL},
    {"audio-sink",           0, 0, G_OPTION_ARG_STRING, &audio_sink_desc,   N_("Set the audio sink used by the playbin element"), NULL},
    {"video-sink",           0, 0, G_OPTION_ARG_STRING, &video_sink_desc,   N_("Set the video sink used by the playbin element"), NULL},
    {"buffer-size",          0, 0, G_OPTION_ARG_INT,    &buffer_size,       N_("Set the playbin buffer-size in bytes"), NULL},
    {"buffer-duration",      0, 0, G_OPTION_ARG_INT64,  &buffer_duration,   N_("Set the playbin buffer-duration in nanoseconds"), NULL},
    {"ping-interval",        0, 0, G_OPTION_ARG_INT64,  &ping_interval,     N_("Set the interval in between automatic ping messages (requires command mode)"), NULL},
    {"disable-buffering",    0, 0, G_OPTION_ARG_NONE,   &disable_buffering, N_("Disables all buffering"), NULL},
    {"disable-download",     0, 0, G_OPTION_ARG_NONE,   &disable_download,  N_("Disables download mode"), NULL},
    {"disable-auto-sync",    0, 0, G_OPTION_ARG_NONE,   &disable_auto_sync, N_("Disables automatically determining if video sinks should enable sync or not"), NULL},
    GST_TOOLS_GOPTION_VERSION,
    {NULL}
  };

  /* Setup error reporting */
  setup_error_reporting();

  /* Clear app */
  memset(app, 0, sizeof(App));

  /* Init threads and set the program name */
  g_thread_init(NULL);


  gst_tools_set_prgname(DEFAULT_PROGRAM_NAME);
  
  /* Parse user's options */
  ctx = g_option_context_new("");
  g_option_context_add_main_entries(ctx, options, GETTEXT_PACKAGE);
  g_option_context_add_group(ctx, gst_init_get_option_group());
  if (!g_option_context_parse(ctx, &argc, &argv, &err)) {
    GST_PLAYER_GST_ERROR_MSG("Initialization error", err);
    exit(ERROR_INITIALIZATION);
  }
  g_option_context_free (ctx);

  /* Turn off normal error messages */
  gst_debug_set_default_threshold(GST_LEVEL_NONE);
  //gst_debug_set_active(FALSE);

  /* Print version info. if the user requested it */
  gst_tools_print_version(DEFAULT_PROGRAM_NAME);

  
  /* Double check provided parameters */

  if (volume > 1.0)
    volume = 1.0;
  if (volume < 0.0)
    volume = 0.0;

  if (fps > 0) {
    fps_n = fps;
    fps_d = 1;
    has_fps = TRUE;
  }

  if (fps_n <= 0)
    fps_n = 1;
  else
    has_fps = TRUE;

  if (fps_d <= 0)
    fps_d = 1;
  else
    has_fps = TRUE;

  if (ping_interval < 0)
    ping_interval = 0;

  if (ping_interval > 0 && !command_mode) {
    GST_PLAYER_PLAIN_ERROR(0, "Initialization error", "Ping intervals require command mode. Please enable --command-mode and try again.");
    exit(ERROR_INITIALIZATION);
  }

  if (!audio_sink_desc || strlen(audio_sink_desc) <= 0)
    audio_sink_desc = DEFAULT_AUDIO_SINK;
  if (!video_sink_desc || strlen(video_sink_desc) <= 0)
    video_sink_desc = DEFAULT_VIDEO_SINK;

  if (!uri || strlen(uri) <= 0) {
    GST_PLAYER_PLAIN_ERROR(0, "Initialization error", "Missing URI");
    exit(ERROR_INITIALIZATION);
  }

  /* Create a bin out of the provided audio/video sinks */

  app->audio_sink = 0;//gst_parse_bin_from_description(audio_sink_desc, TRUE, &err);
//   if (!app->audio_sink) {
//     GST_PLAYER_GST_ERROR_MSG("Initialization error", err);
//     exit(ERROR_INITIALIZATION);
//   }

  app->video_sink = 0;//gst_element_factory_make(video_sink_desc, "d3d_videosink_1"); //gst_parse_bin_from_description(video_sink_desc, TRUE, &err);
//   if (!app->video_sink) {
//     GST_PLAYER_GST_ERROR_MSG("Initialization error", err);
//     exit(ERROR_INITIALIZATION);
//   }

  app->auto_sync_disabled = disable_auto_sync;
  app->buffering_disabled = disable_buffering;
  app->download_disabled = disable_download;
  app->command_mode = command_mode;
  app->window_handle = window_handle;
  app->is_live = live;
  app->width = width;
  app->height = height;
  app->buffer_size = buffer_size;
  app->buffer_duration = buffer_duration;
  app->ping_interval = ping_interval;
  app->fps_n = fps_n;
  app->fps_d = fps_d;
  app->has_fps = has_fps;
  app->has_multipartdemux = FALSE;
  app->has_jpegdec = FALSE;
  app->first_audio_round = TRUE;
  app->first_video_round = TRUE;
  app->repeat_count = 0;
  if (!repeat_forever)
    app->total_repeat_count = repeat_count;
  else
    app->total_repeat_count = -1;
  app->media_type = MEDIA_TYPE_UNKNOWN;
  app->actual_fps = 0.0;
  app->actual_width = 0;
  app->actual_height = 0;
  app->rate = FORWARD_RATE;
  app->uri = uri;
  app->pipeline = NULL;
  app->playbin = NULL;

  /* Begin actual work */

  /* Create a mainloop to get messages */
  app->main_loop = g_main_loop_new (NULL, TRUE);

  /* Support two different kinds of pipelines. One uses playbin2 and the other is a simple pipeline for displaying test patterns. */

  if (!g_str_has_prefix(uri, "local://pattern/")) {
    app->pipeline = gst_pipeline_new("pipeline");
    g_assert(app->pipeline);

    app->playbin = gst_element_factory_make("playbin2", NULL);
    g_assert(app->playbin);

    gst_bin_add_many(GST_BIN(app->pipeline), app->playbin, NULL);
    //gst_element_link_many(app->playbin, NULL);
    
    /* Connect to signals to set properties on elements that are auto-selected */
    g_signal_connect(app->playbin, "element-added", G_CALLBACK(playbin_element_added), app);

    g_object_set(
      app->playbin, 
      "uri",             uri, 
      "mute",            mute, 
      "volume",          volume, 
      "audio-sink",      app->audio_sink,
      "video-sink",      app->video_sink, 
      "buffer-size",     buffer_size, 
      "buffer-duration", buffer_duration, 
      NULL
    );
  } else {
    GstCaps *caps;
    GstPad *sink_pad;
    VideoTestSrcPattern pattern = VIDEO_TEST_SRC_PATTERN_SMPTE;
    GstElement *video_testsrc, *video_rate, *video_colorspace, *video_capsfilter, *video_scale;

    caps = create_app_pattern_caps(app);
    
    if (g_str_has_suffix(uri, "SMPTE"))
      pattern = VIDEO_TEST_SRC_PATTERN_SMPTE;
    else if (g_str_has_suffix(uri, "BLACK"))
      pattern = VIDEO_TEST_SRC_PATTERN_BLACK;

    app->pipeline = gst_pipeline_new("pipeline");
    g_assert(app->pipeline);

    video_testsrc = gst_element_factory_make("videotestsrc", NULL);
    video_rate = gst_element_factory_make("videorate", NULL);
    video_colorspace = gst_element_factory_make("ffmpegcolorspace", NULL);
    video_capsfilter = gst_element_factory_make("capsfilter", NULL);
    video_scale = gst_element_factory_make("videoscale", NULL);
    
    g_object_set(video_testsrc, "pattern", (gint)pattern, NULL);
    g_object_set(video_rate, "silent", TRUE, NULL);
    g_object_set(video_capsfilter, "caps", caps, NULL);

    gst_bin_add_many(GST_BIN(app->pipeline), video_testsrc, video_capsfilter, video_colorspace, video_rate, video_scale, app->video_sink, NULL);
    gst_element_link_many(video_testsrc, video_capsfilter, video_colorspace, video_rate, video_scale, app->video_sink, NULL);

    gst_caps_unref(caps);

    sink_pad = gst_element_get_static_pad(app->video_sink, "sink");
    g_signal_connect(sink_pad, "notify::caps", G_CALLBACK(notify_video_caps), app);
    gst_object_unref(sink_pad);

    /* Set the app media type -- we already know it since we're creating the full pipeline. */
    app->media_type = MEDIA_TYPE_VIDEO;
  }

  app->bus = gst_pipeline_get_bus(GST_PIPELINE(app->pipeline));
  g_assert(app->bus);
  
  /* Add watch for messages and a sync handler to set the window id for xoverlay */
  gst_bus_add_watch(app->bus, (GstBusFunc)bus_message, app);
  gst_bus_set_sync_handler(app->bus, (GstBusSyncHandler)xoverlay_bus_sync_handler, app);

  /* Spin up thread to read from stdin */
  app->cmd_processor_thread = NULL;
  app->cmd_processor_please_exit = FALSE;
  if (app->command_mode) {
    cmd_processor_is_started = FALSE;
    cmd_processor_started = g_cond_new();

    app->cmd_processor_thread = g_thread_create((GThreadFunc)process_commands, app, TRUE, NULL);
    if (!app->cmd_processor_thread) {
      g_cond_free(cmd_processor_started);
      goto end;
    }

    /* Wait for thread to start */
    GST_PLAYER_COMMAND_LOCK
    while(!cmd_processor_is_started)
      g_cond_wait(cmd_processor_started, g_static_mutex_get_mutex(&cmd_lock));
    GST_PLAYER_COMMAND_UNLOCK

    /* Cleanup the condition */
    g_cond_free(cmd_processor_started);
  }

  /* Spin up thread to ping periodically */
  if (app->ping_interval > 0 && app->command_mode) {
    app->ping_thread = NULL;
    app->ping_please_exit = FALSE;
    ping_is_started = FALSE;
    ping_started = g_cond_new();

    app->ping_thread = g_thread_create((GThreadFunc)ping, app, TRUE, NULL);
    if (!app->ping_thread) {
      g_cond_free(ping_started);
      goto end;
    }

    /* Wait for thread to start */
    GST_PLAYER_PING_LOCK
    while(!ping_is_started)
      g_cond_wait(ping_started, g_static_mutex_get_mutex(&ping_lock));
    GST_PLAYER_PING_UNLOCK

    /* Cleanup the condition */
    g_cond_free(ping_started);
  }

  setup_sigint_handler(app);

  /* No need to cleanup cmd_lock -- it's a global, static mutex */

  /* Move to paused */
  ret = app_play(app);

  /* this mainloop is stopped when we receive an error or EOS */
  g_main_loop_run (app->main_loop);

  /* Move to paused */
  gst_element_set_state(app->pipeline, GST_STATE_PAUSED);

  /* iterate mainloop to process pending stuff */
  while (g_main_context_iteration(NULL, FALSE));

  gst_element_set_state(app->pipeline, GST_STATE_READY);
  gst_element_get_state(app->pipeline, &state, &pending, GST_CLOCK_TIME_NONE);

end:
  gst_element_set_state(app->pipeline, GST_STATE_NULL);
  gst_element_get_state(app->pipeline, &state, &pending, GST_CLOCK_TIME_NONE);

  gst_object_unref(app->bus);

  gst_deinit();

  app->exiting = TRUE;
  if (app->cmd_processor_thread) {
    app->cmd_processor_please_exit = TRUE;
  }

  GST_PLAYER_EVENT("quit");
  
  return res;
}
