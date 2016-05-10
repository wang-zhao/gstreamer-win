/* -*- Mode: C; ; c-file-style: "k&r"; c-basic-offset: 4 -*- */
/* gst-python
 * Copyright (C) 2002 David I. Lehn
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
 * 
 * Author: David I. Lehn <dlehn@users.sourceforge.net>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* include this first, before NO_IMPORT_PYGOBJECT is defined */
#include <pygobject.h>
#include <gst/gst.h>
#include <gst/gstversion.h>
#include "pygst-private.h"
#include "pygstexception.h"

#include <locale.h>


void pygst_register_classes (PyObject * d);
void pygst_add_constants (PyObject * module, const gchar * strip_prefix);
void _pygst_register_boxed_types (PyObject * moddict);

extern PyMethodDef pygst_functions[];

GST_DEBUG_CATEGORY (pygst_debug);       /* for bindings code */
GST_DEBUG_CATEGORY (python_debug);      /* for python code */

/* copied from pygtk to register GType */
#define REGISTER_TYPE(d, type, name) \
    type.ob_type = &PyType_Type; \
    type.tp_alloc = PyType_GenericAlloc; \
    type.tp_new = PyType_GenericNew; \
    if (PyType_Ready(&type)) \
        return; \
    PyDict_SetItemString(d, name, (PyObject *)&type);

#define REGISTER_GTYPE(d, type, name, gtype) \
    REGISTER_TYPE(d, type, name); \
    PyDict_SetItemString(type.tp_dict, "__gtype__", \
                         o=pyg_type_wrapper_new(gtype)); \
    Py_DECREF(o);

static PyObject *
pygstminiobject_from_gvalue (const GValue * value)
{
  GstMiniObject *miniobj;

  if ((miniobj = gst_value_get_mini_object (value)) == NULL) {
    Py_INCREF (Py_None);
    return Py_None;
  }
  return pygstminiobject_new (miniobj);
}

static int
pygstminiobject_to_gvalue (GValue * value, PyObject * obj)
{
  PyGstMiniObject *self = (PyGstMiniObject *) obj;

  gst_value_set_mini_object (value, self->obj);
  return 0;
}

static void
sink_gstobject (GObject * object)
{
  if (GST_OBJECT_IS_FLOATING (object)) {
    g_object_ref (object);
    gst_object_sink (GST_OBJECT (object));
  }
}

static PyObject *
pygst_fraction_from_value (const GValue * value)
{
  PyObject *module, *dict, *fraction_type, *args, *fraction;
  gint numerator, denominator;

  numerator = gst_value_get_fraction_numerator (value);
  denominator = gst_value_get_fraction_denominator (value);

  module = PyImport_ImportModule ("gst");
  dict = PyModule_GetDict (module);
  fraction_type = PyMapping_GetItemString (dict, "Fraction");

  args = Py_BuildValue ("(ii)", numerator, denominator);
  fraction = PyObject_Call (fraction_type, args, NULL);
  Py_DECREF (args);
  Py_DECREF (fraction_type);
  Py_DECREF (module);

  return fraction;
}

static int
pygst_fraction_to_value (GValue * value, PyObject * object)
{
  PyObject *numerator, *denominator;

  numerator = PyObject_GetAttrString (object, "num");
  if (numerator == NULL)
    goto fail;

  denominator = PyObject_GetAttrString (object, "denom");
  if (denominator == NULL)
    goto fail;

  gst_value_set_fraction (value,
      PyLong_AsLong (numerator), PyLong_AsLong (denominator));

out:
  return 0;

fail:
  return -1;
}

struct _PyGst_Functions pygst_api_functions = {
  pygst_caps_from_pyobject,
  pygst_iterator_new,
  pygstminiobject_new
};

/* for addon libraries ... */
static void
pygst_register_api (PyObject * d)
{
  PyObject *api;

  api = PyCObject_FromVoidPtr (&pygst_api_functions, NULL);
  PyDict_SetItemString (d, "_PyGst_API", api);
  Py_DECREF (api);
}

int __stdcall DllMain(void * _HDllHandle, unsigned _Reason, void * _Reserved)
{
  return 1;
}

unsigned int
__stdcall
SetErrorMode(
             unsigned int uMode
             );

DL_EXPORT (void)
init_gst (void)
{
  PyObject *m, *d;
  PyObject *av, *tuple;
  int argc, i;
  guint major, minor, micro, nano;
  char **argv;
  GError *error = NULL;

  //SetErrorMode(0);

  init_pygobject ();

  /* pull in arguments */
  av = PySys_GetObject ("argv");
  if (av != NULL) {
    argc = PyList_Size (av);
    argv = g_new (char *, argc);
    for (i = 0; i < argc; i++)
      argv[i] = g_strdup (PyString_AsString (PyList_GetItem (av, i)));
  } else {
    /* gst_init_check does not like argc == 0 */
    argc = 1;
    argv = g_new (char *, argc);
    argv[0] = g_strdup ("");
  }
  if (!gst_init_check (&argc, &argv, &error)) {
    gchar *errstr;

    if (argv != NULL) {
      for (i = 0; i < argc; i++)
        g_free (argv[i]);
      g_free (argv);
    }
    errstr = g_strdup_printf ("can't initialize module gst: %s",
        error ? GST_STR_NULL (error->message) : "no error given");
    PyErr_SetString (PyExc_RuntimeError, errstr);
    g_free (errstr);
    g_error_free (error);
    setlocale (LC_NUMERIC, "C");
    return;
  }

  setlocale (LC_NUMERIC, "C");
  if (argv != NULL) {
    PySys_SetArgv (argc, argv);
    for (i = 0; i < argc; i++)
      g_free (argv[i]);
    g_free (argv);
  }

  /* Initialize debugging category */
  GST_DEBUG_CATEGORY_INIT (pygst_debug, "pygst", 0,
      "GStreamer python bindings");
  GST_DEBUG_CATEGORY_INIT (python_debug, "python", GST_DEBUG_FG_GREEN,
      "python code using gst-python");

  pygobject_register_sinkfunc (GST_TYPE_OBJECT, sink_gstobject);

  m = Py_InitModule ("_gst", pygst_functions);
  d = PyModule_GetDict (m);
  pygst_register_api (d);

  /* gst version */
  gst_version (&major, &minor, &micro, &nano);
  tuple = Py_BuildValue ("(iii)", major, minor, micro);
  PyDict_SetItemString (d, "gst_version", tuple);
  Py_DECREF (tuple);

  /* gst-python version */
  tuple = Py_BuildValue ("(iii)", PYGST_MAJOR_VERSION, PYGST_MINOR_VERSION,
      PYGST_MICRO_VERSION);
  PyDict_SetItemString (d, "pygst_version", tuple);
  Py_DECREF (tuple);

  /* clock stuff */
  PyModule_AddIntConstant (m, "SECOND", GST_SECOND);
  PyModule_AddIntConstant (m, "MSECOND", GST_MSECOND);
  PyModule_AddIntConstant (m, "NSECOND", GST_NSECOND);

  PyModule_AddObject (m, "CLOCK_TIME_NONE",
      PyLong_FromUnsignedLongLong (GST_CLOCK_TIME_NONE));
  PyModule_AddObject (m, "BUFFER_OFFSET_NONE",
      PyLong_FromUnsignedLongLong (GST_BUFFER_OFFSET_NONE));

  pygst_exceptions_register_classes (d);

  REGISTER_TYPE (d, PyGstIterator_Type, "Iterator");


  pygstminiobject_register_class (d, "GstMiniObject", GST_TYPE_MINI_OBJECT,
      &PyGstMiniObject_Type, NULL);
  pyg_register_boxed_custom (GST_TYPE_MINI_OBJECT,
      pygstminiobject_from_gvalue, pygstminiobject_to_gvalue);

  pygst_register_classes (d);
  pygst_add_constants (m, "GST_");

  /* make our types available */
  PyModule_AddObject (m, "TYPE_ELEMENT_FACTORY",
      pyg_type_wrapper_new (GST_TYPE_ELEMENT_FACTORY));
  PyModule_AddObject (m, "TYPE_INDEX_FACTORY",
      pyg_type_wrapper_new (GST_TYPE_INDEX_FACTORY));
  PyModule_AddObject (m, "TYPE_TYPE_FIND_FACTORY",
      pyg_type_wrapper_new (GST_TYPE_TYPE_FIND_FACTORY));

  /* GStreamer core tags */
  PyModule_AddStringConstant (m, "TAG_TITLE", GST_TAG_TITLE);
  PyModule_AddStringConstant (m, "TAG_ARTIST", GST_TAG_ARTIST);
  PyModule_AddStringConstant (m, "TAG_ALBUM", GST_TAG_ALBUM);
  PyModule_AddStringConstant (m, "TAG_DATE", GST_TAG_DATE);
  PyModule_AddStringConstant (m, "TAG_GENRE", GST_TAG_GENRE);
  PyModule_AddStringConstant (m, "TAG_COMMENT", GST_TAG_COMMENT);
  PyModule_AddStringConstant (m, "TAG_TRACK_NUMBER", GST_TAG_TRACK_NUMBER);
  PyModule_AddStringConstant (m, "TAG_TRACK_COUNT", GST_TAG_TRACK_COUNT);
  PyModule_AddStringConstant (m, "TAG_ALBUM_VOLUME_NUMBER",
      GST_TAG_ALBUM_VOLUME_NUMBER);
  PyModule_AddStringConstant (m, "TAG_ALBUM_VOLUME_COUNT",
      GST_TAG_ALBUM_VOLUME_COUNT);
  PyModule_AddStringConstant (m, "TAG_LOCATION", GST_TAG_LOCATION);
  PyModule_AddStringConstant (m, "TAG_DESCRIPTION", GST_TAG_DESCRIPTION);
  PyModule_AddStringConstant (m, "TAG_VERSION", GST_TAG_VERSION);
  PyModule_AddStringConstant (m, "TAG_ISRC", GST_TAG_ISRC);
  PyModule_AddStringConstant (m, "TAG_ORGANIZATION", GST_TAG_ORGANIZATION);
  PyModule_AddStringConstant (m, "TAG_COPYRIGHT", GST_TAG_COPYRIGHT);
  PyModule_AddStringConstant (m, "TAG_CONTACT", GST_TAG_CONTACT);
  PyModule_AddStringConstant (m, "TAG_LICENSE", GST_TAG_LICENSE);
  PyModule_AddStringConstant (m, "TAG_PERFORMER", GST_TAG_PERFORMER);
  PyModule_AddStringConstant (m, "TAG_DURATION", GST_TAG_DURATION);
  PyModule_AddStringConstant (m, "TAG_CODEC", GST_TAG_CODEC);
  PyModule_AddStringConstant (m, "TAG_VIDEO_CODEC", GST_TAG_VIDEO_CODEC);
  PyModule_AddStringConstant (m, "TAG_AUDIO_CODEC", GST_TAG_AUDIO_CODEC);
  PyModule_AddStringConstant (m, "TAG_BITRATE", GST_TAG_BITRATE);
  PyModule_AddStringConstant (m, "TAG_NOMINAL_BITRATE",
      GST_TAG_NOMINAL_BITRATE);
  PyModule_AddStringConstant (m, "TAG_MINIMUM_BITRATE",
      GST_TAG_MINIMUM_BITRATE);
  PyModule_AddStringConstant (m, "TAG_MAXIMUM_BITRATE",
      GST_TAG_MAXIMUM_BITRATE);
  PyModule_AddStringConstant (m, "TAG_SERIAL", GST_TAG_SERIAL);
  PyModule_AddStringConstant (m, "TAG_ENCODER", GST_TAG_ENCODER);
  PyModule_AddStringConstant (m, "TAG_ENCODER_VERSION",
      GST_TAG_ENCODER_VERSION);
  PyModule_AddStringConstant (m, "TAG_TRACK_GAIN", GST_TAG_TRACK_GAIN);
  PyModule_AddStringConstant (m, "TAG_TRACK_PEAK", GST_TAG_TRACK_PEAK);
  PyModule_AddStringConstant (m, "TAG_ALBUM_GAIN", GST_TAG_ALBUM_GAIN);
  PyModule_AddStringConstant (m, "TAG_ALBUM_PEAK", GST_TAG_ALBUM_PEAK);
  PyModule_AddStringConstant (m, "TAG_LANGUAGE_CODE", GST_TAG_LANGUAGE_CODE);
  PyModule_AddStringConstant (m, "TAG_IMAGE", GST_TAG_IMAGE);
  PyModule_AddStringConstant (m, "TAG_PREVIEW_IMAGE", GST_TAG_PREVIEW_IMAGE);
  PyModule_AddStringConstant (m, "TAG_EXTENDED_COMMENT",
      GST_TAG_EXTENDED_COMMENT);
  PyModule_AddStringConstant (m, "TAG_REFERENCE_LEVEL",
      GST_TAG_REFERENCE_LEVEL);
  PyModule_AddStringConstant (m, "TAG_BEATS_PER_MINUTE",
      GST_TAG_BEATS_PER_MINUTE);
  PyModule_AddStringConstant (m, "TAG_LICENSE_URI", GST_TAG_LICENSE_URI);
  PyModule_AddStringConstant (m, "TAG_COPYRIGHT_URI", GST_TAG_COPYRIGHT_URI);
  PyModule_AddStringConstant (m, "TAG_COMPOSER", GST_TAG_COMPOSER);
  PyModule_AddStringConstant (m, "TAG_ARTIST_SORTNAME",
      GST_TAG_ARTIST_SORTNAME);
  PyModule_AddStringConstant (m, "TAG_ALBUM_SORTNAME", GST_TAG_ALBUM_SORTNAME);
  PyModule_AddStringConstant (m, "TAG_TITLE_SORTNAME", GST_TAG_TITLE_SORTNAME);
#if ((GST_VERSION_MICRO >= 21) || (GST_VERSION_MICRO == 20 && GST_VERSION_NANO > 0))
  PyModule_AddStringConstant (m, "TAG_ATTACHMENT", GST_TAG_ATTACHMENT);
  PyModule_AddStringConstant (m, "TAG_KEYWORDS", GST_TAG_KEYWORDS);
  PyModule_AddStringConstant (m, "TAG_GEO_LOCATION_NAME",
      GST_TAG_GEO_LOCATION_NAME);
  PyModule_AddStringConstant (m, "TAG_GEO_LOCATION_LATITUDE",
      GST_TAG_GEO_LOCATION_LATITUDE);
  PyModule_AddStringConstant (m, "TAG_GEO_LOCATION_LONGITUDE",
      GST_TAG_GEO_LOCATION_LONGITUDE);
  PyModule_AddStringConstant (m, "TAG_GEO_LOCATION_ELEVATION",
      GST_TAG_GEO_LOCATION_ELEVATION);
#if ((GST_VERSION_MICRO >= 23) || (GST_VERSION_MICRO == 22 && GST_VERSION_NANO > 0))
  PyModule_AddStringConstant (m, "TAG_SUBTITLE_CODEC", GST_TAG_SUBTITLE_CODEC);
  PyModule_AddStringConstant (m, "TAG_HOMEPAGE", GST_TAG_HOMEPAGE);
#if ((GST_VERSION_MICRO >= 24) || (GST_VERSION_MICRO == 23 && GST_VERSION_NANO > 0))
  PyModule_AddStringConstant (m, "TAG_CONTAINER_FORMAT",
      GST_TAG_CONTAINER_FORMAT);
#if ((GST_VERSION_MICRO >= 25) || (GST_VERSION_MICRO == 24 && GST_VERSION_NANO > 0))
  PyModule_AddStringConstant (m, "TAG_ALBUM_ARTIST", GST_TAG_ALBUM_ARTIST);
  PyModule_AddStringConstant (m, "TAG_ALBUM_ARTIST_SORTNAME",
      GST_TAG_ALBUM_ARTIST_SORTNAME);
#if ((GST_VERSION_MICRO >= 26) || (GST_VERSION_MICRO == 25 && GST_VERSION_NANO > 0))
  PyModule_AddStringConstant (m, "TAG_SHOW_NAME", GST_TAG_SHOW_NAME);
  PyModule_AddStringConstant (m, "TAG_SHOW_SORTNAME", GST_TAG_SHOW_SORTNAME);
  PyModule_AddStringConstant (m, "TAG_SHOW_EPISODE_NUMBER",
      GST_TAG_SHOW_EPISODE_NUMBER);
  PyModule_AddStringConstant (m, "TAG_SHOW_SEASON_NUMBER",
      GST_TAG_SHOW_SEASON_NUMBER);
  PyModule_AddStringConstant (m, "TAG_LYRICS", GST_TAG_LYRICS);
  PyModule_AddStringConstant (m, "TAG_COMPOSER_SORTNAME",
      GST_TAG_COMPOSER_SORTNAME);
  PyModule_AddStringConstant (m, "TAG_GROUPING", GST_TAG_GROUPING);
#if ((GST_VERSION_MICRO >= 29) || (GST_VERSION_MICRO == 28 && GST_VERSION_NANO > 0))
  PyModule_AddStringConstant (m, "TAG_GEO_LOCATION_COUNTRY",
      GST_TAG_GEO_LOCATION_COUNTRY);
  PyModule_AddStringConstant (m, "TAG_GEO_LOCATION_CITY",
      GST_TAG_GEO_LOCATION_CITY);
  PyModule_AddStringConstant (m, "TAG_GEO_LOCATION_SUBLOCATION",
      GST_TAG_GEO_LOCATION_SUBLOCATION);
  PyModule_AddStringConstant (m, "TAG_USER_RATING",
      GST_TAG_GEO_LOCATION_SUBLOCATION);
#if ((GST_VERSION_MICRO >= 30) || (GST_VERSION_MICRO == 29 && GST_VERSION_NANO > 0))
  PyModule_AddStringConstant (m, "TAG_DEVICE_MANUFACTURER",
      GST_TAG_DEVICE_MANUFACTURER);
  PyModule_AddStringConstant (m, "TAG_IMAGE_ORIENTATION",
      GST_TAG_IMAGE_ORIENTATION);
  PyModule_AddStringConstant (m, "TAG_DEVICE_MODEL", GST_TAG_DEVICE_MODEL);
  PyModule_AddStringConstant (m, "TAG_GEO_LOCATION_MOVEMENT_SPEED",
      GST_TAG_GEO_LOCATION_MOVEMENT_SPEED);
  PyModule_AddStringConstant (m, "TAG_GEO_LOCATION_MOVEMENT_DIRECTION",
      GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION);
  PyModule_AddStringConstant (m, "TAG_GEO_LOCATION_CAPTURE_DIRECTION",
      GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION);
#if ((GST_VERSION_MICRO >= 31) || (GST_VERSION_MICRO == 30 && GST_VERSION_NANO > 0))
#define ADD_FACTORY_TYPE(a) PyModule_AddObject(m, "ELEMENT_FACTORY_TYPE_" #a,\
      PyLong_FromUnsignedLongLong(GST_ELEMENT_FACTORY_TYPE_##a))
  ADD_FACTORY_TYPE (DECODER);
  ADD_FACTORY_TYPE (ENCODER);
  ADD_FACTORY_TYPE (SINK);
  ADD_FACTORY_TYPE (SRC);
  ADD_FACTORY_TYPE (MUXER);
  ADD_FACTORY_TYPE (DEMUXER);
  ADD_FACTORY_TYPE (PARSER);
  ADD_FACTORY_TYPE (PAYLOADER);
  ADD_FACTORY_TYPE (DEPAYLOADER);
  ADD_FACTORY_TYPE (FORMATTER);
  ADD_FACTORY_TYPE (MAX_ELEMENTS);
  ADD_FACTORY_TYPE (MEDIA_VIDEO);
  ADD_FACTORY_TYPE (MEDIA_AUDIO);
  ADD_FACTORY_TYPE (MEDIA_IMAGE);
  ADD_FACTORY_TYPE (MEDIA_SUBTITLE);
  ADD_FACTORY_TYPE (MEDIA_METADATA);
  ADD_FACTORY_TYPE (ANY);
  ADD_FACTORY_TYPE (MEDIA_ANY);
  ADD_FACTORY_TYPE (VIDEO_ENCODER);
  ADD_FACTORY_TYPE (AUDIO_ENCODER);
  ADD_FACTORY_TYPE (AUDIOVIDEO_SINKS);
  ADD_FACTORY_TYPE (DECODABLE);
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif

  PyModule_AddStringConstant (m, "LIBRARY_ERROR",
      (gchar *) g_quark_to_string (GST_LIBRARY_ERROR));
  PyModule_AddStringConstant (m, "RESOURCE_ERROR",
      (gchar *) g_quark_to_string (GST_RESOURCE_ERROR));
  PyModule_AddStringConstant (m, "CORE_ERROR",
      (gchar *) g_quark_to_string (GST_CORE_ERROR));
  PyModule_AddStringConstant (m, "STREAM_ERROR",
      (gchar *) g_quark_to_string (GST_STREAM_ERROR));

  pyg_register_gtype_custom (GST_TYPE_FRACTION,
      pygst_fraction_from_value, pygst_fraction_to_value);

  if (PyErr_Occurred ()) {
    Py_FatalError ("can't initialize module gst");
  }
}
