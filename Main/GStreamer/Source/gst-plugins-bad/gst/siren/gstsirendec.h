/*
 * Siren Decoder Gst Element
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

#ifndef __GST_SIREN_DEC_H__
#define __GST_SIREN_DEC_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "siren7.h"

G_BEGIN_DECLS
/* #define's don't like whitespacey bits */
#define GST_TYPE_SIREN_DEC \
  (gst_siren_dec_get_type())
#define GST_SIREN_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
  GST_TYPE_SIREN_DEC,GstSirenDec))
#define GST_SIREN_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
  GST_TYPE_SIREN_DEC,GstSirenDecClass))
#define GST_IS_SIREN_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SIREN_DEC))
#define GST_IS_SIREN_DEC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SIREN_DEC))
typedef struct _GstSirenDec GstSirenDec;
typedef struct _GstSirenDecClass GstSirenDecClass;
typedef struct _GstSirenDecPrivate GstSirenDecPrivate;

struct _GstSirenDec
{
  GstElement parent;

  /* Protected by stream lock */
  SirenDecoder decoder;

  GstAdapter *adapter;
  gboolean discont;

  GstPad *sinkpad;
  GstPad *srcpad;
};

struct _GstSirenDecClass
{
  GstElementClass parent_class;
};

GType gst_siren_dec_get_type (void);

gboolean gst_siren_dec_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* __GST_SIREN_DEC_H__ */
