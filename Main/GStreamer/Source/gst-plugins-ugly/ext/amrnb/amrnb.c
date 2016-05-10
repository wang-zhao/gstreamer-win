/* GStreamer Adaptive Multi-Rate Narrow-Band (AMR-NB) plugin
 * Copyright (C) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
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

#include "amrnbdec.h"
#include "amrnbenc.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "amrnbdec",
      GST_RANK_PRIMARY, GST_TYPE_AMRNBDEC) &&
      gst_element_register (plugin, "amrnbenc",
      GST_RANK_SECONDARY, GST_TYPE_AMRNBENC);
}


GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "amrnb",
    "Adaptive Multi-Rate Narrow-Band",
    plugin_init, VERSION, GST_LICENSE_UNKNOWN, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN);
