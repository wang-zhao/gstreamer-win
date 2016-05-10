#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include <gst/gst.h>

#include "gstvdp/gstvdp.h"

#include "mpeg/gstvdpmpegdec.h"
#include "h264/gstvdph264dec.h"
#include "mpeg4/gstvdpmpeg4dec.h"
#include "gstvdpvideopostprocess.h"
#include "gstvdpsink.h"

static gboolean
vdpau_init (GstPlugin * vdpau_plugin)
{
  gst_vdp_init ();

  /* Before giving these elements a rank again, make sure they pass at
   * least the generic/states test when there's no device available */
  gst_element_register (vdpau_plugin, "vdpaumpegdec",
      GST_RANK_NONE, GST_TYPE_VDP_MPEG_DEC);
  gst_element_register (vdpau_plugin, "vdpauh264dec",
      GST_RANK_NONE, GST_TYPE_VDP_H264_DEC);
  gst_element_register (vdpau_plugin, "vdpaumpeg4dec",
      GST_RANK_NONE, GST_TYPE_VDP_MPEG4_DEC);
  gst_element_register (vdpau_plugin, "vdpauvideopostprocess",
      GST_RANK_NONE, GST_TYPE_VDP_VIDEO_POST_PROCESS);
  gst_element_register (vdpau_plugin, "vdpausink",
      GST_RANK_NONE, GST_TYPE_VDP_SINK);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "vdpau",
    "Various elements utilizing VDPAU",
    vdpau_init, VERSION, "LGPL", "GStreamer", "http://gstreamer.net/")
