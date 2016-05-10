

#ifndef __GSTUDPRECORD_H__
#define __GSTUDPRECORD_H__

#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstpushsrc.h>
#include <gst/base/gstdatacache.h>
#include "ThunkAPI.h"



G_BEGIN_DECLS

#define GST_TYPE_UDP_RECORD  (udp_record_get_type())
#define GST_UDP_RECORD(obj)  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_UDP_RECORD, UdpRecord))

typedef struct _UdpRecord UdpRecord;
typedef struct _UdpRecordClass UdpRecordClass;

struct _UdpRecord {
  GstPushSrc parent;

  gchar* uri;
  int timeout;
  gchar* dest_uri;

  int playing;
  int is_cancel;

  void* ffmpeg;

  int main_stream;

  guint64 last_time;
  guint64 first_time;

  HTHUNK thunk;

  GCond load_done;
  int is_load_done;
  GMutex load_mutex;

  int last_on_data_error;

  FILE* dest_file;

  guint64 last_report;

  GstDataCache* datacache;
};

#define UDP_RECORD_AVFORMAT(__udp) ((AVFormatContext*)(__udp->ffmpeg))

struct _UdpRecordClass  {
  GstPushSrcClass parent_class;
};

GType udp_record_get_type (void);

#define CLOCK_BASE 9LL
#define CLOCK_FREQ (CLOCK_BASE * 10000)   /* 90 kHz PTS clock */
#define CLOCK_FREQ_SCR (CLOCK_FREQ * 300) /* 27 MHz SCR clock */

#define MPEGTIME_TO_GSTTIME(time) (gst_util_uint64_scale ((time), \
                        GST_MSECOND/10, CLOCK_BASE))
#define GSTTIME_TO_MPEGTIME(time) (gst_util_uint64_scale ((time), \
                        CLOCK_BASE, GST_MSECOND/10))

/* 27 MHz SCR conversions: */
#define MPEG_SYS_TIME_TO_GSTTIME(time) (gst_util_uint64_scale ((time), \
                        GST_USECOND, CLOCK_FREQ_SCR / 1000000))
#define GSTTIME_TO_MPEG_SYS_TIME(time) (gst_util_uint64_scale ((time), \
                        CLOCK_FREQ_SCR / 1000000, GST_USECOND))

#define NORMAL_TS_PACKET_LENGTH 188
#define M2TS_PACKET_LENGTH      192

#define MAX_PROG_NUMBER	32
#define DEFAULT_PROG_ID	0

G_END_DECLS

#endif //__GSTUDPRECORDBIN_H__
