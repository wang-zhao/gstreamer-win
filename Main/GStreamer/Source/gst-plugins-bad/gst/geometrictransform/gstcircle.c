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

#include "geometricmath.h"

#include "gstcircle.h"

GST_DEBUG_CATEGORY_STATIC (gst_circle_debug);
#define GST_CAT_DEFAULT gst_circle_debug

enum
{
  PROP_0,
  PROP_ANGLE,
  PROP_HEIGHT,
  PROP_SPREAD_ANGLE
};

#define DEFAULT_ANGLE 0
#define DEFAULT_HEIGHT 20
#define DEFAULT_SPREAD_ANGLE G_PI

GST_BOILERPLATE (GstCircle, gst_circle, GstCircleGeometricTransform,
    GST_TYPE_CIRCLE_GEOMETRIC_TRANSFORM);

static void
gst_circle_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstCircle *circle;
  GstGeometricTransform *gt;
  gdouble v;
  gint h;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
  circle = GST_CIRCLE_CAST (object);

  GST_OBJECT_LOCK (circle);
  switch (prop_id) {
    case PROP_ANGLE:
      v = g_value_get_double (value);
      if (v != circle->angle) {
        circle->angle = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_SPREAD_ANGLE:
      v = g_value_get_double (value);
      if (v != circle->spread_angle) {
        circle->spread_angle = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_HEIGHT:
      h = g_value_get_int (value);
      if (h != circle->height) {
        circle->height = h;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (circle);
}

static void
gst_circle_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCircle *circle;

  circle = GST_CIRCLE_CAST (object);

  switch (prop_id) {
    case PROP_ANGLE:
      g_value_set_double (value, circle->angle);
      break;
    case PROP_SPREAD_ANGLE:
      g_value_set_double (value, circle->spread_angle);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, circle->height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Clean up */
static void
gst_circle_finalize (GObject * obj)
{
  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/* GObject vmethod implementations */

static void
gst_circle_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "circle",
      "Transform/Effect/Video",
      "Warps the picture into an arc shaped form",
      "Thiago Santos<thiago.sousa.santos@collabora.co.uk>");
}

static gboolean
circle_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
  GstCircleGeometricTransform *cgt = GST_CIRCLE_GEOMETRIC_TRANSFORM_CAST (gt);
  GstCircle *circle = GST_CIRCLE_CAST (gt);
  gdouble distance;
  gdouble dx, dy;
  gdouble theta;

  dx = x - cgt->precalc_x_center;
  dy = y - cgt->precalc_y_center;
  distance = sqrt (dx * dx + dy * dy);
  theta = atan2 (-dy, -dx) + circle->angle;

  theta = mod_float (theta, 2 * G_PI);

  *in_x = gt->width * theta / (circle->spread_angle + 0.0001);
  *in_y =
      gt->height * (1 - (distance - cgt->precalc_radius) / (circle->height +
          0.0001));

  GST_DEBUG_OBJECT (circle, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_circle_class_init (GstCircleClass * klass)
{
  GObjectClass *gobject_class;
  GstGeometricTransformClass *gstgt_class;

  gobject_class = (GObjectClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_circle_finalize);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_circle_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_circle_get_property);

  g_object_class_install_property (gobject_class, PROP_ANGLE,
      g_param_spec_double ("angle", "angle",
          "Angle at which the arc starts in radians",
          -G_MAXDOUBLE, G_MAXDOUBLE, DEFAULT_ANGLE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SPREAD_ANGLE,
      g_param_spec_double ("spread-angle", "spread angle",
          "Length of the arc in radians",
          -G_MAXDOUBLE, G_MAXDOUBLE, DEFAULT_SPREAD_ANGLE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_int ("height", "height",
          "Height of the arc",
          0, G_MAXINT, DEFAULT_HEIGHT,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstgt_class->map_func = circle_map;
}

static void
gst_circle_init (GstCircle * filter, GstCircleClass * gclass)
{
  filter->angle = DEFAULT_ANGLE;
  filter->spread_angle = DEFAULT_SPREAD_ANGLE;
  filter->height = DEFAULT_HEIGHT;
}

gboolean
gst_circle_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_circle_debug, "circle", 0, "circle");

  return gst_element_register (plugin, "circle", GST_RANK_NONE,
      GST_TYPE_CIRCLE);
}
