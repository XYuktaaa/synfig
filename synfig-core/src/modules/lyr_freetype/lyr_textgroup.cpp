#ifdef USING_PCH  
# include "pch.h"  
#else  
# ifdef HAVE_CONFIG_H  
#  include <config.h>  
# endif  
  
#include "lyr_textgroup.h"  
  
#include <synfig/canvas.h>  
#include <synfig/context.h>  
#include <synfig/general.h>  
#include <synfig/localization.h>  
#include <synfig/string.h>  
#include <synfig/valuenode.h>  
#include <synfig/layers/layer_shape.h>  
#include <synfig/rendering/primitive/contour.h>
#include "lyr_freetype.h"  
  
#endif  
  
using namespace synfig;  
  
// ft_library is defined in main.cpp  
extern FT_Library ft_library;  
  
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
  
// ==================== Layer_GlyphShape ====================  
  
Layer_GlyphShape::Layer_GlyphShape()  
{  
    SET_INTERPOLATION_DEFAULTS();  
    SET_STATIC_DEFAULTS();  
}  
  
Layer_GlyphShape::~Layer_GlyphShape() {}  
  
String  
Layer_GlyphShape::get_local_name() const  
{  
    return _("Glyph");  
}  
  
void Layer_GlyphShape::set_glyph_chunks(const rendering::Contour::ChunkList& chunks)  
{  
    clear();  // Layer_Shape::clear() - protected, accessible from subclass  
    add(chunks);  // Layer_Shape::add(ChunkList) - protected, accessible from subclass  
    // Force the contour to close so it renders properly  
    shape_contour().close();  
}  
  
void  
Layer_GlyphShape::sync_vfunc()  
{  
    clear();  
    add(stored_chunks);  
}  

void
Layer_TextGroup::on_canvas_set()
{
    Layer_PasteCanvas::on_canvas_set();  // IMPORTANT

    printf("ON_CANVAS_SET CALLED\n");

    sync_glyphs();
}
// ==================== Layer_TextGroup ====================  
  
Layer_TextGroup::Layer_TextGroup()  
    : face(nullptr)  
{  
    param_text  = ValueBase(std::string());  
    param_size  = ValueBase(Vector(0.25, 0.25));     
    param_family = ValueBase(std::string("Sans Serif"));  
    param_style  = ValueBase(int(0));  
    param_weight = ValueBase(int(0));  
    SET_INTERPOLATION_DEFAULTS();  
    SET_STATIC_DEFAULTS();  
  
    param_text = String("");  
  
    // PoC: hardcoded font  
    FT_New_Face(ft_library,  
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",  
        0, &face);  
}  
  
Layer_TextGroup::~Layer_TextGroup()  
{  
    if (face)  
        FT_Done_Face(face);  
}  
  
String  
Layer_TextGroup::get_local_name() const  
{  
    return _("Text Group");  
}  
  
bool  
Layer_TextGroup::set_param(const String& param, const ValueBase& value)  
{  
    // if (param == "text" && value.same_type_as(param_text))
    if (param == "text" && value.can_get(String())) 
    {  
        param_text = value;  
        sync_glyphs();  
        return true;  
    }  
    return Layer_PasteCanvas::set_param(param, value);  
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

void Layer_TextGroup::sync_glyphs()  
{  
    std::string text = param_text.get(std::string());  
    if (text.empty() || !face) return;  
  
    Canvas::Handle canvas = get_sub_canvas();  
    if (!canvas) return;  
    canvas->clear();  
  
    // Scale factor: EM units -> world units  
    Vector size = param_size.get(Vector()) * 2;  
    Real scale_x = size[0] / face->units_per_EM;  
    Real scale_y = size[1] / face->units_per_EM;  
  
    Vector offset(0, 0);  
  
    for (size_t i = 0; i < text.size(); i++)  
    {  
        uint32_t charcode = text[i];  
        FT_UInt glyph_index = FT_Get_Char_Index(face, charcode);  
        if (!glyph_index) continue;  
  
        FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_SCALE);  
        if (error) continue;  
  
        FT_Glyph ftglyph;  
        error = FT_Get_Glyph(face->glyph, &ftglyph);  
        if (error) continue;  
  
        rendering::Contour::ChunkList outline;  
        if (ftglyph->format == FT_GLYPH_FORMAT_OUTLINE) {  
            FT_OutlineGlyph outline_glyph = (FT_OutlineGlyph)ftglyph;  
            Layer_Freetype::convert_outline_to_contours(outline_glyph, outline);  
        }  
  
        if (!outline.empty())  
        {  
            // Shift glyph to its position (in EM units)  
            Layer_Freetype::shift_contour_chunks(outline, offset);  
  
            // Scale from EM units to world units  
            for (auto& chunk : outline) {  
                chunk.p1[0]  *= scale_x;  chunk.p1[1]  *= scale_y;  
                chunk.pp0[0] *= scale_x;  chunk.pp0[1] *= scale_y;  
                chunk.pp1[0] *= scale_x;  chunk.pp1[1] *= scale_y;  
            }  
  
            Layer::Handle child(new Layer_GlyphShape());  
            Layer_GlyphShape* glyph_layer = dynamic_cast<Layer_GlyphShape*>(child.get());  
            if (glyph_layer) {  
                glyph_layer->set_glyph_chunks(outline);  
                child->set_description(String(1, (char)charcode));  
                canvas->push_back(child);  
            }  
        }  
  
        // Advance pen position (in EM units, before scaling)  
        offset[0] += ftglyph->advance.x >> 10;  
        offset[1] += ftglyph->advance.y >> 10;  
  
        FT_Done_Glyph(ftglyph);  
    }  
  
    signal_subcanvas_changed()();  
    changed();  
}
