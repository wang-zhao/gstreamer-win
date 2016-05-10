/* GStreamer
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
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

#ifndef __GST_CDIO_CDDA_SRC_H__
#define __GST_CDIO_CDDA_SRC_H__

#include <gst/cdda/gstcddabasesrc.h>
#include <cdio/cdio.h>

#define GST_TYPE_CDIO_CDDA_SRC            (gst_cdio_cdda_src_get_type ())
#define GST_CDIO_CDDA_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CDIO_CDDA_SRC, GstCdioCddaSrc))
#define GST_CDIO_CDDA_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  GST_TYPE_CDIO_CDDA_SRC, GstCdioCddaSrcClass))
#define GST_IS_CDIO_CDDA_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CDIO_CDDA_SRC))
#define GST_IS_CDIO_CDDA_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  GST_TYPE_CDIO_CDDA_SRC))

typedef struct _GstCdioCddaSrc GstCdioCddaSrc;
typedef struct _GstCdioCddaSrcClass GstCdioCddaSrcClass;

struct _GstCdioCddaSrc
{
  GstCddaBaseSrc cddabasesrc;

  gint           read_speed;    /* ATOMIC */

  CdIo          *cdio;          /* NULL if not open */
};

struct _GstCdioCddaSrcClass
{
  GstCddaBaseSrcClass  cddabasesrc_class;
};

GType   gst_cdio_cdda_src_get_type (void);

#endif /* __GST_CDIO_CDDA_SRC_H__ */

