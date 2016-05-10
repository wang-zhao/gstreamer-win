/* GStreamer
 *
 * unit test for x264enc
 *
 * Copyright (C) <2008> Mark Nauwelaerts <mnauw@users.sf.net>
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

#include <unistd.h>

#include <gst/check/gstcheck.h>

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

#define VIDEO_CAPS_STRING "video/x-raw-yuv, " \
                           "format = (fourcc) I420, " \
                           "width = (int) 384, " \
                           "height = (int) 288, " \
                           "framerate = (fraction) 25/1"

#define MPEG_CAPS_STRING "video/x-h264, " \
                           "width = (int) 384, " \
                           "height = (int) 288, " \
                           "framerate = (fraction) 25/1"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (MPEG_CAPS_STRING));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS_STRING));


static GstElement *
setup_x264enc (void)
{
  GstElement *x264enc;

  GST_DEBUG ("setup_x264enc");
  x264enc = gst_check_setup_element ("x264enc");
  mysrcpad = gst_check_setup_src_pad (x264enc, &srctemplate, NULL);
  mysinkpad = gst_check_setup_sink_pad (x264enc, &sinktemplate, NULL);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  return x264enc;
}

static void
cleanup_x264enc (GstElement * x264enc)
{
  GST_DEBUG ("cleanup_x264enc");
  gst_element_set_state (x264enc, GST_STATE_NULL);

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (x264enc);
  gst_check_teardown_sink_pad (x264enc);
  gst_check_teardown_element (x264enc);
}

static void
check_caps (GstCaps * caps)
{
  GstStructure *s;
  const GValue *sf, *avcc;
  const gchar *stream_format;

  fail_unless (caps != NULL);

  GST_INFO ("caps %" GST_PTR_FORMAT, caps);
  s = gst_caps_get_structure (caps, 0);
  fail_unless (s != NULL);
  fail_if (!gst_structure_has_name (s, "video/x-h264"));
  sf = gst_structure_get_value (s, "stream-format");
  fail_unless (sf != NULL);
  fail_unless (G_VALUE_HOLDS_STRING (sf));
  stream_format = g_value_get_string (sf);
  fail_unless (stream_format != NULL);
  if (strcmp (stream_format, "avc") == 0) {
    const guint8 *data;
    GstBuffer *buf;

    avcc = gst_structure_get_value (s, "codec_data");
    fail_unless (avcc != NULL);
    fail_unless (GST_VALUE_HOLDS_BUFFER (avcc));
    buf = gst_value_get_buffer (avcc);
    fail_unless (buf != NULL);
    data = GST_BUFFER_DATA (buf);
    fail_unless_equals_int (data[0], 1);
    /* should be either baseline, main profile or extended profile */
    fail_unless (data[1] == 0x42 || data[1] == 0x4D || data[1] == 0x58);
  } else if (strcmp (stream_format, "byte-stream") == 0) {
    fail_if (gst_structure_get_value (s, "codec_data") != NULL);
  } else {
    fail_if (TRUE, "unexpected stream-format in caps: %s", stream_format);
  }
}

GST_START_TEST (test_video_pad)
{
  GstElement *x264enc;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  int i, num_buffers;

  x264enc = setup_x264enc ();
  fail_unless (gst_element_set_state (x264enc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* corresponds to I420 buffer for the size mentioned in the caps */
  inbuffer = gst_buffer_new_and_alloc (384 * 288 * 3 / 2);
  /* makes valgrind's memcheck happier */
  memset (GST_BUFFER_DATA (inbuffer), 0, GST_BUFFER_SIZE (inbuffer));
  caps = gst_caps_from_string (VIDEO_CAPS_STRING);
  gst_buffer_set_caps (inbuffer, caps);
  gst_caps_unref (caps);
  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  /* send eos to have all flushed if needed */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()) == TRUE);

  num_buffers = g_list_length (buffers);
  fail_unless (num_buffers == 1);

  /* clean up buffers */
  for (i = 0; i < num_buffers; ++i) {
    outbuffer = GST_BUFFER (buffers->data);
    fail_if (outbuffer == NULL);

    switch (i) {
      case 0:
      {
        gint nsize, npos, j, type, next_type;
        guint8 *data = GST_BUFFER_DATA (outbuffer);
        gint size = GST_BUFFER_SIZE (outbuffer);

        check_caps (GST_BUFFER_CAPS (outbuffer));

        npos = 0;
        j = 0;
        /* need SPS first */
        next_type = 7;
        /* loop through NALs */
        while (npos < size) {
          fail_unless (size - npos >= 4);
          nsize = GST_READ_UINT32_BE (data + npos);
          fail_unless (nsize > 0);
          fail_unless (npos + 4 + nsize <= size);
          type = data[npos + 4] & 0x1F;
          /* check the first NALs, disregard AU (9), SEI (6) */
          if (type != 9 && type != 6) {
            fail_unless (type == next_type);
            switch (type) {
              case 7:
                /* SPS */
                next_type = 8;
                break;
              case 8:
                /* PPS */
                next_type = 5;
                break;
              default:
                break;
            }
            j++;
          }
          npos += nsize + 4;
        }
        /* should have reached the exact end */
        fail_unless (npos == size);
        break;
      }
      default:
        break;
    }
    buffers = g_list_remove (buffers, outbuffer);

    ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
    gst_buffer_unref (outbuffer);
    outbuffer = NULL;
  }

  cleanup_x264enc (x264enc);
  g_list_free (buffers);
  buffers = NULL;
}

GST_END_TEST;

GstCaps *pad_caps;

GstCaps *
getcaps_test (GstPad * pad)
{
  return gst_caps_ref (pad_caps);
}

GST_START_TEST (test_profile_in_caps)
{
  GstElement *x264enc;
  GstPad *srcpad;
  GstPad *sinkpad;
  GstStructure *s;

  pad_caps = gst_caps_from_string (MPEG_CAPS_STRING);

  x264enc = setup_x264enc ();
  gst_pad_set_getcaps_function (mysinkpad, getcaps_test);
  srcpad = gst_element_get_static_pad (x264enc, "src");
  sinkpad = gst_element_get_static_pad (x264enc, "sink");
  fail_unless (gst_element_set_state (x264enc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  fail_unless (gst_pad_set_caps (sinkpad,
          (GstCaps *) gst_pad_get_pad_template_caps (mysrcpad)));
  s = gst_caps_get_structure (GST_PAD_CAPS (srcpad), 0);
  fail_unless (!g_strcmp0 (gst_structure_get_string (s, "profile"), "main"));

  fail_unless (gst_element_set_state (x264enc,
          GST_STATE_READY) == GST_STATE_CHANGE_SUCCESS,
      "could not set to ready");
  g_object_set (x264enc, "profile", 1, NULL);
  fail_unless (gst_element_set_state (x264enc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");
  fail_unless (gst_pad_set_caps (sinkpad,
          (GstCaps *) gst_pad_get_pad_template_caps (mysrcpad)));
  s = gst_caps_get_structure (GST_PAD_CAPS (srcpad), 0);
  fail_unless (!g_strcmp0 (gst_structure_get_string (s, "profile"),
          "constrained-baseline"));

  fail_unless (gst_element_set_state (x264enc,
          GST_STATE_READY) == GST_STATE_CHANGE_SUCCESS,
      "could not set to ready");
  g_object_set (x264enc, "profile", 3, NULL);
  fail_unless (gst_element_set_state (x264enc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");
  fail_unless (gst_pad_set_caps (sinkpad,
          (GstCaps *) gst_pad_get_pad_template_caps (mysrcpad)));
  s = gst_caps_get_structure (GST_PAD_CAPS (srcpad), 0);
  fail_unless (!g_strcmp0 (gst_structure_get_string (s, "profile"), "high"));

  fail_unless (gst_element_set_state (x264enc,
          GST_STATE_READY) == GST_STATE_CHANGE_SUCCESS,
      "could not set to ready");
  g_object_set (x264enc, "profile", 2, NULL);
  fail_unless (gst_element_set_state (x264enc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");
  fail_unless (gst_pad_set_caps (sinkpad,
          (GstCaps *) gst_pad_get_pad_template_caps (mysrcpad)));
  s = gst_caps_get_structure (GST_PAD_CAPS (srcpad), 0);
  fail_unless (!g_strcmp0 (gst_structure_get_string (s, "profile"), "main"));

  s = gst_caps_get_structure (pad_caps, 0);
  gst_structure_set (s, "profile", G_TYPE_STRING, "constrained-baseline", NULL);
  fail_unless (gst_element_set_state (x264enc,
          GST_STATE_READY) == GST_STATE_CHANGE_SUCCESS,
      "could not set to ready");
  fail_unless (gst_element_set_state (x264enc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");
  fail_unless (gst_pad_set_caps (sinkpad,
          (GstCaps *) gst_pad_get_pad_template_caps (mysrcpad)));
  s = gst_caps_get_structure (GST_PAD_CAPS (srcpad), 0);
  fail_unless (!g_strcmp0 (gst_structure_get_string (s, "profile"),
          "constrained-baseline"));

  s = gst_caps_get_structure (pad_caps, 0);
  gst_structure_set (s, "profile", G_TYPE_STRING, "high", NULL);
  fail_unless (gst_element_set_state (x264enc,
          GST_STATE_READY) == GST_STATE_CHANGE_SUCCESS,
      "could not set to ready");
  fail_unless (gst_element_set_state (x264enc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");
  fail_unless (gst_pad_set_caps (sinkpad,
          (GstCaps *) gst_pad_get_pad_template_caps (mysrcpad)));
  s = gst_caps_get_structure (GST_PAD_CAPS (srcpad), 0);
  fail_unless (!g_strcmp0 (gst_structure_get_string (s, "profile"), "high"));

  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);
  cleanup_x264enc (x264enc);
  gst_caps_unref (pad_caps);
}

GST_END_TEST;

Suite *
x264enc_suite (void)
{
  Suite *s = suite_create ("x264enc");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_video_pad);
  tcase_add_test (tc_chain, test_profile_in_caps);

  return s;
}

int
main (int argc, char **argv)
{
  int nf;

  Suite *s = x264enc_suite ();
  SRunner *sr = srunner_create (s);

  gst_check_init (&argc, &argv);

  srunner_run_all (sr, CK_NORMAL);
  nf = srunner_ntests_failed (sr);
  srunner_free (sr);

  return nf;
}
