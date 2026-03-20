#ifndef __SYNFIG_LYR_TEXTGROUP_H  
#define __SYNFIG_LYR_TEXTGROUP_H  

#include <synfig/layers/layer_shape.h>
#include <synfig/layers/layer_pastecanvas.h>  
#include <synfig/rendering/primitive/contour.h>
#include <synfig/value.h>
#include <synfig/string.h>
#include <ft2build.h>  
#include FT_FREETYPE_H  
#include FT_GLYPH_H  
#if HAVE_HARFBUZZ  
#include <hb.h>  
#endif  

using namespace synfig;
using namespace synfig::rendering;
class Layer_TextGroup : public synfig::Layer_PasteCanvas  
{  
    SYNFIG_LAYER_MODULE_EXT  
private:  
    synfig::ValueBase param_text;  
    synfig::ValueBase param_family;  
    synfig::ValueBase param_style;  
    synfig::ValueBase param_weight;  
    synfig::ValueBase param_size;  
    synfig::ValueBase param_compress;  
    synfig::ValueBase param_vcompress;  
    synfig::ValueBase param_orient;  
    synfig::ValueBase param_use_kerning;  
    synfig::ValueBase param_grid_fit;  
    synfig::ValueBase param_direction;  
  
    FT_Face face;  
#if HAVE_HARFBUZZ  
    hb_font_t *font;  
#endif  
  
public:  
    Layer_TextGroup();  
    ~Layer_TextGroup();  
  
    bool set_param(const synfig::String &param, const synfig::ValueBase &value) override;  
    synfig::ValueBase get_param(const synfig::String &param) const override;  
    Vocab get_param_vocab() const override;  
    synfig::String get_local_name() const override;  
  
private:  
    void sync_glyphs();  // The key method: decomposes text into child layers  
    // void new_font(const synfig::String &family, int style, int weight);  
};  

class Layer_GlyphShape : public synfig::Layer_Shape  
{  
    SYNFIG_LAYER_MODULE_EXT  
  
private:  
    synfig::rendering::Contour::ChunkList stored_chunks;  
  
public:  
    Layer_GlyphShape();  
    ~Layer_GlyphShape();  
  
    virtual synfig::String get_local_name() const;  
  
    // Public method to set glyph contour data from Layer_TextGroup  
    void set_glyph_chunks(const synfig::rendering::Contour::ChunkList& chunks);  
  
protected:  
    virtual void sync_vfunc();  
}; 
  
#endif
