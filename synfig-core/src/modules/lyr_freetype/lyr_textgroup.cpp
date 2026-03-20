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
#include "lyr_freetype.h"
  
#endif  
  
using namespace synfig;  
  
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
  
Layer_GlyphShape::~Layer_GlyphShape() {
        if (face)  
        FT_Done_Face(face);  
}  
  
String Layer_GlyphShape::get_local_name() const { return _("Text Group"); }  
  
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
{
    SET_INTERPOLATION_DEFAULTS();
    SET_STATIC_DEFAULTS();

    face = nullptr;

    // PoC font loading
    extern FT_Library ft_library;
    FT_New_Face(ft_library,
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        0,
        &face);
}
bool
Layer_TextGroup::set_param(const String& param, const ValueBase& value)
{
    if (param == "text" && value.can_get(String()))
    {
        param_text = value;
        sync_glyphs();   // 🔥 trigger rebuild
        return true;
    }

    return Layer_PasteCanvas::set_param(param, value);
}

void
Layer_TextGroup::sync_glyphs()
{
    Canvas::Handle canvas = get_sub_canvas();

    if (!canvas)
    {
        canvas = Canvas::create_inline(get_canvas());
        set_sub_canvas(canvas);
    }

    canvas->clear();

    std::string text = param_text.get(std::string());

    if (text.empty() || !face)
        return;

    Vector offset(0, 0);
    int glyph_num = 0;

    for (char c : text)
    {
        uint32_t glyph_index = FT_Get_Char_Index(face, c);

        if (!glyph_index)
            continue;

        FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_SCALE);

        FT_Glyph ftglyph;
        FT_Get_Glyph(face->glyph, &ftglyph);

        rendering::Contour::ChunkList outline;

        if (ftglyph->format == FT_GLYPH_FORMAT_OUTLINE)
        {
            Layer_Freetype::convert_outline_to_contours(
                FT_OutlineGlyph(ftglyph), outline);
        }

        if (!outline.empty())
        {
            Layer::Handle child (new Layer_GlyphShape());

            auto* glyph_layer =
                dynamic_cast<Layer_GlyphShape*>(child.get());

            if (glyph_layer)
            {
                auto shifted = outline;

                Layer_Freetype::shift_contour_chunks(
                    shifted, offset);

                glyph_layer->set_glyph_chunks(shifted);

                child->set_canvas(canvas);
                canvas->push_back(child);
            }
        }

        offset[0] += face->glyph->advance.x >> 6;
        glyph_num++;

        FT_Done_Glyph(ftglyph);
    }
}
