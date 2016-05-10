
#ifndef ___fs_rtp_marshal_MARSHAL_H__
#define ___fs_rtp_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* POINTER:POINTER,POINTER,POINTER,POINTER */
extern void _fs_rtp_marshal_POINTER__POINTER_POINTER_POINTER_POINTER (GClosure     *closure,
                                                                      GValue       *return_value,
                                                                      guint         n_param_values,
                                                                      const GValue *param_values,
                                                                      gpointer      invocation_hint,
                                                                      gpointer      marshal_data);

/* VOID:BOXED,BOXED */
extern void _fs_rtp_marshal_VOID__BOXED_BOXED (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);

/* VOID:INT,STRING,STRING */
extern void _fs_rtp_marshal_VOID__INT_STRING_STRING (GClosure     *closure,
                                                     GValue       *return_value,
                                                     guint         n_param_values,
                                                     const GValue *param_values,
                                                     gpointer      invocation_hint,
                                                     gpointer      marshal_data);

G_END_DECLS

#endif /* ___fs_rtp_marshal_MARSHAL_H__ */

