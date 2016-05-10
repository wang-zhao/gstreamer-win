/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
 * Copyright (C) 2011 Stefan Sauer <ensonic@users.sf.net>
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

/**
 * SECTION:element-facedetect
 *
 * Performs face detection on videos and images.
 *
 * The image is scaled down multiple times using the GstFaceDetect::scale-factor
 * until the size is &lt;= GstFaceDetect::min-size-width or 
 * GstFaceDetect::min-size-height. 
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 autovideosrc ! decodebin2 ! colorspace ! facedetect ! colorspace ! xvimagesink
 * ]| Detect and show faces
 * |[
 * gst-launch-0.10 autovideosrc ! video/x-raw-yuv,width=320,height=240 ! colorspace ! facedetect min-size-width=60 min-size-height=60 ! colorspace ! xvimagesink
 * ]| Detect large faces on a smaller image 
 *
 * </refsect2>
 */

/* FIXME: development version of OpenCV has CV_HAAR_FIND_BIGGEST_OBJECT which
 * we might want to use if available
 * see https://code.ros.org/svn/opencv/trunk/opencv/modules/objdetect/src/haar.cpp
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstopencvutils.h"
#include "gstfacedetect.h"

GST_DEBUG_CATEGORY_STATIC (gst_face_detect_debug);
#define GST_CAT_DEFAULT gst_face_detect_debug

#define DEFAULT_FACE_PROFILE "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml"
#define DEFAULT_NOSE_PROFILE "/usr/share/opencv/haarcascades/haarcascade_mcs_nose.xml"
#define DEFAULT_MOUTH_PROFILE "/usr/share/opencv/haarcascades/haarcascade_mcs_mouth.xml"
#define DEFAULT_EYES_PROFILE "/usr/share/opencv/haarcascades/haarcascade_mcs_eyepair_small.xml"
#define DEFAULT_SCALE_FACTOR 1.1
#define DEFAULT_FLAGS 0
#define DEFAULT_MIN_NEIGHBORS 3
#define DEFAULT_MIN_SIZE_WIDTH 0
#define DEFAULT_MIN_SIZE_HEIGHT 0

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_DISPLAY,
  PROP_FACE_PROFILE,
  PROP_NOSE_PROFILE,
  PROP_MOUTH_PROFILE,
  PROP_EYES_PROFILE,
  PROP_SCALE_FACTOR,
  PROP_MIN_NEIGHBORS,
  PROP_FLAGS,
  PROP_MIN_SIZE_WIDTH,
  PROP_MIN_SIZE_HEIGHT
};


/*
 * GstOpencvFaceDetectFlags:
 *
 * Flags parameter to OpenCV's cvHaarDetectObjects function.
 */
typedef enum
{
  GST_OPENCV_FACE_DETECT_HAAR_DO_CANNY_PRUNING = (1 << 0)
} GstOpencvFaceDetectFlags;

#define GST_TYPE_OPENCV_FACE_DETECT_FLAGS (gst_opencv_face_detect_flags_get_type())

static void
register_gst_opencv_face_detect_flags (GType * id)
{
  static const GFlagsValue values[] = {
    {(guint) GST_OPENCV_FACE_DETECT_HAAR_DO_CANNY_PRUNING,
        "Do Canny edge detection to discard some regions", "do-canny-pruning"},
    {0, NULL, NULL}
  };
  *id = g_flags_register_static ("GstOpencvFaceDetectFlags", values);
}

static GType
gst_opencv_face_detect_flags_get_type (void)
{
  static GType id;
  static GOnce once = G_ONCE_INIT;

  g_once (&once, (GThreadFunc) register_gst_opencv_face_detect_flags, &id);
  return id;
}

/* the capabilities of the inputs and outputs.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_RGB)
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_RGB)
    );

GST_BOILERPLATE (GstFaceDetect, gst_face_detect, GstOpencvVideoFilter,
    GST_TYPE_OPENCV_VIDEO_FILTER);

static void gst_face_detect_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_face_detect_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_face_detect_set_caps (GstOpencvVideoFilter * transform,
    gint in_width, gint in_height, gint in_depth, gint in_channels,
    gint out_width, gint out_height, gint out_depth, gint out_channels);
static GstFlowReturn gst_face_detect_transform_ip (GstOpencvVideoFilter * base,
    GstBuffer * buf, IplImage * img);

static CvHaarClassifierCascade *gst_face_detect_load_profile (GstFaceDetect *
    filter, gchar * profile);

/* Clean up */
static void
gst_face_detect_finalize (GObject * obj)
{
  GstFaceDetect *filter = GST_FACE_DETECT (obj);

  if (filter->cvGray)
    cvReleaseImage (&filter->cvGray);
  if (filter->cvStorage)
    cvReleaseMemStorage (&filter->cvStorage);

  g_free (filter->face_profile);
  g_free (filter->nose_profile);
  g_free (filter->mouth_profile);
  g_free (filter->eyes_profile);

  if (filter->cvFaceDetect)
    cvReleaseHaarClassifierCascade (&filter->cvFaceDetect);
  if (filter->cvNoseDetect)
    cvReleaseHaarClassifierCascade (&filter->cvNoseDetect);
  if (filter->cvMouthDetect)
    cvReleaseHaarClassifierCascade (&filter->cvMouthDetect);
  if (filter->cvEyesDetect)
    cvReleaseHaarClassifierCascade (&filter->cvEyesDetect);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}


/* GObject vmethod implementations */
static void
gst_face_detect_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "facedetect",
      "Filter/Effect/Video",
      "Performs face detection on videos and images, providing detected positions via bus messages",
      "Michael Sheldon <mike@mikeasoft.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);
}

/* initialize the facedetect's class */
static void
gst_face_detect_class_init (GstFaceDetectClass * klass)
{
  GObjectClass *gobject_class;
  GstOpencvVideoFilterClass *gstopencvbasefilter_class;

  gobject_class = (GObjectClass *) klass;
  gstopencvbasefilter_class = (GstOpencvVideoFilterClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_face_detect_finalize);
  gobject_class->set_property = gst_face_detect_set_property;
  gobject_class->get_property = gst_face_detect_get_property;

  gstopencvbasefilter_class->cv_trans_ip_func = gst_face_detect_transform_ip;
  gstopencvbasefilter_class->cv_set_caps = gst_face_detect_set_caps;

  g_object_class_install_property (gobject_class, PROP_DISPLAY,
      g_param_spec_boolean ("display", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FACE_PROFILE,
      g_param_spec_string ("profile", "Face profile",
          "Location of Haar cascade file to use for face detection",
          DEFAULT_FACE_PROFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_NOSE_PROFILE,
      g_param_spec_string ("nose-profile", "Nose profile",
          "Location of Haar cascade file to use for nose detection",
          DEFAULT_NOSE_PROFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MOUTH_PROFILE,
      g_param_spec_string ("mouth-profile", "Mouth profile",
          "Location of Haar cascade file to use for mouth detection",
          DEFAULT_MOUTH_PROFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_EYES_PROFILE,
      g_param_spec_string ("eyes-profile", "Eyes profile",
          "Location of Haar cascade file to use for eye-pair detection",
          DEFAULT_EYES_PROFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FLAGS,
      g_param_spec_flags ("flags", "Flags", "Flags to cvHaarDetectObjects",
          GST_TYPE_OPENCV_FACE_DETECT_FLAGS, DEFAULT_FLAGS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SCALE_FACTOR,
      g_param_spec_double ("scale-factor", "Scale factor",
          "Factor by which the frame is scaled after each object scan",
          1.1, 10.0, DEFAULT_SCALE_FACTOR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MIN_NEIGHBORS,
      g_param_spec_int ("min-neighbors", "Mininum neighbors",
          "Minimum number (minus 1) of neighbor rectangles that makes up "
          "an object", 0, G_MAXINT, DEFAULT_MIN_NEIGHBORS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MIN_SIZE_WIDTH,
      g_param_spec_int ("min-size-width", "Minimum face width",
          "Minimum area width to be recognized as a face", 0, G_MAXINT,
          DEFAULT_MIN_SIZE_WIDTH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MIN_SIZE_HEIGHT,
      g_param_spec_int ("min-size-height", "Minimum face height",
          "Minimum area height to be recognized as a face", 0, G_MAXINT,
          DEFAULT_MIN_SIZE_HEIGHT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_face_detect_init (GstFaceDetect * filter, GstFaceDetectClass * gclass)
{
  filter->face_profile = g_strdup (DEFAULT_FACE_PROFILE);
  filter->nose_profile = g_strdup (DEFAULT_NOSE_PROFILE);
  filter->mouth_profile = g_strdup (DEFAULT_MOUTH_PROFILE);
  filter->eyes_profile = g_strdup (DEFAULT_EYES_PROFILE);
  filter->display = TRUE;
  filter->scale_factor = DEFAULT_SCALE_FACTOR;
  filter->min_neighbors = DEFAULT_MIN_NEIGHBORS;
  filter->flags = DEFAULT_FLAGS;
  filter->min_size_width = DEFAULT_MIN_SIZE_WIDTH;
  filter->min_size_height = DEFAULT_MIN_SIZE_HEIGHT;
  filter->cvFaceDetect =
      gst_face_detect_load_profile (filter, filter->face_profile);
  filter->cvNoseDetect =
      gst_face_detect_load_profile (filter, filter->nose_profile);
  filter->cvMouthDetect =
      gst_face_detect_load_profile (filter, filter->mouth_profile);
  filter->cvEyesDetect =
      gst_face_detect_load_profile (filter, filter->eyes_profile);

  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      TRUE);
}

static void
gst_face_detect_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFaceDetect *filter = GST_FACE_DETECT (object);

  switch (prop_id) {
    case PROP_FACE_PROFILE:
      g_free (filter->face_profile);
      if (filter->cvFaceDetect)
        cvReleaseHaarClassifierCascade (&filter->cvFaceDetect);
      filter->face_profile = g_value_dup_string (value);
      filter->cvFaceDetect =
          gst_face_detect_load_profile (filter, filter->face_profile);
      break;
    case PROP_NOSE_PROFILE:
      g_free (filter->nose_profile);
      if (filter->cvNoseDetect)
        cvReleaseHaarClassifierCascade (&filter->cvNoseDetect);
      filter->nose_profile = g_value_dup_string (value);
      filter->cvNoseDetect =
          gst_face_detect_load_profile (filter, filter->nose_profile);
      break;
    case PROP_MOUTH_PROFILE:
      g_free (filter->mouth_profile);
      if (filter->cvMouthDetect)
        cvReleaseHaarClassifierCascade (&filter->cvMouthDetect);
      filter->mouth_profile = g_value_dup_string (value);
      filter->cvMouthDetect =
          gst_face_detect_load_profile (filter, filter->mouth_profile);
      break;
    case PROP_EYES_PROFILE:
      g_free (filter->eyes_profile);
      if (filter->cvEyesDetect)
        cvReleaseHaarClassifierCascade (&filter->cvEyesDetect);
      filter->eyes_profile = g_value_dup_string (value);
      filter->cvEyesDetect =
          gst_face_detect_load_profile (filter, filter->eyes_profile);
      break;
    case PROP_DISPLAY:
      filter->display = g_value_get_boolean (value);
      break;
    case PROP_SCALE_FACTOR:
      filter->scale_factor = g_value_get_double (value);
      break;
    case PROP_MIN_NEIGHBORS:
      filter->min_neighbors = g_value_get_int (value);
      break;
    case PROP_MIN_SIZE_WIDTH:
      filter->min_size_width = g_value_get_int (value);
      break;
    case PROP_MIN_SIZE_HEIGHT:
      filter->min_size_height = g_value_get_int (value);
      break;
    case PROP_FLAGS:
      filter->flags = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_face_detect_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstFaceDetect *filter = GST_FACE_DETECT (object);

  switch (prop_id) {
    case PROP_FACE_PROFILE:
      g_value_set_string (value, filter->face_profile);
      break;
    case PROP_NOSE_PROFILE:
      g_value_set_string (value, filter->nose_profile);
      break;
    case PROP_MOUTH_PROFILE:
      g_value_set_string (value, filter->mouth_profile);
      break;
    case PROP_EYES_PROFILE:
      g_value_set_string (value, filter->eyes_profile);
      break;
    case PROP_DISPLAY:
      g_value_set_boolean (value, filter->display);
      break;
    case PROP_SCALE_FACTOR:
      g_value_set_double (value, filter->scale_factor);
      break;
    case PROP_MIN_NEIGHBORS:
      g_value_set_int (value, filter->min_neighbors);
      break;
    case PROP_MIN_SIZE_WIDTH:
      g_value_set_int (value, filter->min_size_width);
      break;
    case PROP_MIN_SIZE_HEIGHT:
      g_value_set_int (value, filter->min_size_height);
      break;
    case PROP_FLAGS:
      g_value_set_flags (value, filter->flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_face_detect_set_caps (GstOpencvVideoFilter * transform, gint in_width,
    gint in_height, gint in_depth, gint in_channels,
    gint out_width, gint out_height, gint out_depth, gint out_channels)
{
  GstFaceDetect *filter;

  filter = GST_FACE_DETECT (transform);

  if (filter->cvGray)
    cvReleaseImage (&filter->cvGray);

  filter->cvGray = cvCreateImage (cvSize (in_width, in_height), IPL_DEPTH_8U,
      1);

  if (!filter->cvStorage)
    filter->cvStorage = cvCreateMemStorage (0);
  else
    cvClearMemStorage (filter->cvStorage);

  return TRUE;
}

static GstMessage *
gst_face_detect_message_new (GstFaceDetect * filter, GstBuffer * buf)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM_CAST (filter);
  GstStructure *s;
  GstClockTime running_time, stream_time;

  running_time = gst_segment_to_running_time (&trans->segment, GST_FORMAT_TIME,
      GST_BUFFER_TIMESTAMP (buf));
  stream_time = gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME,
      GST_BUFFER_TIMESTAMP (buf));

  s = gst_structure_new ("facedetect",
      "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP (buf),
      "stream-time", G_TYPE_UINT64, stream_time,
      "running-time", G_TYPE_UINT64, running_time,
      "duration", G_TYPE_UINT64, GST_BUFFER_DURATION (buf), NULL);

  return gst_message_new_element (GST_OBJECT (filter), s);
}


/* 
 * Performs the face detection
 */
static GstFlowReturn
gst_face_detect_transform_ip (GstOpencvVideoFilter * base, GstBuffer * buf,
    IplImage * img)
{
  GstFaceDetect *filter = GST_FACE_DETECT (base);

  if (filter->cvFaceDetect) {
    GstMessage *msg = NULL;
    GValue facelist = { 0 };
    CvSeq *faces;
    CvSeq *mouth, *nose, *eyes;
    gint i;
    gboolean do_display = FALSE;

    if (filter->display) {
      if (gst_buffer_is_writable (buf)) {
        do_display = TRUE;
      } else {
        GST_LOG_OBJECT (filter, "Buffer is not writable, not drawing faces.");
      }
    }

    cvCvtColor (img, filter->cvGray, CV_RGB2GRAY);
    cvClearMemStorage (filter->cvStorage);

    faces =
        cvHaarDetectObjects (filter->cvGray, filter->cvFaceDetect,
        filter->cvStorage, filter->scale_factor, filter->min_neighbors,
        filter->flags, cvSize (filter->min_size_width, filter->min_size_height)
#if (CV_MAJOR_VERSION >= 2) && (CV_MINOR_VERSION >= 2)
        , cvSize (filter->min_size_width + 2, filter->min_size_height + 2)
#endif
        );

    if (faces && faces->total > 0) {
      msg = gst_face_detect_message_new (filter, buf);
      g_value_init (&facelist, GST_TYPE_LIST);
    }

    for (i = 0; i < (faces ? faces->total : 0); i++) {
      CvRect *r = (CvRect *) cvGetSeqElem (faces, i);
      GValue value = { 0 };
      GstStructure *s;
      guint mw = filter->min_size_width / 8;
      guint mh = filter->min_size_height / 8;
      guint rnx, rny, rnw, rnh;
      guint rmx, rmy, rmw, rmh;
      guint rex, rey, rew, reh;
      gboolean have_nose, have_mouth, have_eyes;

      /* detect face features */

      rnx = r->x + r->width / 4;
      rny = r->y + r->height / 4;
      rnw = r->width / 2;
      rnh = r->height / 2;
      cvSetImageROI (filter->cvGray, cvRect (rnx, rny, rnw, rnh));
      nose =
          cvHaarDetectObjects (filter->cvGray, filter->cvNoseDetect,
          filter->cvStorage, filter->scale_factor, filter->min_neighbors,
          filter->flags, cvSize (mw, mh)
#if (CV_MAJOR_VERSION >= 2) && (CV_MINOR_VERSION >= 2)
          , cvSize (mw + 2, mh + 2)
#endif
          );
      have_nose = (nose && nose->total);
      cvResetImageROI (filter->cvGray);

      rmx = r->x;
      rmy = r->y + r->height / 2;
      rmw = r->width;
      rmh = r->height / 2;
      cvSetImageROI (filter->cvGray, cvRect (rmx, rmy, rmw, rmh));
      mouth =
          cvHaarDetectObjects (filter->cvGray, filter->cvMouthDetect,
          filter->cvStorage, filter->scale_factor, filter->min_neighbors,
          filter->flags, cvSize (mw, mh)
#if (CV_MAJOR_VERSION >= 2) && (CV_MINOR_VERSION >= 2)
          , cvSize (mw + 2, mh + 2)
#endif
          );
      have_mouth = (mouth && mouth->total);
      cvResetImageROI (filter->cvGray);

      rex = r->x;
      rey = r->y;
      rew = r->width;
      reh = r->height / 2;
      cvSetImageROI (filter->cvGray, cvRect (rex, rey, rew, reh));
      eyes =
          cvHaarDetectObjects (filter->cvGray, filter->cvEyesDetect,
          filter->cvStorage, filter->scale_factor, filter->min_neighbors,
          filter->flags, cvSize (mw, mh)
#if (CV_MAJOR_VERSION >= 2) && (CV_MINOR_VERSION >= 2)
          , cvSize (mw + 2, mh + 2)
#endif
          );
      have_eyes = (eyes && eyes->total);
      cvResetImageROI (filter->cvGray);

      GST_LOG_OBJECT (filter,
          "%2d/%2d: x,y = %4u,%4u: w.h = %4u,%4u : features(e,n,m) = %d,%d,%d",
          i, faces->total, r->x, r->y, r->width, r->height,
          have_eyes, have_nose, have_mouth);

      /* ignore 'face' where we don't fix mount/nose/eyes ? */
      if (!(have_eyes && have_nose && have_mouth))
        continue;

      s = gst_structure_new ("face",
          "x", G_TYPE_UINT, r->x,
          "y", G_TYPE_UINT, r->y,
          "width", G_TYPE_UINT, r->width,
          "height", G_TYPE_UINT, r->height, NULL);
      if (nose && nose->total) {
        CvRect *sr = (CvRect *) cvGetSeqElem (nose, 0);
        GST_LOG_OBJECT (filter, "nose/%d: x,y = %4u,%4u: w.h = %4u,%4u",
            nose->total, rnx + sr->x, rny + sr->y, sr->width, sr->height);
        gst_structure_set (s,
            "nose->x", G_TYPE_UINT, rnx + sr->x,
            "nose->y", G_TYPE_UINT, rny + sr->y,
            "nose->width", G_TYPE_UINT, sr->width,
            "nose->height", G_TYPE_UINT, sr->height, NULL);
      }
      if (mouth && mouth->total) {
        CvRect *sr = (CvRect *) cvGetSeqElem (mouth, 0);
        GST_LOG_OBJECT (filter, "mouth/%d: x,y = %4u,%4u: w.h = %4u,%4u",
            mouth->total, rmx + sr->x, rmy + sr->y, sr->width, sr->height);
        gst_structure_set (s,
            "mouth->x", G_TYPE_UINT, rmx + sr->x,
            "mouth->y", G_TYPE_UINT, rmy + sr->y,
            "mouth->width", G_TYPE_UINT, sr->width,
            "mouth->height", G_TYPE_UINT, sr->height, NULL);
      }
      if (eyes && eyes->total) {
        CvRect *sr = (CvRect *) cvGetSeqElem (eyes, 0);
        GST_LOG_OBJECT (filter, "eyes/%d: x,y = %4u,%4u: w.h = %4u,%4u",
            eyes->total, rex + sr->x, rey + sr->y, sr->width, sr->height);
        gst_structure_set (s,
            "eyes->x", G_TYPE_UINT, rex + sr->x,
            "eyes->y", G_TYPE_UINT, rey + sr->y,
            "eyes->width", G_TYPE_UINT, sr->width,
            "eyes->height", G_TYPE_UINT, sr->height, NULL);
      }

      g_value_init (&value, GST_TYPE_STRUCTURE);
      gst_value_set_structure (&value, s);
      gst_value_list_append_value (&facelist, &value);
      g_value_unset (&value);

      if (do_display) {
        CvPoint center;
        CvSize axes;
        gdouble w, h;
        gint cb = 255 - ((i & 3) << 7);
        gint cg = 255 - ((i & 12) << 5);
        gint cr = 255 - ((i & 48) << 3);

        w = r->width / 2;
        h = r->height / 2;
        center.x = cvRound ((r->x + w));
        center.y = cvRound ((r->y + h));
        axes.width = w;
        axes.height = h * 1.25; /* tweak for face form */
        cvEllipse (img, center, axes, 0.0, 0.0, 360.0, CV_RGB (cr, cg, cb),
            3, 8, 0);

        if (nose && nose->total) {
          CvRect *sr = (CvRect *) cvGetSeqElem (nose, 0);

          w = sr->width / 2;
          h = sr->height / 2;
          center.x = cvRound ((rnx + sr->x + w));
          center.y = cvRound ((rny + sr->y + h));
          axes.width = w;
          axes.height = h * 1.25;       /* tweak for nose form */
          cvEllipse (img, center, axes, 0.0, 0.0, 360.0, CV_RGB (cr, cg, cb),
              1, 8, 0);
        }
        if (mouth && mouth->total) {
          CvRect *sr = (CvRect *) cvGetSeqElem (mouth, 0);

          w = sr->width / 2;
          h = sr->height / 2;
          center.x = cvRound ((rmx + sr->x + w));
          center.y = cvRound ((rmy + sr->y + h));
          axes.width = w * 1.5; /* tweak for mouth form */
          axes.height = h;
          cvEllipse (img, center, axes, 0.0, 0.0, 360.0, CV_RGB (cr, cg, cb),
              1, 8, 0);
        }
        if (eyes && eyes->total) {
          CvRect *sr = (CvRect *) cvGetSeqElem (eyes, 0);

          w = sr->width / 2;
          h = sr->height / 2;
          center.x = cvRound ((rex + sr->x + w));
          center.y = cvRound ((rey + sr->y + h));
          axes.width = w * 1.5; /* tweak for eyes form */
          axes.height = h;
          cvEllipse (img, center, axes, 0.0, 0.0, 360.0, CV_RGB (cr, cg, cb),
              1, 8, 0);
        }
      }
    }

    if (msg) {
      gst_structure_set_value (msg->structure, "faces", &facelist);
      g_value_unset (&facelist);
      gst_element_post_message (GST_ELEMENT (filter), msg);
    }
  }

  return GST_FLOW_OK;
}


static CvHaarClassifierCascade *
gst_face_detect_load_profile (GstFaceDetect * filter, gchar * profile)
{
  CvHaarClassifierCascade *cascade;

  if (!(cascade = (CvHaarClassifierCascade *) cvLoad (profile, 0, 0, 0))) {
    GST_WARNING_OBJECT (filter, "Couldn't load Haar classifier cascade: %s.",
        profile);
  }
  return cascade;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
gboolean
gst_face_detect_plugin_init (GstPlugin * plugin)
{
  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_face_detect_debug, "facedetect",
      0,
      "Performs face detection on videos and images, providing detected positions via bus messages");

  return gst_element_register (plugin, "facedetect", GST_RANK_NONE,
      GST_TYPE_FACE_DETECT);
}
