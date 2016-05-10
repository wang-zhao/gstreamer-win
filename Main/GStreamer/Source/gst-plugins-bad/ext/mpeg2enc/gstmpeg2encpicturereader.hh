/* GStreamer mpeg2enc (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 * gstmpeg2encpicturereader.hh: GStreamer/mpeg2enc input wrapper
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

#ifndef __GST_MPEG2ENCPICTUREREADER_H__
#define __GST_MPEG2ENCPICTUREREADER_H__

#include <gst/gst.h>

#include <picturereader.hh>
#if GST_MJPEGTOOLS_API >= 10900
#include <imageplanes.hh>
#endif


class GstMpeg2EncPictureReader : public PictureReader {
public:
  GstMpeg2EncPictureReader (GstElement *element, GstCaps *caps,
      EncoderParams *params);
  ~GstMpeg2EncPictureReader ();

  /* get input picture parameters (width/height etc.) */
  void StreamPictureParams (MPEG2EncInVidParams &strm);

protected:
  /* read a frame */
#if GST_MJPEGTOOLS_API >= 10900
  bool LoadFrame (ImagePlanes &image);
#else
  bool LoadFrame ();
#endif

private:
  GstElement *element;
  GstCaps *caps;
};

#endif /* __GST_MPEG2ENCPICTUREREADER_H__ */
