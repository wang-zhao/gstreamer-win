
#ifndef __gst_cairo_marshal_MARSHAL_H__
#define __gst_cairo_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:BOXED,UINT64,UINT64 */
extern void gst_cairo_marshal_VOID__BOXED_UINT64_UINT64 (GClosure     *closure,
                                                         GValue       *return_value,
                                                         guint         n_param_values,
                                                         const GValue *param_values,
                                                         gpointer      invocation_hint,
                                                         gpointer      marshal_data);

/* VOID:BOXED */
#define gst_cairo_marshal_VOID__BOXED	g_cclosure_marshal_VOID__BOXED

G_END_DECLS

#endif /* __gst_cairo_marshal_MARSHAL_H__ */

