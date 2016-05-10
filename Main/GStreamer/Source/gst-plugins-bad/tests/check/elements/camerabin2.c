/* GStreamer
 *
 * unit test for camerabin2 basic operations
 * Copyright (C) 2010 Nokia Corporation <multimedia@maemo.org>
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
 *
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

#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/check/gstcheck.h>
#include <gst/basecamerabinsrc/gstbasecamerasrc.h>
#include <gst/pbutils/encoding-profile.h>

#define IMAGE_FILENAME "image"
#define VIDEO_FILENAME "video"
#define CAPTURE_COUNT 3
#define VIDEO_DURATION 5

#define VIDEO_PAD_SUPPORTED_CAPS GST_VIDEO_CAPS_RGB ", width=600, height=480"
#define IMAGE_PAD_SUPPORTED_CAPS GST_VIDEO_CAPS_RGB ", width=800, height=600"

/* custom test camera src element */
#define GST_TYPE_TEST_CAMERA_SRC \
  (gst_test_camera_src_get_type())
#define GST_TEST_CAMERA_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TEST_CAMERA_SRC,GstTestCameraSrc))
#define GST_TEST_CAMERA_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TEST_CAMERA_SRC,GstTestCameraSrcClass))
#define GST_IS_TEST_REVERSE_NEGOTIATION_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TEST_CAMERA_SRC))
#define GST_IS_TEST_REVERSE_NEGOTIATION_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TEST_CAMERA_SRC))
#define GST_TEST_CAMERA_SRC_CAST(obj) ((GstTestCameraSrc *)obj)

typedef struct _GstTestCameraSrc GstTestCameraSrc;
typedef struct _GstTestCameraSrcClass GstTestCameraSrcClass;
struct _GstTestCameraSrc
{
  GstBaseCameraSrc element;

  GstPad *vfpad;
  GstPad *vidpad;
  GstPad *imgpad;

  GstCameraBinMode mode;
};

struct _GstTestCameraSrcClass
{
  GstBaseCameraSrcClass parent_class;
};

GType gst_test_camera_src_get_type (void);

GST_BOILERPLATE (GstTestCameraSrc,
    gst_test_camera_src, GstBaseCameraSrc, GST_TYPE_BASE_CAMERA_SRC);

static gboolean
gst_test_camera_src_set_mode (GstBaseCameraSrc * src, GstCameraBinMode mode)
{
  GstTestCameraSrc *self = GST_TEST_CAMERA_SRC (src);

  self->mode = mode;
  return TRUE;
}

static GstCaps *
gst_test_camera_src_get_caps (GstPad * pad)
{
  GstTestCameraSrc *self = (GstTestCameraSrc *) GST_PAD_PARENT (pad);
  GstCaps *result = NULL;

  if (pad == self->vfpad) {
    result = gst_caps_new_any ();
  } else if (pad == self->vidpad) {
    result = gst_caps_from_string (VIDEO_PAD_SUPPORTED_CAPS);
  } else if (pad == self->imgpad) {
    result = gst_caps_from_string (IMAGE_PAD_SUPPORTED_CAPS);
  } else {
    g_assert_not_reached ();
  }

  return result;
}

static void
gst_test_camera_src_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstelement_class,
      "Test Camera Src",
      "Camera/Src",
      "Some test camera src",
      "Thiago Santos <thiago.sousa.santos@collabora.co.uk>");
}

static void
gst_test_camera_src_class_init (GstTestCameraSrcClass * klass)
{
  GstBaseCameraSrcClass *gstbasecamera_class;

  gstbasecamera_class = GST_BASE_CAMERA_SRC_CLASS (klass);
  gstbasecamera_class->set_mode = gst_test_camera_src_set_mode;
}

static void
gst_test_camera_src_init (GstTestCameraSrc * self,
    GstTestCameraSrcClass * g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);
  GstPadTemplate *template;

  /* create pads */
  template = gst_element_class_get_pad_template (gstelement_class,
      GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME);
  self->vfpad = gst_pad_new_from_template (template,
      GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME);
  gst_element_add_pad (GST_ELEMENT_CAST (self), self->vfpad);

  template = gst_element_class_get_pad_template (gstelement_class,
      GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME);
  self->imgpad = gst_pad_new_from_template (template,
      GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME);
  gst_element_add_pad (GST_ELEMENT_CAST (self), self->imgpad);

  template = gst_element_class_get_pad_template (gstelement_class,
      GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME);
  self->vidpad = gst_pad_new_from_template (template,
      GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME);
  gst_element_add_pad (GST_ELEMENT_CAST (self), self->vidpad);

  /* add get caps functions */
  gst_pad_set_getcaps_function (self->vfpad, gst_test_camera_src_get_caps);
  gst_pad_set_getcaps_function (self->vidpad, gst_test_camera_src_get_caps);
  gst_pad_set_getcaps_function (self->imgpad, gst_test_camera_src_get_caps);
}

/* end of custom test camera src element */


static GstElement *camera;
static guint bus_source;
static GMainLoop *main_loop;
static gint capture_count = 0;
guint32 test_id = 0;
static gchar *image_filename;
static gchar *video_filename;

static GstBuffer *preview_buffer;
static gchar *preview_filename;
static GstCaps *preview_caps;
static GstTagList *tags_found;

static gboolean
validity_bus_cb (GstBus * bus, GstMessage * message, gpointer data);

static GstMessage *wait_for_element_message (GstElement * camera,
    const gchar * name, GstClockTime timeout);

static void
validate_taglist_foreach (const GstTagList * list, const gchar * tag,
    gpointer user_data)
{
  GstTagList *other = GST_TAG_LIST (user_data);

  const GValue *val1 = gst_tag_list_get_value_index (list, tag, 0);
  const GValue *val2 = gst_tag_list_get_value_index (other, tag, 0);

  GST_DEBUG ("checking tag '%s'", tag);

  fail_if (val1 == NULL);
  fail_if (val2 == NULL);

  fail_unless (gst_value_compare (val1, val2) == GST_VALUE_EQUAL);
}


/* helper function for filenames */
static gchar *
make_test_file_name (const gchar * base_name, gint num)
{
  /* num == -1 means to keep the %d in the resulting string to be used on
   * multifilesink like location */
  if (num == -1) {
    return g_strdup_printf ("%s" G_DIR_SEPARATOR_S
        "gstcamerabin2test_%s_%u_%%03d.cap", g_get_tmp_dir (), base_name,
        test_id);
  } else {
    return g_strdup_printf ("%s" G_DIR_SEPARATOR_S
        "gstcamerabin2test_%s_%u_%03d.cap", g_get_tmp_dir (), base_name,
        test_id, num);
  }
}

static const gchar *
make_const_file_name (const gchar * filename, gint num)
{
  static gchar file_name[1000];

  /* num == -1 means to keep the %d in the resulting string to be used on
   * multifilesink like location */
  g_snprintf (file_name, 999, filename, num);

  return file_name;
}

/* configuration */

static gboolean
capture_bus_cb (GstBus * bus, GstMessage * message, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;
  const GstStructure *st;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *debug = NULL;

      gst_message_parse_error (message, &err, &debug);
      GST_WARNING ("ERROR: %s [%s]", err->message, debug);
      g_error_free (err);
      g_free (debug);
      /* Write debug graph to file */
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (camera),
          GST_DEBUG_GRAPH_SHOW_ALL, "camerabin.error");

      fail_if (TRUE, "error while capturing");
      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_WARNING:{
      GError *err = NULL;
      gchar *debug = NULL;

      gst_message_parse_warning (message, &err, &debug);
      GST_WARNING ("WARNING: %s [%s]", err->message, debug);
      g_error_free (err);
      g_free (debug);
      /* Write debug graph to file */
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (camera),
          GST_DEBUG_GRAPH_SHOW_ALL, "camerabin.warning");
      break;
    }
    case GST_MESSAGE_EOS:
      GST_DEBUG ("eos");
      g_main_loop_quit (loop);
      break;
    default:
      st = gst_message_get_structure (message);
      if (st && gst_structure_has_name (st, "image-done")) {
        GST_INFO ("image captured");
      } else if (st && gst_structure_has_name (st,
              GST_BASE_CAMERA_SRC_PREVIEW_MESSAGE_NAME)) {
        GstBuffer *buf;
        const GValue *value;

        value = gst_structure_get_value (st, "buffer");
        fail_unless (value != NULL);
        buf = gst_value_get_buffer (value);

        if (preview_buffer)
          gst_buffer_unref (preview_buffer);
        preview_buffer = gst_buffer_ref (buf);
        g_free (preview_filename);
        preview_filename = g_strdup (gst_structure_get_string (st, "location"));
      }
      break;
  }
  return TRUE;
}

static void
check_preview_image (GstElement * camera, const gchar * filename, gint index)
{
  gchar *prev_filename = NULL;

  if (!preview_buffer && camera) {
    GstMessage *msg = wait_for_element_message (camera,
        GST_BASE_CAMERA_SRC_PREVIEW_MESSAGE_NAME, GST_CLOCK_TIME_NONE);
    fail_unless (msg != NULL);
    gst_message_unref (msg);
  }
  fail_unless (preview_buffer != NULL);
  if (filename) {
    if (index >= 0) {
      prev_filename = g_strdup_printf (filename, index);
    } else {
      prev_filename = g_strdup (filename);
    }
    fail_unless (preview_filename != NULL);
    fail_unless (strcmp (preview_filename, prev_filename) == 0);
  }
  if (preview_caps) {
    fail_unless (GST_BUFFER_CAPS (preview_buffer) != NULL);
    fail_unless (gst_caps_can_intersect (GST_BUFFER_CAPS (preview_buffer),
            preview_caps));
  }
  g_free (prev_filename);
}

static void
extract_jpeg_tags (const gchar * filename, gint num)
{
  GstBus *bus;
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  const gchar *filepath = make_const_file_name (filename, num);
  gchar *pipeline_str = g_strdup_printf ("filesrc location=%s ! "
      "jpegparse ! fakesink", filepath);
  GstElement *pipeline;
  guint source;

  pipeline = gst_parse_launch (pipeline_str, NULL);
  fail_unless (pipeline != NULL);
  g_free (pipeline_str);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  source = gst_bus_add_watch (bus, (GstBusFunc) validity_bus_cb, loop);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (loop);
  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (bus);
  g_source_remove (source);
  gst_object_unref (pipeline);
  g_main_loop_unref (loop);
}

static void
setup_wrappercamerabinsrc_videotestsrc (void)
{
  GstBus *bus;
  GstElement *vfbin;
  GstElement *fakevideosink;
  GstElement *src;
  GstElement *testsrc;
  GstElement *audiosrc;

  GST_INFO ("init");

  test_id = g_random_int ();
  bus_source = 0;

  main_loop = g_main_loop_new (NULL, TRUE);

  camera = gst_check_setup_element ("camerabin2");
  fakevideosink = gst_element_factory_make ("fakesink", NULL);
  src = gst_element_factory_make ("wrappercamerabinsrc", NULL);
  testsrc = gst_element_factory_make ("videotestsrc", NULL);
  audiosrc = gst_element_factory_make ("audiotestsrc", NULL);

  preview_caps = gst_caps_new_simple ("video/x-raw-rgb", "width", G_TYPE_INT,
      320, "height", G_TYPE_INT, 240, NULL);

  g_object_set (G_OBJECT (testsrc), "is-live", TRUE, "peer-alloc", FALSE, NULL);
  g_object_set (G_OBJECT (audiosrc), "is-live", TRUE, NULL);
  g_object_set (G_OBJECT (src), "video-source", testsrc, NULL);
  g_object_set (G_OBJECT (camera), "camera-source", src, "preview-caps",
      preview_caps, "post-previews", TRUE, "audio-source", audiosrc, NULL);
  gst_object_unref (src);
  gst_object_unref (testsrc);
  gst_object_unref (audiosrc);

  vfbin = gst_bin_get_by_name (GST_BIN (camera), "vf-bin");
  g_object_set (G_OBJECT (vfbin), "video-sink", fakevideosink, NULL);
  gst_object_unref (vfbin);
  gst_object_unref (fakevideosink);

  bus = gst_pipeline_get_bus (GST_PIPELINE (camera));
  bus_source = gst_bus_add_watch (bus, (GstBusFunc) capture_bus_cb, main_loop);
  gst_object_unref (bus);

  tags_found = NULL;
  capture_count = 0;
  image_filename = make_test_file_name (IMAGE_FILENAME, -1);
  video_filename = make_test_file_name (VIDEO_FILENAME, -1);

  GST_INFO ("init finished");
}

static void
teardown (void)
{
  gst_element_set_state (camera, GST_STATE_NULL);

  if (camera)
    gst_check_teardown_element (camera);
  camera = NULL;

  if (bus_source)
    g_source_remove (bus_source);

  if (main_loop)
    g_main_loop_unref (main_loop);
  main_loop = NULL;

  if (preview_caps)
    gst_caps_unref (preview_caps);
  preview_caps = NULL;

  if (preview_buffer)
    gst_buffer_unref (preview_buffer);
  preview_buffer = NULL;

  g_free (preview_filename);
  preview_filename = NULL;

  if (tags_found)
    gst_tag_list_free (tags_found);
  tags_found = NULL;

  g_free (video_filename);
  video_filename = NULL;

  g_free (image_filename);
  image_filename = NULL;

  GST_INFO ("done");
}

static gboolean
validity_bus_cb (GstBus * bus, GstMessage * message, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *debug = NULL;

      gst_message_parse_error (message, &err, &debug);

      GST_ERROR ("Error: %s : %s", err->message, debug);
      g_error_free (err);
      g_free (debug);

      fail_if (TRUE, "validating captured data failed");
      g_main_loop_quit (loop);
    }
      break;
    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      GST_DEBUG ("eos");
      break;
    case GST_MESSAGE_TAG:{
      GstTagList *taglist = NULL;

      gst_message_parse_tag (message, &taglist);
      if (tags_found) {
        gst_tag_list_insert (tags_found, taglist, GST_TAG_MERGE_REPLACE);
        gst_tag_list_free (taglist);
      } else {
        tags_found = taglist;
      }
      GST_DEBUG ("tags: %" GST_PTR_FORMAT, tags_found);
    }
      break;
    default:
      break;
  }
  return TRUE;
}

/* checks that tags in @tags_a are in @tags_b */
static gboolean
taglist_is_subset (GstTagList * tags_a, GstTagList * tags_b)
{
  gst_tag_list_foreach (tags_a, validate_taglist_foreach, tags_b);
  return TRUE;
}

/* Validate captured files by playing them with playbin
 * and checking that no errors occur. */
#define WITH_AUDIO TRUE
#define NO_AUDIO FALSE
static gboolean
check_file_validity (const gchar * filename, gint num, GstTagList * taglist,
    gint width, gint height, gboolean has_audio)
{
  GstBus *bus;
  GstPad *pad;
  GstCaps *caps;
  gint caps_width, caps_height;
  GstState state;
  guint source;

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GstElement *playbin = gst_element_factory_make ("playbin2", NULL);
  GstElement *fakevideo = gst_element_factory_make ("fakesink", NULL);
  GstElement *fakeaudio = gst_element_factory_make ("fakesink", NULL);
  gchar *uri = g_strconcat ("file://", make_const_file_name (filename, num),
      NULL);

  GST_DEBUG ("checking uri: %s", uri);
  g_object_set (G_OBJECT (playbin), "uri", uri, "video-sink", fakevideo,
      "audio-sink", fakeaudio, NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (playbin));
  source = gst_bus_add_watch (bus, (GstBusFunc) validity_bus_cb, loop);

  gst_element_set_state (playbin, GST_STATE_PAUSED);
  gst_element_get_state (playbin, &state, NULL, GST_SECOND * 3);

  if (width != 0 && height != 0) {
    g_signal_emit_by_name (playbin, "get-video-pad", 0, &pad, NULL);
    g_assert (pad != NULL);
    caps = gst_pad_get_negotiated_caps (pad);

    g_assert (gst_structure_get_int (gst_caps_get_structure (caps, 0),
            "width", &caps_width));
    g_assert (gst_structure_get_int (gst_caps_get_structure (caps, 0),
            "height", &caps_height));

    g_assert (width == caps_width);
    g_assert (height == caps_height);

    gst_caps_unref (caps);
    gst_object_unref (pad);
  }
  if (has_audio) {
    g_signal_emit_by_name (playbin, "get-audio-pad", 0, &pad, NULL);
    g_assert (pad != NULL);
    gst_object_unref (pad);
  }

  gst_element_set_state (playbin, GST_STATE_PLAYING);
  g_main_loop_run (loop);
  gst_element_set_state (playbin, GST_STATE_NULL);

  /* special handling for images (jpg) as jpegparse isn't plugged by
   * default due to its current low rank */
  if (taglist && strstr (filename, "image")) {
    extract_jpeg_tags (filename, num);
  }

  if (taglist) {
    fail_unless (tags_found != NULL);
    fail_unless (taglist_is_subset (taglist, tags_found));
  }

  g_free (uri);
  g_source_remove (source);
  gst_object_unref (bus);
  gst_object_unref (playbin);
  g_main_loop_unref (loop);

  return TRUE;
}

static void
remove_file (const gchar * fn_template, guint num)
{
  const gchar *fn;

  fn = make_const_file_name (fn_template, num);
  GST_INFO ("removing %s", fn);
  g_unlink (fn);
}

static gboolean
filter_buffer_count (GstPad * pad, GstMiniObject * obj, gpointer data)
{
  gint *counter = data;

  (*counter)++;

  return TRUE;
}

static GstMessage *
wait_for_element_message (GstElement * camera, const gchar * name,
    GstClockTime timeout)
{
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (camera));
  GstMessage *msg;

  while (1) {
    msg = gst_bus_timed_pop_filtered (bus, timeout, GST_MESSAGE_ERROR |
        GST_MESSAGE_EOS | GST_MESSAGE_ELEMENT);

    if (msg) {
      if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ELEMENT) {
        const GstStructure *st = gst_message_get_structure (msg);
        if (gst_structure_has_name (st,
                GST_BASE_CAMERA_SRC_PREVIEW_MESSAGE_NAME)) {
          GstBuffer *buf;
          const GValue *value;

          value = gst_structure_get_value (st, "buffer");
          fail_unless (value != NULL);
          buf = gst_value_get_buffer (value);

          if (preview_buffer)
            gst_buffer_unref (preview_buffer);
          preview_buffer = gst_buffer_ref (buf);
          g_free (preview_filename);
          preview_filename =
              g_strdup (gst_structure_get_string (st, "location"));
        }

        if (gst_structure_has_name (st, name))
          break;
        else
          gst_message_unref (msg);
      } else {
        gst_message_unref (msg);
        msg = NULL;
        break;
      }
    }
  }

  gst_object_unref (bus);
  return msg;
}

static void
wait_for_idle_state (void)
{
  gboolean idle = FALSE;

  /* not the ideal way, but should be enough for testing */
  while (idle == FALSE) {
    g_object_get (camera, "idle", &idle, NULL);
    if (idle)
      break;

    GST_LOG ("waiting for idle state..");
    g_usleep (G_USEC_PER_SEC / 5);
  }
  fail_unless (idle);
}

GST_START_TEST (test_single_image_capture)
{
  gboolean idle;
  GstMessage *msg;
  if (!camera)
    return;

  /* set still image mode */
  g_object_set (camera, "mode", 1, "location", image_filename, NULL);

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }
  GST_INFO ("starting capture");
  fail_unless (camera != NULL);
  g_object_get (camera, "idle", &idle, NULL);
  fail_unless (idle);
  g_signal_emit_by_name (camera, "start-capture", NULL);

  msg = wait_for_element_message (camera, "image-done", GST_CLOCK_TIME_NONE);
  fail_unless (msg != NULL);
  gst_message_unref (msg);

  /* check that we got a preview image */
  check_preview_image (camera, image_filename, 0);

  wait_for_idle_state ();
  gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
  check_file_validity (image_filename, 0, NULL, 0, 0, NO_AUDIO);
  remove_file (image_filename, 0);
}

GST_END_TEST;


GST_START_TEST (test_multiple_image_captures)
{
  gboolean idle;
  gint i;
  gint widths[] = { 800, 640, 1280 };
  gint heights[] = { 600, 480, 1024 };

  if (!camera)
    return;

  /* set still image mode */
  g_object_set (camera, "mode", 1, "location", image_filename, NULL);

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }
  fail_unless (camera != NULL);
  g_object_get (camera, "idle", &idle, NULL);
  fail_unless (idle);
  GST_INFO ("starting capture");

  for (i = 0; i < 3; i++) {
    GstMessage *msg;
    GstCaps *caps;

    caps = gst_caps_new_simple ("video/x-raw-rgb", "width", G_TYPE_INT,
        widths[i], "height", G_TYPE_INT, heights[i], NULL);

    g_object_set (camera, "image-capture-caps", caps, NULL);
    gst_caps_unref (caps);

    g_signal_emit_by_name (camera, "start-capture", NULL);

    msg = wait_for_element_message (camera, "image-done", GST_CLOCK_TIME_NONE);
    fail_unless (msg != NULL);
    gst_message_unref (msg);

    check_preview_image (camera, image_filename, i);
  }

  wait_for_idle_state ();
  gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
  for (i = 0; i < 3; i++) {
    check_file_validity (image_filename, i, NULL, widths[i], heights[i],
        NO_AUDIO);
    remove_file (image_filename, i);
  }
}

GST_END_TEST;

GST_START_TEST (test_single_video_recording)
{
  GstMessage *msg;
  gboolean idle;
  if (!camera)
    return;

  /* Set video recording mode */
  g_object_set (camera, "mode", 2, "location", video_filename, NULL);

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }

  GST_INFO ("starting capture");
  fail_unless (camera != NULL);
  g_object_get (camera, "idle", &idle, NULL);
  fail_unless (idle);
  g_signal_emit_by_name (camera, "start-capture", NULL);

  g_object_get (camera, "idle", &idle, NULL);
  fail_unless (!idle);

  /* Record for one seconds  */
  g_timeout_add_seconds (VIDEO_DURATION, (GSourceFunc) g_main_loop_quit,
      main_loop);
  g_main_loop_run (main_loop);

  g_signal_emit_by_name (camera, "stop-capture", NULL);

  check_preview_image (camera, video_filename, 0);

  msg = wait_for_element_message (camera, "video-done", GST_CLOCK_TIME_NONE);
  fail_unless (msg != NULL);
  gst_message_unref (msg);

  wait_for_idle_state ();
  gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);

  check_file_validity (video_filename, 0, NULL, 0, 0, WITH_AUDIO);
  remove_file (video_filename, 0);

}

GST_END_TEST;

GST_START_TEST (test_multiple_video_recordings)
{
  gboolean idle;
  gint i;
  gint widths[] = { 800, 640, 1280 };
  gint heights[] = { 600, 480, 1024 };
  gint fr[] = { 20, 30, 5 };

  if (!camera)
    return;

  /* Set video recording mode */
  g_object_set (camera, "mode", 2, "location", video_filename, NULL);

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }

  GST_INFO ("starting capture");
  fail_unless (camera != NULL);
  g_object_get (camera, "idle", &idle, NULL);
  fail_unless (idle);
  for (i = 0; i < 3; i++) {
    GstMessage *msg;
    GstCaps *caps;

    caps = gst_caps_new_simple ("video/x-raw-rgb", "width", G_TYPE_INT,
        widths[i], "height", G_TYPE_INT, heights[i], "framerate",
        GST_TYPE_FRACTION, fr[i], 1, NULL);

    g_object_set (camera, "video-capture-caps", caps, NULL);

    gst_caps_unref (caps);

    GST_LOG ("starting #%d with caps %" GST_PTR_FORMAT, i, caps);
    g_signal_emit_by_name (camera, "start-capture", NULL);

    g_object_get (camera, "idle", &idle, NULL);
    fail_unless (!idle);

    g_timeout_add_seconds (VIDEO_DURATION, (GSourceFunc) g_main_loop_quit,
        main_loop);
    g_main_loop_run (main_loop);

    GST_LOG ("stopping run %d", i);
    g_signal_emit_by_name (camera, "stop-capture", NULL);

    msg = wait_for_element_message (camera, "video-done", GST_CLOCK_TIME_NONE);
    fail_unless (msg != NULL);
    gst_message_unref (msg);

    GST_LOG ("video done, checking preview image");
    check_preview_image (camera, video_filename, i);

    GST_LOG ("waiting for idle state");
    wait_for_idle_state ();
    GST_LOG ("finished run %d", i);
  }
  gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);

  for (i = 0; i < 3; i++) {
    check_file_validity (video_filename, i, NULL, widths[i], heights[i],
        WITH_AUDIO);
    remove_file (video_filename, i);
  }
}

GST_END_TEST;

GST_START_TEST (test_image_video_cycle)
{
  gint i;

  if (!camera)
    return;

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }

  GST_INFO ("starting capture");
  for (i = 0; i < 2; i++) {
    GstMessage *msg;
    const gchar *img_filename;
    const gchar *vid_filename;

    wait_for_idle_state ();

    /* take a picture */
    img_filename = make_const_file_name (image_filename, i);
    g_object_set (camera, "mode", 1, NULL);
    g_object_set (camera, "location", img_filename, NULL);
    g_signal_emit_by_name (camera, "start-capture", NULL);

    msg = wait_for_element_message (camera, "image-done", GST_CLOCK_TIME_NONE);
    fail_unless (msg != NULL);
    gst_message_unref (msg);

    check_preview_image (camera, img_filename, i);

    /* now go to video */
    vid_filename = make_const_file_name (video_filename, i);
    g_object_set (camera, "mode", 2, NULL);
    g_object_set (camera, "location", vid_filename, NULL);

    g_signal_emit_by_name (camera, "start-capture", NULL);
    g_timeout_add_seconds (VIDEO_DURATION, (GSourceFunc) g_main_loop_quit,
        main_loop);
    g_main_loop_run (main_loop);
    g_signal_emit_by_name (camera, "stop-capture", NULL);

    msg = wait_for_element_message (camera, "video-done", GST_CLOCK_TIME_NONE);
    fail_unless (msg != NULL);
    gst_message_unref (msg);

    check_preview_image (camera, vid_filename, i);
  }

  wait_for_idle_state ();
  gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);

  /* validate all the files */
  for (i = 0; i < 2; i++) {
    check_file_validity (image_filename, i, NULL, 0, 0, NO_AUDIO);
    remove_file (image_filename, i);
    check_file_validity (video_filename, i, NULL, 0, 0, WITH_AUDIO);
    remove_file (video_filename, i);
  }
}

GST_END_TEST;


GST_START_TEST (test_image_capture_previews)
{
  gint i;
  gint widths[] = { 800, 640, 1280 };
  gint heights[] = { 600, 480, 1024 };

  if (!camera)
    return;

  /* set still image mode */
  g_object_set (camera, "mode", 1, "location", image_filename, NULL);

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }
  fail_unless (camera != NULL);
  GST_INFO ("starting capture");

  for (i = 0; i < 3; i++) {
    GstMessage *msg;
    GstCaps *caps;

    caps = gst_caps_new_simple ("video/x-raw-rgb", "width", G_TYPE_INT,
        widths[i], "height", G_TYPE_INT, heights[i], NULL);

    g_object_set (camera, "preview-caps", caps, NULL);
    gst_caps_replace (&preview_caps, caps);
    gst_caps_unref (caps);

    g_signal_emit_by_name (camera, "start-capture", NULL);

    msg = wait_for_element_message (camera, "image-done", GST_CLOCK_TIME_NONE);
    fail_unless (msg != NULL);
    gst_message_unref (msg);

    check_preview_image (camera, image_filename, i);
    remove_file (image_filename, i);

    if (preview_buffer)
      gst_buffer_unref (preview_buffer);
    preview_buffer = NULL;
    gst_caps_replace (&preview_caps, NULL);
  }

  gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
}

GST_END_TEST;


GST_START_TEST (test_image_capture_with_tags)
{
  gint i;
  GstTagList *taglists[3];

  if (!camera)
    return;

  taglists[0] = gst_tag_list_new_full (GST_TAG_COMMENT, "test1",
      GST_TAG_GEO_LOCATION_LATITUDE, 36.6, GST_TAG_GEO_LOCATION_LONGITUDE,
      -12.5,
      GST_TAG_COPYRIGHT, "My copyright notice",
      GST_TAG_DEVICE_MANUFACTURER, "MyFavoriteBrand",
      GST_TAG_DEVICE_MODEL, "123v42.1",
      GST_TAG_DESCRIPTION, "some description",
      GST_TAG_APPLICATION_NAME, "camerabin2 test",
      GST_TAG_GEO_LOCATION_ELEVATION, 300.85, NULL);
  taglists[1] = gst_tag_list_new_full (GST_TAG_COMMENT, "test2",
      GST_TAG_GEO_LOCATION_LATITUDE, 1.6, GST_TAG_GEO_LOCATION_LONGITUDE,
      0.0,
      GST_TAG_COPYRIGHT, "some cp",
      GST_TAG_DEVICE_MANUFACTURER, "ABRAND",
      GST_TAG_DEVICE_MODEL, "abcd",
      GST_TAG_DESCRIPTION, "desc",
      GST_TAG_APPLICATION_NAME, "another cam test",
      GST_TAG_GEO_LOCATION_ELEVATION, 10.0, NULL);
  taglists[2] = gst_tag_list_new_full (GST_TAG_COMMENT, "test3",
      GST_TAG_GEO_LOCATION_LATITUDE, 1.3, GST_TAG_GEO_LOCATION_LONGITUDE,
      -5.0,
      GST_TAG_COPYRIGHT, "CC",
      GST_TAG_DEVICE_MANUFACTURER, "Homemade",
      GST_TAG_DEVICE_MODEL, "xpto",
      GST_TAG_DESCRIPTION, "another  description",
      GST_TAG_APPLICATION_NAME, "cam2 test",
      GST_TAG_GEO_LOCATION_ELEVATION, 0.0, NULL);

  /* set still image mode */
  g_object_set (camera, "mode", 1, "location", image_filename, NULL);

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }
  fail_unless (camera != NULL);
  GST_INFO ("starting capture");

  for (i = 0; i < 3; i++) {
    GstMessage *msg;
    gst_tag_setter_merge_tags (GST_TAG_SETTER (camera), taglists[i],
        GST_TAG_MERGE_REPLACE);

    g_signal_emit_by_name (camera, "start-capture", NULL);

    msg = wait_for_element_message (camera, "image-done", GST_CLOCK_TIME_NONE);
    fail_unless (msg != NULL);
    gst_message_unref (msg);
  }

  gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);

  for (i = 0; i < 3; i++) {
    check_file_validity (image_filename, i, taglists[i], 0, 0, NO_AUDIO);
    gst_tag_list_free (taglists[i]);
    remove_file (image_filename, i);
  }
}

GST_END_TEST;


GST_START_TEST (test_video_capture_with_tags)
{
  gint i;
  GstTagList *taglists[3];

  if (!camera)
    return;

  taglists[0] = gst_tag_list_new_full (GST_TAG_COMMENT, "test1", NULL);
  taglists[1] = gst_tag_list_new_full (GST_TAG_COMMENT, "test2", NULL);
  taglists[2] = gst_tag_list_new_full (GST_TAG_COMMENT, "test3", NULL);

  /* set video mode */
  g_object_set (camera, "mode", 2, "location", video_filename, NULL);

  /* set a profile that has xmp support for more tags being saved */
  {
    GstEncodingContainerProfile *profile;
    GstCaps *caps;

    caps =
        gst_caps_new_simple ("video/quicktime", "variant", G_TYPE_STRING,
        "apple", NULL);
    profile = gst_encoding_container_profile_new ("qt", "jpeg+qt", caps, NULL);
    gst_caps_unref (caps);

    caps = gst_caps_new_simple ("image/jpeg", NULL);
    if (!gst_encoding_container_profile_add_profile (profile,
            (GstEncodingProfile *) gst_encoding_video_profile_new (caps,
                NULL, NULL, 1))) {
      GST_WARNING_OBJECT (camera, "Failed to create encoding profiles");
    }
    gst_caps_unref (caps);

    g_object_set (camera, "video-profile", profile, NULL);
    gst_encoding_profile_unref (profile);
  }

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }
  fail_unless (camera != NULL);
  GST_INFO ("starting capture");

  for (i = 0; i < 3; i++) {
    GstMessage *msg;

    gst_tag_setter_merge_tags (GST_TAG_SETTER (camera), taglists[i],
        GST_TAG_MERGE_REPLACE);

    g_signal_emit_by_name (camera, "start-capture", NULL);

    g_timeout_add_seconds (3, (GSourceFunc) g_main_loop_quit, main_loop);
    g_main_loop_run (main_loop);

    g_signal_emit_by_name (camera, "stop-capture", NULL);

    msg = wait_for_element_message (camera, "video-done", GST_CLOCK_TIME_NONE);
    fail_unless (msg != NULL);
    gst_message_unref (msg);
  }

  gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);

  for (i = 0; i < 3; i++) {
    check_file_validity (video_filename, i, taglists[i], 0, 0, NO_AUDIO);
    gst_tag_list_free (taglists[i]);
    remove_file (video_filename, i);
  }
}

GST_END_TEST;


GST_START_TEST (test_supported_caps)
{
  GstCaps *padcaps = NULL;
  GstCaps *expectedcaps;
  GstElement *src;

  if (!camera)
    return;

  src = g_object_new (GST_TYPE_TEST_CAMERA_SRC, NULL);
  g_object_set (camera, "camera-source", src, NULL);
  gst_object_unref (src);

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }
  g_assert (camera != NULL);

  expectedcaps = gst_caps_from_string (VIDEO_PAD_SUPPORTED_CAPS);
  g_object_get (G_OBJECT (camera), "video-capture-supported-caps", &padcaps,
      NULL);
  g_assert (expectedcaps != NULL);
  g_assert (padcaps != NULL);
  g_assert (gst_caps_is_equal (padcaps, expectedcaps));
  gst_caps_unref (expectedcaps);
  gst_caps_unref (padcaps);

  expectedcaps = gst_caps_from_string (IMAGE_PAD_SUPPORTED_CAPS);
  g_object_get (G_OBJECT (camera), "image-capture-supported-caps", &padcaps,
      NULL);
  g_assert (expectedcaps != NULL);
  g_assert (padcaps != NULL);
  g_assert (gst_caps_is_equal (padcaps, expectedcaps));
  gst_caps_unref (expectedcaps);
  gst_caps_unref (padcaps);

  gst_element_set_state (camera, GST_STATE_NULL);
}

GST_END_TEST;


GST_START_TEST (test_idle_property)
{
  GstMessage *msg;
  gboolean idle;
  if (!camera)
    return;

  /* Set video recording mode */
  g_object_set (camera, "mode", 2, "location", video_filename, NULL);

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }

  GST_INFO ("starting capture");
  fail_unless (camera != NULL);
  g_object_get (camera, "idle", &idle, NULL);
  fail_unless (idle);
  g_signal_emit_by_name (camera, "start-capture", NULL);
  g_object_get (camera, "idle", &idle, NULL);
  fail_unless (!idle);

  /* emit a second start-capture that should be ignored */
  g_signal_emit_by_name (camera, "start-capture", NULL);
  g_object_get (camera, "idle", &idle, NULL);
  fail_unless (!idle);

  /* Record for one seconds  */
  g_timeout_add_seconds (VIDEO_DURATION, (GSourceFunc) g_main_loop_quit,
      main_loop);
  g_main_loop_run (main_loop);

  g_signal_emit_by_name (camera, "stop-capture", NULL);

  msg = wait_for_element_message (camera, "video-done", GST_CLOCK_TIME_NONE);
  fail_unless (msg != NULL);
  gst_message_unref (msg);

  check_preview_image (camera, video_filename, 0);

  wait_for_idle_state ();

  gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);

  check_file_validity (video_filename, 0, NULL, 0, 0, WITH_AUDIO);
  remove_file (video_filename, 0);
}

GST_END_TEST;


GST_START_TEST (test_image_custom_filter)
{
  GstElement *vf_filter;
  GstElement *image_filter;
  GstElement *preview_filter;
  GstPad *pad;
  gint vf_probe_counter = 0;
  gint image_probe_counter = 0;
  gint preview_probe_counter = 0;

  if (!camera)
    return;

  vf_filter = gst_element_factory_make ("identity", "vf-filter");
  image_filter = gst_element_factory_make ("identity", "img-filter");
  preview_filter = gst_element_factory_make ("identity", "preview-filter");

  pad = gst_element_get_static_pad (vf_filter, "src");
  gst_pad_add_buffer_probe (pad, (GCallback) filter_buffer_count,
      &vf_probe_counter);
  gst_object_unref (pad);

  pad = gst_element_get_static_pad (image_filter, "src");
  gst_pad_add_buffer_probe (pad, (GCallback) filter_buffer_count,
      &image_probe_counter);
  gst_object_unref (pad);

  pad = gst_element_get_static_pad (preview_filter, "src");
  gst_pad_add_buffer_probe (pad, (GCallback) filter_buffer_count,
      &preview_probe_counter);
  gst_object_unref (pad);

  /* set still image mode and filters */
  g_object_set (camera, "mode", 1,
      "location", image_filename,
      "viewfinder-filter", vf_filter, "image-filter", image_filter,
      "preview-filter", preview_filter, NULL);

  gst_object_unref (vf_filter);
  gst_object_unref (preview_filter);
  gst_object_unref (image_filter);

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }
  GST_INFO ("starting capture");
  fail_unless (camera != NULL);
  g_signal_emit_by_name (camera, "start-capture", NULL);

  g_timeout_add_seconds (3, (GSourceFunc) g_main_loop_quit, main_loop);
  g_main_loop_run (main_loop);

  /* check that we got a preview image */
  check_preview_image (camera, image_filename, 0);

  gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
  check_file_validity (image_filename, 0, NULL, 0, 0, NO_AUDIO);
  remove_file (image_filename, 0);

  fail_unless (vf_probe_counter > 0);
  fail_unless (image_probe_counter == 1);
  fail_unless (preview_probe_counter == 1);
}

GST_END_TEST;


GST_START_TEST (test_video_custom_filter)
{
  GstElement *vf_filter;
  GstElement *video_filter;
  GstElement *preview_filter;
  GstElement *audio_filter;
  GstPad *pad;
  gint vf_probe_counter = 0;
  gint video_probe_counter = 0;
  gint preview_probe_counter = 0;
  gint audio_probe_counter = 0;

  if (!camera)
    return;

  vf_filter = gst_element_factory_make ("identity", "vf-filter");
  video_filter = gst_element_factory_make ("identity", "video-filter");
  preview_filter = gst_element_factory_make ("identity", "preview-filter");
  audio_filter = gst_element_factory_make ("identity", "audio-filter");

  pad = gst_element_get_static_pad (vf_filter, "src");
  gst_pad_add_buffer_probe (pad, (GCallback) filter_buffer_count,
      &vf_probe_counter);
  gst_object_unref (pad);

  pad = gst_element_get_static_pad (video_filter, "src");
  gst_pad_add_buffer_probe (pad, (GCallback) filter_buffer_count,
      &video_probe_counter);
  gst_object_unref (pad);

  pad = gst_element_get_static_pad (audio_filter, "src");
  gst_pad_add_buffer_probe (pad, (GCallback) filter_buffer_count,
      &audio_probe_counter);
  gst_object_unref (pad);

  pad = gst_element_get_static_pad (preview_filter, "src");
  gst_pad_add_buffer_probe (pad, (GCallback) filter_buffer_count,
      &preview_probe_counter);
  gst_object_unref (pad);

  /* set still image mode and filters */
  g_object_set (camera, "mode", 2,
      "location", video_filename,
      "viewfinder-filter", vf_filter, "video-filter", video_filter,
      "preview-filter", preview_filter, "audio-filter", audio_filter, NULL);

  gst_object_unref (vf_filter);
  gst_object_unref (preview_filter);
  gst_object_unref (video_filter);
  gst_object_unref (audio_filter);

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }
  GST_INFO ("starting capture");
  fail_unless (camera != NULL);
  g_signal_emit_by_name (camera, "start-capture", NULL);

  g_timeout_add_seconds (VIDEO_DURATION, (GSourceFunc) g_main_loop_quit,
      main_loop);
  g_main_loop_run (main_loop);
  g_signal_emit_by_name (camera, "stop-capture", NULL);

  /* check that we got a preview image */
  check_preview_image (camera, video_filename, 0);

  gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
  check_file_validity (video_filename, 0, NULL, 0, 0, WITH_AUDIO);
  remove_file (video_filename, 0);

  fail_unless (vf_probe_counter > 0);
  fail_unless (video_probe_counter > 0);
  fail_unless (audio_probe_counter > 0);
  fail_unless (preview_probe_counter == 1);
}

GST_END_TEST;

#define LOCATION_SWITCHING_FILENAMES_COUNT 5

static gboolean
image_location_switch_do_capture (gpointer data)
{
  gchar **filenames = data;
  if (capture_count >= LOCATION_SWITCHING_FILENAMES_COUNT) {
    g_main_loop_quit (main_loop);
  }

  g_object_set (camera, "location", filenames[capture_count], NULL);
  g_signal_emit_by_name (camera, "start-capture", NULL);
  capture_count++;
  return FALSE;
}

static void
image_location_switch_readyforcapture (GObject * obj, GParamSpec * pspec,
    gpointer user_data)
{
  gboolean ready;

  g_object_get (obj, "ready-for-capture", &ready, NULL);
  if (ready) {
    g_idle_add (image_location_switch_do_capture, user_data);
  }
};

/*
 * Tests that setting the location and then doing an image
 * capture will set this capture resulting filename to the
 * correct location.
 *
 * There was a bug in which setting the location, issuing a capture 
 * and then setting a new location would cause this capture to have
 * the location set after this capture. This test should prevent it
 * from happening again.
 */
GST_START_TEST (test_image_location_switching)
{
  gchar *filenames[LOCATION_SWITCHING_FILENAMES_COUNT + 1];
  gint i;
  glong notify_id;
  GstCaps *caps;
  GstElement *src;
  GstMessage *msg;

  if (!camera)
    return;

  g_object_get (camera, "camera-source", &src, NULL);

  for (i = 0; i < LOCATION_SWITCHING_FILENAMES_COUNT; i++) {
    filenames[i] = make_test_file_name ("image-switching-filename-test", i);
  }
  filenames[LOCATION_SWITCHING_FILENAMES_COUNT] = NULL;

  /* set still image mode */
  g_object_set (camera, "mode", 1, NULL);
  caps = gst_caps_new_simple ("video/x-raw-rgb", "width", G_TYPE_INT,
      800, "height", G_TYPE_INT, 600, NULL);
  g_object_set (camera, "image-capture-caps", caps, NULL);
  gst_caps_unref (caps);

  if (gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    GST_WARNING ("setting camerabin to PLAYING failed");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);
    gst_object_unref (camera);
    camera = NULL;
  }
  fail_unless (camera != NULL);
  GST_INFO ("starting capture");

  notify_id = g_signal_connect (G_OBJECT (src),
      "notify::ready-for-capture",
      G_CALLBACK (image_location_switch_readyforcapture), filenames);

  g_idle_add (image_location_switch_do_capture, filenames);
  g_main_loop_run (main_loop);

  msg = wait_for_element_message (camera, "image-done", GST_CLOCK_TIME_NONE);
  fail_unless (msg != NULL);
  gst_message_unref (msg);

  gst_element_set_state (GST_ELEMENT (camera), GST_STATE_NULL);

  for (i = 0; i < LOCATION_SWITCHING_FILENAMES_COUNT; i++) {
    GST_INFO ("Checking for file: %s", filenames[i]);
    fail_unless (g_file_test (filenames[i], G_FILE_TEST_IS_REGULAR));
  }

  for (i = 0; i < LOCATION_SWITCHING_FILENAMES_COUNT; i++) {
    g_unlink (filenames[i]);
    g_free (filenames[i]);
  }
  g_signal_handler_disconnect (src, notify_id);
}

GST_END_TEST;


typedef struct _TestCaseDef
{
  const gchar *name;
  gpointer setup_func;
} TestCaseDef;

TestCaseDef tests[] = {
  {"wrappercamerabinsrc", setup_wrappercamerabinsrc_videotestsrc}
};

static Suite *
camerabin_suite (void)
{
  GstElementFactory *jpegenc_factory;
  Suite *s = suite_create ("camerabin2");
  gint i;
  TCase *tc_generic = tcase_create ("generic");

  jpegenc_factory = gst_element_factory_find ("jpegenc");
  if (jpegenc_factory == NULL) {
    GST_WARNING ("Skipping camerabin2 tests because jpegenc is missing");
    goto end;
  }

  suite_add_tcase (s, tc_generic);
  tcase_add_checked_fixture (tc_generic, setup_wrappercamerabinsrc_videotestsrc,
      teardown);
  tcase_add_test (tc_generic, test_supported_caps);

  for (i = 0; i < G_N_ELEMENTS (tests); i++) {
    TCase *tc_basic = tcase_create (tests[i].name);
    suite_add_tcase (s, tc_basic);

    /* Increase timeout due to video recording */
    tcase_set_timeout (tc_basic, 60);
    tcase_add_checked_fixture (tc_basic, tests[i].setup_func, teardown);

    tcase_add_test (tc_basic, test_single_image_capture);
    tcase_add_test (tc_basic, test_single_video_recording);
    tcase_add_test (tc_basic, test_image_video_cycle);
    if (gst_plugin_feature_check_version ((GstPluginFeature *) jpegenc_factory,
            0, 10, 27))
      tcase_add_test (tc_basic, test_multiple_image_captures);
    else
      GST_WARNING ("Skipping image capture test because -good 0.10.27 is "
          "needed");
    tcase_add_test (tc_basic, test_multiple_video_recordings);

    tcase_add_test (tc_basic, test_image_capture_previews);
    tcase_add_test (tc_basic, test_image_capture_with_tags);

    tcase_add_test (tc_basic, test_video_capture_with_tags);

    tcase_add_test (tc_basic, test_idle_property);

    tcase_add_test (tc_basic, test_image_custom_filter);
    tcase_add_test (tc_basic, test_video_custom_filter);

    tcase_add_test (tc_basic, test_image_location_switching);
  }

end:
  return s;
}

GST_CHECK_MAIN (camerabin);
