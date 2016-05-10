/* VP8
 * Copyright (C) 2006 David Schleef <ds@schleef.org>
 * Copyright (C) 2010 Entropy Wave Inc
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#include <gst/gst.h>
#include <vpx/vpx_codec.h>

G_BEGIN_DECLS

/* Some compatibility defines for older libvpx versions */
#ifndef VPX_IMG_FMT_I420
#define VPX_IMG_FMT_I420 IMG_FMT_I420
#endif

#ifndef VPX_PLANE_Y
#define VPX_PLANE_Y PLANE_Y
#endif

#ifndef VPX_PLANE_U
#define VPX_PLANE_U PLANE_U
#endif

#ifndef VPX_PLANE_V
#define VPX_PLANE_V PLANE_V
#endif

const char * gst_vpx_error_name (vpx_codec_err_t status);

G_END_DECLS
