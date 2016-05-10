/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2002> David A. Schleef <ds@schleef.org>
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

/**
 * SECTION:element-videotestsrc
 *
 * The videotestsrc element is used to produce test video data in a wide variety
 * of formats. The video test data produced can be controlled with the "pattern"
 * property.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v videotestsrc pattern=snow ! ximagesink
 * ]| Shows random noise in an X window.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstavcfilesrc.h"

#include <string.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY_STATIC(avc_file_src_debug);
#define GST_CAT_DEFAULT avc_file_src_debug


GST_BOILERPLATE(GstAvcFileSrc, gst_avc_file_src, GstPushSrc,
                GST_TYPE_PUSH_SRC);

static GstCaps* gst_avc_file_src_getcaps(GstBaseSrc* bsrc);
static void gst_avc_file_src_src_fixate(GstPad* pad, GstCaps* caps);

static gboolean gst_avc_file_src_is_seekable(GstBaseSrc* psrc);
static gboolean gst_avc_file_src_query(GstBaseSrc* bsrc, GstQuery* query);

static void gst_avc_file_src_get_times(GstBaseSrc* basesrc,
                                       GstBuffer* buffer, GstClockTime* start, GstClockTime* end);
static GstFlowReturn gst_avc_file_src_create(GstPushSrc* psrc,
        GstBuffer** buffer);
static gboolean gst_avc_file_src_start(GstBaseSrc* basesrc);


static void
gst_avc_file_src_base_init(gpointer g_class) {
    GstElementClass* element_class = GST_ELEMENT_CLASS(g_class);
    GstPadTemplate* pad_template;

    gst_element_class_set_details_simple(element_class,
                                         "avc file source", "Source/Video",
                                         "extract avc data from data", "wangzhao04");

    pad_template =
        gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                             gst_avc_file_src_getcaps(NULL));
    gst_element_class_add_pad_template(element_class, pad_template);
    gst_object_unref(pad_template);
}

static void
gst_avc_file_src_class_init(GstAvcFileSrcClass* klass) {
    GObjectClass* gobject_class;
    GstBaseSrcClass* gstbasesrc_class;
    GstPushSrcClass* gstpushsrc_class;

    gobject_class = (GObjectClass*) klass;
    gstbasesrc_class = (GstBaseSrcClass*) klass;
    gstpushsrc_class = (GstPushSrcClass*) klass;

    gstbasesrc_class->get_caps = gst_avc_file_src_getcaps;
    gstbasesrc_class->is_seekable = gst_avc_file_src_is_seekable;
    gstbasesrc_class->query = gst_avc_file_src_query;
    gstbasesrc_class->get_times = gst_avc_file_src_get_times;
    gstbasesrc_class->start = gst_avc_file_src_start;

    gstpushsrc_class->create = gst_avc_file_src_create;
}

static void
gst_avc_file_src_init(GstAvcFileSrc* src, GstAvcFileSrcClass* g_class) {
    GstPad* pad = GST_BASE_SRC_PAD(src);

    gst_pad_set_fixatecaps_function(pad, gst_avc_file_src_src_fixate);

    /* we operate in time */
    gst_base_src_set_format(GST_BASE_SRC(src), GST_FORMAT_TIME);
    src->file = NULL;
    src->width = 1280;
    src->height = 720;
    src->rate_numerator = 18;
    src->rate_denominator = 1;
}

static void
gst_avc_file_src_src_fixate(GstPad* pad, GstCaps* caps) {
    GstStructure* structure;

    structure = gst_caps_get_structure(caps, 0);

    gst_structure_fixate_field_nearest_int(structure, "width", 320);
    gst_structure_fixate_field_nearest_int(structure, "height", 240);
    gst_structure_fixate_field_nearest_fraction(structure, "framerate", 30, 1);

    if (gst_structure_has_field(structure, "pixel-aspect-ratio"))
        gst_structure_fixate_field_nearest_fraction(structure,
                "pixel-aspect-ratio", 1, 1);

    if (gst_structure_has_field(structure, "color-matrix")) {
        gst_structure_fixate_field_string(structure, "color-matrix", "sdtv");
    }

    if (gst_structure_has_field(structure, "chroma-site")) {
        gst_structure_fixate_field_string(structure, "chroma-site", "mpeg2");
    }

    if (gst_structure_has_field(structure, "interlaced")) {
        gst_structure_fixate_field_boolean(structure, "interlaced", FALSE);
    }
}

/* threadsafe because this gets called as the plugin is loaded */
static GstCaps*
gst_avc_file_src_getcaps(GstBaseSrc* bsrc) {
    GstAvcFileSrc* src;
    GstCaps* caps = NULL;
    GstBuffer* priv;
    unsigned char codec_data[] = { 0x01, 0x4d, 0x40, 0x1f, 0xff, 0xe1, 0x00, 0x18, 0x67, 0x4d, 0x40, 0x1f, 0xd9, 0x00, 0x50, 0x05
                                   , 0xbb, 0x01, 0x10, 0x00, 0x00, 0x03, 0x00, 0x17, 0x73, 0x59, 0x40, 0x00, 0xf1, 0x83, 0x24, 0x80
                                   , 0x01, 0x00, 0x05, 0x68, 0xeb, 0xc1, 0x32, 0xc8
                                 };

    if (bsrc) {
        src = GST_AVC_FILE_SRC(bsrc);

        caps = gst_caps_new_simple("video/x-h264", "stream-format", G_TYPE_STRING, "avc",
                                   "alignment", G_TYPE_STRING, "au", "width", G_TYPE_INT, src->width,
                                   "height", G_TYPE_INT, src->height, "framerate", GST_TYPE_FRACTION, src->rate_numerator,
                                   src->rate_denominator, NULL);

        priv =  gst_buffer_new_and_alloc(sizeof(codec_data));
        memcpy(GST_BUFFER_DATA(priv), codec_data, sizeof(codec_data));
        gst_caps_set_simple(caps, "codec_data", GST_TYPE_BUFFER, priv, NULL);
        gst_buffer_unref(priv);
    } else {
        caps = gst_caps_new_simple("video/x-h264", "stream-format", G_TYPE_STRING, "avc",
                                   "alignment", G_TYPE_STRING, "au", "width", G_TYPE_INT, 1280,
                                   "height", G_TYPE_INT, 720, "framerate", GST_TYPE_FRACTION, 18, 1, NULL);
    }

    return caps;
}

static gboolean
gst_avc_file_src_query(GstBaseSrc* bsrc, GstQuery* query) {
    gboolean res;
    GstAvcFileSrc* src;

    src = GST_AVC_FILE_SRC(bsrc);

    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_CONVERT: {
        GstFormat src_fmt, dest_fmt;
        gint64 src_val, dest_val;

        gst_query_parse_convert(query, &src_fmt, &src_val, &dest_fmt, &dest_val);

        if (src_fmt == dest_fmt) {
            dest_val = src_val;
            goto done;
        }

        switch (src_fmt) {
        case GST_FORMAT_DEFAULT:
            switch (dest_fmt) {
            case GST_FORMAT_TIME:

                /* frames to time */
                if (src->rate_numerator) {
                    dest_val = gst_util_uint64_scale(src_val,
                                                     src->rate_denominator * GST_SECOND, src->rate_numerator);
                } else {
                    dest_val = 0;
                }

                break;

            default:
                goto error;
            }

            break;

        case GST_FORMAT_TIME:
            switch (dest_fmt) {
            case GST_FORMAT_DEFAULT:

                /* time to frames */
                if (src->rate_numerator) {
                    dest_val = gst_util_uint64_scale(src_val,
                                                     src->rate_numerator, src->rate_denominator * GST_SECOND);
                } else {
                    dest_val = 0;
                }

                break;

            default:
                goto error;
            }

            break;

        default:
            goto error;
        }

done:
        gst_query_set_convert(query, src_fmt, src_val, dest_fmt, dest_val);
        res = TRUE;
        break;
    }

    default:
        res = GST_BASE_SRC_CLASS(parent_class)->query(bsrc, query);
    }

    return res;

    /* ERROR */
error: {
        GST_DEBUG_OBJECT(src, "query failed");
        return FALSE;
    }
}

static void
gst_avc_file_src_get_times(GstBaseSrc* basesrc, GstBuffer* buffer,
                           GstClockTime* start, GstClockTime* end) {
    /* for live sources, sync on the timestamp of the buffer */
    if (gst_base_src_is_live(basesrc)) {
        GstClockTime timestamp = GST_BUFFER_TIMESTAMP(buffer);

        if (GST_CLOCK_TIME_IS_VALID(timestamp)) {
            /* get duration to calculate end time */
            GstClockTime duration = GST_BUFFER_DURATION(buffer);

            if (GST_CLOCK_TIME_IS_VALID(duration)) {
                *end = timestamp + duration;
            }

            *start = timestamp;
        }
    } else {
        *start = -1;
        *end = -1;
    }
}

static gboolean
gst_avc_file_src_is_seekable(GstBaseSrc* psrc) {
    /* we're seekable... */
    return FALSE;
}

static char g_buf[4096];
static int g_buf_size = 0;
static char g_key[] = {0, 0, 0, 2, 9};

int fillBuf(FILE* file) {
    int ret;

    if (feof(file)) {
        return 0;
    }

    ret = fread(&g_buf[g_buf_size], 1, sizeof(g_buf) - g_buf_size, file);

    if (ret > 0) {
        g_buf_size += ret;
        return 1;
    } else {
        return 0;
    }
}

int findKey(char* data, int size) {
    int i;
    size -= 5;

    for (i = 0 ; i < size; i ++) {
        if (memcmp(&data[i], g_key, 5) == 0) {
            return i;
        }
    }

    return -1;
}

void fillFrame(FILE* file, char** data, int* size) {
    fillBuf(file);

    if (g_buf_size > 4) {
        char* buf;
        int seek_back;
        int nalsize = ((unsigned int)g_buf[0] & 0xff) << 24 | ((unsigned int)g_buf[1] & 0xff) << 16 |
                      ((unsigned int)g_buf[2] & 0xff) << 8 | ((unsigned int)g_buf[3] & 0xff);

        if (nalsize > 0xFFFFF || nalsize < 0) {
            return;
        }

        buf = (char*)g_malloc(nalsize);
        seek_back = 0;

        if (nalsize + 4 < g_buf_size) {
            memcpy(buf, &g_buf[4], nalsize);
        } else {
            int left;
            memcpy(buf, &g_buf[4], g_buf_size - 4);
            left = nalsize - g_buf_size + 4;

            if (left != (seek_back = fread(&buf[g_buf_size - 4], 1, left, file))) {
                fseek(file, -seek_back, SEEK_CUR);
                g_free(buf);
                return;
            }
        }

        if (findKey(buf, nalsize) > 0) {
            if (seek_back > 0) {
                fseek(file, -seek_back, SEEK_CUR);
            }

            g_free(buf);
            return;
        }

        *data = (char*)g_realloc(*data, *size + nalsize + 4);
        memcpy(*data + *size, &g_buf[0], 4);
        *size += 4;
        memcpy(*data + *size, buf, nalsize);
        *size += nalsize;
        g_free(buf);

        if (nalsize + 4 < g_buf_size) {
            g_buf_size -= nalsize + 4;
            memmove(&g_buf[0], &g_buf[nalsize + 4], g_buf_size);
        } else {
            g_buf_size = 0;
        }

        fillFrame(file, data, size);
    }
}


int getFrame(FILE* file, char** data, int* size) {
    *data = NULL;
    *size = 0;

    do {
        if (g_buf_size >= 8) {
            int key = findKey(g_buf, g_buf_size);

            if (key >= 0 && key + 7 < g_buf_size) {
                if (g_buf[key + 6] == 0 && g_buf[key + 7] == 0) {
                    *data = (char*)g_realloc(*data, 6);
                    memcpy(*data, &g_buf[key], 6);
                    *size += 6;
                    g_buf_size -= key + 6;
                    memmove(&g_buf[0], &g_buf[key + 6], g_buf_size);
                    fillFrame(file, data, size);
                    return 1;
                } else {
                    g_buf_size -= key + 5;
                    memmove(&g_buf[0], &g_buf[key + 5], g_buf_size);
                    continue;
                }
            } else {
                memmove(&g_buf[0], &g_buf[g_buf_size - 7], 7);
                g_buf_size = 7;
                continue;
            }
        }
    } while (fillBuf(file));

    return 0;
}
static GstFlowReturn
gst_avc_file_src_create(GstPushSrc* psrc, GstBuffer** buffer) {
    GstAvcFileSrc* src;
    gulong newsize;
    GstBuffer* outbuf = NULL;
    GstFlowReturn res;
    GstClockTime next_time;
    unsigned char* data;

    src = GST_AVC_FILE_SRC(psrc);

    if (!src->file) {
        src->file = fopen("d:\\screen.mp4", "rb");

        if (!src->file) {
            goto eos;
        }
    }


    /* 0 framerate and we are at the second frame, eos */
    if (G_UNLIKELY(src->rate_numerator == 0 && src->n_frames == 1)) {
        goto eos;
    }

    if (!getFrame(src->file, &data, &newsize)) {
        goto eos;
    }

    g_return_val_if_fail(newsize > 0, GST_FLOW_ERROR);

    GST_LOG_OBJECT(src,
                   "creating buffer of %lu bytes with %dx%d image for frame %d", newsize,
                   src->width, src->height, (gint) src->n_frames);


    outbuf = gst_buffer_new();

    if (!outbuf) {
        g_free(data);
        res = GST_FLOW_UNEXPECTED;
        goto no_buffer;
    }

    gst_buffer_set_caps(outbuf, GST_PAD_CAPS(GST_BASE_SRC_PAD(psrc)));
    gst_buffer_set_data(outbuf, data, newsize);

    GST_BUFFER_TIMESTAMP(outbuf) = src->timestamp_offset + src->running_time;
    GST_BUFFER_OFFSET(outbuf) = src->n_frames;
    src->n_frames++;
    GST_BUFFER_OFFSET_END(outbuf) = src->n_frames;

    if (src->rate_numerator) {
        next_time = gst_util_uint64_scale_int(src->n_frames * GST_SECOND,
                                              src->rate_denominator, src->rate_numerator);
        GST_BUFFER_DURATION(outbuf) = next_time - src->running_time;
    } else {
        next_time = src->timestamp_offset;
        /* NONE means forever */
        GST_BUFFER_DURATION(outbuf) = GST_CLOCK_TIME_NONE;
    }

    src->running_time = next_time;

    *buffer = outbuf;

    return GST_FLOW_OK;

eos: {
        GST_DEBUG_OBJECT(src, "eos: 0 framerate, frame %d", (gint) src->n_frames);
        return GST_FLOW_UNEXPECTED;
    }
no_buffer: {
        GST_DEBUG_OBJECT(src, "could not allocate buffer, reason %s",
                         gst_flow_get_name(res));
        return res;
    }
}

static gboolean
gst_avc_file_src_start(GstBaseSrc* basesrc) {
    GstAvcFileSrc* src = GST_AVC_FILE_SRC(basesrc);

    src->running_time = 0;
    src->n_frames = 0;

    return TRUE;
}

static gboolean
plugin_init(GstPlugin* plugin) {

    GST_DEBUG_CATEGORY_INIT(avc_file_src_debug, "avcfilesrc", 0,
                            "avc file Source");

    return gst_element_register(plugin, "avcfilesrc", GST_RANK_NONE,
                                GST_TYPE_AVC_FILE_SRC);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  "avcfilesrc",
                  "extract avc data from file",
                  plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
