/*
 * GStreamer
 * Copyright (C) 2008 Nokia Corporation <multimedia@maemo.org>
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

#ifndef __CAMERABIN_GENERAL_H_
#define __CAMERABIN_GENERAL_H_

#include <gst/gst.h>

gboolean gst_camerabin_try_add_element (GstBin * bin, GstElement * new_elem);
gboolean gst_camerabin_add_element (GstBin * bin, GstElement * new_elem);
GstElement *gst_camerabin_create_and_add_element (GstBin * bin, const gchar * elem_name, const gchar * instance_name);

GstElement * gst_camerabin_setup_default_element (GstBin * bin, GstElement *user_elem, const gchar *auto_elem_name, const gchar *default_elem_name);

void gst_camerabin_remove_elements_from_bin (GstBin * bin);

gboolean gst_camerabin_drop_eos_probe (GstPad * pad, GstEvent * event, gpointer u_data);

#endif /* #ifndef __CAMERABIN_GENERAL_H_ */
