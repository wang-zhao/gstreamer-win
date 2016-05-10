/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2003 Colin Walters <cwalters@gnome.org>
 *                    2005 Wim Taymans <wim@fluendo.com>
 *
 * gstdelay.c:
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

#include <gst/gst.h>
#include "gstdelay.h"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (delay_debug);
#define GST_CAT_DEFAULT (delay_debug)
GST_DEBUG_CATEGORY_STATIC (delay_dataflow);

#define STATUS(delay, pad, msg) \
  GST_CAT_LOG_OBJECT (delay_dataflow, delay, \
                      "(%s:%s) " msg ": %" G_GUINT64_FORMAT " of %" G_GUINT64_FORMAT " ns, %u items", \
                      GST_DEBUG_PAD_NAME (pad), \
                      delay->current_level_time, \
                      delay->delay_time, \
                      delay->queue->length)

/* Queue signals and args */
enum
{
  SIGNAL_UNDERRUN,
  SIGNAL_RUNNING,
  SIGNAL_OVERRUN,
  SIGNAL_PUSHING,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_CUR_LEVEL_TIME,
  PROP_DELAY_TIME,
  PROP_SILENT
};

typedef struct _QueueItem
{
  gpointer data;
  GstClockTime input_time;
  gint64 offset;
};

typedef struct _QueueItem QueueItem;


#define GST_DELAY_SIGNAL(_d_) SetEvent((_d_)->render_event)

#define GST_DELAY_WAIT(_d_, timeout) (WaitForSingleObject((_d_)->render_event, timeout) == WAIT_OBJECT_0)

#define GST_DELAY_RESET(_d_) ResetEvent((_d_)->render_event)

/* default property values */
#define DEFAULT_DELAY_TIME     (30*GST_SECOND)    /* 1 second    */

#define GST_DELAY_MUTEX_LOCK(d) G_STMT_START {                          \
  g_mutex_lock (d->qlock);                                              \
} G_STMT_END

#define GST_DELAY_MUTEX_LOCK_CHECK(d,label) G_STMT_START {              \
  GST_DELAY_MUTEX_LOCK (d);                                             \
  if (d->srcresult != GST_FLOW_OK)                                      \
    goto label;                                                         \
} G_STMT_END

#define GST_DELAY_MUTEX_UNLOCK(d) G_STMT_START {                        \
  g_mutex_unlock (d->qlock);                                            \
} G_STMT_END

#define GST_DELAY_WAIT_DEL_CHECK(d, label) G_STMT_START {               \
  STATUS (d, d->sinkpad, "wait for DEL");                               \
  d->waiting_del = TRUE;                                                \
  g_cond_wait (d->item_del, d->qlock);                                  \
  d->waiting_del = FALSE;                                               \
  if (d->srcresult != GST_FLOW_OK) {                                    \
    STATUS (d, d->srcpad, "received DEL wakeup");                       \
    goto label;                                                         \
  }                                                                     \
  STATUS (d, d->sinkpad, "received DEL");                               \
} G_STMT_END

#define GST_DELAY_WAIT_ADD_CHECK(d, label) G_STMT_START {               \
  STATUS (d, d->srcpad, "wait for ADD");                                \
  d->waiting_add = TRUE;                                                \
  g_cond_wait (d->item_add, d->qlock);                                  \
  d->waiting_add = FALSE;                                               \
  if (d->srcresult != GST_FLOW_OK) {                                    \
    STATUS (d, d->srcpad, "received ADD wakeup");                       \
    goto label;                                                         \
  }                                                                     \
  STATUS (d, d->srcpad, "received ADD");                                \
} G_STMT_END

#define GST_DELAY_SIGNAL_DEL(d) G_STMT_START {                          \
  if (d->waiting_del) {                                                 \
    STATUS (d, d->srcpad, "signal DEL");                                \
    g_cond_signal (d->item_del);                                        \
  }                                                                     \
} G_STMT_END

#define GST_DELAY_SIGNAL_ADD(d) G_STMT_START {                          \
  if (d->waiting_add) {                                                 \
    STATUS (d, d->sinkpad, "signal ADD");                               \
    g_cond_signal (d->item_add);                                        \
  }                                                                     \
} G_STMT_END

#define _do_init(bla) \
    GST_DEBUG_CATEGORY_INIT (delay_debug, "delay", 0, "delay element"); \
    GST_DEBUG_CATEGORY_INIT (delay_dataflow, "delay_dataflow", 0, \
        "dataflow inside the delay element");

GST_BOILERPLATE_FULL (GstDelay, gst_delay, GstElement,
    GST_TYPE_ELEMENT, _do_init);

static void gst_delay_finalize (GObject * object);

static void gst_delay_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_delay_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_delay_chain (GstPad * pad, GstBuffer * buffer);
static GstFlowReturn gst_delay_bufferalloc (GstPad * pad, guint64 offset,
    guint size, GstCaps * caps, GstBuffer ** buf);
static GstFlowReturn gst_delay_push_one (GstDelay * delay);
static void gst_delay_loop (GstPad * pad);

static gboolean gst_delay_handle_sink_event (GstPad * pad, GstEvent * event);

static gboolean gst_delay_handle_src_event (GstPad * pad, GstEvent * event);
static gboolean gst_delay_handle_src_query (GstPad * pad, GstQuery * query);

static gboolean gst_delay_acceptcaps (GstPad * pad, GstCaps * caps);
static GstCaps *gst_delay_getcaps (GstPad * pad);
static GstPadLinkReturn gst_delay_link_sink (GstPad * pad, GstPad * peer);
static GstPadLinkReturn gst_delay_link_src (GstPad * pad, GstPad * peer);
static void gst_delay_locked_flush (GstDelay * delay);

static gboolean gst_delay_src_activate_push (GstPad * pad, gboolean active);
static gboolean gst_delay_sink_activate_push (GstPad * pad, gboolean active);

static gboolean gst_delay_is_empty (GstDelay * delay);
static gboolean gst_delay_is_filled (GstDelay * delay);



static guint gst_delay_signals[LAST_SIGNAL] = { 0 };

static void
gst_delay_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstelement_class,
      "Delay",
      "Generic", "Simple Delay Implement", "transparent");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));
  
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));
}

static void
gst_delay_class_init (GstDelayClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_delay_set_property;
  gobject_class->get_property = gst_delay_get_property;

  /* signals */
  /**
   * GstDelay::underrun:
   * @delay: the delay instance
   *
   * Reports that the buffer became empty (underrun).
   * A buffer is empty if the total amount of data inside it (num-buffers, time,
   * size) is lower than the boundary values which can be set through the
   * GObject properties.
   */
  gst_delay_signals[SIGNAL_UNDERRUN] =
      g_signal_new ("underrun", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
      G_STRUCT_OFFSET (GstDelayClass, underrun), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  /**
   * GstDelay::running:
   * @delay: the delay instance
   *
   * Reports that enough (min-threshold) data is in the delay. Use this signal
   * together with the underrun signal to pause the pipeline on underrun and
   * wait for the delay to fill-up before resume playback.
   */
  gst_delay_signals[SIGNAL_RUNNING] =
      g_signal_new ("running", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
      G_STRUCT_OFFSET (GstDelayClass, running), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  /**
   * GstDelay::overrun:
   * @delay: the delay instance
   *
   * Reports that the buffer became full (overrun).
   * A buffer is full if the total amount of data inside it (num-buffers, time,
   * size) is higher than the boundary values which can be set through the
   * GObject properties.
   */
  gst_delay_signals[SIGNAL_OVERRUN] =
      g_signal_new ("overrun", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
      G_STRUCT_OFFSET (GstDelayClass, overrun), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  /**
   * GstDelay::pushing:
   * @delay: the delay instance
   *
   * Reports when the delay has enough data to start pushing data again on the
   * source pad.
   */
  gst_delay_signals[SIGNAL_PUSHING] =
      g_signal_new ("pushing", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
      G_STRUCT_OFFSET (GstDelayClass, pushing), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

 
  g_object_class_install_property (gobject_class, PROP_CUR_LEVEL_TIME,
      g_param_spec_uint64 ("current-level-time", "Current level (ns)",
          "Current amount of data in the delay (in ns)",
          0, G_MAXUINT64, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));


  g_object_class_install_property (gobject_class, PROP_DELAY_TIME,
      g_param_spec_uint64 ("delay-time", "delay time",
          "delay time for each buffer (in ns)",
          0, G_MAXUINT64, DEFAULT_DELAY_TIME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstDelay:silent
   *
   * Don't emit delay signals. Makes delays more lightweight if no signals are
   * needed.
   */
  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent",
          "Don't emit delay signals", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gobject_class->finalize = gst_delay_finalize;

  /* Registering debug symbols for function pointers */
  GST_DEBUG_REGISTER_FUNCPTR (gst_delay_chain);
  GST_DEBUG_REGISTER_FUNCPTR (gst_delay_sink_activate_push);
  GST_DEBUG_REGISTER_FUNCPTR (gst_delay_handle_sink_event);
  GST_DEBUG_REGISTER_FUNCPTR (gst_delay_link_sink);
  GST_DEBUG_REGISTER_FUNCPTR (gst_delay_getcaps);
  GST_DEBUG_REGISTER_FUNCPTR (gst_delay_acceptcaps);
  GST_DEBUG_REGISTER_FUNCPTR (gst_delay_bufferalloc);
  GST_DEBUG_REGISTER_FUNCPTR (gst_delay_src_activate_push);
  GST_DEBUG_REGISTER_FUNCPTR (gst_delay_link_src);
  GST_DEBUG_REGISTER_FUNCPTR (gst_delay_handle_src_event);
  GST_DEBUG_REGISTER_FUNCPTR (gst_delay_handle_src_query);
}

static void
gst_delay_init (GstDelay * delay, GstDelayClass * g_class)
{
  delay->sinkpad = gst_pad_new_from_static_template (&sinktemplate, "sink");

  gst_pad_set_chain_function (delay->sinkpad, gst_delay_chain);
  gst_pad_set_activatepush_function (delay->sinkpad,
      gst_delay_sink_activate_push);
  gst_pad_set_event_function (delay->sinkpad, gst_delay_handle_sink_event);
  gst_pad_set_link_function (delay->sinkpad, gst_delay_link_sink);
  gst_pad_set_getcaps_function (delay->sinkpad, gst_delay_getcaps);
  gst_pad_set_acceptcaps_function (delay->sinkpad, gst_delay_acceptcaps);
  gst_pad_set_bufferalloc_function (delay->sinkpad, gst_delay_bufferalloc);
  gst_element_add_pad (GST_ELEMENT (delay), delay->sinkpad);

  delay->srcpad = gst_pad_new_from_static_template (&srctemplate, "src");

  gst_pad_set_activatepush_function (delay->srcpad,
      gst_delay_src_activate_push);
  gst_pad_set_link_function (delay->srcpad, gst_delay_link_src);
  gst_pad_set_acceptcaps_function (delay->srcpad, gst_delay_acceptcaps);
  gst_pad_set_getcaps_function (delay->srcpad, gst_delay_getcaps);
  gst_pad_set_event_function (delay->srcpad, gst_delay_handle_src_event);
  gst_pad_set_query_function (delay->srcpad, gst_delay_handle_src_query);
  gst_element_add_pad (GST_ELEMENT (delay), delay->srcpad);

  gst_segment_init (&delay->sink_segment, GST_FORMAT_TIME);
  gst_segment_init (&delay->src_segment, GST_FORMAT_TIME);
  delay->head_needs_discont = delay->tail_needs_discont = FALSE;

  delay->srcresult = GST_FLOW_WRONG_STATE;

  delay->qlock = g_mutex_new ();
  delay->item_add = g_cond_new ();
  delay->item_del = g_cond_new ();
  delay->queue = g_queue_new ();

  delay->sinktime = GST_CLOCK_TIME_NONE;
  delay->srctime = GST_CLOCK_TIME_NONE;

  delay->sink_tainted = TRUE;
  delay->src_tainted = TRUE;

  delay->newseg_applied_to_src = FALSE;

  delay->disk_cache = gst_disk_cache_new();
  delay->sys_clock = gst_system_clock_obtain();

  delay->render_event = CreateEvent(NULL, FALSE, FALSE, NULL);
  delay->delay_time = DEFAULT_DELAY_TIME;

  GST_DEBUG_OBJECT (delay,
      "initialized delay's not_empty & not_full conditions");
}

/* called only once, as opposed to dispose */
static void
gst_delay_finalize (GObject * object)
{
  GstDelay *delay = GST_DELAY (object);

  GST_DEBUG_OBJECT (delay, "finalizing delay");

  while (!g_queue_is_empty (delay->queue)) {
    QueueItem *qi= g_queue_pop_head (delay->queue);

    gst_mini_object_unref ((GstMiniObject*)(qi->data));
    g_free(qi);
  }
  g_queue_free (delay->queue);
  g_mutex_free (delay->qlock);
  g_cond_free (delay->item_add);
  g_cond_free (delay->item_del);
  gst_disk_cache_free(delay->disk_cache);
  gst_object_unref(delay->sys_clock);
  CloseHandle(delay->render_event);
  

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_delay_acceptcaps (GstPad * pad, GstCaps * caps)
{
  gboolean result;
  GstDelay *delay;
  GstPad *otherpad;

  delay = GST_DELAY (gst_pad_get_parent (pad));
  if (G_UNLIKELY (delay == NULL))
    return FALSE;

  otherpad = (pad == delay->srcpad ? delay->sinkpad : delay->srcpad);
  result = gst_pad_peer_accept_caps (otherpad, caps);

  gst_object_unref (delay);

  return result;
}

static GstCaps *
gst_delay_getcaps (GstPad * pad)
{
  GstDelay *delay;
  GstPad *otherpad;
  GstCaps *result;

  delay = GST_DELAY (gst_pad_get_parent (pad));
  if (G_UNLIKELY (delay == NULL))
    return gst_caps_new_any ();

  otherpad = (pad == delay->srcpad ? delay->sinkpad : delay->srcpad);
  result = gst_pad_peer_get_caps (otherpad);
  if (result == NULL)
    result = gst_caps_new_any ();

  gst_object_unref (delay);

  return result;
}

static GstPadLinkReturn
gst_delay_link_sink (GstPad * pad, GstPad * peer)
{
  return GST_PAD_LINK_OK;
}

static GstPadLinkReturn
gst_delay_link_src (GstPad * pad, GstPad * peer)
{
  GstPadLinkReturn result = GST_PAD_LINK_OK;
  GstDelay *delay;

  delay = GST_DELAY (gst_pad_get_parent (pad));

  GST_DEBUG_OBJECT (delay, "delay linking source pad");

  if (GST_PAD_LINKFUNC (peer)) {
    result = GST_PAD_LINKFUNC (peer) (peer, pad);
  }

  if (GST_PAD_LINK_SUCCESSFUL (result)) {
    GST_DELAY_MUTEX_LOCK (delay);
    if (delay->srcresult == GST_FLOW_OK) {
      delay->push_newsegment = TRUE;
      gst_pad_start_task (pad, (GstTaskFunction) gst_delay_loop, pad);
      GST_DEBUG_OBJECT (delay, "starting task as pad is linked");
    } else {
      GST_DEBUG_OBJECT (delay, "not starting task reason %s",
          gst_flow_get_name (delay->srcresult));
    }
    GST_DELAY_MUTEX_UNLOCK (delay);
  }
  gst_object_unref (delay);

  return result;
}

static GstFlowReturn
gst_delay_bufferalloc (GstPad * pad, guint64 offset, guint size, GstCaps * caps,
    GstBuffer ** buf)
{
  GstDelay *delay;
  GstFlowReturn result;

  delay = GST_DELAY (gst_pad_get_parent (pad));
  if (G_UNLIKELY (delay == NULL))
    return GST_FLOW_WRONG_STATE;

  /* Forward to src pad, without setting caps on the src pad */
  result = gst_pad_alloc_buffer (delay->srcpad, offset, size, caps, buf);

  gst_object_unref (delay);
  return result;
}

/* calculate the diff between running time on the sink and src of the delay.
 * This is the total amount of time in the delay. */
static void
update_time_level (GstDelay * delay)
{
  gint64 sink_time, src_time;

  if (delay->sink_tainted) {
    delay->sinktime =
        gst_segment_to_running_time (&delay->sink_segment, GST_FORMAT_TIME,
        delay->sink_segment.last_stop);
    delay->sink_tainted = FALSE;
  }
  sink_time = delay->sinktime;

  if (delay->src_tainted) {
    delay->srctime =
        gst_segment_to_running_time (&delay->src_segment, GST_FORMAT_TIME,
        delay->src_segment.last_stop);
    delay->src_tainted = FALSE;
  }
  src_time = delay->srctime;

  GST_LOG_OBJECT (delay, "sink %" GST_TIME_FORMAT ", src %" GST_TIME_FORMAT,
      GST_TIME_ARGS (sink_time), GST_TIME_ARGS (src_time));

  if (sink_time >= src_time)
    delay->current_level_time = sink_time - src_time;
  else
    delay->current_level_time= 0;
}

/* take a NEWSEGMENT event and apply the values to segment, updating the time
 * level of delay. */
static void
apply_segment (GstDelay * delay, GstEvent * event, GstSegment * segment,
    gboolean sink)
{
  gboolean update;
  GstFormat format;
  gdouble rate, arate;
  gint64 start, stop, time;

  gst_event_parse_new_segment_full (event, &update, &rate, &arate,
      &format, &start, &stop, &time);

  /* now configure the values, we use these to track timestamps on the
   * sinkpad. */
  if (format != GST_FORMAT_TIME) {
    /* non-time format, pretent the current time segment is closed with a
     * 0 start and unknown stop time. */
    update = FALSE;
    format = GST_FORMAT_TIME;
    start = 0;
    stop = -1;
    time = 0;
  }
  gst_segment_set_newsegment_full (segment, update,
      rate, arate, format, start, stop, time);

  if (sink)
    delay->sink_tainted = TRUE;
  else
    delay->src_tainted = TRUE;

  GST_DEBUG_OBJECT (delay,
      "configured NEWSEGMENT %" GST_SEGMENT_FORMAT, segment);

  /* segment can update the time level of the delay */
  update_time_level (delay);
}

/* take a buffer and update segment, updating the time level of the delay. */
static void
apply_buffer (GstDelay * delay, GstBuffer * buffer, GstSegment * segment,
    gboolean with_duration, gboolean sink)
{
  GstClockTime duration, timestamp;

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  duration = GST_BUFFER_DURATION (buffer);

  /* if no timestamp is set, assume it's continuous with the previous
   * time */
  if (timestamp == GST_CLOCK_TIME_NONE)
    timestamp = segment->last_stop;

  /* add duration */
  if (with_duration && duration != GST_CLOCK_TIME_NONE)
    timestamp += duration;

  GST_LOG_OBJECT (delay, "last_stop updated to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  gst_segment_set_last_stop (segment, GST_FORMAT_TIME, timestamp);
  if (sink)
    delay->sink_tainted = TRUE;
  else
    delay->src_tainted = TRUE;


  /* calc diff with other end */
  update_time_level (delay);
}

static void
gst_delay_locked_flush (GstDelay * delay)
{
  while (!g_queue_is_empty (delay->queue)) {
    QueueItem *qi= g_queue_pop_head (delay->queue);

    gst_mini_object_unref ((GstMiniObject*)(qi->data));
    g_free(qi);
  }
  delay->current_level_time = 0;
  gst_segment_init (&delay->sink_segment, GST_FORMAT_TIME);
  gst_segment_init (&delay->src_segment, GST_FORMAT_TIME);
  delay->head_needs_discont = delay->tail_needs_discont = FALSE;

  delay->sinktime = delay->srctime = GST_CLOCK_TIME_NONE;
  delay->sink_tainted = delay->src_tainted = TRUE;

  /* we deleted a lot of something */
  GST_DELAY_SIGNAL_DEL (delay);
}

/* enqueue an item an update the level stats, with QUEUE_LOCK */
static inline GstFlowReturn
gst_delay_locked_enqueue_buffer (GstDelay * delay, GstBuffer* item)
{
  GstBuffer *buffer = GST_BUFFER_CAST (item);
  QueueItem* qi = g_malloc0(sizeof(QueueItem));
  GstBuffer *newbuf = gst_buffer_new();

  qi->input_time = gst_clock_get_time(delay->sys_clock);
  gst_buffer_copy_metadata (newbuf, buffer, GST_BUFFER_COPY_ALL);
  /* add buffer to the statistics */
  apply_buffer (delay, buffer, &delay->sink_segment, TRUE, TRUE);

  qi->offset = gst_disk_cache_add_data(delay->disk_cache, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
  //newbuf->data = (guint8*)gst_disk_cache_add_data(delay->disk_cache, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
  newbuf->size = buffer->size;
  newbuf->malloc_data = 0;

  gst_buffer_unref(buffer);
  
  qi->data = newbuf;
  if( newbuf->data != ((guint8*)(-1)))
  {
    g_queue_push_tail (delay->queue, qi);
    GST_DELAY_SIGNAL_ADD (delay);
    return GST_FLOW_OK;
  }
  else
  {
    gst_buffer_unref(newbuf);
    g_free(qi);
    return GST_FLOW_UNEXPECTED;
  }
}

static inline void
gst_delay_locked_enqueue_event (GstDelay * delay, gpointer item)
{
  GstEvent *event = GST_EVENT_CAST (item);
  QueueItem* qi = g_malloc0(sizeof(QueueItem));

  qi->input_time = gst_clock_get_time(delay->sys_clock);
  qi->data =item;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      /* mark the delay as EOS. This prevents us from accepting more data. */
      GST_CAT_INFO_OBJECT (delay_dataflow, delay, "got EOS from upstream");
      delay->eos = TRUE;
      break;
    case GST_EVENT_NEWSEGMENT:
      apply_segment (delay, event, &delay->sink_segment, TRUE);
      /* if the delay is empty, apply sink segment on the source */
      if (delay->queue->length == 0) {
        GST_CAT_LOG_OBJECT (delay_dataflow, delay, "Apply segment on srcpad");
        apply_segment (delay, event, &delay->src_segment, FALSE);
        delay->newseg_applied_to_src = TRUE;
      }
      /* a new segment allows us to accept more buffers if we got UNEXPECTED
       * from downstream */
      delay->unexpected = FALSE;
      break;
    default:
      break;
  }


  g_queue_push_tail (delay->queue, qi);
  GST_DELAY_SIGNAL_ADD (delay);
}

/* dequeue an item from the delay and update level stats, with QUEUE_LOCK */
static QueueItem *
gst_delay_locked_dequeue (GstDelay * delay, gboolean * is_buffer)
{
  QueueItem *item;

  item = g_queue_pop_head (delay->queue);
  if (item == NULL)
    goto no_item;

  if (GST_IS_BUFFER (item->data)) {
    GstBuffer *buffer = GST_BUFFER_CAST (item->data);

    GST_CAT_LOG_OBJECT (delay_dataflow, delay,
        "retrieved buffer %p from delay", buffer);

    apply_buffer (delay, buffer, &delay->src_segment, TRUE, FALSE);

    /* if the delay is empty now, update the other side */
    if (delay->queue->length == 0)
      delay->current_level_time = 0;

    *is_buffer = TRUE;
  } else if (GST_IS_EVENT (item->data)) {
    GstEvent *event = GST_EVENT_CAST (item->data);

    GST_CAT_LOG_OBJECT (delay_dataflow, delay,
        "retrieved event %p from delay", event);

    switch (GST_EVENT_TYPE (event)) {
      case GST_EVENT_EOS:
        /* queue is empty now that we dequeued the EOS */
        delay->current_level_time = 0;
        break;
      case GST_EVENT_NEWSEGMENT:
        /* apply newsegment if it has not already been applied */
        if (G_LIKELY (!delay->newseg_applied_to_src)) {
          apply_segment (delay, event, &delay->src_segment, FALSE);
        } else {
          delay->newseg_applied_to_src = FALSE;
        }
        break;
      default:
        break;
    }

    *is_buffer = FALSE;
  } else {
    g_warning
        ("Unexpected item %p dequeued from delay %s (refcounting problem?)",
        item, GST_OBJECT_NAME (delay));
    item = NULL;
  }
  GST_DELAY_SIGNAL_DEL (delay);

  return item;

  /* ERRORS */
no_item:
  {
    GST_CAT_DEBUG_OBJECT (delay_dataflow, delay, "the delay is empty");
    return NULL;
  }
}

static gboolean
gst_delay_handle_sink_event (GstPad * pad, GstEvent * event)
{
  GstDelay *delay;

  delay = GST_DELAY (gst_pad_get_parent (pad));
  if (G_UNLIKELY (delay == NULL)) {
    gst_event_unref (event);
    return FALSE;
  }

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
    {
      STATUS (delay, pad, "received flush start event");
      /* forward event */
      gst_pad_push_event (delay->srcpad, event);

      /* now unblock the chain function */
      GST_DELAY_MUTEX_LOCK (delay);
      delay->srcresult = GST_FLOW_WRONG_STATE;
      /* unblock the loop and chain functions */
      GST_DELAY_SIGNAL_ADD (delay);
      GST_DELAY_SIGNAL_DEL (delay);
      GST_DELAY_MUTEX_UNLOCK (delay);

      /* make sure it pauses, this should happen since we sent
       * flush_start downstream. */
      gst_pad_pause_task (delay->srcpad);
      GST_CAT_LOG_OBJECT (delay_dataflow, delay, "loop stopped");
      goto done;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      STATUS (delay, pad, "received flush stop event");
      /* forward event */
      gst_pad_push_event (delay->srcpad, event);

      GST_DELAY_MUTEX_LOCK (delay);
      gst_delay_locked_flush (delay);
      delay->srcresult = GST_FLOW_OK;
      delay->eos = FALSE;
      delay->unexpected = FALSE;
      if (gst_pad_is_linked (delay->srcpad)) {
        gst_pad_start_task (delay->srcpad, (GstTaskFunction) gst_delay_loop,
            delay->srcpad);
      } else {
        GST_INFO_OBJECT (delay, "not re-starting task as pad is not linked");
      }
      GST_DELAY_MUTEX_UNLOCK (delay);

      STATUS (delay, pad, "after flush");
      goto done;
    }
    default:
      if (GST_EVENT_IS_SERIALIZED (event)) {
        /* serialized events go in the delay */
        GST_DELAY_MUTEX_LOCK_CHECK (delay, out_flushing);
        /* refuse more events on EOS */
        if (delay->eos)
          goto out_eos;
        gst_delay_locked_enqueue_event (delay, event);
        GST_DELAY_MUTEX_UNLOCK (delay);
      } else {
        /* non-serialized events are passed upstream. */
        gst_pad_push_event (delay->srcpad, event);
      }
      break;
  }
done:
  gst_object_unref (delay);
  return TRUE;

  /* ERRORS */
out_flushing:
  {
    GST_CAT_LOG_OBJECT (delay_dataflow, delay,
        "refusing event, we are flushing");
    GST_DELAY_MUTEX_UNLOCK (delay);
    gst_object_unref (delay);
    gst_event_unref (event);
    return FALSE;
  }
out_eos:
  {
    GST_CAT_LOG_OBJECT (delay_dataflow, delay, "refusing event, we are EOS");
    GST_DELAY_MUTEX_UNLOCK (delay);
    gst_object_unref (delay);
    gst_event_unref (event);
    return FALSE;
  }
}

static gboolean
gst_delay_is_empty (GstDelay * delay)
{
  if (delay->queue->length == 0)
    return TRUE;
  return FALSE;
}

static gboolean
gst_delay_is_filled (GstDelay * delay)
{
  return FALSE;//delay->current_level_time >= delay->delay_time;
}

static void
gst_delay_leak_downstream (GstDelay * delay)
{
  /* for as long as the delay is filled, dequeue an item and discard it */
  while (gst_delay_is_filled (delay)) {
    QueueItem *leak;
    gboolean is_buffer;

    leak = gst_delay_locked_dequeue (delay, &is_buffer);
    /* there is nothing to dequeue and the delay is still filled.. This should
     * not happen */
    g_assert (leak != NULL);

    GST_CAT_DEBUG_OBJECT (delay_dataflow, delay,
        "delay is full, leaking item %p on downstream end", leak);
    gst_mini_object_unref ((GstMiniObject*)(leak->data));
    g_free(leak);

    /* last buffer needs to get a DISCONT flag */
    delay->head_needs_discont = TRUE;
  }
}

static GstFlowReturn
gst_delay_chain (GstPad * pad, GstBuffer * buffer)
{
  GstDelay *delay;
  GstClockTime duration, timestamp;
  GstFlowReturn ret;


  ret = GST_FLOW_OK;
  delay = (GstDelay *) GST_OBJECT_PARENT (pad);

  /* we have to lock the delay since we span threads */
  GST_DELAY_MUTEX_LOCK_CHECK (delay, out_flushing);
  /* when we received EOS, we refuse any more data */
  if (delay->eos)
    goto out_eos;
  if (delay->unexpected)
    goto out_unexpected;

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  duration = GST_BUFFER_DURATION (buffer);

  GST_CAT_LOG_OBJECT (delay_dataflow, delay,
      "received buffer %p of size %d, time %" GST_TIME_FORMAT ", duration %"
      GST_TIME_FORMAT, buffer, GST_BUFFER_SIZE (buffer),
      GST_TIME_ARGS (timestamp), GST_TIME_ARGS (duration));

  /* We make space available if we're "full" according to whatever
   * the user defined as "full". Note that this only applies to buffers.
   * We always handle events and they don't count in our statistics. */
  while (gst_delay_is_filled (delay)) {
    if (!delay->silent) {
      GST_DELAY_MUTEX_UNLOCK (delay);
      g_signal_emit (delay, gst_delay_signals[SIGNAL_OVERRUN], 0);
      GST_DELAY_MUTEX_LOCK_CHECK (delay, out_flushing);
      /* we recheck, the signal could have changed the thresholds */
      if (!gst_delay_is_filled (delay))
        break;
    }


    GST_CAT_DEBUG_OBJECT (delay_dataflow, delay,
      "delay is full, waiting for free space");

    /* don't leak. Instead, wait for space to be available */
    do {
      /* for as long as the delay is filled, wait till an item was deleted. */
      GST_DELAY_WAIT_DEL_CHECK (delay, out_flushing);
    } while (gst_delay_is_filled (delay));

    GST_CAT_DEBUG_OBJECT (delay_dataflow, delay, "delay is not full");

    if (!delay->silent) {
      GST_DELAY_MUTEX_UNLOCK (delay);
      g_signal_emit (delay, gst_delay_signals[SIGNAL_RUNNING], 0);
      GST_DELAY_MUTEX_LOCK_CHECK (delay, out_flushing);
    }
  }


  if (delay->tail_needs_discont) {
    GstBuffer *subbuffer = gst_buffer_make_metadata_writable (buffer);

    if (subbuffer) {
      buffer = subbuffer;
      GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
    } else {
      GST_DEBUG_OBJECT (delay, "Could not mark buffer as DISCONT");
    }
    delay->tail_needs_discont = FALSE;
  }

  /* put buffer in delay now */
  ret = gst_delay_locked_enqueue_buffer (delay, buffer);
  GST_DELAY_MUTEX_UNLOCK (delay);

  return ret;

out_flushing:
  {
    GstFlowReturn ret = delay->srcresult;

    GST_CAT_LOG_OBJECT (delay_dataflow, delay,
        "exit because task paused, reason: %s", gst_flow_get_name (ret));
    GST_DELAY_MUTEX_UNLOCK (delay);
    gst_buffer_unref (buffer);

    return ret;
  }
out_eos:
  {
    GST_CAT_LOG_OBJECT (delay_dataflow, delay, "exit because we received EOS");
    GST_DELAY_MUTEX_UNLOCK (delay);

    gst_buffer_unref (buffer);

    return GST_FLOW_UNEXPECTED;
  }
out_unexpected:
  {
    GST_CAT_LOG_OBJECT (delay_dataflow, delay,
        "exit because we received UNEXPECTED");
    GST_DELAY_MUTEX_UNLOCK (delay);

    gst_buffer_unref (buffer);

    return GST_FLOW_UNEXPECTED;
  }
}

static void
gst_delay_push_newsegment (GstDelay * delay)
{
  GstSegment *s;
  GstEvent *event;

  s = &delay->src_segment;

  if (s->accum != 0) {
    event = gst_event_new_new_segment_full (FALSE, 1.0, 1.0, s->format, 0,
        s->accum, 0);
    GST_CAT_LOG_OBJECT (delay_dataflow, delay,
        "pushing accum newsegment event");
    gst_pad_push_event (delay->srcpad, event);
  }

  event = gst_event_new_new_segment_full (FALSE, s->rate, s->applied_rate,
      s->format, s->start, s->stop, s->time);
  GST_CAT_LOG_OBJECT (delay_dataflow, delay, "pushing real newsegment event");
  gst_pad_push_event (delay->srcpad, event);
}
static gboolean gst_delay_load_data(GstDelay* delay, GstBuffer* buffer, gint64 o){
  buffer->malloc_data = g_malloc (buffer->size);
  if( !buffer->malloc_data )
    return FALSE;

  if(!gst_disk_cache_remove_data(delay->disk_cache, buffer->malloc_data, o, buffer->size))
  {
    g_free(buffer->malloc_data);
    buffer->malloc_data = 0;
    return FALSE;
  }

  GST_BUFFER_DATA( buffer) = buffer->malloc_data;
  //GST_BUFFER_TIMESTAMP(buffer) += delay->delay_time;
  return TRUE;
}

gboolean	gst_delay_clock_callback(GstClock *clock, GstClockTime time,
						 GstClockID id, gpointer user_data)
{
  GstDelay* delay;

  delay = GST_DELAY(user_data);
  GST_DELAY_SIGNAL(delay);
  return TRUE;
}

static gboolean gst_delay_wait_delay(GstDelay* delay, QueueItem* data)
{
  GstClockTime render_time;
  GstClockID render_id;
  GstClockReturn ret;

  render_time = data->input_time + delay->delay_time;

  render_id = gst_clock_new_single_shot_id(delay->sys_clock, render_time);

  GST_DELAY_RESET(delay);

  if(GST_CLOCK_OK == gst_clock_id_wait_async(render_id, gst_delay_clock_callback, delay))
  {
    while(TRUE){
      GST_DELAY_MUTEX_UNLOCK(delay);
      if(GST_DELAY_WAIT(delay, 100))
      {
        GST_DELAY_MUTEX_LOCK(delay);
        ret = GST_CLOCK_OK;
        break;
      }
      GST_DELAY_MUTEX_LOCK(delay);
      if( delay->srcresult != GST_FLOW_OK)
      {
        ret = GST_CLOCK_ERROR;
        break;
      }
    }    
  }
  else
  {
    ret = GST_CLOCK_BUSY;
  }
  gst_clock_id_unref (render_id);
  return ret == GST_CLOCK_OK;
}

/* dequeue an item from the delay an push it downstream. This functions returns
 * the result of the push. */
static GstFlowReturn
gst_delay_push_one (GstDelay * delay)
{
  GstFlowReturn result = GST_FLOW_OK;
  QueueItem *data;
  gboolean is_buffer;

  data = gst_delay_locked_dequeue (delay, &is_buffer);
  if (data == NULL)
    goto no_item;

next:

  if(!gst_delay_wait_delay(delay,data))
  {
    g_free(data);
    goto no_sync;
  }

  if (is_buffer) {
    GstBuffer *buffer;
    GstCaps *caps;
    gint64 offset;

    buffer = GST_BUFFER_CAST (data->data);
    offset = data->offset;
    g_free(data);
    data = 0;

    if(!gst_delay_load_data(delay, buffer, offset))
    {
      gst_buffer_unref(buffer);
      goto no_data;
    }

    if (delay->head_needs_discont) {
      GstBuffer *subbuffer = gst_buffer_make_metadata_writable (buffer);

      if (subbuffer) {
        buffer = subbuffer;
        GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
      } else {
        GST_DEBUG_OBJECT (delay, "Could not mark buffer as DISCONT");
      }
      delay->head_needs_discont = FALSE;
    }

    caps = GST_BUFFER_CAPS (buffer);

    GST_DELAY_MUTEX_UNLOCK (delay);
    /* set the right caps on the pad now. We do this before pushing the buffer
     * because the pad_push call will check (using acceptcaps) if the buffer can
     * be set on the pad, which might fail because this will be propagated
     * upstream. Also note that if the buffer has NULL caps, it means that the
     * caps did not change, so we don't have to change caps on the pad. */
    if (caps && caps != GST_PAD_CAPS (delay->srcpad))
      gst_pad_set_caps (delay->srcpad, caps);

    if (delay->push_newsegment) {
      gst_delay_push_newsegment (delay);
    }
    result = gst_pad_push (delay->srcpad, buffer);

    /* need to check for srcresult here as well */
    GST_DELAY_MUTEX_LOCK_CHECK (delay, out_flushing);

    if (result == GST_FLOW_UNEXPECTED) {
      GST_CAT_LOG_OBJECT (delay_dataflow, delay,
          "got UNEXPECTED from downstream");
      /* stop pushing buffers, we dequeue all items until we see an item that we
       * can push again, which is EOS or NEWSEGMENT. If there is nothing in the
       * delay we can push, we set a flag to make the sinkpad refuse more
       * buffers with an UNEXPECTED return value. */
      while ((data = gst_delay_locked_dequeue (delay, &is_buffer))) {
        if (is_buffer) {
          GST_CAT_LOG_OBJECT (delay_dataflow, delay,
              "dropping UNEXPECTED buffer %p", data);
          gst_buffer_unref (GST_BUFFER_CAST (data->data));
          g_free(data);
        } else {
          GstEvent *event = GST_EVENT_CAST (data->data);
          GstEventType type = GST_EVENT_TYPE (event);

          if (type == GST_EVENT_EOS || type == GST_EVENT_NEWSEGMENT) {
            /* we found a pushable item in the delay, push it out */
            GST_CAT_LOG_OBJECT (delay_dataflow, delay,
                "pushing pushable event %s after UNEXPECTED",
                GST_EVENT_TYPE_NAME (event));
            goto next;
          }
          GST_CAT_LOG_OBJECT (delay_dataflow, delay,
              "dropping UNEXPECTED event %p", event);
          gst_event_unref (event);
          g_free(data);
        }
      }
      /* no more items in the delay. Set the unexpected flag so that upstream
       * make us refuse any more buffers on the sinkpad. Since we will still
       * accept EOS and NEWSEGMENT we return _FLOW_OK to the caller so that the
       * task function does not shut down. */
      delay->unexpected = TRUE;
      result = GST_FLOW_OK;
    }
  } else {
    GstEvent *event = GST_EVENT_CAST (data->data);
    GstEventType type = GST_EVENT_TYPE (event);

    g_free(data);
    data = 0;

    GST_DELAY_MUTEX_UNLOCK (delay);

    if (delay->push_newsegment && type != GST_EVENT_NEWSEGMENT) {
      gst_delay_push_newsegment (delay);
    }
    gst_pad_push_event (delay->srcpad, event);

    GST_DELAY_MUTEX_LOCK_CHECK (delay, out_flushing);
    /* if we're EOS, return UNEXPECTED so that the task pauses. */
    if (type == GST_EVENT_EOS) {
      GST_CAT_INFO_OBJECT (delay_dataflow, delay,
          "pushed EOS event %p, return UNEXPECTED", event);
      result = GST_FLOW_UNEXPECTED;
    }
  }
  return result;

  /* ERRORS */
no_item:
  {
    GST_CAT_LOG_OBJECT (delay_dataflow, delay,
        "exit because we have no item in the delay");
    return GST_FLOW_ERROR;
  }
no_sync:
  {    
    GST_CAT_LOG_OBJECT (delay_dataflow, delay,
      "exit because we are out of sync in the delay");
    return GST_FLOW_UNEXPECTED;
  }
no_data:
  {
    GST_CAT_LOG_OBJECT (delay_dataflow, delay,
      "exit because we failed load data from cache in the delay");
    return GST_FLOW_UNEXPECTED;
  }
out_flushing:
  {
    GST_CAT_LOG_OBJECT (delay_dataflow, delay, "exit because we are flushing");
    return GST_FLOW_WRONG_STATE;
  }
}

static void
gst_delay_loop (GstPad * pad)
{
  GstDelay *delay;
  GstFlowReturn ret;

  delay = (GstDelay *) GST_PAD_PARENT (pad);

  /* have to lock for thread-safety */
  GST_DELAY_MUTEX_LOCK_CHECK (delay, out_flushing);

  while (gst_delay_is_empty (delay)) {
    GST_CAT_DEBUG_OBJECT (delay_dataflow, delay, "delay is empty");
    if (!delay->silent) {
      GST_DELAY_MUTEX_UNLOCK (delay);
      g_signal_emit (delay, gst_delay_signals[SIGNAL_UNDERRUN], 0);
      GST_DELAY_MUTEX_LOCK_CHECK (delay, out_flushing);
    }

    /* we recheck, the signal could have changed the thresholds */
    while (gst_delay_is_empty (delay)) {
      GST_DELAY_WAIT_ADD_CHECK (delay, out_flushing);
    }

    GST_CAT_DEBUG_OBJECT (delay_dataflow, delay, "delay is not empty");
    if (!delay->silent) {
      GST_DELAY_MUTEX_UNLOCK (delay);
      g_signal_emit (delay, gst_delay_signals[SIGNAL_RUNNING], 0);
      g_signal_emit (delay, gst_delay_signals[SIGNAL_PUSHING], 0);
      GST_DELAY_MUTEX_LOCK_CHECK (delay, out_flushing);
    }
  }

  ret = gst_delay_push_one (delay);
  delay->push_newsegment = FALSE;
  delay->srcresult = ret;
  if (ret != GST_FLOW_OK)
    goto out_flushing;

  GST_DELAY_MUTEX_UNLOCK (delay);

  return;

  /* ERRORS */
out_flushing:
  {
    gboolean eos = delay->eos;
    GstFlowReturn ret = delay->srcresult;

    gst_pad_pause_task (delay->srcpad);
    GST_CAT_LOG_OBJECT (delay_dataflow, delay,
        "pause task, reason:  %s", gst_flow_get_name (ret));
    GST_DELAY_SIGNAL_DEL (delay);
    GST_DELAY_MUTEX_UNLOCK (delay);
    /* let app know about us giving up if upstream is not expected to do so */
    /* UNEXPECTED is already taken care of elsewhere */
    if (eos && (ret == GST_FLOW_NOT_LINKED || ret < GST_FLOW_UNEXPECTED)) {
      GST_ELEMENT_ERROR (delay, STREAM, FAILED,
          ("Internal data flow error."),
          ("streaming task paused, reason %s (%d)",
              gst_flow_get_name (ret), ret));
      gst_pad_push_event (delay->srcpad, gst_event_new_eos ());
    }
    return;
  }
}

static gboolean
gst_delay_handle_src_event (GstPad * pad, GstEvent * event)
{
  gboolean res = TRUE;
  GstDelay *delay = GST_DELAY (gst_pad_get_parent (pad));

  if (G_UNLIKELY (delay == NULL)) {
    gst_event_unref (event);
    return FALSE;
  }
#ifndef GST_DISABLE_GST_DEBUG
  GST_CAT_DEBUG_OBJECT (delay_dataflow, delay, "got event %p (%d)",
      event, GST_EVENT_TYPE (event));
#endif

  res = gst_pad_push_event (delay->sinkpad, event);

  gst_object_unref (delay);
  return res;
}

static gboolean
gst_delay_handle_src_query (GstPad * pad, GstQuery * query)
{
  GstDelay *delay = GST_DELAY (gst_pad_get_parent (pad));
  GstPad *peer;
  gboolean res;

  if (G_UNLIKELY (delay == NULL))
    return FALSE;

  if (!(peer = gst_pad_get_peer (delay->sinkpad))) {
    gst_object_unref (delay);
    return FALSE;
  }

  res = gst_pad_query (peer, query);
  gst_object_unref (peer);
  if (!res) {
    gst_object_unref (delay);
    return FALSE;
  }

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      gint64 peer_pos;
      GstFormat format;

      /* get peer position */
      gst_query_parse_position (query, &format, &peer_pos);

      /* FIXME: this code assumes that there's no discont in the delay */
      switch (format) {
             case GST_FORMAT_TIME:
          peer_pos -= delay->current_level_time;
          break;
        default:
          GST_DEBUG_OBJECT (delay, "Can't adjust query in %s format, don't "
              "know how to adjust value", gst_format_get_name (format));
          return TRUE;
      }
      /* set updated position */
      gst_query_set_position (query, format, peer_pos);
      break;
    }
    case GST_QUERY_LATENCY:
    {
      gboolean live;
      GstClockTime min, max;

      gst_query_parse_latency (query, &live, &min, &max);
      max = -1;
      gst_query_set_latency (query, live, min, max);
      break;
    }
    default:
      /* peer handled other queries */
      break;
  }

  gst_object_unref (delay);
  return TRUE;
}

static gboolean
gst_delay_sink_activate_push (GstPad * pad, gboolean active)
{
  gboolean result = TRUE;
  GstDelay *delay;

  delay = GST_DELAY (gst_pad_get_parent (pad));

  if (active) {
    GST_DELAY_MUTEX_LOCK (delay);
    delay->srcresult = GST_FLOW_OK;
    delay->eos = FALSE;
    delay->unexpected = FALSE;
    GST_DELAY_MUTEX_UNLOCK (delay);
  } else {
    /* step 1, unblock chain function */
    GST_DELAY_MUTEX_LOCK (delay);
    delay->srcresult = GST_FLOW_WRONG_STATE;
    gst_delay_locked_flush (delay);
    GST_DELAY_MUTEX_UNLOCK (delay);
  }

  gst_object_unref (delay);

  return result;
}

static gboolean
gst_delay_src_activate_push (GstPad * pad, gboolean active)
{
  gboolean result = FALSE;
  GstDelay *delay;

  delay = GST_DELAY (gst_pad_get_parent (pad));

  if (active) {
    GST_DELAY_MUTEX_LOCK (delay);
    delay->srcresult = GST_FLOW_OK;
    delay->eos = FALSE;
    delay->unexpected = FALSE;
    /* we do not start the task yet if the pad is not connected */
    if (gst_pad_is_linked (pad))
      result = gst_pad_start_task (pad, (GstTaskFunction) gst_delay_loop, pad);
    else {
      GST_INFO_OBJECT (delay, "not starting task as pad is not linked");
      result = TRUE;
    }
    GST_DELAY_MUTEX_UNLOCK (delay);
  } else {
    /* step 1, unblock loop function */
    GST_DELAY_MUTEX_LOCK (delay);
    delay->srcresult = GST_FLOW_WRONG_STATE;
    /* the item add signal will unblock */
    g_cond_signal (delay->item_add);
    GST_DELAY_MUTEX_UNLOCK (delay);

    /* step 2, make sure streaming finishes */
    result = gst_pad_stop_task (pad);
  }

  gst_object_unref (delay);

  return result;
}

static void
queue_capacity_change (GstDelay * delay)
{
  /* changing the capacity of the delay must wake up
   * the _chain function, it might have more room now
   * to store the buffer/event in the delay */
  GST_DELAY_SIGNAL_DEL (delay);
}

/* Changing the minimum required fill level must
 * wake up the _loop function as it might now
 * be able to preceed.
 */
#define DELAY_THRESHOLD_CHANGE(q)\
  GST_DELAY_SIGNAL_ADD (q);

static void
gst_delay_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstDelay *delay = GST_DELAY (object);

  /* someone could change levels here, and since this
   * affects the get/put funcs, we need to lock for safety. */
  GST_DELAY_MUTEX_LOCK (delay);

  switch (prop_id) {
    case PROP_DELAY_TIME:
      delay->delay_time= g_value_get_uint64 (value);
      DELAY_THRESHOLD_CHANGE (delay);
      break;
    case PROP_SILENT:
      delay->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_DELAY_MUTEX_UNLOCK (delay);
}

static void
gst_delay_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstDelay *delay = GST_DELAY (object);

  GST_DELAY_MUTEX_LOCK (delay);

  switch (prop_id) {
    case PROP_CUR_LEVEL_TIME:
      g_value_set_uint64 (value, delay->current_level_time);
      break;
    case PROP_DELAY_TIME:
      g_value_set_uint64 (value, delay->delay_time);
      break;
    case PROP_SILENT:
      g_value_set_boolean (value, delay->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_DELAY_MUTEX_UNLOCK (delay);
}
static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "delay", GST_RANK_NONE, gst_delay_get_type()))
    return FALSE;
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
                   GST_VERSION_MINOR,
                   "delay",
                   "delay element for live broadcast",
                   plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);