/*
 * Copyright (C) 2010 Ole André Vadla Ravnås <oravnas@cisco.com>
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

#include "vtenc.h"

#include "coremediabuffer.h"
#include "vtutil.h"

#define VTENC_DEFAULT_USAGE       6     /* Profile: Baseline  Level: 2.1 */
#define VTENC_DEFAULT_BITRATE     768

#define VTENC_MIN_RESET_INTERVAL  (GST_SECOND / 2)

GST_DEBUG_CATEGORY (gst_vtenc_debug);
#define GST_CAT_DEFAULT (gst_vtenc_debug)

#define GST_VTENC_CODEC_DETAILS_QDATA \
    g_quark_from_static_string ("vtenc-codec-details")

enum
{
  PROP_0,
  PROP_USAGE,
  PROP_BITRATE
};

static GstElementClass *parent_class = NULL;

static void gst_vtenc_get_property (GObject * obj, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_vtenc_set_property (GObject * obj, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static GstStateChangeReturn gst_vtenc_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_vtenc_sink_setcaps (GstPad * pad, GstCaps * caps);
static void gst_vtenc_clear_cached_caps_downstream (GstVTEnc * self);
static GstFlowReturn gst_vtenc_chain (GstPad * pad, GstBuffer * buf);
static gboolean gst_vtenc_src_event (GstPad * pad, GstEvent * event);

static VTCompressionSessionRef gst_vtenc_create_session (GstVTEnc * self);
static void gst_vtenc_destroy_session (GstVTEnc * self,
    VTCompressionSessionRef * session);
static void gst_vtenc_session_dump_properties (GstVTEnc * self,
    VTCompressionSessionRef session);
static void gst_vtenc_session_configure_usage (GstVTEnc * self,
    VTCompressionSessionRef session, gint usage);
static void gst_vtenc_session_configure_expected_framerate (GstVTEnc * self,
    VTCompressionSessionRef session, gdouble framerate);
static void gst_vtenc_session_configure_expected_duration (GstVTEnc * self,
    VTCompressionSessionRef session, gdouble duration);
static void gst_vtenc_session_configure_max_keyframe_interval (GstVTEnc * self,
    VTCompressionSessionRef session, gint interval);
static void gst_vtenc_session_configure_max_keyframe_interval_duration
    (GstVTEnc * self, VTCompressionSessionRef session, gdouble duration);
static void gst_vtenc_session_configure_bitrate (GstVTEnc * self,
    VTCompressionSessionRef session, guint bitrate);
static VTStatus gst_vtenc_session_configure_property_int (GstVTEnc * self,
    VTCompressionSessionRef session, CFStringRef name, gint value);
static VTStatus gst_vtenc_session_configure_property_double (GstVTEnc * self,
    VTCompressionSessionRef session, CFStringRef name, gdouble value);

static GstFlowReturn gst_vtenc_encode_frame (GstVTEnc * self, GstBuffer * buf);
static VTStatus gst_vtenc_enqueue_buffer (void *data, int a2, int a3, int a4,
    CMSampleBufferRef sbuf, int a6, int a7);
static gboolean gst_vtenc_buffer_is_keyframe (GstVTEnc * self,
    CMSampleBufferRef sbuf);

static void
gst_vtenc_base_init (GstVTEncClass * klass)
{
  const GstVTEncoderDetails *codec_details =
      GST_VTENC_CLASS_GET_CODEC_DETAILS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  const int min_width = 1, max_width = G_MAXINT;
  const int min_height = 1, max_height = G_MAXINT;
  const int min_fps_n = 0, max_fps_n = G_MAXINT;
  const int min_fps_d = 1, max_fps_d = 1;
  GstPadTemplate *sink_template, *src_template;
  GstCaps *src_caps;
  GstElementDetails details;

  details.longname = g_strdup_printf ("%s encoder", codec_details->name);
  details.klass = g_strdup_printf ("Codec/Encoder/Video");
  details.description = g_strdup_printf ("%s encoder", codec_details->name);

  gst_element_class_set_details_simple (element_class,
      details.longname, details.klass, details.description,
      "Ole André Vadla Ravnås <oravnas@cisco.com>");

  g_free (details.longname);
  g_free (details.klass);
  g_free (details.description);

  sink_template = gst_pad_template_new ("sink",
      GST_PAD_SINK,
      GST_PAD_ALWAYS,
      gst_caps_new_simple ("video/x-raw-yuv",
          "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('N', 'V', '1', '2'),
          "width", GST_TYPE_INT_RANGE, min_width, max_width,
          "height", GST_TYPE_INT_RANGE, min_height, max_height,
          "framerate", GST_TYPE_FRACTION_RANGE,
          min_fps_n, min_fps_d, max_fps_n, max_fps_d, NULL));
  gst_element_class_add_pad_template (element_class, sink_template);
  gst_object_unref (sink_template);

  src_caps = gst_caps_new_simple (codec_details->mimetype,
      "width", GST_TYPE_INT_RANGE, min_width, max_width,
      "height", GST_TYPE_INT_RANGE, min_height, max_height,
      "framerate", GST_TYPE_FRACTION_RANGE,
      min_fps_n, min_fps_d, max_fps_n, max_fps_d, NULL);
  if (codec_details->format_id == kVTFormatH264) {
    gst_structure_set (gst_caps_get_structure (src_caps, 0),
        "stream-format", G_TYPE_STRING, "avc", NULL);
  }
  src_template = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      src_caps);
  gst_element_class_add_pad_template (element_class, src_template);
  gst_object_unref (src_template);
}

static void
gst_vtenc_class_init (GstVTEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->get_property = gst_vtenc_get_property;
  gobject_class->set_property = gst_vtenc_set_property;

  gstelement_class->change_state = gst_vtenc_change_state;

  g_object_class_install_property (gobject_class, PROP_USAGE,
      g_param_spec_int ("usage", "Usage",
          "Usage enumeration value",
          G_MININT, G_MAXINT, VTENC_DEFAULT_USAGE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate",
          "Target video bitrate in kbps",
          1, G_MAXUINT, VTENC_DEFAULT_BITRATE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

static void
gst_vtenc_init (GstVTEnc * self)
{
  GstVTEncClass *klass = (GstVTEncClass *) G_OBJECT_GET_CLASS (self);
  GstElementClass *element_klass = GST_ELEMENT_CLASS (klass);
  GstElement *element = GST_ELEMENT (self);

  self->details = GST_VTENC_CLASS_GET_CODEC_DETAILS (klass);

  self->sinkpad = gst_pad_new_from_template
      (gst_element_class_get_pad_template (element_klass, "sink"), "sink");
  gst_element_add_pad (element, self->sinkpad);
  gst_pad_set_setcaps_function (self->sinkpad, gst_vtenc_sink_setcaps);
  gst_pad_set_chain_function (self->sinkpad, gst_vtenc_chain);

  self->srcpad = gst_pad_new_from_template
      (gst_element_class_get_pad_template (element_klass, "src"), "src");
  gst_pad_set_event_function (self->srcpad, gst_vtenc_src_event);
  gst_element_add_pad (element, self->srcpad);

  /* These could be controlled by properties later */
  self->dump_properties = FALSE;
  self->dump_attributes = FALSE;
}

static gint
gst_vtenc_get_usage (GstVTEnc * self)
{
  gint result;

  GST_OBJECT_LOCK (self);
  result = self->usage;
  GST_OBJECT_UNLOCK (self);

  return result;
}

static void
gst_vtenc_set_usage (GstVTEnc * self, gint usage)
{
  GST_OBJECT_LOCK (self);

  self->usage = usage;

  if (self->session != NULL)
    gst_vtenc_session_configure_usage (self, self->session, usage);

  GST_OBJECT_UNLOCK (self);
}

static guint
gst_vtenc_get_bitrate (GstVTEnc * self)
{
  guint result;

  GST_OBJECT_LOCK (self);
  result = self->bitrate;
  GST_OBJECT_UNLOCK (self);

  return result;
}

static void
gst_vtenc_set_bitrate (GstVTEnc * self, guint bitrate)
{
  GST_OBJECT_LOCK (self);

  self->bitrate = bitrate;

  if (self->session != NULL)
    gst_vtenc_session_configure_bitrate (self, self->session, bitrate);

  GST_OBJECT_UNLOCK (self);
}

static void
gst_vtenc_get_property (GObject * obj, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVTEnc *self = GST_VTENC_CAST (obj);

  switch (prop_id) {
    case PROP_USAGE:
      g_value_set_int (value, gst_vtenc_get_usage (self));
      break;
    case PROP_BITRATE:
      g_value_set_uint (value, gst_vtenc_get_bitrate (self) * 8 / 1000);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static void
gst_vtenc_set_property (GObject * obj, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstVTEnc *self = GST_VTENC_CAST (obj);

  switch (prop_id) {
    case PROP_USAGE:
      gst_vtenc_set_usage (self, g_value_get_int (value));
      break;
    case PROP_BITRATE:
      gst_vtenc_set_bitrate (self, g_value_get_uint (value) * 1000 / 8);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_vtenc_change_state (GstElement * element, GstStateChange transition)
{
  GstVTEnc *self = GST_VTENC_CAST (element);
  GError *error = NULL;
  GstStateChangeReturn ret;

  if (transition == GST_STATE_CHANGE_NULL_TO_READY) {
    self->ctx = gst_core_media_ctx_new (GST_API_CORE_VIDEO | GST_API_CORE_MEDIA
        | GST_API_VIDEO_TOOLBOX, &error);
    if (error != NULL)
      goto api_error;

    self->cur_outbufs = g_ptr_array_new ();
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  if (transition == GST_STATE_CHANGE_READY_TO_NULL) {
    GST_OBJECT_LOCK (self);

    gst_vtenc_destroy_session (self, &self->session);

    if (self->options != NULL) {
      CFRelease (self->options);
      self->options = NULL;
    }

    self->negotiated_width = self->negotiated_height = 0;
    self->negotiated_fps_n = self->negotiated_fps_d = 0;

    gst_vtenc_clear_cached_caps_downstream (self);

    GST_OBJECT_UNLOCK (self);

    g_ptr_array_free (self->cur_outbufs, TRUE);
    self->cur_outbufs = NULL;

    g_object_unref (self->ctx);
    self->ctx = NULL;
  }

  return ret;

api_error:
  {
    GST_ELEMENT_ERROR (self, RESOURCE, FAILED, ("API error"),
        ("%s", error->message));
    g_clear_error (&error);
    return GST_STATE_CHANGE_FAILURE;
  }
}

static gboolean
gst_vtenc_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  GstVTEnc *self = GST_VTENC_CAST (GST_PAD_PARENT (pad));
  GstStructure *structure;
  VTCompressionSessionRef session;

  GST_OBJECT_LOCK (self);

  structure = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (structure, "width", &self->negotiated_width);
  gst_structure_get_int (structure, "height", &self->negotiated_height);
  gst_structure_get_fraction (structure, "framerate",
      &self->negotiated_fps_n, &self->negotiated_fps_d);

  gst_vtenc_destroy_session (self, &self->session);

  GST_OBJECT_UNLOCK (self);
  session = gst_vtenc_create_session (self);
  GST_OBJECT_LOCK (self);

  self->session = session;

  if (self->options != NULL)
    CFRelease (self->options);
  self->options = CFDictionaryCreateMutable (NULL, 0,
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
gst_vtenc_is_negotiated (GstVTEnc * self)
{
  return self->negotiated_width != 0;
}

static gboolean
gst_vtenc_negotiate_downstream (GstVTEnc * self, CMSampleBufferRef sbuf)
{
  gboolean result;
  GstCMApi *cm = self->ctx->cm;
  GstCaps *caps;
  GstStructure *s;

  if (self->caps_width == self->negotiated_width &&
      self->caps_height == self->negotiated_height &&
      self->caps_fps_n == self->negotiated_fps_n &&
      self->caps_fps_d == self->negotiated_fps_d) {
    return TRUE;
  }

  caps = gst_caps_copy (gst_pad_get_pad_template_caps (self->srcpad));
  s = gst_caps_get_structure (caps, 0);
  gst_structure_set (s,
      "width", G_TYPE_INT, self->negotiated_width,
      "height", G_TYPE_INT, self->negotiated_height,
      "framerate", GST_TYPE_FRACTION,
      self->negotiated_fps_n, self->negotiated_fps_d, NULL);

  if (self->details->format_id == kVTFormatH264) {
    CMFormatDescriptionRef fmt;
    CFDictionaryRef atoms;
    CFStringRef avccKey;
    CFDataRef avcc;
    GstBuffer *codec_data;

    fmt = cm->CMSampleBufferGetFormatDescription (sbuf);
    atoms = cm->CMFormatDescriptionGetExtension (fmt,
        *(cm->kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
    avccKey = CFStringCreateWithCString (NULL, "avcC", kCFStringEncodingUTF8);
    avcc = CFDictionaryGetValue (atoms, avccKey);
    CFRelease (avccKey);
    codec_data = gst_buffer_new_and_alloc (CFDataGetLength (avcc));
    CFDataGetBytes (avcc, CFRangeMake (0, CFDataGetLength (avcc)),
        GST_BUFFER_DATA (codec_data));

    gst_structure_set (s, "codec_data", GST_TYPE_BUFFER, codec_data, NULL);

    gst_buffer_unref (codec_data);
  }

  result = gst_pad_set_caps (self->srcpad, caps);

  gst_caps_unref (caps);

  self->caps_width = self->negotiated_width;
  self->caps_height = self->negotiated_height;
  self->caps_fps_n = self->negotiated_fps_n;
  self->caps_fps_d = self->negotiated_fps_d;

  return result;
}

static void
gst_vtenc_clear_cached_caps_downstream (GstVTEnc * self)
{
  self->caps_width = self->caps_height = 0;
  self->caps_fps_n = self->caps_fps_d = 0;
}

static GstFlowReturn
gst_vtenc_chain (GstPad * pad, GstBuffer * buf)
{
  GstVTEnc *self = GST_VTENC_CAST (GST_PAD_PARENT (pad));

  if (!gst_vtenc_is_negotiated (self))
    goto not_negotiated;

  return gst_vtenc_encode_frame (self, buf);

not_negotiated:
  gst_buffer_unref (buf);
  return GST_FLOW_NOT_NEGOTIATED;
}

static gboolean
gst_vtenc_src_event (GstPad * pad, GstEvent * event)
{
  GstVTEnc *self = GST_VTENC_CAST (GST_PAD_PARENT (pad));
  gboolean ret = TRUE;
  gboolean handled = FALSE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
      if (gst_event_has_name (event, "rtcp-pli")) {
        GST_OBJECT_LOCK (self);
        if (self->options != NULL) {
          GST_INFO_OBJECT (self, "received PLI, will force intra");
          CFDictionaryAddValue (self->options,
              *(self->ctx->vt->kVTEncodeFrameOptionKey_ForceKeyFrame),
              kCFBooleanTrue);
        } else {
          GST_INFO_OBJECT (self,
              "received PLI but encode not yet started, ignoring");
        }
        GST_OBJECT_UNLOCK (self);
        handled = TRUE;
      }
      break;
    default:
      break;
  }

  if (handled)
    gst_event_unref (event);
  else
    ret = gst_pad_push_event (self->sinkpad, event);

  return ret;
}

static VTCompressionSessionRef
gst_vtenc_create_session (GstVTEnc * self)
{
  VTCompressionSessionRef session = NULL;
  GstCVApi *cv = self->ctx->cv;
  GstVTApi *vt = self->ctx->vt;
  CFMutableDictionaryRef pb_attrs;
  VTCompressionOutputCallback callback;
  VTStatus status;

  pb_attrs = CFDictionaryCreateMutable (NULL, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  gst_vtutil_dict_set_i32 (pb_attrs, *(cv->kCVPixelBufferPixelFormatTypeKey),
      kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
  gst_vtutil_dict_set_i32 (pb_attrs, *(cv->kCVPixelBufferWidthKey),
      self->negotiated_width);
  gst_vtutil_dict_set_i32 (pb_attrs, *(cv->kCVPixelBufferHeightKey),
      self->negotiated_height);

  callback.func = gst_vtenc_enqueue_buffer;
  callback.data = self;

  status = vt->VTCompressionSessionCreate (NULL,
      self->negotiated_width, self->negotiated_height,
      self->details->format_id, 0, pb_attrs, 0, callback, &session);
  GST_INFO_OBJECT (self, "VTCompressionSessionCreate for %d x %d => %d",
      self->negotiated_width, self->negotiated_height, status);
  if (status != kVTSuccess)
    goto beach;

  if (self->dump_properties) {
    gst_vtenc_session_dump_properties (self, session);

    self->dump_properties = FALSE;
  }

  gst_vtenc_session_configure_usage (self, session, gst_vtenc_get_usage (self));

  gst_vtenc_session_configure_expected_framerate (self, session,
      (gdouble) self->negotiated_fps_n / (gdouble) self->negotiated_fps_d);
  gst_vtenc_session_configure_expected_duration (self, session,
      (gdouble) self->negotiated_fps_d / (gdouble) self->negotiated_fps_n);

  status = vt->VTCompressionSessionSetProperty (session,
      *(vt->kVTCompressionPropertyKey_ProfileLevel),
      *(vt->kVTProfileLevel_H264_Baseline_3_0));
  GST_DEBUG_OBJECT (self, "kVTCompressionPropertyKey_ProfileLevel => %d",
      status);

  status = vt->VTCompressionSessionSetProperty (session,
      *(vt->kVTCompressionPropertyKey_AllowTemporalCompression),
      kCFBooleanTrue);
  GST_DEBUG_OBJECT (self,
      "kVTCompressionPropertyKey_AllowTemporalCompression => %d", status);

  gst_vtenc_session_configure_max_keyframe_interval (self, session, 0);
  gst_vtenc_session_configure_max_keyframe_interval_duration (self, session,
      G_MAXDOUBLE);

  gst_vtenc_session_configure_bitrate (self, session,
      gst_vtenc_get_bitrate (self));

beach:
  CFRelease (pb_attrs);

  return session;
}

static void
gst_vtenc_destroy_session (GstVTEnc * self, VTCompressionSessionRef * session)
{
  self->ctx->vt->VTCompressionSessionInvalidate (*session);
  self->ctx->vt->VTCompressionSessionRelease (*session);
  *session = NULL;
}

typedef struct
{
  GstVTEnc *self;
  GstVTApi *vt;
  VTCompressionSessionRef session;
} GstVTDumpPropCtx;

static void
gst_vtenc_session_dump_property (CFStringRef prop_name,
    CFDictionaryRef prop_attrs, GstVTDumpPropCtx * dpc)
{
  gchar *name_str;
  CFTypeRef prop_value;
  VTStatus status;

  name_str = gst_vtutil_string_to_utf8 (prop_name);
  if (dpc->self->dump_attributes) {
    gchar *attrs_str;

    attrs_str = gst_vtutil_object_to_string (prop_attrs);
    GST_DEBUG_OBJECT (dpc->self, "%s = %s", name_str, attrs_str);
    g_free (attrs_str);
  }

  status = dpc->vt->VTCompressionSessionCopyProperty (dpc->session, prop_name,
      NULL, &prop_value);
  if (status == kVTSuccess) {
    gchar *value_str;

    value_str = gst_vtutil_object_to_string (prop_value);
    GST_DEBUG_OBJECT (dpc->self, "%s = %s", name_str, value_str);
    g_free (value_str);

    if (prop_value != NULL)
      CFRelease (prop_value);
  } else {
    GST_DEBUG_OBJECT (dpc->self, "%s = <failed to query: %d>",
        name_str, status);
  }

  g_free (name_str);
}

static void
gst_vtenc_session_dump_properties (GstVTEnc * self,
    VTCompressionSessionRef session)
{
  GstVTDumpPropCtx dpc = { self, self->ctx->vt, session };
  CFDictionaryRef dict;
  VTStatus status;

  status = self->ctx->vt->VTCompressionSessionCopySupportedPropertyDictionary
      (session, &dict);
  if (status != kVTSuccess)
    goto error;
  CFDictionaryApplyFunction (dict,
      (CFDictionaryApplierFunction) gst_vtenc_session_dump_property, &dpc);
  CFRelease (dict);

  return;

error:
  GST_WARNING_OBJECT (self, "failed to dump properties");
}

static void
gst_vtenc_session_configure_usage (GstVTEnc * self,
    VTCompressionSessionRef session, gint usage)
{
  gst_vtenc_session_configure_property_int (self, session,
      *(self->ctx->vt->kVTCompressionPropertyKey_Usage), usage);
}

static void
gst_vtenc_session_configure_expected_framerate (GstVTEnc * self,
    VTCompressionSessionRef session, gdouble framerate)
{
  gst_vtenc_session_configure_property_double (self, session,
      *(self->ctx->vt->kVTCompressionPropertyKey_ExpectedFrameRate), framerate);
}

static void
gst_vtenc_session_configure_expected_duration (GstVTEnc * self,
    VTCompressionSessionRef session, gdouble duration)
{
  gst_vtenc_session_configure_property_double (self, session,
      *(self->ctx->vt->kVTCompressionPropertyKey_ExpectedDuration), duration);
}

static void
gst_vtenc_session_configure_max_keyframe_interval (GstVTEnc * self,
    VTCompressionSessionRef session, gint interval)
{
  gst_vtenc_session_configure_property_int (self, session,
      *(self->ctx->vt->kVTCompressionPropertyKey_MaxKeyFrameInterval),
      interval);
}

static void
gst_vtenc_session_configure_max_keyframe_interval_duration (GstVTEnc * self,
    VTCompressionSessionRef session, gdouble duration)
{
  gst_vtenc_session_configure_property_double (self, session,
      *(self->ctx->vt->kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration),
      duration);
}

static void
gst_vtenc_session_configure_bitrate (GstVTEnc * self,
    VTCompressionSessionRef session, guint bitrate)
{
  gst_vtenc_session_configure_property_int (self, session,
      *(self->ctx->vt->kVTCompressionPropertyKey_AverageDataRate), bitrate);
}

static VTStatus
gst_vtenc_session_configure_property_int (GstVTEnc * self,
    VTCompressionSessionRef session, CFStringRef name, gint value)
{
  CFNumberRef num;
  VTStatus status;
  gchar name_str[128];

  num = CFNumberCreate (NULL, kCFNumberIntType, &value);
  status = self->ctx->vt->VTCompressionSessionSetProperty (session, name, num);
  CFRelease (num);

  CFStringGetCString (name, name_str, sizeof (name_str), kCFStringEncodingUTF8);
  GST_DEBUG_OBJECT (self, "%s(%d) => %d", name_str, value, status);

  return status;
}

static VTStatus
gst_vtenc_session_configure_property_double (GstVTEnc * self,
    VTCompressionSessionRef session, CFStringRef name, gdouble value)
{
  CFNumberRef num;
  VTStatus status;
  gchar name_str[128];

  num = CFNumberCreate (NULL, kCFNumberDoubleType, &value);
  status = self->ctx->vt->VTCompressionSessionSetProperty (session, name, num);
  CFRelease (num);

  CFStringGetCString (name, name_str, sizeof (name_str), kCFStringEncodingUTF8);
  GST_DEBUG_OBJECT (self, "%s(%f) => %d", name_str, value, status);

  return status;
}

static GstFlowReturn
gst_vtenc_encode_frame (GstVTEnc * self, GstBuffer * buf)
{
  GstCVApi *cv = self->ctx->cv;
  GstVTApi *vt = self->ctx->vt;
  CMTime ts, duration;
  CVPixelBufferRef pbuf = NULL;
  VTStatus vt_status;
  GstFlowReturn ret = GST_FLOW_OK;
  guint i;

  self->cur_inbuf = buf;

  ts = self->ctx->cm->CMTimeMake
      (GST_TIME_AS_MSECONDS (GST_BUFFER_TIMESTAMP (buf)), 1000);
  duration = self->ctx->cm->CMTimeMake
      (GST_TIME_AS_MSECONDS (GST_BUFFER_DURATION (buf)), 1000);

  if (GST_IS_CORE_MEDIA_BUFFER (buf)) {
    GstCoreMediaBuffer *cmbuf = GST_CORE_MEDIA_BUFFER_CAST (buf);
    pbuf = gst_core_media_buffer_get_pixel_buffer (cmbuf);
  }

  if (pbuf == NULL) {
    CVReturn cv_ret;

    cv_ret = cv->CVPixelBufferCreateWithBytes (NULL,
        self->negotiated_width, self->negotiated_height,
        kCVPixelFormatType_422YpCbCr8_yuvs, GST_BUFFER_DATA (buf),
        self->negotiated_width * 2,
        (CVPixelBufferReleaseBytesCallback) gst_buffer_unref, buf, NULL, &pbuf);
    if (cv_ret != kCVReturnSuccess)
      goto cv_error;
    gst_buffer_ref (buf);
  }

  GST_OBJECT_LOCK (self);

  self->expect_keyframe = CFDictionaryContainsKey (self->options,
      *(vt->kVTEncodeFrameOptionKey_ForceKeyFrame));
  if (self->expect_keyframe)
    gst_vtenc_clear_cached_caps_downstream (self);

  vt_status = self->ctx->vt->VTCompressionSessionEncodeFrame (self->session,
      pbuf, ts, duration, self->options, NULL, NULL);

  if (vt_status != 0) {
    GST_WARNING_OBJECT (self, "VTCompressionSessionEncodeFrame returned %d",
        vt_status);
  }

  self->ctx->vt->VTCompressionSessionCompleteFrames (self->session,
      *(self->ctx->cm->kCMTimeInvalid));

  GST_OBJECT_UNLOCK (self);

  cv->CVPixelBufferRelease (pbuf);
  self->cur_inbuf = NULL;
  gst_buffer_unref (buf);

  if (self->cur_outbufs->len > 0) {
    GstCoreMediaBuffer *cmbuf =
        GST_CORE_MEDIA_BUFFER_CAST (g_ptr_array_index (self->cur_outbufs, 0));
    if (!gst_vtenc_negotiate_downstream (self, cmbuf->sample_buf))
      ret = GST_FLOW_NOT_NEGOTIATED;
  }

  for (i = 0; i != self->cur_outbufs->len; i++) {
    GstBuffer *buf = g_ptr_array_index (self->cur_outbufs, i);

    if (ret == GST_FLOW_OK) {
      gst_buffer_set_caps (buf, GST_PAD_CAPS (self->srcpad));
      ret = gst_pad_push (self->srcpad, buf);
    } else {
      gst_buffer_unref (buf);
    }
  }
  g_ptr_array_set_size (self->cur_outbufs, 0);

  return ret;

cv_error:
  {
    self->cur_inbuf = NULL;
    gst_buffer_unref (buf);

    return GST_FLOW_ERROR;
  }
}

static VTStatus
gst_vtenc_enqueue_buffer (void *data, int a2, int a3, int a4,
    CMSampleBufferRef sbuf, int a6, int a7)
{
  GstVTEnc *self = data;
  gboolean is_keyframe;
  GstBuffer *buf;

  /* This may happen if we don't have enough bitrate */
  if (sbuf == NULL)
    goto beach;

  is_keyframe = gst_vtenc_buffer_is_keyframe (self, sbuf);
  if (self->expect_keyframe) {
    if (!is_keyframe)
      goto beach;
    CFDictionaryRemoveValue (self->options,
        *(self->ctx->vt->kVTEncodeFrameOptionKey_ForceKeyFrame));
  }
  self->expect_keyframe = FALSE;

  buf = gst_core_media_buffer_new (self->ctx, sbuf);
  gst_buffer_copy_metadata (buf, self->cur_inbuf, GST_BUFFER_COPY_TIMESTAMPS);
  if (is_keyframe) {
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
  } else {
    GST_BUFFER_FLAG_UNSET (buf, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);
  }

  g_ptr_array_add (self->cur_outbufs, buf);

beach:
  return kVTSuccess;
}

static gboolean
gst_vtenc_buffer_is_keyframe (GstVTEnc * self, CMSampleBufferRef sbuf)
{
  gboolean result = FALSE;
  CFArrayRef attachments_for_sample;

  attachments_for_sample =
      self->ctx->cm->CMSampleBufferGetSampleAttachmentsArray (sbuf, 0);
  if (attachments_for_sample != NULL) {
    CFDictionaryRef attachments;
    CFBooleanRef depends_on_others;

    attachments = CFArrayGetValueAtIndex (attachments_for_sample, 0);
    depends_on_others = CFDictionaryGetValue (attachments,
        *(self->ctx->cm->kCMSampleAttachmentKey_DependsOnOthers));
    result = (depends_on_others == kCFBooleanFalse);
  }

  return result;
}

static void
gst_vtenc_register (GstPlugin * plugin,
    const GstVTEncoderDetails * codec_details)
{
  GTypeInfo type_info = {
    sizeof (GstVTEncClass),
    (GBaseInitFunc) gst_vtenc_base_init,
    NULL,
    (GClassInitFunc) gst_vtenc_class_init,
    NULL,
    NULL,
    sizeof (GstVTEncClass),
    0,
    (GInstanceInitFunc) gst_vtenc_init,
  };
  gchar *type_name;
  GType type;
  gboolean result;

  type_name = g_strdup_printf ("vtenc_%s", codec_details->element_name);

  type = g_type_register_static (GST_TYPE_ELEMENT, type_name, &type_info, 0);

  g_type_set_qdata (type, GST_VTENC_CODEC_DETAILS_QDATA,
      (gpointer) codec_details);

  result = gst_element_register (plugin, type_name, GST_RANK_NONE, type);
  if (!result) {
    GST_ERROR_OBJECT (plugin, "failed to register element %s", type_name);
  }

  g_free (type_name);
}

static const GstVTEncoderDetails gst_vtenc_codecs[] = {
  {"H.264", "h264", "video/x-h264", kVTFormatH264},
};

void
gst_vtenc_register_elements (GstPlugin * plugin)
{
  guint i;

  GST_DEBUG_CATEGORY_INIT (gst_vtenc_debug, "vtenc",
      0, "Apple VideoToolbox Encoder Wrapper");

  for (i = 0; i != G_N_ELEMENTS (gst_vtenc_codecs); i++)
    gst_vtenc_register (plugin, &gst_vtenc_codecs[i]);
}
