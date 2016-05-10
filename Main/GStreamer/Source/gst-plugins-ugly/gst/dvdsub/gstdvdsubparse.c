/* GStreamer DVD subtitle parser
 * Copyright (C) 2007 Mark Nauwelaerts <mnauw@users.sourceforge.net>
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
#include "config.h"
#endif

#include <string.h>
#include <gst/gst.h>
#include "gstdvdsubparse.h"

GST_DEBUG_CATEGORY_STATIC (dvdsubparse_debug);
#define GST_CAT_DEFAULT   dvdsubparse_debug

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-dvd-subpicture, parsed=(boolean)true")
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-dvd-subpicture")
    );

static void gst_dvd_sub_parse_finalize (GObject * object);

static void gst_dvd_sub_parse_reset (GstDvdSubParse * parse);

static gboolean gst_dvd_sub_parse_event (GstPad * pad, GstEvent * event);
static GstFlowReturn gst_dvd_sub_parse_chain (GstPad * pad, GstBuffer * buf);

static GstStateChangeReturn gst_dvd_sub_parse_change_state (GstElement *
    element, GstStateChange transition);
GST_BOILERPLATE (GstDvdSubParse, gst_dvd_sub_parse, GstElement,
    GST_TYPE_ELEMENT);

static void
gst_dvd_sub_parse_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class, &src_template);
  gst_element_class_add_static_pad_template (element_class,
      &sink_template);

  gst_element_class_set_details_simple (element_class, "DVD subtitle parser",
      "Codec/Parser/Subtitle", "Parses and packetizes DVD subtitle streams",
      "Mark Nauwelaerts <mnauw@users.sourceforge.net>");
}

static void
gst_dvd_sub_parse_class_init (GstDvdSubParseClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->finalize = gst_dvd_sub_parse_finalize;

  GST_DEBUG_CATEGORY_INIT (dvdsubparse_debug, "dvdsubparse", 0,
      "DVD subtitle parser");

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_dvd_sub_parse_change_state);
}

static void
gst_dvd_sub_parse_finalize (GObject * object)
{
  GstDvdSubParse *parse = GST_DVD_SUB_PARSE (object);

  g_object_unref (parse->adapter);
  parse->adapter = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dvd_sub_parse_init (GstDvdSubParse * parse, GstDvdSubParseClass * klass)
{
  parse->sinkpad = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_chain_function (parse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dvd_sub_parse_chain));
  gst_pad_set_event_function (parse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dvd_sub_parse_event));
  gst_element_add_pad (GST_ELEMENT (parse), parse->sinkpad);

  parse->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  gst_pad_use_fixed_caps (parse->srcpad);
  gst_pad_set_caps (parse->srcpad,
      gst_static_pad_template_get_caps (&src_template));
  gst_element_add_pad (GST_ELEMENT (parse), parse->srcpad);

  /* remainder */
  parse->adapter = gst_adapter_new ();
  gst_dvd_sub_parse_reset (parse);
}

static void
gst_dvd_sub_parse_reset (GstDvdSubParse * parse)
{
  parse->needed = 0;
  parse->stamp = GST_CLOCK_TIME_NONE;
  gst_adapter_clear (parse->adapter);
}

static gboolean
gst_dvd_sub_parse_event (GstPad * pad, GstEvent * event)
{
  GstDvdSubParse *parse;
  gboolean ret;

  parse = GST_DVD_SUB_PARSE (gst_pad_get_parent (pad));

  switch GST_EVENT_TYPE
    (event) {
    case GST_EVENT_FLUSH_STOP:
      gst_dvd_sub_parse_reset (parse);
      /* fall-through */
    default:
      ret = gst_pad_event_default (pad, event);
      break;
    }

  gst_object_unref (parse);
  return ret;
}


static GstFlowReturn
gst_dvd_sub_parse_chain (GstPad * pad, GstBuffer * buf)
{
  GstDvdSubParse *parse = GST_DVD_SUB_PARSE (GST_PAD_PARENT (pad));
  GstAdapter *adapter;
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  adapter = parse->adapter;

  GST_LOG_OBJECT (parse, "%4u bytes, ts: %" GST_TIME_FORMAT,
      GST_BUFFER_SIZE (buf), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)));

  gst_adapter_push (adapter, buf);

  if (!parse->needed) {
    const guint8 *data;

    data = gst_adapter_peek (adapter, 2);
    parse->needed = GST_READ_UINT16_BE (data);
  }

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    if (GST_CLOCK_TIME_IS_VALID (parse->stamp))
      /* normally, we expect only the first fragment to carry a timestamp */
      GST_WARNING_OBJECT (parse, "Received more timestamps than expected.");
    else
      parse->stamp = GST_BUFFER_TIMESTAMP (buf);
  }

  if (parse->needed) {
    guint av;

    av = gst_adapter_available (adapter);
    if (av >= parse->needed) {
      if (av > parse->needed) {
        /* normally, we expect several fragment, boundary aligned */
        GST_WARNING_OBJECT (parse, "Unexpected: needed %d, "
            "but more (%d) is available.", parse->needed, av);
      }
      outbuf = gst_adapter_take_buffer (adapter, parse->needed);
      /* decorate buffer */
      gst_buffer_set_caps (outbuf, GST_PAD_CAPS (parse->srcpad));
      GST_BUFFER_TIMESTAMP (outbuf) = parse->stamp;
      /* reset state */
      parse->stamp = GST_CLOCK_TIME_NONE;
      parse->needed = 0;
      /* and send along */
      ret = gst_pad_push (parse->srcpad, outbuf);
    }
  }

  return ret;
}

static GstStateChangeReturn
gst_dvd_sub_parse_change_state (GstElement * element, GstStateChange transition)
{
  GstDvdSubParse *parse = GST_DVD_SUB_PARSE (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret != GST_STATE_CHANGE_SUCCESS)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_dvd_sub_parse_reset (parse);
      break;
    default:
      break;
  }

  return GST_STATE_CHANGE_SUCCESS;
}
