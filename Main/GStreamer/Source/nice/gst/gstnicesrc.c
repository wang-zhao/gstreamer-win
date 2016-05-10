/*
 * This file is part of the Nice GLib ICE library.
 *
 * (C) 2006, 2007 Collabora Ltd.
 *  Contact: Dafydd Harries
 * (C) 2006, 2007 Nokia Corporation. All rights reserved.
 *  Contact: Kai Vehmanen
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Nice GLib ICE library.
 *
 * The Initial Developers of the Original Code are Collabora Ltd and Nokia
 * Corporation. All Rights Reserved.
 *
 * Contributors:
 *   Dafydd Harries, Collabora Ltd.
 *
 * Alternatively, the contents of this file may be used under the terms of the
 * the GNU Lesser General Public License Version 2.1 (the "LGPL"), in which
 * case the provisions of LGPL are applicable instead of those above. If you
 * wish to allow use of your version of this file only under the terms of the
 * LGPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replace
 * them with the notice and other provisions required by the LGPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the LGPL.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>

#include "gstnicesrc.h"

GST_DEBUG_CATEGORY_STATIC (nicesrc_debug);
#define GST_CAT_DEFAULT nicesrc_debug


#define BUFFER_SIZE (65536)

static GstFlowReturn
gst_nice_src_create (
  GstBaseSrc *basesrc,
  guint64 offset,
  guint length,
  GstBuffer **buffer);

static gboolean
gst_nice_src_unlock (
    GstBaseSrc *basesrc);

static gboolean
gst_nice_src_unlock_stop (
    GstBaseSrc *basesrc);

static void
gst_nice_src_set_property (
  GObject *object,
  guint prop_id,
  const GValue *value,
  GParamSpec *pspec);

static void
gst_nice_src_get_property (
  GObject *object,
  guint prop_id,
  GValue *value,
  GParamSpec *pspec);


static void
gst_nice_src_dispose (GObject *object);

static GstStateChangeReturn
gst_nice_src_change_state (
    GstElement * element,
    GstStateChange transition);

static const GstElementDetails gst_nice_src_details =
GST_ELEMENT_DETAILS (
    "ICE source",
    "Source",
    "Interactive UDP connectivity establishment",
    "Dafydd Harries <dafydd.harries@collabora.co.uk>");

static GstStaticPadTemplate gst_nice_src_src_template =
GST_STATIC_PAD_TEMPLATE (
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_BOILERPLATE (GstNiceSrc, gst_nice_src, GstBaseSrc, GST_TYPE_BASE_SRC);

enum
{
  PROP_AGENT = 1,
  PROP_STREAM,
  PROP_COMPONENT
};

static void
gst_nice_src_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_nice_src_src_template));
  gst_element_class_set_details (element_class, &gst_nice_src_details);
}

static void
gst_nice_src_class_init (GstNiceSrcClass *klass)
{
  GstBaseSrcClass *gstbasesrc_class;
  GstElementClass *gstelement_class;
  GObjectClass *gobject_class;

  GST_DEBUG_CATEGORY_INIT (nicesrc_debug, "nicesrc",
      0, "libnice source");

  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstbasesrc_class->create = GST_DEBUG_FUNCPTR (gst_nice_src_create);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_nice_src_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_nice_src_unlock_stop);

  gobject_class = (GObjectClass *) klass;
  gobject_class->set_property = gst_nice_src_set_property;
  gobject_class->get_property = gst_nice_src_get_property;
  gobject_class->dispose = gst_nice_src_dispose;

  gstelement_class = (GstElementClass *) klass;
  gstelement_class->change_state = gst_nice_src_change_state;

  g_object_class_install_property (gobject_class, PROP_AGENT,
      g_param_spec_object (
         "agent",
         "Agent",
         "The NiceAgent this source is bound to",
         NICE_TYPE_AGENT,
         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_STREAM,
      g_param_spec_uint (
         "stream",
         "Stream ID",
         "The ID of the stream to read from",
         0,
         G_MAXUINT,
         0,
         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_COMPONENT,
      g_param_spec_uint (
         "component",
         "Component ID",
         "The ID of the component to read from",
         0,
         G_MAXUINT,
         0,
         G_PARAM_READWRITE));
}

static void
gst_nice_src_init (GstNiceSrc *src, GstNiceSrcClass *g_class)
{
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_do_timestamp (GST_BASE_SRC (src), TRUE);
  src->agent = NULL;
  src->stream_id = 0;
  src->component_id = 0;
  src->mainctx = g_main_context_new ();
  src->mainloop = g_main_loop_new (src->mainctx, FALSE);
  src->unlocked = FALSE;
  src->idle_source = NULL;
}

static void
gst_nice_src_read_callback (NiceAgent *agent,
    guint stream_id,
    guint component_id,
    guint len,
    gchar *buf,
    gpointer data)
{
  GstBaseSrc *basesrc = GST_BASE_SRC (data);
  GstNiceSrc *nicesrc = GST_NICE_SRC (basesrc);

  GST_LOG_OBJECT (agent, "Got buffer, getting out of the main loop");

  nicesrc->flow_ret = gst_pad_alloc_buffer (basesrc->srcpad, nicesrc->offset,
      len, GST_PAD_CAPS (basesrc->srcpad), &nicesrc->outbuf);
  if (nicesrc->flow_ret == GST_FLOW_OK) {
    memcpy (nicesrc->outbuf->data, buf, len);
    nicesrc->outbuf->size = len;
  }

  g_main_loop_quit (nicesrc->mainloop);
}

static gboolean
gst_nice_src_unlock_idler (gpointer data)
{
  GstNiceSrc *nicesrc = GST_NICE_SRC (data);

  GST_OBJECT_LOCK (nicesrc);
  if (nicesrc->unlocked)
    g_main_loop_quit (nicesrc->mainloop);

  if (nicesrc->idle_source) {
    g_source_destroy (nicesrc->idle_source);
    g_source_unref (nicesrc->idle_source);
    nicesrc->idle_source = NULL;
  }
  GST_OBJECT_UNLOCK (nicesrc);

  return FALSE;
}

static gboolean
gst_nice_src_unlock (GstBaseSrc *src)
{
  GstNiceSrc *nicesrc = GST_NICE_SRC (src);

  GST_OBJECT_LOCK (src);
  nicesrc->unlocked = TRUE;

  g_main_loop_quit (nicesrc->mainloop);

  if (!nicesrc->idle_source) {
    nicesrc->idle_source = g_idle_source_new ();
    g_source_set_priority (nicesrc->idle_source, G_PRIORITY_HIGH);
    g_source_set_callback (nicesrc->idle_source, gst_nice_src_unlock_idler, src, NULL);
    g_source_attach (nicesrc->idle_source, g_main_loop_get_context (nicesrc->mainloop));
  }
  GST_OBJECT_UNLOCK (src);

  return TRUE;
}

static gboolean
gst_nice_src_unlock_stop (GstBaseSrc *src)
{
  GstNiceSrc *nicesrc = GST_NICE_SRC (src);

  GST_OBJECT_LOCK (src);
  nicesrc->unlocked = FALSE;
  if (nicesrc->idle_source) {
    g_source_destroy (nicesrc->idle_source);
    g_source_unref(nicesrc->idle_source);
  }
  nicesrc->idle_source = NULL;
  GST_OBJECT_UNLOCK (src);

  return TRUE;
}

static GstFlowReturn
gst_nice_src_create (
  GstBaseSrc *basesrc,
  guint64 offset,
  guint length,
  GstBuffer **buffer)
{
  GstNiceSrc *nicesrc = GST_NICE_SRC (basesrc);

  GST_LOG_OBJECT (nicesrc, "create called");

  nicesrc->outbuf = NULL;
  nicesrc->offset = offset;

  GST_OBJECT_LOCK (basesrc);
  if (nicesrc->unlocked) {
    GST_OBJECT_UNLOCK (basesrc);
    return GST_FLOW_WRONG_STATE;
  }
  GST_OBJECT_UNLOCK (basesrc);

  g_main_loop_run (nicesrc->mainloop);

  if (nicesrc->outbuf) {
    GST_LOG_OBJECT (nicesrc, "Got buffer, pushing");

    *buffer = nicesrc->outbuf;
    return nicesrc->flow_ret;
  } else {
    GST_LOG_OBJECT (nicesrc, "Got interrupting, returning wrong-state");
    return GST_FLOW_WRONG_STATE;
  }

}

static void
gst_nice_src_dispose (GObject *object)
{
  GstNiceSrc *src = GST_NICE_SRC (object);

  if (src->agent)
    g_object_unref (src->agent);
  src->agent = NULL;

  if (src->mainloop)
    g_main_loop_unref (src->mainloop);
  src->mainloop = NULL;

  if (src->mainctx)
    g_main_context_unref (src->mainctx);
  src->mainctx = NULL;

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_nice_src_set_property (
  GObject *object,
  guint prop_id,
  const GValue *value,
  GParamSpec *pspec)
{
  GstNiceSrc *src = GST_NICE_SRC (object);

  switch (prop_id)
    {
    case PROP_AGENT:
      if (src->agent)
        GST_ERROR_OBJECT (object,
            "Changing the agent on a nice src not allowed");
      else
        src->agent = g_value_dup_object (value);
      break;

    case PROP_STREAM:
      src->stream_id = g_value_get_uint (value);
      break;

    case PROP_COMPONENT:
      src->component_id = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gst_nice_src_get_property (
  GObject *object,
  guint prop_id,
  GValue *value,
  GParamSpec *pspec)
{
  GstNiceSrc *src = GST_NICE_SRC (object);

  switch (prop_id)
    {
    case PROP_AGENT:
      g_value_set_object (value, src->agent);
      break;

    case PROP_STREAM:
      g_value_set_uint (value, src->stream_id);
      break;

    case PROP_COMPONENT:
      g_value_set_uint (value, src->component_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GstStateChangeReturn
gst_nice_src_change_state (GstElement * element, GstStateChange transition)
{
  GstNiceSrc *src;
  GstStateChangeReturn ret;

  src = GST_NICE_SRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (src->agent == NULL || src->stream_id == 0 || src->component_id == 0)
        {
          GST_ERROR_OBJECT (element,
              "Trying to start Nice source without an agent set");
          return GST_STATE_CHANGE_FAILURE;
        }
      else
        {
          nice_agent_attach_recv (src->agent, src->stream_id, src->component_id,
              src->mainctx, gst_nice_src_read_callback, (gpointer) src);
        }
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      nice_agent_attach_recv (src->agent, src->stream_id, src->component_id,
          src->mainctx, NULL, NULL);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  return ret;
}


