/* GStreamer
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
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

#ifndef __GST_CDIO_H__
#define __GST_CDIO_H__

#include <gst/gst.h>
#include <cdio/cdio.h>
#include <cdio/cdtext.h>

GST_DEBUG_CATEGORY_EXTERN (gst_cdio_debug);
#define GST_CAT_DEFAULT gst_cdio_debug

void     gst_cdio_add_cdtext_field (GstObject      * src,
                                    cdtext_t       * cdtext,
                                    cdtext_field_t   field,
                                    const gchar    * gst_tag,
                                    GstTagList    ** p_tags);

GstTagList  * gst_cdio_get_cdtext  (GstObject      * src,
                                    CdIo           * cdio,
                                    track_t          track);

void      gst_cdio_add_cdtext_album_tags (GstObject  * src,
                                          CdIo       * cdio,
                                          GstTagList * tags);

#endif /* __GST_CDIO_H__ */

