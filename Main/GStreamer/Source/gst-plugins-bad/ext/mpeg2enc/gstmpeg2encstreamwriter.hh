/* GStreamer mpeg2enc (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * (c) 2006 Mark Nauwelaerts <manauw@skynet.be>
 *
 * gstmpeg2encstreamwriter.hh: GStreamer/mpeg2enc output wrapper
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

#ifndef __GST_MPEG2ENCSTREAMWRITER_H__
#define __GST_MPEG2ENCSTREAMWRITER_H__

#include <gst/gst.h>

#include <elemstrmwriter.hh>

#if GST_MJPEGTOOLS_API >= 10800

class GstMpeg2EncStreamWriter : public ElemStrmWriter {
  public:
  GstMpeg2EncStreamWriter (GstPad *pad, EncoderParams *params);
  ~GstMpeg2EncStreamWriter ();

  /* output functions */
  void WriteOutBufferUpto (const guint8 * buffer,
      const guint32 flush_upto);
  guint64 BitCount ();

  private:
  GstPad *pad;
  GstBuffer *buf;
};

#else

class GstMpeg2EncStreamWriter : public ElemStrmWriter {
public:
  GstMpeg2EncStreamWriter (GstPad *pad, EncoderParams *params);
  ~GstMpeg2EncStreamWriter ();

  /* output functions */
  void PutBits (guint32 val, gint n);
  void FrameBegin ();
  void FrameFlush ();
  void FrameDiscard ();

private:
  GstPad *pad;
  GstBuffer *buf;
};
#endif /* GST_MJPEGTOOLS_API >= 10800 */

#endif /* __GST_MPEG2ENCSTREAMWRITER_H__ */
