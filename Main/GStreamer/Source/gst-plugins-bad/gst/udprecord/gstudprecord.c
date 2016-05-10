

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>

#include "gstudprecord.h"
#include <DemuxAPI/demuxer/avio.h>
#include <DemuxAPI/demuxer/demuxer.h>
#include "ThunkAPI.h"

#include <glib/gstdio.h>

#ifdef _MSC_VER
#include <Windows.h>
#include <assert.h>
#endif

GST_DEBUG_CATEGORY (udprecord_debug);
#define GST_CAT_DEFAULT udprecord_debug


static demuxer_t g_demuxer;

enum
{
  ARG_URI = 1,
  ARG_DEST_URI,
  ARG_TIMEOUT,
  ARG_DURATION
};
static void gst_udp_record_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_udp_record_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void udp_record_dispose (GObject * object);

static GstStateChangeReturn udp_record_change_state (GstElement * element,
    GstStateChange transition);

static GstFlowReturn udp_record_create(GstPushSrc *src, GstBuffer **buf);

static GstStaticPadTemplate gst_udp_record_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
                         GST_PAD_SRC,
                         GST_PAD_ALWAYS,
                         GST_STATIC_CAPS_ANY 
                         );



GST_BOILERPLATE (UdpRecord, udp_record, GstPushSrc, GST_TYPE_PUSH_SRC);

static void
udp_record_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
    gst_static_pad_template_get (&gst_udp_record_src_template));

  gst_element_class_set_details_simple (element_class,
      "Udp MPEG Transport Stream Recorder", "Generic/Source",
      "UDP Ts Recorder",
      "wangzhao@hansci.com");
}

static void
udp_record_class_init (UdpRecordClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  GstPushSrcClass * push_class = GST_PUSH_SRC_CLASS(klass);
  push_class->create = GST_DEBUG_FUNCPTR(udp_record_create);

  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_udp_record_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_udp_record_get_property);
  gobject_class->dispose = GST_DEBUG_FUNCPTR (udp_record_dispose);

  gstelement_class->change_state = GST_DEBUG_FUNCPTR(udp_record_change_state);

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_URI,
    g_param_spec_string ("uri", "udp address",
    "指定UDP的地址和端口","udp://localhost:9900",
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_TIMEOUT,
    g_param_spec_int("timeout", "time waiting for connection",
    "timeout ", 1000, 30000,5000,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_DEST_URI,
    g_param_spec_string("dest-uri", "dest file", 
    "收录目标文件", "rec.ts",
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_DURATION,
    g_param_spec_uint64("duration", "duration", "duration", 0, 0xffffffffffffffffULL, 0,
   G_PARAM_READABLE| G_PARAM_STATIC_STRINGS ));

  get_demuxer(&g_demuxer);

  g_demuxer.av_register_all();

}

static int udp_record_on_data(UdpRecord* mux, uint8_t* data, int size)
{
  guint bsize ;
  guint8* bdata;

  if( mux->playing)
  {
    if( !mux->dest_file)
    {
      mux->dest_file = g_fopen(mux->dest_uri, "wb");
    }

    if(!mux->dest_file)
      mux->last_on_data_error = 1;
    else
    {
      GstBuffer *buf = gst_buffer_new();
      GST_BUFFER_DATA(buf) = data;
      GST_BUFFER_SIZE(buf) = size;

      // 如果cache已经满了，去除cache中的数据，写入文件
      if( gst_data_cache_push(mux->datacache, buf) <= 0)
      {
        bsize = -1;
        if((bdata = gst_data_cache_lock(mux->datacache, &bsize )) != 0)
        {
          if(g_fwrite(bdata,bsize,1, mux->dest_file) != 1)
          {
            gst_data_cache_unlock(mux->datacache, bsize);
            mux->last_on_data_error = 1;
          }
          else
          {
            gst_data_cache_unlock(mux->datacache, bsize);

            // 如果cache容量太小 直接写入文件
            if( gst_data_cache_push(mux->datacache, buf) <= 0 )
            {
              if (g_fwrite (data, size, 1, mux->dest_file) != 1)
                mux->last_on_data_error = 1;
            }
          }

          //mux->last_on_data_error = !(size == g_fwrite(data, 1, size, mux->dest_file));
        }
      } 
      gst_buffer_unref(buf);
    }
  }

  return 0;
}
static int __cdecl udp_record_is_cancel(UdpRecord* mux)
{
  return mux->is_cancel;
}

static void
udp_record_init (UdpRecord * mux, UdpRecordClass * g_class)
{
  mux->timeout = 5000;
  mux->uri = 0;
  mux->dest_uri = 0;
  mux->playing = 0;
  mux->ffmpeg = 0;
  mux->last_on_data_error = 0;
  mux->dest_file = 0;
  mux->last_time = 0;
  mux->first_time = UINT64_MAX;
  mux->last_report = 0;

  gst_base_src_set_live(GST_BASE_SRC(mux), 1);

  mux->thunk = CreateThunk();

  AddThunkEntryA(mux->thunk, "cancel", mux, udp_record_is_cancel , Cdeclcall);
  AddThunkEntryA(mux->thunk, "data", mux, udp_record_on_data, Cdeclcall);

  g_cond_init(&mux->load_done);
  g_mutex_init(&mux->load_mutex);

  mux->is_load_done = 0;

  mux->datacache = gst_data_cache_new(64*1024);

}

static void
udp_record_dispose (GObject * object)
{
  UdpRecord *mux = GST_UDP_RECORD (object);

  CloseThunk(mux->thunk);

  g_cond_clear(&mux->load_done);
  g_mutex_clear(&mux->load_mutex);

  g_free(mux->uri);
  g_free(mux->dest_uri);

  mux->uri = 0;
  mux->dest_uri = 0;

  if(mux->dest_file)
  {
    g_fclose(mux->dest_file);
    mux->dest_file = 0;
  }

  gst_object_unref(mux->datacache);
  mux->datacache = 0;

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_udp_record_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  UdpRecord *mux = GST_UDP_RECORD (object);

  switch (prop_id) {
    case ARG_URI:
      {
        if( mux->uri )
        {
          g_free(mux->uri);
          mux->uri = 0;
        }
        mux->uri = g_strdup_printf("%s?reuse",g_value_get_string(value));
      }
      break;
    case ARG_TIMEOUT:
      {
        mux->timeout = g_value_get_int(value);
      }
      break;
    case ARG_DEST_URI:
      {
        if( mux->dest_uri )
        {
          g_free(mux->dest_uri);
          mux->dest_uri =0;
        }
        mux->dest_uri = g_value_dup_string(value);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_udp_record_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  UdpRecord *mux = GST_UDP_RECORD (object);

  switch (prop_id) {
    case ARG_TIMEOUT:
      g_value_set_int(value, mux->timeout);
      break;
    case ARG_URI:
      g_value_take_string(value, g_strdup(mux->uri));
      break;
    case ARG_DEST_URI:
      g_value_take_string(value, g_strdup(mux->dest_uri));
      break;
    case ARG_DURATION:
      g_value_set_uint64(value, mux->last_time - mux->first_time);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gpointer uriloadthrd(gpointer* d)
{
  UdpRecord* rec = (UdpRecord*)(d);
  GTimeVal time = {0};
  GTimeVal time2 = {0};
  
  g_get_current_time(&time);
  time2 = time;
  g_time_val_add(&time, (rec->timeout > 3000 ? rec->timeout: 3000) * 1000);

  while(1)
  {
    g_mutex_lock(&rec->load_mutex);
    
    if( rec->is_load_done || g_cond_wait_until(&rec->load_done, &rec->load_mutex, 0))
    {
      rec->is_cancel = 0;    
      g_mutex_unlock(&rec->load_mutex);
      return 0;
    }
    g_mutex_unlock(&rec->load_mutex);

    g_thread_yield();

    g_get_current_time(&time2);
    if( time2.tv_sec > time.tv_sec || (time2.tv_sec == time.tv_sec && time2.tv_usec > time.tv_usec))
    {
      rec->is_cancel = 1;
      break;
    }
  }
  return 0;
}

static int udp_record_cue(UdpRecord* rec)
{
  AVFormatParameters av;
  GThread* thrd = 0;
  int i = 0;

  memset(&av, 0, sizeof(AVFormatParameters));

  av.cancel = GetThunkEntryA(rec->thunk, "cancel");
  av.data = GetThunkEntryA(rec->thunk, "data");
  av.url_flags = URL_RDONLY;

  thrd = g_thread_new("uriload",uriloadthrd , rec);
  
  if( 0 == g_demuxer.av_open_input_file(&UDP_RECORD_AVFORMAT(rec), rec->uri, g_demuxer.av_find_input_format("mpegts"), 0, &av))
  {
    g_mutex_lock(&rec->load_mutex);
    g_cond_signal(&rec->load_done);
    rec->is_load_done= 1;
    g_mutex_unlock(&rec->load_mutex);
   
    g_thread_join(thrd);
    g_thread_unref(thrd);

    UDP_RECORD_AVFORMAT(rec)->flags |= AVFMT_FLAG_GENPTS;

    g_demuxer.av_find_stream_info(UDP_RECORD_AVFORMAT(rec));

    g_demuxer.av_read_frame_flush(UDP_RECORD_AVFORMAT(rec));

    for (i = 0 ; i < UDP_RECORD_AVFORMAT(rec)->nb_streams; i ++ )
    {
      if(UDP_RECORD_AVFORMAT(rec)->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO  )
      {
        rec->main_stream = i;
        break;
      }
    }
    return 1;
  }
  
  g_mutex_lock(&rec->load_mutex);
  g_cond_signal(&rec->load_done);
  rec->is_load_done= 1;
  g_mutex_unlock(&rec->load_mutex);

  g_thread_join(thrd);
  g_thread_unref(thrd);

  return 0;
}

static int udp_record_close(UdpRecord* rec)
{
  rec->is_cancel = 1;
  g_demuxer.av_close_input_file(UDP_RECORD_AVFORMAT(rec));
  rec->ffmpeg= 0;

  if( rec->dest_file )
  {
    g_fclose(rec->dest_file);
    rec->dest_file = 0;
  }
  rec->is_load_done = 0;
  rec->last_on_data_error = 0;

  gst_data_cache_clear(rec->datacache);

  return 0;
}

static void udp_record_report(UdpRecord* rec)
{
  guint64 report = rec->last_time - rec->first_time;
  guint64 diff =  report - rec->last_report;
  if( diff > GST_SECOND)
  {
    rec->last_report = report;
    g_object_notify(G_OBJECT(rec), "duration");
  }
}

static GstStateChangeReturn
udp_record_change_state (GstElement * element, GstStateChange transition)
{
  UdpRecord *mux = GST_UDP_RECORD (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  int playing = 0;  
  
  playing = (GST_STATE_TRANSITION_NEXT(transition) == GST_STATE_PLAYING);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:  
      {  
        mux->first_time = UINT64_MAX;
        mux->last_report = 0;
        mux->is_cancel = 0;
        mux->last_on_data_error = 0;
        if(!udp_record_cue(mux))
          return GST_STATE_CHANGE_FAILURE;
      }  
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      {
        mux->is_cancel = 1;
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);


  switch (transition) {
     case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
       {
         udp_record_close(mux);
       }
       break;
     default:
       break;
  }

  mux->playing = (ret == GST_STATE_CHANGE_SUCCESS) ? playing : 0;

  return ret;
}

GstFlowReturn udp_record_create(GstPushSrc *src, GstBuffer **buf)
{
  UdpRecord* rec = 0;
  AVPacket pack;
  guint64 tempts = 0;

  rec = GST_UDP_RECORD(src);

  if(rec->last_on_data_error)
    return GST_FLOW_ERROR;

  *buf = gst_buffer_new();

  g_demuxer.av_init_packet(&pack);
  if( UDP_RECORD_AVFORMAT(rec) && 0 == g_demuxer.av_read_frame(UDP_RECORD_AVFORMAT(rec), &pack))
  {
    if( pack.stream_index == rec->main_stream &&
      pack.pts != AV_NOPTS_VALUE )
    {
      // 对时间进行判断，只有当时间大于上次的时间，才更新，防止时长变短
      tempts = MPEGTIME_TO_GSTTIME(pack.pts);
      
      if( rec->last_time < tempts )
        rec->last_time = tempts;
      
      if( rec->first_time == UINT64_MAX )
        rec->first_time = rec->last_time;
      else
        udp_record_report(rec);
    }

    g_demuxer.av_destruct_packet(&pack);
  }
  else
  {
    if(UDP_RECORD_AVFORMAT(rec))
    {
      g_demuxer.av_close_input_file(UDP_RECORD_AVFORMAT(rec));
      rec->ffmpeg= 0;
      rec->is_load_done = 0;
      rec->last_on_data_error = 0;
      rec->is_cancel = 0;
    }

    udp_record_cue(rec);
  }
  return GST_FLOW_OK;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "udprecord", GST_RANK_PRIMARY,
          udp_record_get_type()))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (udprecord_debug, "udprecord", 0,
      "UDP MPEG Transport Stream Recorder");

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    "udprecord", "UDP MPEG Transport Stream Recorder",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
