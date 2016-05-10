/* GStreamer
 * Copyright (C) <2009> Collabora Ltd
 *  @author: Olivier Crete <olivier.crete@collabora.co.uk
 * Copyright (C) <2009> Nokia Inc
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

#include "gstshmsink.h"

#include <gst/gst.h>

#include <string.h>

/* signals */
enum
{
  SIGNAL_CLIENT_CONNECTED,
  SIGNAL_CLIENT_DISCONNECTED,
  LAST_SIGNAL
};

/* properties */
enum
{
  PROP_0,
  PROP_SOCKET_PATH,
  PROP_PERMS,
  PROP_SHM_SIZE,
  PROP_WAIT_FOR_CONNECTION,
  PROP_BUFFER_TIME
};

struct GstShmClient
{
  ShmClient *client;
  GstPollFD pollfd;
};

#define DEFAULT_SIZE ( 256 * 1024 )
#define DEFAULT_WAIT_FOR_CONNECTION (TRUE)
/* Default is user read/write, group read */
#define DEFAULT_PERMS ( S_IRUSR | S_IWUSR | S_IRGRP )


GST_DEBUG_CATEGORY_STATIC (shmsink_debug);
#define GST_CAT_DEFAULT shmsink_debug

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_BOILERPLATE (GstShmSink, gst_shm_sink, GstBaseSink, GST_TYPE_BASE_SINK);

static void gst_shm_sink_finalize (GObject * object);
static void gst_shm_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_shm_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_shm_sink_start (GstBaseSink * bsink);
static gboolean gst_shm_sink_stop (GstBaseSink * bsink);
static GstFlowReturn gst_shm_sink_render (GstBaseSink * bsink, GstBuffer * buf);
static GstFlowReturn gst_shm_sink_buffer_alloc (GstBaseSink * sink,
    guint64 offset, guint size, GstCaps * caps, GstBuffer ** out_buf);

static gboolean gst_shm_sink_event (GstBaseSink * bsink, GstEvent * event);
static gboolean gst_shm_sink_unlock (GstBaseSink * bsink);
static gboolean gst_shm_sink_unlock_stop (GstBaseSink * bsink);

static gpointer pollthread_func (gpointer data);

static guint signals[LAST_SIGNAL] = { 0 };

static void
gst_shm_sink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class, &sinktemplate);

  gst_element_class_set_details_simple (element_class,
      "Shared Memory Sink",
      "Sink",
      "Send data over shared memory to the matching source",
      "Olivier Crete <olivier.crete@collabora.co.uk>");
}

static void
gst_shm_sink_init (GstShmSink * self, GstShmSinkClass * g_class)
{
  self->cond = g_cond_new ();
  self->size = DEFAULT_SIZE;
  self->wait_for_connection = DEFAULT_WAIT_FOR_CONNECTION;
  self->perms = DEFAULT_PERMS;
}

static void
gst_shm_sink_class_init (GstShmSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSinkClass *gstbasesink_class;

  gobject_class = (GObjectClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;

  gobject_class->finalize = gst_shm_sink_finalize;
  gobject_class->set_property = gst_shm_sink_set_property;
  gobject_class->get_property = gst_shm_sink_get_property;

  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_shm_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_shm_sink_stop);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_shm_sink_render);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_shm_sink_event);
  gstbasesink_class->unlock = GST_DEBUG_FUNCPTR (gst_shm_sink_unlock);
  gstbasesink_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_shm_sink_unlock_stop);
  gstbasesink_class->buffer_alloc =
      GST_DEBUG_FUNCPTR (gst_shm_sink_buffer_alloc);

  g_object_class_install_property (gobject_class, PROP_SOCKET_PATH,
      g_param_spec_string ("socket-path",
          "Path to the control socket",
          "The path to the control socket used to control the shared memory"
          " transport", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PERMS,
      g_param_spec_uint ("perms",
          "Permissions on the shm area",
          "Permissions to set on the shm area",
          0, 07777, DEFAULT_PERMS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SHM_SIZE,
      g_param_spec_uint ("shm-size",
          "Size of the shm area",
          "Size of the shared memory area",
          0, G_MAXUINT, DEFAULT_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_WAIT_FOR_CONNECTION,
      g_param_spec_boolean ("wait-for-connection",
          "Wait for a connection until rendering",
          "Block the stream until the shm pipe is connected",
          DEFAULT_WAIT_FOR_CONNECTION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUFFER_TIME,
      g_param_spec_uint64 ("buffer-time",
          "Buffer Time of the shm buffer",
          "Maximum Size of the shm buffer in nanoseconds (-1 to disable)",
          0, G_MAXUINT64, GST_CLOCK_TIME_NONE,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  signals[SIGNAL_CLIENT_CONNECTED] = g_signal_new ("client-connected",
      GST_TYPE_SHM_SINK, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  signals[SIGNAL_CLIENT_DISCONNECTED] = g_signal_new ("client-disconnected",
      GST_TYPE_SHM_SINK, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  GST_DEBUG_CATEGORY_INIT (shmsink_debug, "shmsink", 0, "Shared Memory Sink");
}

static void
gst_shm_sink_finalize (GObject * object)
{
  GstShmSink *self = GST_SHM_SINK (object);

  g_cond_free (self->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*
 * Set the value of a property for the server sink.
 */
static void
gst_shm_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstShmSink *self = GST_SHM_SINK (object);
  int ret = 0;

  switch (prop_id) {
    case PROP_SOCKET_PATH:
      GST_OBJECT_LOCK (object);
      g_free (self->socket_path);
      self->socket_path = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (object);
      break;
    case PROP_PERMS:
      GST_OBJECT_LOCK (object);
      self->perms = g_value_get_uint (value);
      if (self->pipe)
        ret = sp_writer_setperms_shm (self->pipe, self->perms);
      GST_OBJECT_UNLOCK (object);
      if (ret < 0)
        GST_WARNING_OBJECT (object, "Could not set permissions on pipe: %s",
            strerror (ret));
      break;
    case PROP_SHM_SIZE:
      GST_OBJECT_LOCK (object);
      if (self->pipe) {
        if (sp_writer_resize (self->pipe, g_value_get_uint (value)) < 0)
          GST_DEBUG_OBJECT (self, "Resized shared memory area from %u to "
              "%u bytes", self->size, g_value_get_uint (value));
        else
          GST_WARNING_OBJECT (self, "Could not resize shared memory area from"
              "%u to %u bytes", self->size, g_value_get_uint (value));
      }
      self->size = g_value_get_uint (value);
      GST_OBJECT_UNLOCK (object);
      break;
    case PROP_WAIT_FOR_CONNECTION:
      GST_OBJECT_LOCK (object);
      self->wait_for_connection = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (object);
      g_cond_broadcast (self->cond);
      break;
    case PROP_BUFFER_TIME:
      GST_OBJECT_LOCK (object);
      self->buffer_time = g_value_get_uint64 (value);
      GST_OBJECT_UNLOCK (object);
      g_cond_broadcast (self->cond);
      break;
    default:
      break;
  }
}

static void
gst_shm_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstShmSink *self = GST_SHM_SINK (object);

  GST_OBJECT_LOCK (object);

  switch (prop_id) {
    case PROP_SOCKET_PATH:
      g_value_set_string (value, self->socket_path);
      break;
    case PROP_PERMS:
      self->perms = g_value_get_uint (value);
      if (self->pipe) {
        int ret;

        ret = sp_writer_setperms_shm (self->pipe, self->perms);
        if (ret < 0)
          GST_WARNING_OBJECT (object, "Could not set permissions on pipe: %s",
              strerror (ret));
      }
      break;
    case PROP_SHM_SIZE:
      g_value_set_uint (value, self->size);
      break;
    case PROP_WAIT_FOR_CONNECTION:
      g_value_set_boolean (value, self->wait_for_connection);
      break;
    case PROP_BUFFER_TIME:
      g_value_set_uint64 (value, self->buffer_time);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (object);
}



static gboolean
gst_shm_sink_start (GstBaseSink * bsink)
{
  GstShmSink *self = GST_SHM_SINK (bsink);

  self->stop = FALSE;

  if (!self->socket_path) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ_WRITE,
        ("Could not open socket."), (NULL));
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Creating new socket at %s"
      " with shared memory of %d bytes", self->socket_path, self->size);

  self->pipe = sp_writer_create (self->socket_path, self->size, self->perms);

  if (!self->pipe) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ_WRITE,
        ("Could not open socket."), (NULL));
    return FALSE;
  }

  sp_set_data (self->pipe, self);
  g_free (self->socket_path);
  self->socket_path = g_strdup (sp_writer_get_path (self->pipe));

  GST_DEBUG ("Created socket at %s", self->socket_path);

  self->poll = gst_poll_new (TRUE);
  gst_poll_fd_init (&self->serverpollfd);
  self->serverpollfd.fd = sp_get_fd (self->pipe);
  gst_poll_add_fd (self->poll, &self->serverpollfd);
  gst_poll_fd_ctl_read (self->poll, &self->serverpollfd, TRUE);

  self->pollthread = g_thread_create (pollthread_func, self, TRUE, NULL);

  if (!self->pollthread)
    goto thread_error;

  return TRUE;

thread_error:

  sp_close (self->pipe);
  self->pipe = NULL;
  gst_poll_free (self->poll);

  GST_ELEMENT_ERROR (self, CORE, THREAD, ("Could not srart thread"), (NULL));
  return FALSE;
}


static gboolean
gst_shm_sink_stop (GstBaseSink * bsink)
{
  GstShmSink *self = GST_SHM_SINK (bsink);

  self->stop = TRUE;
  gst_poll_set_flushing (self->poll, TRUE);

  g_thread_join (self->pollthread);
  self->pollthread = NULL;

  GST_DEBUG_OBJECT (self, "Stopping");

  while (self->clients) {
    struct GstShmClient *client = self->clients->data;
    self->clients = g_list_remove (self->clients, client);
    sp_writer_close_client (self->pipe, client->client);
    g_signal_emit (self, signals[SIGNAL_CLIENT_DISCONNECTED], 0,
        client->pollfd.fd);
    g_slice_free (struct GstShmClient, client);
  }

  gst_poll_free (self->poll);
  self->poll = NULL;

  sp_close (self->pipe);
  self->pipe = NULL;

  return TRUE;
}

static gboolean
gst_shm_sink_can_render (GstShmSink * self, GstClockTime time)
{
  ShmBuffer *b;

  if (time == GST_CLOCK_TIME_NONE || self->buffer_time == GST_CLOCK_TIME_NONE)
    return TRUE;

  b = sp_writer_get_pending_buffers (self->pipe);
  for (; b != NULL; b = sp_writer_get_next_buffer (b)) {
    GstClockTime t = sp_writer_buf_get_tag (b);
    if (GST_CLOCK_DIFF (time, t) > self->buffer_time)
      return FALSE;
  }

  return TRUE;
}

static GstFlowReturn
gst_shm_sink_render (GstBaseSink * bsink, GstBuffer * buf)
{
  GstShmSink *self = GST_SHM_SINK (bsink);
  int rv;

  GST_OBJECT_LOCK (self);
  while (self->wait_for_connection && !self->clients) {
    g_cond_wait (self->cond, GST_OBJECT_GET_LOCK (self));
    if (self->unlock) {
      GST_OBJECT_UNLOCK (self);
      return GST_FLOW_WRONG_STATE;
    }
  }

  while (!gst_shm_sink_can_render (self, GST_BUFFER_TIMESTAMP (buf))) {
    g_cond_wait (self->cond, GST_OBJECT_GET_LOCK (self));
    if (self->unlock) {
      GST_OBJECT_UNLOCK (self);
      return GST_FLOW_WRONG_STATE;
    }
  }

  rv = sp_writer_send_buf (self->pipe, (char *) GST_BUFFER_DATA (buf),
      GST_BUFFER_SIZE (buf), GST_BUFFER_TIMESTAMP (buf));

  if (rv == -1) {
    ShmBlock *block = NULL;
    gchar *shmbuf = NULL;
    while ((block = sp_writer_alloc_block (self->pipe,
                GST_BUFFER_SIZE (buf))) == NULL) {
      g_cond_wait (self->cond, GST_OBJECT_GET_LOCK (self));
      if (self->unlock) {
        GST_OBJECT_UNLOCK (self);
        return GST_FLOW_WRONG_STATE;
      }
    }
    while (self->wait_for_connection && !self->clients) {
      g_cond_wait (self->cond, GST_OBJECT_GET_LOCK (self));
      if (self->unlock) {
        sp_writer_free_block (block);
        GST_OBJECT_UNLOCK (self);
        return GST_FLOW_WRONG_STATE;
      }
    }

    shmbuf = sp_writer_block_get_buf (block);
    memcpy (shmbuf, GST_BUFFER_DATA (buf), GST_BUFFER_SIZE (buf));
    sp_writer_send_buf (self->pipe, shmbuf, GST_BUFFER_SIZE (buf),
        GST_BUFFER_TIMESTAMP (buf));
    sp_writer_free_block (block);
  }

  GST_OBJECT_UNLOCK (self);

  return GST_FLOW_OK;
}

static void
gst_shm_sink_free_buffer (gpointer data)
{
  ShmPipe *pipe;
  ShmBlock *block = data;
  GstShmSink *self;

  pipe = sp_writer_block_get_pipe (block);
  self = sp_get_data (pipe);

  GST_OBJECT_LOCK (self);
  sp_writer_free_block (block);
  GST_OBJECT_UNLOCK (self);
  g_object_unref (self);
}

static GstFlowReturn
gst_shm_sink_buffer_alloc (GstBaseSink * sink, guint64 offset, guint size,
    GstCaps * caps, GstBuffer ** out_buf)
{
  GstShmSink *self = GST_SHM_SINK (sink);
  GstBuffer *buffer;
  ShmBlock *block = NULL;
  gpointer buf = NULL;

  GST_OBJECT_LOCK (self);
  block = sp_writer_alloc_block (self->pipe, size);
  if (block) {
    buf = sp_writer_block_get_buf (block);
    g_object_ref (self);
  }
  GST_OBJECT_UNLOCK (self);

  if (block) {
    buffer = gst_buffer_new ();
    GST_BUFFER_DATA (buffer) = buf;
    GST_BUFFER_MALLOCDATA (buffer) = (guint8 *) block;
    GST_BUFFER_FREE_FUNC (buffer) =
        GST_DEBUG_FUNCPTR (gst_shm_sink_free_buffer);
    GST_BUFFER_SIZE (buffer) = size;
    GST_LOG_OBJECT (self,
        "Allocated buffer of %u bytes from shared memory at %p", size, buf);
  } else {
    buffer = gst_buffer_new_and_alloc (size);
    GST_LOG_OBJECT (self, "Not enough shared memory for buffer of %u bytes, "
        "allocating using standard allocator", size);
  }

  GST_BUFFER_OFFSET (buffer) = offset;
  gst_buffer_set_caps (buffer, caps);

  *out_buf = buffer;

  return GST_FLOW_OK;
}

static gpointer
pollthread_func (gpointer data)
{
  GstShmSink *self = GST_SHM_SINK (data);
  GList *item;
  GstClockTime timeout = GST_CLOCK_TIME_NONE;

  while (!self->stop) {

    if (gst_poll_wait (self->poll, timeout) < 0)
      return NULL;

    timeout = GST_CLOCK_TIME_NONE;

    if (self->stop)
      return NULL;

    if (gst_poll_fd_has_closed (self->poll, &self->serverpollfd)) {
      GST_ELEMENT_ERROR (self, RESOURCE, READ, ("Failed read from shmsink"),
          ("Control socket has closed"));
      return NULL;
    }

    if (gst_poll_fd_has_error (self->poll, &self->serverpollfd)) {
      GST_ELEMENT_ERROR (self, RESOURCE, READ, ("Failed to read from shmsink"),
          ("Control socket has error"));
      return NULL;
    }

    if (gst_poll_fd_can_read (self->poll, &self->serverpollfd)) {
      ShmClient *client;
      struct GstShmClient *gclient;

      GST_OBJECT_LOCK (self);
      client = sp_writer_accept_client (self->pipe);
      GST_OBJECT_UNLOCK (self);

      if (!client) {
        GST_ELEMENT_ERROR (self, RESOURCE, READ,
            ("Failed to read from shmsink"),
            ("Control socket returns wrong data"));
        return NULL;
      }

      gclient = g_slice_new (struct GstShmClient);
      gclient->client = client;
      gst_poll_fd_init (&gclient->pollfd);
      gclient->pollfd.fd = sp_writer_get_client_fd (client);
      gst_poll_add_fd (self->poll, &gclient->pollfd);
      gst_poll_fd_ctl_read (self->poll, &gclient->pollfd, TRUE);
      self->clients = g_list_prepend (self->clients, gclient);
      g_signal_emit (self, signals[SIGNAL_CLIENT_CONNECTED], 0,
          gclient->pollfd.fd);
      /* we need to call gst_poll_wait before calling gst_poll_* status
         functions on that new descriptor, so restart the loop, so _wait
         will have been called on all elements of self->poll, whether
         they have just been added or not. */
      timeout = 0;
      continue;
    }

  again:
    for (item = self->clients; item; item = item->next) {
      struct GstShmClient *gclient = item->data;

      if (gst_poll_fd_has_closed (self->poll, &gclient->pollfd)) {
        GST_WARNING_OBJECT (self, "One client is gone, closing");
        goto close_client;
      }

      if (gst_poll_fd_has_error (self->poll, &gclient->pollfd)) {
        GST_WARNING_OBJECT (self, "One client fd has error, closing");
        goto close_client;
      }

      if (gst_poll_fd_can_read (self->poll, &gclient->pollfd)) {
        int rv;

        GST_OBJECT_LOCK (self);
        rv = sp_writer_recv (self->pipe, gclient->client);
        GST_OBJECT_UNLOCK (self);

        if (rv < 0) {
          GST_WARNING_OBJECT (self, "One client has read error,"
              " closing (retval: %d errno: %d)", rv, errno);
          goto close_client;
        }
      }
      continue;
    close_client:
      GST_OBJECT_LOCK (self);
      sp_writer_close_client (self->pipe, gclient->client);
      GST_OBJECT_UNLOCK (self);

      gst_poll_remove_fd (self->poll, &gclient->pollfd);
      self->clients = g_list_remove (self->clients, gclient);

      g_signal_emit (self, signals[SIGNAL_CLIENT_DISCONNECTED], 0,
          gclient->pollfd.fd);
      g_slice_free (struct GstShmClient, gclient);

      goto again;
    }

    g_cond_broadcast (self->cond);
  }

  return NULL;
}

static gboolean
gst_shm_sink_event (GstBaseSink * bsink, GstEvent * event)
{
  GstShmSink *self = GST_SHM_SINK (bsink);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      GST_OBJECT_LOCK (self);
      while (self->wait_for_connection && sp_writer_pending_writes (self->pipe)
          && !self->unlock)
        g_cond_wait (self->cond, GST_OBJECT_GET_LOCK (self));
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      break;
  }

  return TRUE;
}


static gboolean
gst_shm_sink_unlock (GstBaseSink * bsink)
{
  GstShmSink *self = GST_SHM_SINK (bsink);

  GST_OBJECT_LOCK (self);
  self->unlock = TRUE;
  GST_OBJECT_UNLOCK (self);

  g_cond_broadcast (self->cond);
  return TRUE;
}

static gboolean
gst_shm_sink_unlock_stop (GstBaseSink * bsink)
{
  GstShmSink *self = GST_SHM_SINK (bsink);

  GST_OBJECT_LOCK (self);
  self->unlock = FALSE;
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}
