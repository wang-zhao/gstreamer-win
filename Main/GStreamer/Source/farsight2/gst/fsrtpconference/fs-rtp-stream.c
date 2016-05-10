/*
 * Farsight2 - Farsight RTP Stream
 *
 * Copyright 2007 Collabora Ltd.
 *  @author: Olivier Crete <olivier.crete@collabora.co.uk>
 * Copyright 2007 Nokia Corp.
 *
 * fs-rtp-stream.c - A Farsight RTP Stream gobject
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:fs-rtp-stream
 * @short_description: A RTP stream in a #FsRtpSession in a #FsRtpConference
 *
 * This is the conjunction of a #FsRtpParticipant and a #FsRtpSession,
 * it is created by calling fs_session_new_stream() on a
 * #FsRtpSession.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fs-rtp-stream.h"

#include <gst/gst.h>

#include "fs-rtp-marshal.h"

/* Signals */
enum
{
  LAST_SIGNAL
};

/* props */
enum
{
  PROP_0,
#if 0
  /* TODO Do we really need this? */
  PROP_SOURCE_PADS,
#endif
  PROP_REMOTE_CODECS,
  PROP_NEGOTIATED_CODECS,
  PROP_CURRENT_RECV_CODECS,
  PROP_DIRECTION,
  PROP_PARTICIPANT,
  PROP_SESSION,
  PROP_STREAM_TRANSMITTER
};

struct _FsRtpStreamPrivate
{
  FsRtpSession *session;
  FsStreamTransmitter *stream_transmitter;

  FsStreamDirection direction;

  GError *construction_error;

  stream_new_remote_codecs_cb new_remote_codecs_cb;
  stream_known_source_packet_receive_cb known_source_packet_received_cb;
  stream_sending_changed_locked_cb sending_changed_locked_cb;
  stream_ssrc_added_cb ssrc_added_cb;
  gpointer user_data_for_cb;

  gulong local_candidates_prepared_handler_id;
  gulong new_active_candidate_pair_handler_id;
  gulong new_local_candidate_handler_id;
  gulong error_handler_id;
  gulong known_source_packet_received_handler_id;
  gulong state_changed_handler_id;

  GMutex *mutex;
};


G_DEFINE_TYPE(FsRtpStream, fs_rtp_stream, FS_TYPE_STREAM);

#define FS_RTP_STREAM_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), FS_TYPE_RTP_STREAM, FsRtpStreamPrivate))

static void fs_rtp_stream_dispose (GObject *object);
static void fs_rtp_stream_finalize (GObject *object);

static void fs_rtp_stream_get_property (GObject *object,
                                    guint prop_id,
                                    GValue *value,
                                    GParamSpec *pspec);
static void fs_rtp_stream_set_property (GObject *object,
                                    guint prop_id,
                                    const GValue *value,
                                    GParamSpec *pspec);
static void fs_rtp_stream_constructed (GObject *object);


static gboolean fs_rtp_stream_set_remote_candidates (FsStream *stream,
                                                     GList *candidates,
                                                     GError **error);
static gboolean fs_rtp_stream_force_remote_candidates (FsStream *stream,
    GList *remote_candidates,
    GError **error);

static gboolean fs_rtp_stream_set_remote_codecs (FsStream *stream,
                                                 GList *remote_codecs,
                                                 GError **error);

static void fs_rtp_stream_add_id (FsStream *stream, guint id);

static void _local_candidates_prepared (
    FsStreamTransmitter *stream_transmitter,
    gpointer user_data);
static void _new_active_candidate_pair (
    FsStreamTransmitter *stream_transmitter,
    FsCandidate *candidate1,
    FsCandidate *candidate2,
    gpointer user_data);
static void _new_local_candidate (
    FsStreamTransmitter *stream_transmitter,
    FsCandidate *candidate,
    gpointer user_data);
static void
_known_source_packet_received (FsStreamTransmitter *st,
    guint component,
    GstBuffer *buffer,
    FsRtpStream *self);
static void _transmitter_error (
    FsStreamTransmitter *stream_transmitter,
    gint errorno,
    gchar *error_msg,
    gchar *debug_msg,
    gpointer user_data);
static void _substream_codec_changed (FsRtpSubStream *substream,
    FsRtpStream *stream);
static void _state_changed (FsStreamTransmitter *stream_transmitter,
    guint component,
    FsStreamState state,
    gpointer user_data);

// static guint signals[LAST_SIGNAL] = { 0 };

static void
fs_rtp_stream_class_init (FsRtpStreamClass *klass)
{
  GObjectClass *gobject_class;
  FsStreamClass *stream_class = FS_STREAM_CLASS (klass);

  gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = fs_rtp_stream_set_property;
  gobject_class->get_property = fs_rtp_stream_get_property;
  gobject_class->constructed = fs_rtp_stream_constructed;
  gobject_class->dispose = fs_rtp_stream_dispose;
  gobject_class->finalize = fs_rtp_stream_finalize;

  stream_class->set_remote_candidates = fs_rtp_stream_set_remote_candidates;
  stream_class->set_remote_codecs = fs_rtp_stream_set_remote_codecs;
  stream_class->force_remote_candidates = fs_rtp_stream_force_remote_candidates;
  stream_class->add_id = fs_rtp_stream_add_id;


  g_type_class_add_private (klass, sizeof (FsRtpStreamPrivate));

  g_object_class_override_property (gobject_class,
                                    PROP_REMOTE_CODECS,
                                    "remote-codecs");
  g_object_class_override_property (gobject_class,
                                    PROP_NEGOTIATED_CODECS,
                                    "negotiated-codecs");
  g_object_class_override_property (gobject_class,
                                    PROP_CURRENT_RECV_CODECS,
                                    "current-recv-codecs");
  g_object_class_override_property (gobject_class,
                                    PROP_DIRECTION,
                                    "direction");
  g_object_class_override_property (gobject_class,
                                    PROP_PARTICIPANT,
                                    "participant");
  g_object_class_override_property (gobject_class,
                                    PROP_SESSION,
                                    "session");
  /**
   * FsRtpStream:stream-transmitter:
   *
   * The #FsStreamTransmitter for this stream.
   *
   */
  g_object_class_install_property (gobject_class,
      PROP_STREAM_TRANSMITTER,
      g_param_spec_object ("stream-transmitter",
        "The transmitter use by the stream",
        "An FsStreamTransmitter used by this stream",
        FS_TYPE_STREAM_TRANSMITTER,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

}

static void
fs_rtp_stream_init (FsRtpStream *self)
{
  /* member init */
  self->priv = FS_RTP_STREAM_GET_PRIVATE (self);

  self->priv->session = NULL;
  self->participant = NULL;
  self->priv->stream_transmitter = NULL;

  self->priv->mutex = g_mutex_new ();

  self->priv->direction = FS_DIRECTION_NONE;
}

static FsRtpSession *
fs_rtp_stream_get_session (FsRtpStream *self, GError **error)
{
  FsRtpSession *session;

  g_mutex_lock (self->priv->mutex);
  session = self->priv->session;
  if (session)
    g_object_ref (session);
  g_mutex_unlock (self->priv->mutex);

  if (!session)
    g_set_error (error, FS_ERROR, FS_ERROR_DISPOSED,
        "Called function after stream has been disposed");

  return session;
}


static FsStreamTransmitter *
fs_rtp_stream_get_stream_transmitter (FsRtpStream *self, GError **error)
{
  FsRtpSession *session = fs_rtp_stream_get_session (self, error);
  FsStreamTransmitter *st = NULL;

  if (!session)
    return NULL;

  FS_RTP_SESSION_LOCK (session);
  st = self->priv->stream_transmitter;
  if (st)
    g_object_ref (st);
  FS_RTP_SESSION_UNLOCK (session);

  if (!st)
    g_set_error (error, FS_ERROR, FS_ERROR_DISPOSED,
        "Called function after stream has been disposed");

  g_object_unref (session);
  return st;
}

static void
fs_rtp_stream_dispose (GObject *object)
{
  FsRtpStream *self = FS_RTP_STREAM (object);
  FsStreamTransmitter *st;
  FsRtpParticipant *participant;
  FsRtpSession *session = fs_rtp_stream_get_session (self, NULL);

  if (!session)
    return;

  g_mutex_lock (self->priv->mutex);
  self->priv->session = NULL;
  g_mutex_unlock (self->priv->mutex);

  FS_RTP_SESSION_LOCK (session);

  if (self->priv->sending_changed_locked_cb &&
      self->priv->direction & FS_DIRECTION_SEND)
    self->priv->sending_changed_locked_cb (self, FALSE,
        self->priv->user_data_for_cb);

  participant = self->participant;
  self->participant = NULL;

  st = self->priv->stream_transmitter;
  self->priv->stream_transmitter = NULL;

  if (st)
  {
    g_signal_handler_disconnect (st,
        self->priv->local_candidates_prepared_handler_id);
    g_signal_handler_disconnect (st,
        self->priv->new_active_candidate_pair_handler_id);
    g_signal_handler_disconnect (st,
        self->priv->new_local_candidate_handler_id);
    g_signal_handler_disconnect (st,
        self->priv->error_handler_id);
    g_signal_handler_disconnect (st,
        self->priv->known_source_packet_received_handler_id);
    g_signal_handler_disconnect (st,
        self->priv->state_changed_handler_id);

    FS_RTP_SESSION_UNLOCK (session);
    fs_stream_transmitter_stop (st);
    g_object_unref (st);
    FS_RTP_SESSION_LOCK (session);
  }

  while (self->substreams)
  {
    FsRtpSubStream *substream = self->substreams->data;
    self->substreams = g_list_remove (self->substreams, substream);
    FS_RTP_SESSION_UNLOCK (session);
    g_object_unref (substream);
    FS_RTP_SESSION_LOCK (session);
  }

  FS_RTP_SESSION_UNLOCK (session);

  g_object_unref (participant);
  g_object_unref (session);
  g_object_unref (session);

  G_OBJECT_CLASS (fs_rtp_stream_parent_class)->dispose (object);
}

static void
fs_rtp_stream_finalize (GObject *object)
{
  FsRtpStream *self = FS_RTP_STREAM (object);

  fs_codec_list_destroy (self->remote_codecs);
  fs_codec_list_destroy (self->negotiated_codecs);

  g_mutex_free (self->priv->mutex);

  G_OBJECT_CLASS (fs_rtp_stream_parent_class)->finalize (object);
}

static gboolean
_codec_list_has_codec (GList *list, FsCodec *codec)
{
  for (; list; list = g_list_next (list))
  {
    FsCodec *listcodec = list->data;
    if (fs_codec_are_equal (codec, listcodec))
      return TRUE;
  }

  return FALSE;
}

static void
fs_rtp_stream_get_property (GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  FsRtpStream *self = FS_RTP_STREAM (object);
  FsRtpSession *session = fs_rtp_stream_get_session (self, NULL);

  if (!session)
    return;

  switch (prop_id) {
    case PROP_REMOTE_CODECS:
      FS_RTP_SESSION_LOCK (session);
      g_value_set_boxed (value, self->remote_codecs);
      FS_RTP_SESSION_UNLOCK (session);
      break;
    case PROP_NEGOTIATED_CODECS:
      FS_RTP_SESSION_LOCK (session);
      g_value_set_boxed (value, self->negotiated_codecs);
      FS_RTP_SESSION_UNLOCK (session);
      break;
    case PROP_SESSION:
      g_value_set_object (value, session);
      break;
    case PROP_PARTICIPANT:
      FS_RTP_SESSION_LOCK (session);
      g_value_set_object (value, self->participant);
      FS_RTP_SESSION_UNLOCK (session);
      break;
    case PROP_DIRECTION:
      g_value_set_flags (value, self->priv->direction);
      break;
    case PROP_CURRENT_RECV_CODECS:
      {
        GList *codeclist = NULL;
        GList *substream_item;

        FS_RTP_SESSION_LOCK (session);
        for (substream_item = g_list_first (self->substreams);
             substream_item;
             substream_item = g_list_next (substream_item))
        {
          FsRtpSubStream *substream = substream_item->data;

          if (substream->codec)
          {
            if (!_codec_list_has_codec (codeclist, substream->codec))
              codeclist = g_list_append (codeclist,
                  fs_codec_copy (substream->codec));
          }
        }

        g_value_take_boxed (value, codeclist);
        FS_RTP_SESSION_UNLOCK (session);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_object_unref (session);
}

static void
fs_rtp_stream_set_property (GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  FsRtpStream *self = FS_RTP_STREAM (object);
  GList *item;

  switch (prop_id) {
    case PROP_SESSION:
      self->priv->session = FS_RTP_SESSION (g_value_dup_object (value));
      break;
    case PROP_PARTICIPANT:
      self->participant = FS_RTP_PARTICIPANT (g_value_dup_object (value));
      break;
    case PROP_STREAM_TRANSMITTER:
      self->priv->stream_transmitter =
        FS_STREAM_TRANSMITTER (g_value_get_object (value));
      break;
    case PROP_DIRECTION:
      {
        FsStreamTransmitter *st = NULL;
        GList *copy = NULL;
        FsRtpSession *session = fs_rtp_stream_get_session (self, NULL);
        FsStreamDirection dir;

        if (!session)
        {
          self->priv->direction = g_value_get_flags (value);
          return;
        }

        FS_RTP_SESSION_LOCK (session);
        if (self->priv->sending_changed_locked_cb &&
            (self->priv->direction & FS_DIRECTION_SEND) !=
            (g_value_get_flags (value) & FS_DIRECTION_SEND))
          self->priv->sending_changed_locked_cb (self,
              g_value_get_flags (value) & FS_DIRECTION_SEND,
              self->priv->user_data_for_cb);

        dir = self->priv->direction = g_value_get_flags (value);
        FS_RTP_SESSION_UNLOCK (session);
        st = fs_rtp_stream_get_stream_transmitter (self, NULL);
        if (st)
        {
          g_object_set (self->priv->stream_transmitter, "sending",
              dir & FS_DIRECTION_SEND, NULL);
          g_object_unref (st);
        }

        FS_RTP_SESSION_LOCK (session);
        copy = g_list_copy (g_list_first (self->substreams));
        g_list_foreach (copy, (GFunc) g_object_ref, NULL);
        FS_RTP_SESSION_UNLOCK (session);

        for (item = copy;  item; item = g_list_next (item))
          g_object_set (G_OBJECT (item->data),
              "receiving", ((dir & FS_DIRECTION_RECV) != 0),
              NULL);
        g_list_foreach (copy, (GFunc) g_object_unref, NULL);
        g_list_free (copy);
        g_object_unref (session);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

}

static void
fs_rtp_stream_constructed (GObject *object)
{
  FsRtpStream *self = FS_RTP_STREAM_CAST (object);

  if (!self->priv->stream_transmitter) {
    self->priv->construction_error = g_error_new (FS_ERROR,
      FS_ERROR_CONSTRUCTION, "The Stream Transmitter has not been set");
    return;
  }


  g_object_set (self->priv->stream_transmitter, "sending",
    self->priv->direction & FS_DIRECTION_SEND, NULL);

  self->priv->local_candidates_prepared_handler_id =
    g_signal_connect_object (self->priv->stream_transmitter,
        "local-candidates-prepared",
        G_CALLBACK (_local_candidates_prepared),
        self, 0);
  self->priv->new_active_candidate_pair_handler_id =
    g_signal_connect_object (self->priv->stream_transmitter,
        "new-active-candidate-pair",
        G_CALLBACK (_new_active_candidate_pair),
        self, 0);
  self->priv->new_local_candidate_handler_id =
    g_signal_connect_object (self->priv->stream_transmitter,
        "new-local-candidate",
        G_CALLBACK (_new_local_candidate),
        self, 0);
  self->priv->error_handler_id =
    g_signal_connect_object (self->priv->stream_transmitter,
        "error",
        G_CALLBACK (_transmitter_error),
        self, 0);
  self->priv->known_source_packet_received_handler_id =
    g_signal_connect_object (self->priv->stream_transmitter,
        "known-source-packet-received",
        G_CALLBACK (_known_source_packet_received),
        self, 0);
  self->priv->state_changed_handler_id =
    g_signal_connect_object (self->priv->stream_transmitter,
        "state-changed",
        G_CALLBACK (_state_changed),
        self, 0);

  if (!fs_stream_transmitter_gather_local_candidates (
          self->priv->stream_transmitter,
          &self->priv->construction_error))
  {
    if (!self->priv->construction_error)
      self->priv->construction_error = g_error_new (FS_ERROR,
          FS_ERROR_INTERNAL,
          "Unknown error while gathering local candidates");
    return;
  }

  if (G_OBJECT_CLASS (fs_rtp_stream_parent_class)->constructed)
    G_OBJECT_CLASS (fs_rtp_stream_parent_class)->constructed(object);
}


/**
 * fs_rtp_stream_set_remote_candidate:
 */
static gboolean
fs_rtp_stream_set_remote_candidates (FsStream *stream, GList *candidates,
                                     GError **error)
{
  FsRtpStream *self = FS_RTP_STREAM (stream);
  FsStreamTransmitter *st = fs_rtp_stream_get_stream_transmitter (self, error);
  gboolean ret = FALSE;

  if (!st)
    return FALSE;

  ret = fs_stream_transmitter_set_remote_candidates (st, candidates, error);

  g_object_unref (st);
  return ret;
}

/**
 * fs_rtp_stream_force_remote_candidates
 *
 * Implement FsStream -> force_remote_candidates
 * by calling the same function in the stream transmittrer
 */

static gboolean
fs_rtp_stream_force_remote_candidates (FsStream *stream,
    GList *remote_candidates,
    GError **error)
{
  FsRtpStream *self = FS_RTP_STREAM (stream);
  FsStreamTransmitter *st = fs_rtp_stream_get_stream_transmitter (self, error);
  gboolean ret = FALSE;

  if (!st)
    return FALSE;


  ret = fs_stream_transmitter_force_remote_candidates (
      self->priv->stream_transmitter, remote_candidates,
      error);

  g_object_unref (st);
  return ret;
}


/**
 * fs_rtp_stream_set_remote_codecs:
 * @stream: an #FsStream
 * @remote_codecs: a #GList of #FsCodec representing the remote codecs
 * @error: location of a #GError, or NULL if no error occured
 *
 * This function will set the list of remote codecs for this stream. If
 * the given remote codecs couldn't be negotiated with the list of local
 * codecs or already negotiated codecs for the corresponding #FsSession, @error
 * will be set and %FALSE will be returned. The @remote_codecs list will be
 * copied so it must be free'd using fs_codec_list_destroy() when done.
 *
 * Returns: %FALSE if the remote codecs couldn't be set.
 */
static gboolean
fs_rtp_stream_set_remote_codecs (FsStream *stream,
                                 GList *remote_codecs, GError **error)
{
  FsRtpStream *self = FS_RTP_STREAM (stream);
  GList *item = NULL;
  FsMediaType media_type;
  FsRtpSession *session = fs_rtp_stream_get_session (self, error);
  GList *remote_codecs_copy;

  if (!session)
    return FALSE;

  if (remote_codecs == NULL) {
    g_set_error (error, FS_ERROR, FS_ERROR_INVALID_ARGUMENTS,
      "You can not set NULL remote codecs");
    goto error;
  }

  g_object_get (session, "media-type", &media_type, NULL);

  for (item = g_list_first (remote_codecs); item; item = g_list_next (item))
  {
    FsCodec *codec = item->data;

    if (!codec->encoding_name)
    {
      g_set_error (error, FS_ERROR, FS_ERROR_INVALID_ARGUMENTS,
          "The codec must have an encoding name");
      goto error;
    }
    if (codec->id < 0 || codec->id > 128)
    {
      g_set_error (error, FS_ERROR, FS_ERROR_INVALID_ARGUMENTS,
          "The codec id must be between 0 ans 128 for %s",
          codec->encoding_name);
      goto error;
    }
    if (codec->media_type != media_type)
    {
      g_set_error (error, FS_ERROR, FS_ERROR_INVALID_ARGUMENTS,
          "The media type for codec %s is not %s", codec->encoding_name,
          fs_media_type_to_string (media_type));
      goto error;
    }
  }

  remote_codecs_copy = codecs_copy_with_new_ptime (remote_codecs);
  if (self->priv->new_remote_codecs_cb (self, remote_codecs_copy, error,
          self->priv->user_data_for_cb))
  {
    gboolean is_new = TRUE;

    FS_RTP_SESSION_LOCK (session);
    if (self->remote_codecs)
    {
      is_new = !fs_codec_list_are_equal (self->remote_codecs, remote_codecs);
      fs_codec_list_destroy (self->remote_codecs);
    }
    self->remote_codecs = remote_codecs_copy;
    FS_RTP_SESSION_UNLOCK (session);

    if (is_new)
      g_object_notify (G_OBJECT (stream), "remote-codecs");
  } else {
    fs_codec_list_destroy (remote_codecs_copy);
    goto error;
  }

  g_object_unref (session);
  return TRUE;

 error:

  g_object_unref (session);
  return FALSE;
}

/**
 * fs_rtp_stream_new:
 * @session: The #FsRtpSession this stream is a child of
 * @participant: The #FsRtpParticipant this stream is for
 * @direction: the initial #FsDirection for this stream
 * @stream_transmitter: the #FsStreamTransmitter for this stream, one
 *   reference to it will be eaten
 * @new_remote_codecs_cb: Callback called when the remote codecs change
 * (ie when fs_rtp_stream_set_remote_codecs() is called). One must hold
 * the session lock across calls.
 * @known_source_packet_received: Callback called when a packet from a
 * known source is receive.
 * @sending_changed_locked_cb: Callback called when the sending status of
 *  this stream changes
 * @user_data: User data for the callbacks.
 * This function create a new stream
 *
 * Returns: the newly created string or NULL on error
 */

FsRtpStream *
fs_rtp_stream_new (FsRtpSession *session,
    FsRtpParticipant *participant,
    FsStreamDirection direction,
    FsStreamTransmitter *stream_transmitter,
    stream_new_remote_codecs_cb new_remote_codecs_cb,
    stream_known_source_packet_receive_cb known_source_packet_received_cb,
    stream_sending_changed_locked_cb sending_changed_locked_cb,
    stream_ssrc_added_cb ssrc_added_cb,
    gpointer user_data_for_cb,
    GError **error)
{
  FsRtpStream *self;

  g_return_val_if_fail (session, NULL);
  g_return_val_if_fail (participant, NULL);
  g_return_val_if_fail (stream_transmitter, NULL);
  g_return_val_if_fail (new_remote_codecs_cb, NULL);
  g_return_val_if_fail (known_source_packet_received_cb, NULL);

  self = g_object_new (FS_TYPE_RTP_STREAM,
    "session", session,
    "participant", participant,
    "direction", direction,
    "stream-transmitter", stream_transmitter,
    NULL);

  self->priv->new_remote_codecs_cb = new_remote_codecs_cb;
  self->priv->known_source_packet_received_cb = known_source_packet_received_cb;
  self->priv->sending_changed_locked_cb = sending_changed_locked_cb;
  self->priv->ssrc_added_cb = ssrc_added_cb;
  self->priv->user_data_for_cb = user_data_for_cb;

  FS_RTP_SESSION_LOCK (session);
  if (sending_changed_locked_cb && (direction & FS_DIRECTION_SEND))
    sending_changed_locked_cb (self, direction & FS_DIRECTION_SEND,
        user_data_for_cb);
  FS_RTP_SESSION_UNLOCK (session);

  if (self->priv->construction_error) {
    g_propagate_error (error, self->priv->construction_error);
    g_object_unref (self);
    return NULL;
  }

  return self;
}


static void
_local_candidates_prepared (FsStreamTransmitter *stream_transmitter,
    gpointer user_data)
{
  FsRtpStream *self = FS_RTP_STREAM (user_data);
  GstElement *conf = NULL;
  FsRtpSession *session = fs_rtp_stream_get_session (self, NULL);

  if (!session)
    return;

  g_object_get (session, "conference", &conf, NULL);

  gst_element_post_message (conf,
      gst_message_new_element (GST_OBJECT (conf),
          gst_structure_new ("farsight-local-candidates-prepared",
              "stream", FS_TYPE_STREAM, self,
              NULL)));

  gst_object_unref (conf);
  g_object_unref (session);
}


static void
_new_active_candidate_pair (
    FsStreamTransmitter *stream_transmitter,
    FsCandidate *local_candidate,
    FsCandidate *remote_candidate,
    gpointer user_data)
{
  FsRtpStream *self = FS_RTP_STREAM (user_data);
  FsRtpSession *session = fs_rtp_stream_get_session (self, NULL);
  GstElement *conf = NULL;

  if (!session)
    return;

  g_object_get (session, "conference", &conf, NULL);

  gst_element_post_message (conf,
      gst_message_new_element (GST_OBJECT (conf),
          gst_structure_new ("farsight-new-active-candidate-pair",
              "stream", FS_TYPE_STREAM, self,
              "local-candidate", FS_TYPE_CANDIDATE, local_candidate,
              "remote-candidate", FS_TYPE_CANDIDATE, remote_candidate,
              NULL)));

  gst_object_unref (conf);
  g_object_unref (session);
}


static void
_new_local_candidate (
    FsStreamTransmitter *stream_transmitter,
    FsCandidate *candidate,
    gpointer user_data)
{
  FsRtpStream *self = FS_RTP_STREAM (user_data);
  FsRtpSession *session = fs_rtp_stream_get_session (self, NULL);
  GstElement *conf = NULL;

  if (!session)
    return;

  g_object_get (session, "conference", &conf, NULL);

  gst_element_post_message (conf,
      gst_message_new_element (GST_OBJECT (conf),
          gst_structure_new ("farsight-new-local-candidate",
              "stream", FS_TYPE_STREAM, self,
              "candidate", FS_TYPE_CANDIDATE, candidate,
              NULL)));

  gst_object_unref (conf);
  g_object_unref (session);
}

static void
_transmitter_error (
    FsStreamTransmitter *stream_transmitter,
    gint errorno,
    gchar *error_msg,
    gchar *debug_msg,
    gpointer user_data)
{
  FsStream *stream = FS_STREAM (user_data);

  fs_stream_emit_error (stream, errorno, error_msg, debug_msg);
}

static void
_known_source_packet_received (FsStreamTransmitter *st,
    guint component,
    GstBuffer *buffer,
    FsRtpStream *self)
{
  self->priv->known_source_packet_received_cb (self, component, buffer,
      self->priv->user_data_for_cb);
}

static void
_state_changed (FsStreamTransmitter *stream_transmitter,
    guint component,
    FsStreamState state,
    gpointer user_data)
{
  FsRtpStream *self = FS_RTP_STREAM (user_data);
  FsRtpSession *session = fs_rtp_stream_get_session (self, NULL);
  GstElement *conf = NULL;

  if (!session)
    return;

  g_object_get (session, "conference", &conf, NULL);

  gst_element_post_message (conf,
      gst_message_new_element (GST_OBJECT (conf),
          gst_structure_new ("farsight-component-state-changed",
              "stream", FS_TYPE_STREAM, self,
              "component", G_TYPE_UINT, component,
              "state", FS_TYPE_STREAM_STATE, state,
              NULL)));

  gst_object_unref (conf);
  g_object_unref (session);

  if (component == 1 && state == FS_STREAM_STATE_FAILED)
    fs_stream_emit_error (FS_STREAM (self), FS_ERROR_CONNECTION_FAILED,
        "Could not establish connection", "Could not establish connection"
        " on the RTP component");
}

static void
_substream_src_pad_added (FsRtpSubStream *substream, GstPad *pad,
                          FsCodec *codec, gpointer user_data)
{
  FsStream *stream = FS_STREAM (user_data);

  fs_stream_emit_src_pad_added (stream, pad, codec);
}

static void
_substream_error (FsRtpSubStream *substream,
    gint errorno,
    gchar *error_msg,
    gchar *debug_msg,
    gpointer user_data)
{
  FsStream *stream = FS_STREAM (user_data);

  fs_stream_emit_error (stream, errorno, error_msg, debug_msg);
}


static void
_substream_unlinked (FsRtpSubStream *substream, gpointer user_data)
{
  FsRtpStream *stream = FS_RTP_STREAM (user_data);
  FsRtpSession *session =  fs_rtp_stream_get_session (stream, NULL);

  if (!session)
    return;

  FS_RTP_SESSION_LOCK (session);
  stream->substreams = g_list_remove (stream->substreams,
      substream);
  FS_RTP_SESSION_UNLOCK (session);

  fs_rtp_sub_stream_stop (substream);

  g_object_unref (substream);
  g_object_unref (session);
}


/**
 * fs_rtp_stream_add_substream_unlock:
 * @stream: a #FsRtpStream
 * @substream: the #FsRtpSubStream to associate with this stream
 *
 * This functions associates a substream with this stream
 *
 * You must enter this function with the session lock held and it will release
 * it.
 *
 * Returns: TRUE on success, FALSE on failure
 */
gboolean
fs_rtp_stream_add_substream_unlock (FsRtpStream *stream,
    FsRtpSubStream *substream,
    GError **error)
{
  gboolean ret = TRUE;
  FsRtpSession *session = fs_rtp_stream_get_session (stream, error);

  if (!session)
    return FALSE;

  stream->substreams = g_list_prepend (stream->substreams,
      substream);
  g_object_set (substream,
      "stream", stream,
      "receiving", ((stream->priv->direction & FS_DIRECTION_RECV) != 0),
      NULL);

  g_signal_connect_object (substream, "unlinked",
      G_CALLBACK (_substream_unlinked), stream, 0);
  g_signal_connect_object (substream, "src-pad-added",
      G_CALLBACK (_substream_src_pad_added), stream, 0);
  g_signal_connect_object (substream, "codec-changed",
      G_CALLBACK (_substream_codec_changed), stream, 0);
  g_signal_connect_object (substream, "error",
      G_CALLBACK (_substream_error), stream, 0);

  fs_rtp_sub_stream_verify_codec_locked (substream);

  /* Only announce a pad if it has a codec attached to it */
  if (substream->codec)
    ret = fs_rtp_sub_stream_add_output_ghostpad_unlock (substream, error);
  else
    FS_RTP_SESSION_UNLOCK (session);

  g_object_unref (session);

  return ret;
}

/**
 *  _substream_codec_changed
 * @substream: The #FsRtpSubStream that may have a new receive codec
 * @stream: a #FsRtpStream
 *
 * This function checks if the specified substream introduces a new codec
 * not present in another substream and if it does, it emits a GstMessage
 * and the notify signal
 */

static void
_substream_codec_changed (FsRtpSubStream *substream,
    FsRtpStream *stream)
{
  GList *substream_item = NULL;
  GList *codeclist = NULL;
  FsRtpSession *session = fs_rtp_stream_get_session (stream, NULL);

  if (!session)
    return;

  FS_RTP_SESSION_LOCK (session);

  if (!substream->codec)
  {
    FS_RTP_SESSION_UNLOCK (session);
    g_object_unref (session);
    return;
  }

  codeclist = g_list_prepend (NULL, fs_codec_copy (substream->codec));

  for (substream_item = stream->substreams;
       substream_item;
       substream_item = g_list_next (substream_item))
  {
    FsRtpSubStream *othersubstream = substream_item->data;

    if (othersubstream != substream)
    {
      if (othersubstream->codec)
      {
        if (fs_codec_are_equal (substream->codec, othersubstream->codec))
          break;

        if (!_codec_list_has_codec (codeclist, othersubstream->codec))
          codeclist = g_list_append (codeclist,
              fs_codec_copy (othersubstream->codec));
      }
    }
  }

  FS_RTP_SESSION_UNLOCK (session);

  if (substream_item == NULL)
  {
    GstElement *conf = NULL;

    g_object_notify (G_OBJECT (stream), "current-recv-codecs");

    g_object_get (session, "conference", &conf, NULL);

    gst_element_post_message (conf,
        gst_message_new_element (GST_OBJECT (conf),
            gst_structure_new ("farsight-recv-codecs-changed",
                "stream", FS_TYPE_STREAM, stream,
                "codecs", FS_TYPE_CODEC_LIST, codeclist,
                NULL)));

    gst_object_unref (conf);
  }

  fs_codec_list_destroy (codeclist);
  g_object_unref (session);
}

/**
 * fs_rtp_stream_set_negotiated_codecs_unlock
 * @stream: a #FsRtpStream
 * @codecs: The #GList of #FsCodec to set for the negotiated-codecs property
 *
 * This function sets the value of the FsStream:negotiated-codecs property.
 * Unlike most other functions in this element, it TAKES the reference to the
 * codecs, so you have to give it its own copy.
 *
 * You must enter this function with the session lock held and it will release
 * it.
 */
void
fs_rtp_stream_set_negotiated_codecs_unlock (FsRtpStream *stream,
    GList *codecs)
{
  FsRtpSession *session = fs_rtp_stream_get_session (stream, NULL);

  if (!session)
    return;

  if (fs_codec_list_are_equal (stream->negotiated_codecs, codecs))
  {
    fs_codec_list_destroy (codecs);
    FS_RTP_SESSION_UNLOCK (session);
    return;
  }

  if (stream->negotiated_codecs)
    fs_codec_list_destroy (stream->negotiated_codecs);

  stream->negotiated_codecs = codecs;

  FS_RTP_SESSION_UNLOCK (session);

  g_object_notify (G_OBJECT (stream), "negotiated-codecs");

  g_object_unref (session);
}

static void
fs_rtp_stream_add_id (FsStream *stream, guint id)
{
  FsRtpStream *self = FS_RTP_STREAM (stream);
  FsRtpSession *session = fs_rtp_stream_get_session (self, NULL);

  if (!session)
    return;

  if (self->priv->ssrc_added_cb)
    self->priv->ssrc_added_cb (self, id, self->priv->user_data_for_cb);

  g_object_unref (session);
}
