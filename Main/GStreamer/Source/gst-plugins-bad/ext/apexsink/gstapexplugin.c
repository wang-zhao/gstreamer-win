/* GStreamer AirPort Express Plugin
 *
 * Copyright (C) 2008 Jérémie Bernard [GRemi] <gremimail@gmail.com>
 *
 * gstapexpugin.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gstapexsink.h>

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, GST_APEX_SINK_NAME, GST_RANK_NONE,
      GST_TYPE_APEX_SINK);
}

/* plugin export resolution */
GST_PLUGIN_DEFINE
    (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "apexsink",
    "Apple AirPort Express Plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
