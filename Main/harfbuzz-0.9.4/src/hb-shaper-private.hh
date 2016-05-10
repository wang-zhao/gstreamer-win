/*
 * Copyright © 2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_SHAPER_PRIVATE_HH
#define HB_SHAPER_PRIVATE_HH

#include "hb-private.hh"

#include "hb-shape-plan.h" /* TODO remove */

typedef hb_bool_t hb_shape_func_t (hb_shape_plan_t    *shape_plan,
				   hb_font_t          *font,
				   hb_buffer_t        *buffer,
				   const hb_feature_t *features,
				   unsigned int        num_features);

#define HB_SHAPER_IMPLEMENT(name) \
	extern "C" HB_INTERNAL hb_shape_func_t _hb_##name##_shape;
#include "hb-shaper-list.hh"
#undef HB_SHAPER_IMPLEMENT

struct hb_shaper_pair_t {
  char name[16];
  hb_shape_func_t *func;
};

HB_INTERNAL const hb_shaper_pair_t *
_hb_shapers_get (void);


/* For embedding in face / font / ... */
struct hb_shaper_data_t {
#define HB_SHAPER_IMPLEMENT(shaper) void *shaper;
#include "hb-shaper-list.hh"
#undef HB_SHAPER_IMPLEMENT
};

#define HB_SHAPERS_COUNT (sizeof (hb_shaper_data_t) / sizeof (void *))

/* Means: succeeded, but don't need to keep any data. */
#define HB_SHAPER_DATA_SUCCEEDED ((void *) +1)

/* Means: tried but failed to create. */
#define HB_SHAPER_DATA_INVALID ((void *) -1)
#define HB_SHAPER_DATA_IS_INVALID(data) ((void *) (data) == HB_SHAPER_DATA_INVALID)

#define HB_SHAPER_DATA_TYPE(__shaper__, __object__)		struct hb_##__shaper__##_shaper_##__object__##_data_t
#define HB_SHAPER_DATA_INSTANCE(__shaper__, __object__, __instance__)	(* (HB_SHAPER_DATA_TYPE(__shaper__, __object__) **) &(__instance__)->shaper_data.__shaper__)
#define HB_SHAPER_DATA(__shaper__, __object__)			HB_SHAPER_DATA_INSTANCE(__shaper__, __object__, __object__)
#define HB_SHAPER_DATA_CREATE_FUNC(__shaper__, __object__)	_hb_##__shaper__##_shaper_##__object__##_data_create
#define HB_SHAPER_DATA_DESTROY_FUNC(__shaper__, __object__)	_hb_##__shaper__##_shaper_##__object__##_data_destroy

#define HB_SHAPER_DATA_PROTOTYPE(__shaper__, __object__) \
	HB_SHAPER_DATA_TYPE (__shaper__, __object__); /* Type forward declaration. */ \
	extern "C" HB_INTERNAL HB_SHAPER_DATA_TYPE (__shaper__, __object__) * \
	HB_SHAPER_DATA_CREATE_FUNC (__shaper__, __object__) (hb_##__object__##_t *__object__ HB_SHAPER_DATA_CREATE_FUNC_EXTRA_ARGS); \
	extern "C" HB_INTERNAL void \
	HB_SHAPER_DATA_DESTROY_FUNC (__shaper__, __object__) (HB_SHAPER_DATA_TYPE (__shaper__, __object__) *data)

#define HB_SHAPER_DATA_DESTROY(__shaper__, __object__) \
	if (__object__->shaper_data.__shaper__ && \
	    __object__->shaper_data.__shaper__ != HB_SHAPER_DATA_INVALID && \
	    __object__->shaper_data.__shaper__ != HB_SHAPER_DATA_SUCCEEDED) \
	  HB_SHAPER_DATA_DESTROY_FUNC (__shaper__, __object__) (HB_SHAPER_DATA (__shaper__, __object__));

#define HB_SHAPER_DATA_ENSURE_DECLARE(__shaper__, __object__) \
static inline bool \
hb_##__shaper__##_shaper_##__object__##_data_ensure (hb_##__object__##_t *__object__) \
{\
  retry: \
  HB_SHAPER_DATA_TYPE (__shaper__, __object__) *data = (HB_SHAPER_DATA_TYPE (__shaper__, __object__) *) hb_atomic_ptr_get (&HB_SHAPER_DATA (__shaper__, __object__)); \
  if (unlikely (!data)) { \
    data = HB_SHAPER_DATA_CREATE_FUNC (__shaper__, __object__) (__object__); \
    if (unlikely (!data)) \
      data = (HB_SHAPER_DATA_TYPE (__shaper__, __object__) *) HB_SHAPER_DATA_INVALID; \
    if (!hb_atomic_ptr_cmpexch (&HB_SHAPER_DATA (__shaper__, __object__), NULL, data)) { \
      HB_SHAPER_DATA_DESTROY_FUNC (__shaper__, __object__) (data); \
      goto retry; \
    } \
  } \
  return data != NULL && !HB_SHAPER_DATA_IS_INVALID (data); \
}


#endif /* HB_SHAPER_PRIVATE_HH */
