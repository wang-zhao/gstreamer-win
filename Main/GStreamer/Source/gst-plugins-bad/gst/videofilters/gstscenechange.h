/* GStreamer
 * Copyright (C) 2011 FIXME <fixme@example.com>
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

#ifndef _GST_SCENE_CHANGE_H_
#define _GST_SCENE_CHANGE_H_

#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include "gstvideofilter2.h"

G_BEGIN_DECLS

#define GST_TYPE_SCENE_CHANGE   (gst_scene_change_get_type())
#define GST_SCENE_CHANGE(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SCENE_CHANGE,GstSceneChange))
#define GST_SCENE_CHANGE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SCENE_CHANGE,GstSceneChangeClass))
#define GST_IS_SCENE_CHANGE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SCENE_CHANGE))
#define GST_IS_SCENE_CHANGE_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SCENE_CHANGE))

typedef struct _GstSceneChange GstSceneChange;
typedef struct _GstSceneChangeClass GstSceneChangeClass;

#define SC_N_DIFFS 5

struct _GstSceneChange
{
  GstVideoFilter2 base_scenechange;

  int n_diffs;
  double diffs[SC_N_DIFFS];
  GstBuffer *oldbuf;
};

struct _GstSceneChangeClass
{
  GstVideoFilter2Class base_scenechange_class;
};

GType gst_scene_change_get_type (void);

G_END_DECLS

#endif
