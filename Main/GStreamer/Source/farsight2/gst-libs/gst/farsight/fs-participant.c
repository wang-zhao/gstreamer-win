/*
 * Farsight2 - Farsight Participant
 *
 * Copyright 2007 Collabora Ltd.
 *  @author: Philippe Kalaf <philippe.kalaf@collabora.co.uk>
 * Copyright 2007 Nokia Corp.
 *
 * fs-participant.c - A Farsight Participant gobject (base implementation)
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
 * SECTION:fs-participant
 * @short_description: A participant in a conference
 *
 * This object is the base implementation of a Farsight Participant. It needs to be
 * derived and implemented by a farsight conference gstreamer element. A
 * participant represents any source of media in a conference. This could be a
 * human-participant or an automaton.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fs-participant.h"
#include "fs-enumtypes.h"
#include "fs-marshal.h"

/* Signals */
enum
{
  ERROR_SIGNAL,
  LAST_SIGNAL
};

/* props */
enum
{
  PROP_0,
  PROP_CNAME
};

struct _FsParticipantPrivate
{
  gchar *cname;
};

G_DEFINE_ABSTRACT_TYPE(FsParticipant, fs_participant, G_TYPE_OBJECT);

#define FS_PARTICIPANT_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), FS_TYPE_PARTICIPANT, \
   FsParticipantPrivate))

static void fs_participant_finalize (GObject *object);

static void fs_participant_get_property (GObject *object,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec);
static void fs_participant_set_property (GObject *object,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec);

static guint signals[LAST_SIGNAL] = { 0 };

static void
fs_participant_class_init (FsParticipantClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = fs_participant_set_property;
  gobject_class->get_property = fs_participant_get_property;

  /**
   * FsParticipant:cname:
   *
   * A string representing the cname of the current participant.
   * User must free the string after getting it.
   *
   */
  g_object_class_install_property (gobject_class,
      PROP_CNAME,
      g_param_spec_string ("cname",
        "The cname of the participant",
        "A string of the cname of the participant",
        NULL,
        G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * FsParticipant::error:
   * @self: #FsParticipant that emitted the signal
   * @object: The #Gobject that emitted the signal
   * @errorno: The number of the error
   * @error_msg: Error message to be displayed to user
   * @dbg_msg: Debugging error message
   *
   * This signal is emitted in any error condition
   */
  signals[ERROR_SIGNAL] = g_signal_new ("error",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL,
      NULL,
      _fs_marshal_VOID__OBJECT_ENUM_STRING_STRING,
      G_TYPE_NONE, 4, G_TYPE_OBJECT, FS_TYPE_ERROR, G_TYPE_STRING,
      G_TYPE_STRING);

  gobject_class->finalize = fs_participant_finalize;

  g_type_class_add_private (klass, sizeof (FsParticipantPrivate));
}

static void
fs_participant_init (FsParticipant *self)
{
  /* member init */
  self->priv = FS_PARTICIPANT_GET_PRIVATE (self);

  self->mutex = g_mutex_new ();
}

static void
fs_participant_finalize (GObject *object)
{
  FsParticipant *self = FS_PARTICIPANT (object);

  if (self->priv->cname) {
    g_free (self->priv->cname);
    self->priv->cname = NULL;
  }

  g_mutex_free (self->mutex);

  G_OBJECT_CLASS (fs_participant_parent_class)->finalize (object);
}

static void
fs_participant_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
  FsParticipant *self = FS_PARTICIPANT (object);

  switch (prop_id) {
    case PROP_CNAME:
      g_value_set_string (value, self->priv->cname);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
fs_participant_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
  FsParticipant *self = FS_PARTICIPANT (object);

  switch (prop_id) {
    case PROP_CNAME:
      self->priv->cname = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
