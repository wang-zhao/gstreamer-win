/* GStreamer xvid encoder/decoder plugin
 * Copyright (C) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
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

#include <string.h>
#include <xvid.h>

#include <gst/video/video.h>
#include "gstxviddec.h"
#include "gstxvidenc.h"

gboolean
gst_xvid_init (void)
{
  xvid_gbl_init_t xinit;
  gint ret;
  static gboolean is_init = FALSE;

  /* only init once */
  if (is_init == TRUE) {
    return TRUE;
  }

  /* set up xvid initially (function pointers, CPU flags) */
  gst_xvid_init_struct (xinit);

  if ((ret = xvid_global (NULL, XVID_GBL_INIT, &xinit, NULL)) < 0) {
    g_warning ("Failed to initialize XviD: %s (%d)", gst_xvid_error (ret), ret);
    return FALSE;
  }

  GST_LOG ("Initted XviD version %d.%d.%d (API %d.%d)",
      XVID_VERSION_MAJOR (XVID_VERSION),
      XVID_VERSION_MINOR (XVID_VERSION),
      XVID_VERSION_PATCH (XVID_VERSION),
      XVID_API_MAJOR (XVID_API), XVID_API_MINOR (XVID_API));

  is_init = TRUE;
  return TRUE;
}

const gchar *
gst_xvid_error (int errorcode)
{
  const gchar *error;

  switch (errorcode) {
    case XVID_ERR_FAIL:
      error = "Operation failed";
      break;
    case 0:
      error = "No error";
      break;
    case XVID_ERR_MEMORY:
      error = "Memory allocation error";
      break;
    case XVID_ERR_FORMAT:
      error = "File format not supported";
      break;
    case XVID_ERR_VERSION:
      error = "Structure version not supported";
      break;
    default:
      error = "Unknown error";
      break;
  }

  return error;
}

gint
gst_xvid_structure_to_csp (GstStructure * structure)
{
  const gchar *mime = gst_structure_get_name (structure);
  gint xvid_cs = -1;

  if (!strcmp (mime, "video/x-raw-yuv")) {
    guint32 fourcc;

    gst_structure_get_fourcc (structure, "format", &fourcc);
    switch (fourcc) {
      case GST_MAKE_FOURCC ('I', '4', '2', '0'):
        xvid_cs = XVID_CSP_I420;
        break;
      case GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'):
        xvid_cs = XVID_CSP_YUY2;
        break;
      case GST_MAKE_FOURCC ('Y', 'V', '1', '2'):
        xvid_cs = XVID_CSP_YV12;
        break;
      case GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'):
        xvid_cs = XVID_CSP_UYVY;
        break;
      case GST_MAKE_FOURCC ('Y', 'V', 'Y', 'U'):
        xvid_cs = XVID_CSP_YVYU;
        break;
    }
  } else {
    gint depth, bpp, r_mask;

    gst_structure_get_int (structure, "depth", &depth);
    gst_structure_get_int (structure, "bpp", &bpp);
    gst_structure_get_int (structure, "red_mask", &r_mask);

    switch (depth) {
      case 15:
        xvid_cs = XVID_CSP_RGB555;
        break;
      case 16:
        xvid_cs = XVID_CSP_RGB565;
        break;
      case 24:
        if (bpp == 24) {
          xvid_cs = XVID_CSP_BGR;
        } else {
          switch (r_mask) {
            case 0xff000000:
              xvid_cs = XVID_CSP_RGBA;
              break;
#ifdef XVID_CSP_ARGB
            case 0x00ff0000:
              xvid_cs = XVID_CSP_ARGB;
              break;
#endif
            case 0x0000ff00:
              xvid_cs = XVID_CSP_BGRA;
              break;
            case 0x000000ff:
              xvid_cs = XVID_CSP_ABGR;
              break;
          }
        }
        break;
      default:
        break;
    }

  }

  return xvid_cs;
}

GstCaps *
gst_xvid_csp_to_caps (gint csp, gint w, gint h)
{
  GstStructure *structure = NULL;

  switch (csp) {
    case XVID_CSP_RGB555:
    case XVID_CSP_RGB565:
    case XVID_CSP_BGR:
    case XVID_CSP_ABGR:
    case XVID_CSP_BGRA:
#ifdef XVID_CSP_ARGB
    case XVID_CSP_ARGB:
#endif
    case XVID_CSP_RGBA:{
      gint r_mask = 0, b_mask = 0, g_mask = 0,
          endianness = 0, bpp = 0, depth = 0;

      switch (csp) {
        case XVID_CSP_RGB555:
          r_mask = GST_VIDEO_COMP1_MASK_15_INT;
          g_mask = GST_VIDEO_COMP2_MASK_15_INT;
          b_mask = GST_VIDEO_COMP3_MASK_15_INT;
          endianness = G_BYTE_ORDER;
          depth = 15;
          bpp = 16;
          break;
        case XVID_CSP_RGB565:
          r_mask = GST_VIDEO_COMP1_MASK_16_INT;
          g_mask = GST_VIDEO_COMP2_MASK_16_INT;
          b_mask = GST_VIDEO_COMP3_MASK_16_INT;
          endianness = G_BYTE_ORDER;
          depth = 16;
          bpp = 16;
          break;
        case XVID_CSP_BGR:
          r_mask = 0x0000ff;
          g_mask = 0x00ff00;
          b_mask = 0xff0000;
          endianness = G_BIG_ENDIAN;
          depth = 24;
          bpp = 24;
          break;
        case XVID_CSP_ABGR:
          r_mask = 0x000000ff;
          g_mask = 0x0000ff00;
          b_mask = 0x00ff0000;
          endianness = G_BIG_ENDIAN;
          depth = 24;
          bpp = 32;
          break;
        case XVID_CSP_BGRA:
          r_mask = 0x0000ff00;
          g_mask = 0x00ff0000;
          b_mask = 0xff000000;
          endianness = G_BIG_ENDIAN;
          depth = 24;
          bpp = 32;
          break;
#ifdef XVID_CSP_ARGB
        case XVID_CSP_ARGB:
          r_mask = 0x00ff0000;
          g_mask = 0x0000ff00;
          b_mask = 0x000000ff;
          endianness = G_BIG_ENDIAN;
          depth = 24;
          bpp = 32;
          break;
#endif
        case XVID_CSP_RGBA:
          r_mask = 0xff000000;
          g_mask = 0x00ff0000;
          b_mask = 0x0000ff00;
          endianness = G_BIG_ENDIAN;
          depth = 24;
          bpp = 32;
          break;
      }

      structure = gst_structure_new ("video/x-raw-rgb",
          "width", G_TYPE_INT, w,
          "height", G_TYPE_INT, h,
          "depth", G_TYPE_INT, depth,
          "bpp", G_TYPE_INT, bpp,
          "endianness", G_TYPE_INT, endianness,
          "red_mask", G_TYPE_INT, r_mask,
          "green_mask", G_TYPE_INT, g_mask,
          "blue_mask", G_TYPE_INT, b_mask, NULL);
      break;
    }

    case XVID_CSP_YUY2:
    case XVID_CSP_YVYU:
    case XVID_CSP_UYVY:
    case XVID_CSP_I420:
    case XVID_CSP_YV12:{
      guint32 fourcc = 0;

      switch (csp) {
        case XVID_CSP_YUY2:
          fourcc = GST_MAKE_FOURCC ('Y', 'U', 'Y', '2');
          break;
        case XVID_CSP_YVYU:
          fourcc = GST_MAKE_FOURCC ('Y', 'V', 'Y', 'U');
          break;
        case XVID_CSP_UYVY:
          fourcc = GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y');
          break;
        case XVID_CSP_I420:
          fourcc = GST_MAKE_FOURCC ('I', '4', '2', '0');
          break;
        case XVID_CSP_YV12:
          fourcc = GST_MAKE_FOURCC ('Y', 'V', '1', '2');
          break;
      }

      structure = gst_structure_new ("video/x-raw-yuv",
          "width", G_TYPE_INT, w,
          "height", G_TYPE_INT, h, "format", GST_TYPE_FOURCC, fourcc, NULL);
      break;
    }
  }

  return gst_caps_new_full (structure, NULL);
}


gint
gst_xvid_image_get_size (gint csp, gint width, gint height)
{
  xvid_image_t dummy_im;

  return gst_xvid_image_fill (&dummy_im, NULL, csp, width, height);
}

gint
gst_xvid_image_fill (xvid_image_t * im, void *ptr, gint csp,
    gint width, gint height)
{
  gint stride, h2, size = 0;

  im->csp = csp;

  switch (csp) {
    case XVID_CSP_I420:
    case XVID_CSP_YV12:
      /* planar */
      /* luma */
      stride = GST_ROUND_UP_4 (width);
      h2 = GST_ROUND_UP_2 (height);
      im->stride[0] = stride;
      im->plane[0] = ptr;
      /* chroma */
      im->plane[1] = ((guint8 *) im->plane[0]) + (stride * h2);
      size += stride * height;
      stride = GST_ROUND_UP_8 (width) / 2;
      h2 = GST_ROUND_UP_2 (height) / 2;
      im->stride[1] = stride;

      im->plane[2] = ((guint8 *) im->plane[1]) + (stride * h2);
      im->stride[2] = stride;
      size += 2 * (stride * h2);
      break;
    case XVID_CSP_RGB555:
    case XVID_CSP_RGB565:
    case XVID_CSP_YUY2:
    case XVID_CSP_UYVY:
    case XVID_CSP_YVYU:
      /* packed */
      stride = GST_ROUND_UP_4 (width * 2);
      im->plane[0] = ptr;
      im->stride[0] = stride;
      size = stride * height;
      break;
    case XVID_CSP_BGR:
      stride = GST_ROUND_UP_4 (width * 3);
      im->plane[0] = ptr;
      im->stride[0] = stride;
      size = stride * height * 2;
      break;
    case XVID_CSP_ABGR:
    case XVID_CSP_BGRA:
    case XVID_CSP_RGBA:
#ifdef XVID_CSP_ARGB
    case XVID_CSP_ARGB:
#endif
      stride = width * 4;
      im->plane[0] = ptr;
      im->stride[0] = stride;
      size = stride * height;
      break;
  }

  return size;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return (gst_element_register (plugin, "xvidenc",
          GST_RANK_SECONDARY, GST_TYPE_XVIDENC) &&
      gst_element_register (plugin, "xviddec",
          GST_RANK_NONE, GST_TYPE_XVIDDEC));
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "xvid",
    "XviD plugin library", plugin_init, VERSION, "GPL", GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
