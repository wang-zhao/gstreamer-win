/* -*- Mode: C; ; c-file-style: "python" -*- */
/* gst-python
 * Copyright (C) 2004 Johan Dahlin
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
 * Author: Johan Dahlin <johan@gnome.org>
 */
#ifndef __COMMON_H__
#define __COMMON_H__

#include <Python.h>
#include <pygobject.h>
#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <pygobject.h>
#include "pygstminiobject.h"

#if (defined HAVE_OLD_PYGTK && (PY_VERSION_HEX < 0x02030000))
  typedef destructor freefunc;
#endif

#if PY_VERSION_HEX < 0x02050000
#define lenfunc inquiry
#define ssizeargfunc intargfunc
#define ssizessizeargfunc intintargfunc
#define ssizeobjargproc intobjargproc
#define ssizessizeobjargproc intintobjargproc
#endif

typedef struct {
	PyGObject *pad;
	GClosure *link_function;
	GClosure *event_function;
	GClosure *chain_function;
	GClosure *get_function; 
	GClosure *getcaps_function; 
	GClosure *setcaps_function; 
	GClosure *activate_function;
	GClosure *activatepull_function;
	GClosure *activatepush_function;
        /* Query is not implemented as a closure to avoid refcounting
         * making the query immutable and therefore useless */
	PyObject *query_function;
} PyGstPadPrivate;

typedef struct {
     PyObject *func, *data;
} PyGstCustomNotify;

typedef struct {
    PyObject_HEAD
    GstIterator *iter;
} PyGstIterator;

extern PyTypeObject PyGstIterator_Type;


#endif /* __COMMON_H__ */
