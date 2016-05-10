/*
 * gst-rtp-vp8-depay.c - Source for GstRtpVP8Depay
 * Copyright (C) 2011 Sjoerd Simons <sjoerd@luon.net>
 * Copyright (C) 2011 Collabora Ltd.
 *   Contact: Youness Alaoui <youness.alaoui@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "gstrtpvp8depay.h"

GST_DEBUG_CATEGORY_STATIC (gst_rtp_vp8_depay_debug);
#define GST_CAT_DEFAULT gst_rtp_vp8_depay_debug

GST_BOILERPLATE (GstRtpVP8Depay, gst_rtp_vp8_depay, GstBaseRTPDepayload,
    GST_TYPE_BASE_RTP_DEPAYLOAD);

static GstStaticPadTemplate gst_rtp_vp8_depay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp8"));

static GstStaticPadTemplate gst_rtp_vp8_depay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "payload = (int) " GST_RTP_PAYLOAD_DYNAMIC_STRING ","
        "clock-rate = (int) 90000,"
        "media = (string) \"video\","
        "encoding-name = (string) \"VP8-DRAFT-IETF-01\""));

static void
gst_rtp_vp8_depay_init (GstRtpVP8Depay * self, GstRtpVP8DepayClass * klass)
{
  self->adapter = gst_adapter_new ();
  self->started = FALSE;
}

static void gst_rtp_vp8_depay_dispose (GObject * object);
static GstBuffer *gst_rtp_vp8_depay_process (GstBaseRTPDepayload * depayload,
    GstBuffer * buf);
static gboolean gst_rtp_vp8_depay_set_caps (GstBaseRTPDepayload * depayload,
    GstCaps * caps);

static void
gst_rtp_vp8_depay_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class,
      &gst_rtp_vp8_depay_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_rtp_vp8_depay_src_template);

  gst_element_class_set_details_simple (element_class, "RTP VP8 depayloader",
      "Codec/Depayloader/Network/RTP",
      "Extracts VP8 video from RTP packets)",
      "Sjoerd Simons <sjoerd@luon.net>");
}

static void
gst_rtp_vp8_depay_class_init (GstRtpVP8DepayClass * gst_rtp_vp8_depay_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (gst_rtp_vp8_depay_class);
  GstBaseRTPDepayloadClass *depay_class =
      (GstBaseRTPDepayloadClass *) (gst_rtp_vp8_depay_class);

  object_class->dispose = gst_rtp_vp8_depay_dispose;

  depay_class->process = gst_rtp_vp8_depay_process;
  depay_class->set_caps = gst_rtp_vp8_depay_set_caps;

  GST_DEBUG_CATEGORY_INIT (gst_rtp_vp8_depay_debug, "rtpvp8depay", 0,
      "VP8 Video RTP Depayloader");
}

static void
gst_rtp_vp8_depay_dispose (GObject * object)
{
  GstRtpVP8Depay *self = GST_RTP_VP8_DEPAY (object);

  if (self->adapter != NULL)
    g_object_unref (self->adapter);
  self->adapter = NULL;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (parent_class)->dispose)
    G_OBJECT_CLASS (parent_class)->dispose (object);
}

static GstBuffer *
gst_rtp_vp8_depay_process (GstBaseRTPDepayload * depay, GstBuffer * buf)
{
  GstRtpVP8Depay *self = GST_RTP_VP8_DEPAY (depay);
  GstBuffer *payload;
  guint8 *data;
  guint offset;
  guint size = gst_rtp_buffer_get_payload_len (buf);

  if (G_UNLIKELY (GST_BUFFER_IS_DISCONT (buf))) {
    GST_LOG_OBJECT (self, "Discontinuity, flushing adapter");
    gst_adapter_clear (self->adapter);
    self->started = FALSE;
  }

  /* At least one header and one vp8 byte */
  if (G_UNLIKELY (size < 2))
    goto too_small;

  data = gst_rtp_buffer_get_payload (buf);

  if (G_UNLIKELY (!self->started)) {
    /* Check if this is the start of a VP8 frame, otherwise bail */
    /* S=1 and PartID= 0 */
    if ((data[0] & 0x1F) != 0x10)
      return NULL;

    self->started = TRUE;
  }

  offset = 1;
  /* Check X optional header */
  if ((data[0] & 0x80) != 0) {
    offset++;
    /* Check I optional header */
    if ((data[1] & 0x80) != 0) {
      offset++;
      if (G_UNLIKELY (offset + 2 >= size))
        goto too_small;
      /* Check for 16 bits PictureID */
      if ((data[2] & 0x80) != 0)
        offset++;
    }
    /* Check L optional header */
    if ((data[1] & 0x40) != 0)
      offset++;
    /* Check T optional header */
    if ((data[1] & 0x20) != 0)
      offset++;
  }

  if (G_UNLIKELY (offset >= size))
    goto too_small;

  payload = gst_rtp_buffer_get_payload_subbuffer (buf, offset, -1);
  gst_adapter_push (self->adapter, payload);

  /* Marker indicates that it was the last rtp packet for this frame */
  if (gst_rtp_buffer_get_marker (buf)) {
    GstBuffer *out;

    out = gst_adapter_take_buffer (self->adapter,
        gst_adapter_available (self->adapter));

    self->started = FALSE;
    return out;
  }

  return NULL;

too_small:
  GST_LOG_OBJECT (self, "Invalid rtp packet (too small), ignoring");
  gst_adapter_clear (self->adapter);
  self->started = FALSE;
  return NULL;
}

static gboolean
gst_rtp_vp8_depay_set_caps (GstBaseRTPDepayload * depayload, GstCaps * caps)
{
  GstCaps *srccaps = gst_caps_new_simple ("video/x-vp8",
      "framerate", GST_TYPE_FRACTION, 0, 1,
      NULL);

  gst_pad_set_caps (GST_BASE_RTP_DEPAYLOAD_SRCPAD (depayload), srccaps);
  gst_caps_unref (srccaps);

  return TRUE;
}

gboolean
gst_rtp_vp8_depay_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "rtpvp8depay",
      GST_RANK_MARGINAL, GST_TYPE_RTP_VP8_DEPAY);
}
