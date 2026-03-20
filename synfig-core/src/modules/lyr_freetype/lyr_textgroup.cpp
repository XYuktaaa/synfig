#ifdef USING_PCH  
# include "pch.h"  
#else  
# ifdef HAVE_CONFIG_H  
#  include <config.h>  
# endif  
  
#include "lyr_textgroup.h"  
#include "lyr_freetype.h"  
  
#include <synfig/canvas.h>  
#include <synfig/general.h>  
#include <synfig/localization.h>  
#include <synfig/string_helper.h>  
#include <synfig/layers/layer_shape.h>  
#include <synfig/rendering/primitive/contour.h>  
  
#include <ft2build.h>  
#include FT_FREETYPE_H  
#include FT_GLYPH_H  
#include FT_IMAGE_H  
#include FT_OUTLINE_H  
  
#if HAVE_HARFBUZZ  
#include <hb.h>  
#include <hb-ft.h>  
#endif   
  
using namespace synfig;  

extern FT_Library ft_library
  
// === Layer_TextGroup registration ===  
SYNFIG_LAYER_INIT(Layer_TextGroup);  
SYNFIG_LAYER_SET_NAME(Layer_TextGroup,"text_group");  
SYNFIG_LAYER_SET_LOCAL_NAME(Layer_TextGroup,N_("Text Group"));  
SYNFIG_LAYER_SET_CATEGORY(Layer_TextGroup,N_("Other"));  
SYNFIG_LAYER_SET_VERSION(Layer_TextGroup,"0.1");  
  
// === Layer_GlyphShape registration ===  
SYNFIG_LAYER_INIT(Layer_GlyphShape);  
SYNFIG_LAYER_SET_NAME(Layer_GlyphShape,"glyph_shape");  
SYNFIG_LAYER_SET_LOCAL_NAME(Layer_GlyphShape,N_("Glyph"));  
SYNFIG_LAYER_SET_CATEGORY(Layer_GlyphShape,CATEGORY_DO_NOT_USE);  
SYNFIG_LAYER_SET_VERSION(Layer_GlyphShape,"0.1");    
  
Layer_GlyphShape::Layer_GlyphShape() {  
    SET_INTERPOLATION_DEFAULTS();  
    SET_STATIC_DEFAULTS();  
}  

Layer_TextGroup::~Layer_TextGroup()  
{  
    if (face)  
        FT_Done_Face(face);  
}  
  
String Layer_TextGroup::get_local_name() const  
{  
    return _("Text Group");  
}
Layer_GlyphShape::~Layer_GlyphShape() {}  
  
String Layer_GlyphShape::get_local_name() const { return _("Glyph"); }  
  
void Layer_GlyphShape::set_glyph_chunks(  
    const rendering::Contour::ChunkList& chunks)  
{  
    stored_chunks = chunks;  
    force_sync();  
}  

ValueBase  
Layer_TextGroup::get_param(const String& param) const  
{  
    EXPORT_VALUE(param_text);  
    EXPORT_NAME();  
    EXPORT_VERSION();  
    return Layer_PasteCanvas::get_param(param);  
}  
  
Layer::Vocab  
Layer_TextGroup::get_param_vocab() const  
{  
    Layer::Vocab ret(Layer_PasteCanvas::get_param_vocab());  
    ret.push_back(ParamDesc("text")  
        .set_local_name(_("Text"))  
        .set_description(_("The text to decompose into per-character layers"))  
    );  
    return ret;  
}
  
void Layer_GlyphShape::sync_vfunc()  
{  
    clear();  
    add(stored_chunks);  
}

Layer_TextGroup::Layer_TextGroup()  
    : face(nullptr)  
{  
#if HAVE_HARFBUZZ  
    font = nullptr;  
#endif  
    param_text = ValueBase(std::string());  
  
    // Load a default font for the PoC  
    FT_Error error = FT_New_Face(ft_library,  
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, &face);  
    if (error)  
        synfig::error("Layer_TextGroup: Failed to load default font");  
  
    SET_INTERPOLATION_DEFAULTS();  
    SET_STATIC_DEFAULTS();  
}

bool Layer_TextGroup::set_param(const String& param, const ValueBase& value)  
{  
    if (param == "text" && value.same_type_as(param_text)) {  
        param_text = value;  
        sync_glyphs();  
        return true;  
    }  
    return Layer_PasteCanvas::set_param(param, value);  
}

void Layer_GlyphShape::set_glyph_contour(  
    const rendering::Contour::ChunkList& chunks)  
{  
    clear();       // inherited from Layer_Shape (protected, accessible here)  
    add(chunks);   // inherited from Layer_Shape (protected, accessible here)  
}

void Layer_TextGroup::sync_glyphs()  
{  
    std::string text = param_text.get(std::string());  
    if (synfig::trim(text).empty() || !face)  
        return;  
  
    // 1. Create or clear the sub-canvas  
    Canvas::Handle canvas = get_sub_canvas();  
    if (!canvas) {  
        if (!get_canvas()) return;  
        canvas = Canvas::create_inline(get_canvas());  
        set_sub_canvas(canvas);  
    }  
    canvas->clear();  
  
    // 2. Get glyph indices (simplified — single line, LTR, no HarfBuzz for PoC)  
    std::vector<uint32_t> glyph_indices;  
    for (size_t i = 0; i < text.size(); ) {  
        // Simple UTF-8 → codepoint (ASCII subset for PoC)  
        uint32_t cp = (unsigned char)text[i];  
        i++;  
        uint32_t glyph_index = FT_Get_Char_Index(face, cp);  
        glyph_indices.push_back(glyph_index);  
    }  
  
    // 3. For each glyph, extract outline and create a child Layer_Shape  
    Vector offset(0, 0);  
    int char_index = 0;  
  
    for (uint32_t glyph_index : glyph_indices) {  
        // Load glyph  
        FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_SCALE);  
        if (error) { char_index++; continue; }  
  
        FT_Glyph ftglyph;  
        error = FT_Get_Glyph(face->glyph, &ftglyph);  
        if (error) { char_index++; continue; }  
  
        Vector advance(ftglyph->advance.x >> 10, ftglyph->advance.y >> 10);  
  
        // Extract outline contours  
        rendering::Contour::ChunkList chunks;  
        if (ftglyph->format == FT_GLYPH_FORMAT_OUTLINE) {  
            FT_OutlineGlyph outline_glyph = (FT_OutlineGlyph)ftglyph;  
            Layer_Freetype::convert_outline_to_contours(outline_glyph, chunks);  
            Layer_Freetype::shift_contour_chunks(chunks, offset);  
        }  
  
        FT_Done_Glyph(ftglyph);  
  
        // Skip whitespace glyphs (no outline) but still advance  
        if (chunks.empty()) {  
            offset[0] += advance[0];  
            offset[1] += advance[1];  
            char_index++;  
            continue;  
        }  
  
        // Create a child Layer_Shape for this glyph  
        // We use Layer_Freetype as the child since it extends Layer_Shape  
        // and we need access to the protected add() method.  
        // Alternative: create a minimal Layer_GlyphShape that exposes add().  
        Layer::Handle child(new Layer_GlyphShape());  
        child->set_description(  
            synfig::strprintf("Glyph '%c'", (char)text[char_index]));  
  
        // Layer_GlyphShape needs a method to set its contour chunks  
        // (see below for the add_chunks method)  
        Layer_GlyphShape* glyph_layer = dynamic_cast<Layer_GlyphShape*>(child.get());  
        if (glyph_layer) {  
            glyph_layer->set_glyph_contour(chunks);  
        }  
  
        canvas->push_back(child);  
  
        offset[0] += advance[0];  
        offset[1] += advance[1];  
        char_index++;  
    }  
}
