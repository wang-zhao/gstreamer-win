/*
 * GStreamer
 * Copyright (C) 2008 Nokia Corporation <multimedia@maemo.org>
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
 * SECTION:camerabingeneral
 * @short_description: helper functions for #GstCameraBin and it's modules
 *
 * Common helper functions for #GstCameraBin, #GstCameraBinImage and
 * #GstCameraBinVideo.
 *
 */
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include "gstcamerabinpreview.h"
#include "gstbasecamerasrc.h"

GST_DEBUG_CATEGORY_EXTERN (base_camera_src_debug);
#define GST_CAT_DEFAULT base_camera_src_debug

static void _gst_camerabin_preview_set_caps (GstCameraBinPreviewPipelineData *
    preview, GstCaps * caps);

static gboolean
bus_callback (GstBus * bus, GstMessage * message, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      GstCameraBinPreviewPipelineData *data;

      data = user_data;

      gst_message_parse_error (message, &err, NULL);
      GST_WARNING ("Error from preview pipeline: %s", err->message);
      g_error_free (err);

      /* TODO Not sure if we should post an Error or Warning here */
      GST_ELEMENT_ERROR (data, CORE, FAILED,
          ("fatal error in preview pipeline, disposing the pipeline"), (NULL));

      /* Possible error situations:
       * 1) cond_wait pending. prevent deadlock by signalling the cond
       * 2) preview_pipeline_post called with new buffer to handle. returns
       *    because data->pipeline is set to null
       * 3) new preview caps incoming. returns because data->pipeline is null
       */

      if (data->pipeline) {
        gst_element_set_state (data->pipeline, GST_STATE_NULL);
        gst_object_unref (data->pipeline);
        data->pipeline = NULL;
      }

      g_cond_signal (data->processing_cond);

      break;
    }
    default:
      break;
  }
  return TRUE;
}

static GstFlowReturn
gst_camerabin_preview_pipeline_new_buffer (GstAppSink * appsink,
    gpointer user_data)
{
  GstBuffer *buffer;
  GstStructure *s;
  GstMessage *msg;
  GstCameraBinPreviewPipelineData *data;

  data = user_data;

  buffer = gst_app_sink_pull_buffer (appsink);
  s = gst_structure_new (GST_BASE_CAMERA_SRC_PREVIEW_MESSAGE_NAME,
      "buffer", GST_TYPE_BUFFER, buffer, NULL);
  gst_buffer_unref (buffer);
  msg = gst_message_new_element (GST_OBJECT (data->element), s);

  GST_DEBUG_OBJECT (data->element, "sending message with preview image");
  if (gst_element_post_message (data->element, msg) == FALSE) {
    GST_WARNING_OBJECT (data->element,
        "This element has no bus, therefore no message sent!");
  }

  g_mutex_lock (data->processing_lock);

  data->processing--;
  if (data->processing == 0)
    g_cond_signal (data->processing_cond);

  g_mutex_unlock (data->processing_lock);

  return GST_FLOW_OK;
}

/**
 * gst_camerabin_create_preview_pipeline:
 * @element: Owner of this pipeline
 * @filter: Custom filter to process preview data (an extra ref is taken)
 *
 * Creates a new previewing pipeline that can receive buffers
 * to be posted as camerabin preview messages for @element
 *
 * Returns: The newly created #GstCameraBinPreviewPipelineData
 */
GstCameraBinPreviewPipelineData *
gst_camerabin_create_preview_pipeline (GstElement * element,
    GstElement * filter)
{
  GstCameraBinPreviewPipelineData *data;
  GstElement *csp;
  GstElement *vscale;
  gboolean added = FALSE;
  gboolean linkfail = FALSE;
  GstBus *bus;
  GstAppSinkCallbacks callbacks = { 0, };

  data = g_new0 (GstCameraBinPreviewPipelineData, 1);

  data->pipeline = gst_pipeline_new ("preview-pipeline");
  data->appsrc = gst_element_factory_make ("appsrc", "preview-appsrc");
  data->appsink = gst_element_factory_make ("appsink", "preview-appsink");
  csp = gst_element_factory_make ("ffmpegcolorspace", "preview-csp");
  vscale = gst_element_factory_make ("videoscale", "preview-vscale");

  if (!data->appsrc || !data->appsink || !csp || !vscale) {
    goto error;
  }

  g_object_set (data->appsrc, "emit-signals", FALSE, NULL);
  g_object_set (data->appsink, "sync", FALSE, "enable-last-buffer",
      FALSE, NULL);

  gst_bin_add_many (GST_BIN (data->pipeline), data->appsrc,
      data->appsink, csp, vscale, NULL);
  if (filter)
    gst_bin_add (GST_BIN (data->pipeline), gst_object_ref (filter));
  added = TRUE;

  if (filter) {
    linkfail |=
        GST_PAD_LINK_FAILED (gst_element_link_pads_full (data->appsrc, "src",
            filter, NULL, GST_PAD_LINK_CHECK_NOTHING));
    linkfail |=
        GST_PAD_LINK_FAILED (gst_element_link_pads_full (filter, NULL,
            vscale, "sink", GST_PAD_LINK_CHECK_CAPS));
  } else {
    linkfail |=
        GST_PAD_LINK_FAILED (gst_element_link_pads_full (data->appsrc, "src",
            vscale, "sink", GST_PAD_LINK_CHECK_NOTHING));
  }

  linkfail |=
      GST_PAD_LINK_FAILED (gst_element_link_pads_full (vscale, "src", csp,
          "sink", GST_PAD_LINK_CHECK_NOTHING));
  linkfail |=
      GST_PAD_LINK_FAILED (gst_element_link_pads_full (csp, "src",
          data->appsink, "sink", GST_PAD_LINK_CHECK_NOTHING));

  if (linkfail) {
    GST_WARNING ("Failed to link preview pipeline elements");
    goto error;
  }

  callbacks.new_buffer = gst_camerabin_preview_pipeline_new_buffer;
  gst_app_sink_set_callbacks ((GstAppSink *) data->appsink, &callbacks, data,
      NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (data->pipeline));
  gst_bus_add_watch (bus, bus_callback, data);
  gst_object_unref (bus);

  g_object_set (data->appsink, "sync", FALSE, NULL);

  data->element = element;
  data->filter = filter;
  data->vscale = vscale;

  data->processing_lock = g_mutex_new ();
  data->processing_cond = g_cond_new ();

  data->pending_preview_caps = NULL;
  data->processing = 0;

  return data;
error:
  GST_WARNING ("Failed to create camerabin's preview pipeline");
  if (!added) {
    if (csp)
      gst_object_unref (csp);
    if (vscale)
      gst_object_unref (vscale);
    if (data->appsrc)
      gst_object_unref (data->appsrc);
    if (data->appsink)
      gst_object_unref (data->appsink);
  }
  gst_camerabin_destroy_preview_pipeline (data);
  return NULL;
}

/**
 * gst_camerabin_destroy_preview_pipeline:
 * @preview: the #GstCameraBinPreviewPipelineData
 *
 * Frees a #GstCameraBinPreviewPipelineData
 */
void
gst_camerabin_destroy_preview_pipeline (GstCameraBinPreviewPipelineData *
    preview)
{
  g_return_if_fail (preview != NULL);

  if (preview->processing_lock) {
    g_mutex_free (preview->processing_lock);
    preview->processing_lock = NULL;
  }
  if (preview->processing_cond) {
    g_cond_free (preview->processing_cond);
    preview->processing_cond = NULL;
  }
  if (preview->pipeline) {
    gst_element_set_state (preview->pipeline, GST_STATE_NULL);
    gst_object_unref (preview->pipeline);
  }
  g_free (preview);
}

/**
 * gst_camerabin_preview_pipeline_post:
 * @preview: the #GstCameraBinPreviewPipelineData
 * @buffer: the buffer to be posted as a preview
 *
 * Converts the @buffer to the desired format and posts the preview
 * message to the bus.
 *
 * Returns: %TRUE on success
 */
gboolean
gst_camerabin_preview_pipeline_post (GstCameraBinPreviewPipelineData * preview,
    GstBuffer * buffer)
{
  g_return_val_if_fail (preview != NULL, FALSE);
  g_return_val_if_fail (preview->pipeline != NULL, FALSE);
  g_return_val_if_fail (buffer, FALSE);

  g_mutex_lock (preview->processing_lock);
  g_return_val_if_fail (preview->pipeline != NULL, FALSE);

  if (preview->pending_preview_caps) {
    if (preview->processing > 0) {
      g_cond_wait (preview->processing_cond, preview->processing_lock);
    }
    _gst_camerabin_preview_set_caps (preview, preview->pending_preview_caps);
    gst_caps_replace (&preview->pending_preview_caps, NULL);
  }

  preview->processing++;

  gst_app_src_push_buffer ((GstAppSrc *) preview->appsrc,
      gst_buffer_ref (buffer));

  g_mutex_unlock (preview->processing_lock);

  return TRUE;
}

static void
_gst_camerabin_preview_set_caps (GstCameraBinPreviewPipelineData * preview,
    GstCaps * caps)
{
  GstState state, pending;
  GstStateChangeReturn ret;

  g_return_if_fail (preview != NULL);
  g_return_if_fail (preview->pipeline != NULL);

  ret = gst_element_get_state (preview->pipeline, &state, &pending, 0);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    /* make it try again */
    state = GST_STATE_PLAYING;
    pending = GST_STATE_VOID_PENDING;
  }
  gst_element_set_state (preview->pipeline, GST_STATE_NULL);
  g_object_set (preview->appsink, "caps", caps, NULL);
  if (pending != GST_STATE_VOID_PENDING)
    state = pending;
  gst_element_set_state (preview->pipeline, state);
}

/**
 * gst_camerabin_preview_set_caps:
 * @preview: the #GstCameraBinPreviewPipelineData
 * @caps: the #GstCaps to be set (a new ref will be taken)
 *
 * The caps that preview buffers should have when posted
 * on the bus
 */
void
gst_camerabin_preview_set_caps (GstCameraBinPreviewPipelineData * preview,
    GstCaps * caps)
{
  g_return_if_fail (preview != NULL);

  g_mutex_lock (preview->processing_lock);

  if (preview->processing == 0) {
    _gst_camerabin_preview_set_caps (preview, caps);
  } else {
    GST_DEBUG ("Preview pipeline busy, storing new caps as pending");
    gst_caps_replace (&preview->pending_preview_caps, caps);
  }
  g_mutex_unlock (preview->processing_lock);
}

/**
 * gst_camerabin_preview_set_filter:
 * @preview: the #GstCameraBinPreviewPipelineData
 * @filter: Custom filter to process preview data (an extra ref is taken)
 *
 * Set the filter element into preview pipeline.
 *
 * Returns: %TRUE on success
 */
gboolean
gst_camerabin_preview_set_filter (GstCameraBinPreviewPipelineData * preview,
    GstElement * filter)
{
  gboolean ret = TRUE;
  GstState current;

  g_return_val_if_fail (preview != NULL, FALSE);

  GST_DEBUG ("Preview pipeline setting new filter %p", filter);

  g_mutex_lock (preview->processing_lock);

  gst_element_get_state (preview->pipeline, &current, NULL, 0);

  if (preview->processing == 0 && current == GST_STATE_NULL) {
    gboolean linkfail = FALSE;

    if (preview->filter) {
      /* Unlink and remove old filter */
      gst_element_unlink (preview->appsrc, preview->filter);
      gst_element_unlink (preview->filter, preview->vscale);
      gst_bin_remove (GST_BIN (preview->pipeline), preview->filter);
    } else {
      /* Make room for filter by breaking the link between appsrc and vcale */
      gst_element_unlink (preview->appsrc, preview->vscale);
    }

    if (filter) {
      /* Add and link the new filter between appsrc and vscale */
      gst_bin_add (GST_BIN (preview->pipeline), gst_object_ref (filter));

      linkfail |=
          GST_PAD_LINK_FAILED (gst_element_link_pads_full (preview->appsrc,
              "src", filter, NULL, GST_PAD_LINK_CHECK_NOTHING));

      linkfail |=
          GST_PAD_LINK_FAILED (gst_element_link_pads_full (filter, NULL,
              preview->vscale, "sink", GST_PAD_LINK_CHECK_CAPS));
    } else {
      /* No filter was given. Just link the appsrc to vscale directly */
      linkfail |=
          GST_PAD_LINK_FAILED (gst_element_link_pads_full (preview->appsrc,
              "src", preview->vscale, "sink", GST_PAD_LINK_CHECK_NOTHING));
    }

    if (linkfail) {
      GST_WARNING ("Linking the filter to pipeline failed");
      ret = FALSE;
    } else {
      GST_DEBUG ("Linking the filter to pipeline successful");
      preview->filter = filter;
    }
  } else {
    GST_WARNING ("Cannot change filter when pipeline is running");
    ret = FALSE;
  }
  g_mutex_unlock (preview->processing_lock);

  return ret;
}
