/* Pango - Gujarati module
 * 
 * Copyright (C) 2000 SuSE Linux Ltd
 * Author: Robert Brady <robert@suse.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * Licence as published by the Free Software Foundation; either
 * version 2.1 of the Licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public Licence for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * Licence along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 */


#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "pangox.h"

#define RANGE_START 0xA80
#define RANGE_SIZE 0x80

#define ISCII_BASED
#define SCRIPT_STRING "Gujarati"
#define SCRIPT_ENGINE_NAME SCRIPT_STRING "ScriptEngineX"

#include "pango-indic.h"
#include "pango-indic-script.h"
#include "pango-engine.h"

static gboolean is_prefixing_vowel (gunichar i);
static gboolean is_vowel_sign (gunichar i);

static PangoIndicScript script = {
  SCRIPT_STRING,
  &is_prefixing_vowel,
  &is_vowel_sign,
  NULL, /* vowel sign to matra must die! */
  NULL, /* no split vowels for Devanagari */
};
   
#define INTERMEDIATE_HALF_FORM_OFFSET 0xf000
#define FINAL_HALF_FORM_OFFSET 0xe000
#define RA_SUPERSCRIPT 0xeaff
#define RA_SUBSCRIPT 0xeafe

static char *default_charset = "iso10646-guj";

static PangoEngineRange pango_indic_range[] =
{
  {RANGE_START, RANGE_END, "*"},
  {ZERO_WIDTH_JOINER, ZERO_WIDTH_JOINER, "*"},
};

SCRIPT_ENGINE_DEFINITION

static PangoCoverage *
pango_indic_engine_get_coverage (PangoFont * font, PangoLanguage *lang)
{
  gunichar i;
  PangoCoverage *result = pango_coverage_new ();
  PangoXSubfont subfont;

  int dev_font = pango_x_find_first_subfont (font, &default_charset, 1, &subfont);

  if (dev_font)
    {
      for (i = RANGE_START; i <= RANGE_END; i++)
	pango_coverage_set (result, i, PANGO_COVERAGE_EXACT);

      pango_coverage_set (result, ZERO_WIDTH_JOINER, PANGO_COVERAGE_EXACT);
    }

  return result;
}

static gboolean
is_vowel_sign (gunichar i)
{
  /* one that combines, whether or not it spaces */
  return (i >= 0xABE && i <= 0xACC);
}

static int
is_consonant (int i)
{
  return (i >= 0xA95 && i <= 0xAB9) || (i == 0xAE0);
}

static int
is_ind_vowel (int i)
{
  return (i >= 0xA85 && i <= 0xA94);
}

static gboolean
is_prefixing_vowel (gunichar i)
{
   return i == 0xABF;
}

static void 
dev_mini_shuffle (gunichar *s, gunichar *e) 
{
  gunichar *dest = s;
  while (s < e) 
    {
      if (*s) 
	{
	  *dest = *s;
	  dest++;
	}
      s++;
    }
  while (dest < e) 
    {
      *dest = 0;
      dest++;
    }
}

static int 
is_intermediate_form (int q) 
{
  return (q >= 0xf000 && q <= 0xffff);
}

static int 
is_consonantal_form (int q) 
{
  return (q == ZERO_WIDTH_JOINER) || is_consonant (q) || (q >= 0xc000);
}

static int 
nominal_form (int q) 
{
  return q - 0xf000;
}

static int
half_form (int q) 
{
  return (q & 0xfff) + 0xe000;
}

static void 
shuffle_one_along (gunichar *start, gunichar *end) 
{
  end--;
  if (*end != 0) 
    {
      return;
    }
  while (end > start) 
    {
      end[0] = end[-1];
      end--;
    }
  start[0] = 0;
}

static void
pango_indic_make_ligs (gunichar * start, gunichar * end)
{
  int num = end - start;
  int i;

  for (i = 0; i < (end - start); i++)
    {
      gunichar t0 = pango_indic_get_char (start + i, end);
      gunichar t1 = pango_indic_get_char (start + 1 + i, end);

      if (is_consonant (t0) && t1 == VIRAMA)
	{
	  start[i+0] = t0 + INTERMEDIATE_HALF_FORM_OFFSET;
	  start[i+1] = 0;
	}
    }

  while (num && !is_consonant (start[num-1])) 
    {
      num--;
    }
  if (num > 2 && start[0] == INTERMEDIATE_HALF_FORM_OFFSET + RA) 
    {
      for (i=1;i<num;i++) 
	{
	  start[i-1] = start[i];
	}
      start[num-1] = RA_SUPERSCRIPT;
    }
  
  dev_mini_shuffle (start, end);

  for (i = 0; i < (end - start - 1); i++)
    if (is_intermediate_form (start[i])) 
      {
	if (start[i+1] == RA) 
	  {
	    start[i] = nominal_form (start[i]);
	    start[i+1] = RA_SUBSCRIPT;
	  }
	else if (start[i+1] == (RA + INTERMEDIATE_HALF_FORM_OFFSET)) 
	  {
	    start[i] = nominal_form (start[i]);
	    start[i+1] = RA_SUBSCRIPT;
	    shuffle_one_along (start+2, end);
	    start[i+2] = VIRAMA;
	  }
      }
}

static void
pango_indic_engine_shape (PangoFont * font,
			 const char *text,
			 int length,
			 PangoAnalysis * analysis, PangoGlyphString * glyphs)
{
  PangoXSubfont subfont;

  int n_chars, n_glyph;
  int lvl;
  int i;
  gunichar *wc;
  int sb;
  int n_syls;
  gunichar **syls = g_malloc (sizeof (gunichar **));

  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);
  g_return_if_fail (length >= 0);
  g_return_if_fail (analysis != NULL);

  n_chars = n_glyph = g_utf8_strlen (text, length);
  lvl = pango_x_find_first_subfont (font, &default_charset, 1, &subfont);
  if (!lvl)
    {
      pango_x_fallback_shape (font, glyphs, text, n_chars);
      return;
    }

  pango_indic_split_out_characters (&script, text, n_chars, &wc, 
				    &n_glyph, glyphs);
  pango_indic_convert_vowels (&script, TRUE, &n_glyph, wc, FALSE);

  n_syls = 1;
  syls[0] = wc;
  sb = glyphs->log_clusters[0];
  for (i = 0; i < n_chars; i++)
    {
      if (i && (is_consonant (wc[i]) | is_ind_vowel (wc[i]))
	  && wc[i - 1] != VIRAMA)
	{
	  syls = g_realloc (syls, ((n_syls + 2) * sizeof (gunichar **)));
	  syls[n_syls] = wc + i;
	  n_syls++;
	  sb = glyphs->log_clusters[i];
	}
      glyphs->log_clusters[i] = sb;
    }
  syls[n_syls] = wc + i;

  for (i = 0; i < n_syls; i++)
    {
      pango_indic_make_ligs (syls[i], syls[i+1]);
      pango_indic_shift_vowels (&script, syls[i], syls[i + 1]);
    }

  pango_indic_compact (&script, &n_glyph, wc, glyphs->log_clusters);
  pango_x_apply_ligatures (font, subfont, &wc, &n_glyph, &glyphs->log_clusters);
  pango_indic_compact (&script, &n_glyph, wc, glyphs->log_clusters);
  pango_glyph_string_set_size (glyphs, n_glyph);

  for (i = 0; i < n_glyph; i++)
    {
      PangoRectangle logical_rect;
      if ((i != (n_glyph - 1)) && is_intermediate_form (wc[i]) && 
	  is_consonantal_form (wc[i+1])) 
	{
	  wc[i] = half_form (wc[i]);
	}
      glyphs->glyphs[i].glyph = PANGO_X_MAKE_GLYPH (subfont, wc[i]);
      pango_font_get_glyph_extents (font, glyphs->glyphs[i].glyph,
				    NULL, &logical_rect);
      glyphs->glyphs[i].geometry.x_offset = 0;
      glyphs->glyphs[i].geometry.y_offset = 0;
      glyphs->glyphs[i].geometry.width = logical_rect.width;
    }
  g_free (syls);
}

static PangoEngine *
pango_indic_engine_x_new ()
{
  PangoEngineShape *result;
  result = g_new (PangoEngineShape, 1);
  result->engine.id = SCRIPT_ENGINE_NAME;
  result->engine.type = PANGO_ENGINE_TYPE_SHAPE;
  result->engine.length = sizeof (result);
  result->script_shape = pango_indic_engine_shape;
  result->get_coverage = pango_indic_engine_get_coverage;
  return (PangoEngine *) result;
}

#ifdef GUJARATI_X_MODULE_PREFIX
#define MODULE_ENTRY(func) _pango_gujarati_x_##func
#else
#define MODULE_ENTRY(func) func
#endif

void
MODULE_ENTRY(script_engine_list) (PangoEngineInfo ** engines, int *n_engines)
{
  *engines = script_engines;
  *n_engines = n_script_engines;
}

PangoEngine *
MODULE_ENTRY(script_engine_load) (const char *id)
{
  if (!strcmp (id, SCRIPT_ENGINE_NAME))
    return pango_indic_engine_x_new ();
  else
    return NULL;
}

void
MODULE_ENTRY(script_engine_unload) (PangoEngine * engine)
{
}
