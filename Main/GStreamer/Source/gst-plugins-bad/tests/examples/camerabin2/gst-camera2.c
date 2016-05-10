/*
 * GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
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
/*
 * This is a demo application to test the camerabin element.
 * If you have question don't hesitate in contact me edgard.lima@indt.org.br
 */

/*
 * Includes
 */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gst-camera2.h"

#include <string.h>

#include <gst/pbutils/encoding-profile.h>
#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#define UI_FILE CAMERA_APPS_UIDIR G_DIR_SEPARATOR_S "gst-camera2.ui"

static GstElement *camera;
static GtkBuilder *builder;
static GtkWidget *ui_main_window;

typedef struct
{
  const gchar *name;
  GstEncodingProfile *(*create_profile) ();
} GstCameraVideoFormat;

static GstEncodingProfile *
create_ogg_profile (void)
{
  GstEncodingContainerProfile *container;

  container = gst_encoding_container_profile_new ("ogg", NULL,
      gst_caps_new_simple ("application/ogg", NULL), NULL);

  gst_encoding_container_profile_add_profile (container, (GstEncodingProfile *)
      gst_encoding_video_profile_new (gst_caps_new_simple ("video/x-theora",
              NULL), NULL, NULL, 1));
  gst_encoding_container_profile_add_profile (container, (GstEncodingProfile *)
      gst_encoding_audio_profile_new (gst_caps_new_simple ("audio/x-vorbis",
              NULL), NULL, NULL, 1));

  return (GstEncodingProfile *) container;
}

static GstEncodingProfile *
create_webm_profile (void)
{
  GstEncodingContainerProfile *container;

  container = gst_encoding_container_profile_new ("webm", NULL,
      gst_caps_new_simple ("video/webm", NULL), NULL);

  gst_encoding_container_profile_add_profile (container, (GstEncodingProfile *)
      gst_encoding_video_profile_new (gst_caps_new_simple ("video/x-vp8", NULL),
          NULL, NULL, 1));
  gst_encoding_container_profile_add_profile (container, (GstEncodingProfile *)
      gst_encoding_audio_profile_new (gst_caps_new_simple ("audio/x-vorbis",
              NULL), NULL, NULL, 1));

  return (GstEncodingProfile *) container;
}

static GstEncodingProfile *
create_mp4_profile (void)
{
  GstEncodingContainerProfile *container;

  container = gst_encoding_container_profile_new ("mp4", NULL,
      gst_caps_new_simple ("video/quicktime", "variant", G_TYPE_STRING, "iso",
          NULL), NULL);

  gst_encoding_container_profile_add_profile (container, (GstEncodingProfile *)
      gst_encoding_video_profile_new (gst_caps_new_simple ("video/x-h264",
              NULL), NULL, NULL, 1));
  gst_encoding_container_profile_add_profile (container, (GstEncodingProfile *)
      gst_encoding_audio_profile_new (gst_caps_new_simple ("audio/mpeg",
              "version", G_TYPE_INT, 4, NULL), NULL, NULL, 1));

  return (GstEncodingProfile *) container;
}

GstCameraVideoFormat formats[] = {
  {"ogg (theora/vorbis)", create_ogg_profile}
  ,
  {"webm (vp8/vorbis)", create_webm_profile}
  ,
  {"mp4 (h264+aac)", create_mp4_profile}
  ,
  {NULL, NULL}
};

void
on_mainWindow_delete_event (GtkWidget * widget, GdkEvent * event, gpointer data)
{
  gtk_main_quit ();
}

void
on_captureButton_clicked (GtkButton * button, gpointer user_data)
{
  g_signal_emit_by_name (camera, "start-capture", NULL);
}

void
on_stopCaptureButton_clicked (GtkButton * button, gpointer user_data)
{
  g_signal_emit_by_name (camera, "stop-capture", NULL);
}

void
on_imageRButton_toggled (GtkToggleButton * button, gpointer user_data)
{
  if (gtk_toggle_button_get_active (button)) {
    g_object_set (camera, "mode", 1, NULL);     /* Image mode */
  }
}

void
on_videoRButton_toggled (GtkToggleButton * button, gpointer user_data)
{
  if (gtk_toggle_button_get_active (button)) {
    g_object_set (camera, "mode", 2, NULL);     /* Video mode */
  }
}

void
on_viewfinderArea_realize (GtkWidget * widget, gpointer data)
{
#if GTK_CHECK_VERSION (2, 18, 0)
  gdk_window_ensure_native (gtk_widget_get_window (widget));
#endif
}

void
on_formatComboBox_changed (GtkWidget * widget, gpointer data)
{
  GstEncodingProfile *profile = NULL;
  gint index = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

  if (formats[index].create_profile) {
    profile = formats[index].create_profile ();
  }

  g_return_if_fail (profile != NULL);
  gst_element_set_state (camera, GST_STATE_NULL);
  g_object_set (camera, "video-profile", profile, NULL);
  gst_encoding_profile_unref (profile);

  if (GST_STATE_CHANGE_FAILURE == gst_element_set_state (camera,
          GST_STATE_PLAYING)) {
    GtkWidget *dialog =
        gtk_message_dialog_new (GTK_WINDOW (ui_main_window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
        "Could not initialize camerabin2 with the "
        "selected format. Your system might not have the required plugins installed.\n"
        "Please select another format.");

    gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);
  }
}

static GstBusSyncReply
bus_sync_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  GtkWidget *ui_drawing;

  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return GST_BUS_PASS;

  if (!gst_structure_has_name (message->structure, "prepare-xwindow-id"))
    return GST_BUS_PASS;

  /* FIXME: make sure to get XID in main thread */
  ui_drawing = GTK_WIDGET (gtk_builder_get_object (builder, "viewfinderArea"));
  gst_x_overlay_set_window_handle (GST_X_OVERLAY (message->src),
#if GTK_CHECK_VERSION (2, 91, 6)
      GDK_WINDOW_XID (gtk_widget_get_window (ui_drawing)));
#else
      GDK_WINDOW_XWINDOW (gtk_widget_get_window (ui_drawing)));
#endif

  gst_message_unref (message);
  return GST_BUS_DROP;
}


static gboolean
bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_WARNING:{
      GError *err;
      gchar *debug;

      gst_message_parse_warning (message, &err, &debug);
      g_print ("Warning: %s\n", err->message);
      g_error_free (err);
      g_free (debug);
      break;
    }
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *debug = NULL;

      gst_message_parse_error (message, &err, &debug);
      g_print ("Error: %s : %s\n", err->message, debug);
      g_error_free (err);
      g_free (debug);

      gtk_main_quit ();
      break;
    }
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      g_print ("Eos\n");
      gtk_main_quit ();
      break;
    case GST_MESSAGE_ELEMENT:
    {
      //handle_element_message (message);
      break;
    }
    default:
      /* unhandled message */
      break;
  }
  return TRUE;
}

static gboolean
init_gtkwidgets_data (void)
{
#if GTK_CHECK_VERSION(2,24,0)
  gint i;
  GtkComboBoxText *combobox =
      GTK_COMBO_BOX_TEXT (gtk_builder_get_object (builder, "formatComboBox"));

  /* init formats combobox */
  i = 0;
  while (formats[i].name) {
    gtk_combo_box_text_append_text (combobox, formats[i].name);
    i++;
  }

  /* default to the first one -> ogg */
  gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);
  return TRUE;
#else
  g_warning ("This needs a newer version of GTK (2.24 at least)");
  return FALSE;
#endif
}

int
main (int argc, char *argv[])
{
  int ret = 0;
  GError *error = NULL;
  GstBus *bus;

  gst_init (&argc, &argv);
  gtk_init (&argc, &argv);

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_file (builder, UI_FILE, &error)) {
    g_warning ("Error: %s", error->message);
    g_error_free (error);
    return 1;
  }

  camera = gst_element_factory_make ("camerabin2", "camera");
  bus = gst_pipeline_get_bus (GST_PIPELINE (camera));
  gst_bus_add_watch (bus, bus_callback, NULL);
  gst_bus_set_sync_handler (bus, bus_sync_callback, NULL);
  gst_object_unref (bus);

  if (!init_gtkwidgets_data ()) {
    goto error;
  }

  ui_main_window = GTK_WIDGET (gtk_builder_get_object (builder, "mainWindow"));
  gtk_builder_connect_signals (builder, NULL);
  gtk_widget_show_all (ui_main_window);

  gst_element_set_state (camera, GST_STATE_PLAYING);

  gtk_main ();

error:
  gst_element_set_state (camera, GST_STATE_NULL);
  gst_object_unref (camera);
  return ret;
}
