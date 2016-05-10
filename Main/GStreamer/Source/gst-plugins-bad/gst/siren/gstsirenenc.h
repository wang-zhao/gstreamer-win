/*
 * Siren Encoder Gst Element
 *
 *   @author: Youness Alaoui <kakaroto@kakaroto.homelinux.net>
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
 *
 */

#ifndef __GST_SIREN_ENC_H__
#define __GST_SIREN_ENC_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "siren7.h"

G_BEGIN_DECLS
/* #define's don't like whitespacey bits */
#define GST_TYPE_SIREN_ENC \
  (gst_siren_enc_get_type())
#define GST_SIREN_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
  GST_TYPE_SIREN_ENC,GstSirenEnc))
#define GST_SIREN_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
  GST_TYPE_SIREN_ENC,GstSirenEncClass))
#define GST_IS_SIREN_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SIREN_ENC))
#define GST_IS_SIREN_ENC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SIREN_ENC))
typedef struct _GstSirenEnc GstSirenEnc;
typedef struct _GstSirenEncClass GstSirenEncClass;
typedef struct _GstSirenEncPrivate GstSirenEncPrivate;

struct _GstSirenEnc
{
  GstElement parent;

  /* protected by the stream lock */
  SirenEncoder encoder;
  GstAdapter *adapter;

  gboolean discont;

  GstPad *srcpad;
  GstPad *sinkpad;
};

struct _GstSirenEncClass
{
  GstElementClass parent_class;
};

GType gst_siren_enc_get_type (void);

gboolean gst_siren_enc_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* __GST_SIREN_ENC_H__ */
