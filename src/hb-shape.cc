/*
 * Copyright © 2009  Red Hat, Inc.
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
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#include "hb.hh"

#include "hb-shaper.hh"
#include "hb-shape-plan.hh"
#include "hb-buffer.hh"
#include "hb-font.hh"
#include "hb-machinery.hh"


#ifndef HB_NO_SHAPER

/**
 * SECTION:hb-shape
 * @title: hb-shape
 * @short_description: Conversion of text strings into positioned glyphs
 * @include: hb.h
 *
 * Shaping is the central operation of HarfBuzz. Shaping operates on buffers,
 * which are sequences of Unicode characters that use the same font and have
 * the same text direction, script, and language. After shaping the buffer
 * contains the output glyphs and their positions.
 **/


static inline void free_static_shaper_list ();

static const char * const nil_shaper_list[] = {nullptr};

static struct hb_shaper_list_lazy_loader_t : hb_lazy_loader_t<const char *,
							      hb_shaper_list_lazy_loader_t>
{
  static const char ** create ()
  {
    const char **shaper_list = (const char **) hb_calloc (1 + HB_SHAPERS_COUNT, sizeof (const char *));
    if (unlikely (!shaper_list))
      return nullptr;

    const hb_shaper_entry_t *shapers = _hb_shapers_get ();
    unsigned int i;
    for (i = 0; i < HB_SHAPERS_COUNT; i++)
      shaper_list[i] = shapers[i].name;
    shaper_list[i] = nullptr;

    hb_atexit (free_static_shaper_list);

    return shaper_list;
  }
  static void destroy (const char **l)
  { hb_free (l); }
  static const char * const * get_null ()
  { return nil_shaper_list; }
} static_shaper_list;

static inline
void free_static_shaper_list ()
{
  static_shaper_list.free_instance ();
}


/**
 * hb_shape_list_shapers:
 *
 * Retrieves the list of shapers supported by HarfBuzz.
 *
 * Return value: (transfer none) (array zero-terminated=1): an array of
 *    constant strings
 *
 * Since: 0.9.2
 **/
const char **
hb_shape_list_shapers ()
{
  return static_shaper_list.get_unconst ();
}


/**
 * hb_shape_full:
 * @font: an #hb_font_t to use for shaping
 * @buffer: an #hb_buffer_t to shape
 * @features: (array length=num_features) (nullable): an array of user
 *    specified #hb_feature_t or `NULL`
 * @num_features: the length of @features array
 * @shaper_list: (array zero-terminated=1) (nullable): a `NULL`-terminated
 *    array of shapers to use or `NULL`
 *
 * See hb_shape() for details. If @shaper_list is not `NULL`, the specified
 * shapers will be used in the given order, otherwise the default shapers list
 * will be used.
 *
 * Return value: false if all shapers failed, true otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_shape_full (hb_font_t          *font,
	       hb_buffer_t        *buffer,
	       const hb_feature_t *features,
	       unsigned int        num_features,
	       const char * const *shaper_list)
{
  if (unlikely (!buffer->len))
    return true;

  buffer->enter ();

  hb_buffer_t *text_buffer = nullptr;
  if (buffer->flags & HB_BUFFER_FLAG_VERIFY)
  {
    text_buffer = hb_buffer_create ();
    hb_buffer_append (text_buffer, buffer, 0, -1);
  }

  hb_shape_plan_t *shape_plan = hb_shape_plan_create_cached2 (font->face, &buffer->props,
							      features, num_features,
							      font->coords, font->num_coords,
							      shaper_list);

  hb_bool_t res = hb_shape_plan_execute (shape_plan, font, buffer, features, num_features);

  if (buffer->max_ops <= 0)
    buffer->shaping_failed = true;

  hb_shape_plan_destroy (shape_plan);

  if (text_buffer)
  {
    if (res && buffer->successful && !buffer->shaping_failed
	    && text_buffer->successful
	    && !buffer->verify (text_buffer,
				font,
				features,
				num_features,
				shaper_list))
      res = false;
    hb_buffer_destroy (text_buffer);
  }

  buffer->leave ();

  return res;
}

/**
 * hb_shape:
 * @font: an #hb_font_t to use for shaping
 * @buffer: an #hb_buffer_t to shape
 * @features: (array length=num_features) (nullable): an array of user
 *    specified #hb_feature_t or `NULL`
 * @num_features: the length of @features array
 *
 * Shapes @buffer using @font turning its Unicode characters content to
 * positioned glyphs. If @features is not `NULL`, it will be used to control the
 * features applied during shaping. If two @features have the same tag but
 * overlapping ranges the value of the feature with the higher index takes
 * precedence.
 *
 * Since: 0.9.2
 **/
void
hb_shape (hb_font_t           *font,
	  hb_buffer_t         *buffer,
	  const hb_feature_t  *features,
	  unsigned int         num_features)
{
  hb_shape_full (font, buffer, features, num_features, nullptr);
}


static float
buffer_width (hb_buffer_t *buffer)
{
  float w = 0;
  auto *pos = buffer->pos;
  unsigned count = buffer->len;
  if (HB_DIRECTION_IS_HORIZONTAL (buffer->props.direction))
    for (unsigned i = 0; i < count; i++)
      w += pos[i].x_advance;
  else
    for (unsigned i = 0; i < count; i++)
      w += pos[i].y_advance;
  return w;
}

static void
reset_buffer (hb_buffer_t *buffer,
	      hb_array_t<const hb_glyph_info_t> text)
{
  assert (buffer->ensure (text.length));
  buffer->have_positions = false;
  buffer->len = text.length;
  memcpy (buffer->info, text.arrayZ, text.length * sizeof (buffer->info[0]));
  hb_buffer_set_content_type (buffer, HB_BUFFER_CONTENT_TYPE_UNICODE);
}

hb_bool_t
hb_shape_justify (hb_font_t          *font,
		  hb_buffer_t        *buffer,
		  const hb_feature_t *features,
		  unsigned int        num_features,
		  const char * const *shaper_list,
		  float               target_width,
		  float              *width, /* IN/OUT */
		  hb_tag_t           *var_tag, /* OUT */
		  float              *var_value /* OUT */)
{
  // TODO Negative font scales?

  if (target_width == *width)
    return hb_shape_full (font, buffer,
			  features, num_features,
			  shaper_list);

  hb_face_t *face = font->face;

  hb_tag_t tag = HB_TAG_NONE;
  hb_ot_var_axis_info_t axis_info;

  hb_tag_t tags[] =
  {
    HB_TAG ('j','s','t','f'),
    HB_TAG ('w','d','t','h'),
  };
  for (unsigned i = 0; i < ARRAY_LENGTH (tags); i++)
    if (hb_ot_var_find_axis_info (face, tags[i], &axis_info))
    {
      tag = *var_tag = tags[i];
      break;
    }

  if (!tag)
  {
    if (hb_shape_full (font, buffer,
		       features, num_features,
		       shaper_list))
    {
      *width = buffer_width (buffer);
      return true;
    }
    else
      return false;
  }

  unsigned text_len = buffer->len;
  auto *text_info = (hb_glyph_info_t *) hb_malloc (text_len * sizeof (buffer->info[0]));
  if (unlikely (text_len && !text_info))
    return false;
  hb_memcpy (text_info, buffer->info, text_len * sizeof (buffer->info[0]));
  auto text = hb_array<const hb_glyph_info_t> (text_info, text_len);

  if (!*width)
  {
    hb_font_set_variation (font, tag, axis_info.default_value);
    if (!hb_shape_full (font, buffer,
			features, num_features,
			shaper_list))
      return false;
    *width = buffer_width (buffer);
  }

  if (target_width == *width)
    return true;

  double a, b, ya, yb;
  if (*width < target_width)
  {
    ya = *width - target_width;
    a = axis_info.default_value;
    b = axis_info.max_value;

    hb_font_set_variation (font, tag, (float) b);
    reset_buffer (buffer, text);
    if (!hb_shape_full (font, buffer,
			features, num_features,
			shaper_list))
      return false;
    yb = buffer_width (buffer) - target_width;
    if (yb <= 0)
    {
      *width = (float) yb + target_width;
      return true;
    }
  }
  else
  {
    yb = *width - target_width;
    a = axis_info.min_value;
    b = axis_info.default_value;

    hb_font_set_variation (font, tag, (float) a);
    reset_buffer (buffer, text);
    if (!hb_shape_full (font, buffer,
			features, num_features,
			shaper_list))
      return false;
    ya = buffer_width (buffer) - target_width;
    if (ya >= 0)
    {
      *width = (float) ya + target_width;
      return true;
    }
  }

  double epsilon = (b - a) / (1<<14);
  const unsigned n0 = 1;
  const double k1 = 0.2 / (b - a);
  bool failed = false;

  auto f = [&] (double x)
  {
    hb_font_set_variation (font, tag, (float) x);
    reset_buffer (buffer, text);
    if (unlikely (!hb_shape_full (font, buffer,
				  features, num_features,
				  shaper_list)))
    {
      failed = true;
      return target_width;
    }

    return buffer_width (buffer) - target_width;
  };

  double itp = solve_itp (f,
			  a, b,
			  epsilon,
			  n0,
			  k1,
			  ya, yb);

  hb_free (text_info);

  if (failed)
    return false;

  *width = (float) (ya + yb) * 0.5f + target_width;
  *var_value = (float) itp;

  return true;
}


#endif
