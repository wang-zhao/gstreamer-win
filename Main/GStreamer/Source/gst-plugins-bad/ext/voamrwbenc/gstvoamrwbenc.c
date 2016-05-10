/* GStreamer Adaptive Multi-Rate Wide-Band (AMR-WB) plugin
 * Copyright (C) 2006 Edgard Lima <edgard.lima@indt.org.br>
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
 * SECTION:element-voamrwbenc
 * @see_also: #GstAmrWbDec, #GstAmrWbParse
 *
 * AMR wideband encoder based on the 
 * <ulink url="http://www.penguin.cz/~utx/amr">reference codec implementation</ulink>.
 * 
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch filesrc location=abc.wav ! wavparse ! audioresample ! audioconvert ! voamrwbenc ! filesink location=abc.amr
 * ]|
 * Please note that the above stream misses the header, that is needed to play
 * the stream.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvoamrwbenc.h"

#define MR660  0
#define MR885  1
#define MR1265 2
#define MR1425 2
#define MR1585 3
#define MR1825 4
#define MR1985 5
#define MR2305 6
#define MR2385 7
#define MRDTX  8

#define L_FRAME16k      320     /* Frame size at 16kHz  */

static GType
gst_voamrwbenc_bandmode_get_type (void)
{
  static GType gst_voamrwbenc_bandmode_type = 0;
  static GEnumValue gst_voamrwbenc_bandmode[] = {
    {MR660, "MR660", "MR660"},
    {MR885, "MR885", "MR885"},
    {MR1265, "MR1265", "MR1265"},
    {MR1425, "MR1425", "MR1425"},
    {MR1585, "MR1585", "MR1585"},
    {MR1825, "MR1825", "MR1825"},
    {MR1985, "MR1985", "MR1985"},
    {MR2305, "MR2305", "MR2305"},
    {MR2385, "MR2385", "MR2385"},
    {MRDTX, "MRDTX", "MRDTX"},
    {0, NULL, NULL},
  };
  if (!gst_voamrwbenc_bandmode_type) {
    gst_voamrwbenc_bandmode_type =
        g_enum_register_static ("GstVoAmrWbEncBandMode",
        gst_voamrwbenc_bandmode);
  }
  return gst_voamrwbenc_bandmode_type;
}

#define GST_VOAMRWBENC_BANDMODE_TYPE (gst_voamrwbenc_bandmode_get_type())

#define BANDMODE_DEFAULT MR660

enum
{
  PROP_0,
  PROP_BANDMODE
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "width = (int) 16, "
        "depth = (int) 16, "
        "signed = (boolean) TRUE, "
        "endianness = (int) BYTE_ORDER, "
        "rate = (int) 16000, " "channels = (int) 1")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/AMR-WB, "
        "rate = (int) 16000, " "channels = (int) 1")
    );

GST_DEBUG_CATEGORY_STATIC (gst_voamrwbenc_debug);
#define GST_CAT_DEFAULT gst_voamrwbenc_debug

static gboolean gst_voamrwbenc_start (GstAudioEncoder * enc);
static gboolean gst_voamrwbenc_stop (GstAudioEncoder * enc);
static gboolean gst_voamrwbenc_set_format (GstAudioEncoder * enc,
    GstAudioInfo * info);
static GstFlowReturn gst_voamrwbenc_handle_frame (GstAudioEncoder * enc,
    GstBuffer * in_buf);

GST_BOILERPLATE (GstVoAmrWbEnc, gst_voamrwbenc, GstAudioEncoder,
    GST_TYPE_AUDIO_ENCODER);

static void
gst_voamrwbenc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVoAmrWbEnc *self = GST_VOAMRWBENC (object);

  switch (prop_id) {
    case PROP_BANDMODE:
      self->bandmode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  return;
}

static void
gst_voamrwbenc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVoAmrWbEnc *self = GST_VOAMRWBENC (object);

  switch (prop_id) {
    case PROP_BANDMODE:
      g_value_set_enum (value, self->bandmode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  return;
}

static void
gst_voamrwbenc_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class,
      &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_details_simple (element_class, "AMR-WB audio encoder",
      "Codec/Encoder/Audio",
      "Adaptive Multi-Rate Wideband audio encoder",
      "Renato Araujo <renato.filho@indt.org.br>");
}

static void
gst_voamrwbenc_class_init (GstVoAmrWbEncClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstAudioEncoderClass *base_class = GST_AUDIO_ENCODER_CLASS (klass);

  object_class->set_property = gst_voamrwbenc_set_property;
  object_class->get_property = gst_voamrwbenc_get_property;

  base_class->start = GST_DEBUG_FUNCPTR (gst_voamrwbenc_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_voamrwbenc_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_voamrwbenc_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_voamrwbenc_handle_frame);

  g_object_class_install_property (object_class, PROP_BANDMODE,
      g_param_spec_enum ("band-mode", "Band Mode",
          "Encoding Band Mode (Kbps)", GST_VOAMRWBENC_BANDMODE_TYPE,
          BANDMODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (gst_voamrwbenc_debug, "voamrwbenc", 0,
      "voamrwb encoder");
}

static void
gst_voamrwbenc_init (GstVoAmrWbEnc * amrwbenc, GstVoAmrWbEncClass * klass)
{
  /* init rest */
  amrwbenc->handle = NULL;
  amrwbenc->channels = 0;
  amrwbenc->rate = 0;
}

static gboolean
gst_voamrwbenc_start (GstAudioEncoder * enc)
{
  GstVoAmrWbEnc *voamrwbenc = GST_VOAMRWBENC (enc);

  GST_DEBUG_OBJECT (enc, "start");

  if (!(voamrwbenc->handle = E_IF_init ()))
    return FALSE;

  voamrwbenc->rate = 0;
  voamrwbenc->channels = 0;

  return TRUE;
}

static gboolean
gst_voamrwbenc_stop (GstAudioEncoder * enc)
{
  GstVoAmrWbEnc *voamrwbenc = GST_VOAMRWBENC (enc);

  GST_DEBUG_OBJECT (enc, "stop");

  if (voamrwbenc->handle) {
    E_IF_exit (voamrwbenc->handle);
    voamrwbenc->handle = NULL;
  }

  return TRUE;
}

static gboolean
gst_voamrwbenc_set_format (GstAudioEncoder * benc, GstAudioInfo * info)
{
  GstVoAmrWbEnc *amrwbenc;
  GstCaps *copy;

  amrwbenc = GST_VOAMRWBENC (benc);

  /* get channel count */
  amrwbenc->channels = GST_AUDIO_INFO_CHANNELS (info);
  amrwbenc->rate = GST_AUDIO_INFO_RATE (info);

  /* this is not wrong but will sound bad */
  if (amrwbenc->channels != 1) {
    GST_WARNING ("amrwbdec is only optimized for mono channels");
  }
  if (amrwbenc->rate != 16000) {
    GST_WARNING ("amrwbdec is only optimized for 16000 Hz samplerate");
  }

  /* create reverse caps */
  copy = gst_caps_new_simple ("audio/AMR-WB",
      "channels", G_TYPE_INT, amrwbenc->channels,
      "rate", G_TYPE_INT, amrwbenc->rate, NULL);

  gst_pad_set_caps (GST_AUDIO_ENCODER_SRC_PAD (amrwbenc), copy);
  gst_caps_unref (copy);

  /* report needs to base class: one frame at a time */
  gst_audio_encoder_set_frame_samples_min (benc, L_FRAME16k);
  gst_audio_encoder_set_frame_samples_max (benc, L_FRAME16k);
  gst_audio_encoder_set_frame_max (benc, 1);

  return TRUE;
}

static GstFlowReturn
gst_voamrwbenc_handle_frame (GstAudioEncoder * benc, GstBuffer * buffer)
{
  GstVoAmrWbEnc *amrwbenc;
  GstFlowReturn ret = GST_FLOW_OK;
  const int buffer_size = sizeof (short) * L_FRAME16k;
  GstBuffer *out;
  gint outsize;

  amrwbenc = GST_VOAMRWBENC (benc);

  g_return_val_if_fail (amrwbenc->handle, GST_FLOW_NOT_NEGOTIATED);

  if (amrwbenc->rate == 0 || amrwbenc->channels == 0) {
    ret = GST_FLOW_NOT_NEGOTIATED;
    goto done;
  }

  /* we don't deal with squeezing remnants, so simply discard those */
  if (G_UNLIKELY (buffer == NULL)) {
    GST_DEBUG_OBJECT (amrwbenc, "no data");
    goto done;
  }

  if (G_UNLIKELY (GST_BUFFER_SIZE (buffer) < buffer_size)) {
    GST_DEBUG_OBJECT (amrwbenc, "discarding trailing data %d",
        buffer ? GST_BUFFER_SIZE (buffer) : 0);
    ret = gst_audio_encoder_finish_frame (benc, NULL, -1);
    goto done;
  }

  out = gst_buffer_new_and_alloc (buffer_size);
  /* encode */
  outsize = E_IF_encode (amrwbenc->handle, amrwbenc->bandmode,
      (const short *) GST_BUFFER_DATA (buffer),
      (unsigned char *) GST_BUFFER_DATA (out), 0);

  GST_LOG_OBJECT (amrwbenc, "encoded to %d bytes", outsize);
  GST_BUFFER_SIZE (out) = outsize;

  ret = gst_audio_encoder_finish_frame (benc, out, L_FRAME16k);

done:
  return ret;
}
