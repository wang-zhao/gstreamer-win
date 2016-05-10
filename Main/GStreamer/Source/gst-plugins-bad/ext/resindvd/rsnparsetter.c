/*
 * Copyright (C) 2008 Jan Schmidt <thaytan@noraisin.net>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>

#include "rsnparsetter.h"
#include "rsnwrappedbuffer.h"

GST_DEBUG_CATEGORY_STATIC (rsn_parsetter_debug);
#define GST_CAT_DEFAULT rsn_parsetter_debug

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-rgb; video/x-raw-yuv")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-rgb; video/x-raw-yuv")
    );

static void rsn_parsetter_register_extra (GType rsn_parsetter_type);

GST_BOILERPLATE_FULL (RsnParSetter, rsn_parsetter, GstElement,
    GST_TYPE_ELEMENT, rsn_parsetter_register_extra);

static void rsn_parsetter_finalize (GObject * object);
static GstFlowReturn rsn_parsetter_chain (GstPad * pad, GstBuffer * buf);
static gboolean rsn_parsetter_sink_event (GstPad * pad, GstEvent * event);
static gboolean rsn_parsetter_sink_setcaps (GstPad * pad, GstCaps * caps);
static GstFlowReturn rsn_parsetter_sink_bufferalloc (GstPad * pad,
    guint64 offset, guint size, GstCaps * caps, GstBuffer ** buf);

static GstCaps *rsn_parsetter_src_getcaps (GstPad * pad);
static GstCaps *rsn_parsetter_convert_caps (RsnParSetter * parset,
    GstCaps * caps, gboolean widescreen);
static gboolean rsn_parsetter_check_caps (RsnParSetter * parset,
    GstCaps * caps);

static void
rsn_parsetter_register_extra (GType rsn_parsetter_type)
{
  GST_DEBUG_CATEGORY_INIT (rsn_parsetter_debug, "rsnparsetter", 0,
      "Resin DVD aspect ratio adjuster");
}

static void
rsn_parsetter_base_init (gpointer gclass)
{

  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);
  gst_element_class_set_details_simple (element_class,
      "Resin Aspect Ratio Setter", "Filter/Video",
      "Overrides caps on video buffers to force a particular display ratio",
      "Jan Schmidt <thaytan@noraisin.net>");
}

static void
rsn_parsetter_class_init (RsnParSetterClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = rsn_parsetter_finalize;
}

static void
rsn_parsetter_init (RsnParSetter * parset, RsnParSetterClass * gclass)
{
  parset->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_getcaps_function (parset->sinkpad,
      GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (parset->sinkpad,
      GST_DEBUG_FUNCPTR (rsn_parsetter_chain));
  gst_pad_set_event_function (parset->sinkpad,
      GST_DEBUG_FUNCPTR (rsn_parsetter_sink_event));
  gst_pad_set_setcaps_function (parset->sinkpad,
      GST_DEBUG_FUNCPTR (rsn_parsetter_sink_setcaps));
  gst_pad_set_bufferalloc_function (parset->sinkpad,
      GST_DEBUG_FUNCPTR (rsn_parsetter_sink_bufferalloc));
  gst_element_add_pad (GST_ELEMENT (parset), parset->sinkpad);

  parset->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (parset->srcpad,
      GST_DEBUG_FUNCPTR (rsn_parsetter_src_getcaps));
  gst_element_add_pad (GST_ELEMENT (parset), parset->srcpad);

  parset->caps_lock = g_mutex_new ();
}

static void
rsn_parsetter_finalize (GObject * object)
{
  RsnParSetter *parset = RSN_PARSETTER (object);

  gst_caps_replace (&parset->outcaps, NULL);
  gst_caps_replace (&parset->in_caps_last, NULL);
  gst_caps_replace (&parset->in_caps_converted, NULL);

  g_mutex_free (parset->caps_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstFlowReturn
rsn_parsetter_chain (GstPad * pad, GstBuffer * buf)
{
  RsnParSetter *parset = RSN_PARSETTER (GST_OBJECT_PARENT (pad));

  /* If this is a buffer we wrapped up earlier, unwrap it now */
  if (RSN_IS_WRAPPEDBUFFER (buf)) {
    RsnWrappedBuffer *wrap_buf = RSN_WRAPPEDBUFFER (buf);

    if (wrap_buf->owner == GST_ELEMENT (parset)) {
      buf = rsn_wrappedbuffer_unwrap_and_unref (wrap_buf);
      GST_DEBUG_OBJECT (parset, "Unwrapping %p yields buffer %p with caps %"
          GST_PTR_FORMAT, wrap_buf, buf, GST_BUFFER_CAPS (buf));
    }
  }

  if (parset->outcaps != GST_BUFFER_CAPS (buf)) {
    if (parset->override_outcaps == FALSE &&
        gst_caps_is_equal (parset->outcaps, GST_BUFFER_CAPS (buf))) {
      /* Just update our output caps var */
      gst_caps_replace (&parset->outcaps, GST_BUFFER_CAPS (buf));
      goto out;
    }

    /* Replace the caps on the output buffer */
    buf = gst_buffer_make_metadata_writable (buf);
    gst_buffer_set_caps (buf, parset->outcaps);

    GST_DEBUG_OBJECT (parset,
        "Replacing caps on buffer %p with caps %" GST_PTR_FORMAT,
        buf, parset->outcaps);
  }

out:
  return gst_pad_push (parset->srcpad, buf);
}

static gboolean
rsn_parsetter_sink_event (GstPad * pad, GstEvent * event)
{
  RsnParSetter *parset = RSN_PARSETTER (gst_pad_get_parent (pad));
  const GstStructure *structure = gst_event_get_structure (event);

  if (structure != NULL &&
      gst_structure_has_name (structure, "application/x-gst-dvd")) {
    const char *type = gst_structure_get_string (structure, "event");
    if (type == NULL)
      goto out;

    if (strcmp (type, "dvd-video-format") == 0) {
      gboolean is_widescreen;

      gst_structure_get_boolean (structure, "video-widescreen", &is_widescreen);

      GST_DEBUG_OBJECT (parset, "Video is %s",
          parset->is_widescreen ? "16:9" : "4:3");

      g_mutex_lock (parset->caps_lock);
      if (parset->is_widescreen != is_widescreen) {
        /* Force caps check */
        gst_caps_replace (&parset->in_caps_last, NULL);
        gst_caps_replace (&parset->in_caps_converted, NULL);
      }
      parset->is_widescreen = is_widescreen;

      /* FIXME: Added for testing: */
      // parset->is_widescreen = FALSE;

      g_mutex_unlock (parset->caps_lock);
    }
  }

out:
  gst_object_unref (GST_OBJECT (parset));
  return gst_pad_event_default (pad, event);
}

static GstCaps *
rsn_parsetter_src_getcaps (GstPad * pad)
{
  RsnParSetter *parset = RSN_PARSETTER (gst_pad_get_parent (pad));
  GstCaps *ret;
  const GstCaps *templ_caps = gst_pad_get_pad_template_caps (pad);

  ret = gst_pad_peer_get_caps (parset->sinkpad);
  if (ret == NULL)
    ret = gst_caps_copy (templ_caps);
  else {
    GstCaps *temp;
    temp = gst_caps_intersect (templ_caps, ret);
    gst_caps_unref (ret);
    ret = rsn_parsetter_convert_caps (parset, temp, parset->is_widescreen);
    gst_caps_unref (temp);
  }

  gst_object_unref (parset);
  return ret;
}

static gboolean
rsn_parsetter_check_caps (RsnParSetter * parset, GstCaps * caps)
{
  GstStructure *s;
  gint width, height;
  gint par_n, par_d;
  guint dar_n, dar_d;
  gboolean ret = FALSE;

  g_mutex_lock (parset->caps_lock);

  if (caps == parset->in_caps_last ||
      gst_caps_is_equal (caps, parset->in_caps_last)) {
    ret = parset->in_caps_was_ok;
    goto out;
  }

  /* Calculate the DAR from the incoming caps, and return TRUE if it matches
   * the required DAR, FALSE if not */
  s = gst_caps_get_structure (caps, 0);
  if (s == NULL)
    goto out;

  if (!gst_structure_get_int (s, "width", &width) ||
      !gst_structure_get_int (s, "height", &height))
    goto out;

  if (!gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d))
    par_n = par_d = 1;

  if (!gst_video_calculate_display_ratio (&dar_n, &dar_d, width, height,
          par_n, par_d, 1, 1))
    goto out;

  GST_DEBUG_OBJECT (parset,
      "Incoming video caps now: w %d h %d PAR %d/%d = DAR %d/%d",
      width, height, par_n, par_d, dar_n, dar_d);

  if (parset->is_widescreen) {
    if (dar_n == 16 && dar_d == 9)
      ret = TRUE;
  } else {
    if (dar_n == 4 && dar_d == 3)
      ret = TRUE;
  }

  gst_caps_replace (&parset->in_caps_last, caps);
  gst_caps_replace (&parset->in_caps_converted, NULL);
  parset->in_caps_was_ok = ret;

out:
  g_mutex_unlock (parset->caps_lock);
  return ret;
}

static GstCaps *
rsn_parsetter_convert_caps (RsnParSetter * parset, GstCaps * caps,
    gboolean widescreen)
{
  /* Duplicate the given caps, with a PAR that provides the desired DAR */
  GstCaps *outcaps;
  GstStructure *s;
  gint width, height;
  gint par_n, par_d;
  guint dar_n, dar_d;
  GValue par = { 0, };

  g_mutex_lock (parset->caps_lock);
  if (caps == parset->in_caps_last && parset->in_caps_converted) {
    outcaps = gst_caps_ref (parset->in_caps_converted);
    goto out;
  }

  outcaps = gst_caps_copy (caps);

  /* Calculate the DAR from the incoming caps, and return TRUE if it matches
   * the required DAR, FALSE if not */
  s = gst_caps_get_structure (outcaps, 0);
  if (s == NULL)
    goto out;

  if (!gst_structure_get_int (s, "width", &width) ||
      !gst_structure_get_int (s, "height", &height))
    goto out;

  if (widescreen) {
    dar_n = 16;
    dar_d = 9;
  } else {
    dar_n = 4;
    dar_d = 3;
  }

  par_n = dar_n * height;
  par_d = dar_d * width;

  g_value_init (&par, GST_TYPE_FRACTION);
  gst_value_set_fraction (&par, par_n, par_d);
  gst_structure_set_value (s, "pixel-aspect-ratio", &par);
  g_value_unset (&par);

  gst_caps_replace (&parset->in_caps_converted, outcaps);
out:
  g_mutex_unlock (parset->caps_lock);
  return outcaps;
}

static gboolean
rsn_parsetter_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  /* Check the new incoming caps against our current DAR, and mark
   * whether the buffers will need adjusting */
  RsnParSetter *parset = RSN_PARSETTER (gst_pad_get_parent (pad));

  if (rsn_parsetter_check_caps (parset, caps)) {
    parset->override_outcaps = FALSE;
    gst_caps_replace (&parset->outcaps, caps);
  } else {
    GstCaps *override_caps = rsn_parsetter_convert_caps (parset, caps,
        parset->is_widescreen);
    if (parset->outcaps)
      gst_caps_unref (parset->outcaps);
    parset->outcaps = override_caps;

    parset->override_outcaps = TRUE;
  }

  GST_DEBUG_OBJECT (parset, "caps changed: need_override now = %d",
      parset->override_outcaps);

  gst_object_unref (parset);
  return TRUE;
}

static GstFlowReturn
rsn_parsetter_sink_bufferalloc (GstPad * pad, guint64 offset, guint size,
    GstCaps * caps, GstBuffer ** buf)
{
  RsnParSetter *parset = RSN_PARSETTER (gst_pad_get_parent (pad));
  GstFlowReturn ret;

  GST_LOG_OBJECT (parset, "Entering bufferalloc");

  if (rsn_parsetter_check_caps (parset, caps)) {
    ret = gst_pad_alloc_buffer (parset->srcpad, offset, size, caps, buf);
    GST_LOG_OBJECT (parset, "Not wrapping buf %p", *buf);
  } else {
    /* Allocate and wrap a downstream buffer */
    GstBuffer *orig_buf;
    GstBuffer *outbuf;
    GstCaps *override_caps = rsn_parsetter_convert_caps (parset, caps,
        parset->is_widescreen);

    ret = gst_pad_alloc_buffer (parset->srcpad, offset, size,
        override_caps, &orig_buf);
    gst_caps_unref (override_caps);

    if (ret != GST_FLOW_OK)
      return ret;

    outbuf = (GstBuffer *) rsn_wrapped_buffer_new (orig_buf);
    if (!outbuf) {
      /* FIXME: Throw error */
      return GST_FLOW_ERROR;
    }

    rsn_wrapped_buffer_set_owner (RSN_WRAPPEDBUFFER (outbuf),
        GST_ELEMENT (parset));

    gst_buffer_set_caps (outbuf, caps);

    GST_LOG_OBJECT (parset,
        "Wrapped ds buf %p with caps %" GST_PTR_FORMAT
        " into new buf %p with caps %" GST_PTR_FORMAT,
        orig_buf, GST_BUFFER_CAPS (orig_buf), outbuf, GST_BUFFER_CAPS (outbuf));

    *buf = outbuf;
  }

  gst_object_unref (GST_OBJECT (parset));

  return ret;
}
