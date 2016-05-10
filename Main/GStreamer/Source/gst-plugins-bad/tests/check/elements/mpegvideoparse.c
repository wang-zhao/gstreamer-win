/*
 * GStreamer
 *
 * unit test for mpegvideoparse
 *
 * Copyright (C) 2011 Nokia Corporation. All rights reserved.
 *
 * Contact: Stefan Kost <stefan.kost@nokia.com>
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

#include <gst/check/gstcheck.h>
#include "parser.h"

#define SRC_CAPS_TMPL   "video/mpeg, framed=(boolean)false"
#define SINK_CAPS_TMPL  "video/mpeg, framed=(boolean)true"

GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SINK_CAPS_TMPL)
    );

GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SRC_CAPS_TMPL)
    );

/* some data */

/* actually seq + gop */
static guint8 mpeg2_seq[] = {
  0x00, 0x00, 0x01, 0xb3, 0x02, 0x00, 0x18, 0x15,
  0xff, 0xff, 0xe0, 0x28, 0x00, 0x00, 0x01, 0xb5,
  0x14, 0x8a, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x01, 0xb8, 0x00, 0x08, 0x00, 0x00
};

/* actually seq + gop */
static guint8 mpeg1_seq[] = {
  0x00, 0x00, 0x01, 0xb3, 0x02, 0x00, 0x18, 0x15,
  0xff, 0xff, 0xe0, 0x28, 0x00, 0x00, 0x01, 0xb8,
  0x00, 0x08, 00, 00
};

/* keyframes all around */
static guint8 mpeg2_iframe[] = {
  0x00, 0x00, 0x01, 0x00, 0x00, 0x0f, 0xff, 0xf8,
  0x00, 0x00, 0x01, 0xb5, 0x8f, 0xff, 0xf3, 0x41,
  0x80, 0x00, 0x00, 0x01, 0x01, 0x23, 0xf8, 0x7d,
  0x29, 0x48, 0x8b, 0x94, 0xa5, 0x22, 0x20, 0x00,
  0x00, 0x01, 0x02, 0x23, 0xf8, 0x7d, 0x29, 0x48,
  0x8b, 0x94, 0xa5, 0x22, 0x20
};

static gboolean
verify_buffer (buffer_verify_data_s * vdata, GstBuffer * buffer)
{
  /* check initial header special case, otherwise delegate to default */
  if (vdata->discard) {
    /* header is separate */
    fail_unless (GST_BUFFER_SIZE (buffer) == ctx_headers[0].size - 8);
    fail_unless (memcmp (GST_BUFFER_DATA (buffer), ctx_headers[0].data,
            GST_BUFFER_SIZE (buffer)) == 0);
  } else {
    /* header is merged in initial frame */
    if (vdata->buffer_counter == 0) {
      fail_unless (GST_BUFFER_SIZE (buffer) > 4);
      if (GST_READ_UINT32_BE (GST_BUFFER_DATA (buffer)) == 0x1b3) {
        /* the whole sequence header is included */
        fail_unless (GST_BUFFER_SIZE (buffer) ==
            ctx_headers[0].size + vdata->data_to_verify_size);
        fail_unless (memcmp (GST_BUFFER_DATA (buffer), ctx_headers[0].data,
                ctx_headers[0].size) == 0);
        fail_unless (memcmp (GST_BUFFER_DATA (buffer) + ctx_headers[0].size,
                vdata->data_to_verify, vdata->data_to_verify_size) == 0);
      } else {
        /* sequence was separate, only gop here */
        fail_unless (GST_BUFFER_SIZE (buffer) ==
            8 + vdata->data_to_verify_size);
        fail_unless (memcmp (GST_BUFFER_DATA (buffer),
                ctx_headers[0].data + ctx_headers[0].size - 8, 8) == 0);
        fail_unless (memcmp (GST_BUFFER_DATA (buffer) + 8,
                vdata->data_to_verify, vdata->data_to_verify_size) == 0);
      }
      return TRUE;
    }
  }

  return FALSE;
}

#define  GOP_SPLIT           "gop-split"

static GstElement *
setup_element (const gchar * desc)
{
  GstElement *element;

  if (strcmp (desc, GOP_SPLIT) == 0) {
    element = gst_check_setup_element ("mpegvideoparse");
    g_object_set (G_OBJECT (element), "gop-split", TRUE, NULL);
  } else {
    element = gst_check_setup_element ("mpegvideoparse");
  }

  return element;
}

GST_START_TEST (test_parse_normal)
{
  gst_parser_test_normal (mpeg2_iframe, sizeof (mpeg2_iframe));
}

GST_END_TEST;


GST_START_TEST (test_parse_drain_single)
{
  gst_parser_test_drain_single (mpeg2_iframe, sizeof (mpeg2_iframe));
}

GST_END_TEST;


GST_START_TEST (test_parse_split)
{
  gst_parser_test_split (mpeg2_iframe, sizeof (mpeg2_iframe));
}

GST_END_TEST;


#define structure_get_int(s,f) \
    (g_value_get_int(gst_structure_get_value(s,f)))
#define fail_unless_structure_field_int_equals(s,field,num) \
    fail_unless_equals_int (structure_get_int(s,field), num)

static void
mpeg_video_parse_check_caps (guint version, guint8 * seq, gint size)
{
  GstCaps *caps;
  GstStructure *s;
  GstBuffer *buf;
  const GValue *val;

  ctx_headers[0].data = seq;
  ctx_headers[0].size = size;
  /* parser does not really care that mpeg1 and mpeg2 frame data
   * should be a bit different */
  caps = gst_parser_test_get_output_caps (mpeg2_iframe, sizeof (mpeg2_iframe),
      NULL);
  fail_unless (caps != NULL);

  /* Check that the negotiated caps are as expected */
  /* When codec_data is present, parser assumes that data is version 4 */
  GST_LOG ("mpegvideo output caps: %" GST_PTR_FORMAT, caps);
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_has_name (s, "video/mpeg"));
  fail_unless_structure_field_int_equals (s, "mpegversion", version);
  fail_unless_structure_field_int_equals (s, "width", 32);
  fail_unless_structure_field_int_equals (s, "height", 24);
  fail_unless (gst_structure_has_field (s, "codec_data"));

  /* check codec-data in more detail */
  val = gst_structure_get_value (s, "codec_data");
  fail_unless (val != NULL);
  buf = gst_value_get_buffer (val);
  fail_unless (buf != NULL);
  /* codec-data = header - GOP */
  assert_equals_int (GST_BUFFER_SIZE (buf), size - 8);
  fail_unless (memcmp (GST_BUFFER_DATA (buf), seq + 4,
          GST_BUFFER_SIZE (buf)) == 0);

  gst_caps_unref (caps);
}

GST_START_TEST (test_parse_detect_stream_mpeg2)
{
  mpeg_video_parse_check_caps (2, mpeg2_seq, sizeof (mpeg2_seq));
}

GST_END_TEST;


GST_START_TEST (test_parse_detect_stream_mpeg1)
{
  mpeg_video_parse_check_caps (1, mpeg1_seq, sizeof (mpeg1_seq));
}

GST_END_TEST;


GST_START_TEST (test_parse_gop_split)
{
  ctx_factory = GOP_SPLIT;
  ctx_discard = 1;
  gst_parser_test_normal (mpeg2_iframe, sizeof (mpeg2_iframe));
  ctx_factory = "mpegvideoparse";
  ctx_discard = 0;
}

GST_END_TEST;


static Suite *
mpegvideoparse_suite (void)
{
  Suite *s = suite_create ("mpegvideoparse");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_parse_normal);
  tcase_add_test (tc_chain, test_parse_drain_single);
  tcase_add_test (tc_chain, test_parse_split);
  tcase_add_test (tc_chain, test_parse_detect_stream_mpeg1);
  tcase_add_test (tc_chain, test_parse_detect_stream_mpeg2);
  tcase_add_test (tc_chain, test_parse_gop_split);

  return s;
}


/*
 * TODO:
 *   - Both push- and pull-modes need to be tested
 *      * Pull-mode & EOS
 */

int
main (int argc, char **argv)
{
  int nf;

  Suite *s = mpegvideoparse_suite ();
  SRunner *sr = srunner_create (s);

  gst_check_init (&argc, &argv);

  /* init test context */
  ctx_factory = "mpegvideoparse";
  ctx_sink_template = &sinktemplate;
  ctx_src_template = &srctemplate;
  ctx_headers[0].data = mpeg2_seq;
  ctx_headers[0].size = sizeof (mpeg2_seq);
  ctx_verify_buffer = verify_buffer;
  ctx_setup = setup_element;

  srunner_run_all (sr, CK_NORMAL);
  nf = srunner_ntests_failed (sr);
  srunner_free (sr);

  return nf;
}
