/*
 * GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

/*
 * Thanks to Jerry Huxtable <http://www.jhlabs.com> work on its java
 * image editor and filters. The algorithms here were extracted from
 * his code.
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gstwaterripple.h"

GST_DEBUG_CATEGORY_STATIC (gst_water_ripple_debug);
#define GST_CAT_DEFAULT gst_water_ripple_debug

enum
{
  PROP_0,
  PROP_AMPLITUDE,
  PROP_PHASE,
  PROP_WAVELENGTH
};

#define DEFAULT_AMPLITUDE 10.0
#define DEFAULT_PHASE 0.0
#define DEFAULT_WAVELENGTH 16.0

GST_BOILERPLATE (GstWaterRipple, gst_water_ripple, GstCircleGeometricTransform,
    GST_TYPE_CIRCLE_GEOMETRIC_TRANSFORM);

static void
gst_water_ripple_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWaterRipple *water_ripple;
  GstGeometricTransform *gt;
  gdouble v;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
  water_ripple = GST_WATER_RIPPLE_CAST (object);

  GST_OBJECT_LOCK (gt);
  switch (prop_id) {
    case PROP_AMPLITUDE:
      v = g_value_get_double (value);
      if (v != water_ripple->amplitude) {
        water_ripple->amplitude = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_PHASE:
      v = g_value_get_double (value);
      if (v != water_ripple->phase) {
        water_ripple->phase = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_WAVELENGTH:
      v = g_value_get_double (value);
      if (v != water_ripple->wavelength) {
        water_ripple->wavelength = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (gt);
}

static void
gst_water_ripple_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWaterRipple *water_ripple;

  water_ripple = GST_WATER_RIPPLE_CAST (object);

  switch (prop_id) {
    case PROP_AMPLITUDE:
      g_value_set_double (value, water_ripple->amplitude);
      break;
    case PROP_PHASE:
      g_value_set_double (value, water_ripple->phase);
      break;
    case PROP_WAVELENGTH:
      g_value_set_double (value, water_ripple->wavelength);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Clean up */
static void
gst_water_ripple_finalize (GObject * obj)
{
  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/* GObject vmethod implementations */

static void
gst_water_ripple_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "waterripple",
      "Transform/Effect/Video",
      "Creates a water ripple effect on the image",
      "Thiago Santos<thiago.sousa.santos@collabora.co.uk>");
}

static gboolean
water_ripple_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
  GstCircleGeometricTransform *cgt = GST_CIRCLE_GEOMETRIC_TRANSFORM_CAST (gt);
  GstWaterRipple *water = GST_WATER_RIPPLE_CAST (gt);
  gdouble distance;
  gdouble dx, dy;

  dx = x - cgt->precalc_x_center;
  dy = y - cgt->precalc_y_center;
  distance = dx * dx + dy * dy;

  if (distance > cgt->precalc_radius2) {
    *in_x = x;
    *in_y = y;
  } else {
    gdouble d = sqrt (distance);
    gdouble amount =
        water->amplitude * sin (d / water->wavelength * G_PI * 2 -
        water->phase);

    amount *= (cgt->precalc_radius - d) / cgt->precalc_radius;
    if (d != 0)
      amount *= water->wavelength / d;
    *in_x = x + dx * amount;
    *in_y = y + dy * amount;
  }


  GST_DEBUG_OBJECT (water, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_water_ripple_class_init (GstWaterRippleClass * klass)
{
  GObjectClass *gobject_class;
  GstGeometricTransformClass *gstgt_class;

  gobject_class = (GObjectClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_water_ripple_finalize);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_water_ripple_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_water_ripple_get_property);

  g_object_class_install_property (gobject_class, PROP_AMPLITUDE,
      g_param_spec_double ("amplitude", "amplitude", "amplitude",
          -G_MAXDOUBLE, G_MAXDOUBLE, DEFAULT_AMPLITUDE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PHASE,
      g_param_spec_double ("phase", "phase", "phase",
          -G_MAXDOUBLE, G_MAXDOUBLE, DEFAULT_PHASE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_WAVELENGTH,
      g_param_spec_double ("wavelength", "wavelength", "wavelength",
          -G_MAXDOUBLE, G_MAXDOUBLE, DEFAULT_WAVELENGTH,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstgt_class->map_func = water_ripple_map;
}

static void
gst_water_ripple_init (GstWaterRipple * filter, GstWaterRippleClass * gclass)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM (filter);

  gt->off_edge_pixels = GST_GT_OFF_EDGES_PIXELS_CLAMP;
  filter->amplitude = DEFAULT_AMPLITUDE;
  filter->phase = DEFAULT_PHASE;
  filter->wavelength = DEFAULT_WAVELENGTH;
}

gboolean
gst_water_ripple_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_water_ripple_debug, "waterripple", 0,
      "waterripple");

  return gst_element_register (plugin, "waterripple", GST_RANK_NONE,
      GST_TYPE_WATER_RIPPLE);
}
