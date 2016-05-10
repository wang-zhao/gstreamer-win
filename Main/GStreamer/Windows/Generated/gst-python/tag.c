/* -- THIS FILE IS GENERATED - DO NOT EDIT *//* -*- Mode: C; c-basic-offset: 4 -*- */

#include <Python.h>



#line 22 "..\\..\\..\\Source\\gst-python\\gst\\tag.override"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define NO_IMPORT_PYGOBJECT
#include "pygst.h"

#include <gst/gst.h>

#include <gst/tag/tag.h>
#include <gst/tag/gsttagdemux.h>
#include "pygstminiobject.h"
GST_DEBUG_CATEGORY_EXTERN (pygst_debug);
#define GST_CAT_DEFAULT pygst_debug

/* Boonky define that allows for backwards compatibility with Python 2.4 */
#if PY_VERSION_HEX < 0x02050000
#define Py_ssize_t int
#endif

#line 30 "..\\..\\..\\Source\\gst-python\\gst\\tag.c"


/* ---------- types from other modules ---------- */
static PyTypeObject *_PyGObject_Type;
#define PyGObject_Type (*_PyGObject_Type)
static PyTypeObject *_PyGstObject_Type;
#define PyGstObject_Type (*_PyGstObject_Type)
static PyTypeObject *_PyGstStructure_Type;
#define PyGstStructure_Type (*_PyGstStructure_Type)
static PyTypeObject *_PyGstElement_Type;
#define PyGstElement_Type (*_PyGstElement_Type)
static PyTypeObject *_PyGstPad_Type;
#define PyGstPad_Type (*_PyGstPad_Type)
static PyTypeObject *_PyGstBuffer_Type;
#define PyGstBuffer_Type (*_PyGstBuffer_Type)
static PyTypeObject *_PyGstMessage_Type;
#define PyGstMessage_Type (*_PyGstMessage_Type)
static PyTypeObject *_PyGstSystemClock_Type;
#define PyGstSystemClock_Type (*_PyGstSystemClock_Type)
static PyTypeObject *_PyGstBaseTransform_Type;
#define PyGstBaseTransform_Type (*_PyGstBaseTransform_Type)
static PyTypeObject *_PyGstBaseSink_Type;
#define PyGstBaseSink_Type (*_PyGstBaseSink_Type)


/* ---------- forward type declarations ---------- */
PyTypeObject PyGstTagDemux_Type;

#line 59 "..\\..\\..\\Source\\gst-python\\gst\\tag.c"



/* ----------- GstTagDemux ----------- */

PyTypeObject PyGstTagDemux_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                 /* ob_size */
    "gst.tag.TagDemux",                   /* tp_name */
    sizeof(PyGObject),          /* tp_basicsize */
    0,                                 /* tp_itemsize */
    /* methods */
    (destructor)0,        /* tp_dealloc */
    (printfunc)0,                      /* tp_print */
    (getattrfunc)0,       /* tp_getattr */
    (setattrfunc)0,       /* tp_setattr */
    (cmpfunc)0,           /* tp_compare */
    (reprfunc)0,             /* tp_repr */
    (PyNumberMethods*)0,     /* tp_as_number */
    (PySequenceMethods*)0, /* tp_as_sequence */
    (PyMappingMethods*)0,   /* tp_as_mapping */
    (hashfunc)0,             /* tp_hash */
    (ternaryfunc)0,          /* tp_call */
    (reprfunc)0,              /* tp_str */
    (getattrofunc)0,     /* tp_getattro */
    (setattrofunc)0,     /* tp_setattro */
    (PyBufferProcs*)0,  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,                      /* tp_flags */
    NULL,                        /* Documentation string */
    (traverseproc)0,     /* tp_traverse */
    (inquiry)0,             /* tp_clear */
    (richcmpfunc)0,   /* tp_richcompare */
    offsetof(PyGObject, weakreflist),             /* tp_weaklistoffset */
    (getiterfunc)0,          /* tp_iter */
    (iternextfunc)0,     /* tp_iternext */
    (struct PyMethodDef*)NULL, /* tp_methods */
    (struct PyMemberDef*)0,              /* tp_members */
    (struct PyGetSetDef*)0,  /* tp_getset */
    NULL,                              /* tp_base */
    NULL,                              /* tp_dict */
    (descrgetfunc)0,    /* tp_descr_get */
    (descrsetfunc)0,    /* tp_descr_set */
    offsetof(PyGObject, inst_dict),                 /* tp_dictoffset */
    (initproc)0,             /* tp_init */
    (allocfunc)0,           /* tp_alloc */
    (newfunc)0,               /* tp_new */
    (freefunc)0,             /* tp_free */
    (inquiry)0              /* tp_is_gc */
};



/* ----------- functions ----------- */

static PyObject *
_wrap_gst_tag_from_vorbis_tag(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "vorbis_tag", NULL };
    char *vorbis_tag;
    const gchar *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"s:from_vorbis_tag", kwlist, &vorbis_tag))
        return NULL;
    pyg_begin_allow_threads;
    ret = gst_tag_from_vorbis_tag(vorbis_tag);
    pyg_end_allow_threads;
    if (ret)
        return PyString_FromString(ret);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_gst_tag_to_vorbis_tag(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "gst_tag", NULL };
    char *gst_tag;
    const gchar *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"s:to_vorbis_tag", kwlist, &gst_tag))
        return NULL;
    pyg_begin_allow_threads;
    ret = gst_tag_to_vorbis_tag(gst_tag);
    pyg_end_allow_threads;
    if (ret)
        return PyString_FromString(ret);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_gst_vorbis_tag_add(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "list", "tag", "value", NULL };
    GstTagList *list = NULL;
    PyObject *py_list;
    char *tag, *value;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"Oss:gst_vorbis_tag_add", kwlist, &py_list, &tag, &value))
        return NULL;
    if (pyg_boxed_check(py_list, GST_TYPE_TAG_LIST))
        list = pyg_boxed_get(py_list, GstTagList);
    else {
        PyErr_SetString(PyExc_TypeError, "list should be a GstTagList");
        return NULL;
    }
    pyg_begin_allow_threads;
    gst_vorbis_tag_add(list, tag, value);
    pyg_end_allow_threads;
    Py_INCREF(Py_None);
    return Py_None;
}

#line 68 "..\\..\\..\\Source\\gst-python\\gst\\tag.override"
static PyObject *
_wrap_gst_tag_to_vorbis_comments(PyObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *py_taglist;
    const GstTagList *taglist;
    const gchar *tag;

    const GList *list;
    const GList *l;
    PyObject *py_list;

    if (!PyArg_ParseTuple(args, "Os", &py_taglist, &tag))
        return NULL;

    if (pyg_boxed_check(py_taglist, GST_TYPE_TAG_LIST))
        taglist = pyg_boxed_get(py_taglist, GstTagList);
    else {
        PyErr_SetString(PyExc_TypeError, "list should be a GstTagList");
        return NULL;
    }


    pyg_begin_allow_threads;
    list = gst_tag_to_vorbis_comments (taglist, tag);
    pyg_end_allow_threads;

    py_list = PyList_New(0);

    for (l = list; l; l = l->next) {
        gchar *pair = (gchar *)l->data;
        PyObject *py_pair = PyString_FromString(pair);
        PyList_Append(py_list, py_pair);
        Py_DECREF(py_pair);
    }
    return py_list;

} 
#line 211 "..\\..\\..\\Source\\gst-python\\gst\\tag.c"


static PyObject *
_wrap_gst_tag_id3_genre_count(PyObject *self)
{
    guint ret;

    pyg_begin_allow_threads;
    ret = gst_tag_id3_genre_count();
    pyg_end_allow_threads;
    return PyLong_FromUnsignedLong(ret);
}

static PyObject *
_wrap_gst_tag_from_id3_tag(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "id3_tag", NULL };
    char *id3_tag;
    const gchar *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"s:from_id3_tag", kwlist, &id3_tag))
        return NULL;
    pyg_begin_allow_threads;
    ret = gst_tag_from_id3_tag(id3_tag);
    pyg_end_allow_threads;
    if (ret)
        return PyString_FromString(ret);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_gst_tag_from_id3_user_tag(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "type", "id3_user_tag", NULL };
    char *type, *id3_user_tag;
    const gchar *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"ss:from_id3_user_tag", kwlist, &type, &id3_user_tag))
        return NULL;
    pyg_begin_allow_threads;
    ret = gst_tag_from_id3_user_tag(type, id3_user_tag);
    pyg_end_allow_threads;
    if (ret)
        return PyString_FromString(ret);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_gst_tag_to_id3_tag(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "gst_tag", NULL };
    char *gst_tag;
    const gchar *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"s:to_id3_tag", kwlist, &gst_tag))
        return NULL;
    pyg_begin_allow_threads;
    ret = gst_tag_to_id3_tag(gst_tag);
    pyg_end_allow_threads;
    if (ret)
        return PyString_FromString(ret);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_gst_tag_list_from_exif_buffer(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "buffer", "byte_order", "base_offset", NULL };
    PyObject *py_buffer;
    int byte_order;
    GstTagList *ret;
    unsigned long base_offset;
    const GstBuffer *buffer;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"Oik:tag_list_from_exif_buffer", kwlist, &py_buffer, &byte_order, &base_offset))
        return NULL;
    buffer = GST_BUFFER(pygstminiobject_get (py_buffer));
    if (PyErr_Occurred())
      return NULL;
    pyg_begin_allow_threads;
    ret = gst_tag_list_from_exif_buffer(buffer, byte_order, base_offset);
    pyg_end_allow_threads;
    /* pyg_boxed_new handles NULL checking */
    return pyg_boxed_new(GST_TYPE_TAG_LIST, (GstTagList*) ret, TRUE, TRUE);
}

static PyObject *
_wrap_gst_tag_list_from_exif_buffer_with_tiff_header(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "buffer", NULL };
    PyObject *py_buffer;
    GstTagList *ret;
    const GstBuffer *buffer;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"O:tag_list_from_exif_buffer_with_tiff_header", kwlist, &py_buffer))
        return NULL;
    buffer = GST_BUFFER(pygstminiobject_get (py_buffer));
    if (PyErr_Occurred())
      return NULL;
    pyg_begin_allow_threads;
    ret = gst_tag_list_from_exif_buffer_with_tiff_header(buffer);
    pyg_end_allow_threads;
    /* pyg_boxed_new handles NULL checking */
    return pyg_boxed_new(GST_TYPE_TAG_LIST, (GstTagList*) ret, TRUE, TRUE);
}

static PyObject *
_wrap_gst_tag_register_musicbrainz_tags(PyObject *self)
{
    pyg_begin_allow_threads;
    gst_tag_register_musicbrainz_tags();
    pyg_end_allow_threads;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_gst_tag_get_language_name(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "language_code", NULL };
    char *language_code;
    const gchar *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"s:get_language_name", kwlist, &language_code))
        return NULL;
    pyg_begin_allow_threads;
    ret = gst_tag_get_language_name(language_code);
    pyg_end_allow_threads;
    if (ret)
        return PyString_FromString(ret);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_gst_tag_get_language_code_iso_639_1(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "lang_code", NULL };
    char *lang_code;
    const gchar *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"s:get_language_code_iso_639_1", kwlist, &lang_code))
        return NULL;
    pyg_begin_allow_threads;
    ret = gst_tag_get_language_code_iso_639_1(lang_code);
    pyg_end_allow_threads;
    if (ret)
        return PyString_FromString(ret);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_gst_tag_get_language_code_iso_639_2B(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "lang_code", NULL };
    char *lang_code;
    const gchar *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"s:get_language_code_iso_639_2B", kwlist, &lang_code))
        return NULL;
    pyg_begin_allow_threads;
    ret = gst_tag_get_language_code_iso_639_2B(lang_code);
    pyg_end_allow_threads;
    if (ret)
        return PyString_FromString(ret);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_gst_tag_get_language_code_iso_639_2T(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "lang_code", NULL };
    char *lang_code;
    const gchar *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"s:get_language_code_iso_639_2T", kwlist, &lang_code))
        return NULL;
    pyg_begin_allow_threads;
    ret = gst_tag_get_language_code_iso_639_2T(lang_code);
    pyg_end_allow_threads;
    if (ret)
        return PyString_FromString(ret);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_gst_tag_list_from_xmp_buffer(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "buffer", NULL };
    PyObject *py_buffer;
    GstTagList *ret;
    const GstBuffer *buffer;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"O:tag_list_from_xmp_buffer", kwlist, &py_buffer))
        return NULL;
    buffer = GST_BUFFER(pygstminiobject_get (py_buffer));
    if (PyErr_Occurred())
      return NULL;
    pyg_begin_allow_threads;
    ret = gst_tag_list_from_xmp_buffer(buffer);
    pyg_end_allow_threads;
    /* pyg_boxed_new handles NULL checking */
    return pyg_boxed_new(GST_TYPE_TAG_LIST, (GstTagList*) ret, TRUE, TRUE);
}

static PyObject *
_wrap_gst_tag_list_to_xmp_buffer(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "taglist", "readonly", NULL };
    GstTagList *taglist = NULL;
    PyObject *py_taglist;
    int readonly;
    GstBuffer *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,"Oi:tag_list_to_xmp_buffer", kwlist, &py_taglist, &readonly))
        return NULL;
    if (pyg_boxed_check(py_taglist, GST_TYPE_TAG_LIST))
        taglist = pyg_boxed_get(py_taglist, GstTagList);
    else {
        PyErr_SetString(PyExc_TypeError, "taglist should be a GstTagList");
        return NULL;
    }
    pyg_begin_allow_threads;
    ret = gst_tag_list_to_xmp_buffer(taglist, readonly);
    pyg_end_allow_threads;
    /* pygobject_new handles NULL checking */
    return pygstminiobject_new((GstMiniObject *)ret);
}

const PyMethodDef pytag_functions[] = {
    { "from_vorbis_tag", (PyCFunction)_wrap_gst_tag_from_vorbis_tag, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "to_vorbis_tag", (PyCFunction)_wrap_gst_tag_to_vorbis_tag, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "gst_vorbis_tag_add", (PyCFunction)_wrap_gst_vorbis_tag_add, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "to_vorbis_comments", (PyCFunction)_wrap_gst_tag_to_vorbis_comments, METH_VARARGS,
      NULL },
    { "id3_genre_count", (PyCFunction)_wrap_gst_tag_id3_genre_count, METH_NOARGS,
      NULL },
    { "from_id3_tag", (PyCFunction)_wrap_gst_tag_from_id3_tag, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "from_id3_user_tag", (PyCFunction)_wrap_gst_tag_from_id3_user_tag, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "to_id3_tag", (PyCFunction)_wrap_gst_tag_to_id3_tag, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "tag_list_from_exif_buffer", (PyCFunction)_wrap_gst_tag_list_from_exif_buffer, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "tag_list_from_exif_buffer_with_tiff_header", (PyCFunction)_wrap_gst_tag_list_from_exif_buffer_with_tiff_header, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "register_musicbrainz_tags", (PyCFunction)_wrap_gst_tag_register_musicbrainz_tags, METH_NOARGS,
      NULL },
    { "get_language_name", (PyCFunction)_wrap_gst_tag_get_language_name, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "get_language_code_iso_639_1", (PyCFunction)_wrap_gst_tag_get_language_code_iso_639_1, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "get_language_code_iso_639_2B", (PyCFunction)_wrap_gst_tag_get_language_code_iso_639_2B, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "get_language_code_iso_639_2T", (PyCFunction)_wrap_gst_tag_get_language_code_iso_639_2T, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "tag_list_from_xmp_buffer", (PyCFunction)_wrap_gst_tag_list_from_xmp_buffer, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { "tag_list_to_xmp_buffer", (PyCFunction)_wrap_gst_tag_list_to_xmp_buffer, METH_VARARGS|METH_KEYWORDS,
      NULL },
    { NULL, NULL, 0, NULL }
};


/* ----------- enums and flags ----------- */

void
pytag_add_constants(PyObject *module, const gchar *strip_prefix)
{
  pyg_enum_add(module, "TagDemuxResult", strip_prefix, GST_TYPE_TAG_DEMUX_RESULT);
  pyg_enum_add(module, "TagImageType", strip_prefix, GST_TYPE_TAG_IMAGE_TYPE);

  if (PyErr_Occurred())
    PyErr_Print();
}

/* initialise stuff extension classes */
void
pytag_register_classes(PyObject *d)
{
    PyObject *module;

    if ((module = PyImport_ImportModule("gobject")) != NULL) {
        PyObject *moddict = PyModule_GetDict(module);

        _PyGObject_Type = (PyTypeObject *)PyDict_GetItemString(moddict, "GObject");
        if (_PyGObject_Type == NULL) {
            PyErr_SetString(PyExc_ImportError,
                "cannot import name GObject from gobject");
            return;
        }
    } else {
        PyErr_SetString(PyExc_ImportError,
            "could not import gobject");
        return;
    }
    if ((module = PyImport_ImportModule("gst")) != NULL) {
        PyObject *moddict = PyModule_GetDict(module);

        _PyGstObject_Type = (PyTypeObject *)PyDict_GetItemString(moddict, "Object");
        if (_PyGstObject_Type == NULL) {
            PyErr_SetString(PyExc_ImportError,
                "cannot import name Object from gst");
            return;
        }
        _PyGstStructure_Type = (PyTypeObject *)PyDict_GetItemString(moddict, "Structure");
        if (_PyGstStructure_Type == NULL) {
            PyErr_SetString(PyExc_ImportError,
                "cannot import name Structure from gst");
            return;
        }
        _PyGstElement_Type = (PyTypeObject *)PyDict_GetItemString(moddict, "Element");
        if (_PyGstElement_Type == NULL) {
            PyErr_SetString(PyExc_ImportError,
                "cannot import name Element from gst");
            return;
        }
        _PyGstPad_Type = (PyTypeObject *)PyDict_GetItemString(moddict, "Pad");
        if (_PyGstPad_Type == NULL) {
            PyErr_SetString(PyExc_ImportError,
                "cannot import name Pad from gst");
            return;
        }
        _PyGstBuffer_Type = (PyTypeObject *)PyDict_GetItemString(moddict, "Buffer");
        if (_PyGstBuffer_Type == NULL) {
            PyErr_SetString(PyExc_ImportError,
                "cannot import name Buffer from gst");
            return;
        }
        _PyGstMessage_Type = (PyTypeObject *)PyDict_GetItemString(moddict, "Message");
        if (_PyGstMessage_Type == NULL) {
            PyErr_SetString(PyExc_ImportError,
                "cannot import name Message from gst");
            return;
        }
        _PyGstSystemClock_Type = (PyTypeObject *)PyDict_GetItemString(moddict, "SystemClock");
        if (_PyGstSystemClock_Type == NULL) {
            PyErr_SetString(PyExc_ImportError,
                "cannot import name SystemClock from gst");
            return;
        }
        _PyGstBaseTransform_Type = (PyTypeObject *)PyDict_GetItemString(moddict, "BaseTransform");
        if (_PyGstBaseTransform_Type == NULL) {
            PyErr_SetString(PyExc_ImportError,
                "cannot import name BaseTransform from gst");
            return;
        }
        _PyGstBaseSink_Type = (PyTypeObject *)PyDict_GetItemString(moddict, "BaseSink");
        if (_PyGstBaseSink_Type == NULL) {
            PyErr_SetString(PyExc_ImportError,
                "cannot import name BaseSink from gst");
            return;
        }
    } else {
        PyErr_SetString(PyExc_ImportError,
            "could not import gst");
        return;
    }


#line 582 "..\\..\\..\\Source\\gst-python\\gst\\tag.c"
    pygobject_register_class(d, "GstTagDemux", GST_TYPE_TAG_DEMUX, &PyGstTagDemux_Type, Py_BuildValue("(O)", &PyGstElement_Type));
}
