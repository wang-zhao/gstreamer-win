/* GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
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
 * SECTION:element-camerabin2
 *
 * CameraBin2 is a high-level camera object that encapsulates gstreamer
 * elements, providing an API for controlling a digital camera.
 *
 * <note>
 * Note that camerabin2 is still UNSTABLE and under
 * development.
 * </note>
 *
 * CameraBin2 has the following main features:
 * <itemizedlist>
 * <listitem>
 * Record videos
 * </listitem>
 * <listitem>
 * Capture pictures
 * </listitem>
 * <listitem>
 * Display a viewfinder
 * </listitem>
 * <listitem>
 * Post preview images for each capture (video and image)
 * </listitem>
 * </itemizedlist>
 *
 * <refsect2>
 * <title>Usage</title>
 * <para>
 * Camerabin2 can be created using gst_element_factory_make() just like
 * any other element. Video or image capture mode can be selected using
 * the #GstCameraBin2:mode property and the file to save the capture is
 * selected using #GstCameraBin2:location property.
 *
 * After creating camerabin2, applications might want to do some
 * customization (there's a section about this below), then select
 * the desired mode and start capturing.
 *
 * In image capture mode, just send a #GstCameraBin:start-capture and a
 * picture will be captured. When the picture is stored on the selected
 * location, a %GST_MESSAGE_ELEMENT named 'image-done' will be posted on
 * the #GstBus.
 *
 * In video capture mode, send a #GstCameraBin2:start-capture to start
 * recording, then send a #GstCameraBin2:stop-capture to stop recording.
 * Note that both signals are asynchronous, so, calling
 * #GstCameraBin2:stop-capture doesn't guarantee that the video has been
 * properly finished yet. Applications should wait for the 'video-done'
 * message to be posted on the bus.
 *
 * In both modes, if #GstCameraBin2:post-previews is %TRUE, a #GstBuffer
 * will be post to the #GstBus in a field named 'buffer', in a
 * 'preview-image' message of type %GST_MESSAGE_ELEMENT.
 * </para>
 * </refsect2>

 * <refsect2>
 * <title>Customization</title>
 * <para>
 * Camerabin2 provides various customization properties, allowing the user
 * to set custom filters, selecting the viewfinder sink and formats to
 * use to encode the captured images/videos.
 *
 * #GstEncodingProfile<!-- -->s are used to tell camerabin2 which formats it
 * should encode the captures to, those should be set to
 * #GstCameraBin2:image-profile and #GstCameraBin2:video-profile. Default is
 * jpeg for images, and ogg (theora and vorbis) for video. If a profile without
 * an audio stream is set for video, audio will be disabled on recordings.
 *
 * #GstCameraBin2:preview-caps can be used to select which format preview
 * images should be posted on the #GstBus. It has to be a raw video format.
 *
 * Camerabin2 has a #GstCameraBin2:camera-source property so applications can
 * set their source that will provide buffers for the viewfinder and for
 * captures. This camera source is a special type of source that has 3 pads.
 * To use a 'regular' source with a single pad you should use
 * #GstWrapperCameraBinSource, it will adapt your source and provide 3 pads.
 *
 * Applications can also select the desired viewfinder sink using
 * #GstCameraBin2:viewfinder-sink, it is also possible to select the audio
 * source using #GstCameraBin2:audio-source.
 *
 * The viewfinder resolution can be configured using
 * #GstCameraBin2:viewfinder-caps, these #GstCaps should be a subset of
 * #GstCameraBin2:viewfinder-supported-caps.
 *
 * To select the desired resolution for captures, camerabin2 provides
 * #GstCameraBin2:image-capture-caps and #GstCameraBin2:video-capture-caps,
 * these caps must be a subset of what the source can produce. The allowed
 * caps can be probed using #GstCameraBin2:image-capture-supported-caps and
 * #GstCameraBin2:video-capture-supported-caps. In an analogous way, there
 * are #GstCameraBin2:audio-capture-caps and
 * #GstCameraBin2:audio-capture-supported-caps.
 *
 * Camerabin2 also allows applications to insert custom #GstElements on any
 * of its branches: video capture, image capture, viewfinder and preview.
 * Check #GstCameraBin2:video-filter, #GstCameraBin2:image-filter,
 * #GstCameraBin2:viewfinder-filter and #GstCameraBin2:preview-filter.
 * </para>
 * </refsect2>
 *
 * <refsect2>
 * <title>Example launch line</title>
 * <para>
 * Unfortunatelly, camerabin2 can't be really used from gst-launch, as you need
 * to send signals to control it. The following pipeline might be able
 * to show the viewfinder using all the default elements.
 * |[
 * gst-launch -v -m camerabin2
 * ]|
 * </para>
 * </refsect2>

 */

/*
 * Detail Topics:
 *
 * videorecordingbin state management (for now on called 'videobin')
 * - The problem: keeping videobin state in sync with camerabin will make it
 *                go to playing when it might not be used, causing its internal
 *                filesink to open a file that might be left blank.
 * - The solution: videobin state is set to locked upon its creation and camerabin
 *                 registers itself on the notify::ready-for-capture of the src.
 *                 Whenever the src readyness goes to FALSE it means a new
 *                 capture is starting. If we are on video mode, the videobin's
 *                 state is set to NULL and then PLAYING (in between this we
 *                 have room to set the destination filename).
 *                 There is no problem to leave it on playing after an EOS, so
 *                 no action is taken on stop-capture.
 *
 * - TODO: What happens when an error pops?
 * - TODO: Should we split properties in image/video variants? We already do so
 *         for some of them
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/basecamerabinsrc/gstbasecamerasrc.h>
#include "gstcamerabin2.h"
#include <gst/gst-i18n-plugin.h>
#include <gst/pbutils/pbutils.h>

#if GLIB_CHECK_VERSION(2,29,6)
#define gst_camerabin2_atomic_int_add g_atomic_int_add
#else
#define gst_camerabin2_atomic_int_add g_atomic_int_exchange_and_add
#endif

#define GST_CAMERA_BIN2_PROCESSING_INC(c)                                \
{                                                                       \
  gint bef = gst_camerabin2_atomic_int_add (&c->processing_counter, 1); \
  if (bef == 0)                                                         \
    g_object_notify (G_OBJECT (c), "idle");                             \
  GST_DEBUG_OBJECT ((c), "Processing counter incremented to: %d",       \
      bef + 1);                                                         \
}

#define GST_CAMERA_BIN2_PROCESSING_DEC(c)                                \
{                                                                       \
  if (g_atomic_int_dec_and_test (&c->processing_counter)) {             \
    g_object_notify (G_OBJECT (c), "idle");                             \
    GST_DEBUG_OBJECT ((c), "Camerabin now idle");			\
  }									\
  GST_DEBUG_OBJECT ((c), "Processing counter decremented");             \
}

#define GST_CAMERA_BIN2_RESET_PROCESSING_COUNTER(c)                      \
{                                                                       \
  g_atomic_int_set (&c->processing_counter, 0);                         \
  GST_DEBUG_OBJECT ((c), "Processing counter reset");                   \
}

GST_DEBUG_CATEGORY_STATIC (gst_camera_bin_debug);
#define GST_CAT_DEFAULT gst_camera_bin_debug

/* prototypes */

enum
{
  PROP_0,
  PROP_MODE,
  PROP_LOCATION,
  PROP_CAMERA_SRC,
  PROP_IMAGE_CAPTURE_SUPPORTED_CAPS,
  PROP_VIDEO_CAPTURE_SUPPORTED_CAPS,
  PROP_IMAGE_CAPTURE_CAPS,
  PROP_VIDEO_CAPTURE_CAPS,
  PROP_POST_PREVIEWS,
  PROP_PREVIEW_CAPS,
  PROP_VIDEO_ENCODING_PROFILE,
  PROP_IMAGE_FILTER,
  PROP_VIDEO_FILTER,
  PROP_VIEWFINDER_FILTER,
  PROP_PREVIEW_FILTER,
  PROP_VIEWFINDER_SINK,
  PROP_VIEWFINDER_SUPPORTED_CAPS,
  PROP_VIEWFINDER_CAPS,
  PROP_AUDIO_SRC,
  PROP_MUTE_AUDIO,
  PROP_AUDIO_CAPTURE_SUPPORTED_CAPS,
  PROP_AUDIO_CAPTURE_CAPS,
  PROP_ZOOM,
  PROP_MAX_ZOOM,
  PROP_IMAGE_ENCODING_PROFILE,
  PROP_IDLE,
  PROP_FLAGS,
  PROP_AUDIO_FILTER
};

enum
{
  /* action signals */
  START_CAPTURE_SIGNAL,
  STOP_CAPTURE_SIGNAL,
  /* emit signals */
  LAST_SIGNAL
};
static guint camerabin_signals[LAST_SIGNAL];

#define DEFAULT_MODE MODE_IMAGE
#define DEFAULT_LOCATION "cap_%d"
#define DEFAULT_POST_PREVIEWS FALSE
#define DEFAULT_MUTE_AUDIO FALSE
#define DEFAULT_IDLE TRUE
#define DEFAULT_FLAGS 0

#define DEFAULT_AUDIO_SRC "autoaudiosrc"

/********************************
 * Standard GObject boilerplate *
 * and GObject types            *
 ********************************/

static GstPipelineClass *parent_class;
static void gst_camera_bin_class_init (GstCameraBin2Class * klass);
static void gst_camera_bin_base_init (gpointer klass);
static void gst_camera_bin_init (GstCameraBin2 * camera);
static void gst_camera_bin_dispose (GObject * object);
static void gst_camera_bin_finalize (GObject * object);

static void gst_camera_bin_handle_message (GstBin * bin, GstMessage * message);
static gboolean gst_camera_bin_send_event (GstElement * element,
    GstEvent * event);

#define C_FLAGS(v) ((guint) v)
#define GST_TYPE_CAM_FLAGS (gst_cam_flags_get_type())
static GType
gst_cam_flags_get_type (void)
{
  static const GFlagsValue values[] = {
    {C_FLAGS (GST_CAM_FLAG_NO_AUDIO_CONVERSION), "Do not use audio conversion "
          "elements", "no-audio-conversion"},
    {C_FLAGS (GST_CAM_FLAG_NO_VIDEO_CONVERSION), "Do not use video conversion "
          "elements", "no-video-conversion"},
    {C_FLAGS (GST_CAM_FLAG_NO_VIEWFINDER_CONVERSION),
          "Do not use viewfinder conversion " "elements",
        "no-viewfinder-conversion"},
    {C_FLAGS (GST_CAM_FLAG_NO_IMAGE_CONVERSION), "Do not use image conversion "
          "elements", "no-image-conversion"},
    {0, NULL, NULL}
  };
  static volatile GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_flags_register_static ("GstCamFlags", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

GType
gst_camera_bin2_get_type (void)
{
  static GType gst_camera_bin_type = 0;
  static const GInterfaceInfo camerabin_tagsetter_info = {
    NULL,
    NULL,
    NULL,
  };

  if (!gst_camera_bin_type) {
    static const GTypeInfo gst_camera_bin_info = {
      sizeof (GstCameraBin2Class),
      (GBaseInitFunc) gst_camera_bin_base_init,
      NULL,
      (GClassInitFunc) gst_camera_bin_class_init,
      NULL,
      NULL,
      sizeof (GstCameraBin2),
      0,
      (GInstanceInitFunc) gst_camera_bin_init,
      NULL
    };

    gst_camera_bin_type =
        g_type_register_static (GST_TYPE_PIPELINE, "GstCameraBin2",
        &gst_camera_bin_info, 0);

    g_type_add_interface_static (gst_camera_bin_type, GST_TYPE_TAG_SETTER,
        &camerabin_tagsetter_info);
  }

  return gst_camera_bin_type;
}

/* GObject class functions */
static void gst_camera_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_camera_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* Element class functions */
static GstStateChangeReturn
gst_camera_bin_change_state (GstElement * element, GstStateChange trans);


/* Camerabin functions */

static GstEvent *
gst_camera_bin_new_event_renegotiate (void)
{
  return gst_event_new_custom (GST_EVENT_CUSTOM_BOTH,
      gst_structure_new ("renegotiate", NULL));
}

static GstEvent *
gst_camera_bin_new_event_file_location (const gchar * location)
{
  return gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
      gst_structure_new ("new-location", "location", G_TYPE_STRING, location,
          NULL));
}

static void
gst_camera_bin_start_capture (GstCameraBin2 * camerabin)
{
  const GstTagList *taglist;
  gint capture_index = camerabin->capture_index;
  gchar *location = NULL;
  GST_DEBUG_OBJECT (camerabin, "Received start-capture");

  /* check that we have a valid location */
  if (camerabin->mode == MODE_VIDEO) {
    if (camerabin->location == NULL) {
      GST_ELEMENT_ERROR (camerabin, RESOURCE, OPEN_WRITE,
          (_("File location is set to NULL, please set it to a valid filename")), (NULL));
      return;
    }

    g_mutex_lock (camerabin->video_capture_mutex);
    while (camerabin->video_state == GST_CAMERA_BIN_VIDEO_FINISHING) {
      g_cond_wait (camerabin->video_state_cond, camerabin->video_capture_mutex);
    }
    if (camerabin->video_state != GST_CAMERA_BIN_VIDEO_IDLE) {
      GST_WARNING_OBJECT (camerabin, "Another video recording is ongoing"
          " (state %d), cannot start a new one", camerabin->video_state);
      g_mutex_unlock (camerabin->video_capture_mutex);
      return;
    }
    camerabin->video_state = GST_CAMERA_BIN_VIDEO_STARTING;
  }

  GST_CAMERA_BIN2_PROCESSING_INC (camerabin);

  if (camerabin->location)
    location = g_strdup_printf (camerabin->location, capture_index);

  if (camerabin->mode == MODE_VIDEO) {
    if (camerabin->audio_src) {
      GstClock *clock = gst_pipeline_get_clock (GST_PIPELINE_CAST (camerabin));

      gst_element_set_state (camerabin->audio_src, GST_STATE_PAUSED);

      gst_element_set_base_time (camerabin->audio_src,
          gst_element_get_base_time (GST_ELEMENT_CAST (camerabin)));
      if (clock) {
        gst_element_set_clock (camerabin->audio_src, clock);
        gst_object_unref (clock);
      }
    }
  } else {
    /* store the next capture buffer filename */
    g_mutex_lock (camerabin->image_capture_mutex);
    camerabin->image_location_list =
        g_slist_append (camerabin->image_location_list, g_strdup (location));
    g_mutex_unlock (camerabin->image_capture_mutex);
  }

  if (camerabin->post_previews) {
    /* Count processing of preview images too */
    GST_CAMERA_BIN2_PROCESSING_INC (camerabin);
    /* store the next preview filename */
    g_mutex_lock (camerabin->preview_list_mutex);
    camerabin->preview_location_list =
        g_slist_append (camerabin->preview_location_list, location);
    g_mutex_unlock (camerabin->preview_list_mutex);
  } else {
    g_free (location);
  }

  g_signal_emit_by_name (camerabin->src, "start-capture", NULL);
  if (camerabin->mode == MODE_VIDEO) {
    camerabin->audio_send_newseg = TRUE;
    if (camerabin->audio_src)
      gst_element_set_state (camerabin->audio_src, GST_STATE_PLAYING);

    camerabin->video_state = GST_CAMERA_BIN_VIDEO_RECORDING;
    g_mutex_unlock (camerabin->video_capture_mutex);
  }

  /*
   * We have to push tags after start capture because the video elements
   * might be flushing from the previous capture and are reset only on the
   * notify from ready for capture going to FALSE
   */
  taglist = gst_tag_setter_get_tag_list (GST_TAG_SETTER (camerabin));
  GST_DEBUG_OBJECT (camerabin, "Have tags from application: %"
      GST_PTR_FORMAT, taglist);

  if (camerabin->mode == MODE_IMAGE) {
    /* Store image tags in a list and push them later, this prevents
       start_capture() from blocking in pad_push_event call */
    g_mutex_lock (camerabin->image_capture_mutex);
    camerabin->image_tags_list =
        g_slist_append (camerabin->image_tags_list,
        taglist ? gst_tag_list_copy (taglist) : NULL);
    g_mutex_unlock (camerabin->image_capture_mutex);
  } else if (taglist) {
    GstPad *active_pad;

    active_pad = gst_element_get_static_pad (camerabin->src,
        GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME);
    gst_pad_push_event (active_pad,
        gst_event_new_tag (gst_tag_list_copy (taglist)));

    gst_object_unref (active_pad);
  }

  GST_DEBUG_OBJECT (camerabin, "Start-capture end");
}

static void
gst_camera_bin_stop_capture (GstCameraBin2 * camerabin)
{
  GST_DEBUG_OBJECT (camerabin, "Received stop-capture");
  if (camerabin->mode == MODE_VIDEO) {
    g_mutex_lock (camerabin->video_capture_mutex);
    if (camerabin->video_state == GST_CAMERA_BIN_VIDEO_RECORDING) {
      if (camerabin->src)
        g_signal_emit_by_name (camerabin->src, "stop-capture", NULL);

      camerabin->video_state = GST_CAMERA_BIN_VIDEO_FINISHING;
      if (camerabin->audio_src) {
        camerabin->audio_drop_eos = FALSE;
        gst_element_send_event (camerabin->audio_src, gst_event_new_eos ());
      }
    }
    g_mutex_unlock (camerabin->video_capture_mutex);
  }
}

static void
gst_camera_bin_change_mode (GstCameraBin2 * camerabin, gint mode)
{
  if (mode == camerabin->mode)
    return;

  GST_DEBUG_OBJECT (camerabin, "Changing mode to %d", mode);

  /* stop any ongoing capture */
  gst_camera_bin_stop_capture (camerabin);
  camerabin->mode = mode;
  if (camerabin->src)
    g_object_set (camerabin->src, "mode", mode, NULL);
}

static void
gst_camera_bin_src_notify_readyforcapture (GObject * obj, GParamSpec * pspec,
    gpointer user_data)
{
  GstCameraBin2 *camera = GST_CAMERA_BIN2_CAST (user_data);
  gboolean ready;

  g_object_get (camera->src, "ready-for-capture", &ready, NULL);
  if (!ready) {
    gchar *location = NULL;

    if (camera->mode == MODE_VIDEO) {
      /* a video recording is about to start, change the filesink location */
      gst_element_set_state (camera->videosink, GST_STATE_NULL);
      location = g_strdup_printf (camera->location, camera->capture_index);
      GST_DEBUG_OBJECT (camera, "Switching videobin location to %s", location);
      g_object_set (camera->videosink, "location", location, NULL);
      g_free (location);
      if (gst_element_set_state (camera->videosink, GST_STATE_PLAYING) ==
          GST_STATE_CHANGE_FAILURE) {
        /* Resets the latest state change return, that would be a failure
         * and could cause problems in a camerabin2 state change */
        gst_element_set_state (camera->videosink, GST_STATE_NULL);
      }
    }

    camera->capture_index++;
  }
}

static void
gst_camera_bin_dispose (GObject * object)
{
  GstCameraBin2 *camerabin = GST_CAMERA_BIN2_CAST (object);

  g_free (camerabin->location);
  g_mutex_free (camerabin->preview_list_mutex);
  g_mutex_free (camerabin->image_capture_mutex);
  g_mutex_free (camerabin->video_capture_mutex);
  g_cond_free (camerabin->video_state_cond);

  if (camerabin->src_capture_notify_id)
    g_signal_handler_disconnect (camerabin->src,
        camerabin->src_capture_notify_id);
  if (camerabin->src)
    gst_object_unref (camerabin->src);
  if (camerabin->user_src)
    gst_object_unref (camerabin->user_src);

  if (camerabin->audio_src)
    gst_object_unref (camerabin->audio_src);
  if (camerabin->user_audio_src)
    gst_object_unref (camerabin->user_audio_src);

  if (camerabin->audio_capsfilter)
    gst_object_unref (camerabin->audio_capsfilter);
  if (camerabin->audio_volume)
    gst_object_unref (camerabin->audio_volume);

  if (camerabin->viewfinderbin)
    gst_object_unref (camerabin->viewfinderbin);
  if (camerabin->viewfinderbin_queue)
    gst_object_unref (camerabin->viewfinderbin_queue);
  if (camerabin->viewfinderbin_capsfilter)
    gst_object_unref (camerabin->viewfinderbin_capsfilter);

  if (camerabin->video_encodebin_signal_id)
    g_signal_handler_disconnect (camerabin->video_encodebin,
        camerabin->video_encodebin_signal_id);

  if (camerabin->videosink)
    gst_object_unref (camerabin->videosink);
  if (camerabin->video_encodebin)
    gst_object_unref (camerabin->video_encodebin);
  if (camerabin->videobin_capsfilter)
    gst_object_unref (camerabin->videobin_capsfilter);

  if (camerabin->image_encodebin_signal_id)
    g_signal_handler_disconnect (camerabin->image_encodebin,
        camerabin->image_encodebin_signal_id);
  if (camerabin->imagesink)
    gst_object_unref (camerabin->imagesink);
  if (camerabin->image_encodebin)
    gst_object_unref (camerabin->image_encodebin);
  if (camerabin->imagebin_capsfilter)
    gst_object_unref (camerabin->imagebin_capsfilter);

  if (camerabin->video_filter)
    gst_object_unref (camerabin->video_filter);
  if (camerabin->image_filter)
    gst_object_unref (camerabin->image_filter);
  if (camerabin->viewfinder_filter)
    gst_object_unref (camerabin->viewfinder_filter);
  if (camerabin->audio_filter)
    gst_object_unref (camerabin->audio_filter);

  if (camerabin->user_video_filter)
    gst_object_unref (camerabin->user_video_filter);
  if (camerabin->user_audio_filter)
    gst_object_unref (camerabin->user_audio_filter);
  if (camerabin->user_image_filter)
    gst_object_unref (camerabin->user_image_filter);
  if (camerabin->user_viewfinder_filter)
    gst_object_unref (camerabin->user_viewfinder_filter);

  if (camerabin->video_profile)
    gst_encoding_profile_unref (camerabin->video_profile);
  if (camerabin->image_profile)
    gst_encoding_profile_unref (camerabin->image_profile);

  if (camerabin->preview_caps)
    gst_caps_replace (&camerabin->preview_caps, NULL);
  if (camerabin->preview_filter) {
    gst_object_unref (camerabin->preview_filter);
    camerabin->preview_filter = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_camera_bin_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_camera_bin_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (element_class, "CameraBin2",
      "Generic/Bin/Camera", "CameraBin2",
      "Thiago Santos <thiago.sousa.santos@collabora.co.uk>");
}

static void
gst_camera_bin_class_init (GstCameraBin2Class * klass)
{
  GObjectClass *object_class;
  GstElementClass *element_class;
  GstBinClass *bin_class;

  parent_class = g_type_class_peek_parent (klass);
  object_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  bin_class = GST_BIN_CLASS (klass);

  object_class->dispose = gst_camera_bin_dispose;
  object_class->finalize = gst_camera_bin_finalize;
  object_class->set_property = gst_camera_bin_set_property;
  object_class->get_property = gst_camera_bin_get_property;

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_camera_bin_change_state);
  element_class->send_event = GST_DEBUG_FUNCPTR (gst_camera_bin_send_event);

  bin_class->handle_message = gst_camera_bin_handle_message;

  klass->start_capture = gst_camera_bin_start_capture;
  klass->stop_capture = gst_camera_bin_stop_capture;

  /**
   * GstCameraBin2:mode:
   *
   * Set the mode of operation: still image capturing or video recording.
   */
  g_object_class_install_property (object_class, PROP_MODE,
      g_param_spec_enum ("mode", "Mode",
          "The capture mode (still image capture or video recording)",
          GST_TYPE_CAMERABIN_MODE, DEFAULT_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_LOCATION,
      g_param_spec_string ("location", "Location",
          "Location to save the captured files. A %d might be used on the"
          "filename as a placeholder for a numeric index of the capture."
          "Default is cap_%d",
          DEFAULT_LOCATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_CAMERA_SRC,
      g_param_spec_object ("camera-source", "Camera source",
          "The camera source element to be used. It is only taken into use on"
          " the next null to ready transition",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_AUDIO_SRC,
      g_param_spec_object ("audio-source", "Audio source",
          "The audio source element to be used on video recordings. It is only"
          " taken into use on the next null to ready transition",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MUTE_AUDIO,
      g_param_spec_boolean ("mute", "Mute",
          "If the audio recording should be muted. Note that this still "
          "saves audio data to the resulting file, but they are silent. Use "
          "a video-profile without audio to disable audio completely",
          DEFAULT_MUTE_AUDIO, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_AUDIO_CAPTURE_SUPPORTED_CAPS,
      g_param_spec_boxed ("audio-capture-supported-caps",
          "Audio capture supported caps",
          "Formats supported for capturing audio represented as GstCaps",
          GST_TYPE_CAPS, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_AUDIO_CAPTURE_CAPS,
      g_param_spec_boxed ("audio-capture-caps",
          "Audio capture caps",
          "Format to capture audio for video recording represented as GstCaps",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_IMAGE_CAPTURE_SUPPORTED_CAPS,
      g_param_spec_boxed ("image-capture-supported-caps",
          "Image capture supported caps",
          "Formats supported for capturing images represented as GstCaps",
          GST_TYPE_CAPS, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_VIDEO_CAPTURE_SUPPORTED_CAPS,
      g_param_spec_boxed ("video-capture-supported-caps",
          "Video capture supported caps",
          "Formats supported for capturing videos represented as GstCaps",
          GST_TYPE_CAPS, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_IMAGE_CAPTURE_CAPS,
      g_param_spec_boxed ("image-capture-caps",
          "Image capture caps",
          "Caps for image capture",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_VIDEO_CAPTURE_CAPS,
      g_param_spec_boxed ("video-capture-caps",
          "Video capture caps",
          "Caps for video capture",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_POST_PREVIEWS,
      g_param_spec_boolean ("post-previews", "Post Previews",
          "If capture preview images should be posted to the bus",
          DEFAULT_POST_PREVIEWS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PREVIEW_CAPS,
      g_param_spec_boxed ("preview-caps", "Preview caps",
          "The caps of the preview image to be posted",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_VIDEO_ENCODING_PROFILE,
      gst_param_spec_mini_object ("video-profile", "Video Profile",
          "The GstEncodingProfile to use for video recording. Audio is enabled "
          "when this profile supports audio.", GST_TYPE_ENCODING_PROFILE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_IMAGE_FILTER,
      g_param_spec_object ("image-filter", "Image filter",
          "The element that will process captured image frames. (Should be"
          " set on NULL state)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_VIDEO_FILTER,
      g_param_spec_object ("video-filter", "Video filter",
          "The element that will process captured video frames. (Should be"
          " set on NULL state)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_VIEWFINDER_FILTER,
      g_param_spec_object ("viewfinder-filter", "Viewfinder filter",
          "The element that will process frames going to the viewfinder."
          " (Should be set on NULL state)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_AUDIO_FILTER,
      g_param_spec_object ("audio-filter", "Audio filter",
          "The element that will process captured audio buffers when recording"
          ". (Should be set on NULL state)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PREVIEW_FILTER,
      g_param_spec_object ("preview-filter", "Preview filter",
          "The element that will process preview buffers."
          " (Should be set on NULL state)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_VIEWFINDER_SINK,
      g_param_spec_object ("viewfinder-sink", "Viewfinder sink",
          "The video sink of the viewfinder. It is only taken into use"
          " on the next null to ready transition",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_VIEWFINDER_CAPS,
      g_param_spec_boxed ("viewfinder-caps",
          "Viewfinder caps",
          "Restricts the caps that can be used on the viewfinder",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_ZOOM,
      g_param_spec_float ("zoom", "Zoom",
          "Digital zoom factor (e.g. 1.5 means 1.5x)", MIN_ZOOM, MAX_ZOOM,
          DEFAULT_ZOOM, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MAX_ZOOM,
      g_param_spec_float ("max-zoom", "Maximum zoom level (note: may change "
          "depending on resolution/implementation)",
          "Digital zoom factor (e.g. 1.5 means 1.5x)", MIN_ZOOM, G_MAXFLOAT,
          MAX_ZOOM, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /* TODO
   * Review before stable
   * - One problem with using encodebin for images here is how jifmux
   *   autoplugging works. We need to give it a higher rank and fix its
   *   caps (it has image/jpeg on sink and src pads). Preliminary tests
   *   show that jifmux is picked if image/jpeg is the caps of a container
   *   profile. So this could work.
   * - There seems to be a problem with encodebin for images currently as
   *   it autoplugs a videorate that only starts outputing buffers after
   *   getting the 2nd buffer.
   */
  g_object_class_install_property (object_class, PROP_IMAGE_ENCODING_PROFILE,
      gst_param_spec_mini_object ("image-profile", "Image Profile",
          "The GstEncodingProfile to use for image captures.",
          GST_TYPE_ENCODING_PROFILE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  g_object_class_install_property (object_class, PROP_IDLE,
      g_param_spec_boolean ("idle", "Idle",
          "If camerabin2 is idle (not doing captures).", DEFAULT_IDLE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /* TODO review before going stable
   * We have viewfinder-supported-caps that returns the caps that the
   * camerasrc can produce on its viewfinder pad, this could easily be
   * confused with what the viewfinder-sink accepts.
   *
   * Do we want to add a 'viewfinder-sink-supported-caps' or maybe change
   * the name of this property?
   */
  g_object_class_install_property (object_class,
      PROP_VIEWFINDER_SUPPORTED_CAPS,
      g_param_spec_boxed ("viewfinder-supported-caps",
          "Camera source Viewfinder pad supported caps",
          "The caps that the camera source can produce on the viewfinder pad",
          GST_TYPE_CAPS, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

   /**
    * GstCameraBin:flags
    *
    * Control the behaviour of encodebin.
    */
  g_object_class_install_property (object_class, PROP_FLAGS,
      g_param_spec_flags ("flags", "Flags", "Flags to control behaviour",
          GST_TYPE_CAM_FLAGS, DEFAULT_FLAGS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin2::capture-start:
   * @camera: the camera bin element
   *
   * Starts image capture or video recording depending on the Mode.
   */
  camerabin_signals[START_CAPTURE_SIGNAL] =
      g_signal_new ("start-capture",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstCameraBin2Class, start_capture),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GstCameraBin2::capture-stop:
   * @camera: the camera bin element
   */
  camerabin_signals[STOP_CAPTURE_SIGNAL] =
      g_signal_new ("stop-capture",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstCameraBin2Class, stop_capture),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
gst_camera_bin_init (GstCameraBin2 * camera)
{
  camera->post_previews = DEFAULT_POST_PREVIEWS;
  camera->mode = DEFAULT_MODE;
  camera->location = g_strdup (DEFAULT_LOCATION);
  camera->viewfinderbin = gst_element_factory_make ("viewfinderbin", "vf-bin");
  camera->zoom = DEFAULT_ZOOM;
  camera->max_zoom = MAX_ZOOM;
  camera->flags = DEFAULT_FLAGS;
  camera->preview_list_mutex = g_mutex_new ();
  camera->image_capture_mutex = g_mutex_new ();
  camera->video_capture_mutex = g_mutex_new ();
  camera->video_state_cond = g_cond_new ();

  /* capsfilters are created here as we proxy their caps properties and
   * this way we avoid having to store the caps while on NULL state to 
   * set them later */
  camera->videobin_capsfilter = gst_element_factory_make ("capsfilter",
      "videobin-capsfilter");
  camera->imagebin_capsfilter = gst_element_factory_make ("capsfilter",
      "imagebin-capsfilter");
  camera->viewfinderbin_capsfilter = gst_element_factory_make ("capsfilter",
      "viewfinderbin-capsfilter");

  gst_bin_add_many (GST_BIN (camera),
      gst_object_ref (camera->viewfinderbin),
      gst_object_ref (camera->videobin_capsfilter),
      gst_object_ref (camera->imagebin_capsfilter),
      gst_object_ref (camera->viewfinderbin_capsfilter), NULL);

  /* these elements are only added if they are going to be used */
  camera->audio_capsfilter = gst_element_factory_make ("capsfilter",
      "audio-capsfilter");
  camera->audio_volume = gst_element_factory_make ("volume", "audio-volume");
}

static void
gst_image_capture_bin_post_image_done (GstCameraBin2 * camera,
    const gchar * filename)
{
  GstMessage *msg;

  g_return_if_fail (filename != NULL);

  msg = gst_message_new_element (GST_OBJECT_CAST (camera),
      gst_structure_new ("image-done", "filename", G_TYPE_STRING,
          filename, NULL));

  if (!gst_element_post_message (GST_ELEMENT_CAST (camera), msg))
    GST_WARNING_OBJECT (camera, "Failed to post image-done message");
}

static void
gst_video_capture_bin_post_video_done (GstCameraBin2 * camera)
{
  GstMessage *msg;

  msg = gst_message_new_element (GST_OBJECT_CAST (camera),
      gst_structure_new ("video-done", NULL));

  if (!gst_element_post_message (GST_ELEMENT_CAST (camera), msg))
    GST_WARNING_OBJECT (camera, "Failed to post video-done message");
}

static void
gst_camera_bin_skip_next_preview (GstCameraBin2 * camerabin)
{
  gchar *location;

  g_mutex_lock (camerabin->preview_list_mutex);
  if (camerabin->preview_location_list) {
    location = camerabin->preview_location_list->data;
    GST_DEBUG_OBJECT (camerabin, "Skipping preview for %s", location);
    g_free (location);
    camerabin->preview_location_list =
        g_slist_delete_link (camerabin->preview_location_list,
        camerabin->preview_location_list);
    GST_CAMERA_BIN2_PROCESSING_DEC (camerabin);
  } else {
    GST_WARNING_OBJECT (camerabin, "No previews to skip");
  }
  g_mutex_unlock (camerabin->preview_list_mutex);
}

static gpointer
gst_camera_bin_video_reset_elements (gpointer u_data)
{
  GstCameraBin2 *camerabin = GST_CAMERA_BIN2_CAST (u_data);

  GST_DEBUG_OBJECT (camerabin, "Resetting video elements state");
  g_mutex_lock (camerabin->video_capture_mutex);

  /* reset element states to clear eos/flushing pads */
  gst_element_set_state (camerabin->video_encodebin, GST_STATE_READY);
  gst_element_set_state (camerabin->videobin_capsfilter, GST_STATE_READY);
  if (camerabin->video_filter) {
    gst_element_set_state (camerabin->video_filter, GST_STATE_READY);
    gst_element_sync_state_with_parent (camerabin->video_filter);
  }
  gst_element_sync_state_with_parent (camerabin->videobin_capsfilter);
  gst_element_sync_state_with_parent (camerabin->video_encodebin);

  if (camerabin->audio_src) {
    gst_element_set_state (camerabin->audio_capsfilter, GST_STATE_READY);
    gst_element_set_state (camerabin->audio_volume, GST_STATE_READY);

    /* FIXME We need to set audiosrc to null to make it resync the ringbuffer
     * while bug https://bugzilla.gnome.org/show_bug.cgi?id=648359 isn't
     * fixed.
     *
     * Also, we don't reinit the audiosrc to keep audio devices from being open
     * and running until we really need them */
    gst_element_set_state (camerabin->audio_src, GST_STATE_NULL);

    if (camerabin->audio_filter) {
      gst_element_set_state (camerabin->audio_filter, GST_STATE_READY);
      gst_element_sync_state_with_parent (camerabin->audio_filter);
    }

    gst_element_sync_state_with_parent (camerabin->audio_capsfilter);
    gst_element_sync_state_with_parent (camerabin->audio_volume);

  }

  GST_DEBUG_OBJECT (camerabin, "Setting video state to idle");
  camerabin->video_state = GST_CAMERA_BIN_VIDEO_IDLE;
  g_cond_signal (camerabin->video_state_cond);
  g_mutex_unlock (camerabin->video_capture_mutex);

  gst_object_unref (camerabin);
  return NULL;
}

static void
gst_camera_bin_handle_message (GstBin * bin, GstMessage * message)
{
  GstCameraBin2 *camerabin = GST_CAMERA_BIN2_CAST (bin);
  gboolean dec_counter = FALSE;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ELEMENT:{
      const GstStructure *structure = gst_message_get_structure (message);
      const gchar *filename;

      if (gst_structure_has_name (structure, "GstMultiFileSink")) {
        filename = gst_structure_get_string (structure, "filename");
        GST_DEBUG_OBJECT (bin, "Got file save message from multifilesink, "
            "image %s has been saved", filename);
        if (filename) {
          gst_image_capture_bin_post_image_done (GST_CAMERA_BIN2_CAST (bin),
              filename);
        }
        dec_counter = TRUE;
      } else if (gst_structure_has_name (structure, "preview-image")) {
        gchar *location = NULL;

        g_mutex_lock (camerabin->preview_list_mutex);
        if (camerabin->preview_location_list) {
          location = camerabin->preview_location_list->data;
          camerabin->preview_location_list =
              g_slist_delete_link (camerabin->preview_location_list,
              camerabin->preview_location_list);
          GST_DEBUG_OBJECT (camerabin, "Adding preview location to preview "
              "message '%s'", location);
        } else {
          GST_WARNING_OBJECT (camerabin, "Unexpected preview message received, "
              "won't be able to put location field into the message. This can "
              "happen if the source is posting previews while camerabin2 is "
              "shutting down");
        }
        g_mutex_unlock (camerabin->preview_list_mutex);

        if (location) {
          GValue value = { 0 };
          g_value_init (&value, G_TYPE_STRING);
          g_value_take_string (&value, location);
          gst_structure_take_value ((GstStructure *) structure, "location",
              &value);
        }

        GST_LOG_OBJECT (bin, "received preview-image message");
        dec_counter = TRUE;
      }
    }
      break;
    case GST_MESSAGE_WARNING:{
      GError *err = NULL;
      gchar *debug = NULL;

      gst_message_parse_warning (message, &err, &debug);
      if (err->domain == GST_RESOURCE_ERROR) {
        /* some capturing failed */
        GST_WARNING_OBJECT (bin, "Capture failed, reason: %s - %s",
            err->message, debug);
        if (camerabin->post_previews) {
          gst_camera_bin_skip_next_preview (camerabin);
        }
        dec_counter = TRUE;
      }
      g_error_free (err);
      g_free (debug);
    }
      break;
    case GST_MESSAGE_EOS:{
      GstElement *src = GST_ELEMENT (GST_MESSAGE_SRC (message));
      if (src == GST_CAMERA_BIN2_CAST (bin)->videosink) {

        g_mutex_lock (camerabin->video_capture_mutex);
        GST_DEBUG_OBJECT (bin, "EOS from video branch");
        g_assert (camerabin->video_state == GST_CAMERA_BIN_VIDEO_FINISHING);

        gst_video_capture_bin_post_video_done (GST_CAMERA_BIN2_CAST (bin));
        dec_counter = TRUE;

        if (!g_thread_create (gst_camera_bin_video_reset_elements,
                gst_object_ref (camerabin), FALSE, NULL)) {
          GST_WARNING_OBJECT (camerabin, "Failed to create thread to "
              "reset video elements' state, video recordings may not work "
              "anymore");
          gst_object_unref (camerabin);
          camerabin->video_state = GST_CAMERA_BIN_VIDEO_IDLE;
        }

        g_mutex_unlock (camerabin->video_capture_mutex);
      }
    }
      break;
    default:
      break;
  }
  if (message)
    GST_BIN_CLASS (parent_class)->handle_message (bin, message);

  if (dec_counter)
    GST_CAMERA_BIN2_PROCESSING_DEC (camerabin);
}

/*
 * Transforms:
 * ... ! previous_element [ ! current_filter ] ! next_element ! ...
 *
 * into:
 * ... ! previous_element [ ! new_filter ] ! next_element ! ...
 *
 * Where current_filter and new_filter might or might not be NULL
 */
static void
gst_camera_bin_check_and_replace_filter (GstCameraBin2 * camera,
    GstElement ** current_filter, GstElement * new_filter,
    GstElement * previous_element, GstElement * next_element,
    const gchar * prev_elem_pad)
{
  if (*current_filter == new_filter) {
    GST_DEBUG_OBJECT (camera, "Current filter is the same as the previous, "
        "no switch needed.");
    return;
  }

  GST_DEBUG_OBJECT (camera, "Replacing current filter (%s) with new filter "
      "(%s)", *current_filter ? GST_ELEMENT_NAME (*current_filter) : "null",
      new_filter ? GST_ELEMENT_NAME (new_filter) : "null");

  if (*current_filter) {
    gst_bin_remove (GST_BIN_CAST (camera), *current_filter);
    gst_object_unref (*current_filter);
    *current_filter = NULL;
  } else {
    /* unlink the pads */
    gst_element_unlink (previous_element, next_element);
  }

  if (new_filter) {
    *current_filter = gst_object_ref (new_filter);
    gst_bin_add (GST_BIN_CAST (camera), gst_object_ref (new_filter));
  }

  if (prev_elem_pad) {
    if (new_filter) {
      gst_element_link_pads (previous_element, prev_elem_pad, new_filter, NULL);
      gst_element_link (new_filter, next_element);
    } else {
      gst_element_link_pads (previous_element, prev_elem_pad, next_element,
          NULL);
    }
  } else {
    if (new_filter)
      gst_element_link_many (previous_element, new_filter, next_element, NULL);
    else
      gst_element_link (previous_element, next_element);
  }
}

static void
encodebin_element_added (GstElement * encodebin, GstElement * new_element,
    GstCameraBin2 * camera)
{
  GstElementFactory *factory = gst_element_get_factory (new_element);

  if (factory != NULL) {
    if (strcmp (GST_PLUGIN_FEATURE_NAME (factory), "audiorate") == 0 ||
        strcmp (GST_PLUGIN_FEATURE_NAME (factory), "videorate") == 0) {
      g_object_set (new_element, "skip-to-first", TRUE, NULL);
    }
  }

  if (gst_element_implements_interface (new_element, GST_TYPE_TAG_SETTER)) {
    GstTagSetter *tagsetter = GST_TAG_SETTER (new_element);

    gst_tag_setter_set_tag_merge_mode (tagsetter, GST_TAG_MERGE_REPLACE);
  }
}

#define VIDEO_PAD 1
#define AUDIO_PAD 2
static GstPad *
encodebin_find_pad (GstCameraBin2 * camera, GstElement * encodebin,
    gint pad_type)
{
  GstPad *pad = NULL;
  GstIterator *iter;
  gboolean done;

  GST_DEBUG_OBJECT (camera, "Looking at encodebin pads, searching for %s pad",
      pad_type == VIDEO_PAD ? "video" : "audio");

  iter = gst_element_iterate_sink_pads (encodebin);
  done = FALSE;
  while (!done) {
    switch (gst_iterator_next (iter, (gpointer *) & pad)) {
      case GST_ITERATOR_OK:
        if (pad_type == VIDEO_PAD) {
          if (strstr (GST_PAD_NAME (pad), "video") != NULL) {
            GST_DEBUG_OBJECT (camera, "Found video pad %s", GST_PAD_NAME (pad));
            done = TRUE;
            break;
          }
        } else if (pad_type == AUDIO_PAD) {
          if (strstr (GST_PAD_NAME (pad), "audio") != NULL) {
            GST_DEBUG_OBJECT (camera, "Found audio pad %s", GST_PAD_NAME (pad));
            done = TRUE;
            break;
          }
        }
        gst_object_unref (pad);
        pad = NULL;
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (iter);
        break;
      case GST_ITERATOR_ERROR:
        pad = NULL;
        done = TRUE;
        break;
      case GST_ITERATOR_DONE:
        pad = NULL;
        done = TRUE;
        break;
    }
  }
  gst_iterator_free (iter);

  /* no static pad, try requesting one */
  if (pad == NULL) {
    GstElementClass *klass;
    GstPadTemplate *tmpl;

    GST_DEBUG_OBJECT (camera, "No pads found, trying to request one");

    klass = GST_ELEMENT_GET_CLASS (encodebin);
    tmpl = gst_element_class_get_pad_template (klass, pad_type == VIDEO_PAD ?
        "video_%d" : "audio_%d");

    pad = gst_element_request_pad (encodebin, tmpl, NULL, NULL);
    GST_DEBUG_OBJECT (camera, "Got pad: %s", pad ? GST_PAD_NAME (pad) : "null");
  }

  return pad;
}

static gboolean
gst_camera_bin_video_profile_has_audio (GstCameraBin2 * camera)
{
  const GList *list;

  g_return_val_if_fail (camera->video_profile != NULL, FALSE);

  if (GST_IS_ENCODING_VIDEO_PROFILE (camera->video_profile))
    return FALSE;

  for (list =
      gst_encoding_container_profile_get_profiles ((GstEncodingContainerProfile
              *) camera->video_profile); list; list = g_list_next (list)) {
    GstEncodingProfile *profile = (GstEncodingProfile *) list->data;

    if (GST_IS_ENCODING_AUDIO_PROFILE (profile))
      return TRUE;
  }

  return FALSE;
}

static GstPadLinkReturn
gst_camera_bin_link_encodebin (GstCameraBin2 * camera, GstElement * encodebin,
    GstElement * element, gint padtype)
{
  GstPadLinkReturn ret;
  GstPad *srcpad;
  GstPad *sinkpad = NULL;

  srcpad = gst_element_get_static_pad (element, "src");
  g_assert (srcpad != NULL);

  sinkpad = encodebin_find_pad (camera, encodebin, padtype);

  /* there may be no available sink pad for encodebin in some situations:
   * e.g. missing elements or incompatible padtype */
  if (sinkpad == NULL) {
    gst_object_unref (srcpad);
    return GST_PAD_LINK_REFUSED;
  }

  ret = gst_pad_link_full (srcpad, sinkpad, GST_PAD_LINK_CHECK_CAPS);
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  return ret;
}

static void
gst_camera_bin_src_notify_max_zoom_cb (GObject * self, GParamSpec * pspec,
    gpointer user_data)
{
  GstCameraBin2 *camera = (GstCameraBin2 *) user_data;

  g_object_get (self, "max-zoom", &camera->max_zoom, NULL);
  GST_DEBUG_OBJECT (camera, "Max zoom updated to %f", camera->max_zoom);
  g_object_notify (G_OBJECT (camera), "max-zoom");
}

static void
gst_camera_bin_src_notify_zoom_cb (GObject * self, GParamSpec * pspec,
    gpointer user_data)
{
  GstCameraBin2 *camera = (GstCameraBin2 *) user_data;

  g_object_get (self, "zoom", &camera->zoom, NULL);
  GST_DEBUG_OBJECT (camera, "Zoom updated to %f", camera->zoom);
  g_object_notify (G_OBJECT (camera), "zoom");
}

static gboolean
gst_camera_bin_image_src_buffer_probe (GstPad * pad, GstBuffer * buf,
    gpointer data)
{
  gboolean ret = TRUE;
  GstCameraBin2 *camerabin = data;
  GstEvent *evt;
  gchar *location = NULL;
  GstPad *peer;
  GstTagList *tags;

  g_mutex_lock (camerabin->image_capture_mutex);

  /* Push pending image tags */
  if (camerabin->image_tags_list) {
    tags = camerabin->image_tags_list->data;
    camerabin->image_tags_list =
        g_slist_delete_link (camerabin->image_tags_list,
        camerabin->image_tags_list);
    GST_DEBUG_OBJECT (camerabin, "Pushing tags from application: %"
        GST_PTR_FORMAT, tags);
    if (tags) {
      peer = gst_pad_get_peer (pad);
      gst_pad_send_event (peer, gst_event_new_tag (tags));
      gst_object_unref (peer);
    }
  } else {
    GST_DEBUG_OBJECT (camerabin, "No tags from application to send");
  }

  /* Push image location event */
  if (camerabin->image_location_list) {
    location = camerabin->image_location_list->data;
    camerabin->image_location_list =
        g_slist_delete_link (camerabin->image_location_list,
        camerabin->image_location_list);
    GST_DEBUG_OBJECT (camerabin, "Sending image location change to '%s'",
        location);
  } else {
    GST_DEBUG_OBJECT (camerabin, "No filename location change to send");
    g_mutex_unlock (camerabin->image_capture_mutex);
    return ret;
  }
  g_mutex_unlock (camerabin->image_capture_mutex);

  if (location) {
    evt = gst_camera_bin_new_event_file_location (location);
    peer = gst_pad_get_peer (pad);
    gst_pad_send_event (peer, evt);
    gst_object_unref (peer);
    g_free (location);
  } else {
    /* This means we don't have to encode the capture, it is used for
     * signaling the application just wants the preview */
    ret = FALSE;
    GST_CAMERA_BIN2_PROCESSING_DEC (camerabin);
  }

  return ret;
}


static gboolean
gst_camera_bin_image_sink_event_probe (GstPad * pad, GstEvent * event,
    gpointer data)
{
  GstCameraBin2 *camerabin = data;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM:{
      if (gst_event_has_name (event, "new-location")) {
        const GstStructure *structure = gst_event_get_structure (event);
        const gchar *filename = gst_structure_get_string (structure,
            "location");

        gst_element_set_state (camerabin->imagesink, GST_STATE_NULL);
        GST_DEBUG_OBJECT (camerabin, "Setting filename to imagesink: %s",
            filename);
        g_object_set (camerabin->imagesink, "location", filename, NULL);
        if (gst_element_set_state (camerabin->imagesink, GST_STATE_PLAYING) ==
            GST_STATE_CHANGE_FAILURE) {
          /* Resets the latest state change return, that would be a failure
           * and could cause problems in a camerabin2 state change */
          gst_element_set_state (camerabin->imagesink, GST_STATE_NULL);
        }
      }
    }
      break;
    default:
      break;
  }

  return TRUE;
}

static gboolean
gst_camera_bin_audio_src_data_probe (GstPad * pad, GstMiniObject * obj,
    gpointer data)
{
  GstCameraBin2 *camera = data;
  gboolean ret = TRUE;

  if (GST_IS_BUFFER (obj)) {
    if (G_UNLIKELY (camera->audio_send_newseg)) {
      GstBuffer *buf = GST_BUFFER_CAST (obj);
      GstClockTime ts = GST_BUFFER_TIMESTAMP (buf);
      GstPad *peer;

      if (!GST_CLOCK_TIME_IS_VALID (ts)) {
        ts = 0;
      }

      peer = gst_pad_get_peer (pad);
      g_return_val_if_fail (peer != NULL, TRUE);

      gst_pad_send_event (peer, gst_event_new_new_segment (FALSE, 1.0,
              GST_FORMAT_TIME, ts, -1, 0));

      gst_object_unref (peer);

      camera->audio_send_newseg = FALSE;
    }
  } else {
    GstEvent *event = GST_EVENT_CAST (obj);
    if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
      /* we only let an EOS pass when the user is stopping a capture */
      if (camera->audio_drop_eos) {
        ret = FALSE;
      } else {
        camera->audio_drop_eos = TRUE;
        /* should already be false, but reinforce in case no buffers get
         * pushed */
        camera->audio_send_newseg = FALSE;
      }
    } else if (GST_EVENT_TYPE (event) == GST_EVENT_NEWSEGMENT) {
      ret = FALSE;
    }
  }

  return ret;
}

/**
 * gst_camera_bin_create_elements:
 * @param camera: the #GstCameraBin2
 *
 * Creates all elements inside #GstCameraBin2
 *
 * Each of the pads on the camera source is linked as follows:
 * .pad ! queue ! capsfilter ! correspondingbin
 *
 * Where 'correspondingbin' is the bin appropriate for
 * the camera source pad.
 */
static gboolean
gst_camera_bin_create_elements (GstCameraBin2 * camera)
{
  gboolean new_src = FALSE;
  gboolean new_audio_src = FALSE;
  gboolean has_audio;
  gboolean profile_switched = FALSE;
  const gchar *missing_element_name;
  gint encbin_flags = 0;

  if (!camera->elements_created) {
    /* Check that elements created in _init were really created */
    if (!(camera->audio_capsfilter && camera->videobin_capsfilter &&
            camera->imagebin_capsfilter && camera->viewfinderbin_capsfilter)) {
      missing_element_name = "capsfilter";
      goto missing_element;
    }

    camera->video_encodebin =
        gst_element_factory_make ("encodebin", "video-encodebin");
    if (!camera->video_encodebin) {
      missing_element_name = "encodebin";
      goto missing_element;
    }
    camera->video_encodebin_signal_id =
        g_signal_connect (camera->video_encodebin, "element-added",
        (GCallback) encodebin_element_added, camera);

    camera->videosink =
        gst_element_factory_make ("filesink", "videobin-filesink");
    if (!camera->videosink) {
      missing_element_name = "filesink";
      goto missing_element;
    }
    g_object_set (camera->videosink, "async", FALSE, NULL);

    /* audio elements */
    if (!camera->audio_volume) {
      missing_element_name = "volume";
      goto missing_element;
    }

    if (camera->video_profile == NULL) {
      GstEncodingContainerProfile *prof;
      GstCaps *caps;

      caps = gst_caps_new_simple ("application/ogg", NULL);
      prof = gst_encoding_container_profile_new ("ogg", "theora+vorbis+ogg",
          caps, NULL);
      gst_caps_unref (caps);

      caps = gst_caps_new_simple ("video/x-theora", NULL);
      if (!gst_encoding_container_profile_add_profile (prof,
              (GstEncodingProfile *) gst_encoding_video_profile_new (caps,
                  NULL, NULL, 1))) {
        GST_WARNING_OBJECT (camera, "Failed to create encoding profiles");
      }
      gst_caps_unref (caps);

      caps = gst_caps_new_simple ("audio/x-vorbis", NULL);
      if (!gst_encoding_container_profile_add_profile (prof,
              (GstEncodingProfile *) gst_encoding_audio_profile_new (caps,
                  NULL, NULL, 1))) {
        GST_WARNING_OBJECT (camera, "Failed to create encoding profiles");
      }
      gst_caps_unref (caps);

      camera->video_profile = (GstEncodingProfile *) prof;
      camera->video_profile_switch = TRUE;
    }

    camera->image_encodebin =
        gst_element_factory_make ("encodebin", "image-encodebin");
    if (!camera->image_encodebin) {
      missing_element_name = "encodebin";
      goto missing_element;
    }
    /* durations have no meaning for image captures */
    g_object_set (camera->image_encodebin, "queue-time-max", (guint64) 0, NULL);

    camera->image_encodebin_signal_id =
        g_signal_connect (camera->image_encodebin, "element-added",
        (GCallback) encodebin_element_added, camera);

    camera->imagesink =
        gst_element_factory_make ("multifilesink", "imagebin-filesink");
    if (!camera->imagesink) {
      missing_element_name = "multifilesink";
      goto missing_element;
    }
    g_object_set (camera->imagesink, "async", FALSE, "post-messages", TRUE,
        NULL);

    if (camera->image_profile == NULL) {
      GstEncodingContainerProfile *prof;
      GstEncodingVideoProfile *vprof;
      GstCaps *caps;

      caps = gst_caps_new_simple ("image/jpeg", NULL);
      vprof = gst_encoding_video_profile_new (caps, NULL, NULL, 1);
      gst_encoding_video_profile_set_variableframerate (vprof, TRUE);

      prof = gst_encoding_container_profile_new ("jpeg", "jpeg container", caps,
          NULL);
      gst_encoding_container_profile_add_profile (prof,
          (GstEncodingProfile *) vprof);

      gst_caps_unref (caps);
      camera->image_profile = (GstEncodingProfile *) prof;
      camera->image_profile_switch = TRUE;
    }

    camera->viewfinderbin_queue =
        gst_element_factory_make ("queue", "viewfinderbin-queue");
    if (!camera->viewfinderbin_queue) {
      missing_element_name = "queue";
      goto missing_element;
    }

    g_object_set (camera->viewfinderbin_queue, "leaky", 2, "silent", TRUE,
        "max-size-time", (guint64) 0, "max-size-bytes", (guint) 0,
        "max-size-buffers", (guint) 1, NULL);

    gst_bin_add_many (GST_BIN_CAST (camera),
        gst_object_ref (camera->video_encodebin),
        gst_object_ref (camera->videosink),
        gst_object_ref (camera->image_encodebin),
        gst_object_ref (camera->imagesink),
        gst_object_ref (camera->viewfinderbin_queue), NULL);

    gst_element_link_pads_full (camera->video_encodebin, "src",
        camera->videosink, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full (camera->image_encodebin, "src",
        camera->imagesink, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full (camera->viewfinderbin_queue, "src",
        camera->viewfinderbin_capsfilter, "sink", GST_PAD_LINK_CHECK_CAPS);
    gst_element_link_pads_full (camera->viewfinderbin_capsfilter, "src",
        camera->viewfinderbin, "sink", GST_PAD_LINK_CHECK_CAPS);

    {
      /* set an event probe to watch for custom location changes */
      GstPad *srcpad;

      srcpad = gst_element_get_static_pad (camera->image_encodebin, "src");

      gst_pad_add_event_probe (srcpad,
          (GCallback) gst_camera_bin_image_sink_event_probe, camera);

      gst_object_unref (srcpad);
    }

    /*
     * Video can't get into playing as its internal filesink will open
     * a file for writing and leave it empty if unused.
     *
     * Its state is managed using the current mode and the source's
     * ready-for-capture notify callback. When we are at video mode and
     * the source's ready-for-capture goes to FALSE it means it is
     * starting recording, so we should prepare the video bin.
     */
    gst_element_set_locked_state (camera->videosink, TRUE);
    gst_element_set_locked_state (camera->imagesink, TRUE);

    g_object_set (camera->videosink, "location", camera->location, NULL);
    g_object_set (camera->imagesink, "location", camera->location, NULL);
  }

  /* propagate the flags property by translating appropriate values
   * to GstEncFlags values */
  if (camera->flags & GST_CAM_FLAG_NO_AUDIO_CONVERSION)
    encbin_flags |= (1 << 0);
  if (camera->flags & GST_CAM_FLAG_NO_VIDEO_CONVERSION)
    encbin_flags |= (1 << 1);
  g_object_set (camera->video_encodebin, "flags", encbin_flags, NULL);

  /* image encodebin has only video branch so disable its conversion elements
   * appropriately */
  if (camera->flags & GST_CAM_FLAG_NO_IMAGE_CONVERSION)
    g_object_set (camera->image_encodebin, "flags", (1 << 1), NULL);

  g_object_set (camera->viewfinderbin, "disable-converters",
      camera->flags & GST_CAM_FLAG_NO_VIEWFINDER_CONVERSION ? TRUE : FALSE,
      NULL);

  if (camera->video_profile_switch) {
    GST_DEBUG_OBJECT (camera, "Switching video-encodebin's profile");
    g_object_set (camera->video_encodebin, "profile", camera->video_profile,
        NULL);
    if (GST_PAD_LINK_FAILED (gst_camera_bin_link_encodebin (camera,
                camera->video_encodebin, camera->videobin_capsfilter,
                VIDEO_PAD))) {
      goto fail;
    }
    camera->video_profile_switch = FALSE;

    /* used to trigger relinking further down */
    profile_switched = TRUE;
  }

  if (camera->image_profile_switch) {
    GST_DEBUG_OBJECT (camera, "Switching image-encodebin's profile");
    g_object_set (camera->image_encodebin, "profile", camera->image_profile,
        NULL);
    if (GST_PAD_LINK_FAILED (gst_camera_bin_link_encodebin (camera,
                camera->image_encodebin, camera->imagebin_capsfilter,
                VIDEO_PAD))) {
      goto fail;
    }
    camera->image_profile_switch = FALSE;
  }

  /* check if we need to replace the camera src */
  if (camera->src) {
    if (camera->user_src && camera->user_src != camera->src) {

      if (camera->src_capture_notify_id)
        g_signal_handler_disconnect (camera->src,
            camera->src_capture_notify_id);

      gst_bin_remove (GST_BIN_CAST (camera), camera->src);
      gst_object_unref (camera->src);
      camera->src = NULL;
    }
  }

  if (!camera->src) {
    if (camera->user_src) {
      camera->src = gst_object_ref (camera->user_src);
    } else {
      camera->src =
          gst_element_factory_make ("wrappercamerabinsrc", "camerasrc");
    }

    new_src = TRUE;
  }

  g_assert (camera->src != NULL);
  g_object_set (camera->src, "mode", camera->mode, NULL);
  if (camera->src) {
    if (g_object_class_find_property (G_OBJECT_GET_CLASS (camera->src),
            "preview-caps")) {
      g_object_set (camera->src, "post-previews", camera->post_previews,
          "preview-caps", camera->preview_caps, "preview-filter",
          camera->preview_filter, NULL);
    }
    g_signal_connect (G_OBJECT (camera->src), "notify::zoom",
        (GCallback) gst_camera_bin_src_notify_zoom_cb, camera);
    g_object_set (camera->src, "zoom", camera->zoom, NULL);
    g_signal_connect (G_OBJECT (camera->src), "notify::max-zoom",
        (GCallback) gst_camera_bin_src_notify_max_zoom_cb, camera);
  }
  if (new_src) {
    GstPad *imgsrc = gst_element_get_static_pad (camera->src, "imgsrc");

    gst_bin_add (GST_BIN_CAST (camera), gst_object_ref (camera->src));
    camera->src_capture_notify_id = g_signal_connect (G_OBJECT (camera->src),
        "notify::ready-for-capture",
        G_CALLBACK (gst_camera_bin_src_notify_readyforcapture), camera);
    gst_element_link_pads (camera->src, "vfsrc", camera->viewfinderbin_queue,
        "sink");

    if (!gst_element_link_pads (camera->src, "imgsrc",
            camera->imagebin_capsfilter, "sink")) {
      GST_ERROR_OBJECT (camera,
          "Failed to link camera source's imgsrc pad to image bin capsfilter");
      goto fail;
    }
    if (!gst_element_link_pads (camera->src, "vidsrc",
            camera->videobin_capsfilter, "sink")) {
      GST_ERROR_OBJECT (camera,
          "Failed to link camera source's vidsrc pad to video bin capsfilter");
      goto fail;
    }

    gst_pad_add_buffer_probe (imgsrc,
        (GCallback) gst_camera_bin_image_src_buffer_probe, camera);
    gst_object_unref (imgsrc);
  }

  gst_camera_bin_check_and_replace_filter (camera, &camera->image_filter,
      camera->user_image_filter, camera->src, camera->imagebin_capsfilter,
      "imgsrc");
  gst_camera_bin_check_and_replace_filter (camera, &camera->video_filter,
      camera->user_video_filter, camera->src, camera->videobin_capsfilter,
      "vidsrc");
  gst_camera_bin_check_and_replace_filter (camera, &camera->viewfinder_filter,
      camera->user_viewfinder_filter, camera->viewfinderbin_queue,
      camera->viewfinderbin_capsfilter, NULL);

  /* check if we need to replace the camera audio src */
  has_audio = gst_camera_bin_video_profile_has_audio (camera);
  if (camera->audio_src) {
    if ((camera->user_audio_src && camera->user_audio_src != camera->audio_src)
        || !has_audio) {
      gst_bin_remove (GST_BIN_CAST (camera), camera->audio_src);
      gst_bin_remove (GST_BIN_CAST (camera), camera->audio_volume);
      gst_bin_remove (GST_BIN_CAST (camera), camera->audio_capsfilter);
      gst_object_unref (camera->audio_src);
      camera->audio_src = NULL;
    }
  }

  if (!camera->audio_src && has_audio) {
    if (camera->user_audio_src) {
      camera->audio_src = gst_object_ref (camera->user_audio_src);
    } else {
      camera->audio_src =
          gst_element_factory_make (DEFAULT_AUDIO_SRC, "audiosrc");
      if (!camera->audio_src) {
        missing_element_name = DEFAULT_AUDIO_SRC;
        goto missing_element;
      }
    }

    gst_element_set_locked_state (camera->audio_src, TRUE);
    new_audio_src = TRUE;
  }

  if (new_audio_src) {
    GstPad *srcpad;

    if (g_object_class_find_property (G_OBJECT_GET_CLASS (camera->audio_src),
            "provide-clock")) {
      g_object_set (camera->audio_src, "provide-clock", FALSE, NULL);
    }
    gst_bin_add (GST_BIN_CAST (camera), gst_object_ref (camera->audio_src));
    gst_bin_add (GST_BIN_CAST (camera), gst_object_ref (camera->audio_volume));
    gst_bin_add (GST_BIN_CAST (camera),
        gst_object_ref (camera->audio_capsfilter));

    gst_element_link_pads_full (camera->audio_src, "src",
        camera->audio_volume, "sink", GST_PAD_LINK_CHECK_CAPS);
    gst_element_link_pads_full (camera->audio_volume, "src",
        camera->audio_capsfilter, "sink", GST_PAD_LINK_CHECK_CAPS);

    srcpad = gst_element_get_static_pad (camera->audio_src, "src");

    /* 1) drop EOS for audiosrc elements that push them on state_changes
     * (basesrc does this) 
     * 2) Fix newsegment events to have start time = first buffer ts */
    gst_pad_add_data_probe (srcpad,
        (GCallback) gst_camera_bin_audio_src_data_probe, camera);

    gst_object_unref (srcpad);
  }
  if (has_audio) {
    gst_camera_bin_check_and_replace_filter (camera, &camera->audio_filter,
        camera->user_audio_filter, camera->audio_src, camera->audio_volume,
        "src");
  }

  if ((profile_switched && has_audio) || new_audio_src) {
    if (GST_PAD_LINK_FAILED (gst_camera_bin_link_encodebin (camera,
                camera->video_encodebin, camera->audio_capsfilter,
                AUDIO_PAD))) {
      goto fail;
    }
  }

  camera->elements_created = TRUE;
  return TRUE;

missing_element:
  gst_element_post_message (GST_ELEMENT_CAST (camera),
      gst_missing_element_message_new (GST_ELEMENT_CAST (camera),
          missing_element_name));
  GST_ELEMENT_ERROR (camera, CORE, MISSING_PLUGIN,
      (_("Missing element '%s' - check your GStreamer installation."),
          missing_element_name), (NULL));
  goto fail;

fail:
  /* FIXME properly clean up */
  return FALSE;
}

static void
_gst_tag_list_free_maybe (GstTagList * taglist)
{
  if (taglist)
    gst_tag_list_free (taglist);
}

static GstStateChangeReturn
gst_camera_bin_change_state (GstElement * element, GstStateChange trans)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstCameraBin2 *camera = GST_CAMERA_BIN2_CAST (element);


  switch (trans) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_camera_bin_create_elements (camera)) {
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_CAMERA_BIN2_RESET_PROCESSING_COUNTER (camera);
      camera->audio_drop_eos = TRUE;
      camera->audio_send_newseg = FALSE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (GST_STATE (camera->videosink) >= GST_STATE_PAUSED)
        gst_element_set_state (camera->videosink, GST_STATE_READY);
      if (GST_STATE (camera->imagesink) >= GST_STATE_PAUSED)
        gst_element_set_state (camera->imagesink, GST_STATE_READY);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_element_set_state (camera->videosink, GST_STATE_NULL);
      gst_element_set_state (camera->imagesink, GST_STATE_NULL);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, trans);

  switch (trans) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (camera->audio_src && GST_STATE (camera->audio_src) >= GST_STATE_READY)
        gst_element_set_state (camera->audio_src, GST_STATE_READY);

      gst_tag_setter_reset_tags (GST_TAG_SETTER (camera));
      GST_CAMERA_BIN2_RESET_PROCESSING_COUNTER (camera);
      camera->video_state = GST_CAMERA_BIN_VIDEO_IDLE;

      g_mutex_lock (camera->image_capture_mutex);
      g_slist_foreach (camera->image_location_list, (GFunc) g_free, NULL);
      g_slist_free (camera->image_location_list);
      camera->image_location_list = NULL;

      g_slist_foreach (camera->image_tags_list,
          (GFunc) _gst_tag_list_free_maybe, NULL);
      g_slist_free (camera->image_tags_list);
      camera->image_tags_list = NULL;
      g_mutex_unlock (camera->image_capture_mutex);

      g_mutex_lock (camera->preview_list_mutex);
      g_slist_foreach (camera->preview_location_list, (GFunc) g_free, NULL);
      g_slist_free (camera->preview_location_list);
      camera->preview_location_list = NULL;
      g_mutex_unlock (camera->preview_list_mutex);

      /* explicitly set to READY as they might be outside of the bin */
      gst_element_set_state (camera->audio_volume, GST_STATE_READY);
      gst_element_set_state (camera->audio_capsfilter, GST_STATE_READY);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (camera->audio_src)
        gst_element_set_state (camera->audio_src, GST_STATE_NULL);

      /* explicitly set to NULL as they might be outside of the bin */
      gst_element_set_state (camera->audio_volume, GST_STATE_NULL);
      gst_element_set_state (camera->audio_capsfilter, GST_STATE_NULL);

      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_camera_bin_send_event (GstElement * element, GstEvent * event)
{
  GstCameraBin2 *camera = GST_CAMERA_BIN2_CAST (element);
  gboolean res;

  /* avoid losing our ref to send_event */
  gst_event_ref (event);

  res = GST_ELEMENT_CLASS (parent_class)->send_event (element, event);
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
    {
      GstState current;

      if (camera->videosink) {
        gst_element_get_state (camera->videosink, &current, NULL, 0);
        if (current <= GST_STATE_READY)
          gst_element_post_message (camera->videosink,
              gst_message_new_eos (GST_OBJECT (camera->videosink)));
      }
      if (camera->imagesink) {
        gst_element_get_state (camera->imagesink, &current, NULL, 0);
        if (current <= GST_STATE_READY)
          gst_element_post_message (camera->imagesink,
              gst_message_new_eos (GST_OBJECT (camera->imagesink)));
      }
      break;
    }

    default:
      break;
  }

  gst_event_unref (event);
  return res;
}

static void
gst_camera_bin_set_location (GstCameraBin2 * camera, const gchar * location)
{
  GST_DEBUG_OBJECT (camera, "Setting mode %d location to %s", camera->mode,
      location);
  g_free (camera->location);
  camera->location = g_strdup (location);
}

static void
gst_camera_bin_set_audio_src (GstCameraBin2 * camera, GstElement * src)
{
  GST_DEBUG_OBJECT (GST_OBJECT (camera),
      "Setting audio source %" GST_PTR_FORMAT, src);

  if (camera->user_audio_src)
    g_object_unref (camera->user_audio_src);

  if (src)
    g_object_ref (src);
  camera->user_audio_src = src;
}

static void
gst_camera_bin_set_camera_src (GstCameraBin2 * camera, GstElement * src)
{
  GST_DEBUG_OBJECT (GST_OBJECT (camera),
      "Setting camera source %" GST_PTR_FORMAT, src);

  if (camera->user_src)
    g_object_unref (camera->user_src);

  if (src)
    g_object_ref (src);
  camera->user_src = src;
}

static void
gst_camera_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCameraBin2 *camera = GST_CAMERA_BIN2_CAST (object);

  switch (prop_id) {
    case PROP_MODE:
      gst_camera_bin_change_mode (camera, g_value_get_enum (value));
      break;
    case PROP_LOCATION:
      gst_camera_bin_set_location (camera, g_value_get_string (value));
      break;
    case PROP_CAMERA_SRC:
      gst_camera_bin_set_camera_src (camera, g_value_get_object (value));
      break;
    case PROP_AUDIO_SRC:
      gst_camera_bin_set_audio_src (camera, g_value_get_object (value));
      break;
    case PROP_MUTE_AUDIO:
      g_object_set (camera->audio_volume, "mute", g_value_get_boolean (value),
          NULL);
      break;
    case PROP_AUDIO_CAPTURE_CAPS:{
      GST_DEBUG_OBJECT (camera,
          "Setting audio capture caps to %" GST_PTR_FORMAT,
          gst_value_get_caps (value));

      if (G_LIKELY (camera->audio_capsfilter)) {
        g_object_set (camera->audio_capsfilter, "caps",
            gst_value_get_caps (value), NULL);
      } else {
        GST_WARNING_OBJECT (camera, "Audio capsfilter missing");
      }
    }
      break;
    case PROP_IMAGE_CAPTURE_CAPS:{
      GstPad *pad = NULL;

      if (camera->src)
        pad =
            gst_element_get_static_pad (camera->src,
            GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME);

      GST_DEBUG_OBJECT (camera,
          "Setting image capture caps to %" GST_PTR_FORMAT,
          gst_value_get_caps (value));

      if (G_LIKELY (camera->imagebin_capsfilter)) {
        g_object_set (camera->imagebin_capsfilter, "caps",
            gst_value_get_caps (value), NULL);
      } else {
        GST_WARNING_OBJECT (camera, "Image capsfilter missing");
      }

      /* set the capsfilter caps and notify the src to renegotiate */
      if (pad) {
        GST_DEBUG_OBJECT (camera, "Pushing renegotiate on %s",
            GST_PAD_NAME (pad));
        gst_pad_send_event (pad, gst_camera_bin_new_event_renegotiate ());
        gst_object_unref (pad);
      }
    }
      break;
    case PROP_VIDEO_CAPTURE_CAPS:{
      GstPad *pad = NULL;

      if (camera->src)
        pad =
            gst_element_get_static_pad (camera->src,
            GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME);

      GST_DEBUG_OBJECT (camera,
          "Setting video capture caps to %" GST_PTR_FORMAT,
          gst_value_get_caps (value));

      /* set the capsfilter caps and notify the src to renegotiate */
      if (G_LIKELY (camera->videobin_capsfilter)) {
        g_object_set (camera->videobin_capsfilter, "caps",
            gst_value_get_caps (value), NULL);
      } else {
        GST_WARNING_OBJECT (camera, "Video capsfilter missing");
      }

      if (pad) {
        GST_DEBUG_OBJECT (camera, "Pushing renegotiate on %s",
            GST_PAD_NAME (pad));
        gst_pad_send_event (pad, gst_camera_bin_new_event_renegotiate ());
        gst_object_unref (pad);
      }
    }
      break;
    case PROP_VIEWFINDER_CAPS:{
      GstPad *pad = NULL;

      if (camera->src)
        pad =
            gst_element_get_static_pad (camera->src,
            GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME);

      GST_DEBUG_OBJECT (camera,
          "Setting viewfinder capture caps to %" GST_PTR_FORMAT,
          gst_value_get_caps (value));

      /* set the capsfilter caps and notify the src to renegotiate */
      if (G_LIKELY (camera->viewfinderbin_capsfilter)) {
        g_object_set (camera->viewfinderbin_capsfilter, "caps",
            gst_value_get_caps (value), NULL);
      } else {
        GST_WARNING_OBJECT (camera, "Viewfinder capsfilter missing");
      }

      if (pad) {
        GST_DEBUG_OBJECT (camera, "Pushing renegotiate on %s",
            GST_PAD_NAME (pad));
        gst_pad_send_event (pad, gst_camera_bin_new_event_renegotiate ());
        gst_object_unref (pad);
      }
    }
      break;
    case PROP_POST_PREVIEWS:
      camera->post_previews = g_value_get_boolean (value);
      if (camera->src
          && g_object_class_find_property (G_OBJECT_GET_CLASS (camera->src),
              "post-previews"))
        g_object_set (camera->src, "post-previews", camera->post_previews,
            NULL);
      break;
    case PROP_PREVIEW_CAPS:
      gst_caps_replace (&camera->preview_caps,
          (GstCaps *) gst_value_get_caps (value));
      if (camera->src
          && g_object_class_find_property (G_OBJECT_GET_CLASS (camera->src),
              "preview-caps"))
        g_object_set (camera->src, "preview-caps", camera->preview_caps, NULL);
      break;
    case PROP_VIDEO_ENCODING_PROFILE:
      if (camera->video_profile)
        gst_encoding_profile_unref (camera->video_profile);
      camera->video_profile =
          (GstEncodingProfile *) gst_value_dup_mini_object (value);
      camera->video_profile_switch = TRUE;
      break;
    case PROP_IMAGE_FILTER:
      if (camera->user_image_filter)
        g_object_unref (camera->user_image_filter);

      camera->user_image_filter = g_value_dup_object (value);
      break;
    case PROP_VIDEO_FILTER:
      if (camera->user_video_filter)
        g_object_unref (camera->user_video_filter);

      camera->user_video_filter = g_value_dup_object (value);
      break;
    case PROP_VIEWFINDER_FILTER:
      if (camera->user_viewfinder_filter)
        g_object_unref (camera->user_viewfinder_filter);

      camera->user_viewfinder_filter = g_value_dup_object (value);
      break;
    case PROP_PREVIEW_FILTER:
      if (camera->preview_filter)
        g_object_unref (camera->preview_filter);

      camera->preview_filter = g_value_dup_object (value);
      if (camera->src
          && g_object_class_find_property (G_OBJECT_GET_CLASS (camera->src),
              "preview-filter"))
        g_object_set (camera->src, "preview-filter", camera->preview_filter,
            NULL);
      break;
    case PROP_AUDIO_FILTER:
      if (camera->user_audio_filter)
        g_object_unref (camera->user_audio_filter);

      camera->user_audio_filter = g_value_dup_object (value);
      break;
    case PROP_VIEWFINDER_SINK:
      g_object_set (camera->viewfinderbin, "video-sink",
          g_value_get_object (value), NULL);
      break;
    case PROP_ZOOM:
      camera->zoom = g_value_get_float (value);
      /* limit to max-zoom */
      if (camera->zoom > camera->max_zoom) {
        GST_DEBUG_OBJECT (camera, "Clipping zoom %f to max-zoom %f",
            camera->zoom, camera->max_zoom);
        camera->zoom = camera->max_zoom;
      }
      if (camera->src)
        g_object_set (camera->src, "zoom", camera->zoom, NULL);
      break;
    case PROP_IMAGE_ENCODING_PROFILE:
      if (camera->image_profile)
        gst_encoding_profile_unref (camera->image_profile);
      camera->image_profile =
          (GstEncodingProfile *) gst_value_dup_mini_object (value);
      camera->image_profile_switch = TRUE;
      break;
    case PROP_FLAGS:
      camera->flags = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_camera_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCameraBin2 *camera = GST_CAMERA_BIN2_CAST (object);

  switch (prop_id) {
    case PROP_MODE:
      g_value_set_enum (value, camera->mode);
      break;
    case PROP_LOCATION:
      g_value_set_string (value, camera->location);
      break;
    case PROP_CAMERA_SRC:
      g_value_set_object (value, camera->user_src);
      break;
    case PROP_AUDIO_SRC:
      g_value_set_object (value, camera->user_audio_src);
      break;
    case PROP_MUTE_AUDIO:{
      gboolean mute;

      g_object_get (camera->audio_volume, "mute", &mute, NULL);
      g_value_set_boolean (value, mute);
      break;
    }
    case PROP_AUDIO_CAPTURE_SUPPORTED_CAPS:
    case PROP_VIDEO_CAPTURE_SUPPORTED_CAPS:
    case PROP_VIEWFINDER_SUPPORTED_CAPS:
    case PROP_IMAGE_CAPTURE_SUPPORTED_CAPS:{
      GstPad *pad;
      GstElement *element;
      GstCaps *caps;
      const gchar *padname;

      if (prop_id == PROP_VIDEO_CAPTURE_SUPPORTED_CAPS) {
        element = camera->src;
        padname = GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME;
      } else if (prop_id == PROP_IMAGE_CAPTURE_SUPPORTED_CAPS) {
        element = camera->src;
        padname = GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME;
      } else if (prop_id == PROP_VIEWFINDER_SUPPORTED_CAPS) {
        element = camera->src;
        padname = GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME;
      } else {
        element = camera->audio_src;
        padname = "src";
      }

      if (element) {
        pad = gst_element_get_static_pad (element, padname);

        g_assert (pad != NULL);

        /* TODO not sure if we want get_caps or get_allowed_caps to already
         * consider the full pipeline scenario and avoid picking a caps that
         * won't negotiate. Need to take care on the special case of the
         * pad being unlinked.
         */
        caps = gst_pad_get_caps_reffed (pad);
        if (caps) {
          gst_value_set_caps (value, caps);
          gst_caps_unref (caps);
        }

        gst_object_unref (pad);
      } else {
        GST_DEBUG_OBJECT (camera, "Source not created, can't get "
            "supported caps");
      }
    }
      break;
    case PROP_AUDIO_CAPTURE_CAPS:{
      GstCaps *caps = NULL;
      if (G_LIKELY (camera->audio_capsfilter)) {
        g_object_get (camera->audio_capsfilter, "caps", &caps, NULL);
      } else {
        GST_WARNING ("Missing audio capsfilter");
      }
      gst_value_set_caps (value, caps);
      gst_caps_unref (caps);
    }
      break;
    case PROP_IMAGE_CAPTURE_CAPS:{
      GstCaps *caps = NULL;
      if (G_LIKELY (camera->imagebin_capsfilter)) {
        g_object_get (camera->imagebin_capsfilter, "caps", &caps, NULL);
      } else {
        GST_WARNING ("Missing imagebin capsfilter");
      }
      gst_value_set_caps (value, caps);
      gst_caps_unref (caps);
    }
      break;
    case PROP_VIDEO_CAPTURE_CAPS:{
      GstCaps *caps = NULL;
      if (G_LIKELY (camera->videobin_capsfilter)) {
        g_object_get (camera->videobin_capsfilter, "caps", &caps, NULL);
      } else {
        GST_WARNING ("Missing imagebin capsfilter");
      }
      gst_value_set_caps (value, caps);
      gst_caps_unref (caps);
    }
      break;
    case PROP_VIEWFINDER_CAPS:{
      GstCaps *caps = NULL;
      if (G_LIKELY (camera->viewfinderbin_capsfilter)) {
        g_object_get (camera->viewfinderbin_capsfilter, "caps", &caps, NULL);
      } else {
        GST_WARNING ("Missing imagebin capsfilter");
      }
      gst_value_set_caps (value, caps);
      gst_caps_unref (caps);
    }
      break;
    case PROP_POST_PREVIEWS:
      g_value_set_boolean (value, camera->post_previews);
      break;
    case PROP_PREVIEW_CAPS:
      if (camera->preview_caps)
        gst_value_set_caps (value, camera->preview_caps);
      break;
    case PROP_VIDEO_ENCODING_PROFILE:
      if (camera->video_profile) {
        gst_value_set_mini_object (value,
            (GstMiniObject *) camera->video_profile);
      }
      break;
    case PROP_VIDEO_FILTER:
      if (camera->user_video_filter)
        g_value_set_object (value, camera->user_video_filter);
      break;
    case PROP_IMAGE_FILTER:
      if (camera->user_image_filter)
        g_value_set_object (value, camera->user_image_filter);
      break;
    case PROP_VIEWFINDER_FILTER:
      if (camera->user_viewfinder_filter)
        g_value_set_object (value, camera->user_viewfinder_filter);
      break;
    case PROP_AUDIO_FILTER:
      if (camera->user_audio_filter)
        g_value_set_object (value, camera->user_audio_filter);
      break;
    case PROP_PREVIEW_FILTER:
      if (camera->preview_filter)
        g_value_set_object (value, camera->preview_filter);
      break;
    case PROP_VIEWFINDER_SINK:{
      GstElement *sink;

      g_object_get (camera->viewfinderbin, "video-sink", &sink, NULL);
      g_value_take_object (value, sink);
      break;
    }
    case PROP_ZOOM:
      g_value_set_float (value, camera->zoom);
      break;
    case PROP_MAX_ZOOM:
      g_value_set_float (value, camera->max_zoom);
      break;
    case PROP_IMAGE_ENCODING_PROFILE:
      if (camera->image_profile) {
        gst_value_set_mini_object (value,
            (GstMiniObject *) camera->image_profile);
      }
      break;
    case PROP_IDLE:
      g_value_set_boolean (value,
          g_atomic_int_get (&camera->processing_counter) == 0);
      break;
    case PROP_FLAGS:
      g_value_set_flags (value, camera->flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

gboolean
gst_camera_bin2_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_camera_bin_debug, "camerabin2", 0, "CameraBin2");

  return gst_element_register (plugin, "camerabin2", GST_RANK_NONE,
      gst_camera_bin2_get_type ());
}
