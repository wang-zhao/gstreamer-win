/*
 * GStreamer
 * Copyright (C) 2010 Texas Instruments, Inc
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
 * SECTION:element-basecamerasrc
 *
 * Base class for the camera source bin used by camerabin for capture. 
 * Sophisticated camera hardware can derive from this baseclass and map the
 * features to this interface.
 *
 * The design mandates that the subclasses implement the following features and
 * behaviour:
 * <itemizedlist>
 *   <listitem><para>
 *     3 pads: viewfinder, image capture, video capture
 *   </para></listitem>
 *   <listitem><para>
 *   </para></listitem>
 * </itemizedlist>
 *
 * During construct_pipeline() vmethod a subclass can add several elements into
 * the bin and expose 3 srcs pads as ghostpads implementing the 3 pad templates.
 *
 * It is also possible to add regular pads from the subclass and implement the
 * dataflow methods on these pads. This way all functionality can be implemneted
 * directly in the subclass without extra elements.
 *
 * The src will receive the capture mode from #GstCameraBin2 on the 
 * #GstBaseCameraSrc:mode property. Possible capture modes are defined in
 * #GstCameraBinMode.
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstbasecamerasrc.h"

enum
{
  PROP_0,
  PROP_MODE,
  PROP_ZOOM,
  PROP_MAX_ZOOM,
  PROP_READY_FOR_CAPTURE,
  PROP_POST_PREVIEW,
  PROP_PREVIEW_CAPS,
  PROP_PREVIEW_FILTER
};

enum
{
  /* action signals */
  START_CAPTURE_SIGNAL,
  STOP_CAPTURE_SIGNAL,
  /* emit signals */
  LAST_SIGNAL
};

#define DEFAULT_POST_PREVIEW TRUE

static guint basecamerasrc_signals[LAST_SIGNAL];

GST_DEBUG_CATEGORY (base_camera_src_debug);
#define GST_CAT_DEFAULT base_camera_src_debug

GST_BOILERPLATE (GstBaseCameraSrc, gst_base_camera_src, GstBin, GST_TYPE_BIN);

static GstStaticPadTemplate vfsrc_template =
GST_STATIC_PAD_TEMPLATE (GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME,
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate imgsrc_template =
GST_STATIC_PAD_TEMPLATE (GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME,
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate vidsrc_template =
GST_STATIC_PAD_TEMPLATE (GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME,
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

/* NOTE: we could provide a vmethod for derived class to overload to provide
 * it's own implementation of interface..  but in all cases I can think of at
 * moment, either the camerasrc itself, or some element within the bin, will
 * be implementing the interface..
 */

/**
 * gst_base_camera_src_set_mode:
 * @self: the camerasrc bin
 * @mode: the mode
 *
 * Set the chosen #GstCameraBinMode capture mode.
 */
gboolean
gst_base_camera_src_set_mode (GstBaseCameraSrc * self, GstCameraBinMode mode)
{
  GstBaseCameraSrcClass *bclass = GST_BASE_CAMERA_SRC_GET_CLASS (self);

  g_return_val_if_fail (bclass->set_mode, FALSE);

  if (bclass->set_mode (self, mode)) {
    self->mode = mode;
    return TRUE;
  }
  return FALSE;
}

/**
 * gst_base_camera_src_setup_zoom:
 * @self: camerasrc object
 *
 * Apply zoom configured to camerabin to capture.
 */
void
gst_base_camera_src_setup_zoom (GstBaseCameraSrc * self)
{
  GstBaseCameraSrcClass *bclass = GST_BASE_CAMERA_SRC_GET_CLASS (self);

  g_return_if_fail (self->zoom);
  g_return_if_fail (bclass->set_zoom);

  bclass->set_zoom (self, self->zoom);
}

/**
 * gst_base_camera_src_setup_preview:
 * @self: camerasrc bin
 * @preview_caps: preview caps to set
 *
 * Apply preview caps to preview pipeline and to video source.
 */
void
gst_base_camera_src_setup_preview (GstBaseCameraSrc * self,
    GstCaps * preview_caps)
{
  GstBaseCameraSrcClass *bclass = GST_BASE_CAMERA_SRC_GET_CLASS (self);

  if (self->preview_pipeline) {
    GST_DEBUG_OBJECT (self,
        "Setting preview pipeline caps %" GST_PTR_FORMAT, self->preview_caps);
    gst_camerabin_preview_set_caps (self->preview_pipeline, preview_caps);
  }

  if (bclass->set_preview)
    bclass->set_preview (self, preview_caps);
}

static void
gst_base_camera_src_start_capture (GstBaseCameraSrc * src)
{
  GstBaseCameraSrcClass *klass = GST_BASE_CAMERA_SRC_GET_CLASS (src);

  g_return_if_fail (klass->start_capture != NULL);

  GST_DEBUG_OBJECT (src, "Starting capture");

  g_mutex_lock (src->capturing_mutex);
  if (src->capturing) {
    GST_WARNING_OBJECT (src, "Capturing already ongoing");
    g_mutex_unlock (src->capturing_mutex);

    /* post a warning to notify camerabin2 that the capture failed */
    GST_ELEMENT_WARNING (src, RESOURCE, BUSY, (NULL), (NULL));
    return;
  }

  src->capturing = TRUE;
  g_object_notify (G_OBJECT (src), "ready-for-capture");
  if (klass->start_capture (src)) {
    GST_DEBUG_OBJECT (src, "Capture started");
  } else {
    src->capturing = FALSE;
    g_object_notify (G_OBJECT (src), "ready-for-capture");
    GST_WARNING_OBJECT (src, "Failed to start capture");
  }
  g_mutex_unlock (src->capturing_mutex);
}

static void
gst_base_camera_src_stop_capture (GstBaseCameraSrc * src)
{
  GstBaseCameraSrcClass *klass = GST_BASE_CAMERA_SRC_GET_CLASS (src);

  g_return_if_fail (klass->stop_capture != NULL);

  g_mutex_lock (src->capturing_mutex);
  if (!src->capturing) {
    GST_DEBUG_OBJECT (src, "No ongoing capture");
    g_mutex_unlock (src->capturing_mutex);
    return;
  }
  klass->stop_capture (src);
  g_mutex_unlock (src->capturing_mutex);
}

void
gst_base_camera_src_finish_capture (GstBaseCameraSrc * self)
{
  GST_DEBUG_OBJECT (self, "Finishing capture");
  g_return_if_fail (self->capturing);
  self->capturing = FALSE;
  g_object_notify (G_OBJECT (self), "ready-for-capture");
}

static void
gst_base_camera_src_dispose (GObject * object)
{
  GstBaseCameraSrc *src = GST_BASE_CAMERA_SRC_CAST (object);

  g_mutex_free (src->capturing_mutex);

  if (src->preview_pipeline) {
    gst_camerabin_destroy_preview_pipeline (src->preview_pipeline);
    src->preview_pipeline = NULL;
  }

  if (src->preview_caps)
    gst_caps_replace (&src->preview_caps, NULL);

  if (src->preview_filter) {
    gst_object_unref (src->preview_filter);
    src->preview_filter = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_base_camera_src_finalize (GstBaseCameraSrc * self)
{
  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) (self));
}

static void
gst_base_camera_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstBaseCameraSrc *self = GST_BASE_CAMERA_SRC (object);

  switch (prop_id) {
    case PROP_MODE:
      gst_base_camera_src_set_mode (GST_BASE_CAMERA_SRC (self),
          g_value_get_enum (value));
      break;
    case PROP_ZOOM:{
      self->zoom = g_value_get_float (value);
      /* limit to max-zoom */
      if (self->zoom > self->max_zoom) {
        GST_DEBUG_OBJECT (self, "Clipping zoom %f to max-zoom %f", self->zoom,
            self->max_zoom);
        self->zoom = self->max_zoom;
      }
      /* does not set it if in NULL, the src is not created yet */
      if (GST_STATE (self) != GST_STATE_NULL)
        gst_base_camera_src_setup_zoom (self);
      break;
    }
    case PROP_POST_PREVIEW:
      self->post_preview = g_value_get_boolean (value);
      break;
    case PROP_PREVIEW_CAPS:{
      GstCaps *new_caps = NULL;
      new_caps = (GstCaps *) gst_value_get_caps (value);
      if (!gst_caps_is_equal (self->preview_caps, new_caps)) {
        gst_caps_replace (&self->preview_caps, new_caps);
        gst_base_camera_src_setup_preview (self, new_caps);
      } else {
        GST_DEBUG_OBJECT (self, "New preview caps equal current preview caps");
      }
    }
      break;
    case PROP_PREVIEW_FILTER:
      if (self->preview_filter)
        gst_object_unref (self->preview_filter);
      self->preview_filter = g_value_dup_object (value);
      if (!gst_camerabin_preview_set_filter (self->preview_pipeline,
              self->preview_filter)) {
        GST_WARNING_OBJECT (self,
            "Cannot change preview filter, is element in NULL state?");
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

static void
gst_base_camera_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstBaseCameraSrc *self = GST_BASE_CAMERA_SRC (object);

  switch (prop_id) {
    case PROP_MODE:
      g_value_set_enum (value, self->mode);
      break;
    case PROP_READY_FOR_CAPTURE:
      g_value_set_boolean (value, !self->capturing);
      break;
    case PROP_ZOOM:
      g_value_set_float (value, self->zoom);
      break;
    case PROP_MAX_ZOOM:
      g_value_set_float (value, self->max_zoom);
      break;
    case PROP_POST_PREVIEW:
      g_value_set_boolean (value, self->post_preview);
      break;
    case PROP_PREVIEW_CAPS:
      if (self->preview_caps)
        gst_value_set_caps (value, self->preview_caps);
      break;
    case PROP_PREVIEW_FILTER:
      if (self->preview_filter)
        g_value_set_object (value, self->preview_filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

static gboolean
construct_pipeline (GstBaseCameraSrc * self)
{
  GstBaseCameraSrcClass *bclass = GST_BASE_CAMERA_SRC_GET_CLASS (self);

  if (bclass->construct_pipeline) {
    if (!bclass->construct_pipeline (self)) {
      GST_ERROR_OBJECT (self, "pipeline construction failed");
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
setup_pipeline (GstBaseCameraSrc * self)
{
  GstBaseCameraSrcClass *bclass = GST_BASE_CAMERA_SRC_GET_CLASS (self);
  if (bclass->setup_pipeline)
    return bclass->setup_pipeline (self);
  return TRUE;
}

static GstStateChangeReturn
gst_base_camera_src_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstBaseCameraSrc *self = GST_BASE_CAMERA_SRC (element);

  GST_DEBUG_OBJECT (self, "%d -> %d",
      GST_STATE_TRANSITION_CURRENT (transition),
      GST_STATE_TRANSITION_NEXT (transition));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!construct_pipeline (self))
        return GST_STATE_CHANGE_FAILURE;

      if (self->preview_pipeline == NULL) {
        /* failed to create preview pipeline, fail state change */
        return GST_STATE_CHANGE_FAILURE;
      }

      if (self->preview_caps) {
        GST_DEBUG_OBJECT (self,
            "Setting preview pipeline caps %" GST_PTR_FORMAT,
            self->preview_caps);
        gst_camerabin_preview_set_caps (self->preview_pipeline,
            self->preview_caps);
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (!setup_pipeline (self))
        return GST_STATE_CHANGE_FAILURE;
      /* without this the preview pipeline will not post buffer
       * messages on the pipeline */
      gst_element_set_state (self->preview_pipeline->pipeline,
          GST_STATE_PLAYING);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_element_set_state (self->preview_pipeline->pipeline, GST_STATE_READY);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_element_set_state (self->preview_pipeline->pipeline, GST_STATE_NULL);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_base_camera_src_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  GST_DEBUG_CATEGORY_INIT (base_camera_src_debug, "base_camera_src", 0,
      "Base camera src");

  gst_element_class_set_details_simple (gstelement_class,
      "Base class for camerabin src bin", "Source/Video",
      "Abstracts capture device for camerabin2", "Rob Clark <rob@ti.com>");

  gst_element_class_add_static_pad_template (gstelement_class, &vfsrc_template);

  gst_element_class_add_static_pad_template (gstelement_class,
      &imgsrc_template);

  gst_element_class_add_static_pad_template (gstelement_class,
      &vidsrc_template);
}

static void
gst_base_camera_src_class_init (GstBaseCameraSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->dispose = gst_base_camera_src_dispose;
  gobject_class->finalize = (GObjectFinalizeFunc) gst_base_camera_src_finalize;
  gobject_class->set_property = gst_base_camera_src_set_property;
  gobject_class->get_property = gst_base_camera_src_get_property;

  g_object_class_install_property (gobject_class, PROP_MODE,
      g_param_spec_enum ("mode", "Mode",
          "The capture mode (still image capture or video recording)",
          GST_TYPE_CAMERABIN_MODE, MODE_IMAGE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ZOOM,
      g_param_spec_float ("zoom", "Zoom",
          "Digital zoom factor (e.g. 1.5 means 1.5x)", MIN_ZOOM, G_MAXFLOAT,
          DEFAULT_ZOOM, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_ZOOM,
      g_param_spec_float ("max-zoom", "Maximum zoom level (note: may change "
          "depending on resolution/implementation)",
          "Digital zoom factor (e.g. 1.5 means 1.5x)", MIN_ZOOM, G_MAXFLOAT,
          MAX_ZOOM, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstBaseCameraSrc:post-previews:
   *
   * When %TRUE, preview images should be posted to the bus when
   * captures are made
   */
  g_object_class_install_property (gobject_class, PROP_POST_PREVIEW,
      g_param_spec_boolean ("post-previews", "Post Previews",
          "If capture preview images should be posted to the bus",
          DEFAULT_POST_PREVIEW, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PREVIEW_CAPS,
      g_param_spec_boxed ("preview-caps", "Preview caps",
          "The caps of the preview image to be posted",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PREVIEW_FILTER,
      g_param_spec_object ("preview-filter", "Preview filter",
          "A custom preview filter to process preview image data",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstBaseCameraSrc:ready-for-capture:
   *
   * When TRUE new capture can be prepared. If FALSE capturing is ongoing
   * and starting a new capture immediately is not possible.
   *
   * Note that calling start-capture from the notify callback of this property
   * will cause a deadlock. If you need to react like this on the notify
   * function, please schedule a new thread to do it. If you're using glib's
   * mainloop you can use g_idle_add() for example.
   */
  g_object_class_install_property (gobject_class, PROP_READY_FOR_CAPTURE,
      g_param_spec_boolean ("ready-for-capture", "Ready for capture",
          "Informs this element is ready for starting another capture",
          TRUE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));


  /* Signals */
  basecamerasrc_signals[START_CAPTURE_SIGNAL] =
      g_signal_new_class_handler ("start-capture",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_base_camera_src_start_capture),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  basecamerasrc_signals[STOP_CAPTURE_SIGNAL] =
      g_signal_new_class_handler ("stop-capture",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_base_camera_src_stop_capture),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  gstelement_class->change_state = gst_base_camera_src_change_state;
}

static void
gst_base_camera_src_init (GstBaseCameraSrc * self,
    GstBaseCameraSrcClass * klass)
{
  self->width = DEFAULT_WIDTH;
  self->height = DEFAULT_HEIGHT;
  self->zoom = DEFAULT_ZOOM;
  self->max_zoom = MAX_ZOOM;
  self->mode = MODE_IMAGE;

  self->capturing = FALSE;
  self->capturing_mutex = g_mutex_new ();

  self->post_preview = DEFAULT_POST_PREVIEW;

  self->preview_pipeline =
      gst_camerabin_create_preview_pipeline (GST_ELEMENT_CAST (self), NULL);
}

void
gst_base_camera_src_post_preview (GstBaseCameraSrc * self, GstBuffer * buf)
{
  if (self->post_preview) {
    gst_camerabin_preview_pipeline_post (self->preview_pipeline, buf);
  } else {
    GST_DEBUG_OBJECT (self, "Previews not enabled, not posting");
  }
}
