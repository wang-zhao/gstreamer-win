#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         priv_gst_parse_yyparse
#define yylex           priv_gst_parse_yylex
#define yyerror         priv_gst_parse_yyerror
#define yydebug         priv_gst_parse_yydebug
#define yynerrs         priv_gst_parse_yynerrs


/* Copy the first part of user declarations.  */
//#line 1 "./grammar.y" /* yacc.c:339  */

#include "../gst_private.h"

#include <glib-object.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../gst-i18n-lib.h"

#include "../gstconfig.h"
#include "../gstparse.h"
#include "../gstinfo.h"
#include "../gsterror.h"
#include "../gststructure.h"
#include "../gsturi.h"
#include "../gstutils.h"
#include "../gstvalue.h"
#include "../gstchildproxy.h"
#include "types.h"

/* All error messages in this file are user-visible and need to be translated.
 * Don't start the message with a capital, and don't end them with a period,
 * as they will be presented inside a sentence/error.
 */

#define YYERROR_VERBOSE 1

#define YYENABLE_NLS 0

#ifndef YYLTYPE_IS_TRIVIAL
#define YYLTYPE_IS_TRIVIAL 0
#endif

typedef void* yyscan_t;

int priv_gst_parse_yylex (void * yylval_param , yyscan_t yyscanner);
int priv_gst_parse_yylex_init (yyscan_t scanner);
int priv_gst_parse_yylex_destroy (yyscan_t scanner);
struct yy_buffer_state * priv_gst_parse_yy_scan_string (char* , yyscan_t);
void _gst_parse_yypush_buffer_state (void * new_buffer ,yyscan_t yyscanner );
void _gst_parse_yypop_buffer_state (yyscan_t yyscanner );

#ifdef __GST_PARSE_TRACE
static guint __strings;
static guint __links;
static guint __chains;
gchar *
__gst_parse_strdup (gchar *org)
{
  gchar *ret;
  __strings++;
  ret = g_strdup (org);
  /* g_print ("ALLOCATED STR   (%3u): %p %s\n", __strings, ret, ret); */
  return ret;
}
void
__gst_parse_strfree (gchar *str)
{
  if (str) {
    /* g_print ("FREEING STR     (%3u): %p %s\n", __strings - 1, str, str); */
    g_free (str);
    g_return_if_fail (__strings > 0);
    __strings--;
  }
}
link_t *__gst_parse_link_new ()
{
  link_t *ret;
  __links++;
  ret = g_slice_new0 (link_t);
  /* g_print ("ALLOCATED LINK  (%3u): %p\n", __links, ret); */
  return ret;
}
void
__gst_parse_link_free (link_t *data)
{
  if (data) {
    /* g_print ("FREEING LINK    (%3u): %p\n", __links - 1, data); */
    g_slice_free (link_t, data);
    g_return_if_fail (__links > 0);
    __links--;
  }
}
chain_t *
__gst_parse_chain_new ()
{
  chain_t *ret;
  __chains++;
  ret = g_slice_new0 (chain_t);
  /* g_print ("ALLOCATED CHAIN (%3u): %p\n", __chains, ret); */
  return ret;
}
void
__gst_parse_chain_free (chain_t *data)
{
  /* g_print ("FREEING CHAIN   (%3u): %p\n", __chains - 1, data); */
  g_slice_free (chain_t, data);
  g_return_if_fail (__chains > 0);
  __chains--;
}

#endif /* __GST_PARSE_TRACE */

typedef struct {
  gchar *src_pad;
  gchar *sink_pad;
  GstElement *sink;
  GstCaps *caps;
  gulong signal_id;
} DelayedLink;

typedef struct {
  gchar *name;
  gchar *value_str;
  gulong signal_id;
} DelayedSet;

/*** define SET_ERROR macro/function */

#ifdef G_HAVE_ISO_VARARGS

#  define SET_ERROR(error, type, ...) \
G_STMT_START { \
  GST_CAT_ERROR (GST_CAT_PIPELINE, __VA_ARGS__); \
  if ((error) && !*(error)) { \
    g_set_error ((error), GST_PARSE_ERROR, (type), __VA_ARGS__); \
  } \
} G_STMT_END

#elif defined(G_HAVE_GNUC_VARARGS)

#  define SET_ERROR(error, type, args...) \
G_STMT_START { \
  GST_CAT_ERROR (GST_CAT_PIPELINE, args ); \
  if ((error) && !*(error)) { \
    g_set_error ((error), GST_PARSE_ERROR, (type), args ); \
  } \
} G_STMT_END

#else

static inline void
SET_ERROR (GError **error, gint type, const char *format, ...)
{
  if (error) {
    if (*error) {
      g_warning ("error while parsing");
    } else {
      va_list varargs;
      char *string;

      va_start (varargs, format);
      string = g_strdup_vprintf (format, varargs);
      va_end (varargs);

      g_set_error (error, GST_PARSE_ERROR, type, string);

      g_free (string);
    }
  }
}

#endif /* G_HAVE_ISO_VARARGS */

/*** define YYPRINTF macro/function if we're debugging */

/* bison 1.35 calls this macro with side effects, we need to make sure the
   side effects work - crappy bison */

#ifndef GST_DISABLE_GST_DEBUG
#  define YYDEBUG 1

#  ifdef G_HAVE_ISO_VARARGS

/* #  define YYFPRINTF(a, ...) GST_CAT_DEBUG (GST_CAT_PIPELINE, __VA_ARGS__) */
#    define YYFPRINTF(a, ...) \
G_STMT_START { \
     GST_CAT_LOG (GST_CAT_PIPELINE, __VA_ARGS__); \
} G_STMT_END

#  elif defined(G_HAVE_GNUC_VARARGS)

#    define YYFPRINTF(a, args...) \
G_STMT_START { \
     GST_CAT_LOG (GST_CAT_PIPELINE, args); \
} G_STMT_END

#  else

static inline void
YYPRINTF(const char *format, ...)
{
  va_list varargs;
  gchar *temp;

  va_start (varargs, format);
  temp = g_strdup_vprintf (format, varargs);
  GST_CAT_LOG (GST_CAT_PIPELINE, "%s", temp);
  g_free (temp);
  va_end (varargs);
}

#  endif /* G_HAVE_ISO_VARARGS */

#endif /* GST_DISABLE_GST_DEBUG */

#define ADD_MISSING_ELEMENT(graph,name) G_STMT_START {                      \
    if ((graph)->ctx) {                                                     \
      (graph)->ctx->missing_elements =                                      \
          g_list_append ((graph)->ctx->missing_elements, g_strdup (name));  \
    } } G_STMT_END

static void
no_free (gconstpointer foo)
{
  /* do nothing */
}

#define GST_BIN_MAKE(res, type, chainval, assign, type_string_free_func) \
G_STMT_START { \
  chain_t *chain = chainval; \
  GSList *walk; \
  GstBin *bin = (GstBin *) gst_element_factory_make (type, NULL); \
  if (!chain) { \
    SET_ERROR (graph->error, GST_PARSE_ERROR_EMPTY_BIN, \
        _("specified empty bin \"%s\", not allowed"), type); \
    g_slist_foreach (assign, (GFunc) gst_parse_strfree, NULL); \
    g_slist_free (assign); \
    gst_object_unref (bin); \
    type_string_free_func (type); /* Need to clean up the string */ \
    YYERROR; \
  } else if (!bin) { \
    ADD_MISSING_ELEMENT(graph, type); \
    SET_ERROR (graph->error, GST_PARSE_ERROR_NO_SUCH_ELEMENT, \
        _("no bin \"%s\", skipping"), type); \
    g_slist_foreach (assign, (GFunc) gst_parse_strfree, NULL); \
    g_slist_free (assign); \
    res = chain; \
  } else { \
    for (walk = chain->elements; walk; walk = walk->next ) \
      gst_bin_add (bin, GST_ELEMENT (walk->data)); \
    g_slist_free (chain->elements); \
    chain->elements = g_slist_prepend (NULL, bin); \
    res = chain; \
    /* set the properties now */ \
    for (walk = assign; walk; walk = walk->next) \
      gst_parse_element_set ((gchar *) walk->data, GST_ELEMENT (bin), graph); \
    g_slist_free (assign); \
  } \
} G_STMT_END

#define MAKE_LINK(link, _src, _src_name, _src_pads, _sink, _sink_name, _sink_pads) \
G_STMT_START { \
  link = gst_parse_link_new (); \
  link->src = _src; \
  link->sink = _sink; \
  link->src_name = _src_name; \
  link->sink_name = _sink_name; \
  link->src_pads = _src_pads; \
  link->sink_pads = _sink_pads; \
  link->caps = NULL; \
} G_STMT_END

#define MAKE_REF(link, _src, _pads) \
G_STMT_START { \
  gchar *padname = _src; \
  GSList *pads = _pads; \
  if (padname) { \
    while (*padname != '.') padname++; \
    *padname = '\0'; \
    padname++; \
    if (*padname != '\0') \
      pads = g_slist_prepend (pads, gst_parse_strdup (padname)); \
  } \
  MAKE_LINK (link, NULL, _src, pads, NULL, NULL, NULL); \
} G_STMT_END

static void
gst_parse_free_delayed_set (DelayedSet *set)
{
  g_free(set->name);
  g_free(set->value_str);
  g_slice_free(DelayedSet, set);
}

static void gst_parse_new_child(GstChildProxy *child_proxy, GObject *object,
                                gpointer data);

static void
gst_parse_add_delayed_set (GstElement *element, gchar *name, gchar *value_str)
{
  DelayedSet *data = g_slice_new0 (DelayedSet);

  GST_CAT_LOG_OBJECT (GST_CAT_PIPELINE, element, "delaying property set %s to %s",
    name, value_str);

  data->name = g_strdup(name);
  data->value_str = g_strdup(value_str);
  data->signal_id = g_signal_connect_data(element, "child-added",
      G_CALLBACK (gst_parse_new_child), data, (GClosureNotify)
      gst_parse_free_delayed_set, (GConnectFlags) 0);

  /* FIXME: we would need to listen on all intermediate bins too */
  if (GST_IS_BIN (element)) {
    gchar **names, **current;
    GstElement *parent, *child;

    current = names = g_strsplit (name, "::", -1);
    parent = gst_bin_get_by_name (GST_BIN_CAST (element), current[0]);
    current++;
    while (parent && current[0]) {
      child = gst_bin_get_by_name (GST_BIN (parent), current[0]);
      if (!child && current[1]) {
        char *sub_name = g_strjoinv ("::", &current[0]);

        gst_parse_add_delayed_set(parent, sub_name, value_str);
        g_free (sub_name);
      }
      parent = child;
      current++;
    }
    g_strfreev (names);
  }
}

static void gst_parse_new_child(GstChildProxy *child_proxy, GObject *object,
                                gpointer data)
{
  DelayedSet *set = (DelayedSet *) data;
  GParamSpec *pspec;
  GValue v = { 0, };
  GstObject *target = NULL;
  GType value_type;

  GST_CAT_LOG_OBJECT (GST_CAT_PIPELINE, child_proxy, "new child %s, checking property %s",
      GST_OBJECT_NAME(object), set->name);

  if (gst_child_proxy_lookup (GST_OBJECT (child_proxy), set->name, &target, &pspec)) {
    gboolean got_value = FALSE;

    value_type = pspec->value_type;

    GST_CAT_LOG_OBJECT (GST_CAT_PIPELINE, child_proxy, "parsing delayed property %s as a %s from %s",
      pspec->name, g_type_name (value_type), set->value_str);
    g_value_init (&v, value_type);
    if (gst_value_deserialize (&v, set->value_str))
      got_value = TRUE;
    else if (g_type_is_a (value_type, GST_TYPE_ELEMENT)) {
       GstElement *bin;

       bin = gst_parse_bin_from_description (set->value_str, TRUE, NULL);
       if (bin) {
         g_value_set_object (&v, bin);
         got_value = TRUE;
       }
    }
    g_signal_handler_disconnect (child_proxy, set->signal_id);
    if (!got_value)
      goto error;
    g_object_set_property (G_OBJECT (target), pspec->name, &v);
  } else {
    const gchar *obj_name = GST_OBJECT_NAME(object);
    gint len = strlen (obj_name);

    /* do a delayed set */
    if ((strlen (set->name) > (len + 2)) && !strncmp (set->name, obj_name, len) && !strncmp (&set->name[len], "::", 2)) {
      gst_parse_add_delayed_set (GST_ELEMENT(child_proxy), set->name, set->value_str);
    }
  }

out:
  if (G_IS_VALUE (&v))
    g_value_unset (&v);
  if (target)
    gst_object_unref (target);
  return;

error:
  GST_CAT_ERROR (GST_CAT_PIPELINE, "could not set property \"%s\" in element \"%s\"",
	 pspec->name, GST_ELEMENT_NAME (target));
  goto out;
}

static void
gst_parse_element_set (gchar *value, GstElement *element, graph_t *graph)
{
  GParamSpec *pspec;
  gchar *pos = value;
  GValue v = { 0, };
  GstObject *target = NULL;
  GType value_type;

  /* do nothing if assignment is for missing element */
  if (element == NULL)
    goto out;

  /* parse the string, so the property name is null-terminated an pos points
     to the beginning of the value */
  while (!g_ascii_isspace (*pos) && (*pos != '=')) pos++;
  if (*pos == '=') {
    *pos = '\0';
  } else {
    *pos = '\0';
    pos++;
    while (g_ascii_isspace (*pos)) pos++;
  }
  pos++;
  while (g_ascii_isspace (*pos)) pos++;
  if (*pos == '"') {
    pos++;
    pos[strlen (pos) - 1] = '\0';
  }
  gst_parse_unescape (pos);

  if (gst_child_proxy_lookup (GST_OBJECT (element), value, &target, &pspec)) {
    gboolean got_value = FALSE;

    value_type = pspec->value_type;

    GST_CAT_LOG_OBJECT (GST_CAT_PIPELINE, element, "parsing property %s as a %s", pspec->name,
      g_type_name (value_type));
    g_value_init (&v, value_type);
    if (gst_value_deserialize (&v, pos))
      got_value = TRUE;
    else if (g_type_is_a (value_type, GST_TYPE_ELEMENT)) {
       GstElement *bin;

       bin = gst_parse_bin_from_description (pos, TRUE, NULL);
       if (bin) {
         g_value_set_object (&v, bin);
         got_value = TRUE;
       }
    }
    if (!got_value)
      goto error;
    g_object_set_property (G_OBJECT (target), pspec->name, &v);
  } else {
    /* do a delayed set */
    if (GST_IS_CHILD_PROXY (element)) {
      gst_parse_add_delayed_set (element, value, pos);
    }
    else {
      SET_ERROR (graph->error, GST_PARSE_ERROR_NO_SUCH_PROPERTY, \
          _("no property \"%s\" in element \"%s\""), value, \
          GST_ELEMENT_NAME (element));
    }
  }

out:
  gst_parse_strfree (value);
  if (G_IS_VALUE (&v))
    g_value_unset (&v);
  if (target)
    gst_object_unref (target);
  return;

error:
  SET_ERROR (graph->error, GST_PARSE_ERROR_COULD_NOT_SET_PROPERTY,
         _("could not set property \"%s\" in element \"%s\" to \"%s\""),
	 value, GST_ELEMENT_NAME (element), pos);
  goto out;
}

static inline void
gst_parse_free_link (link_t *link)
{
  gst_parse_strfree (link->src_name);
  gst_parse_strfree (link->sink_name);
  g_slist_foreach (link->src_pads, (GFunc) gst_parse_strfree, NULL);
  g_slist_foreach (link->sink_pads, (GFunc) gst_parse_strfree, NULL);
  g_slist_free (link->src_pads);
  g_slist_free (link->sink_pads);
  if (link->caps) gst_caps_unref (link->caps);
  gst_parse_link_free (link);
}

static void
gst_parse_free_delayed_link (DelayedLink *link)
{
  g_free (link->src_pad);
  g_free (link->sink_pad);
  if (link->caps) gst_caps_unref (link->caps);
  g_slice_free (DelayedLink, link);
}

static void
gst_parse_found_pad (GstElement *src, GstPad *pad, gpointer data)
{
  DelayedLink *link = data;

  GST_CAT_INFO (GST_CAT_PIPELINE, "trying delayed linking %s:%s to %s:%s",
                GST_STR_NULL (GST_ELEMENT_NAME (src)), GST_STR_NULL (link->src_pad),
                GST_STR_NULL (GST_ELEMENT_NAME (link->sink)), GST_STR_NULL (link->sink_pad));

  if (gst_element_link_pads_filtered (src, link->src_pad, link->sink,
      link->sink_pad, link->caps)) {
    /* do this here, we don't want to get any problems later on when
     * unlocking states */
    GST_CAT_DEBUG (GST_CAT_PIPELINE, "delayed linking %s:%s to %s:%s worked",
               	   GST_STR_NULL (GST_ELEMENT_NAME (src)), GST_STR_NULL (link->src_pad),
               	   GST_STR_NULL (GST_ELEMENT_NAME (link->sink)), GST_STR_NULL (link->sink_pad));
    g_signal_handler_disconnect (src, link->signal_id);
  }
}

/* both padnames and the caps may be NULL */
static gboolean
gst_parse_perform_delayed_link (GstElement *src, const gchar *src_pad,
                                GstElement *sink, const gchar *sink_pad,
                                GstCaps *caps)
{
  GList *templs = gst_element_class_get_pad_template_list (
      GST_ELEMENT_GET_CLASS (src));

  for (; templs; templs = templs->next) {
    GstPadTemplate *templ = (GstPadTemplate *) templs->data;
    if ((GST_PAD_TEMPLATE_DIRECTION (templ) == GST_PAD_SRC) &&
        (GST_PAD_TEMPLATE_PRESENCE(templ) == GST_PAD_SOMETIMES))
    {
      DelayedLink *data = g_slice_new (DelayedLink);

      /* TODO: maybe we should check if src_pad matches this template's names */

      GST_CAT_DEBUG (GST_CAT_PIPELINE, "trying delayed link %s:%s to %s:%s",
                     GST_STR_NULL (GST_ELEMENT_NAME (src)), GST_STR_NULL (src_pad),
                     GST_STR_NULL (GST_ELEMENT_NAME (sink)), GST_STR_NULL (sink_pad));

      data->src_pad = g_strdup (src_pad);
      data->sink = sink;
      data->sink_pad = g_strdup (sink_pad);
      if (caps) {
      	data->caps = gst_caps_copy (caps);
      } else {
      	data->caps = NULL;
      }
      data->signal_id = g_signal_connect_data (src, "pad-added",
          G_CALLBACK (gst_parse_found_pad), data,
          (GClosureNotify) gst_parse_free_delayed_link, (GConnectFlags) 0);
      return TRUE;
    }
  }
  return FALSE;
}

/*
 * performs a link and frees the struct. src and sink elements must be given
 * return values   0 - link performed
 *                 1 - link delayed
 *                <0 - error
 */
static gint
gst_parse_perform_link (link_t *link, graph_t *graph)
{
  GstElement *src = link->src;
  GstElement *sink = link->sink;
  GSList *srcs = link->src_pads;
  GSList *sinks = link->sink_pads;
  g_assert (GST_IS_ELEMENT (src));
  g_assert (GST_IS_ELEMENT (sink));

  GST_CAT_INFO (GST_CAT_PIPELINE,
      "linking %s:%s to %s:%s (%u/%u) with caps \"%" GST_PTR_FORMAT "\"",
      GST_ELEMENT_NAME (src), link->src_name ? link->src_name : "(any)",
      GST_ELEMENT_NAME (sink), link->sink_name ? link->sink_name : "(any)",
      g_slist_length (srcs), g_slist_length (sinks), link->caps);

  if (!srcs || !sinks) {
    if (gst_element_link_pads_filtered (src,
        srcs ? (const gchar *) srcs->data : NULL, sink,
        sinks ? (const gchar *) sinks->data : NULL, link->caps)) {
      goto success;
    } else {
      if (gst_parse_perform_delayed_link (src,
          srcs ? (const gchar *) srcs->data : NULL,
          sink, sinks ? (const gchar *) sinks->data : NULL, link->caps)) {
	goto success;
      } else {
        goto error;
      }
    }
  }
  if (g_slist_length (link->src_pads) != g_slist_length (link->sink_pads)) {
    goto error;
  }
  while (srcs && sinks) {
    const gchar *src_pad = (const gchar *) srcs->data;
    const gchar *sink_pad = (const gchar *) sinks->data;
    srcs = g_slist_next (srcs);
    sinks = g_slist_next (sinks);
    if (gst_element_link_pads_filtered (src, src_pad, sink, sink_pad,
        link->caps)) {
      continue;
    } else {
      if (gst_parse_perform_delayed_link (src, src_pad,
                                          sink, sink_pad,
					  link->caps)) {
	continue;
      } else {
        goto error;
      }
    }
  }

success:
  gst_parse_free_link (link);
  return 0;

error:
  SET_ERROR (graph->error, GST_PARSE_ERROR_LINK,
      _("could not link %s to %s"), GST_ELEMENT_NAME (src),
      GST_ELEMENT_NAME (sink));
  gst_parse_free_link (link);
  return -1;
}


static int yyerror (void *scanner, graph_t *graph, const char *s);

//#line 693 "grammar.tab.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "grammar.tab.h".  */
#ifndef YY_PRIV_GST_PARSE_YY_GRAMMAR_TAB_H_INCLUDED
# define YY_PRIV_GST_PARSE_YY_GRAMMAR_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int priv_gst_parse_yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    PARSE_URL = 258,
    IDENTIFIER = 259,
    REF = 260,
    PADREF = 261,
    BINREF = 262,
    ASSIGNMENT = 263,
    LINK = 264
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
//#line 622 "./grammar.y" /* yacc.c:355  */

    gchar *s;
    chain_t *c;
    link_t *l;
    GstElement *e;
    GSList *p;
    graph_t *g;

//#line 752 "grammar.tab.c" /* yacc.c:355  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int priv_gst_parse_yyparse (void *scanner, graph_t *graph);

#endif /* !YY_PRIV_GST_PARSE_YY_GRAMMAR_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

//#line 766 "grammar.tab.c" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  29
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   177

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  16
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  12
/* YYNRULES -- Number of rules.  */
#define YYNRULES  32
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  43

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   264

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    14,     2,     2,     2,     2,     2,     2,
      10,    11,     2,     2,    12,     2,    13,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    15,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   658,   658,   672,   676,   677,   679,   680,   683,   686,
     691,   692,   696,   697,   700,   701,   704,   705,   706,   709,
     722,   723,   724,   727,   732,   733,   768,   796,   797,   811,
     831,   856,   859
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "PARSE_URL", "IDENTIFIER", "REF",
  "PADREF", "BINREF", "ASSIGNMENT", "LINK", "'('", "')'", "','", "'.'",
  "'!'", "'='", "$accept", "element", "assignments", "bin", "pads",
  "padlist", "reference", "linkpart", "link", "linklist", "chain", "graph", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
      40,    41,    44,    46,    33,    61
};
# endif

#define YYPACT_NINF -10

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-10)))

#define YYTABLE_NINF -33

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     135,    11,   -10,    -9,    -9,   -10,   -10,     1,   -10,   -10,
     -10,    -3,   167,   102,     7,    31,     4,   -10,   -10,    90,
     143,   -10,    -1,   151,    43,   -10,   159,    55,    67,   -10,
      -9,     2,   -10,   -10,   112,   123,   -10,    79,   -10,   -10,
     -10,   -10,   -10
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
      18,    18,     2,    14,    10,     4,     4,    23,    24,    17,
      16,     0,    18,     0,     0,     0,     0,    15,    11,     0,
      18,     3,    18,    30,     0,    27,    20,     0,     0,     1,
      12,     0,     5,     8,     0,     0,    19,     0,    22,    13,
       9,     7,     6
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -10,   -10,    18,   -10,   -10,    -2,   -10,     3,    10,    13,
       0,   -10
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     7,    19,     8,     9,    17,    10,    11,    26,    27,
      28,    14
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] =
{
      13,    15,    18,    16,     3,     4,    22,    29,    30,    21,
      12,    12,    24,    40,     1,     2,     3,     4,     5,    34,
      35,     6,    12,    15,    20,    36,    24,     0,    39,    12,
      12,   -29,    25,    12,     1,     2,     3,     4,     5,    37,
     -18,     6,   -29,   -28,    25,     0,     1,     2,     3,     4,
       5,     0,   -18,     6,   -28,   -26,    38,     0,   -26,   -26,
     -26,   -26,   -26,     0,   -26,   -26,   -26,   -25,    25,     0,
       1,     2,     3,     4,     5,     0,   -18,     6,   -25,   -21,
      38,     0,   -21,   -21,   -21,   -21,   -21,     0,   -21,   -21,
     -21,    31,     0,     1,     2,     3,     4,     5,    32,   -18,
       6,    33,   -32,    25,     0,     1,     2,     3,     4,     5,
       0,   -18,     6,    25,     0,     1,     2,     3,     4,     5,
       0,   -18,     6,    41,    25,     0,     1,     2,     3,     4,
       5,     0,   -18,     6,    42,   -31,     0,     0,     1,     2,
       3,     4,     5,     0,     0,     6,     1,     2,     3,     4,
       5,    32,     0,     6,     1,     2,     3,     4,     5,     0,
     -18,     6,    23,     2,     3,     4,     5,     0,   -18,     6,
      23,     2,     3,     4,     5,     0,     0,     6
};

static const yytype_int8 yycheck[] =
{
       0,     1,     4,    12,     5,     6,     9,     0,     4,     8,
       0,     1,    12,    11,     3,     4,     5,     6,     7,    19,
      20,    10,    12,    23,     6,    22,    26,    -1,    30,    19,
      20,     0,     1,    23,     3,     4,     5,     6,     7,    26,
       9,    10,    11,     0,     1,    -1,     3,     4,     5,     6,
       7,    -1,     9,    10,    11,     0,     1,    -1,     3,     4,
       5,     6,     7,    -1,     9,    10,    11,     0,     1,    -1,
       3,     4,     5,     6,     7,    -1,     9,    10,    11,     0,
       1,    -1,     3,     4,     5,     6,     7,    -1,     9,    10,
      11,     1,    -1,     3,     4,     5,     6,     7,     8,     9,
      10,    11,     0,     1,    -1,     3,     4,     5,     6,     7,
      -1,     9,    10,     1,    -1,     3,     4,     5,     6,     7,
      -1,     9,    10,    11,     1,    -1,     3,     4,     5,     6,
       7,    -1,     9,    10,    11,     0,    -1,    -1,     3,     4,
       5,     6,     7,    -1,    -1,    10,     3,     4,     5,     6,
       7,     8,    -1,    10,     3,     4,     5,     6,     7,    -1,
       9,    10,     3,     4,     5,     6,     7,    -1,     9,    10,
       3,     4,     5,     6,     7,    -1,    -1,    10
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,     4,     5,     6,     7,    10,    17,    19,    20,
      22,    23,    24,    26,    27,    26,    12,    21,    21,    18,
      18,     8,     9,     3,    26,     1,    24,    25,    26,     0,
       4,     1,     8,    11,    26,    26,    23,    25,     1,    21,
      11,    11,    11
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    16,    17,    17,    18,    18,    19,    19,    19,    19,
      20,    20,    21,    21,    22,    22,    23,    23,    23,    24,
      25,    25,    25,    26,    26,    26,    26,    26,    26,    26,
      26,    27,    27
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     2,     0,     2,     4,     4,     3,     4,
       1,     2,     2,     3,     1,     2,     1,     1,     0,     3,
       1,     2,     2,     1,     1,     2,     2,     2,     2,     2,
       2,     0,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (scanner, graph, YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, scanner, graph); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, void *scanner, graph_t *graph)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (scanner);
  YYUSE (graph);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, void *scanner, graph_t *graph)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, scanner, graph);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, void *scanner, graph_t *graph)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              , scanner, graph);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, scanner, graph); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, void *scanner, graph_t *graph)
{
  YYUSE (yyvaluep);
  YYUSE (scanner);
  YYUSE (graph);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void *scanner, graph_t *graph)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, scanner);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
//#line 658 "./grammar.y" /* yacc.c:1661  */
    { (yyval.e) = gst_element_factory_make ((yyvsp[0].s), NULL);
						if ((yyval.e) == NULL) {
						  ADD_MISSING_ELEMENT (graph, (yyvsp[0].s));
						  SET_ERROR (graph->error, GST_PARSE_ERROR_NO_SUCH_ELEMENT, _("no element \"%s\""), (yyvsp[0].s));
						  /* if FATAL_ERRORS flag is set, we don't have to worry about backwards
						   * compatibility and can continue parsing and check for other missing
						   * elements */
						  if ((graph->flags & GST_PARSE_FLAG_FATAL_ERRORS) == 0) {
						    gst_parse_strfree ((yyvsp[0].s));
						    YYERROR;
						  }
						}
						gst_parse_strfree ((yyvsp[0].s));
                                              }
//#line 1913 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 3:
//#line 672 "./grammar.y" /* yacc.c:1661  */
    { gst_parse_element_set ((yyvsp[0].s), (yyvsp[-1].e), graph);
						(yyval.e) = (yyvsp[-1].e);
	                                      }
//#line 1921 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 4:
//#line 676 "./grammar.y" /* yacc.c:1661  */
    { (yyval.p) = NULL; }
//#line 1927 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 5:
//#line 677 "./grammar.y" /* yacc.c:1661  */
    { (yyval.p) = g_slist_prepend ((yyvsp[-1].p), (yyvsp[0].s)); }
//#line 1933 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 6:
//#line 679 "./grammar.y" /* yacc.c:1661  */
    { GST_BIN_MAKE ((yyval.c), "bin", (yyvsp[-1].c), (yyvsp[-2].p), no_free); }
//#line 1939 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 7:
//#line 680 "./grammar.y" /* yacc.c:1661  */
    { GST_BIN_MAKE ((yyval.c), (yyvsp[-3].s), (yyvsp[-1].c), (yyvsp[-2].p), gst_parse_strfree);
						gst_parse_strfree ((yyvsp[-3].s));
					      }
//#line 1947 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 8:
//#line 683 "./grammar.y" /* yacc.c:1661  */
    { GST_BIN_MAKE ((yyval.c), (yyvsp[-2].s), NULL, (yyvsp[-1].p), gst_parse_strfree);
						gst_parse_strfree ((yyvsp[-2].s));
					      }
//#line 1955 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 9:
//#line 686 "./grammar.y" /* yacc.c:1661  */
    { GST_BIN_MAKE ((yyval.c), (yyvsp[-3].s), NULL, (yyvsp[-2].p), gst_parse_strfree);
						gst_parse_strfree ((yyvsp[-3].s));
					      }
//#line 1963 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 10:
//#line 691 "./grammar.y" /* yacc.c:1661  */
    { (yyval.p) = g_slist_prepend (NULL, (yyvsp[0].s)); }
//#line 1969 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 11:
//#line 692 "./grammar.y" /* yacc.c:1661  */
    { (yyval.p) = (yyvsp[0].p);
						(yyval.p) = g_slist_prepend ((yyval.p), (yyvsp[-1].s));
					      }
//#line 1977 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 12:
//#line 696 "./grammar.y" /* yacc.c:1661  */
    { (yyval.p) = g_slist_prepend (NULL, (yyvsp[0].s)); }
//#line 1983 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 13:
//#line 697 "./grammar.y" /* yacc.c:1661  */
    { (yyval.p) = g_slist_prepend ((yyvsp[0].p), (yyvsp[-1].s)); }
//#line 1989 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 14:
//#line 700 "./grammar.y" /* yacc.c:1661  */
    { MAKE_REF ((yyval.l), (yyvsp[0].s), NULL); }
//#line 1995 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 15:
//#line 701 "./grammar.y" /* yacc.c:1661  */
    { MAKE_REF ((yyval.l), (yyvsp[-1].s), (yyvsp[0].p)); }
//#line 2001 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 16:
//#line 704 "./grammar.y" /* yacc.c:1661  */
    { (yyval.l) = (yyvsp[0].l); }
//#line 2007 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 17:
//#line 705 "./grammar.y" /* yacc.c:1661  */
    { MAKE_REF ((yyval.l), NULL, (yyvsp[0].p)); }
//#line 2013 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 18:
//#line 706 "./grammar.y" /* yacc.c:1661  */
    { MAKE_REF ((yyval.l), NULL, NULL); }
//#line 2019 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 19:
//#line 709 "./grammar.y" /* yacc.c:1661  */
    { (yyval.l) = (yyvsp[-2].l);
						if ((yyvsp[-1].s)) {
						  (yyval.l)->caps = gst_caps_from_string ((yyvsp[-1].s));
						  if ((yyval.l)->caps == NULL)
						    SET_ERROR (graph->error, GST_PARSE_ERROR_LINK, _("could not parse caps \"%s\""), (yyvsp[-1].s));
						  gst_parse_strfree ((yyvsp[-1].s));
						}
						(yyval.l)->sink_name = (yyvsp[0].l)->src_name;
						(yyval.l)->sink_pads = (yyvsp[0].l)->src_pads;
						gst_parse_link_free ((yyvsp[0].l));
					      }
//#line 2035 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 20:
//#line 722 "./grammar.y" /* yacc.c:1661  */
    { (yyval.p) = g_slist_prepend (NULL, (yyvsp[0].l)); }
//#line 2041 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 21:
//#line 723 "./grammar.y" /* yacc.c:1661  */
    { (yyval.p) = g_slist_prepend ((yyvsp[0].p), (yyvsp[-1].l)); }
//#line 2047 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 22:
//#line 724 "./grammar.y" /* yacc.c:1661  */
    { (yyval.p) = (yyvsp[-1].p); }
//#line 2053 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 23:
//#line 727 "./grammar.y" /* yacc.c:1661  */
    { (yyval.c) = gst_parse_chain_new ();
						(yyval.c)->first = (yyval.c)->last = (yyvsp[0].e);
						(yyval.c)->front = (yyval.c)->back = NULL;
						(yyval.c)->elements = g_slist_prepend (NULL, (yyvsp[0].e));
					      }
//#line 2063 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 24:
//#line 732 "./grammar.y" /* yacc.c:1661  */
    { (yyval.c) = (yyvsp[0].c); }
//#line 2069 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 25:
//#line 733 "./grammar.y" /* yacc.c:1661  */
    { if ((yyvsp[-1].c)->back && (yyvsp[0].c)->front) {
						  if (!(yyvsp[-1].c)->back->sink_name) {
						    SET_ERROR (graph->error, GST_PARSE_ERROR_LINK, _("link without source element"));
						    gst_parse_free_link ((yyvsp[-1].c)->back);
						  } else {
						    graph->links = g_slist_prepend (graph->links, (yyvsp[-1].c)->back);
						  }
						  if (!(yyvsp[0].c)->front->src_name) {
						    SET_ERROR (graph->error, GST_PARSE_ERROR_LINK, _("link without sink element"));
						    gst_parse_free_link ((yyvsp[0].c)->front);
						  } else {
						    graph->links = g_slist_prepend (graph->links, (yyvsp[0].c)->front);
						  }
						  (yyvsp[-1].c)->back = NULL;
						} else if ((yyvsp[-1].c)->back) {
						  if (!(yyvsp[-1].c)->back->sink_name) {
						    (yyvsp[-1].c)->back->sink = (yyvsp[0].c)->first;
						  }
						} else if ((yyvsp[0].c)->front) {
						  if (!(yyvsp[0].c)->front->src_name) {
						    (yyvsp[0].c)->front->src = (yyvsp[-1].c)->last;
						  }
						  (yyvsp[-1].c)->back = (yyvsp[0].c)->front;
						}

						if ((yyvsp[-1].c)->back) {
						  graph->links = g_slist_prepend (graph->links, (yyvsp[-1].c)->back);
						}
						(yyvsp[-1].c)->last = (yyvsp[0].c)->last;
						(yyvsp[-1].c)->back = (yyvsp[0].c)->back;
						(yyvsp[-1].c)->elements = g_slist_concat ((yyvsp[-1].c)->elements, (yyvsp[0].c)->elements);
						if ((yyvsp[0].c))
						  gst_parse_chain_free ((yyvsp[0].c));
						(yyval.c) = (yyvsp[-1].c);
					      }
//#line 2109 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 26:
//#line 768 "./grammar.y" /* yacc.c:1661  */
    { GSList *walk;
						if ((yyvsp[-1].c)->back) {
						  (yyvsp[0].p) = g_slist_prepend ((yyvsp[0].p), (yyvsp[-1].c)->back);
						  (yyvsp[-1].c)->back = NULL;
						} else {
						  if (!((link_t *) (yyvsp[0].p)->data)->src_name) {
						    ((link_t *) (yyvsp[0].p)->data)->src = (yyvsp[-1].c)->last;
						  }
						}
						for (walk = (yyvsp[0].p); walk; walk = walk->next) {
						  link_t *link = (link_t *) walk->data;
						  if (!link->sink_name && walk->next) {
						    SET_ERROR (graph->error, GST_PARSE_ERROR_LINK, _("link without sink element"));
						    gst_parse_free_link (link);
						  } else if (!link->src_name && !link->src) {
						    SET_ERROR (graph->error, GST_PARSE_ERROR_LINK, _("link without source element"));
						    gst_parse_free_link (link);
						  } else {
						    if (walk->next) {
						      graph->links = g_slist_prepend (graph->links, link);
						    } else {
						      (yyvsp[-1].c)->back = link;
						    }
						  }
						}
						g_slist_free ((yyvsp[0].p));
						(yyval.c) = (yyvsp[-1].c);
					      }
//#line 2142 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 27:
//#line 796 "./grammar.y" /* yacc.c:1661  */
    { (yyval.c) = (yyvsp[-1].c); }
//#line 2148 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 28:
//#line 797 "./grammar.y" /* yacc.c:1661  */
    { if ((yyvsp[0].c)->front) {
						  if (!(yyvsp[0].c)->front->src_name) {
						    SET_ERROR (graph->error, GST_PARSE_ERROR_LINK, _("link without source element"));
						    gst_parse_free_link ((yyvsp[0].c)->front);
						  } else {
						    graph->links = g_slist_prepend (graph->links, (yyvsp[0].c)->front);
						  }
						}
						if (!(yyvsp[-1].l)->sink_name) {
						  (yyvsp[-1].l)->sink = (yyvsp[0].c)->first;
						}
						(yyvsp[0].c)->front = (yyvsp[-1].l);
						(yyval.c) = (yyvsp[0].c);
					      }
//#line 2167 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 29:
//#line 811 "./grammar.y" /* yacc.c:1661  */
    { (yyval.c) = (yyvsp[0].c);
						if ((yyval.c)->front) {
						  GstElement *element =
							  gst_element_make_from_uri (GST_URI_SRC, (yyvsp[-1].s), NULL);
						  if (!element) {
						    SET_ERROR (graph->error, GST_PARSE_ERROR_NO_SUCH_ELEMENT,
							    _("no source element for URI \"%s\""), (yyvsp[-1].s));
						  } else {
						    (yyval.c)->front->src = element;
						    graph->links = g_slist_prepend (
							    graph->links, (yyval.c)->front);
						    (yyval.c)->front = NULL;
						    (yyval.c)->elements = g_slist_prepend ((yyval.c)->elements, element);
						  }
						} else {
						  SET_ERROR (graph->error, GST_PARSE_ERROR_LINK,
							  _("no element to link URI \"%s\" to"), (yyvsp[-1].s));
						}
						g_free ((yyvsp[-1].s));
					      }
//#line 2192 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 30:
//#line 831 "./grammar.y" /* yacc.c:1661  */
    { GstElement *element =
							  gst_element_make_from_uri (GST_URI_SINK, (yyvsp[0].s), NULL);
						if (!element) {
						  SET_ERROR (graph->error, GST_PARSE_ERROR_NO_SUCH_ELEMENT,
							  _("no sink element for URI \"%s\""), (yyvsp[0].s));
						  gst_parse_link_free ((yyvsp[-1].l));
						  g_free ((yyvsp[0].s));
						  YYERROR;
						} else if ((yyvsp[-1].l)->sink_name || (yyvsp[-1].l)->sink_pads) {
                                                  gst_object_unref (element);
						  SET_ERROR (graph->error, GST_PARSE_ERROR_LINK,
							  _("could not link sink element for URI \"%s\""), (yyvsp[0].s));
						  gst_parse_link_free ((yyvsp[-1].l));
						  g_free ((yyvsp[0].s));
						  YYERROR;
						} else {
						  (yyval.c) = gst_parse_chain_new ();
						  (yyval.c)->first = (yyval.c)->last = element;
						  (yyval.c)->front = (yyvsp[-1].l);
						  (yyval.c)->front->sink = element;
						  (yyval.c)->elements = g_slist_prepend (NULL, element);
						}
						g_free ((yyvsp[0].s));
					      }
//#line 2221 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 31:
//#line 856 "./grammar.y" /* yacc.c:1661  */
    { SET_ERROR (graph->error, GST_PARSE_ERROR_EMPTY, _("empty pipeline not allowed"));
						(yyval.g) = graph;
					      }
//#line 2229 "grammar.tab.c" /* yacc.c:1661  */
    break;

  case 32:
//#line 859 "./grammar.y" /* yacc.c:1661  */
    { (yyval.g) = graph;
						if ((yyvsp[0].c)->front) {
						  if (!(yyvsp[0].c)->front->src_name) {
						    SET_ERROR (graph->error, GST_PARSE_ERROR_LINK, _("link without source element"));
						    gst_parse_free_link ((yyvsp[0].c)->front);
						  } else {
						    (yyval.g)->links = g_slist_prepend ((yyval.g)->links, (yyvsp[0].c)->front);
						  }
						  (yyvsp[0].c)->front = NULL;
						}
						if ((yyvsp[0].c)->back) {
						  if (!(yyvsp[0].c)->back->sink_name) {
						    SET_ERROR (graph->error, GST_PARSE_ERROR_LINK, _("link without sink element"));
						    gst_parse_free_link ((yyvsp[0].c)->back);
						  } else {
						    (yyval.g)->links = g_slist_prepend ((yyval.g)->links, (yyvsp[0].c)->back);
						  }
						  (yyvsp[0].c)->back = NULL;
						}
						(yyval.g)->chain = (yyvsp[0].c);
					      }
//#line 2255 "grammar.tab.c" /* yacc.c:1661  */
    break;


//#line 2259 "grammar.tab.c" /* yacc.c:1661  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (scanner, graph, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (scanner, graph, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, scanner, graph);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, scanner, graph);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (scanner, graph, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, scanner, graph);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, scanner, graph);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
//#line 882 "./grammar.y" /* yacc.c:1906  */



static int
yyerror (void *scanner, graph_t *graph, const char *s)
{
  /* FIXME: This should go into the GError somehow, but how? */
  GST_WARNING ("Error during parsing: %s", s);
  return -1;
}


GstElement *
priv_gst_parse_launch (const gchar *str, GError **error, GstParseContext *ctx,
    GstParseFlags flags)
{
  graph_t g;
  gchar *dstr;
  GSList *walk;
  GstBin *bin = NULL;
  GstElement *ret;
  yyscan_t scanner;

  g_return_val_if_fail (str != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  g.chain = NULL;
  g.links = NULL;
  g.error = error;
  g.ctx = ctx;
  g.flags = flags;

#ifdef __GST_PARSE_TRACE
  GST_CAT_DEBUG (GST_CAT_PIPELINE, "TRACE: tracing enabled");
  __strings = __chains = __links = 0;
#endif /* __GST_PARSE_TRACE */

  dstr = g_strdup (str);
  priv_gst_parse_yylex_init (&scanner);
  priv_gst_parse_yy_scan_string (dstr, scanner);

#ifndef YYDEBUG
  yydebug = 1;
#endif

  if (yyparse (scanner, &g) != 0) {
    SET_ERROR (error, GST_PARSE_ERROR_SYNTAX,
        "Unrecoverable syntax error while parsing pipeline %s", str);

    priv_gst_parse_yylex_destroy (scanner);
    g_free (dstr);

    goto error1;
  }
  priv_gst_parse_yylex_destroy (scanner);
  g_free (dstr);

  GST_CAT_DEBUG (GST_CAT_PIPELINE, "got %u elements and %u links",
      g.chain ? g_slist_length (g.chain->elements) : 0,
      g_slist_length (g.links));

  if (!g.chain) {
    ret = NULL;
  } else if (!g.chain->elements->next) {
    /* only one toplevel element */
    ret = (GstElement *) g.chain->elements->data;
    g_slist_free (g.chain->elements);
    if (GST_IS_BIN (ret))
      bin = GST_BIN (ret);
    gst_parse_chain_free (g.chain);
  } else {
    /* put all elements in our bin */
    bin = GST_BIN (gst_element_factory_make ("pipeline", NULL));
    g_assert (bin);

    for (walk = g.chain->elements; walk; walk = walk->next) {
      if (walk->data != NULL)
        gst_bin_add (bin, GST_ELEMENT (walk->data));
    }

    g_slist_free (g.chain->elements);
    ret = GST_ELEMENT (bin);
    gst_parse_chain_free (g.chain);
  }

  /* remove links */
  for (walk = g.links; walk; walk = walk->next) {
    link_t *l = (link_t *) walk->data;
    if (!l->src) {
      if (l->src_name) {
        if (bin) {
          l->src = gst_bin_get_by_name_recurse_up (bin, l->src_name);
          if (l->src)
            gst_object_unref (l->src);
        } else {
          l->src = strcmp (GST_ELEMENT_NAME (ret), l->src_name) == 0 ? ret : NULL;
        }
      }
      if (!l->src) {
        if (l->src_name) {
          SET_ERROR (error, GST_PARSE_ERROR_NO_SUCH_ELEMENT,
              "No element named \"%s\" - omitting link", l->src_name);
        } else {
          /* probably a missing element which we've handled already */
        }
        gst_parse_free_link (l);
        continue;
      }
    }
    if (!l->sink) {
      if (l->sink_name) {
        if (bin) {
          l->sink = gst_bin_get_by_name_recurse_up (bin, l->sink_name);
          if (l->sink)
            gst_object_unref (l->sink);
        } else {
          l->sink = strcmp (GST_ELEMENT_NAME (ret), l->sink_name) == 0 ? ret : NULL;
        }
      }
      if (!l->sink) {
        if (l->sink_name) {
          SET_ERROR (error, GST_PARSE_ERROR_NO_SUCH_ELEMENT,
              "No element named \"%s\" - omitting link", l->sink_name);
        } else {
          /* probably a missing element which we've handled already */
        }
        gst_parse_free_link (l);
        continue;
      }
    }
    gst_parse_perform_link (l, &g);
  }
  g_slist_free (g.links);

out:
#ifdef __GST_PARSE_TRACE
  GST_CAT_DEBUG (GST_CAT_PIPELINE,
      "TRACE: %u strings, %u chains and %u links left", __strings, __chains,
      __links);
  if (__strings || __chains || __links) {
    g_warning ("TRACE: %u strings, %u chains and %u links left", __strings,
        __chains, __links);
  }
#endif /* __GST_PARSE_TRACE */

  return ret;

error1:
  if (g.chain) {
    g_slist_foreach (g.chain->elements, (GFunc)gst_object_unref, NULL);
    g_slist_free (g.chain->elements);
    gst_parse_chain_free (g.chain);
  }

  g_slist_foreach (g.links, (GFunc)gst_parse_free_link, NULL);
  g_slist_free (g.links);

  if (error)
    g_assert (*error);
  ret = NULL;

  goto out;
}
