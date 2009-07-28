/*
 * Copyright (C) 1998-2004  David Turner and Werner Lemberg
 * Copyright (C) 2006  Behdad Esfahbod
 * Copyright (C) 2007,2008,2009  Red Hat, Inc.
 *
 *  This is part of HarfBuzz, an OpenType Layout engine library.
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
 */

#define HB_OT_LAYOUT_CC

/* XXX */
#include "hb-buffer-private.h"

#include "hb-ot-layout.h"
#include "hb-ot-layout-private.h"

#include "hb-ot-layout-open-private.h"
#include "hb-ot-layout-gdef-private.h"
#include "hb-ot-layout-gsub-private.h"
#include "hb-ot-layout-gpos-private.h"


#include <stdlib.h>
#include <string.h>


hb_ot_layout_t *
hb_ot_layout_create (void)
{
  hb_ot_layout_t *layout = (hb_ot_layout_t *) calloc (1, sizeof (hb_ot_layout_t));

  layout->gdef = &Null(GDEF);
  layout->gsub = &Null(GSUB);
  layout->gpos = &Null(GPOS);

  return layout;
}

hb_bool_t
hb_ot_layout_has_substitution (hb_ot_layout_t *layout)
{
  return layout->gsub != &Null(GSUB);
}

hb_bool_t
hb_ot_layout_has_positioning (hb_ot_layout_t *layout)
{
  return layout->gpos != &Null(GPOS);
}

hb_ot_layout_t *
hb_ot_layout_create_for_data (const char *font_data,
			      int         face_index)
{
  hb_ot_layout_t *layout;

  if (HB_UNLIKELY (font_data == NULL))
    return hb_ot_layout_create ();

  layout = (hb_ot_layout_t *) calloc (1, sizeof (hb_ot_layout_t));

  const OpenTypeFontFile &font = OpenTypeFontFile::get_for_data (font_data);
  const OpenTypeFontFace &face = font.get_face (face_index);

  layout->gdef = &GDEF::get_for_data (font.get_table_data (face.get_table_by_tag (GDEF::Tag)));
  layout->gsub = &GSUB::get_for_data (font.get_table_data (face.get_table_by_tag (GSUB::Tag)));
  layout->gpos = &GPOS::get_for_data (font.get_table_data (face.get_table_by_tag (GPOS::Tag)));

  return layout;
}

hb_ot_layout_t *
hb_ot_layout_create_for_tables (const char *gdef_data,
				const char *gsub_data,
				const char *gpos_data)
{
  hb_ot_layout_t *layout;
  
  if (HB_UNLIKELY (gdef_data == NULL && gsub_data == NULL && gpos_data == NULL))
    return hb_ot_layout_create ();
  
  layout = (hb_ot_layout_t *) calloc (1, sizeof (hb_ot_layout_t));
  
  layout->gdef = &GDEF::get_for_data (gdef_data);
  layout->gsub = &GSUB::get_for_data (gsub_data);
  layout->gpos = &GPOS::get_for_data (gpos_data);
  
  return layout;
}

void
hb_ot_layout_destroy (hb_ot_layout_t *layout)
{
  free (layout);
}

void
hb_ot_layout_set_direction (hb_ot_layout_t *layout,
			    hb_bool_t r2l)
{
  layout->gpos_info.r2l = !!r2l;
}

void
hb_ot_layout_set_hinting (hb_ot_layout_t *layout,
			  hb_bool_t hinted)
{
  layout->gpos_info.dvi = !hinted;
}

void
hb_ot_layout_set_scale (hb_ot_layout_t *layout,
			hb_16dot16_t x_scale, hb_16dot16_t y_scale)
{
  layout->gpos_info.x_scale = x_scale;
  layout->gpos_info.y_scale = y_scale;
}

void
hb_ot_layout_set_ppem (hb_ot_layout_t *layout,
		       unsigned int x_ppem, unsigned int y_ppem)
{
  layout->gpos_info.x_ppem = x_ppem;
  layout->gpos_info.y_ppem = y_ppem;
}


/*
 * GDEF
 */

/* TODO the public class_t is a mess */

hb_bool_t
hb_ot_layout_has_font_glyph_classes (hb_ot_layout_t *layout)
{
  return layout->gdef->has_glyph_classes ();
}

HB_INTERNAL hb_bool_t
_hb_ot_layout_has_new_glyph_classes (hb_ot_layout_t *layout)
{
  return layout->new_gdef.len > 0;
}

HB_INTERNAL unsigned int
_hb_ot_layout_get_glyph_property (hb_ot_layout_t *layout,
				  hb_codepoint_t  glyph)
{
  hb_ot_layout_class_t klass;

  klass = layout->gdef->get_glyph_class (glyph);

  if (!klass && glyph < layout->new_gdef.len)
    klass = layout->new_gdef.klasses[glyph];

  switch (klass) {
  default:
  case GDEF::UnclassifiedGlyph:	return HB_OT_LAYOUT_GLYPH_CLASS_UNCLASSIFIED;
  case GDEF::BaseGlyph:		return HB_OT_LAYOUT_GLYPH_CLASS_BASE_GLYPH;
  case GDEF::LigatureGlyph:	return HB_OT_LAYOUT_GLYPH_CLASS_LIGATURE;
  case GDEF::ComponentGlyph:	return HB_OT_LAYOUT_GLYPH_CLASS_COMPONENT;
  case GDEF::MarkGlyph:
	/* TODO old harfbuzz doesn't always parse mark attachments as it says it was
	 * introduced without a version bump, so it may not be safe */
	klass = layout->gdef->get_mark_attachment_type (glyph);
	return HB_OT_LAYOUT_GLYPH_CLASS_MARK + (klass << 8);
  }
}

HB_INTERNAL hb_bool_t
_hb_ot_layout_check_glyph_property (hb_ot_layout_t  *layout,
				    hb_glyph_info_t *ginfo,
				    unsigned int     lookup_flags,
				    unsigned int    *property_out)
{
  unsigned int property;

  if (ginfo->gproperty == HB_BUFFER_GLYPH_PROPERTIES_UNKNOWN)
    ginfo->gproperty = _hb_ot_layout_get_glyph_property (layout, ginfo->gindex);
  property = ginfo->gproperty;
  if (property_out)
    *property_out = property;

  /* Not covered, if, for example, glyph class is ligature and
   * lookup_flags includes LookupFlags::IgnoreLigatures
   */
  if (property & lookup_flags)
    return false;

  if (property & HB_OT_LAYOUT_GLYPH_CLASS_MARK)
  {
    /* If using mark filtering sets, the high short of
     * lookup_flags has the set index.
     */
    if (lookup_flags & LookupFlag::UseMarkFilteringSet)
      return layout->gdef->mark_set_covers (lookup_flags >> 16, ginfo->gindex);

    /* The second byte of lookup_flags has the meaning
     * "ignore marks of attachment type different than
     * the attachment type specified."
     */
    if (lookup_flags & LookupFlag::MarkAttachmentType && property & LookupFlag::MarkAttachmentType)
      return (lookup_flags & LookupFlag::MarkAttachmentType) == (property & LookupFlag::MarkAttachmentType);
  }

  return true;
}

HB_INTERNAL hb_bool_t
_hb_ot_layout_skip_mark (hb_ot_layout_t  *layout,
			 hb_glyph_info_t *ginfo,
			 unsigned int     lookup_flags,
			 unsigned int    *property_out)
{
  unsigned int property;

  if (ginfo->gproperty == HB_BUFFER_GLYPH_PROPERTIES_UNKNOWN)
    ginfo->gproperty = _hb_ot_layout_get_glyph_property (layout, ginfo->gindex);
  property = ginfo->gproperty;
  if (property_out)
    *property_out = property;

  if (property & HB_OT_LAYOUT_GLYPH_CLASS_MARK)
  {
    /* Skip mark if lookup_flags includes LookupFlags::IgnoreMarks */
    if (lookup_flags & LookupFlag::IgnoreMarks)
      return true;

    /* If using mark filtering sets, the high short of lookup_flags has the set index. */
    if (lookup_flags & LookupFlag::UseMarkFilteringSet)
      return !layout->gdef->mark_set_covers (lookup_flags >> 16, ginfo->gindex);

    /* The second byte of lookup_flags has the meaning "ignore marks of attachment type
     * different than the attachment type specified." */
    if (lookup_flags & LookupFlag::MarkAttachmentType && property & LookupFlag::MarkAttachmentType)
      return (lookup_flags & LookupFlag::MarkAttachmentType) != (property & LookupFlag::MarkAttachmentType);
  }

  return false;
}

HB_INTERNAL void
_hb_ot_layout_set_glyph_property (hb_ot_layout_t *layout,
				  hb_codepoint_t  glyph,
				  unsigned int    property)
{ hb_ot_layout_set_glyph_class (layout, glyph, (hb_ot_layout_glyph_class_t) (property & 0xff)); }


hb_ot_layout_glyph_class_t
hb_ot_layout_get_glyph_class (hb_ot_layout_t *layout,
			      hb_codepoint_t  glyph)
{ return (hb_ot_layout_glyph_class_t) (_hb_ot_layout_get_glyph_property (layout, glyph) & 0xff); }

void
hb_ot_layout_set_glyph_class (hb_ot_layout_t             *layout,
			      hb_codepoint_t              glyph,
			      hb_ot_layout_glyph_class_t  klass)
{
  /* TODO optimize this, similar to old harfbuzz code for example */

  hb_ot_layout_class_t gdef_klass;
  int len = layout->new_gdef.len;

  if (HB_UNLIKELY (glyph > 65535))
    return;

  if (glyph >= len) {
    int new_len;
    unsigned char *new_klasses;

    new_len = len == 0 ? 120 : 2 * len;
    if (new_len > 65535)
      new_len = 65535;
    new_klasses = (unsigned char *) realloc (layout->new_gdef.klasses, new_len * sizeof (unsigned char));

    if (HB_UNLIKELY (!new_klasses))
      return;

    memset (new_klasses + len, 0, new_len - len);

    layout->new_gdef.klasses = new_klasses;
    layout->new_gdef.len = new_len;
  }

  switch (klass) {
  default:
  case HB_OT_LAYOUT_GLYPH_CLASS_UNCLASSIFIED:	gdef_klass = GDEF::UnclassifiedGlyph;	break;
  case HB_OT_LAYOUT_GLYPH_CLASS_BASE_GLYPH:	gdef_klass = GDEF::BaseGlyph;		break;
  case HB_OT_LAYOUT_GLYPH_CLASS_LIGATURE:	gdef_klass = GDEF::LigatureGlyph;	break;
  case HB_OT_LAYOUT_GLYPH_CLASS_MARK:		gdef_klass = GDEF::MarkGlyph;		break;
  case HB_OT_LAYOUT_GLYPH_CLASS_COMPONENT:	gdef_klass = GDEF::ComponentGlyph;	break;
  }

  layout->new_gdef.klasses[glyph] = gdef_klass;
  return;
}

void
hb_ot_layout_build_glyph_classes (hb_ot_layout_t *layout,
				  uint16_t        num_total_glyphs,
				  hb_codepoint_t *glyphs,
				  unsigned char  *klasses,
				  uint16_t        count)
{
  if (HB_UNLIKELY (!count || !glyphs || !klasses))
    return;

  if (layout->new_gdef.len == 0) {
    layout->new_gdef.klasses = (unsigned char *) calloc (num_total_glyphs, sizeof (unsigned char));
    layout->new_gdef.len = count;
  }

  for (unsigned int i = 0; i < count; i++)
    hb_ot_layout_set_glyph_class (layout, glyphs[i], (hb_ot_layout_glyph_class_t) klasses[i]);
}

hb_bool_t
hb_ot_layout_get_attach_points (hb_ot_layout_t *layout,
				hb_codepoint_t  glyph,
				unsigned int   *point_count /* IN/OUT */,
				unsigned int   *point_array /* OUT */)
{
  return layout->gdef->get_attach_points (glyph, point_count, point_array);
}

hb_bool_t
hb_ot_layout_get_lig_carets (hb_ot_layout_t *layout,
			     hb_codepoint_t  glyph,
			     unsigned int   *caret_count /* IN/OUT */,
			     int            *caret_array /* OUT */)
{
  return layout->gdef->get_lig_carets (layout, glyph, caret_count, caret_array);
}

/*
 * GSUB/GPOS
 */

static const GSUBGPOS&
get_gsubgpos_table (hb_ot_layout_t            *layout,
		    hb_ot_layout_table_type_t  table_type)
{
  switch (table_type) {
    case HB_OT_LAYOUT_TABLE_TYPE_GSUB: return *(layout->gsub);
    case HB_OT_LAYOUT_TABLE_TYPE_GPOS: return *(layout->gpos);
    default:                           return Null(GSUBGPOS);
  }
}


unsigned int
hb_ot_layout_table_get_script_count (hb_ot_layout_t            *layout,
				     hb_ot_layout_table_type_t  table_type)
{
  const GSUBGPOS &g = get_gsubgpos_table (layout, table_type);

  return g.get_script_count ();
}

hb_tag_t
hb_ot_layout_table_get_script_tag (hb_ot_layout_t            *layout,
				   hb_ot_layout_table_type_t  table_type,
				   unsigned int               script_index)
{
  const GSUBGPOS &g = get_gsubgpos_table (layout, table_type);

  return g.get_script_tag (script_index);
}

hb_bool_t
hb_ot_layout_table_find_script (hb_ot_layout_t            *layout,
				hb_ot_layout_table_type_t  table_type,
				hb_tag_t                   script_tag,
				unsigned int              *script_index)
{
  ASSERT_STATIC (NO_INDEX == HB_OT_LAYOUT_NO_SCRIPT_INDEX);
  const GSUBGPOS &g = get_gsubgpos_table (layout, table_type);

  if (g.find_script_index (script_tag, script_index))
    return TRUE;

  /* try finding 'DFLT' */
  if (g.find_script_index (HB_OT_LAYOUT_TAG_DEFAULT_SCRIPT, script_index))
    return FALSE;

  /* try with 'dflt'; MS site has had typos and many fonts use it now :( */
  if (g.find_script_index (HB_OT_LAYOUT_TAG_DEFAULT_LANGUAGE, script_index))
    return FALSE;

  if (script_index) *script_index = HB_OT_LAYOUT_NO_SCRIPT_INDEX;
  return FALSE;
}

unsigned int
hb_ot_layout_table_get_feature_count (hb_ot_layout_t            *layout,
				      hb_ot_layout_table_type_t  table_type)
{
  const GSUBGPOS &g = get_gsubgpos_table (layout, table_type);

  return g.get_feature_count ();
}

hb_tag_t
hb_ot_layout_table_get_feature_tag (hb_ot_layout_t            *layout,
				    hb_ot_layout_table_type_t  table_type,
				    unsigned int               feature_index)
{
  const GSUBGPOS &g = get_gsubgpos_table (layout, table_type);

  return g.get_feature_tag (feature_index);
}

hb_bool_t
hb_ot_layout_table_find_feature (hb_ot_layout_t            *layout,
				 hb_ot_layout_table_type_t  table_type,
				 hb_tag_t                   feature_tag,
				 unsigned int              *feature_index)
{
  ASSERT_STATIC (NO_INDEX == HB_OT_LAYOUT_NO_FEATURE_INDEX);
  const GSUBGPOS &g = get_gsubgpos_table (layout, table_type);

  if (g.find_feature_index (feature_tag, feature_index))
    return TRUE;

  if (feature_index) *feature_index = HB_OT_LAYOUT_NO_FEATURE_INDEX;
  return FALSE;
}

unsigned int
hb_ot_layout_table_get_lookup_count (hb_ot_layout_t            *layout,
				     hb_ot_layout_table_type_t  table_type)
{
  const GSUBGPOS &g = get_gsubgpos_table (layout, table_type);

  return g.get_lookup_count ();
}


unsigned int
hb_ot_layout_script_get_language_count (hb_ot_layout_t            *layout,
					hb_ot_layout_table_type_t  table_type,
					unsigned int               script_index)
{
  const Script &s = get_gsubgpos_table (layout, table_type).get_script (script_index);

  return s.get_lang_sys_count ();
}

hb_tag_t
hb_ot_layout_script_get_language_tag (hb_ot_layout_t            *layout,
				      hb_ot_layout_table_type_t  table_type,
				      unsigned int               script_index,
				      unsigned int               language_index)
{
  const Script &s = get_gsubgpos_table (layout, table_type).get_script (script_index);

  return s.get_lang_sys_tag (language_index);
}

hb_bool_t
hb_ot_layout_script_find_language (hb_ot_layout_t            *layout,
				   hb_ot_layout_table_type_t  table_type,
				   unsigned int               script_index,
				   hb_tag_t                   language_tag,
				   unsigned int              *language_index)
{
  ASSERT_STATIC (NO_INDEX == HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX);
  const Script &s = get_gsubgpos_table (layout, table_type).get_script (script_index);

  if (s.find_lang_sys_index (language_tag, language_index))
    return TRUE;

  /* try with 'dflt'; MS site has had typos and many fonts use it now :( */
  if (s.find_lang_sys_index (HB_OT_LAYOUT_TAG_DEFAULT_LANGUAGE, language_index))
    return FALSE;

  if (language_index) *language_index = HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX;
  return FALSE;
}

hb_bool_t
hb_ot_layout_language_get_required_feature_index (hb_ot_layout_t            *layout,
						  hb_ot_layout_table_type_t  table_type,
						  unsigned int               script_index,
						  unsigned int               language_index,
						  unsigned int              *feature_index)
{
  const LangSys &l = get_gsubgpos_table (layout, table_type).get_script (script_index).get_lang_sys (language_index);

  if (feature_index) *feature_index = l.get_required_feature_index ();

  return l.has_required_feature ();
}

unsigned int
hb_ot_layout_language_get_feature_count (hb_ot_layout_t            *layout,
					 hb_ot_layout_table_type_t  table_type,
					 unsigned int               script_index,
					 unsigned int               language_index)
{
  const LangSys &l = get_gsubgpos_table (layout, table_type).get_script (script_index).get_lang_sys (language_index);

  return l.get_feature_count ();
}

unsigned int
hb_ot_layout_language_get_feature_index (hb_ot_layout_t            *layout,
					 hb_ot_layout_table_type_t  table_type,
					 unsigned int               script_index,
					 unsigned int               language_index,
					 unsigned int               num_feature)
{
  const GSUBGPOS &g = get_gsubgpos_table (layout, table_type);
  const LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

  return l.get_feature_index (num_feature);
}

hb_tag_t
hb_ot_layout_language_get_feature_tag (hb_ot_layout_t            *layout,
				       hb_ot_layout_table_type_t  table_type,
				       unsigned int               script_index,
				       unsigned int               language_index,
				       unsigned int               num_feature)
{
  const GSUBGPOS &g = get_gsubgpos_table (layout, table_type);
  const LangSys &l = g.get_script (script_index).get_lang_sys (language_index);
  unsigned int feature_index = l.get_feature_index (num_feature);

  return g.get_feature_tag (feature_index);
}


hb_bool_t
hb_ot_layout_language_find_feature (hb_ot_layout_t            *layout,
				    hb_ot_layout_table_type_t  table_type,
				    unsigned int               script_index,
				    unsigned int               language_index,
				    hb_tag_t                   feature_tag,
				    unsigned int              *feature_index)
{
  ASSERT_STATIC (NO_INDEX == HB_OT_LAYOUT_NO_FEATURE_INDEX);
  const GSUBGPOS &g = get_gsubgpos_table (layout, table_type);
  const LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

  unsigned int num_features = l.get_feature_count ();
  for (unsigned int i = 0; i < num_features; i++) {
    unsigned int f_index = l.get_feature_index (i);

    if (feature_tag == g.get_feature_tag (f_index)) {
      if (feature_index) *feature_index = f_index;
      return TRUE;
    }
  }

  if (feature_index) *feature_index = HB_OT_LAYOUT_NO_FEATURE_INDEX;
  return FALSE;
}

unsigned int
hb_ot_layout_feature_get_lookup_count (hb_ot_layout_t            *layout,
				       hb_ot_layout_table_type_t  table_type,
				       unsigned int               feature_index)
{
  const GSUBGPOS &g = get_gsubgpos_table (layout, table_type);
  const Feature &f = g.get_feature (feature_index);

  return f.get_lookup_count ();
}

unsigned int
hb_ot_layout_feature_get_lookup_index (hb_ot_layout_t            *layout,
				       hb_ot_layout_table_type_t  table_type,
				       unsigned int               feature_index,
				       unsigned int               num_lookup)
{
  const GSUBGPOS &g = get_gsubgpos_table (layout, table_type);
  const Feature &f = g.get_feature (feature_index);

  return f.get_lookup_index (num_lookup);
}

/*
 * GSUB
 */

hb_bool_t
hb_ot_layout_substitute_lookup (hb_ot_layout_t              *layout,
				hb_buffer_t                 *buffer,
			        unsigned int                 lookup_index,
				hb_ot_layout_feature_mask_t  mask)
{
  return layout->gsub->substitute_lookup (layout, buffer, lookup_index, mask);
}

/*
 * GPOS
 */

hb_bool_t
hb_ot_layout_position_lookup   (hb_ot_layout_t              *layout,
				hb_buffer_t                 *buffer,
			        unsigned int                 lookup_index,
				hb_ot_layout_feature_mask_t  mask)
{
  return layout->gpos->position_lookup (layout, buffer, lookup_index, mask);
}