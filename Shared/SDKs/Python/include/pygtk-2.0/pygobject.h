/* -*- Mode: C; c-basic-offset: 4 -*- */
#ifndef _PYGOBJECT_H_
#define _PYGOBJECT_H_

#include <Python.h>

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* This is deprecated, don't use */
#define PYGIL_API_IS_BUGGY FALSE

  /* PyGClosure is a _private_ structure */
typedef void (* PyClosureExceptionHandler) (GValue *ret, guint n_param_values, const GValue *params);
typedef struct _PyGClosure PyGClosure;
typedef struct _PyGObjectData PyGObjectData;

struct _PyGClosure {
    GClosure closure;
    PyObject *callback;
    PyObject *extra_args; /* tuple of extra args to pass to callback */
    PyObject *swap_data; /* other object for gtk_signal_connect_object */
    PyClosureExceptionHandler exception_handler;
};

typedef enum {
    PYGOBJECT_USING_TOGGLE_REF = 1 << 0
} PyGObjectFlags;

  /* closures is just an alias for what is found in the
   * PyGObjectData */
typedef struct {
    PyObject_HEAD
    GObject *obj;
    PyObject *inst_dict; /* the instance dictionary -- must be last */
    PyObject *weakreflist; /* list of weak references */
    
      /*< private >*/
      /* using union to preserve ABI compatibility (structure size
       * must not change) */
    union {
        GSList *closures; /* stale field; no longer updated DO-NOT-USE! */
        PyGObjectFlags flags;
    } private_flags;

} PyGObject;

#define pygobject_get(v) (((PyGObject *)(v))->obj)
#define pygobject_check(v,base) (PyObject_TypeCheck(v,base))

typedef struct {
    PyObject_HEAD
    gpointer boxed;
    GType gtype;
    gboolean free_on_dealloc;
} PyGBoxed;

#define pyg_boxed_get(v,t)      ((t *)((PyGBoxed *)(v))->boxed)
#define pyg_boxed_check(v,typecode) (PyObject_TypeCheck(v, &PyGBoxed_Type) && ((PyGBoxed *)(v))->gtype == typecode)

typedef struct {
    PyObject_HEAD
    gpointer pointer;
    GType gtype;
} PyGPointer;

#define pyg_pointer_get(v,t)      ((t *)((PyGPointer *)(v))->pointer)
#define pyg_pointer_check(v,typecode) (PyObject_TypeCheck(v, &PyGPointer_Type) && ((PyGPointer *)(v))->gtype == typecode)

typedef void (*PyGFatalExceptionFunc) (void);
typedef void (*PyGThreadBlockFunc) (void);

typedef struct {
    PyObject_HEAD
    GParamSpec *pspec;
} PyGParamSpec;

#define PyGParamSpec_Get(v) (((PyGParamSpec *)v)->pspec)
#define PyGParamSpec_Check(v) (PyObject_TypeCheck(v, &PyGParamSpec_Type))

typedef int (*PyGClassInitFunc) (gpointer gclass, PyTypeObject *pyclass);
typedef PyTypeObject * (*PyGTypeRegistrationFunction) (const gchar *name,
						       gpointer data);

struct _PyGObject_Functions {
    /* 
     * All field names in here are considered private,
     * use the macros below instead, which provides stability
     */
    void (* register_class)(PyObject *dict, const gchar *class_name,
			    GType gtype, PyTypeObject *type, PyObject *bases);
    void (* register_wrapper)(PyObject *self);
    void (* register_sinkfunc)(GType type,
			       void (* sinkfunc)(GObject *object));
    PyTypeObject *(* lookup_class)(GType type);
    PyObject *(* newgobj)(GObject *obj);

    GClosure *(* closure_new)(PyObject *callback, PyObject *extra_args,
			      PyObject *swap_data);
    void      (* object_watch_closure)(PyObject *self, GClosure *closure);
    GDestroyNotify destroy_notify;

    GType (* type_from_object)(PyObject *obj);
    PyObject *(* type_wrapper_new)(GType type);

    gint (* enum_get_value)(GType enum_type, PyObject *obj, gint *val);
    gint (* flags_get_value)(GType flag_type, PyObject *obj, gint *val);
    void (* register_gtype_custom)(GType gtype,
			    PyObject *(* from_func)(const GValue *value),
			    int (* to_func)(GValue *value, PyObject *obj));
    int (* value_from_pyobject)(GValue *value, PyObject *obj);
    PyObject *(* value_as_pyobject)(const GValue *value, gboolean copy_boxed);

    void (* register_interface)(PyObject *dict, const gchar *class_name,
				GType gtype, PyTypeObject *type);

    PyTypeObject *boxed_type;
    void (* register_boxed)(PyObject *dict, const gchar *class_name,
			    GType boxed_type, PyTypeObject *type);
    PyObject *(* boxed_new)(GType boxed_type, gpointer boxed,
			    gboolean copy_boxed, gboolean own_ref);

    PyTypeObject *pointer_type;
    void (* register_pointer)(PyObject *dict, const gchar *class_name,
			      GType pointer_type, PyTypeObject *type);
    PyObject *(* pointer_new)(GType boxed_type, gpointer pointer);

    void (* enum_add_constants)(PyObject *module, GType enum_type,
				const gchar *strip_prefix);
    void (* flags_add_constants)(PyObject *module, GType flags_type,
				 const gchar *strip_prefix);

    const gchar *(* constant_strip_prefix)(const gchar *name,
				     const gchar *strip_prefix);

    gboolean (* error_check)(GError **error);

    /* hooks to register handlers for getting GDK threads to cooperate
     * with python threading */
    void (* set_thread_block_funcs) (PyGThreadBlockFunc block_threads_func,
				     PyGThreadBlockFunc unblock_threads_func);
    PyGThreadBlockFunc block_threads;
    PyGThreadBlockFunc unblock_threads;
    PyTypeObject *paramspec_type;
    PyObject *(* paramspec_new)(GParamSpec *spec);
    GParamSpec *(*paramspec_get)(PyObject *tuple);
    int (*pyobj_to_unichar_conv)(PyObject *pyobj, void* ptr);
    gboolean (*parse_constructor_args)(GType        obj_type,
                                       char       **arg_names,
                                       char       **prop_names,
                                       GParameter  *params,
                                       guint       *nparams,
                                       PyObject   **py_args);
    PyObject *(* param_gvalue_as_pyobject) (const GValue* gvalue, 
                                            gboolean copy_boxed,
					    const GParamSpec* pspec);
    int (* gvalue_from_param_pyobject) (GValue* value, 
                                        PyObject* py_obj, 
					const GParamSpec* pspec);
    PyTypeObject *enum_type;
    PyObject *(*enum_add)(PyObject *module,
			  const char *type_name_,
			  const char *strip_prefix,
			  GType gtype);
    PyObject* (*enum_from_gtype)(GType gtype, int value);
    
    PyTypeObject *flags_type;
    PyObject *(*flags_add)(PyObject *module,
			   const char *type_name_,
			   const char *strip_prefix,
			   GType gtype);
    PyObject* (*flags_from_gtype)(GType gtype, int value);

    gboolean threads_enabled;
    int       (*enable_threads) (void);

    /* These 2 are deprecated */
    int       (*gil_state_ensure) (void);
    void      (*gil_state_release) (int flag);
    
    void      (*register_class_init) (GType gtype, PyGClassInitFunc class_init);
    void      (*register_interface_info) (GType gtype, const GInterfaceInfo *info);
    void      (*closure_set_exception_handler) (GClosure *closure, PyClosureExceptionHandler handler);
    int       (*pygobject_constructv) (PyGObject  *self,
                                       guint       n_parameters,
                                       GParameter *parameters);
    int       (*pygobject_construct) (PyGObject  *self,
                                      const char *first_property_name,
                                      ...);
    void      (*set_object_has_new_constructor) (GType type);

    void      (*add_warning_redirection) (const char *domain,
                                          PyObject   *warning);
    void      (*disable_warning_redirections) (void);
    void      (*type_register_custom)(const gchar *type_name,
				      PyGTypeRegistrationFunction callback,
				      gpointer data);
    gboolean  (*gerror_exception_check) (GError **error);

};

#ifndef _INSIDE_PYGOBJECT_

#if defined(NO_IMPORT) || defined(NO_IMPORT_PYGOBJECT)
extern struct _PyGObject_Functions *_PyGObject_API;
#else
struct _PyGObject_Functions *_PyGObject_API;
#endif

#define pygobject_register_class    (_PyGObject_API->register_class)
#define pygobject_register_wrapper  (_PyGObject_API->register_wrapper)
#define pygobject_register_sinkfunc (_PyGObject_API->register_sinkfunc)
#define pygobject_lookup_class      (_PyGObject_API->lookup_class)
#define pygobject_new               (_PyGObject_API->newgobj)
#define pyg_closure_new             (_PyGObject_API->closure_new)
#define pygobject_watch_closure     (_PyGObject_API->object_watch_closure)
#define pyg_closure_set_exception_handler (_PyGObject_API->closure_set_exception_handler)
#define pyg_destroy_notify          (_PyGObject_API->destroy_notify)
#define pyg_type_from_object        (_PyGObject_API->type_from_object)
#define pyg_type_wrapper_new        (_PyGObject_API->type_wrapper_new)
#define pyg_enum_get_value          (_PyGObject_API->enum_get_value)
#define pyg_flags_get_value         (_PyGObject_API->flags_get_value)
/* This is deprecated, call pyg_register_gtype_custom directly instead */
#define pyg_register_boxed_custom   pyg_register_gtype_custom
#define pyg_register_gtype_custom   (_PyGObject_API->register_gtype_custom)
#define pyg_value_from_pyobject     (_PyGObject_API->value_from_pyobject)
#define pyg_value_as_pyobject       (_PyGObject_API->value_as_pyobject)
#define pyg_register_interface      (_PyGObject_API->register_interface)
#define PyGBoxed_Type               (*_PyGObject_API->boxed_type)
#define pyg_register_boxed          (_PyGObject_API->register_boxed)
#define pyg_boxed_new               (_PyGObject_API->boxed_new)
#define PyGPointer_Type             (*_PyGObject_API->pointer_type)
#define pyg_register_pointer        (_PyGObject_API->register_pointer)
#define pyg_pointer_new             (_PyGObject_API->pointer_new)
#define pyg_enum_add_constants      (_PyGObject_API->enum_add_constants)
#define pyg_flags_add_constants     (_PyGObject_API->flags_add_constants)
#define pyg_constant_strip_prefix   (_PyGObject_API->constant_strip_prefix)
#define pyg_error_check             (_PyGObject_API->error_check)
#define pyg_set_thread_block_funcs  (_PyGObject_API->set_thread_block_funcs)
#define PyGParamSpec_Type           (*_PyGObject_API->paramspec_type)
#define pyg_param_spec_new          (_PyGObject_API->paramspec_new)
#define pyg_param_spec_from_object  (_PyGObject_API->paramspec_get)
#define pyg_pyobj_to_unichar_conv   (_PyGObject_API->pyobj_to_unichar_conv)
#define pyg_parse_constructor_args  (_PyGObject_API->parse_constructor_args)
#define pyg_param_gvalue_as_pyobject   (_PyGObject_API->value_as_pyobject)
#define pyg_param_gvalue_from_pyobject (_PyGObject_API->gvalue_from_param_pyobject)
#define PyGEnum_Type                (*_PyGObject_API->enum_type)
#define pyg_enum_add                (_PyGObject_API->enum_add)
#define pyg_enum_from_gtype         (_PyGObject_API->enum_from_gtype)
#define PyGFlags_Type               (*_PyGObject_API->flags_type)
#define pyg_flags_add               (_PyGObject_API->flags_add)
#define pyg_flags_from_gtype        (_PyGObject_API->flags_from_gtype)
#define pyg_enable_threads          (_PyGObject_API->enable_threads)
#define pyg_register_class_init     (_PyGObject_API->register_class_init)
#define pyg_register_interface_info (_PyGObject_API->register_interface_info)
#define pygobject_construct         (_PyGObject_API->pygobject_construct)
#define pygobject_constructv        (_PyGObject_API->pygobject_constructv)
#define pyg_set_object_has_new_constructor (_PyGObject_API->set_object_has_new_constructor)
#define pyg_add_warning_redirection   (_PyGObject_API->add_warning_redirection)
#define pyg_disable_warning_redirections (_PyGObject_API->disable_warning_redirections)
#define pyg_type_register_custom_callback (_PyGObject_API->type_register_custom)
#define pyg_gerror_exception_check (_PyGObject_API->gerror_exception_check)


#define pyg_block_threads()   G_STMT_START {   \
    if (_PyGObject_API->block_threads != NULL) \
      (* _PyGObject_API->block_threads)();     \
  } G_STMT_END
#define pyg_unblock_threads() G_STMT_START {     \
    if (_PyGObject_API->unblock_threads != NULL) \
      (* _PyGObject_API->unblock_threads)();     \
  } G_STMT_END

#define pyg_threads_enabled (_PyGObject_API->threads_enabled)

#define pyg_gil_state_ensure() \
    (_PyGObject_API->threads_enabled ? \
      (PyGILState_Ensure()) : 0)
#define pyg_gil_state_release(state) G_STMT_START {     \
    if (_PyGObject_API->threads_enabled)                \
        PyGILState_Release(state);                      \
    } G_STMT_END

#define pyg_begin_allow_threads                 \
    G_STMT_START {                              \
        PyThreadState *_save = NULL;            \
        if (_PyGObject_API->threads_enabled)    \
            _save = PyEval_SaveThread();
#define pyg_end_allow_threads                   \
        if (_PyGObject_API->threads_enabled)    \
            PyEval_RestoreThread(_save);        \
    } G_STMT_END


/**
 * pygobject_init:
 * @req_major: minimum version major number, or -1
 * @req_minor: minimum version minor number, or -1
 * @req_micro: minimum version micro number, or -1
 * 
 * Imports and initializes the 'gobject' python module.  Can
 * optionally check for a required minimum version if @req_major,
 * @req_minor, and @req_micro are all different from -1.
 * 
 * Returns: a new reference to the gobject module on success, NULL in
 * case of failure (and raises ImportError).
 **/
static inline PyObject *
pygobject_init(int req_major, int req_minor, int req_micro)
{
    PyObject *gobject, *cobject;
    
    gobject = PyImport_ImportModule("gobject");
    if (!gobject) {
        if (PyErr_Occurred())
        {
            PyObject *type, *value, *traceback;
            PyObject *py_orig_exc;
            PyErr_Fetch(&type, &value, &traceback);
            py_orig_exc = PyObject_Repr(value);
            Py_XDECREF(type);
            Py_XDECREF(value);
            Py_XDECREF(traceback);
            PyErr_Format(PyExc_ImportError,
                         "could not import gobject (error was: %s)",
                         PyString_AsString(py_orig_exc));
            Py_DECREF(py_orig_exc);
        } else {
            PyErr_SetString(PyExc_ImportError,
                            "could not import gobject (no error given)");
        }
        return NULL;
    }

    cobject = PyObject_GetAttrString(gobject, "_PyGObject_API");
    if (cobject && PyCObject_Check(cobject))
        _PyGObject_API = (struct _PyGObject_Functions *) PyCObject_AsVoidPtr(cobject);
    else {
        PyErr_SetString(PyExc_ImportError,
                        "could not import gobject (could not find _PyGObject_API object)");
        Py_DECREF(gobject);
        return NULL;
    }

    if (req_major != -1)
    {
        int found_major, found_minor, found_micro;
        PyObject *version;

        version = PyObject_GetAttrString(gobject, "pygobject_version");
        if (!version)
            version = PyObject_GetAttrString(gobject, "pygtk_version");
        if (!version) {
            PyErr_SetString(PyExc_ImportError,
                            "could not import gobject (version too old)");
            Py_DECREF(gobject);
            return NULL;
        }
        if (!PyArg_ParseTuple(version, "iii",
                              &found_major, &found_minor, &found_micro)) {
            PyErr_SetString(PyExc_ImportError,
                            "could not import gobject (version has invalid format)");
            Py_DECREF(version);
            Py_DECREF(gobject);
            return NULL;
        }
        Py_DECREF(version);
        if (req_major != found_major ||
            req_minor >  found_minor ||
            (req_minor == found_minor && req_micro > found_micro)) {
            PyErr_Format(PyExc_ImportError,
                         "could not import gobject (version mismatch, %d.%d.%d is required, "
                         "found %d.%d.%d)", req_major, req_minor, req_micro,
                         found_major, found_minor, found_micro);
            Py_DECREF(gobject);
            return NULL;
        }
    }
    return gobject;
}

/* deprecated macro, use pygobject_init() instead. */
#define init_pygobject() G_STMT_START {         \
    if (!pygobject_init(-1, -1, -1))            \
        return;                                 \
} G_STMT_END

/* deprecated macro, use pygobject_init() instead. */
#define init_pygobject_check(req_major, req_minor, req_micro) G_STMT_START {    \
    if (!pygobject_init(req_major, req_minor, req_micro))                       \
        return;                                                                 \
} G_STMT_END


#endif /* !_INSIDE_PYGOBJECT_ */

G_END_DECLS

#endif /* !_PYGOBJECT_H_ */
