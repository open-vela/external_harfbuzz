/*
 * Copyright © 2010  Behdad Esfahbod
 * Copyright © 2011,2012  Google, Inc.
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

#include "batch.hh"
#include "font-options.hh"

const unsigned DEFAULT_FONT_SIZE = FONT_SIZE_UPEM;
const unsigned SUBPIXEL_BITS = 0;

static void
_hb_ot_name_get_utf8 (hb_face_t       *face,
		      hb_ot_name_id_t  name_id,
		      hb_language_t    language,
		      unsigned int    *text_size /* IN/OUT */,
		      char            *text      /* OUT */)
{
  static hb_language_t en = hb_language_from_string ("en", -1);

  unsigned len = *text_size;
  if (!hb_ot_name_get_utf8 (face, name_id,
			    language,
			    &len, text))
  {
    len = *text_size;
    hb_ot_name_get_utf8 (face, name_id,
			 en,
			 &len, text);
  }
  *text_size = len;
}

struct info_t
{
  void add_options (option_parser_t *parser)
  {
    GOptionEntry misc_entries[] =
    {
      {"direction",	0, 0, G_OPTION_ARG_STRING,	&this->direction_str,		"Set direction (default: ltr)",		"ltr/rtl/ttb/btt"},
      {"script",	0, 0, G_OPTION_ARG_STRING,	&this->script_str,		"Set script (default: none)",		"ISO-15924 tag; eg. 'Latn'"},
      {"language",	0, 0, G_OPTION_ARG_STRING,	&this->language_str,		"Set language (default: $LANG)",	"BCP 47 tag; eg. 'en'"},
      {"ot-script",	0, 0, G_OPTION_ARG_STRING,	&this->ot_script_str,		"Set OpenType script tag (default: none)","tag; eg. 'latn'"},
      {"ot-language",	0, 0, G_OPTION_ARG_STRING,	&this->ot_language_str,		"Set OpenType language tag (default: none)",	"tag; eg. 'ENG'"},

      {nullptr}
    };
    parser->add_group (misc_entries,
		       "misc",
		       "Miscellaneaous options:",
		       "Miscellaneaous options affecting queries",
		       this,
		       false /* We add below. */);

    GOptionEntry query_entries[] =
    {
      {"all",		'a', 0, G_OPTION_ARG_NONE,	&this->all,			"Show everything",		nullptr},

      {"show-all",	0, 0, G_OPTION_ARG_NONE,	&this->show_all,		"Show all short information (default)",	nullptr},
      {"show-face-count",0, 0, G_OPTION_ARG_NONE,	&this->show_face_count,		"Show face count",		nullptr},
      {"show-family",	0, 0, G_OPTION_ARG_NONE,	&this->show_family,		"Show family name",		nullptr},
      {"show-style",	0, 0, G_OPTION_ARG_NONE,	&this->show_style,		"Show style name",		nullptr},
      {"show-unique-name",0, 0, G_OPTION_ARG_NONE,	&this->show_unique_name,	"Show unique name",		nullptr},
      {"show-full-name",0, 0, G_OPTION_ARG_NONE,	&this->show_full_name,		"Show full name",		nullptr},
      {"show-postscript-name",0, 0, G_OPTION_ARG_NONE,	&this->show_postscript_name,	"Show Postscript name",		nullptr},
      {"show-version",	0, 0, G_OPTION_ARG_NONE,	&this->show_version,		"Show version",			nullptr},
      {"show-technology",0, 0, G_OPTION_ARG_NONE,	&this->show_technology,		"Show technology",		nullptr},
      {"show-unicode-count",0, 0, G_OPTION_ARG_NONE,	&this->show_unicode_count,	"Show Unicode count",		nullptr},
      {"show-glyph-count",0, 0, G_OPTION_ARG_NONE,	&this->show_glyph_count,	"Show glyph count",		nullptr},
      {"show-upem",	0, 0, G_OPTION_ARG_NONE,	&this->show_upem,		"Show Units-Per-EM",		nullptr},
      {"show-extents",	0, 0, G_OPTION_ARG_NONE,	&this->show_extents,		"Show extents",			nullptr},

      {"get-name",	0, 0, G_OPTION_ARG_STRING_ARRAY,&this->get_name,		"Get name",			"name id; eg. '13'"},
      {"get-metric",	0, 0, G_OPTION_ARG_STRING_ARRAY,&this->get_metric,		"Get metric",			"metric tag; eg. 'hasc'"},
      {"get-baseline",	0, 0, G_OPTION_ARG_STRING_ARRAY,&this->get_baseline,		"Get baseline",			"baseline tag; eg. 'hang'"},
      {"get-table",	0, 0, G_OPTION_ARG_STRING,	&this->get_table,		"Get font table",		"table tag; eg. 'cmap'"},

      {"list-all",	0, 0, G_OPTION_ARG_NONE,	&this->list_all,		"List all long information",	nullptr},
      {"list-names",	0, 0, G_OPTION_ARG_NONE,	&this->list_names,		"List names",			nullptr},
      {"list-tables",	'l', 0, G_OPTION_ARG_NONE,	&this->list_tables,		"List tables",			nullptr},
      {"list-unicodes",	0, 0, G_OPTION_ARG_NONE,	&this->list_unicodes,		"List characters",		nullptr},
      {"list-glyphs",	0, 0, G_OPTION_ARG_NONE,	&this->list_glyphs,		"List glyphs",			nullptr},
      {"list-scripts",	0, 0, G_OPTION_ARG_NONE,	&this->list_scripts,		"List layout scripts",		nullptr},
      {"list-features",	0, 0, G_OPTION_ARG_NONE,	&this->list_features,		"List layout features",		nullptr},
#ifndef HB_NO_VAR
      {"list-variations",0, 0, G_OPTION_ARG_NONE,	&this->list_variations,		"List variations",		nullptr},
#endif
      {"list-palettes",	0, 0, G_OPTION_ARG_NONE,	&this->list_palettes,		"List color palettes",		nullptr},

      {nullptr}
    };
    parser->add_group (query_entries,
		       "query",
		       "Query options:",
		       "Options to query the font instance",
		       this,
		       true);
  }

  protected:
  hb_blob_t *blob = nullptr;
  hb_face_t *face = nullptr;
  hb_font_t *font = nullptr;

  hb_bool_t verbose = true;
  hb_bool_t first_item = true;

  char *direction_str = nullptr;
  char *script_str = nullptr;
  char *language_str = nullptr;
  hb_direction_t direction = HB_DIRECTION_LTR;
  hb_script_t script = HB_SCRIPT_INVALID;
  hb_language_t language = HB_LANGUAGE_INVALID;
  char *ot_script_str = nullptr;
  char *ot_language_str = nullptr;

  hb_bool_t all = false;

  hb_bool_t show_all = false;
  hb_bool_t show_face_count = false;
  hb_bool_t show_family = false;
  hb_bool_t show_style = false;
  hb_bool_t show_unique_name = false;
  hb_bool_t show_full_name = false;
  hb_bool_t show_postscript_name = false;
  hb_bool_t show_version = false;
  hb_bool_t show_technology = false;
  hb_bool_t show_unicode_count = false;
  hb_bool_t show_glyph_count = false;
  hb_bool_t show_upem = false;
  hb_bool_t show_extents = false;

  char **get_name = nullptr;
  char **get_metric = nullptr;
  char **get_baseline = nullptr;
  char *get_table = nullptr;

  hb_bool_t list_all = false;
  hb_bool_t list_names = false;
  hb_bool_t list_tables = false;
  hb_bool_t list_unicodes = false;
  hb_bool_t list_glyphs = false;
  hb_bool_t list_scripts = false;
  hb_bool_t list_features = false;
#ifndef HB_NO_VAR
  hb_bool_t list_variations = false;
#endif
  hb_bool_t list_palettes = false;

  public:

  void
  post_parse (GError **error)
  {
    if (direction_str)
      direction = hb_direction_from_string (direction_str, -1);
    if (script_str)
      script = hb_script_from_string (script_str, -1);
    language = hb_language_get_default ();
    if (language_str)
      language = hb_language_from_string (language_str, -1);
  }

  template <typename app_t>
  void operator () (app_t *app)
  {
    blob = hb_blob_reference (((font_options_t *) app)->blob);
    face = hb_face_reference (((font_options_t *) app)->face);
    font = hb_font_reference (((font_options_t *) app)->font);
    verbose = !app->quiet;

    if (all)
    {
      show_all =
      list_all =
      true;
    }

    if (show_all)
    {
      show_face_count =
      show_family =
      show_style =
      show_unique_name =
      show_full_name =
      show_postscript_name =
      show_version =
      show_technology =
      show_unicode_count =
      show_glyph_count =
      show_upem =
      show_extents =
      true;
      first_item = false;
    }

    if (list_all)
    {
      list_names =
      list_tables =
      list_unicodes =
      list_glyphs =
      list_scripts =
      list_features =
#ifndef HB_NO_VAR
      list_variations =
#endif
      list_palettes =
      true;
    }

    if (show_face_count)  _show_face_count ();
    if (show_family)	  _show_family ();
    if (show_style)	  _show_style ();
    if (show_unique_name) _show_unique_name ();
    if (show_full_name)	  _show_full_name ();
    if (show_postscript_name)_show_postscript_name ();
    if (show_version)	  _show_version ();
    if (show_technology)  _show_technology ();
    if (show_unicode_count)_show_unicode_count ();
    if (show_glyph_count) _show_glyph_count ();
    if (show_upem)	  _show_upem ();
    if (show_extents)	  _show_extents ();

    if (get_name)	  _get_name ();
    if (get_metric)	  _get_metric ();
    if (get_baseline)	  _get_baseline ();
    if (get_table)	  _get_table ();

    if (list_names)	  _list_names ();
    if (list_tables)	  _list_tables ();
    if (list_unicodes)	  _list_unicodes ();
    if (list_glyphs)	  _list_glyphs ();
    if (list_scripts)	  _list_scripts ();
    if (list_features)	  _list_features ();
#ifndef HB_NO_VAR
    if (list_variations)  _list_variations ();
#endif
    if (list_palettes)	  _list_palettes ();

    hb_font_destroy (font);
    hb_face_destroy (face);
    hb_blob_destroy (blob);
  }

  protected:

  void separator ()
  {
    if (first_item)
    {
      first_item = false;
      return;
    }
    printf ("\n===\n\n");
  }

  void
  _show_face_count ()
  {
    printf ("Face count: %u\n", hb_face_count (blob));
  }

  void
  _show_name (const char *label, hb_ot_name_id_t name_id)
  {
    if (verbose)
    {
      printf ("%s: ", label);
    }

    char name[16384];
    unsigned name_len = sizeof name;
    _hb_ot_name_get_utf8 (face, name_id,
			  language,
			  &name_len, name);

    printf ("%s\n", name);
  }
  void _show_family ()		{ _show_name ("Family", 1); }
  void _show_style ()
  {
    hb_ot_name_id_t name_id = 2;

    unsigned named_instance = hb_font_get_var_named_instance (font);
    if (named_instance != HB_FONT_NO_VAR_NAMED_INSTANCE)
      name_id = hb_ot_var_named_instance_get_subfamily_name_id (face, named_instance);

    _show_name ("Style", name_id);
  }
  void _show_unique_name ()	{ _show_name ("Unique name", 3); }
  void _show_full_name ()	{ _show_name ("Full name", 4); }
  void _show_postscript_name ()
  {
    hb_ot_name_id_t name_id = 6;

    unsigned named_instance = hb_font_get_var_named_instance (font);
    if (named_instance != HB_FONT_NO_VAR_NAMED_INSTANCE)
      name_id = hb_ot_var_named_instance_get_postscript_name_id (face, named_instance);


    _show_name ("Postscript name", name_id);
  }
  void _show_version ()		{ _show_name ("Version", 5); }

  bool _has_blob (hb_tag_t tag)
  {
    hb_blob_t *blob = hb_face_reference_table (face, tag);
    bool ret = hb_blob_get_length (blob);
    hb_blob_destroy (blob);
    return ret;
  }

  void _show_technology ()
  {
    if (_has_blob (HB_TAG('g','l','y','f')))
      printf ("Has TrueType outlines\n");
    if (_has_blob (HB_TAG('C','F','F',' ')) || _has_blob (HB_TAG('C','F','F','2')))
      printf ("Has Postscript outlines\n");

    if (_has_blob (HB_TAG('f','p','g','m')) || _has_blob (HB_TAG('p','r','e','p')) || _has_blob (HB_TAG('c','v','t',' ')))
      printf ("Has TrueType hinting\n");

    if (_has_blob (HB_TAG('G','S','U','B')) || _has_blob (HB_TAG('G','P','O','S')))
      printf ("Has OpenType layout\n");
    if (_has_blob (HB_TAG('m','o','r','x')) || _has_blob (HB_TAG('k','e','r','x')))
      printf ("Has AAT layout\n");
    if (_has_blob (HB_TAG('S','i','l','f')))
      printf ("Has Graphite layout\n");
    if (_has_blob (HB_TAG('k','e','r','n')))
      printf ("Has legacy kerning\n");

    if (_has_blob (HB_TAG('E','B','D','T')))
      printf ("Has monochrome bitmaps\n");

    if (_has_blob (HB_TAG('C','B','D','T')) || _has_blob (HB_TAG('s','b','i','x')))
      printf ("Has color bitmaps\n");
    if (_has_blob (HB_TAG('S','V','G',' ')))
      printf ("Has color SVGs\n");
    if (_has_blob (HB_TAG('C','O','L','R')))
      printf ("Has color paintings\n");

    if (_has_blob (HB_TAG('f','v','a','r')))  printf ("Has variations\n");
  }

  void _show_unicode_count ()
  {
    if (verbose)
    {
      printf ("Unicode count: ");
    }

    hb_set_t *unicodes = hb_set_create ();
    hb_face_collect_unicodes (face, unicodes);

    printf ("%u\n", hb_set_get_population (unicodes));

    hb_set_destroy (unicodes);
  }

  void _show_glyph_count ()
  {
    if (verbose)
    {
      printf ("Glyph count: ");
    }

    printf ("%u\n", hb_face_get_glyph_count (face));
  }

  void _show_upem ()
  {
    if (verbose)
    {
      printf ("Units-Per-EM: ");
    }

    printf ("%u\n", hb_face_get_upem (face));
  }

  void _show_extents ()
  {
    hb_font_extents_t extents;
    hb_font_get_extents_for_direction (font, direction, &extents);

    if (verbose) printf ("Ascender: ");
    printf ("%d\n", extents.ascender);

    if (verbose) printf ("Descender: ");
    printf ("%d\n", extents.descender);

    if (verbose) printf ("Line gap: ");
    printf ("%d\n", extents.line_gap);
  }

  void _get_name ()
  {
    for (char **p = get_name; *p; p++)
    {
      hb_ot_name_id_t name_id = (hb_ot_name_id_t) atoi (*p);
      _show_name (*p, name_id);
    }
  }

  void _get_metric ()
  {
    bool fallback = false;
    for (char **p = get_metric; *p; p++)
    {
      hb_ot_metrics_tag_t tag = (hb_ot_metrics_tag_t) hb_tag_from_string (*p, -1);
      hb_position_t position;

      if (verbose)
	printf ("Metric %c%c%c%c: ", HB_UNTAG (tag));

      if (hb_ot_metrics_get_position (font, tag, &position))
	printf ("%d\n", position);
      else
      {
	hb_ot_metrics_get_position_with_fallback (font, tag, &position);
	printf ("%d	*\n", position);
	fallback = true;
      }
    }

    if (verbose && fallback)
    {
      printf ("\n[*] Fallback value\n");
    }
  }

  void _get_baseline ()
  {

    hb_tag_t script_tags[HB_OT_MAX_TAGS_PER_SCRIPT];
    hb_tag_t language_tags[HB_OT_MAX_TAGS_PER_LANGUAGE];
    unsigned script_count = HB_OT_MAX_TAGS_PER_SCRIPT;
    unsigned language_count = HB_OT_MAX_TAGS_PER_LANGUAGE;

    hb_ot_tags_from_script_and_language (script, language,
					 &script_count, script_tags,
					 &language_count, language_tags);

    hb_tag_t script_tag = script_count ? script_tags[script_count - 1] : HB_TAG_NONE;
    hb_tag_t language_tag = language_count ? language_tags[0] : HB_TAG_NONE;

    if (ot_script_str)
      script_tag = hb_tag_from_string (ot_script_str, -1);
    if (ot_language_str)
      language_tag = hb_tag_from_string (ot_language_str, -1);

    bool fallback = false;
    for (char **p = get_baseline; *p; p++)
    {
      hb_ot_layout_baseline_tag_t tag = (hb_ot_layout_baseline_tag_t) hb_tag_from_string (*p, -1);
      hb_position_t position;

      if (verbose)
	printf ("Baseline %c%c%c%c: ", HB_UNTAG (tag));

      if (hb_ot_layout_get_baseline (font, tag, direction, script_tag, language_tag, &position))
	printf ("%d\n", position);
      else
      {
	hb_ot_layout_get_baseline_with_fallback (font, tag, direction, script_tag, language_tag, &position);
	printf ("%d	*\n", position);
	fallback = true;
      }
    }

    if (verbose && fallback)
    {
      printf ("\n[*] Fallback value\n");
    }
  }

  void
  _get_table ()
  {
    hb_blob_t *blob = hb_face_reference_table (face, hb_tag_from_string (get_table, -1));
    unsigned count = 0;
    const char *data = hb_blob_get_data (blob, &count);
    fwrite (data, 1, count, stdout);
    hb_blob_destroy (blob);
  }

  void _list_names ()
  {
    if (verbose)
    {
      separator ();
      printf ("Name information:\n\n");
      printf ("Id	Text\n------------\n");
    }

    unsigned count;
    const auto *entries = hb_ot_name_list_names (face, &count);
    for (unsigned i = 0; i < count; i++)
    {
      char name[16384];
      unsigned name_len = sizeof name;
      _hb_ot_name_get_utf8 (face, entries[i].name_id,
			    language,
			    &name_len, name);

      printf ("%u	%s\n", entries[i].name_id, name);
    }
  }

  void _list_tables ()
  {
    if (verbose)
    {
      separator ();
      printf ("Table information:\n\n");
      printf ("Tag	Size\n------------\n");
    }

    unsigned count = hb_face_get_table_tags (face, 0, nullptr, nullptr);
    hb_tag_t *tags = (hb_tag_t *) calloc (count, sizeof (hb_tag_t));
    hb_face_get_table_tags (face, 0, &count, tags);

    for (unsigned i = 0; i < count; i++)
    {
      hb_tag_t tag = tags[i];

      hb_blob_t *blob = hb_face_reference_table (face, tag);

      printf ("%c%c%c%c %8u bytes\n", HB_UNTAG (tag), hb_blob_get_length (blob));

      hb_blob_destroy (blob);
    }

    free (tags);
  }

  void
  _list_unicodes ()
  {
    if (verbose)
    {
      separator ();
      printf ("Character-set information:\n\n");
      printf ("Unicode	Glyph name\n------------------\n");
    }

    hb_set_t *unicodes = hb_set_create ();
    hb_map_t *cmap = hb_map_create ();

    hb_face_collect_nominal_glyph_mapping (face, cmap, unicodes);

    for (hb_codepoint_t u = HB_SET_VALUE_INVALID;
	 hb_set_next (unicodes, &u);)
    {
      hb_codepoint_t gid = hb_map_get (cmap, u);

      char glyphname[64];
      hb_font_glyph_to_string (font, gid,
			       glyphname, sizeof glyphname);

      printf ("U+%04X	%s\n", u, glyphname);
    }

    hb_map_destroy (cmap);


    /* List variation-selector sequences. */
    hb_set_t *vars = hb_set_create ();

    hb_face_collect_variation_selectors (face, vars);

    for (hb_codepoint_t vs = HB_SET_VALUE_INVALID;
	 hb_set_next (vars, &vs);)
    {
      hb_set_clear (unicodes);
      hb_face_collect_variation_unicodes (face, vs, unicodes);

      for (hb_codepoint_t u = HB_SET_VALUE_INVALID;
	   hb_set_next (unicodes, &u);)
      {
	hb_codepoint_t gid = 0;
	bool b = hb_font_get_variation_glyph (font, u, vs, &gid);
	assert (b);

	char glyphname[64];
	hb_font_glyph_to_string (font, gid,
				 glyphname, sizeof glyphname);

	printf ("U+%04X,U+%04X	%s\n", vs, u, glyphname);
      }
    }

    hb_set_destroy (vars);
    hb_set_destroy (unicodes);
  }

  void
  _list_glyphs ()
  {
    if (verbose)
    {
      separator ();
      printf ("Glyph-set information:\n\n");
      printf ("GlyphID	Glyph name\n------------------\n");
    }

    unsigned num_glyphs = hb_face_get_glyph_count (face);

    for (hb_codepoint_t gid = 0; gid < num_glyphs; gid++)
    {
      char glyphname[64];
      hb_font_glyph_to_string (font, gid,
			       glyphname, sizeof glyphname);

      printf ("%u	%s\n", gid, glyphname);
    }
  }

  void
  _list_scripts ()
  {
    if (verbose)
    {
      separator ();
      printf ("Layout script information:\n\n");
    }

    hb_tag_t table_tags[] = {HB_OT_TAG_GSUB, HB_OT_TAG_GPOS, HB_TAG_NONE};

    for (unsigned int i = 0; table_tags[i]; i++)
    {
      if (verbose) printf ("Table: ");
      printf ("%c%c%c%c\n", HB_UNTAG (table_tags[i]));

      hb_tag_t script_array[32];
      unsigned script_count = sizeof script_array / sizeof script_array[0];
      unsigned script_offset = 0;
      do
      {
	hb_ot_layout_table_get_script_tags (face, table_tags[i],
					    script_offset,
					    &script_count,
					    script_array);

	for (unsigned script_index = 0; script_index < script_count; script_index++)
	{
	  printf ("	");
	  if (verbose) printf ("Script: ");

	  hb_tag_t hb_sc = hb_script_to_iso15924_tag (hb_ot_tag_to_script (script_array[script_index]));
	  if (script_array[script_index] == HB_TAG ('D','F','L','T'))
	    hb_sc = HB_SCRIPT_COMMON;

	  printf ("%c%c%c%c (%c%c%c%c)\n",
		  HB_UNTAG (hb_sc),
		  HB_UNTAG (script_array[script_index]));

	  hb_tag_t language_array[32];
	  unsigned language_count = sizeof language_array / sizeof language_array[0];
	  unsigned language_offset = 0;
	  do
	  {
	    hb_ot_layout_script_get_language_tags (face, table_tags[i],
						   script_offset + script_index,
						   language_offset,
						   &language_count,
						   language_array);

	    for (unsigned language_index = 0; language_index < language_count; language_index++)
	    {
	      printf ("		");
	      if (verbose) printf ("Language: ");
	      printf ("%s (%c%c%c%c)\n",
		      hb_language_to_string (hb_ot_tag_to_language (language_array[language_index])),
		      HB_UNTAG (language_array[language_index]));
	    }

	    language_offset += language_count;
	  }
	  while (language_count == sizeof language_array / sizeof language_array[0]);
	}

	script_offset += script_count;
      }
      while (script_count == sizeof script_array / sizeof script_array[0]);

    }

  }

  void
  _list_features_no_script ()
  {
    if (verbose)
    {
      printf ("Showing all font features with duplicates removed.\n\n");
    }

    hb_tag_t table_tags[] = {HB_OT_TAG_GSUB, HB_OT_TAG_GPOS, HB_TAG_NONE};

    hb_set_t *features = hb_set_create ();

    for (unsigned int i = 0; table_tags[i]; i++)
    {
      if (verbose) printf ("Table: ");
      printf ("%c%c%c%c\n", HB_UNTAG (table_tags[i]));

      hb_set_clear (features);
      hb_tag_t feature_array[32];
      unsigned feature_count = sizeof feature_array / sizeof feature_array[0];
      unsigned feature_offset = 0;
      do
      {
	hb_ot_layout_table_get_feature_tags (face, table_tags[i],
					     feature_offset,
					     &feature_count,
					     feature_array);

	for (unsigned feature_index = 0; feature_index < feature_count; feature_index++)
	{
	  if (hb_set_has (features, feature_array[feature_index]))
	    continue;
	  hb_set_add (features, feature_array[feature_index]);

	  hb_ot_name_id_t label_id;

	  hb_ot_layout_feature_get_name_ids (face,
					     table_tags[i],
					     feature_offset + feature_index,
					     &label_id,
					     nullptr,
					     nullptr,
					     nullptr,
					     nullptr);

	  char name[64];
	  unsigned name_len = sizeof name;

	  _hb_ot_name_get_utf8 (face, label_id,
				language,
				&name_len, name);

	  printf ("	");
	  if (verbose) printf ("Feature: ");
	  printf ("%c%c%c%c", HB_UNTAG (feature_array[feature_index]));

	  if (*name)
	    printf ("	%s", name);

	  printf ("\n");
	}

	feature_offset += feature_count;
      }
      while (feature_count == sizeof feature_array / sizeof feature_array[0]);
    }

    hb_set_destroy (features);
  }

  void
  _list_features ()
  {
    if (verbose)
    {
      separator ();
      printf ("Layout features information:\n\n");
    }

    hb_tag_t table_tags[] = {HB_OT_TAG_GSUB, HB_OT_TAG_GPOS, HB_TAG_NONE};

    if (script == HB_SCRIPT_INVALID && !ot_script_str)
    {
      _list_features_no_script ();
      return;
    }

    for (unsigned int i = 0; table_tags[i]; i++)
    {
      if (verbose) printf ("Table: ");
      printf ("%c%c%c%c\n", HB_UNTAG (table_tags[i]));

      auto table_tag = table_tags[i];

      hb_tag_t script_tags[HB_OT_MAX_TAGS_PER_SCRIPT];
      hb_tag_t language_tags[HB_OT_MAX_TAGS_PER_LANGUAGE];
      unsigned script_count = HB_OT_MAX_TAGS_PER_SCRIPT;
      unsigned language_count = HB_OT_MAX_TAGS_PER_LANGUAGE;

      hb_ot_tags_from_script_and_language (script, language,
					   &script_count, script_tags,
					   &language_count, language_tags);

      if (ot_script_str)
      {
	script_tags[0] = hb_tag_from_string (ot_script_str, -1);
	script_count = 1;
      }
      if (ot_language_str)
      {
	language_tags[0] = hb_tag_from_string (ot_language_str, -1);
	language_count = 1;
      }

      unsigned script_index;
      hb_tag_t chosen_script;
      hb_ot_layout_table_select_script (face, table_tag,
					script_count, script_tags,
					&script_index, &chosen_script);

      unsigned language_index;
      hb_tag_t chosen_language;
      hb_ot_layout_script_select_language2 (face, table_tag,
					   script_index,
					   language_count, language_tags,
					   &language_index, &chosen_language);

      if (verbose)
      {
        if (chosen_script)
	{
	  printf ("	Script: %c%c%c%c\n", HB_UNTAG (chosen_script));
	  if (chosen_language)
	    printf ("	Language: %c%c%c%c\n", HB_UNTAG (chosen_language));
	  else
	    printf ("	Language: Default\n");
	}
      }

      unsigned feature_array[32];
      unsigned feature_count = sizeof feature_array / sizeof feature_array[0];
      unsigned feature_offset = 0;
      do
      {
	hb_ot_layout_language_get_feature_indexes (face, table_tag,
						   script_index, language_index,
						   feature_offset,
						   &feature_count,
						   feature_array);

	for (unsigned feature_index = 0; feature_index < feature_count; feature_index++)
	{
	  hb_ot_name_id_t label_id;

	  hb_ot_layout_feature_get_name_ids (face,
					     table_tags[i],
					     feature_array[feature_index],
					     &label_id,
					     nullptr,
					     nullptr,
					     nullptr,
					     nullptr);

	  char name[64];
	  unsigned name_len = sizeof name;

	  _hb_ot_name_get_utf8 (face, label_id,
				language,
				&name_len, name);

	  printf ("	");
	  if (verbose) printf ("Feature: ");
	  hb_tag_t feature_tag;
	  unsigned f_count = 1;
	  hb_ot_layout_table_get_feature_tags (face, table_tag,
					       feature_array[feature_index],
					       &f_count, &feature_tag);
	  printf ("%c%c%c%c", HB_UNTAG (feature_tag));

	  if (*name)
	    printf ("	%s", name);

	  printf ("\n");
	}

	feature_offset += feature_count;
      }
      while (feature_count == sizeof feature_array / sizeof feature_array[0]);
    }
  }

#ifndef HB_NO_VAR
  void
  _list_variations ()
  {
    if (verbose)
    {
      separator ();
      printf ("Variations information:\n\n");
    }

    hb_ot_var_axis_info_t *axes;

    unsigned count = hb_ot_var_get_axis_infos (face, 0, nullptr, nullptr);
    axes = (hb_ot_var_axis_info_t *) calloc (count, sizeof (hb_ot_var_axis_info_t));
    hb_ot_var_get_axis_infos (face, 0, &count, axes);

    bool has_hidden = false;

    if (verbose && count)
    {
      printf ("Varitation axes:\n\n");
      printf ("Tag	Minimum	Default	Maximum	Name\n------------------------------------\n");
    }
    for (unsigned i = 0; i < count; i++)
    {
      const auto &axis = axes[i];
      if (axis.flags & HB_OT_VAR_AXIS_FLAG_HIDDEN)
	has_hidden = true;

      char name[64];
      unsigned name_len = sizeof name;

      _hb_ot_name_get_utf8 (face, axis.name_id,
			    language,
			    &name_len, name);

      printf ("%c%c%c%c%s	%g	%g	%g	%s\n",
	      HB_UNTAG (axis.tag),
	      axis.flags & HB_OT_VAR_AXIS_FLAG_HIDDEN ? "*" : "",
	      (double) axis.min_value,
	      (double) axis.default_value,
	      (double) axis.max_value,
	      name);
    }
    if (verbose && has_hidden)
      printf ("\n[*] Hidden axis\n");

    free (axes);
    axes = nullptr;

    count = hb_ot_var_get_named_instance_count (face);
    if (count)
    {
      if (verbose)
      {
	printf ("\n\nNamed instances:\n\n");
      printf ("Index	Name				Position\n------------------------------------------------\n");
      }

      for (unsigned i = 0; i < count; i++)
      {
	char name[64];
	unsigned name_len = sizeof name;

	hb_ot_name_id_t name_id = hb_ot_var_named_instance_get_subfamily_name_id (face, i);
	_hb_ot_name_get_utf8 (face, name_id,
			      language,
			      &name_len, name);

	unsigned coords_count = hb_ot_var_named_instance_get_design_coords (face, i, nullptr, nullptr);
	float* coords;
	coords = (float *) calloc (coords_count, sizeof (float));
	hb_ot_var_named_instance_get_design_coords (face, i, &coords_count, coords);

	printf ("%u	%-32s", i, name);
	for (unsigned j = 0; j < coords_count; j++)
	  printf ("%g, ", (double) coords[j]);
	printf ("\n");

	free (coords);
      }
    }
  }
#endif

  void
  _list_palettes ()
  {
    if (verbose)
    {
      separator ();
      printf ("Color palettes information:\n");
    }

    {
      if (verbose)
      {
	printf ("\nPalettes:\n\n");
	printf ("Index	Flags	Name\n---------------------\n");
      }
      unsigned count = hb_ot_color_palette_get_count (face);
      for (unsigned i = 0; i < count; i++)
      {
	hb_ot_name_id_t name_id = hb_ot_color_palette_get_name_id (face, i);
	hb_ot_color_palette_flags_t flags = hb_ot_color_palette_get_flags (face, i);

	char name[64];
	unsigned name_len = sizeof name;

	_hb_ot_name_get_utf8 (face, name_id,
			      language,
			      &name_len, name);
        const char *type = "";
	if (flags)
	{
	  if (flags & HB_OT_COLOR_PALETTE_FLAG_USABLE_WITH_LIGHT_BACKGROUND)
          {
	    if (flags & HB_OT_COLOR_PALETTE_FLAG_USABLE_WITH_DARK_BACKGROUND)
	      type = "Both";
            else
	      type = "Light";
          }
          else {
	    type = "Dark";
          }
	}

	printf ("%u	%s	%s\n", i, type, name);
      }
    }

    {
      if (verbose)
      {
	printf ("\nColors:\n\n");
	printf ("Index	Name\n------------\n");
      }
      unsigned count = hb_ot_color_palette_get_colors (face, 0, 0, nullptr, nullptr);
      for (unsigned i = 0; i < count; i++)
      {
	hb_ot_name_id_t name_id = hb_ot_color_palette_color_get_name_id (face, i);

	char name[64];
	unsigned name_len = sizeof name;
	_hb_ot_name_get_utf8 (face, name_id,
			      language,
			      &name_len, name);

	printf ("%u	%s\n", i, name);
      }
    }
  }
};


template <typename consumer_t,
	  typename font_options_type>
struct main_font_t :
       option_parser_t,
       font_options_type,
       consumer_t
{
  int operator () (int argc, char **argv)
  {
    add_options ();

    if (argc == 2)
      consumer_t::show_all = true;

    parse (&argc, &argv);

    consumer_t::operator () (this);

    return 0;
  }

  protected:

  void add_options ()
  {
    font_options_type::add_options (this);
    consumer_t::add_options (this);

    GOptionEntry entries[] =
    {
      {"quiet",		'q', 0, G_OPTION_ARG_NONE,	&this->quiet,			"Generate machine-readable output",	nullptr},
      {G_OPTION_REMAINING,	0, G_OPTION_FLAG_IN_MAIN,
				G_OPTION_ARG_CALLBACK,	(gpointer) &collect_rest,	nullptr,	"[FONT-FILE]"},
      {nullptr}
    };
    add_main_group (entries, this);
    option_parser_t::add_options ();
  }

  private:

  static gboolean
  collect_rest (const char *name G_GNUC_UNUSED,
		const char *arg,
		gpointer    data,
		GError    **error)
  {
    main_font_t *thiz = (main_font_t *) data;

    if (!thiz->font_file)
    {
      thiz->font_file = g_strdup (arg);
      return true;
    }

    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
		 "Too many arguments on the command line");
    return false;
  }

  public:
  hb_bool_t quiet = false;
};

int
main (int argc, char **argv)
{
  using main_t = main_font_t<info_t, font_options_t>;
  return batch_main<main_t> (argc, argv);
}
