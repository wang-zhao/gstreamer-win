/*
 * Image Scaling Functions
 * Copyright (c) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <string.h>

#include "vs_fill_borders.h"
#include "gstvideoscaleorc.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define READ_UINT32(ptr) GST_READ_UINT32_LE(ptr)
#define READ_UINT16(ptr) GST_READ_UINT16_LE(ptr)
#else
#define READ_UINT32(ptr) GST_READ_UINT32_BE(ptr)
#define READ_UINT16(ptr) GST_READ_UINT16_BE(ptr)
#endif


void
gst_video_scale_setup_vs_image(FFVSImage* image, GstVideoFormat format,
                               gint component, gint width, gint height, gint b_w, gint b_h, uint8_t* data) {
    image->real_width =
        gst_video_format_get_component_width(format, component, width);
    image->real_height =
        gst_video_format_get_component_height(format, component, height);
    image->width =
        gst_video_format_get_component_width(format, component, MAX(1,
                width - b_w));
    image->height =
        gst_video_format_get_component_height(format, component, MAX(1,
                height - b_h));
    image->stride = gst_video_format_get_row_stride(format, component, width);

    image->border_top = (image->real_height - image->height) / 2;
    image->border_bottom = image->real_height - image->height - image->border_top;

    if (format == GST_VIDEO_FORMAT_YUY2 || format == GST_VIDEO_FORMAT_YVYU
            || format == GST_VIDEO_FORMAT_UYVY) {
        g_assert(component == 0);

        image->border_left = (image->real_width - image->width) / 2;

        if (image->border_left % 2 == 1) {
            image->border_left--;
        }

        image->border_right = image->real_width - image->width - image->border_left;
    } else {
        image->border_left = (image->real_width - image->width) / 2;
        image->border_right = image->real_width - image->width - image->border_left;
    }

    if (format == GST_VIDEO_FORMAT_I420
            || format == GST_VIDEO_FORMAT_YV12
            || format == GST_VIDEO_FORMAT_Y444
            || format == GST_VIDEO_FORMAT_Y42B || format == GST_VIDEO_FORMAT_Y41B) {
        image->real_pixels = data + gst_video_format_get_component_offset(format,
                             component, width, height);
    } else {
        g_assert(component == 0);
        image->real_pixels = data;
    }

    image->pixels =
        image->real_pixels + image->border_top * image->stride +
        image->border_left * gst_video_format_get_pixel_stride(format,
                component);
}

const guint8*
get_black_for_format(GstVideoFormat format) {
    static const guint8 black[][4] = {
        {255, 0, 0, 0},             /*  0 = ARGB, ABGR, xRGB, xBGR */
        {0, 0, 0, 255},             /*  1 = RGBA, BGRA, RGBx, BGRx */
        {255, 16, 128, 128},        /*  2 = AYUV */
        {0, 0, 0, 0},               /*  3 = RGB and BGR */
        {16, 128, 128, 0},          /*  4 = v301 */
        {16, 128, 16, 128},         /*  5 = YUY2, YUYV */
        {128, 16, 128, 16},         /*  6 = UYVY */
        {16, 0, 0, 0},              /*  7 = Y */
        {0, 0, 0, 0}                /*  8 = RGB565, RGB666 */
    };

    switch (format) {
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_ARGB64:
        return black[0];

    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRx:
        return black[1];

    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_AYUV64:
        return black[2];

    case GST_VIDEO_FORMAT_RGB:
    case GST_VIDEO_FORMAT_BGR:
        return black[3];

    case GST_VIDEO_FORMAT_v308:
        return black[4];

    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_YVYU:
        return black[5];

    case GST_VIDEO_FORMAT_UYVY:
        return black[6];

    //    case GST_VIDEO_FORMAT_Y800:
    case GST_VIDEO_FORMAT_GRAY8:
        return black[7];

    case GST_VIDEO_FORMAT_GRAY16_LE:
    case GST_VIDEO_FORMAT_GRAY16_BE:
        //    case GST_VIDEO_FORMAT_Y16:
        return NULL;              /* Handled by the caller */

    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_Y41B:
        return black[4];          /* Y, U, V, 0 */

    case GST_VIDEO_FORMAT_RGB16:
    case GST_VIDEO_FORMAT_RGB15:
        return black[8];

    default:
        return NULL;
    }
}


void
vs_fill_borders_RGBA(const FFVSImage* dest, const uint8_t* val) {
    int i;
    int top = dest->border_top, bottom = dest->border_bottom;
    int left = dest->border_left, right = dest->border_right;
    int width = dest->width;
    int height = dest->height;
    int real_width = dest->real_width;
    int stride = dest->stride;
    int tmp, tmp2;
    uint8_t* data;
    uint32_t v = READ_UINT32(val);

    data = dest->real_pixels;

    for (i = 0; i < top; i++) {
        orc_splat_u32((uint32_t*) data, v, real_width);
        data += stride;
    }

    if (left || right) {
        tmp = height;
        tmp2 = (left + width) * 4;

        for (i = 0; i < tmp; i++) {
            orc_splat_u32((uint32_t*) data, v, left);
            orc_splat_u32((uint32_t*)(data + tmp2), v, right);
            data += stride;
        }
    } else {
        data += stride * height;
    }

    for (i = 0; i < bottom; i++) {
        orc_splat_u32((uint32_t*) data, v, real_width);
        data += stride;
    }
}

static void
_memset_u24(uint8_t* data, uint8_t val1, uint8_t val2, uint8_t val3,
            unsigned int n) {
    unsigned int i;

    for (i = 0; i < n; i++) {
        data[0] = val1;
        data[1] = val2;
        data[2] = val3;
        data += 3;
    }
}

void
vs_fill_borders_RGB(const FFVSImage* dest, const uint8_t* val) {
    int i;
    int top = dest->border_top, bottom = dest->border_bottom;
    int left = dest->border_left, right = dest->border_right;
    int width = dest->width;
    int height = dest->height;
    int real_width = dest->real_width;
    int stride = dest->stride;
    int tmp, tmp2;
    uint8_t* data;

    data = dest->real_pixels;

    for (i = 0; i < top; i++) {
        _memset_u24(data, val[0], val[1], val[2], real_width);
        data += stride;
    }

    if (left || right) {
        tmp = height;
        tmp2 = (left + width) * 3;

        for (i = 0; i < tmp; i++) {
            _memset_u24(data, val[0], val[1], val[2], left);
            _memset_u24(data + tmp2, val[0], val[1], val[2], right);
            data += stride;
        }
    } else {
        data += stride * height;
    }

    for (i = 0; i < bottom; i++) {
        _memset_u24(data, val[0], val[1], val[2], real_width);
        data += stride;
    }
}

void
vs_fill_borders_YUYV(const FFVSImage* dest, const uint8_t* val) {
    int i, j;
    int top = dest->border_top, bottom = dest->border_bottom;
    int left = dest->border_left, right = dest->border_right;
    int width = dest->width;
    int height = dest->height;
    int real_width = dest->real_width;
    int stride = dest->stride;
    int tmp, tmp2;
    uint8_t* data;

    data = dest->real_pixels;

    for (i = 0; i < top; i++) {
        for (j = 0; j < real_width; j++) {
            data[2 * j] = val[0];
            data[2 * j + 1] = (j % 2 == 0) ? val[1] : val[3];
        }

        data += stride;
    }

    if (left || right) {
        tmp = height;
        tmp2 = (left + width) * 2;

        for (i = 0; i < tmp; i++) {
            for (j = 0; j < left; j++) {
                data[2 * j] = val[0];
                data[2 * j + 1] = (j % 2 == 0) ? val[1] : val[3];
            }

            for (j = 0; j < right; j++) {
                data[tmp2 + 2 * j] = val[0];
                data[tmp2 + 2 * j + 1] = (j % 2 == 0) ? val[1] : val[3];
            }

            data += stride;
        }
    } else {
        data += stride * height;
    }

    for (i = 0; i < bottom; i++) {
        for (j = 0; j < real_width; j++) {
            data[2 * j] = val[0];
            data[2 * j + 1] = (j % 2 == 0) ? val[1] : val[3];
        }

        data += stride;
    }
}

void
vs_fill_borders_UYVY(const FFVSImage* dest, const uint8_t* val) {
    int i, j;
    int top = dest->border_top, bottom = dest->border_bottom;
    int left = dest->border_left, right = dest->border_right;
    int width = dest->width;
    int height = dest->height;
    int real_width = dest->real_width;
    int stride = dest->stride;
    int tmp, tmp2;
    uint8_t* data;

    data = dest->real_pixels;

    for (i = 0; i < top; i++) {
        for (j = 0; j < real_width; j++) {
            data[2 * j] = (j % 2 == 0) ? val[0] : val[2];
            data[2 * j + 1] = val[1];
        }

        data += stride;
    }

    if (left || right) {
        tmp = height;
        tmp2 = (left + width) * 2;

        for (i = 0; i < tmp; i++) {
            for (j = 0; j < left; j++) {
                data[2 * j] = (j % 2 == 0) ? val[0] : val[2];
                data[2 * j + 1] = val[1];
            }

            for (j = 0; j < right; j++) {
                data[tmp2 + 2 * j] = (j % 2 == 0) ? val[0] : val[2];
                data[tmp2 + 2 * j + 1] = val[1];
            }

            data += stride;
        }
    } else {
        data += stride * height;
    }

    for (i = 0; i < bottom; i++) {
        for (j = 0; j < real_width; j++) {
            data[2 * j] = (j % 2 == 0) ? val[0] : val[2];
            data[2 * j + 1] = val[1];
        }

        data += stride;
    }
}

void
vs_fill_borders_Y(const FFVSImage* dest, const uint8_t* val) {
    int i;
    int top = dest->border_top, bottom = dest->border_bottom;
    int left = dest->border_left, right = dest->border_right;
    int width = dest->width;
    int height = dest->height;
    int real_width = dest->real_width;
    int stride = dest->stride;
    int tmp, tmp2;
    uint8_t* data;

    data = dest->real_pixels;

    for (i = 0; i < top; i++) {
        memset(data, *val, real_width);
        data += stride;
    }

    if (left || right) {
        tmp = height;
        tmp2 = left + width;

        for (i = 0; i < tmp; i++) {
            memset(data, *val, left);
            memset(data + tmp2, *val, right);
            data += stride;
        }
    } else {
        data += stride * height;
    }

    for (i = 0; i < bottom; i++) {
        memset(data, *val, real_width);
        data += stride;
    }
}

void
vs_fill_borders_Y16(const FFVSImage* dest, const uint16_t val) {
    int i;
    int top = dest->border_top, bottom = dest->border_bottom;
    int left = dest->border_left, right = dest->border_right;
    int width = dest->width;
    int height = dest->height;
    int real_width = dest->real_width;
    int stride = dest->stride;
    int tmp, tmp2;
    uint8_t* data;

    data = dest->real_pixels;

    for (i = 0; i < top; i++) {
        orc_splat_u16((uint16_t*) data, val, real_width);
        data += stride;
    }

    if (left || right) {
        tmp = height;
        tmp2 = (left + width) * 2;

        for (i = 0; i < tmp; i++) {
            orc_splat_u16((uint16_t*) data, val, left);
            orc_splat_u16((uint16_t*)(data + tmp2), val, right);
            data += stride;
        }
    } else {
        data += stride * height;
    }

    for (i = 0; i < bottom; i++) {
        orc_splat_u16((uint16_t*) data, val, real_width);
        data += stride;
    }
}

void
vs_fill_borders_RGB565(const FFVSImage* dest, const uint8_t* val) {
    int i;
    int top = dest->border_top, bottom = dest->border_bottom;
    int left = dest->border_left, right = dest->border_right;
    int width = dest->width;
    int height = dest->height;
    int real_width = dest->real_width;
    int stride = dest->stride;
    int tmp, tmp2;
    uint8_t* data;
    uint16_t v = READ_UINT16(val);

    data = dest->real_pixels;

    for (i = 0; i < top; i++) {
        orc_splat_u16((uint16_t*) data, v, real_width);
        data += stride;
    }

    if (left || right) {
        tmp = height;
        tmp2 = (left + width) * 2;

        for (i = 0; i < tmp; i++) {
            orc_splat_u16((uint16_t*) data, v, left);
            orc_splat_u16((uint16_t*)(data + tmp2), v, right);
            data += stride;
        }
    } else {
        data += stride * height;
    }

    for (i = 0; i < bottom; i++) {
        orc_splat_u16((uint16_t*) data, v, real_width);
        data += stride;
    }
}

void
vs_fill_borders_RGB555(const FFVSImage* dest, const uint8_t* val) {
    int i;
    int top = dest->border_top, bottom = dest->border_bottom;
    int left = dest->border_left, right = dest->border_right;
    int width = dest->width;
    int height = dest->height;
    int real_width = dest->real_width;
    int stride = dest->stride;
    int tmp, tmp2;
    uint8_t* data;
    uint16_t v = READ_UINT16(val);

    data = dest->real_pixels;

    for (i = 0; i < top; i++) {
        orc_splat_u16((uint16_t*) data, v, real_width);
        data += stride;
    }

    if (left || right) {
        tmp = height;
        tmp2 = (left + width) * 2;

        for (i = 0; i < tmp; i++) {
            orc_splat_u16((uint16_t*) data, v, left);
            orc_splat_u16((uint16_t*)(data + tmp2), v, right);
            data += stride;
        }
    } else {
        data += stride * height;
    }

    for (i = 0; i < bottom; i++) {
        orc_splat_u16((uint16_t*) data, v, real_width);
        data += stride;
    }
}

void
vs_fill_borders_AYUV64(const FFVSImage* dest, const uint8_t* val) {
    int i;
    int top = dest->border_top, bottom = dest->border_bottom;
    int left = dest->border_left, right = dest->border_right;
    int width = dest->width;
    int height = dest->height;
    int real_width = dest->real_width;
    int stride = dest->stride;
    int tmp, tmp2;
    uint8_t* data;
    uint64_t v;

    v = (((guint32) val[0]) << 8) | (((guint32) val[1]) << 24) |
        (((guint64) val[2]) << 40) | (((guint64) val[3]) << 56);

    data = dest->real_pixels;

    for (i = 0; i < top; i++) {
        orc_splat_u64((uint64_t*) data, v, real_width);
        data += stride;
    }

    if (left || right) {
        tmp = height;
        tmp2 = (left + width) * 8;

        for (i = 0; i < tmp; i++) {
            orc_splat_u64((uint64_t*) data, v, left);
            orc_splat_u64((uint64_t*)(data + tmp2), v, right);
            data += stride;
        }
    } else {
        data += stride * height;
    }

    for (i = 0; i < bottom; i++) {
        orc_splat_u64((uint64_t*) data, v, real_width);
        data += stride;
    }
}
